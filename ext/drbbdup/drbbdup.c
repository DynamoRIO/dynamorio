/* **********************************************************
 * Copyright (c) 2013-2020 Google, Inc.   All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "dr_defines.h"
#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "hashtable.h"
#include "drbbdup.h"
#include <string.h>
#include <stdint.h>
#include "../ext_utils.h"

#undef drmgr_is_first_instr
#undef drmgr_is_first_nonlabel_instr
#undef drmgr_is_last_instr

#ifdef DEBUG
#    define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#    define LOG(dc, mask, level, ...) dr_log(dc, mask, level, __VA_ARGS__)
#else
#    define ASSERT(x, msg)
#    define LOG(dc, mask, level, ...)
#endif

/* DynamoRIO Basic Block Duplication Extension: a code builder that
 * duplicates code of basic blocks and dispatches control according to runtime
 * conditions so that different instrumentation may be efficiently executed.
 */

/* TODO i#4134: ARM not yet supported. */
#ifndef X86
#    error ARM is not yet supported
#endif

#define HASH_BIT_TABLE 13

/* Definitions for drbbdup's hit-table that drives dynamic case handling.
 * Essentially, a hash-table tracks which BBs are frequently encountering
 * new unhandled cases.
 */
#define TABLE_SIZE 65536 /* Must be a power of 2 to perform efficient mod. */

typedef enum {
    DRBBDUP_ENCODING_SLOT = 0,
    DRBBDUP_XAX_REG_SLOT = 1,
    DRBBDUP_FLAG_REG_SLOT = 2,
    DRBBDUP_HIT_TABLE_SLOT = 3,
    DRBBDUP_SLOT_COUNT = 4, /* Need to update if more slots are added. */
} drbbdup_thread_slots_t;

/* A scratch register used by drbbdup's dispatcher. */
#define DRBBDUP_SCRATCH_REG DR_REG_XAX

/* Special index values are used to help guide case selection. */
#define DRBBDUP_DEFAULT_INDEX -1
#define DRBBDUP_IGNORE_INDEX -2

/* Contains information of a case that maps to a copy of a bb. */
typedef struct {
    uintptr_t encoding; /* The encoding specific to the case. */
    bool is_defined;    /* Denotes whether the case is defined. */
} drbbdup_case_t;

/* Contains per bb information required for managing bb copies. */
typedef struct {
    int ref_counter;
    bool enable_dup;              /* Denotes whether to duplicate blocks. */
    bool enable_dynamic_handling; /* Denotes whether to dynamically generate cases. */
    bool are_flags_dead;      /* Denotes whether flags are dead at the start of a bb. */
    bool is_scratch_reg_dead; /* Denotes whether DRBBDUP_SCRATCH_REG is dead at start. */
    bool is_gen; /* Denotes whether a new bb copy is dynamically being generated. */
    drbbdup_case_t default_case;
    drbbdup_case_t *cases; /* Is NULL if enable_dup is not set. */
} drbbdup_manager_t;

/* Label types. */
typedef enum {
    DRBBDUP_LABEL_START = 78, /* Denotes the start of a bb copy. */
    DRBBDUP_LABEL_EXIT = 79,  /* Denotes the end of all bb copies. */
} drbbdup_label_t;

typedef struct {
    int case_index; /* Used to keep track of the current case during insertion. */
    void *orig_analysis_data;        /* Analysis data accessible for all cases. */
    void *default_analysis_data;     /* Analysis data specific to default case. */
    void **case_analysis_data;       /* Analysis data specific to cases. */
    uint16_t hit_counts[TABLE_SIZE]; /* Keeps track of hit-counts of unhandled cases. */
    instr_t *first_instr;          /* The first instr of the bb copy being considered. */
    instr_t *first_nonlabel_instr; /* The first non label instr of the bb copy. */
    instr_t *last_instr;           /* The last instr of the bb copy being considered. */
} drbbdup_per_thread;

static uint ref_count = 0;        /* Instance count of drbbdup. */
static hashtable_t manager_table; /* Maps bbs with book-keeping data. */
static drbbdup_options_t opts;
static void *rw_lock = NULL;

/* For tracking statistics. */
static void *stat_mutex = NULL;
static drbbdup_stats_t stats;

/* An outlined code cache (storing a clean call) for dynamically generating a case. */
static app_pc new_case_cache_pc = NULL;

static int tls_idx = -1; /* For thread local storage info. */
static reg_id_t tls_raw_reg;
static uint tls_raw_base;

static uintptr_t *
drbbdup_set_tls_raw_slot_addr(drbbdup_thread_slots_t slot_idx)
{
    ASSERT(0 <= slot_idx && slot_idx < DRBBDUP_SLOT_COUNT, "out-of-bounds slot index");
    byte *base = dr_get_dr_segment_base(tls_raw_reg);
    return (uintptr_t *)(base + tls_raw_base);
}

static void
drbbdup_set_tls_raw_slot_val(drbbdup_thread_slots_t slot_idx, uintptr_t val)
{
    uintptr_t *addr = drbbdup_set_tls_raw_slot_addr(slot_idx);
    *addr = val;
}

static uintptr_t
drbbdup_get_tls_raw_slot_val(drbbdup_thread_slots_t slot_idx)
{
    uintptr_t *addr = drbbdup_set_tls_raw_slot_addr(slot_idx);
    return *addr;
}

static opnd_t
drbbdup_get_tls_raw_slot_opnd(drbbdup_thread_slots_t slot_idx)
{
    return opnd_create_far_base_disp_ex(tls_raw_reg, REG_NULL, REG_NULL, 1,
                                        tls_raw_base + (slot_idx * (sizeof(void *))),
                                        OPSZ_PTR, false, true, false);
}

drbbdup_status_t
drbbdup_set_encoding(uintptr_t encoding)
{
    drbbdup_set_tls_raw_slot_val(DRBBDUP_ENCODING_SLOT, encoding);
    return DRBBDUP_SUCCESS;
}

opnd_t
drbbdup_get_encoding_opnd()
{
    return drbbdup_get_tls_raw_slot_opnd(DRBBDUP_ENCODING_SLOT);
}

static void
drbbdup_spill_register(void *drcontext, instrlist_t *ilist, instr_t *where, int slot_idx,
                       reg_id_t reg_id)
{
    opnd_t slot_opnd = drbbdup_get_tls_raw_slot_opnd(slot_idx);
    instr_t *instr = INSTR_CREATE_mov_st(drcontext, slot_opnd, opnd_create_reg(reg_id));
    instrlist_meta_preinsert(ilist, where, instr);
}

static void
drbbdup_restore_register(void *drcontext, instrlist_t *ilist, instr_t *where,
                         int slot_idx, reg_id_t reg_id)
{
    opnd_t slot_opnd = drbbdup_get_tls_raw_slot_opnd(slot_idx);
    instr_t *instr = INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(reg_id), slot_opnd);
    instrlist_meta_preinsert(ilist, where, instr);
}

/* Returns whether or not instr is a special instruction that must be the last instr in a
 * bb in accordance to DR rules.
 */
static bool
drbbdup_is_special_instr(instr_t *instr)
{
    return instr_is_syscall(instr) || instr_is_cti(instr) || instr_is_ubr(instr) ||
        instr_is_interrupt(instr);
}

/****************************************************************************
 * DUPlICATION PHASE
 *
 * This phase is responsible for performing the actual duplications of bbs.
 */

