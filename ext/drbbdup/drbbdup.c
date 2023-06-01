/* **********************************************************
 * Copyright (c) 2013-2023 Google, Inc.   All rights reserved.
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

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "hashtable.h"
#include "drbbdup.h"
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include "../ext_utils.h"

#undef drmgr_is_first_instr
#undef drmgr_is_first_nonlabel_instr
#undef drmgr_is_last_instr

#ifdef DEBUG
#    define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#    define LOG(dc, mask, level, ...) dr_log(dc, mask, level, __VA_ARGS__)
#    define IF_DEBUG(x) x
#else
#    define ASSERT(x, msg) /* nothing */
#    define LOG(dc, mask, level, ...)
#    define IF_DEBUG(x)
#endif

/* DynamoRIO Basic Block Duplication Extension: a code builder that
 * duplicates code of basic blocks and dispatches control according to runtime
 * conditions so that different instrumentation may be efficiently executed.
 */

#define HASH_BIT_TABLE 13

/* Definitions for drbbdup's hit-table that drives dynamic case handling.
 * Essentially, a hash-table tracks which BBs are frequently encountering
 * new unhandled cases.
 */
#define TABLE_SIZE 65536 /* Must be a power of 2 to perform efficient mod. */

#ifdef AARCHXX
#    define MAX_IMMED_IN_CMP 255
#endif

typedef enum {
    DRBBDUP_ENCODING_SLOT = 0, /* Used as a spill slot for dynamic case generation. */
    DRBBDUP_SCRATCH_REG_SLOT = 1,
    DRBBDUP_FLAG_REG_SLOT = 2,
    DRBBDUP_HIT_TABLE_SLOT = 3,
#ifdef AARCHXX
    DRBBDUP_SCRATCH_REG2_SLOT,
#endif
    DRBBDUP_SLOT_COUNT,
} drbbdup_thread_slots_t;

/* A scratch register used by drbbdup's dispatcher. */
#define DRBBDUP_SCRATCH_REG IF_X86_ELSE(DR_REG_XAX, IF_RISCV64_ELSE(DR_REG_A0, DR_REG_R0))
#define DRBBDUP_SCRATCH_REG_NO_FLAGS \
    IF_X86_ELSE(DR_REG_XCX, IF_RISCV64_ELSE(DR_REG_A0, DR_REG_R0))
#ifdef AARCHXX
/* RISC architectures need a 2nd scratch register. */
#    define DRBBDUP_SCRATCH_REG2 DR_REG_R1
#endif

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
    bool enable_dup;              /* Denotes whether to duplicate blocks. */
    bool enable_dynamic_handling; /* Denotes whether to dynamically generate cases. */
#if !defined(RISCV64)
    bool are_flags_dead; /* Denotes whether flags are dead at the start of a bb. */
#endif
    bool is_scratch_reg_dead; /* Denotes whether DRBBDUP_SCRATCH_REG is dead at start. */
    reg_id_t scratch_reg;
#ifdef AARCHXX
    bool is_scratch_reg2_needed;
    bool is_scratch_reg2_dead; /* If _needed, is DRBBDUP_SCRATCH_REG2 dead at start. */
#endif
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
    hashtable_t manager_table; /* Maps bbs with book-keeping data (for thread-private
                                  caches only). */
    int case_index; /* Used to keep track of the current case during insertion. */
    bool inserted_restore_all;     /* Track if we need to restore regs at the end of the
                                      block.  */
    void *orig_analysis_data;      /* Analysis data accessible for all cases. */
    void *default_analysis_data;   /* Analysis data specific to default case. */
    void **case_analysis_data;     /* Analysis data specific to cases. */
    uint16_t *hit_counts;          /* Keeps track of hit-counts of unhandled cases. */
    instr_t *first_instr;          /* The first instr of the bb copy being considered. */
    instr_t *first_nonlabel_instr; /* The first non label instr of the bb copy. */
    instr_t *last_instr;           /* The last instr of the bb copy being considered. */
    byte *tls_seg_base;            /* For access from another thread. */
} drbbdup_per_thread;

static bool is_thread_private = false; /* Denotes whether DR caches are thread-private. */
static int drbbdup_init_count = 0;     /* Instance count of drbbdup. */
static hashtable_t global_manager_table; /* Maps bbs with book-keeping data. */
static drbbdup_options_t opts;
static void *rw_lock = NULL;

/* For tracking statistics. */
static void *stat_mutex = NULL;
static drbbdup_stats_t stats;

/* An outlined code cache (storing a clean call) for dynamically generating a case. */
static app_pc new_case_cache_pc = NULL;
static void *case_cache_mutex = NULL;

static int tls_idx = -1; /* For thread local storage info. */
static reg_id_t tls_raw_reg;
static uint tls_raw_base;

static bool
drbbdup_encoding_already_included(drbbdup_manager_t *manager, uintptr_t encoding_check,
                                  bool check_default);

static void
drbbdup_handle_new_case();

static app_pc
init_fp_cache(void (*clean_call_func)());

static instr_t *
drbbdup_first_app(instrlist_t *bb);

static uintptr_t *
drbbdup_get_tls_raw_slot_addr(void *drcontext, drbbdup_thread_slots_t slot_idx)
{
    ASSERT(0 <= slot_idx && slot_idx < DRBBDUP_SLOT_COUNT, "out-of-bounds slot index");
    /* We cannot call dr_get_dr_segment_base() here since we need to support
     * being called from another thread so we use our stored value.
     */
    drbbdup_per_thread *pt =
        (drbbdup_per_thread *)drmgr_get_tls_field(drcontext, tls_idx);
    byte *base = pt->tls_seg_base;
    return (uintptr_t *)(base + tls_raw_base + slot_idx * sizeof(uintptr_t));
}

static void
drbbdup_set_tls_raw_slot_val(void *drcontext, drbbdup_thread_slots_t slot_idx,
                             uintptr_t val)
{
    uintptr_t *addr = drbbdup_get_tls_raw_slot_addr(drcontext, slot_idx);
    *addr = val;
}

static uintptr_t
drbbdup_get_tls_raw_slot_val(void *drcontext, drbbdup_thread_slots_t slot_idx)
{
    uintptr_t *addr = drbbdup_get_tls_raw_slot_addr(drcontext, slot_idx);
    return *addr;
}

static opnd_t
drbbdup_get_tls_raw_slot_opnd(void *drcontext, drbbdup_thread_slots_t slot_idx)
{
    return dr_raw_tls_opnd(drcontext, tls_raw_reg,
                           tls_raw_base + (slot_idx * (sizeof(void *))));
}

static void
drbbdup_spill_register(void *drcontext, instrlist_t *ilist, instr_t *where,
                       drbbdup_thread_slots_t slot_idx, reg_id_t reg_id)
{
    opnd_t slot_opnd = drbbdup_get_tls_raw_slot_opnd(drcontext, slot_idx);
    instr_t *instr = XINST_CREATE_store(drcontext, slot_opnd, opnd_create_reg(reg_id));
    instrlist_meta_preinsert(ilist, where, instr);
}

static void
drbbdup_restore_register(void *drcontext, instrlist_t *ilist, instr_t *where,
                         drbbdup_thread_slots_t slot_idx, reg_id_t reg_id)
{
    opnd_t slot_opnd = drbbdup_get_tls_raw_slot_opnd(drcontext, slot_idx);
    instr_t *instr = XINST_CREATE_load(drcontext, opnd_create_reg(reg_id), slot_opnd);
    instrlist_meta_preinsert(ilist, where, instr);
}

/* Returns whether or not instr is a special instruction that must be the last instr in a
 * bb in accordance to DR rules.
 */
static bool
drbbdup_is_special_instr(instr_t *instr)
{
    return instr != NULL &&
        (instr_is_syscall(instr) || instr_is_cti(instr) || instr_is_ubr(instr) ||
         instr_is_interrupt(instr) IF_AARCH64(|| instr_get_opcode(instr) == OP_isb));
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
    for (i = 0; i < opts.non_default_case_limit; i++) {
        /* If case is defined, increment the counter. */
        if (manager->cases[i].is_defined)
            count++;
    }

    return count;
}