/* Returns the number of bb duplications excluding the default case. */
static uint
drbbdup_count(drbbdup_manager_t *manager)
{
    ASSERT(manager != NULL, "should not be NULL");

    uint count = 0;
    int i;
    for (i = 0; i < opts.dup_limit; i++) {
        /* If case is defined, increment the counter. */
        if (manager->cases[i].is_defined)
            count++;
    }

    return count;
}

/* Clone from original instrlist, but place duplication in bb. */
static void
drbbdup_add_copy(void *drcontext, instrlist_t *bb, instrlist_t *orig_bb)
{
    if (instrlist_first(orig_bb) != NULL) {
        instrlist_t *dup = instrlist_clone(drcontext, orig_bb);
        instr_t *start = instrlist_first(dup);
        instrlist_prepend(bb, start);

        /* Empty list and destroy. Do not use clear as instrs are needed. */
        instrlist_init(dup);
        instrlist_destroy(drcontext, dup);
    }
}

/* Creates a manager, which contains book-keeping data for a fragment. */
static drbbdup_manager_t *
drbbdup_create_manager(void *drcontext, void *tag, instrlist_t *bb)
{
    drbbdup_manager_t *manager = dr_global_alloc(sizeof(drbbdup_manager_t));
    memset(manager, 0, sizeof(drbbdup_manager_t));

    manager->cases = NULL;
    ASSERT(opts.dup_limit > 0, "dup limit should be greater than zero");
    manager->cases = dr_global_alloc(sizeof(drbbdup_case_t) * opts.dup_limit);
    memset(manager->cases, 0, sizeof(drbbdup_case_t) * opts.dup_limit);
    manager->enable_dup = true;
    manager->enable_dynamic_handling = true;
    manager->ref_counter = 1;
    manager->is_gen = false;

    ASSERT(opts.set_up_bb_dups != NULL, "set up call-back cannot be NULL");
    manager->default_case.encoding =
        opts.set_up_bb_dups(manager, drcontext, tag, bb, &manager->enable_dup,
                            &manager->enable_dynamic_handling, opts.user_data);

    /* XXX i#3778: To remove once we support specific fragment deletion. */
    DR_ASSERT_MSG(!manager->enable_dynamic_handling,
                  "dynamic case generation is not yet supported");

    /* Check whether user wants copies for this particular bb. */
    if (!manager->enable_dup && manager->cases != NULL) {
        /* Multiple cases not wanted. Destroy cases. */
        dr_global_free(manager->cases, sizeof(drbbdup_case_t) * opts.dup_limit);
        manager->cases = NULL;
    }

    manager->default_case.is_defined = true;
    return manager;
}

/* Transforms the bb to contain additional copies (within the same fragment). */
static void
drbbdup_set_up_copies(void *drcontext, instrlist_t *bb, drbbdup_manager_t *manager)
{
    ASSERT(manager != NULL, "manager should not be NULL");
    ASSERT(manager->enable_dup, "bb duplication should be enabled");
    ASSERT(manager->cases != NULL, "cases should not be NULL");

    /* Example: Lets say we have the following bb:
     *   mov ebx ecx
     *   mov esi eax
     *   ret
     *
     * We require 2 cases, we need to construct the bb as follows:
     *   LABEL 1
     *   mov ebx ecx
     *   mov esi eax
     *   jmp EXIT LABEL
     *
     *   LABEL 2
     *   mov ebx ecx
     *   mov esi eax
     *   EXIT LABEL
     *   ret
     *
     * The inclusion of the dispatcher is left for the instrumentation stage.
     *
     * Note, we add jmp instructions here and DR will set them to meta automatically.
     */

    /* We create a duplication here to keep track of original bb. */
    instrlist_t *original = instrlist_clone(drcontext, bb);

    /* If the last instruction is a system call/cti, we remove it from the original.
     * This is done so that we do not copy such instructions and abide by DR rules.
     */
    instr_t *last = instrlist_last_app(original);
    if (drbbdup_is_special_instr(last)) {
        instrlist_remove(original, last);
        instr_destroy(drcontext, last);
    }

    /* Tell drreg to ignore control flow as it is ensured that all registers
     * are live at the start of bb copies.
     */
    drreg_set_bb_properties(drcontext, DRREG_IGNORE_CONTROL_FLOW);
    /* Restoration at the end of the block is not done automatically
     * by drreg but is managed by drbbdup. Different cases could
     * have different registers spilled and therefore restoration is
     * specific to cases. During the insert stage, drbbdup restores
     * all unreserved registers upon exit of a bb copy by calling
     * drreg_restore_all().
     */
    drreg_set_bb_properties(drcontext, DRREG_USER_RESTORES_AT_BB_END);

    /* Create an EXIT label. */
    instr_t *exit_label = INSTR_CREATE_label(drcontext);
    opnd_t exit_label_opnd = opnd_create_instr(exit_label);
    instr_set_note(exit_label, (void *)(intptr_t)DRBBDUP_LABEL_EXIT);

    /* Prepend a START label. */
    instr_t *label = INSTR_CREATE_label(drcontext);
    instr_set_note(label, (void *)(intptr_t)DRBBDUP_LABEL_START);
    instrlist_meta_preinsert(bb, instrlist_first(bb), label);

    /* Perform duplication. */
    int num_copies = (int)drbbdup_count(manager);
    ASSERT(num_copies >= 1, "there must be at least one copy");
    int start = num_copies - 1;
    int i;
    for (i = start; i >= 0; i--) {
        /* Prepend a jmp targeting the EXIT label. */
        instr_t *jmp_exit = INSTR_CREATE_jmp(drcontext, exit_label_opnd);
        instrlist_preinsert(bb, instrlist_first(bb), jmp_exit);

        /* Prepend a copy. */
        drbbdup_add_copy(drcontext, bb, original);

        /* Prepend a START label. */
        label = INSTR_CREATE_label(drcontext);
        instr_set_note(label, (void *)(intptr_t)DRBBDUP_LABEL_START);
        instrlist_meta_preinsert(bb, instrlist_first(bb), label);
    }

    /* Delete original. We are done from making further copies. */
    instrlist_clear_and_destroy(drcontext, original);

    /* Add the EXIT label to the last copy of the bb.
     * If there is a syscall, place the exit label prior, leaving the syscall
     * last. Again, this is to abide by DR rules.
     */
    last = instrlist_last(bb);
    if (drbbdup_is_special_instr(last))
        instrlist_meta_preinsert(bb, last, exit_label);
    else
        instrlist_meta_postinsert(bb, last, exit_label);
}

static dr_emit_flags_t
drbbdup_duplicate_phase(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                        bool translating)
{
    if (translating)
        return DR_EMIT_DEFAULT;

    app_pc pc = instr_get_app_pc(instrlist_first_app(bb));
    ASSERT(pc != NULL, "pc cannot be NULL");

    dr_rwlock_write_lock(rw_lock);

    drbbdup_manager_t *manager =
        (drbbdup_manager_t *)hashtable_lookup(&manager_table, pc);

    /* A manager is created if there does not exit one that does not already
     * book-keeping this bb. */
    if (manager == NULL) {
        manager = drbbdup_create_manager(drcontext, tag, bb);
        ASSERT(manager != NULL, "created manager cannot be NULL");
        hashtable_add(&manager_table, pc, manager);
        if (opts.is_stat_enabled) {
            dr_mutex_lock(stat_mutex);
            if (!manager->enable_dup)
                stats.no_dup_count++;
            if (!manager->enable_dynamic_handling)
                stats.no_dynamic_handling_count++;
            dr_mutex_unlock(stat_mutex);
        }
    } else {
        /* A manager is already book-keeping this bb. Two scenarios are considered:
         *   1) A new case is registered and re-instrumentation is
         *      triggered via flushing.
         *   2) The bb has been deleted by DR due to other reasons (e.g. memory)
         *      and re-instrumented again.
         *
         * If we are handling a new case, there is no need to increment ref.
         */
        if (manager->is_gen)
            manager->is_gen = false;
        else
            manager->ref_counter++;
    }

    if (manager->enable_dup) {
        /* Add the copies. */
        drbbdup_set_up_copies(drcontext, bb, manager);
    }

    dr_rwlock_write_unlock(rw_lock);

    return DR_EMIT_STORE_TRANSLATIONS;
}

/****************************************************************************
 * ANALYSIS PHASE
 */

/* Determines whether or not we reached a special label recognisable by drbbdup. */
static bool
drbbdup_is_at_label(instr_t *check_instr, drbbdup_label_t label)
{
    if (check_instr == NULL)
        return false;

    /* If it is not a meta label just skip! */
    if (!(instr_is_label(check_instr) && instr_is_meta(check_instr)))
        return false;

    /* Notes are inspected to check whether the label is relevant to drbbdup. */
    drbbdup_label_t actual_label =
        (drbbdup_label_t)(uintptr_t)instr_get_note(check_instr);
    return actual_label == label;
}

/* Returns true if at the start of a bb version is reached. */
static bool
drbbdup_is_at_start(instr_t *check_instr)
{
    return drbbdup_is_at_label(check_instr, DRBBDUP_LABEL_START);
}

/* Returns true if at the end of a bb version is reached. */
static bool
drbbdup_is_at_end(instr_t *check_instr)
{
    if (drbbdup_is_at_label(check_instr, DRBBDUP_LABEL_EXIT))
        return true;

    if (instr_is_cti(check_instr)) {
        instr_t *next_instr = instr_get_next(check_instr);
        return drbbdup_is_at_label(next_instr, DRBBDUP_LABEL_START);
    }

    return false;
}

/* Iterates forward to the start of the next bb copy. Returns NULL upon failure. */
static instr_t *
drbbdup_next_start(instr_t *instr)
{
    while (instr != NULL && !drbbdup_is_at_start(instr))
        instr = instr_get_next(instr);

    return instr;
}

static instr_t *
drbbdup_first_app(instrlist_t *bb)
{
    instr_t *instr = instrlist_first_app(bb);
    /* We also check for at end labels, because the jmp inserted by drbbdup is
     * an app instr which should not be considered.
     */
    while (instr != NULL && (drbbdup_is_at_start(instr) || drbbdup_is_at_end(instr)))
        instr = instr_get_next_app(instr);

    return instr;
}

/* Iterates forward to the end of the next bb copy. Returns NULL upon failure. */
static instr_t *
drbbdup_next_end(instr_t *instr)
{
    while (instr != NULL && !drbbdup_is_at_end(instr))
        instr = instr_get_next(instr);

    return instr;
}

/* Extracts a single bb copy from the overall bb starting from start.
 * start is also set to the beginning of next bb copy for easy chaining.
 *  Overall, separate instr lists simplify user call-backs.
 *  The returned instr list needs to be destroyed using instrlist_clear_and_destroy().
 */
static instrlist_t *
drbbdup_extract_single_bb_copy(void *drcontext, instrlist_t *bb, instr_t *start,
                               OUT instr_t **next)
{
    instrlist_t *case_bb = instrlist_create(drcontext);

    ASSERT(start != NULL, "start instruction cannot be NULL");
    ASSERT(instr_get_note(start) == (void *)DRBBDUP_LABEL_START,
           "start instruction should be a START label");

    instr_t *instr = instr_get_next(start); /* Skip START label. */
    while (instr != NULL && !drbbdup_is_at_end(instr)) {
        instr_t *instr_cpy = instr_clone(drcontext, instr);
        instrlist_append(case_bb, instr_cpy);
        instr = instr_get_next(instr);
    }
    ASSERT(instr != NULL, "end instruction cannot be NULL");
    ASSERT(!drbbdup_is_at_start(instr), "end cannot be at start");

    /* Point to the next bb. */
    if (next != NULL)
        *next = drbbdup_next_start(instr);

    /* Also include the last instruction in the bb if it is a
     * syscall/cti instr.
     */
    instr_t *last_instr = instrlist_last(bb);
    if (drbbdup_is_special_instr(last_instr)) {
        instr_t *instr_cpy = instr_clone(drcontext, last_instr);
        instrlist_append(case_bb, instr_cpy);
    }

    return case_bb;
}

/* Trigger orig analysis event. This useful to set up and share common data
 * that transcends over different cases.
 */
static void *
drbbdup_do_orig_analysis(drbbdup_manager_t *manager, void *drcontext, void *tag,
                         instrlist_t *bb, instr_t *start)
{
    if (opts.analyze_orig == NULL)
        return NULL;

    void *orig_analysis_data = NULL;
    if (manager->enable_dup) {
        instrlist_t *case_bb = drbbdup_extract_single_bb_copy(drcontext, bb, start, NULL);
        opts.analyze_orig(drcontext, tag, case_bb, opts.user_data, &orig_analysis_data);
        instrlist_clear_and_destroy(drcontext, case_bb);
    } else {
        /* For bb with no wanted copies, simply invoke the call-back with original bb.
         */
        opts.analyze_orig(drcontext, tag, bb, opts.user_data, &orig_analysis_data);
    }

    return orig_analysis_data;
}

/* Performs analysis specific to a case. */
static void *
drbbdup_do_case_analysis(drbbdup_manager_t *manager, void *drcontext, void *tag,
                         instrlist_t *bb, instr_t *strt, const drbbdup_case_t *case_info,
                         void *orig_analysis_data)
{
    if (opts.analyze_case == NULL)
        return NULL;

    void *case_analysis_data = NULL;
    if (manager->enable_dup) {
        instrlist_t *case_bb = drbbdup_extract_single_bb_copy(drcontext, bb, strt, NULL);
        /* Let the user analyse the BB for the given case. */
        opts.analyze_case(drcontext, tag, case_bb, case_info->encoding, opts.user_data,
                          orig_analysis_data, &case_analysis_data);
        instrlist_clear_and_destroy(drcontext, case_bb);
    } else {
        /* For bb with no wanted copies, simply invoke the call-back with the original
         * bb.
         */
        opts.analyze_case(drcontext, tag, bb, case_info->encoding, opts.user_data,
                          orig_analysis_data, &case_analysis_data);
    }

    return case_analysis_data;
}

static dr_emit_flags_t
drbbdup_analyse_phase(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                      bool translating, void *user_data)
{
    drbbdup_case_t *case_info = NULL;
    instr_t *first = instrlist_first(bb);

    /* Store analysis data in thread storage. */
    drbbdup_per_thread *pt =
        (drbbdup_per_thread *)drmgr_get_tls_field(drcontext, tls_idx);

    app_pc pc = instr_get_app_pc(drbbdup_first_app(bb));
    ASSERT(pc != NULL, "pc cannot be NULL");

    dr_rwlock_read_lock(rw_lock);
    drbbdup_manager_t *manager =
        (drbbdup_manager_t *)hashtable_lookup(&manager_table, pc);
    ASSERT(manager != NULL, "manager cannot be NULL");

    /* Perform orig analysis - only done once regardless of how many copies. */
    pt->orig_analysis_data = drbbdup_do_orig_analysis(manager, drcontext, tag, bb, first);

    /* Perform analysis for default case. Note, we do the analysis even if the manager
     * does not have dups enabled.
     */
    case_info = &manager->default_case;
    ASSERT(case_info->is_defined, "default case must be defined");
    pt->default_analysis_data = drbbdup_do_case_analysis(
        manager, drcontext, tag, bb, first, case_info, pt->orig_analysis_data);

    /* Perform analysis for each (non-default) case. */
    if (manager->enable_dup) {
        ASSERT(manager->cases != NULL, "case information must exit");
        int i;
        for (i = 0; i < opts.dup_limit; i++) {
            case_info = &manager->cases[i];
            if (case_info->is_defined) {
                pt->case_analysis_data[i] =
                    drbbdup_do_case_analysis(manager, drcontext, tag, bb, first,
                                             case_info, pt->orig_analysis_data);
            }
        }
    }
    dr_rwlock_read_unlock(rw_lock);

    return DR_EMIT_DEFAULT;
}