/* Returns whether there are only two cases and one has a zero encoding. */
static bool
drbbdup_case_zero_vs_nonzero(drbbdup_manager_t *manager)
{
    ASSERT(manager != NULL, "should not be NULL");
    if (manager->enable_dynamic_handling)
        return false; /* More cases could be added. */
    uintptr_t nondefault_encoding = 0;
    bool found = false;
    for (int i = 0; i < opts.non_default_case_limit; i++) {
        if (manager->cases[i].is_defined) {
            if (found)
                return false;
            found = true;
            nondefault_encoding = manager->cases[i].encoding;
        }
    }
    ASSERT(found, "must be one non-default case");
    return nondefault_encoding == 0 || manager->default_case.encoding == 0;
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
    /* This per-block memory can add up: for 2.5M basic blocks we can take up >512M
     * of space, which if it's in the limited-size vmcode region is a problem.
     * We thus explicitly request unreachable heap.
     * XXX: Maybe DR should break compatibility and change the default.
     */
    drbbdup_manager_t *manager = dr_custom_alloc(
        /* We want the global heap for which NULL for the drcontext is required. */
        NULL, 0, sizeof(drbbdup_manager_t), DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    memset(manager, 0, sizeof(drbbdup_manager_t));

    manager->cases = NULL;
    ASSERT(opts.non_default_case_limit > 0, "dup limit should be greater than zero");
    manager->cases =
        dr_custom_alloc(NULL, 0, sizeof(drbbdup_case_t) * opts.non_default_case_limit,
                        DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    memset(manager->cases, 0, sizeof(drbbdup_case_t) * opts.non_default_case_limit);
    manager->enable_dup = true;
    manager->enable_dynamic_handling = true;
    manager->is_gen = false;
    manager->scratch_reg = DRBBDUP_SCRATCH_REG;

    ASSERT(opts.set_up_bb_dups != NULL, "set up call-back cannot be NULL");
    manager->default_case.encoding =
        opts.set_up_bb_dups(manager, drcontext, tag, bb, &manager->enable_dup,
                            &manager->enable_dynamic_handling, opts.user_data);
    DR_ASSERT_MSG(opts.max_case_encoding == 0 ||
                      manager->default_case.encoding <= opts.max_case_encoding,
                  "default case encoding > specifed max_case_encoding");
    /* Default case encoding should not be already registered. */
    DR_ASSERT_MSG(
        !drbbdup_encoding_already_included(manager, manager->default_case.encoding,
                                           false /* don't check default case */),
        "default case encoding cannot be already registered");
    /* XXX i#3778: To remove once we support specific fragment deletion. */
    DR_ASSERT_MSG(!manager->enable_dynamic_handling,
                  "dynamic case generation is not yet supported");
    if (opts.never_enable_dynamic_handling) {
        DR_ASSERT_MSG(!manager->enable_dynamic_handling,
                      "dynamic case generation was disabled globally: cannot enable");
    }

    /* Check whether user wants copies for this particular bb. */
    if (!manager->enable_dup && manager->cases != NULL) {
        /* Multiple cases not wanted. Destroy cases. */
        dr_custom_free(NULL, 0, manager->cases,
                       sizeof(drbbdup_case_t) * opts.non_default_case_limit);
        manager->cases = NULL;
    }

    manager->default_case.is_defined = true;
    return manager;
}

static void
drbbdup_destroy_manager(void *manager_opaque)
{
    drbbdup_manager_t *manager = (drbbdup_manager_t *)manager_opaque;
    ASSERT(manager != NULL, "manager should not be NULL");

    if (manager->enable_dup && manager->cases != NULL) {
        ASSERT(opts.non_default_case_limit > 0, "dup limit should be greater than zero");
        dr_custom_free(NULL, 0, manager->cases,
                       sizeof(drbbdup_case_t) * opts.non_default_case_limit);
    }
    dr_custom_free(NULL, 0, manager, sizeof(drbbdup_manager_t));
}

/* This must be called prior to inserting drbbdup's own cti. */
static bool
drbbdup_ilist_has_cti(instrlist_t *bb)
{
    for (instr_t *inst = instrlist_first(bb); inst != NULL; inst = instr_get_next(inst)) {
        if (instr_is_cti(inst))
            return true;
    }
    return false;
}

static bool
drbbdup_ilist_has_unending_emulation(instrlist_t *bb)
{
    emulated_instr_t emul_info = { sizeof(emul_info), 0 };
    for (instr_t *inst = instrlist_first(bb); inst != NULL; inst = instr_get_next(inst)) {
        if (drmgr_is_emulation_start(inst) &&
            drmgr_get_emulated_instr_data(inst, &emul_info) &&
            TEST(DR_EMULATE_REST_OF_BLOCK, emul_info.flags))
            return true;
    }
    return false;
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

    bool has_rest_of_block_emulation = drbbdup_ilist_has_unending_emulation(bb);
    bool has_prior_control_flow = drbbdup_ilist_has_cti(bb);

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
     * are live at the start of bb copies, unless there is other control
     * flow from prior expansions such as drutil_expand_rep_string(), in which
     * case we have to disable drreg optimizations for this block for safety.
     */
    if (!has_prior_control_flow)
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
        instr_t *jmp_exit = XINST_CREATE_jump(drcontext, exit_label_opnd);
        instr_t *first = instrlist_first(bb);
        last = instrlist_last(bb);
        if (has_rest_of_block_emulation) {
            /* For DR_EMULATE_REST_OF_BLOCK defer to the original by not inserting
             * our own for a special instr.  Also, make sure the region ends at
             * the end of this copy and doesn't extend into subsequent copies.
             * We assume that drmgr ends at an end label even if the REST_OF_BLOCK
             * flag is set.
             */
            instrlist_preinsert(bb, first, jmp_exit);
            drmgr_insert_emulation_end(drcontext, bb, first);
        } else if (drbbdup_is_special_instr(last)) {
            /* Mark this jmp as emulating the final special instr to ensure that
             * drmgr_orig_app_instr_for_fetch() and drmgr_orig_app_instr_for_operands()
             * work properly (without this those two will look at this jmp since drmgr
             * does not have a "where" vs "instr" split).
             * XXX i#5390: Integrating drbbdup into drmgr is another way to solve this.
             */
            emulated_instr_t emulated_instr;
            emulated_instr.size = sizeof(emulated_instr);
            emulated_instr.pc = instr_get_app_pc(last);
            emulated_instr.instr = instr_clone(drcontext, last); /* Freed by label. */
            emulated_instr.flags = 0;
            drmgr_insert_emulation_start(drcontext, bb, first, &emulated_instr);
            instrlist_preinsert(bb, first, jmp_exit);
            drmgr_insert_emulation_end(drcontext, bb, first);
        } else {
            instrlist_preinsert(bb, first, jmp_exit);
        }

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
    if (drbbdup_is_special_instr(last)) {
        emulated_instr_t emulated_instr;
        emulated_instr.size = sizeof(emulated_instr);
        emulated_instr.pc = instr_get_app_pc(last);
        emulated_instr.instr = instr_clone(drcontext, last); /* Freed by label. */
        emulated_instr.flags = 0;
        drmgr_insert_emulation_start(drcontext, bb, last, &emulated_instr);
        instrlist_meta_preinsert(bb, last, exit_label);
        drmgr_insert_emulation_end(drcontext, bb, last);
    } else
        instrlist_meta_postinsert(bb, last, exit_label);
}

static bool
is_dup_expected(drbbdup_manager_t *manager, bool for_trace, bool translating)
{
    return for_trace || translating || (manager != NULL && manager->is_gen);
}

static dr_emit_flags_t
drbbdup_do_duplication(hashtable_t *manager_table, void *drcontext, void *tag,
                       instrlist_t *bb, bool for_trace, bool translating)
{
    drbbdup_manager_t *manager =
        (drbbdup_manager_t *)hashtable_lookup(manager_table, tag);

    if (!is_dup_expected(manager, for_trace, translating)) {
        /* Remove existing invalid book-keeping data. */
        hashtable_remove(manager_table, tag);
        manager = NULL;
    }

    /* A manager is created if there does not already exist one that "book-keeps"
     * this basic block.
     */
    if (manager == NULL) {
        manager = drbbdup_create_manager(drcontext, tag, bb);
        ASSERT(manager != NULL, "created manager cannot be NULL");
        hashtable_add(manager_table, tag, manager);

        if (opts.is_stat_enabled) {
            dr_mutex_lock(stat_mutex);
            if (!manager->enable_dup)
                stats.no_dup_count++;
            if (!manager->enable_dynamic_handling)
                stats.no_dynamic_handling_count++;
            dr_mutex_unlock(stat_mutex);
        }
        if (manager->enable_dynamic_handling) {
            dr_mutex_lock(case_cache_mutex);

            if (new_case_cache_pc == NULL)
                new_case_cache_pc = init_fp_cache(drbbdup_handle_new_case);

            dr_mutex_unlock(case_cache_mutex);
        }
    }

    if (manager->enable_dup) {
        /* Add the copies. */
        drbbdup_set_up_copies(drcontext, bb, manager);
    }

    /* If there's no dynamic handling, we do not need to store translations,
     * which saves memory (and is currently better supported in DR and drreg).
     */
    return manager->enable_dynamic_handling ? DR_EMIT_STORE_TRANSLATIONS
                                            : DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
drbbdup_duplicate_phase(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                        bool translating)
{
    dr_emit_flags_t emit_flags = DR_EMIT_DEFAULT;

    /* XXX i#5400: By integrating drbbdup into drmgr we should be able to simplify
     * some of these awkward conditions where we have to handle a missing manager in
     * order to not waste memory when duplication is disabled.
     */
    if (opts.non_default_case_limit == 0)
        return emit_flags;

    if (is_thread_private) {
        drbbdup_per_thread *pt =
            (drbbdup_per_thread *)drmgr_get_tls_field(drcontext, tls_idx);
        emit_flags = drbbdup_do_duplication(&pt->manager_table, drcontext, tag, bb,
                                            for_trace, translating);

    } else {
        dr_rwlock_write_lock(rw_lock);
        emit_flags = drbbdup_do_duplication(&global_manager_table, drcontext, tag, bb,
                                            for_trace, translating);
        dr_rwlock_write_unlock(rw_lock);
    }

    return emit_flags;
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

/* Returns true if at the end of a bb version is reached: if check_instr is
 * the inserted jump or is the exit label.  There may be an emulation end label
 * after check_instr.
 */
static bool
drbbdup_is_at_end(instr_t *check_instr)
{
    if (drbbdup_is_at_label(check_instr, DRBBDUP_LABEL_EXIT))
        return true;

    if (instr_is_cti(check_instr)) {
        instr_t *next_instr = instr_get_next(check_instr);
        return drbbdup_is_at_label(next_instr, DRBBDUP_LABEL_START) ||
            /* There may be an emulation endpoint label in between. */
            (next_instr != NULL && instr_get_next(next_instr) != NULL &&
             drmgr_is_emulation_end(next_instr) &&
             drbbdup_is_at_label(instr_get_next(next_instr), DRBBDUP_LABEL_START));
    }

    return false;
}

/* Returns true if at the start of the end of a bb version: if check_instr is
 * the start emulation label for the inserted jump or the exit label. This does not return
 * true for certain types of blocks e.g., blocks that do not end in a branch/syscall or
 * blocks that have unending emulation like repstr.
 */
static bool
drbbdup_is_at_end_initial(instr_t *check_instr)
{
    /* We need to stop at the emulation start label so that drmgr will point
     * there for drmgr_orig_app_instr_for_*().
     */
    if (!drmgr_is_emulation_start(check_instr))
        return false;
    instr_t *next_instr = instr_get_next(check_instr);
    if (next_instr == NULL)
        return false;

    return drbbdup_is_at_end(next_instr);
}

static bool
drbbdup_is_exit_jmp_emulation_marker(instr_t *check_instr)
{
    if (check_instr == NULL)
        return false;
    if (drmgr_is_emulation_start(check_instr))
        return drbbdup_is_at_end(instr_get_next(check_instr));
    if (drmgr_is_emulation_end(check_instr))
        return drbbdup_is_at_end(instr_get_prev(check_instr));
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

/* Iterates forward to the end of the next bb copy.  This may followed by an
 * emulation end label.  Returns NULL upon failure.
 */
static instr_t *
drbbdup_next_end(instr_t *instr)
{
    while (instr != NULL && !drbbdup_is_at_end(instr))
        instr = instr_get_next(instr);

    return instr;
}

/* Iterates forward to the end of the next bb copy.  If the end instruction has
 * emulation labels, steps back to point at the first of those: i.e., this is the
 * start of the end sequence.  Returns NULL upon failure.
 */
static instr_t *
drbbdup_next_end_initial(instr_t *instr)
{
    while (instr != NULL && !drbbdup_is_at_end(instr))
        instr = instr_get_next(instr);
    if (instr != NULL) {
        instr_t *prev = instr_get_prev(instr);
        if (drbbdup_is_exit_jmp_emulation_marker(prev)) {
            instr = prev;
            ASSERT(drmgr_is_emulation_start(instr), "should be start marker");
        }
    }

    return instr;
}

/* Extracts a single bb copy from the overall bb starting from start.
 * start is also set to the beginning of next bb copy for easy chaining.
 *  Overall, separate instr lists simplify user call-backs.
 *  The returned instr list needs to be destroyed using instrlist_clear_and_destroy().
 */
static instrlist_t *
drbbdup_extract_bb_copy(void *drcontext, instrlist_t *bb, instr_t *start,
                        OUT instr_t **prev, OUT instr_t **post)
{
    instrlist_t *case_bb = instrlist_create(drcontext);

    ASSERT(start != NULL, "start instruction cannot be NULL");
    ASSERT(prev != NULL, "prev instr storage cannot be NULL");
    ASSERT(post != NULL, "post instr storage cannot be NULL");
    ASSERT(instr_get_note(start) == (void *)DRBBDUP_LABEL_START,
           "start instruction should be a START label");

    /* Use end_initial to avoid placing emulation markers in the list at all
     * (no need since we have the real final instr, and the markers mess up
     * things like drmemtrace elision).
     */
    *post = drbbdup_next_end_initial(start);
    ASSERT(*post != NULL, "end instruction cannot be NULL");
    ASSERT(!drbbdup_is_at_start(*post), "end cannot be at start");

    /* Also include the last instruction in the bb if it is a
     * syscall/cti instr.
     */
    instr_t *last_instr = instrlist_last(bb);
    if (drbbdup_is_special_instr(last_instr)) {
        instr_t *instr_cpy = instr_clone(drcontext, last_instr);
        instrlist_preinsert(bb, *post, instr_cpy);
    }
    instrlist_cut(bb, *post);
    *prev = start;
    start = instr_get_next(start); /* Skip START label. */

    if (start != NULL) {
        instrlist_cut(bb, start);
        instrlist_append(case_bb, start);
    }

    return case_bb;
}

static void
drbbdup_stitch_bb_copy(void *drcontext, instrlist_t *bb, instrlist_t *case_bb,
                       instr_t *pre, instr_t *post)
{
    instr_t *last_instr = instrlist_last(case_bb);
    if (drbbdup_is_special_instr(last_instr)) {
        instrlist_remove(case_bb, last_instr);
        instr_destroy(drcontext, last_instr);
    }

    instrlist_append(case_bb, post);
    instr_t *instr = instrlist_first(case_bb);
    instrlist_postinsert(bb, pre, instr);

    instrlist_init(case_bb);
    instrlist_destroy(drcontext, case_bb);
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
        instr_t *pre = NULL;  /* used for stitching */
        instr_t *post = NULL; /* used for stitching */
        instrlist_t *case_bb = drbbdup_extract_bb_copy(drcontext, bb, start, &pre, &post);
        opts.analyze_orig(drcontext, tag, case_bb, opts.user_data, &orig_analysis_data);
        drbbdup_stitch_bb_copy(drcontext, bb, case_bb, pre, post);
    } else {
        /* For bb with no wanted copies, just invoke the call-back with original bb. */
        opts.analyze_orig(drcontext, tag, bb, opts.user_data, &orig_analysis_data);
    }

    return orig_analysis_data;
}

/* Performs analysis specific to a case. */
static void *
drbbdup_do_case_analysis(drbbdup_manager_t *manager, void *drcontext, void *tag,
                         instrlist_t *bb, instr_t *start, bool for_trace,
                         bool translating, const drbbdup_case_t *case_info,
                         void *orig_analysis_data, OUT instr_t **next,
                         INOUT dr_emit_flags_t *emit_flags)
{
    if (opts.analyze_case == NULL && opts.analyze_case_ex == NULL)
        return NULL;

    void *case_analysis_data = NULL;
    dr_emit_flags_t flags = DR_EMIT_DEFAULT;
    if (manager != NULL && manager->enable_dup) {
        instr_t *pre = NULL;  /* used for stitching */
        instr_t *post = NULL; /* used for stitching */
        instrlist_t *case_bb = drbbdup_extract_bb_copy(drcontext, bb, start, &pre, &post);
        /* Let the user analyse the BB for the given case. */
        if (opts.analyze_case_ex != NULL) {
            flags |= opts.analyze_case_ex(drcontext, tag, case_bb, for_trace, translating,
                                          case_info->encoding, opts.user_data,
                                          orig_analysis_data, &case_analysis_data);
        } else {
            opts.analyze_case(drcontext, tag, case_bb, case_info->encoding,
                              opts.user_data, orig_analysis_data, &case_analysis_data);
        }
        drbbdup_stitch_bb_copy(drcontext, bb, case_bb, pre, post);
        if (next != NULL)
            *next = drbbdup_next_start(post);
    } else {
        /* For bb with no wanted copies, simply invoke the call-back with the original
         * bb.
         */
        if (opts.analyze_case_ex != NULL) {
            flags |= opts.analyze_case_ex(drcontext, tag, bb, for_trace, translating,
                                          case_info->encoding, opts.user_data,
                                          orig_analysis_data, &case_analysis_data);
        } else {
            opts.analyze_case(drcontext, tag, bb, case_info->encoding, opts.user_data,
                              orig_analysis_data, &case_analysis_data);
        }
        if (next != NULL)
            *next = NULL;
    }
    if (emit_flags != NULL)
        *emit_flags |= flags;

    return case_analysis_data;
}

static dr_emit_flags_t
drbbdup_do_analysis(void *drcontext, drbbdup_per_thread *pt, hashtable_t *manager_table,
                    void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    drbbdup_case_t *case_info = NULL;
    instr_t *first = instrlist_first(bb);

    drbbdup_manager_t *manager =
        (drbbdup_manager_t *)hashtable_lookup(manager_table, tag);
    ASSERT(manager != NULL || opts.non_default_case_limit == 0,
           "manager cannot be NULL unless dups are globally disabled");

    /* Perform orig analysis - only done once regardless of how many copies. */
    pt->orig_analysis_data = drbbdup_do_orig_analysis(manager, drcontext, tag, bb, first);

    /* Perform analysis for each (non-default) case. */
    dr_emit_flags_t emit_flags = DR_EMIT_DEFAULT;
    if (manager != NULL && manager->enable_dup) {
        ASSERT(manager->cases != NULL, "case information must exit");
        int i;
        for (i = 0; i < opts.non_default_case_limit; i++) {
            case_info = &manager->cases[i];
            if (case_info->is_defined) {
                pt->case_analysis_data[i] = drbbdup_do_case_analysis(
                    manager, drcontext, tag, bb, first, for_trace, translating, case_info,
                    pt->orig_analysis_data, &first, &emit_flags);
            }
        }
    }

    /* Perform analysis for default case. Note, we do the analysis even if the manager
     * does not have dups enabled.
     */
    /* XXX i#5400: By integrating drbbdup into drmgr we should be able to simplify
     * some of these awkward conditions where we have to handle a missing manager in
     * order to not waste memory when duplication is disabled.
     */
    drbbdup_case_t empty = { 0, true };
    if (manager == NULL)
        case_info = &empty;
    else
        case_info = &manager->default_case;
    ASSERT(case_info->is_defined, "default case must be defined");
    pt->default_analysis_data = drbbdup_do_case_analysis(
        manager, drcontext, tag, bb, first, for_trace, translating, case_info,
        pt->orig_analysis_data, NULL, &emit_flags);

    return emit_flags;
}

static dr_emit_flags_t
drbbdup_analyse_phase(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                      bool translating, void *user_data)
{
    dr_emit_flags_t emit_flags;

    /* Store analysis data in thread storage. */
    drbbdup_per_thread *pt =
        (drbbdup_per_thread *)drmgr_get_tls_field(drcontext, tls_idx);

    if (is_thread_private) {
        emit_flags = drbbdup_do_analysis(drcontext, pt, &pt->manager_table, tag, bb,
                                         for_trace, translating);
    } else {
        dr_rwlock_read_lock(rw_lock);
        emit_flags = drbbdup_do_analysis(drcontext, pt, &global_manager_table, tag, bb,
                                         for_trace, translating);
        dr_rwlock_read_unlock(rw_lock);
    }

    return emit_flags;
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
#if !defined(RISCV64)
    if (!manager->are_flags_dead) {
        drbbdup_restore_register(drcontext, bb, where, DRBBDUP_FLAG_REG_SLOT,
                                 manager->scratch_reg);
        dr_restore_arith_flags_from_reg(drcontext, bb, where, manager->scratch_reg);
    }
#endif
    if (!manager->is_scratch_reg_dead) {
        drbbdup_restore_register(drcontext, bb, where, DRBBDUP_SCRATCH_REG_SLOT,
                                 manager->scratch_reg);
    }
#ifdef AARCHXX
    if (manager->is_scratch_reg2_needed && !manager->is_scratch_reg2_dead) {
        drbbdup_restore_register(drcontext, bb, where, DRBBDUP_SCRATCH_REG2_SLOT,
                                 DRBBDUP_SCRATCH_REG2);
    }
#endif
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
#if !defined(RISCV64)
    if (drbbdup_case_zero_vs_nonzero(manager)) {
        manager->are_flags_dead = true; /* Not used, so don't restore. */
        manager->scratch_reg = DRBBDUP_SCRATCH_REG_NO_FLAGS;
    } else {
        drreg_are_aflags_dead(drcontext, where, &manager->are_flags_dead);
        manager->scratch_reg = DRBBDUP_SCRATCH_REG;
    }
    drreg_is_register_dead(drcontext, manager->scratch_reg, where,
                           &manager->is_scratch_reg_dead);
#else
    /* Since RISC-V does not have a flags register, always use the standard reg. */
    manager->scratch_reg = DRBBDUP_SCRATCH_REG;
#endif
    if (!manager->is_scratch_reg_dead) {
        drbbdup_spill_register(drcontext, bb, where, DRBBDUP_SCRATCH_REG_SLOT,
                               manager->scratch_reg);
    }
#ifdef AARCHXX
    if (opts.max_case_encoding > 0 && opts.max_case_encoding <= MAX_IMMED_IN_CMP)
        manager->is_scratch_reg2_needed = false;
    else {
        manager->is_scratch_reg2_needed = true;
        drreg_is_register_dead(drcontext, DRBBDUP_SCRATCH_REG2, where,
                               &manager->is_scratch_reg2_dead);
        if (!manager->is_scratch_reg2_dead) {
            drbbdup_spill_register(drcontext, bb, where, DRBBDUP_SCRATCH_REG2_SLOT,
                                   DRBBDUP_SCRATCH_REG2);
        }
    }
#endif
#if !defined(RISCV64)
    if (!manager->are_flags_dead) {
        dr_save_arith_flags_to_reg(drcontext, bb, where, manager->scratch_reg);
        drbbdup_spill_register(drcontext, bb, where, DRBBDUP_FLAG_REG_SLOT,
                               manager->scratch_reg);
        /* If we're invoking a clean call, restore the scratch reg.  If we're not,
         * we assume runtime_case_opnd will not refer to the scratch reg (it has
         * to be absolute/pc-rel if it does not use a clean call).
         */
        if (!manager->is_scratch_reg_dead && opts.insert_encode != NULL) {
            /* This extra restore that keeps scratch_reg spilled requires special
             * handling in drbbdup_event_restore_state().
             */
            drbbdup_restore_register(drcontext, bb, where, DRBBDUP_SCRATCH_REG_SLOT,
                                     manager->scratch_reg);
        }
    }
#endif

    /* Encoding is application-specific and therefore we need the user to define the
     * encoding of the runtime case. Therefore, we invoke a user-defined call-back.
     *
     * It could also be that the encoding is done directly and changed on demand.
     * Therefore, the call-back may be NULL.
     */
    if (opts.insert_encode != NULL) {

        /* Note, we could tell the user not to reserve flags and scratch register since
         * drbbdup is doing that already. However, for flexibility/backwards compatibility
         * ease, this might not be the best approach.
         */
        opts.insert_encode(drcontext, tag, bb, where, opts.user_data,
                           pt->orig_analysis_data);

        /* Restore all unreserved registers used by the call-back. */
        drreg_restore_all(drcontext, bb, where);
    }

    /* Load the encoding to the scratch register. */
    opnd_t case_opnd = opts.runtime_case_opnd;
    opnd_t scratch_reg_opnd = opnd_create_reg(manager->scratch_reg);
#ifdef AARCHXX
    if (opnd_is_rel_addr(case_opnd)) {
        /* Work around two problems:
         * 1) DR's AArch64 decoder doesn't yet support OP_ldr with pc-rel opnd
         *    (i#4847, i#5316).
         * 2) To ensure we can reach we may need to load the address into the
         *    register in a separate step.  DR may mangle this for us (i#1834)
         *    so we may be able to remove this in the future.
         */
        instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)opnd_get_addr(case_opnd),
                                         scratch_reg_opnd, bb, where, NULL, NULL);
        case_opnd = OPND_CREATE_MEMPTR(manager->scratch_reg, 0);
    }
    if (opts.atomic_load_encoding) {
#    ifdef AARCH64
        instrlist_meta_preinsert(
            bb, where, INSTR_CREATE_ldar(drcontext, scratch_reg_opnd, case_opnd));
#    else
        instrlist_meta_preinsert(
            bb, where, XINST_CREATE_load(drcontext, scratch_reg_opnd, case_opnd));
        instrlist_meta_preinsert(
            bb, where, INSTR_CREATE_dmb(drcontext, OPND_CREATE_INT(DR_DMB_ISH)));
#    endif
    } else {
        instrlist_meta_preinsert(
            bb, where, XINST_CREATE_load(drcontext, scratch_reg_opnd, case_opnd));
    }
#else
    /* For x86, a regular load has acquire semantics. */
    instrlist_meta_preinsert(bb, where,
                             XINST_CREATE_load(drcontext, scratch_reg_opnd, case_opnd));
#endif
}

/* If avoid_flags and current_case->encoding == 0, uses a compare that does not
 * affect the flags.
 */
static void
drbbdup_insert_compare_encoding_and_branch(void *drcontext, instrlist_t *bb,
                                           instr_t *where, drbbdup_manager_t *manager,
                                           drbbdup_case_t *current_case, bool avoid_flags,
                                           reg_id_t reg_encoding, bool jmp_if_equal,
                                           instr_t *jmp_label)
{
#ifdef X86
    if (avoid_flags && current_case->encoding == 0) {
        /* JECXZ is a slow instruction on modern processors.  But, it avoids spilling and
         * restoring the flags.  In some quick SPEC2006 tests, a trivial client with no
         * instrumentation was slower with JECXZ than with flag preservation on bzip2
         * test, but faster on mcf test (more memory pressure).  With more
         * instrumentation the mcf result might hold on typical clients and apps; more
         * experimentation is needed.  For now we keep JECXZ as the default; we can make
         * it under an option or remove it if we find evidence that mcf w/ a trivial
         * client is the outlier.
         */
        ASSERT(reg_encoding == DR_REG_XCX, "scratch must be xcx");
        /* To avoid any problems with reach we use landing pads. */
        if (jmp_if_equal) {
            /* We could use "LEA xcx+1; LOOP" for a "JECXNZ" but I measured it and
             * it is quite slow: LOOP is much worse than JECXZ apparently.
             * We document that is is better to have the default case be the non-zero
             * one, landing in the else below with fewer jumps.
             */
            instr_t *dojmp = INSTR_CREATE_label(drcontext);
            instr_t *nojmp = INSTR_CREATE_label(drcontext);
            instrlist_meta_preinsert(
                bb, where, INSTR_CREATE_jecxz(drcontext, opnd_create_instr(dojmp)));
            instrlist_meta_preinsert(
                bb, where, XINST_CREATE_jump(drcontext, opnd_create_instr(nojmp)));
            instrlist_meta_preinsert(bb, where, dojmp);
            instrlist_meta_preinsert(
                bb, where, XINST_CREATE_jump(drcontext, opnd_create_instr(jmp_label)));
            instrlist_meta_preinsert(bb, where, nojmp);
        } else {
            instr_t *nojmp = INSTR_CREATE_label(drcontext);
            instrlist_meta_preinsert(
                bb, where, INSTR_CREATE_jecxz(drcontext, opnd_create_instr(nojmp)));
            instrlist_meta_preinsert(
                bb, where, XINST_CREATE_jump(drcontext, opnd_create_instr(jmp_label)));
            instrlist_meta_preinsert(bb, where, nojmp);
        }
        return;
    }
#    ifdef X86_64
    if (current_case->encoding <= INT_MAX) {
        /* It fits in an immediate so we can avoid the load. */
        opnd_t opnd = opnd_create_immed_uint(current_case->encoding, OPSZ_4);
        instrlist_meta_preinsert(
            bb, where, XINST_CREATE_cmp(drcontext, opnd_create_reg(reg_encoding), opnd));
    } else {
        opnd_t opnd = opnd_create_abs_addr(&current_case->encoding, OPSZ_PTR);
        instrlist_meta_preinsert(
            bb, where, XINST_CREATE_cmp(drcontext, opnd, opnd_create_reg(reg_encoding)));
    }
#    elif defined(X86_32)
    opnd_t opnd = opnd_create_immed_uint(current_case->encoding, OPSZ_PTR);
    instrlist_meta_preinsert(
        bb, where, XINST_CREATE_cmp(drcontext, opnd_create_reg(reg_encoding), opnd));
#    endif
    instrlist_meta_preinsert(bb, where,
                             INSTR_CREATE_jcc(drcontext, jmp_if_equal ? OP_jz : OP_jnz,
                                              opnd_create_instr(jmp_label)));
#elif defined(AARCHXX)
    if (avoid_flags && current_case->encoding == 0) {
#    ifdef AARCH64
        if (jmp_if_equal) {
            instrlist_meta_preinsert(bb, where,
                                     INSTR_CREATE_cbz(drcontext,
                                                      opnd_create_instr(jmp_label),
                                                      opnd_create_reg(reg_encoding)));
        } else {
            instrlist_meta_preinsert(bb, where,
                                     INSTR_CREATE_cbnz(drcontext,
                                                       opnd_create_instr(jmp_label),
                                                       opnd_create_reg(reg_encoding)));
        }
        return;
#    else
        if (dr_get_isa_mode(drcontext) == DR_ISA_ARM_THUMB &&
            reg_encoding <= DR_REG_R7) { /* CBZ can't take r8+ */
            /* CBZ has a very short reach so we use a landing pad. */
            instr_t *nojmp = INSTR_CREATE_label(drcontext);
            if (jmp_if_equal) {
                instrlist_meta_preinsert(
                    bb, where,
                    INSTR_CREATE_cbnz(drcontext, opnd_create_instr(nojmp),
                                      opnd_create_reg(reg_encoding)));
            } else {
                instrlist_meta_preinsert(bb, where,
                                         INSTR_CREATE_cbz(drcontext,
                                                          opnd_create_instr(nojmp),
                                                          opnd_create_reg(reg_encoding)));
            }
            instrlist_meta_preinsert(
                bb, where, XINST_CREATE_jump(drcontext, opnd_create_instr(jmp_label)));
            instrlist_meta_preinsert(bb, where, nojmp);
            return;
        }
#    endif
    }
    if (current_case->encoding <= MAX_IMMED_IN_CMP) {
        /* Various larger immediates can be handled but it varies by ISA and mode.
         * XXX: Should DR provide utilities to help figure out whether an integer
         * will fit in a compare immediate?
         */
        opnd_t opnd = opnd_create_immed_uint(current_case->encoding, OPSZ_PTR);
        instrlist_meta_preinsert(
            bb, where, XINST_CREATE_cmp(drcontext, opnd_create_reg(reg_encoding), opnd));
        instrlist_meta_preinsert(
            bb, where,
            XINST_CREATE_jump_cond(drcontext, jmp_if_equal ? DR_PRED_EQ : DR_PRED_NE,
                                   opnd_create_instr(jmp_label)));
        return;
    }
    DR_ASSERT_MSG(manager->is_scratch_reg2_needed, "scratch2 was not saved");
    instrlist_insert_mov_immed_ptrsz(drcontext, current_case->encoding,
                                     opnd_create_reg(DRBBDUP_SCRATCH_REG2), bb, where,
                                     NULL, NULL);
    instrlist_meta_preinsert(bb, where,
                             XINST_CREATE_cmp(drcontext, opnd_create_reg(reg_encoding),
                                              opnd_create_reg(DRBBDUP_SCRATCH_REG2)));
    instrlist_meta_preinsert(
        bb, where,
        XINST_CREATE_jump_cond(drcontext, jmp_if_equal ? DR_PRED_EQ : DR_PRED_NE,
                               opnd_create_instr(jmp_label)));
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
    ASSERT(next_label != NULL, "the label to the next bb copy cannot be NULL");

    /* If runtime encoding not equal to encoding of current case, just jump to next.
     */
    bool jmp_if_equal = false, avoid_flags = false;
    if (drbbdup_case_zero_vs_nonzero(manager)) {
        /* Use an aflags-less jump-if-zero. */
        avoid_flags = true;
        if (current_case->encoding != 0) {
            /* Invert the compare to ensure comparison with zero. */
            ASSERT(manager->default_case.encoding == 0, "not zero-vs-nonzero");
            current_case = &manager->default_case;
            jmp_if_equal = true;
        }
    }
    drbbdup_insert_compare_encoding_and_branch(
        drcontext, bb, where, manager, current_case, avoid_flags, manager->scratch_reg,
        jmp_if_equal, next_label);

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
    for (i = 0; i < opts.non_default_case_limit; i++) {
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
drbbdup_insert_dynamic_handling(void *drcontext, void *tag, instrlist_t *bb,
                                instr_t *where, drbbdup_manager_t *manager)
{

    instr_t *instr;
    opnd_t drbbdup_opnd = opnd_create_reg(manager->scratch_reg);

    instr_t *done_label = INSTR_CREATE_label(drcontext);

    ASSERT(new_case_cache_pc != NULL,
           "new case cache for dynamic handling must be already initialised.");
    DR_ASSERT_MSG(!opts.never_enable_dynamic_handling,
                  "should not reach here if dynamic cases were disabled globally");

    /* Check whether case limit has not been reached. */
    if (drbbdup_do_dynamic_handling(manager)) {
        drbbdup_case_t *default_case = &manager->default_case;
        ASSERT(default_case->is_defined, "default case must be defined");

        /* Jump if runtime encoding matches default encoding.
         * Unknown encoding encountered upon fall-through.
         */
        drbbdup_insert_compare_encoding_and_branch(
            drcontext, bb, where, manager, default_case, false /*avoid_flags*/,
            manager->scratch_reg, true /*=jmp_if_equal*/, done_label);

        /* We need manager->scratch_reg. Bail on keeping the encoding in the register. */
        opnd_t encoding_opnd =
            drbbdup_get_tls_raw_slot_opnd(drcontext, DRBBDUP_ENCODING_SLOT);
        instr = XINST_CREATE_store(drcontext, encoding_opnd, drbbdup_opnd);
        instrlist_meta_preinsert(bb, where, instr);

        /* Don't bother insertion if threshold limit is zero. */
        if (opts.hit_threshold > 0) {
            /* Update hit count and check whether threshold is reached. */
            opnd_t hit_table_opnd =
                drbbdup_get_tls_raw_slot_opnd(drcontext, DRBBDUP_HIT_TABLE_SLOT);

            /* Load the hit counter table. */
            instr = XINST_CREATE_load(drcontext, drbbdup_opnd, hit_table_opnd);
            instrlist_meta_preinsert(bb, where, instr);

            /* Register hit. */
            uint hash = drbbdup_get_hitcount_hash((intptr_t)tag);
            opnd_t hit_count_opnd =
                OPND_CREATE_MEM16(manager->scratch_reg, hash * sizeof(ushort));
#ifdef X86
            instr = INSTR_CREATE_sub(drcontext, hit_count_opnd,
                                     opnd_create_immed_uint(1, OPSZ_2));
            instrlist_meta_preinsert(bb, where, instr);
#else
            instr = XINST_CREATE_load_2bytes(drcontext, drbbdup_opnd, hit_count_opnd);
            instrlist_meta_preinsert(bb, where, instr);
            instr = XINST_CREATE_sub(drcontext, drbbdup_opnd,
                                     opnd_create_immed_uint(1, OPSZ_2));
            instrlist_meta_preinsert(bb, where, instr);
            instr = XINST_CREATE_store_2bytes(drcontext, hit_count_opnd, drbbdup_opnd);
            instrlist_meta_preinsert(bb, where, instr);
#endif

            /* Load bb tag to register so that it can be accessed by outlined clean
             * call.
             */
            instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)tag, drbbdup_opnd, bb,
                                             where, NULL, NULL);

            /* Jump if hit reaches zero. */
            instr = XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ,
                                           opnd_create_pc(new_case_cache_pc));
            instrlist_meta_preinsert(bb, where, instr);

        } else {
            /* Load bb tag to register so that it can be accessed by outlined clean
             * call.
             */
            instrlist_insert_mov_immed_ptrsz(drcontext, (intptr_t)tag, drbbdup_opnd, bb,
                                             instr, NULL, NULL);

            /* Jump to outlined clean call code for new case registration. */
            instr = XINST_CREATE_jump(drcontext, opnd_create_pc(new_case_cache_pc));
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
drbbdup_insert_dispatch_end(void *drcontext, void *tag, instrlist_t *bb, instr_t *where,
                            drbbdup_manager_t *manager)
{
    /* Check whether dynamic case handling is enabled by the user to handle an unkown
     * case encoding.
     */
    if (manager->enable_dynamic_handling) {
        drbbdup_insert_dynamic_handling(drcontext, tag, bb, where, manager);
    }

    /* Last bb version is always the default case. */
    drbbdup_insert_landing_restoration(drcontext, bb, where, manager);
}

static dr_emit_flags_t
drbbdup_instrument_instr(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                         instr_t *where, bool for_trace, bool translating,
                         drbbdup_per_thread *pt, drbbdup_manager_t *manager)
{
    drbbdup_case_t *drbbdup_case = NULL;
    void *analysis_data = NULL;

    ASSERT(opts.instrument_instr != NULL || opts.instrument_instr_ex != NULL,
           "one of the instrument call-back functions must be non-NULL");
    ASSERT(pt->case_index != DRBBDUP_IGNORE_INDEX, "case index cannot be ignored");

    drbbdup_case_t empty = { 0, true };
    if (pt->case_index == DRBBDUP_DEFAULT_INDEX) {
        /* Use default case. */
        if (manager == NULL)
            drbbdup_case = &empty;
        else
            drbbdup_case = &manager->default_case;
        analysis_data = pt->default_analysis_data;
    } else {
        ASSERT(pt->case_analysis_data != NULL,
               "container for analysis data cannot be NULL");
        ASSERT(pt->case_index >= 0 && pt->case_index < opts.non_default_case_limit,
               "case index cannot be out-of-bounds");
        ASSERT(manager != NULL && manager->enable_dup, "bb dup must be enabled");

        drbbdup_case = &manager->cases[pt->case_index];
        analysis_data = pt->case_analysis_data[pt->case_index];
    }

    ASSERT(drbbdup_case->is_defined, "case must be defined upon instrumentation");
    if (opts.instrument_instr_ex != NULL) {
        return opts.instrument_instr_ex(drcontext, tag, bb, instr, where, for_trace,
                                        translating, drbbdup_case->encoding,
                                        opts.user_data, pt->orig_analysis_data,
                                        analysis_data);
    } else {
        opts.instrument_instr(drcontext, tag, bb, instr, where, drbbdup_case->encoding,
                              opts.user_data, pt->orig_analysis_data, analysis_data);
        return DR_EMIT_DEFAULT;
    }
}

/* Support different instrumentation for different bb copies. Tracks which case is
 * currently being considered via an index (namely pt->case_index) in thread-local
 * storage, and update this index upon encountering the start/end of bb copies.
 */
static dr_emit_flags_t
drbbdup_instrument_dups(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                        bool for_trace, bool translating, drbbdup_per_thread *pt,
                        drbbdup_manager_t *manager)
{
    drbbdup_case_t *drbbdup_case = NULL;
    dr_emit_flags_t flags = DR_EMIT_DEFAULT;
    ASSERT(manager->cases != NULL, "case info should not be NULL");
    ASSERT(pt != NULL, "thread-local storage should not be NULL");

    instr_t *last = instrlist_last_app(bb);
    /* We invoke drbbdup_is_at_end() to ensure we do not consider drbbdup inserted jumps.
     */
    bool is_last_special = drbbdup_is_special_instr(last) && !drbbdup_is_at_end(last);

    /* Insert runtime case encoding at start. */
    if (drmgr_is_first_instr(drcontext, instr)) {
        ASSERT(pt->case_index == -1, "case index should start at -1");
        drbbdup_encode_runtime_case(drcontext, pt, tag, bb, instr, manager);
    }

    if (drbbdup_is_at_start(instr)) {
        instr_t *next_instr = instr_get_next(instr); /* Skip START label. */
        instr_t *end_instr = drbbdup_next_end(next_instr);
        ASSERT(end_instr != NULL, "end instruction cannot be NULL");
        instr_t *end_initial = drbbdup_next_end_initial(next_instr);
        ASSERT(end_initial != NULL, "end instruction cannot be NULL");
        pt->inserted_restore_all = false;

        /* Cache first, first nonlabel and last instructions. */
        if (next_instr == end_initial) {
            if (is_last_special) {
                pt->first_instr = last;
                pt->first_nonlabel_instr = last;
            } else {
                pt->first_instr = NULL;
                pt->first_nonlabel_instr = NULL;
            }
        } else {
            /* Update cache to first instr. */
            pt->first_instr = next_instr;
            instr_t *first_non_label = next_instr;
            while (instr_is_label(first_non_label) && first_non_label != end_instr) {
                first_non_label = instr_get_next(first_non_label);
            }
            if (first_non_label == end_instr) {
                if (is_last_special) {
                    pt->first_nonlabel_instr = last;
                } else {
                    pt->first_nonlabel_instr = NULL;
                }
            } else {
                pt->first_nonlabel_instr = first_non_label;
            }
        }

        /* Update cache to last instr. */
        if (is_last_special) {
            pt->last_instr = last;
        } else {
            instr_t *prev = instr_get_prev(end_initial);
            if (drbbdup_is_at_start(prev)) {
                pt->last_instr = NULL;
            } else {
                pt->last_instr = prev;
            }
        }

        /* Check whether we reached the last bb version (namely the default case). */
        instr_t *next_bb_label = drbbdup_next_start(end_instr);
        if (next_bb_label == NULL) {
            pt->case_index = DRBBDUP_DEFAULT_INDEX; /* Refer to default. */
            drbbdup_insert_dispatch_end(drcontext, tag, bb, next_instr, manager);
        } else {
            /* We have reached the start of a new bb version (not the last one). */
            IF_DEBUG(bool found = false;)
            int i;
            for (i = pt->case_index + 1; i < opts.non_default_case_limit; i++) {
                drbbdup_case = &manager->cases[i];
                if (drbbdup_case->is_defined) {
                    IF_DEBUG(found = true;)
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
    } else if (drbbdup_is_at_end_initial(instr)) {
        /* Handle last special instruction (if present).
         * We use drbbdup_is_at_end_initial() to ensure drmgr will point to the
         * emulation data we setup for the exit label.
         */
        if (is_last_special) {
            flags = drbbdup_instrument_instr(drcontext, tag, bb, last, instr, for_trace,
                                             translating, pt, manager);
            if (pt->case_index == DRBBDUP_DEFAULT_INDEX) {
                pt->case_index =
                    DRBBDUP_IGNORE_INDEX; /* Ignore remaining instructions. */
            }
        }
        drreg_restore_all(drcontext, bb, instr);
        pt->inserted_restore_all = true;
    } else if (drbbdup_is_at_end(instr)) {
        /* i#5906: if the emulation start label is missing we might still need to restore
         * registers for blocks that don't end in a branch or for rep-expanded blocks. */
        if (!pt->inserted_restore_all)
            drreg_restore_all(drcontext, bb, instr);
    } else if (drbbdup_is_exit_jmp_emulation_marker(instr)) {
        /* Ignore instruction: hide drbbdup's own markers and the rest of the end.
         * Do not call drreg_restore_all either. */
    } else if (pt->case_index == DRBBDUP_IGNORE_INDEX) {
        /* Ignore instruction. */
        ASSERT(drbbdup_is_special_instr(instr), "ignored instr should be cti or syscall");
    } else {
        /* Instrument instructions inside the bb specified by pt->case_index. */
        flags = drbbdup_instrument_instr(drcontext, tag, bb, instr, instr, for_trace,
                                         translating, pt, manager);
    }
    return flags;
}

static dr_emit_flags_t
drbbdup_instrument_without_dups(void *drcontext, void *tag, instrlist_t *bb,
                                instr_t *instr, bool for_trace, bool translating,
                                drbbdup_per_thread *pt, drbbdup_manager_t *manager)
{
    ASSERT(manager == NULL || manager->cases == NULL, "case info should not be needed");
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
    return drbbdup_instrument_instr(drcontext, tag, bb, instr, instr, for_trace,
                                    translating, pt, manager);
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
            for (i = 0; i < opts.non_default_case_limit; i++) {
                if (pt->case_analysis_data[i] != NULL) {
                    opts.destroy_case_analysis(drcontext, manager->cases[i].encoding,
                                               opts.user_data, pt->orig_analysis_data,
                                               pt->case_analysis_data[i]);
                    pt->case_analysis_data[i] = NULL;
                }
            }
        }
        if (pt->default_analysis_data != NULL) {
            opts.destroy_case_analysis(
                drcontext, manager == NULL ? 0 : manager->default_case.encoding,
                opts.user_data, pt->orig_analysis_data, pt->default_analysis_data);
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
drbbdup_do_linking(void *drcontext, drbbdup_per_thread *pt, hashtable_t *manager_table,
                   void *tag, instrlist_t *bb, instr_t *instr, bool for_trace,
                   bool translating)
{

    drbbdup_manager_t *manager =
        (drbbdup_manager_t *)hashtable_lookup(manager_table, tag);
    ASSERT(manager != NULL || opts.non_default_case_limit == 0,
           "manager cannot be NULL unless dups are globally disabled");

    dr_emit_flags_t flags = DR_EMIT_DEFAULT;
    if (manager != NULL && manager->enable_dup) {
        flags |= drbbdup_instrument_dups(drcontext, tag, bb, instr, for_trace,
                                         translating, pt, manager);
    } else {
        flags |= drbbdup_instrument_without_dups(drcontext, tag, bb, instr, for_trace,
                                                 translating, pt, manager);
    }

    if (drmgr_is_last_instr(drcontext, instr))
        drbbdup_destroy_all_analyses(drcontext, manager, pt);

    return flags;
}

static dr_emit_flags_t
drbbdup_link_phase(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                   bool for_trace, bool translating, void *user_data)
{
    dr_emit_flags_t emit_flags;

    drbbdup_per_thread *pt =
        (drbbdup_per_thread *)drmgr_get_tls_field(drcontext, tls_idx);

    ASSERT(opts.instrument_instr != NULL || opts.instrument_instr_ex != NULL,
           "instrumentation call-back must not be NULL");

    /* Start off with the default case index. */
    if (drmgr_is_first_instr(drcontext, instr)) {
        pt->case_index = DRBBDUP_DEFAULT_INDEX;
        pt->inserted_restore_all = false;
    }

    if (is_thread_private) {
        emit_flags = drbbdup_do_linking(drcontext, pt, &pt->manager_table, tag, bb, instr,
                                        for_trace, translating);
    } else {
        dr_rwlock_read_lock(rw_lock);
        emit_flags = drbbdup_do_linking(drcontext, pt, &global_manager_table, tag, bb,
                                        instr, for_trace, translating);
        dr_rwlock_read_unlock(rw_lock);
    }

    return emit_flags;
}

static bool
drbbdup_encoding_already_included(drbbdup_manager_t *manager, uintptr_t encoding_check,
                                  bool check_default)
{
    drbbdup_case_t *drbbdup_case;
    if (manager->enable_dup) {
        int i;
        for (i = 0; i < opts.non_default_case_limit; i++) {
            drbbdup_case = &manager->cases[i];
            if (drbbdup_case->is_defined && drbbdup_case->encoding == encoding_check)
                return true;
        }
    }

    /* Check default case. */
    if (check_default) {
        drbbdup_case = &manager->default_case;
        if (drbbdup_case->is_defined && drbbdup_case->encoding == encoding_check)
            return true;
    }

    return false;
}

static bool
drbbdup_include_encoding(drbbdup_manager_t *manager, uintptr_t new_encoding)
{
    if (manager->enable_dup) {
        int i;
        drbbdup_case_t *dup_case;
        for (i = 0; i < opts.non_default_case_limit; i++) {
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
drbbdup_prepare_redirect(void *drcontext, dr_mcontext_t *mcontext,
                         drbbdup_manager_t *manager, app_pc bb_pc)
{
    /* Restore flags and scratch reg to their original app values. */
#if !defined(RISCV64)
    if (!manager->are_flags_dead) {
        reg_t val = (reg_t)drbbdup_get_tls_raw_slot_val(drcontext, DRBBDUP_FLAG_REG_SLOT);
        mcontext->xflags = dr_merge_arith_flags(mcontext->xflags, val);
    }
#endif
    if (!manager->is_scratch_reg_dead) {
        reg_set_value(
            manager->scratch_reg, mcontext,
            (reg_t)drbbdup_get_tls_raw_slot_val(drcontext, DRBBDUP_SCRATCH_REG_SLOT));
    }
#ifdef AARCHXX
    if (manager->is_scratch_reg2_needed && !manager->is_scratch_reg2_dead) {
        reg_set_value(
            DRBBDUP_SCRATCH_REG2, mcontext,
            (reg_t)drbbdup_get_tls_raw_slot_val(drcontext, DRBBDUP_SCRATCH_REG2_SLOT));
    }
#endif

    mcontext->pc =
        dr_app_pc_as_jump_target(dr_get_isa_mode(dr_get_current_drcontext()),
                                 bb_pc); /* redirect execution to the start of the bb. */
}

/* Returns whether to flush. */
static bool
drbbdup_manage_new_case(void *drcontext, hashtable_t *manager_table,
                        uintptr_t new_encoding, void *tag, instrlist_t *ilist,
                        dr_mcontext_t *mcontext, app_pc pc)
{

    bool do_flush = false;

    drbbdup_manager_t *manager =
        (drbbdup_manager_t *)hashtable_lookup(manager_table, tag);
    ASSERT(manager != NULL, "manager cannot be NULL");
    ASSERT(manager->enable_dup, "duplication should be enabled");
    ASSERT(new_encoding != manager->default_case.encoding,
           "unhandled encoding cannot be the default case");
    ASSERT(manager->scratch_reg == DRBBDUP_SCRATCH_REG, "must have main scratch reg");

    /* Could have been turned off potentially by another thread. */
    if (manager->enable_dynamic_handling) {
        /* Case already registered potentially by another thread. */
        if (!drbbdup_encoding_already_included(manager, new_encoding,
                                               true /* check default case */)) {
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
            /* Mark that flushing is happening for drbbdup. */
            if (do_flush)
                manager->is_gen = true;

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
    drbbdup_prepare_redirect(drcontext, mcontext, manager, pc);

    return do_flush;
}

static void
drbbdup_handle_new_case()
{
    void *drcontext = dr_get_current_drcontext();

    drbbdup_per_thread *pt =
        (drbbdup_per_thread *)drmgr_get_tls_field(drcontext, tls_idx);

    DR_ASSERT_MSG(!opts.never_enable_dynamic_handling,
                  "should not reach here if dynamic cases were disabled globally");

    /* Must use DR_MC_ALL due to dr_redirect_execution. */
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    /* Scratch register holds the tag. */
    void *tag = (void *)reg_get_value(DRBBDUP_SCRATCH_REG, &mcontext);

    instrlist_t *ilist = decode_as_bb(drcontext, dr_fragment_app_pc(tag));
    app_pc pc = instr_get_app_pc(drbbdup_first_app(ilist));
    ASSERT(pc != NULL, "pc cannot be NULL");

    /* Get the missing case. */
    uintptr_t new_encoding =
        drbbdup_get_tls_raw_slot_val(drcontext, DRBBDUP_ENCODING_SLOT);

    bool do_flush = false;

    if (is_thread_private) {
        do_flush = drbbdup_manage_new_case(drcontext, &pt->manager_table, new_encoding,
                                           tag, ilist, &mcontext, pc);
    } else {
        dr_rwlock_write_lock(rw_lock);
        do_flush = drbbdup_manage_new_case(drcontext, &global_manager_table, new_encoding,
                                           tag, ilist, &mcontext, pc);
        dr_rwlock_write_unlock(rw_lock);
    }

    instrlist_clear_and_destroy(drcontext, ilist);

    /* Refresh hit counter. */
    if (opts.hit_threshold > 0) {
        uint hash = drbbdup_get_hitcount_hash((intptr_t)tag);
        DR_ASSERT(pt->hit_counts[hash] == 0);
        pt->hit_counts[hash] = opts.hit_threshold; /* Reset threshold. */
    }

    /* Delete bb fragment. */
    if (do_flush) {
        LOG(drcontext, DR_LOG_ALL, 2,
            "%s Found new case! Going to flush bb with"
            "tag %p to generate a copy to handle the new case.\n",
            __FUNCTION__, tag);

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
    /* Assumes caller manages synchronisation. */
    app_pc cache_pc;
    instrlist_t *ilist;
    void *drcontext = dr_get_current_drcontext();
    size_t size = dr_page_size();
    ilist = instrlist_create(drcontext);

    DR_ASSERT_MSG(!opts.never_enable_dynamic_handling,
                  "should not reach here if dynamic cases were disabled globally");

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
 * INTERFACE
 */

drbbdup_status_t
drbbdup_register_case_encoding(void *drbbdup_ctx, uintptr_t encoding)
{
    if (drbbdup_init_count == 0)
        return DRBBDUP_ERROR_NOT_INITIALIZED;

    drbbdup_manager_t *manager = (drbbdup_manager_t *)drbbdup_ctx;

    if (opts.max_case_encoding > 0 && encoding > opts.max_case_encoding)
        return DRBBDUP_ERROR_INVALID_PARAMETER;

    /* Don't check default case because it is not yet set. */
    if (drbbdup_encoding_already_included(manager, encoding, false))
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
drbbdup_thread_init(void *drcontext)
{
    /* We use unreachable heap here too, though with the hit_counts array
     * dynamically allocated the usage is now small enough to not matter for
     * most non_default_case_limit values.
     */
    drbbdup_per_thread *pt = (drbbdup_per_thread *)dr_custom_alloc(
        drcontext, DR_ALLOC_THREAD_PRIVATE, sizeof(drbbdup_per_thread),
        DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    memset(pt, 0, sizeof(*pt));

    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);

    pt->tls_seg_base = dr_get_dr_segment_base(tls_raw_reg);

    if (is_thread_private) {
        /* Initialise hash table that keeps track of defined cases per
         * basic block (for thread-private DR caches only).
         */
        hashtable_init_ex(&pt->manager_table, HASH_BIT_TABLE, HASH_INTPTR, false, false,
                          drbbdup_destroy_manager, NULL, NULL);
    }

    pt->case_index = 0;
    pt->inserted_restore_all = false;
    pt->orig_analysis_data = NULL;
    if (opts.non_default_case_limit > 0) {
        pt->case_analysis_data =
            dr_custom_alloc(drcontext, DR_ALLOC_THREAD_PRIVATE,
                            sizeof(void *) * opts.non_default_case_limit,
                            DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
        memset(pt->case_analysis_data, 0, sizeof(void *) * opts.non_default_case_limit);
    }

    if (!opts.never_enable_dynamic_handling) {
        /* Dynamically allocated to avoid using space when not needed (128K per
         * thread adds up on large apps), and with explicit unreachable heap.
         */
        pt->hit_counts = (uint16_t *)dr_custom_alloc(
            drcontext, DR_ALLOC_THREAD_PRIVATE, TABLE_SIZE * sizeof(pt->hit_counts[0]),
            DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
        /* Init hit table. */
        for (int i = 0; i < TABLE_SIZE; i++)
            pt->hit_counts[i] = opts.hit_threshold;
        drbbdup_set_tls_raw_slot_val(drcontext, DRBBDUP_HIT_TABLE_SLOT,
                                     (uintptr_t)pt->hit_counts);
    }
}

static void
drbbdup_thread_exit(void *drcontext)
{
    drbbdup_per_thread *pt =
        (drbbdup_per_thread *)drmgr_get_tls_field(drcontext, tls_idx);
    ASSERT(pt != NULL, "thread-local storage should not be NULL");

    if (is_thread_private)
        hashtable_delete(&pt->manager_table);

    if (pt->case_analysis_data != NULL) {
        dr_custom_free(drcontext, DR_ALLOC_THREAD_PRIVATE, pt->case_analysis_data,
                       sizeof(void *) * opts.non_default_case_limit);
    }
    if (pt->hit_counts != NULL) {
        DR_ASSERT_MSG(!opts.never_enable_dynamic_handling,
                      "should not reach here if dynamic cases were disabled globally");
        dr_custom_free(drcontext, DR_ALLOC_THREAD_PRIVATE, pt->hit_counts,
                       TABLE_SIZE * sizeof(pt->hit_counts[0]));
    }
    dr_custom_free(drcontext, DR_ALLOC_THREAD_PRIVATE, pt, sizeof(drbbdup_per_thread));
}

/****************************************************************************
 * STATE RESTORATION
 */

/* TODO i#5686: We need to provide restore-state events to other libraries/clients
 * so we can present just the bb copy containing the target translation point.
 */

static bool
is_our_spill_or_restore(void *drcontext, instr_t *instr, bool *spill OUT,
                        reg_id_t *reg_out OUT, uint *slot_out OUT, uint *offs_out OUT)
{
    bool tls;
    uint offs;
    reg_id_t reg;
    if (!instr_is_reg_spill_or_restore(drcontext, instr, &tls, spill, &reg, &offs))
        return false;
    if (!tls)
        return false;
    if (offs < tls_raw_base ||
        offs > (tls_raw_base + (DRBBDUP_SLOT_COUNT - 1) * sizeof(uintptr_t)))
        return false;
    uint slot = (offs - tls_raw_base) / sizeof(uintptr_t);
    /* DRBBDUP_ENCODING_SLOT and DRBBDUP_HIT_TABLE_SLOT are not used as app spills. */
    if (slot == DRBBDUP_ENCODING_SLOT || slot == DRBBDUP_HIT_TABLE_SLOT)
        return false;
    if (reg_out != NULL)
        *reg_out = reg;
    if (slot_out != NULL)
        *slot_out = slot;
    if (offs_out != NULL)
        *offs_out = offs;
    return true;
}

static bool
drbbdup_event_restore_state(void *drcontext, bool restore_memory,
                            dr_restore_state_info_t *info)
{
    if (info->fragment_info.cache_start_pc == NULL ||
        /* Check for a DR-added prefix. */
        info->raw_mcontext->pc < info->fragment_info.cache_start_pc) {
        /* We have no non-code-cache state to restore. */
        return true;
    }
    if (info->fragment_info.ilist == NULL) {
        /* TODO i#5686: Decode from the cache to build an ilist and pass it to the code
         * below.  We'll need heuristics to generate DRBBDUP_LABEL_START (XXX i#3801 on
         * letting us store our labels).  For now we bail and assume this is rare enough
         * to force an asynch xl8 to retry.
         */
        return false;
    }
    /* We expect spills at the top of the bb from drbbdup_encode_runtime_case()
     * with (duplicated) restores at the top of each copy from
     * drbbdup_insert_landing_restoration().
     */
    reg_id_t slots[DRBBDUP_SLOT_COUNT];
    for (int i = 0; i < DRBBDUP_SLOT_COUNT; i++)
        slots[i] = DR_REG_NULL;
    reg_id_t top_slots[DRBBDUP_SLOT_COUNT];
    byte *pc = info->fragment_info.cache_start_pc;
    byte *containing_copy_start_pc = NULL;
    instr_t *containing_copy_start_instr = NULL;
    bool prior_instr_was_flag_spill = false;
    bool found_copy = false;
    for (instr_t *inst = instrlist_first(info->fragment_info.ilist); inst != NULL;
         inst = instr_get_next(inst)) {
        if (pc == info->raw_mcontext->pc) {
            /* We found the faulting instruction. */
            for (int i = 0; i < DRBBDUP_SLOT_COUNT; i++) {
                if (slots[i] == DR_REG_NULL)
                    continue;
                reg_t val = drbbdup_get_tls_raw_slot_val(drcontext, i);
#if !defined(RISCV64)
                if (i == DRBBDUP_FLAG_REG_SLOT) {
                    reg_t cur = info->mcontext->xflags;
                    cur = dr_merge_arith_flags(cur, val);
                    LOG(drcontext, DR_LOG_ALL, 3,
                        "%s: restoring aflags at %p (+%zd) from slot %d from " PFX
                        " to " PFX "\n",
                        __FUNCTION__, pc, pc - info->fragment_info.cache_start_pc, i,
                        info->mcontext->xflags, cur);
                    info->mcontext->xflags = cur;
                } else {
#endif
                    LOG(drcontext, DR_LOG_ALL, 3,
                        "%s: restoring %s at %p (+%zd) from slot %d from " PFX " to " PFX
                        "\n",
                        __FUNCTION__, get_register_name(slots[i]), pc,
                        pc - info->fragment_info.cache_start_pc, i,
                        reg_get_value(slots[i], info->mcontext), val);
                    reg_set_value(slots[i], info->mcontext, val);
#if !defined(RISCV64)
                }
#endif
            }
            /* Modify the parameters to subsequent restore callbacks so they focus on
             * just the relevant copy so that clients and libraries don't have to be
             * drbbdup-aware (if they have state machines or other construts they may
             * get confused as they cross from one copy to another).  This is a little
             * hacky but the alternative is a big change: integrate drbbdup into drmgr
             * so it can more cleanly control the parameters.
             */
            if (containing_copy_start_pc != NULL) {
                LOG(drcontext, DR_LOG_ALL, 2,
                    "%s: changing cache_start_pc from %p to %p\n", __FUNCTION__,
                    info->fragment_info.cache_start_pc, containing_copy_start_pc);
                info->fragment_info.cache_start_pc = containing_copy_start_pc;
                instr_t *in = instrlist_first(info->fragment_info.ilist);
                instr_t *nxt;
                while (in != containing_copy_start_instr) {
                    nxt = instr_get_next(in);
                    instrlist_remove(info->fragment_info.ilist, in);
                    instr_destroy(drcontext, in);
                    in = nxt;
                }
            }
            return true;
        }
        if (drbbdup_is_at_label(inst, DRBBDUP_LABEL_START)) {
            /* Remember the top slots and re-use them for each copy.  This label is
             * before the next dispatch, but that is what we want as this is the target
             * of the prior no-match dispatch.
             */
            LOG(drcontext, DR_LOG_ALL, 4, "%s: start label at %p\n", __FUNCTION__, pc);
            containing_copy_start_pc = pc;
            containing_copy_start_instr = inst;
            if (!found_copy) {
                found_copy = true;
                memcpy(top_slots, slots, sizeof(top_slots));
            } else {
                memcpy(slots, top_slots, sizeof(slots));
            }
        }
        bool spill;
        reg_id_t reg;
        uint slot, offs;
        if (is_our_spill_or_restore(drcontext, inst, &spill, &reg, &slot, &offs)) {
            LOG(drcontext, DR_LOG_ALL, 4, "%s: %s at %p\n", __FUNCTION__,
                spill ? "spill" : "restore", pc);
            if (spill) {
                if (slots[slot] != DR_REG_NULL && slots[slot] != reg) {
                    ASSERT(false, "spill clobbers another slot: state restore error");
                    return false;
                }
                if (slot == DRBBDUP_FLAG_REG_SLOT)
                    prior_instr_was_flag_spill = true;
                else
                    prior_instr_was_flag_spill = false;
                slots[slot] = reg;
            } else {
                if (slots[slot] == DR_REG_NULL) {
                    ASSERT(false, "restore uses empty slot: state restore error");
                    return false;
                }
                /* Special case: do not clear for the extra restore after the flag spill
                 * in drbbdup_encode_runtime_case().
                 */
                if (!prior_instr_was_flag_spill)
                    slots[slot] = DR_REG_NULL;
                prior_instr_was_flag_spill = false;
            }
        }
        pc += instr_length(drcontext, inst);
        if (pc > info->raw_mcontext->pc)
            break; /* Error, with assert outside the loop. */
    }
    ASSERT(false, "state restore failed to find target instr");
    return false;
}

/****************************************************************************
 * INIT AND EXIT
 */

static bool
drbbdup_check_options(drbbdup_options_t *ops_in)
{
    if (ops_in == NULL)
        return false;
    if (ops_in->set_up_bb_dups == NULL)
        return false;
    if (ops_in->struct_size < offsetof(drbbdup_options_t, analyze_case_ex)) {
        if (ops_in->instrument_instr == NULL)
            return false;
        return true;
    }
    /* Only one of these can be set. */
    if (ops_in->analyze_case != NULL && ops_in->analyze_case_ex != NULL)
        return false;
    /* Exactly one of these must be set. */
    if ((ops_in->instrument_instr != NULL && ops_in->instrument_instr_ex != NULL) ||
        (ops_in->instrument_instr == NULL && ops_in->instrument_instr_ex == NULL))
        return false;
    return true;
}

static bool
drbbdup_check_case_opnd(opnd_t case_opnd)
{
    /* As stated in the docs, runtime case operand must be a memory reference
     * and pointer-sized.
     */
    if (opnd_is_memory_reference(case_opnd) && opnd_get_size(case_opnd) == OPSZ_PTR)
        return true;

    return false;
}

drbbdup_status_t
drbbdup_init(drbbdup_options_t *ops_in)
{
    int count = dr_atomic_add32_return_sum(&drbbdup_init_count, 1);

    /* Return with error if drbbdup has already been initialised. */
    if (count != 1) {
        /* XXX: We do not revert back the counter and therefore consider this error as
         * fatal! */
        ASSERT(false, "drbbdup has already been initialised");
        return DRBBDUP_ERROR_ALREADY_INITIALISED;
    }

    if (!drbbdup_check_options(ops_in))
        return DRBBDUP_ERROR_INVALID_PARAMETER;
    if (ops_in->non_default_case_limit > 0 &&
        !drbbdup_check_case_opnd(ops_in->runtime_case_opnd))
        return DRBBDUP_ERROR_INVALID_OPND;

    if (ops_in->struct_size > sizeof(drbbdup_options_t) ||
        /* This is the first size we exported so it shouldn't be smaller. */
        ops_in->struct_size < offsetof(drbbdup_options_t, max_case_encoding))
        return DRBBDUP_ERROR_INVALID_PARAMETER;
    /* Do not copy from beyond ops_in.  We ruled out ops_in being larger than opts
     * above.  Fields beyond ops_in will be left zero.
     */
    memcpy(&opts, ops_in, ops_in->struct_size);

    drreg_options_t drreg_ops = { sizeof(drreg_ops), 0 /* no regs needed */, false, NULL,
                                  true };
    drmgr_priority_t app2app_priority = { sizeof(drmgr_priority_t),
                                          DRMGR_PRIORITY_APP2APP_NAME_DRBBDUP, NULL, NULL,
                                          DRMGR_PRIORITY_APP2APP_DRBBDUP };
    drmgr_priority_t insert_priority = { sizeof(drmgr_priority_t),
                                         DRMGR_PRIORITY_INSERT_NAME_DRBBDUP, NULL, NULL,
                                         DRMGR_PRIORITY_INSERT_DRBBDUP };
    drmgr_priority_t restore_priority = { sizeof(drmgr_priority_t),
                                          DRMGR_PRIORITY_RESTORE_NAME_DRBBDUP, NULL, NULL,
                                          DRMGR_PRIORITY_RESTORE_DRBBDUP };

    if (!drmgr_register_bb_app2app_event(drbbdup_duplicate_phase, &app2app_priority) ||
        !drmgr_register_bb_instrumentation_ex_event(
            NULL, drbbdup_analyse_phase, drbbdup_link_phase, NULL, &insert_priority) ||
        !drmgr_register_thread_init_event(drbbdup_thread_init) ||
        !drmgr_register_thread_exit_event(drbbdup_thread_exit) ||
        !drmgr_register_restore_state_ex_event_ex(drbbdup_event_restore_state,
                                                  &restore_priority) ||
        !dr_raw_tls_calloc(&tls_raw_reg, &tls_raw_base, DRBBDUP_SLOT_COUNT, 0) ||
        drreg_init(&drreg_ops) != DRREG_SUCCESS)
        return DRBBDUP_ERROR;

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1)
        return DRBBDUP_ERROR;

    case_cache_mutex = dr_mutex_create();
    ASSERT(new_case_cache_pc == NULL, "should be equal to NULL (as lazily initialised).");

    is_thread_private = dr_using_all_private_caches();

    if (!is_thread_private) {
        /* Initialise hash table that keeps track of defined cases per
         * basic block.
         */
        hashtable_init_ex(&global_manager_table, HASH_BIT_TABLE, HASH_INTPTR, false,
                          false, drbbdup_destroy_manager, NULL, NULL);

        rw_lock = dr_rwlock_create();
        if (rw_lock == NULL)
            return DRBBDUP_ERROR;
    }

    if (opts.is_stat_enabled) {
        memset(&stats, 0, sizeof(drbbdup_stats_t));
        stats.struct_size = sizeof(drbbdup_stats_t);
        stat_mutex = dr_mutex_create();
        if (stat_mutex == NULL)
            return DRBBDUP_ERROR;
    }

    return DRBBDUP_SUCCESS;
}

drbbdup_status_t
drbbdup_exit(void)
{
    int count = dr_atomic_add32_return_sum(&drbbdup_init_count, -1);

    if (count == 0) {
        /* Destroy only if initialised (which is done in a lazy fashion). */
        if (new_case_cache_pc != NULL)
            destroy_fp_cache(new_case_cache_pc);
        dr_mutex_destroy(case_cache_mutex);

        if (!drmgr_unregister_bb_app2app_event(drbbdup_duplicate_phase) ||
            !drmgr_unregister_bb_instrumentation_ex_event(NULL, drbbdup_analyse_phase,
                                                          drbbdup_link_phase, NULL) ||
            !drmgr_unregister_thread_init_event(drbbdup_thread_init) ||
            !drmgr_unregister_thread_exit_event(drbbdup_thread_exit) ||
            !drmgr_unregister_restore_state_ex_event(drbbdup_event_restore_state) ||
            !dr_raw_tls_cfree(tls_raw_base, DRBBDUP_SLOT_COUNT) ||
            !drmgr_unregister_tls_field(tls_idx) || drreg_exit() != DRREG_SUCCESS)
            return DRBBDUP_ERROR;

        if (!is_thread_private) {
            hashtable_delete(&global_manager_table);
            dr_rwlock_destroy(rw_lock);
        }

        if (opts.is_stat_enabled)
            dr_mutex_destroy(stat_mutex);

        /* Reset for re-attach. */
        new_case_cache_pc = NULL;

    } else {
        /* Cannot have more than one initialisation of drbbdup. */
        return DRBBDUP_ERROR;
    }

    return DRBBDUP_SUCCESS;
}