/****************************************************************************
 * LINK/INSTRUMENTATION PHASE
 *
 * After the analysis phase, the link phase kicks in. The link phase
 * is responsible for linking the flow of execution to bbs
 * based on the case being handled. Essentially, it inserts the dispatcher.
 */

/* When control reaches a bb, we need to restore regs used by the dispatcher's jump.
 * This function inserts the restoration landing.
 */
static void
drbbdup_insert_landing_restoration(void *drcontext, instrlist_t *bb, instr_t *where,
                                   const drbbdup_manager_t *manager)
{
    if (!manager->are_flags_dead) {
        drbbdup_restore_register(drcontext, bb, where, 2, DRBBDUP_SCRATCH_REG);
        dr_restore_arith_flags_from_xax(drcontext, bb, where);
    }

    if (!manager->is_scratch_reg_dead)
        drbbdup_restore_register(drcontext, bb, where, 1, DRBBDUP_SCRATCH_REG);
}

/* Calculates hash index of a particular bb to access the hit table. */
static uint
drbbdup_get_hitcount_hash(intptr_t bb_id)
{
    uint hash = ((uint)bb_id) & (TABLE_SIZE - 1);
    ASSERT(hash < TABLE_SIZE, "index to hit table should be within bounds");
    return hash;
}

/* Insert encoding of runtime case by invoking user call-back. */
static void
drbbdup_encode_runtime_case(void *drcontext, drbbdup_per_thread *pt, void *tag,
                            instrlist_t *bb, instr_t *where, drbbdup_manager_t *manager)
{
    /* XXX i#4134: statistics -- insert code that tracks the number of times the fragment
     * is executed.
     */

    /* Spill scratch register and flags. We use drreg to check their liveness but
     * manually perform the spilling for finer control across branches used by the
     * dispatcher.
     */
    drreg_are_aflags_dead(drcontext, where, &manager->are_flags_dead);
    if (!manager->is_scratch_reg_dead)
        drbbdup_spill_register(drcontext, bb, where, 1, DRBBDUP_SCRATCH_REG);

    drreg_is_register_dead(drcontext, DRBBDUP_SCRATCH_REG, where,
                           &manager->is_scratch_reg_dead);
    if (!manager->are_flags_dead) {
        dr_save_arith_flags_to_xax(drcontext, bb, where);
        drbbdup_spill_register(drcontext, bb, where, 2, DRBBDUP_SCRATCH_REG);
        if (!manager->is_scratch_reg_dead)
            drbbdup_restore_register(drcontext, bb, where, 1, DRBBDUP_SCRATCH_REG);
    }

    /* Encoding is application-specific and therefore we need to user to define the
     * encoding of the runtime case. We invoke a user-defined call-back.
     */
    ASSERT(opts.insert_encode, "The encode call-back cannot be NULL");
    /* Note, we could tell the user not to reserve flags and scratch register since
     * drbbdup is doing that already. However, for flexibility/backwards compatibility
     * ease, this might not be the best approach.
     */
    opts.insert_encode(drcontext, tag, bb, where, opts.user_data, pt->orig_analysis_data);

    /* Restore all unreserved registers used by the call-back. */
    drreg_restore_all(drcontext, bb, where);

#ifdef X86_32
    /* Load the encoding to the scratch register.
     * The dispatcher could compare directly via mem, but this will
     * destroy micro-fusing (mem and immed).
     */
    opnd_t scratch_reg_opnd = opnd_create_reg(DRBBDUP_SCRATCH_REG);
    opnd_t opnd = drbbdup_get_encoding_opnd();
    instr_t *instr = INSTR_CREATE_mov_ld(drcontext, scratch_reg_opnd, opnd);
    instrlist_meta_preinsert(bb, where, instr);
#endif
}

/* At the start of a bb copy, dispatcher code is inserted. The runtime encoding
 * is compared with the encoding of the defined case, and if they match control
 * falls-through to execute the bb. Otherwise, control  branches to the next bb
 * via next_label.
 */
static void
drbbdup_insert_dispatch(void *drcontext, instrlist_t *bb, instr_t *where,
                        drbbdup_manager_t *manager, instr_t *next_label,
                        drbbdup_case_t *current_case)
{
    instr_t *instr;

    ASSERT(next_label != NULL, "the label to the next bb copy cannot be NULL");

#ifdef X86_64
    opnd_t scratch_reg_opnd = opnd_create_reg(DRBBDUP_SCRATCH_REG);
    instrlist_insert_mov_immed_ptrsz(drcontext, current_case->encoding, scratch_reg_opnd,
                                     bb, where, NULL, NULL);
    instr = INSTR_CREATE_cmp(drcontext, drbbdup_get_encoding_opnd(), scratch_reg_opnd);
    instrlist_meta_preinsert(bb, where, instr);
#elif X86_32
    /* Note, DRBBDUP_SCRATCH_REG contains the runtime case encoding. */
    opnd_t opnd = opnd_create_immed_uint(current_case->encoding, OPSZ_PTR);
    instr = INSTR_CREATE_cmp(drcontext, opnd_create_reg(DRBBDUP_SCRATCH_REG), opnd);
    instrlist_meta_preinsert(bb, where, instr);
#endif

    /* If runtime encoding not equal to encoding of current case, just jump to next.
     */
    instr = INSTR_CREATE_jcc(drcontext, OP_jnz, opnd_create_instr(next_label));
    instrlist_meta_preinsert(bb, where, instr);

    /* If fall-through, restore regs back to their original values. */
    drbbdup_insert_landing_restoration(drcontext, bb, where, manager);
}

/* Returns whether or not additional cases should be handled by checking if the
 * copy limit, defined by the user, has been reached.
 */
static bool
drbbdup_do_dynamic_handling(drbbdup_manager_t *manager)
{
    drbbdup_case_t *drbbdup_case;
    int i;
    for (i = 0; i < opts.dup_limit; i++) {
        drbbdup_case = &manager->cases[i];
        /* Search for empty undefined slot. */
        if (!drbbdup_case->is_defined)
            return true;
    }

    return false;
}

/* Increments the execution count of bails to default case. */
static void
drbbdup_inc_bail_count()
{
    dr_mutex_lock(stat_mutex);
    stats.bail_count++;
    dr_mutex_unlock(stat_mutex);
}

/* Insert trigger for dynamic case handling. */
static void
drbbdup_insert_dynamic_handling(void *drcontext, app_pc translation_pc, void *tag,
                                instrlist_t *bb, instr_t *where,
                                drbbdup_manager_t *manager)
{
    instr_t *instr;
    opnd_t opnd;

    instr_t *done_label = INSTR_CREATE_label(drcontext);
    opnd_t mask_opnd = opnd_create_reg(DRBBDUP_SCRATCH_REG);

    /* Check whether case limit has not been reached. */
    if (drbbdup_do_dynamic_handling(manager)) {
        drbbdup_case_t *default_info = &manager->default_case;
        ASSERT(default_info->is_defined, "default case must be defined");

        /* Jump if runtime encoding matches default encoding.
         * Unknown encoding encountered upon fall-through.
         */
        opnd = opnd_create_immed_uint((uintptr_t)default_info->encoding, OPSZ_PTR);
        instr = INSTR_CREATE_cmp(drcontext, mask_opnd, opnd);
        instrlist_meta_preinsert(bb, where, instr);

        instr = INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(done_label));
        instrlist_meta_preinsert(bb, where, instr);

        /* Don't bother insertion if threshold limit is zero. */
        if (opts.hit_threshold > 0) {
            /* Update hit count and check whether threshold is reached. */
            opnd_t hit_table_opnd = drbbdup_get_tls_raw_slot_opnd(DRBBDUP_HIT_TABLE_SLOT);

            /* Load the hit counter table. */
            instr = INSTR_CREATE_mov_ld(drcontext, mask_opnd, hit_table_opnd);
            instrlist_meta_preinsert(bb, where, instr);

            /* Register hit. */
            uint hash = drbbdup_get_hitcount_hash((intptr_t)translation_pc);
            opnd_t hit_count_opnd =
                OPND_CREATE_MEM16(DRBBDUP_SCRATCH_REG, hash * sizeof(ushort));
            opnd = opnd_create_immed_uint(1, OPSZ_2);
            instr = INSTR_CREATE_sub(drcontext, hit_count_opnd, opnd);
            instrlist_meta_preinsert(bb, where, instr);

            /* Load bb tag to register so that it can be accessed by outlined clean
             * call.
             */
            instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)tag, mask_opnd, bb,
                                             where, NULL, NULL);

            /* Jump if hit reaches zero. */
            opnd = opnd_create_pc(new_case_cache_pc);
            instr = INSTR_CREATE_jcc(drcontext, OP_jz, opnd);
            instrlist_meta_preinsert(bb, where, instr);

        } else {
            /* Load bb tag to register so that it can be accessed by outlined clean
             * call.
             */
            instr = INSTR_CREATE_mov_imm(drcontext, mask_opnd,
                                         opnd_create_immed_int((intptr_t)tag, OPSZ_PTR));
            instrlist_meta_preinsert(bb, where, instr);

            /* Jump to outlined clean call code for new case registration. */
            opnd = opnd_create_pc(new_case_cache_pc);
            instr = INSTR_CREATE_jmp(drcontext, opnd);
            instrlist_meta_preinsert(bb, where, instr);
        }
    }

    /* XXX i#4215: Use atomic counter when 64-bit sized integers can be used
     * on 32-bit platforms.
     */
    if (opts.is_stat_enabled) {
        /* Insert clean call so that we can lock stat_mutex. */
        dr_insert_clean_call(drcontext, bb, where, (void *)drbbdup_inc_bail_count, false,
                             0);
    }

    instrlist_meta_preinsert(bb, where, done_label);
}

/* Inserts code right before the last bb copy which is used to handle the default
 * case. */
static void
drbbdup_insert_dispatch_end(void *drcontext, app_pc translation_pc, void *tag,
                            instrlist_t *bb, instr_t *where, drbbdup_manager_t *manager)
{
    /* Check whether dynamic case handling is enabled by the user to handle an unkown
     * case encoding.
     */
    if (manager->enable_dynamic_handling) {
        drbbdup_insert_dynamic_handling(drcontext, translation_pc, tag, bb, where,
                                        manager);
    }

    /* Last bb version is always the default case. */
    drbbdup_insert_landing_restoration(drcontext, bb, where, manager);
}

static void
drbbdup_instrument_instr(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                         instr_t *where, drbbdup_per_thread *pt,
                         drbbdup_manager_t *manager)
{
    drbbdup_case_t *drbbdup_case = NULL;
    void *analysis_data = NULL;

    ASSERT(opts.instrument_instr, "instrument call-back function cannot be NULL");
    ASSERT(pt->case_index != DRBBDUP_IGNORE_INDEX, "case index cannot be ignored");

    if (pt->case_index == DRBBDUP_DEFAULT_INDEX) {
        /* Use default case. */
        drbbdup_case = &manager->default_case;
        analysis_data = pt->default_analysis_data;
    } else {
        ASSERT(pt->case_analysis_data != NULL,
               "container for analysis data cannot be NULL");
        ASSERT(pt->case_index >= 0 && pt->case_index < opts.dup_limit,
               "case index cannot be out-of-bounds");
        ASSERT(manager->enable_dup, "bb dup must be enabled");

        drbbdup_case = &manager->cases[pt->case_index];
        analysis_data = pt->case_analysis_data[pt->case_index];
    }

    ASSERT(drbbdup_case->is_defined, "case must be defined upon instrumentation");
    opts.instrument_instr(drcontext, tag, bb, instr, where, drbbdup_case->encoding,
                          opts.user_data, pt->orig_analysis_data, analysis_data);
}

/* Support different instrumentation for different bb copies. Tracks which case is
 * currently being considered via an index (namely pt->case_index) in thread-local
 * storage, and update this index upon encountering the start/end of bb copies.
 */
static void
drbbdup_instrument_dups(void *drcontext, app_pc pc, void *tag, instrlist_t *bb,
                        instr_t *instr, drbbdup_per_thread *pt,
                        drbbdup_manager_t *manager)
{
    drbbdup_case_t *drbbdup_case = NULL;
    ASSERT(manager->cases != NULL, "case info should not be NULL");
    ASSERT(pt != NULL, "thread-local storage should not be NULL");

    instr_t *last = instrlist_last_app(bb);
    bool is_last_special = drbbdup_is_special_instr(last);

    /* Insert runtime case encoding at start. */
    if (drmgr_is_first_instr(drcontext, instr)) {
        ASSERT(pt->case_index == -1, "case index should start at -1");
        drbbdup_encode_runtime_case(drcontext, pt, tag, bb, instr, manager);
    }

    if (drbbdup_is_at_start(instr)) {
        instr_t *next_instr = instr_get_next(instr); /* Skip START label. */
        instr_t *end_instr = drbbdup_next_end(next_instr);
        ASSERT(end_instr != NULL, "end instruction cannot be NULL");

        /* Cache first, first nonlabel and last instructions. */
        if (next_instr == end_instr && is_last_special) {
            pt->first_instr = last;
            pt->first_nonlabel_instr = last;
        } else {
            pt->first_instr = next_instr; /* Update cache to first instr. */
            instr_t *first_non_label = next_instr;
            while (instr_is_label(first_non_label) && first_non_label != end_instr) {
                first_non_label = instr_get_next(first_non_label);
            }
            if (first_non_label == end_instr && is_last_special) {
                pt->first_nonlabel_instr = last;
            } else {
                pt->first_nonlabel_instr = first_non_label;
            }
        }

        if (is_last_special)
            pt->last_instr = last;
        else
            pt->last_instr = instr_get_prev(end_instr); /* Update cache to last instr. */

        /* Check whether we reached the last bb version (namely the default case). */
        instr_t *next_bb_label = drbbdup_next_start(end_instr);
        if (next_bb_label == NULL) {
            pt->case_index = DRBBDUP_DEFAULT_INDEX; /* Refer to default. */
            drbbdup_insert_dispatch_end(drcontext, pc, tag, bb, next_instr, manager);
        } else {
            /* We have reached the start of a new bb version (not the last one). */
            bool found = false;
            int i;
            for (i = pt->case_index + 1; i < opts.dup_limit; i++) {
                drbbdup_case = &manager->cases[i];
                if (drbbdup_case->is_defined) {
                    found = true;
                    break;
                }
            }
            ASSERT(found, "mismatch between bb copy count and case count detected");
            ASSERT(drbbdup_case->is_defined, "the found case cannot be undefined");
            ASSERT(pt->case_index + 1 == i,
                   "the next case considered should be the next increment");
            pt->case_index = i; /* Move on to the next case. */
            drbbdup_insert_dispatch(drcontext, bb,
                                    next_instr /* insert after START label. */, manager,
                                    next_bb_label, drbbdup_case);
        }

        /* XXX i#4134: statistics -- insert code that tracks the number of times the
         * current case (pt->case_index) is executed.
         */
    } else if (drbbdup_is_at_end(instr)) {
        /* Handle last special instruction (if present). */
        if (is_last_special) {
            drbbdup_instrument_instr(drcontext, tag, bb, last, instr, pt, manager);
            if (pt->case_index == DRBBDUP_DEFAULT_INDEX) {
                pt->case_index =
                    DRBBDUP_IGNORE_INDEX; /* Ignore remaining instructions. */
            }
        }
        drreg_restore_all(drcontext, bb, instr);
    } else if (pt->case_index == DRBBDUP_IGNORE_INDEX) {
        /* Ignore instruction. */
        ASSERT(drbbdup_is_special_instr(instr), "ignored instr should be cti or syscall");
    } else {
        /* Instrument instructions inside the bb specified by pt->case_index. */
        drbbdup_instrument_instr(drcontext, tag, bb, instr, instr, pt, manager);
    }
}

static void
drbbdup_instrument_without_dups(void *drcontext, void *tag, instrlist_t *bb,
                                instr_t *instr, drbbdup_per_thread *pt,
                                drbbdup_manager_t *manager)
{
    ASSERT(manager->cases == NULL, "case info should not be needed");
    ASSERT(pt != NULL, "thread-local storage should not be NULL");

    if (drmgr_is_first_instr(drcontext, instr)) {
        pt->first_instr = instr;
        pt->first_nonlabel_instr = instrlist_first_nonlabel(bb);
        pt->last_instr = instrlist_last(bb);
        ASSERT(drmgr_is_last_instr(drcontext, pt->last_instr), "instr should be last");
    }

    /* No dups wanted! Just instrument normally using default case. */
    ASSERT(pt->case_index == DRBBDUP_DEFAULT_INDEX,
           "case index should direct to default case");
    drbbdup_instrument_instr(drcontext, tag, bb, instr, instr, pt, manager);
}

/* Invokes user call-backs to destroy analysis data.
 */
static void
drbbdup_destroy_all_analyses(void *drcontext, drbbdup_manager_t *manager,
                             drbbdup_per_thread *pt)
{
    if (opts.destroy_case_analysis != NULL) {
        if (pt->case_analysis_data != NULL) {
            int i;
            for (i = 0; i < opts.dup_limit; i++) {
                if (pt->case_analysis_data[i] != NULL) {
                    opts.destroy_case_analysis(drcontext, manager->cases[i].encoding,
                                               opts.user_data, pt->orig_analysis_data,
                                               pt->case_analysis_data[i]);
                    pt->case_analysis_data[i] = NULL;
                }
            }
        }
        if (pt->default_analysis_data != NULL) {
            opts.destroy_case_analysis(drcontext, manager->default_case.encoding,
                                       opts.user_data, pt->orig_analysis_data,
                                       pt->default_analysis_data);
            pt->default_analysis_data = NULL;
        }
    }

    if (opts.destroy_orig_analysis != NULL) {
        if (pt->orig_analysis_data != NULL) {
            opts.destroy_orig_analysis(drcontext, opts.user_data, pt->orig_analysis_data);
            pt->orig_analysis_data = NULL;
        }
    }
}

static dr_emit_flags_t
drbbdup_link_phase(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                   bool for_trace, bool translating, void *user_data)
{
    drbbdup_per_thread *pt =
        (drbbdup_per_thread *)drmgr_get_tls_field(drcontext, tls_idx);

    app_pc pc = instr_get_app_pc(drbbdup_first_app(bb));
    ASSERT(pc != NULL, "pc cannot be NULL");

    ASSERT(opts.instrument_instr != NULL, "instrumentation call-back must not be NULL");

    /* Start off with the default case index. */
    if (drmgr_is_first_instr(drcontext, instr))
        pt->case_index = DRBBDUP_DEFAULT_INDEX;

    dr_rwlock_read_lock(rw_lock);
    drbbdup_manager_t *manager =
        (drbbdup_manager_t *)hashtable_lookup(&manager_table, pc);
    ASSERT(manager != NULL, "manager cannot be NULL");

    if (manager->enable_dup)
        drbbdup_instrument_dups(drcontext, pc, tag, bb, instr, pt, manager);
    else
        drbbdup_instrument_without_dups(drcontext, tag, bb, instr, pt, manager);

    if (drmgr_is_last_instr(drcontext, instr))
        drbbdup_destroy_all_analyses(drcontext, manager, pt);

    dr_rwlock_read_unlock(rw_lock);

    return DR_EMIT_DEFAULT;
}

static bool
drbbdup_encoding_already_included(drbbdup_manager_t *manager, uintptr_t encoding_check)
{
    drbbdup_case_t *drbbdup_case;
    if (manager->enable_dup) {
        int i;
        for (i = 0; i < opts.dup_limit; i++) {
            drbbdup_case = &manager->cases[i];
            if (drbbdup_case->is_defined && drbbdup_case->encoding == encoding_check)
                return true;
        }
    }

    /* Check default case. */
    drbbdup_case = &manager->default_case;
    if (drbbdup_case->is_defined && drbbdup_case->encoding == encoding_check)
        return true;

    return false;
}

static bool
drbbdup_include_encoding(drbbdup_manager_t *manager, uintptr_t new_encoding)
{
    if (manager->enable_dup) {
        int i;
        drbbdup_case_t *dup_case;
        for (i = 0; i < opts.dup_limit; i++) {
            dup_case = &manager->cases[i];
            if (!dup_case->is_defined) {
                dup_case->is_defined = true;
                dup_case->encoding = new_encoding;
                return true;
            }
        }
    }

    return false;
}

/****************************************************************************
 * Dynamic case handling via flushing.
 */

static void
drbbdup_prepare_redirect(dr_mcontext_t *mcontext, drbbdup_manager_t *manager,
                         app_pc bb_pc)
{
    /* Restore flags and scratch reg to their original app values. */
    if (!manager->are_flags_dead) {
        reg_t val = (reg_t)drbbdup_get_tls_raw_slot_val(DRBBDUP_FLAG_REG_SLOT);
        mcontext->xflags = dr_merge_arith_flags(mcontext->xflags, val);
    }
    if (!manager->is_scratch_reg_dead) {
        reg_set_value(DRBBDUP_SCRATCH_REG, mcontext,
                      (reg_t)drbbdup_get_tls_raw_slot_val(DRBBDUP_XAX_REG_SLOT));
    }

    mcontext->pc = bb_pc; /* redirect execution to the start of the bb. */
}

static void
drbbdup_handle_new_case()
{
    void *drcontext = dr_get_current_drcontext();

    drbbdup_per_thread *pt =
        (drbbdup_per_thread *)drmgr_get_tls_field(drcontext, tls_idx);

    /* Must use DR_MC_ALL due to dr_redirect_execution. */
    dr_mcontext_t mcontext = {
        sizeof(mcontext),
        DR_MC_ALL,
    };
    dr_get_mcontext(drcontext, &mcontext);

    /* Scratch register holds the tag. */
    void *tag = (void *)reg_get_value(DRBBDUP_SCRATCH_REG, &mcontext);

    instrlist_t *ilist = decode_as_bb(drcontext, dr_fragment_app_pc(tag));
    app_pc pc = instr_get_app_pc(drbbdup_first_app(ilist));
    ASSERT(pc != NULL, "pc cannot be NULL");

    bool do_flush = false;

    /* Get the missing case. */
    uintptr_t new_encoding = drbbdup_get_tls_raw_slot_val(DRBBDUP_ENCODING_SLOT);

    dr_rwlock_write_lock(rw_lock);
    drbbdup_manager_t *manager =
        (drbbdup_manager_t *)hashtable_lookup(&manager_table, pc);
    ASSERT(manager != NULL, "manager cannot be NULL");
    ASSERT(manager->enable_dup, "duplication should be enabled");
    ASSERT(new_encoding != manager->default_case.encoding,
           "unhandled encoding cannot be the default case");

    /* Could have been turned off potentially by another thread. */
    if (manager->enable_dynamic_handling) {
        /* Case already registered potentially by another thread. */
        if (!drbbdup_encoding_already_included(manager, new_encoding)) {
            /* By default, do case gen. */
            bool do_gen = true;
            if (opts.allow_gen != NULL) {
                do_gen =
                    opts.allow_gen(drcontext, tag, ilist, new_encoding,
                                   &manager->enable_dynamic_handling, opts.user_data);
            }
            if (do_gen)
                drbbdup_include_encoding(manager, new_encoding);

            /* Flush only if a new case needs to be generated or
             * dynamic handling has been disabled.
             */
            do_flush = do_gen || !manager->enable_dynamic_handling;
            if (do_flush) {
                /* Mark that flushing is happening for drbbdup. */
                manager->is_gen = true;
                /* Increment counter so manager won't get freed due to flushing. */
                manager->ref_counter++;
            }

            if (opts.is_stat_enabled) {
                dr_mutex_lock(stat_mutex);
                if (do_gen)
                    stats.gen_count++;
                if (!manager->enable_dynamic_handling)
                    stats.no_dynamic_handling_count++;
                dr_mutex_unlock(stat_mutex);
            }
        }
    }
    /* Regardless of whether or not flushing is going to happen, redirection will
     * always be performed.
     */
    drbbdup_prepare_redirect(&mcontext, manager, pc);

    dr_rwlock_write_unlock(rw_lock);

    instrlist_clear_and_destroy(drcontext, ilist);

    /* Refresh hit counter. */
    if (opts.hit_threshold > 0) {
        uint hash = drbbdup_get_hitcount_hash((intptr_t)pc);
        DR_ASSERT(pt->hit_counts[hash] == 0);
        pt->hit_counts[hash] = opts.hit_threshold; /* Reset threshold. */
    }

    /* Delete bb fragment. */
    if (do_flush) {
        LOG(drcontext, DR_LOG_ALL, 2,
            "%s Found new case! Going to flush bb with"
            "pc %p to generate a copy to handle the new case.\n",
            __FUNCTION__, pc);

        /* No locks held upon fragment deletion. */
        /* XXX i#3778: To include once we support specific fragment deletion. */
#if 0
        dr_delete_shared_fragment(tag);
#endif
    }

    dr_redirect_execution(&mcontext);
}

static app_pc
init_fp_cache(void (*clean_call_func)())
{
    app_pc cache_pc;
    instrlist_t *ilist;
    void *drcontext = dr_get_current_drcontext();
    size_t size = dr_page_size();
    ilist = instrlist_create(drcontext);

    dr_insert_clean_call(drcontext, ilist, NULL, (void *)clean_call_func, false, 0);

    /* Allocate code cache, and set Read-Write-Execute permissions using
     * dr_nonheap_alloc function.
     */
    cache_pc = (app_pc)dr_nonheap_alloc(
        size, DR_MEMPROT_READ | DR_MEMPROT_WRITE | DR_MEMPROT_EXEC);
    byte *end = instrlist_encode(drcontext, ilist, cache_pc, true);
    DR_ASSERT(end - cache_pc <= (int)size);

    instrlist_clear_and_destroy(drcontext, ilist);

    /* Change the permission Read-Write-Execute permissions. */
    dr_memory_protect(cache_pc, size, DR_MEMPROT_READ | DR_MEMPROT_EXEC);

    return cache_pc;
}

static void
destroy_fp_cache(app_pc cache_pc)
{
    ASSERT(cache_pc, "Code cache should not be NULL");
    dr_nonheap_free(cache_pc, dr_page_size());
}

/****************************************************************************
 * Frag Deletion
 */

static void
deleted_frag(void *drcontext, void *tag)
{
    /* Note, drcontext could be NULL during process exit. */
    if (drcontext == NULL)
        return;

    app_pc bb_pc = dr_fragment_app_pc(tag);

    dr_rwlock_write_lock(rw_lock);

    drbbdup_manager_t *manager =
        (drbbdup_manager_t *)hashtable_lookup(&manager_table, bb_pc);
    if (manager != NULL) {
        ASSERT(manager->ref_counter > 0, "ref count should be greater than zero");
        manager->ref_counter--;
        if (manager->ref_counter <= 0)
            hashtable_remove(&manager_table, bb_pc);
    }

    dr_rwlock_write_unlock(rw_lock);
}

/****************************************************************************
 * INTERFACE
 */

drbbdup_status_t
drbbdup_register_case_encoding(void *drbbdup_ctx, uintptr_t encoding)
{
    drbbdup_manager_t *manager = (drbbdup_manager_t *)drbbdup_ctx;

    if (drbbdup_encoding_already_included(manager, encoding))
        return DRBBDUP_ERROR_CASE_ALREADY_REGISTERED;

    if (drbbdup_include_encoding(manager, encoding))
        return DRBBDUP_SUCCESS;
    else
        return DRBBDUP_ERROR_CASE_LIMIT_REACHED;
}

drbbdup_status_t
drbbdup_is_first_instr(void *drcontext, instr_t *instr, bool *is_start)
{
    if (instr == NULL || is_start == NULL)
        return DRBBDUP_ERROR_INVALID_PARAMETER;

    drbbdup_per_thread *pt =
        (drbbdup_per_thread *)drmgr_get_tls_field(drcontext, tls_idx);
    if (pt == NULL)
        return DRBBDUP_ERROR;

    *is_start = pt->first_instr == instr;

    return DRBBDUP_SUCCESS;
}

drbbdup_status_t
drbbdup_is_first_nonlabel_instr(void *drcontext, instr_t *instr, bool *is_nonlabel)
{
    if (instr == NULL || is_nonlabel == NULL)
        return DRBBDUP_ERROR_INVALID_PARAMETER;

    drbbdup_per_thread *pt =
        (drbbdup_per_thread *)drmgr_get_tls_field(drcontext, tls_idx);
    if (pt == NULL)
        return DRBBDUP_ERROR;

    *is_nonlabel = pt->first_nonlabel_instr == instr;

    return DRBBDUP_SUCCESS;
}

drbbdup_status_t
drbbdup_is_last_instr(void *drcontext, instr_t *instr, bool *is_last)
{
    if (instr == NULL || is_last == NULL)
        return DRBBDUP_ERROR_INVALID_PARAMETER;

    drbbdup_per_thread *pt =
        (drbbdup_per_thread *)drmgr_get_tls_field(drcontext, tls_idx);
    if (pt == NULL)
        return DRBBDUP_ERROR;

    *is_last = pt->last_instr == instr;

    return DRBBDUP_SUCCESS;
}

drbbdup_status_t
drbbdup_get_stats(OUT drbbdup_stats_t *stats_in)
{
    if (!opts.is_stat_enabled)
        return DRBBDUP_ERROR_UNSET_FEATURE;
    if (stats_in == NULL || stats_in->struct_size == 0 ||
        stats_in->struct_size > stats.struct_size)
        return DRBBDUP_ERROR_INVALID_PARAMETER;

    dr_mutex_lock(stat_mutex);
    memcpy(stats_in, &stats, stats_in->struct_size);
    dr_mutex_unlock(stat_mutex);
    return DRBBDUP_SUCCESS;
}

/****************************************************************************
 * THREAD INIT AND EXIT
 */

static void
drbbdup_destroy_manager(void *manager_opaque)
{
    drbbdup_manager_t *manager = (drbbdup_manager_t *)manager_opaque;
    ASSERT(manager != NULL, "manager should not be NULL");

    if (manager->enable_dup && manager->cases != NULL) {
        ASSERT(opts.dup_limit > 0, "dup limit should be greater than zero");
        dr_global_free(manager->cases, sizeof(drbbdup_case_t) * opts.dup_limit);
    }
    dr_global_free(manager, sizeof(drbbdup_manager_t));
}

static void
drbbdup_thread_init(void *drcontext)
{
    drbbdup_per_thread *pt =
        (drbbdup_per_thread *)dr_thread_alloc(drcontext, sizeof(drbbdup_per_thread));

    pt->case_index = 0;
    pt->orig_analysis_data = NULL;
    ASSERT(opts.dup_limit > 0, "dup limit should be greater than zero");
    pt->case_analysis_data = dr_thread_alloc(drcontext, sizeof(void *) * opts.dup_limit);
    memset(pt->case_analysis_data, 0, sizeof(void *) * opts.dup_limit);

    /* Init hit table. */
    for (int i = 0; i < TABLE_SIZE; i++)
        pt->hit_counts[i] = opts.hit_threshold;
    drbbdup_set_tls_raw_slot_val(DRBBDUP_HIT_TABLE_SLOT, (uintptr_t)pt->hit_counts);

    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
}

static void
drbbdup_thread_exit(void *drcontext)
{
    drbbdup_per_thread *pt =
        (drbbdup_per_thread *)drmgr_get_tls_field(drcontext, tls_idx);
    ASSERT(pt != NULL, "thread-local storage should not be NULL");
    ASSERT(opts.dup_limit > 0, "dup limit should be greater than zero");

    dr_thread_free(drcontext, pt->case_analysis_data, sizeof(void *) * opts.dup_limit);
    dr_thread_free(drcontext, pt, sizeof(drbbdup_per_thread));
}

/****************************************************************************
 * INIT AND EXIT
 */

static bool
drbbdup_check_options(drbbdup_options_t *ops_in)
{
    if (ops_in != NULL && ops_in->set_up_bb_dups != NULL &&
        ops_in->insert_encode != NULL && ops_in->instrument_instr &&
        ops_in->dup_limit > 0)
        return true;

    return false;
}

drbbdup_status_t
drbbdup_init(drbbdup_options_t *ops_in)
{
    /* Return with error if drbbdup has already been initialised. */
    if (ref_count != 0)
        return DRBBDUP_ERROR_ALREADY_INITIALISED;

    if (!drbbdup_check_options(ops_in))
        return DRBBDUP_ERROR_INVALID_PARAMETER;
    memcpy(&opts, ops_in, sizeof(drbbdup_options_t));

    drreg_options_t drreg_ops = { sizeof(drreg_ops), 0 /* no regs needed */, false, NULL,
                                  true };
    drmgr_priority_t app2app_priority = { sizeof(drmgr_priority_t),
                                          DRMGR_PRIORITY_APP2APP_NAME_DRBBDUP, NULL, NULL,
                                          DRMGR_PRIORITY_APP2APP_DRBBDUP };
    drmgr_priority_t insert_priority = { sizeof(drmgr_priority_t),
                                         DRMGR_PRIORITY_INSERT_NAME_DRBBDUP, NULL, NULL,
                                         DRMGR_PRIORITY_INSERT_DRBBDUP };

    if (!drmgr_register_bb_app2app_event(drbbdup_duplicate_phase, &app2app_priority) ||
        !drmgr_register_bb_instrumentation_ex_event(
            NULL, drbbdup_analyse_phase, drbbdup_link_phase, NULL, &insert_priority) ||
        !drmgr_register_thread_init_event(drbbdup_thread_init) ||
        !drmgr_register_thread_exit_event(drbbdup_thread_exit) ||
        !dr_raw_tls_calloc(&tls_raw_reg, &tls_raw_base, DRBBDUP_SLOT_COUNT, 0) ||
        drreg_init(&drreg_ops) != DRREG_SUCCESS)
        return DRBBDUP_ERROR;

    dr_register_delete_event(deleted_frag);

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1)
        return DRBBDUP_ERROR;

    new_case_cache_pc = init_fp_cache(drbbdup_handle_new_case);
    if (new_case_cache_pc == NULL)
        return DRBBDUP_ERROR;

    /* Initialise hash table that keeps track of defined cases per
     * basic block.
     */
    hashtable_init_ex(&manager_table, HASH_BIT_TABLE, HASH_INTPTR, false, false,
                      drbbdup_destroy_manager, NULL, NULL);

    rw_lock = dr_rwlock_create();
    if (rw_lock == NULL)
        return DRBBDUP_ERROR;

    if (opts.is_stat_enabled) {
        memset(&stats, 0, sizeof(drbbdup_stats_t));
        stats.struct_size = sizeof(drbbdup_stats_t);
        stat_mutex = dr_mutex_create();
        if (stat_mutex == NULL)
            return DRBBDUP_ERROR;
    }

    ref_count++;

    return DRBBDUP_SUCCESS;
}

drbbdup_status_t
drbbdup_exit(void)
{
    DR_ASSERT(ref_count > 0);
    ref_count--;

    if (ref_count == 0) {
        destroy_fp_cache(new_case_cache_pc);

        if (!drmgr_unregister_bb_app2app_event(drbbdup_duplicate_phase) ||
            !drmgr_unregister_bb_instrumentation_ex_event(NULL, drbbdup_analyse_phase,
                                                          drbbdup_link_phase, NULL) ||
            !drmgr_unregister_thread_init_event(drbbdup_thread_init) ||
            !drmgr_unregister_thread_exit_event(drbbdup_thread_exit) ||
            !dr_raw_tls_cfree(tls_raw_base, DRBBDUP_SLOT_COUNT) ||
            !drmgr_unregister_tls_field(tls_idx) ||
            !dr_unregister_delete_event(deleted_frag) || drreg_exit() != DRREG_SUCCESS)
            return DRBBDUP_ERROR;

        hashtable_delete(&manager_table);
        dr_rwlock_destroy(rw_lock);

        if (opts.is_stat_enabled)
            dr_mutex_destroy(stat_mutex);

    } else {
        /* Cannot have more than one initialisation of drbbdup. */
        return DRBBDUP_ERROR;
    }

    return DRBBDUP_SUCCESS;
}
