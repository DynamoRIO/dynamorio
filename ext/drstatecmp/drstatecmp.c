/* **********************************************************
 * Copyright (c) 2021 Google, Inc.   All rights reserved.
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
#include "drstatecmp.h"
#include <string.h>
#include <stdint.h>

#ifdef DEBUG
#    define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#else
#    define ASSERT(x, msg)
#endif

typedef struct {
    bool side_effect_free; /* Denotes whether the current bb has side-effects. */
    bool orig_bb_mode; /* True when processing the instructions of the original version of
                          a side-effect-free bb. Becomes false when starting processing
                           the copies of these instructions*/
} drstatecmp_user_data_t;

typedef struct {
    dr_mcontext_t saved_state_for_restore; /* Last saved machine state for restoration. */
    dr_mcontext_t saved_state_for_cmp;     /* Last saved machine state for comparison. */
} drstatecmp_saved_states;

static int tls_idx = -1; /* For thread local storage info. */

/* Label types. */
typedef enum {
    DRSTATECMP_LABEL_TERM,    /* Denotes the terminator of the original bb. */
    DRSTATECMP_LABEL_ORIG_BB, /* Denotes the original bb. */
    DRSTATECMP_LABEL_COPY_BB, /* Denotes the bb copy. */
    DRSTATECMP_LABEL_COUNT,
} drstatecmp_label_t;

/* Reserve space for the label values. */
static ptr_uint_t label_base;
static void
drstatecmp_label_init(void)
{
    label_base = drmgr_reserve_note_range(DRSTATECMP_LABEL_COUNT);
    ASSERT(label_base != DRMGR_NOTE_NONE, "failed to reserve note space");
}

/* Get label values. */
static inline ptr_int_t
get_label_val(drstatecmp_label_t label_type)
{
    return (ptr_int_t)(label_base + label_type);
}

/* Compare instr label against a label_type. Returns true if they match. */
static inline bool
match_label_val(instr_t *instr, drstatecmp_label_t label_type)
{
    return instr_get_note(instr) == (void *)get_label_val(label_type);
}

/* Create and insert bb labels. */
static instr_t *
drstatecmp_insert_labels(void *drcontext, instrlist_t *ilist, instr_t *where,
                         drstatecmp_label_t label_type, bool preinsert)
{
    instr_t *label = INSTR_CREATE_label(drcontext);
    instr_set_meta(label);
    instr_set_note(label, (void *)get_label_val(label_type));
    if (preinsert)
        instrlist_meta_preinsert(ilist, where, label);
    else
        instrlist_meta_postinsert(ilist, where, label);
    return label;
}

/* Returns whether or not instr may have side effects. */
static bool
drstatecmp_may_have_side_effects_instr(instr_t *instr)
{
    /* Instructions with side effects include instructions that write to memory,
     * interrupts, and syscalls. To avoid inter-procedural analysis, all function calls
     * are conservatively assumed to have side effects.
     */
    return instr_writes_memory(instr) || instr_is_interrupt(instr) ||
        instr_is_syscall(instr) || instr_is_call(instr);
}

/****************************************************************************
 * APPLICATION-TO-APPLICATION PHASE
 *
 * In this phase, the side-effect-free basic blocks are duplicated. */

/* Duplicates the bb if it is side-effect-free. */
static dr_emit_flags_t
drstatecmp_duplicate_phase(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                           bool translating, OUT void **user_data)
{
    /* Allocate space for user_data. */
    drstatecmp_user_data_t *data = (drstatecmp_user_data_t *)dr_thread_alloc(
        drcontext, sizeof(drstatecmp_user_data_t));
    memset(data, 0, sizeof(*data));
    *user_data = (void *)data;

    /* Determine whether the basic block is free of side effects. */
    bool side_effect_free = true;
    for (instr_t *inst = instrlist_first_app(bb); inst != NULL;
         inst = instr_get_next_app(inst)) {
        side_effect_free &= !drstatecmp_may_have_side_effects_instr(inst);
    }
    /* Cannot easily execute twice the bb if it has side-effects. Thus, no need to
     * duplicate.
     */
    if (!side_effect_free) {
        data->side_effect_free = false;
        return DR_EMIT_DEFAULT;
    }
    data->side_effect_free = true;

    /* Duplication process.
     * Consider the following example bb:
     *   instr1
     *   instr2
     *   term_instr
     *
     * In this stage, we just duplicate the bb (except for its terminating
     * instruction) and add special labels to the original and duplicated blocks. The
     * duplicated bb is for now skipped with a jump. The insert instrumentation stage will
     * remove this jump, and add saving/restoring of machine state and the state
     * comparison.
     *
     * The example bb is transformed, in this stage, as follows:
     * ORIG_BB:
     *   instr1
     *   instr2
     *   jmp TERM
     *
     * COPY_BB:
     *   instr1
     *   instr2
     * TERM:
     *   term_instr
     *
     */

    /* Create a clone of bb. */
    instrlist_t *copy_bb = instrlist_clone(drcontext, bb);

    /* Create and insert the labels. */
    drstatecmp_insert_labels(drcontext, bb, instrlist_first(bb), DRSTATECMP_LABEL_ORIG_BB,
                             /*preinsert=*/true);
    instr_t *copy_bb_label =
        drstatecmp_insert_labels(drcontext, copy_bb, instrlist_first(copy_bb),
                                 DRSTATECMP_LABEL_COPY_BB, /*preinsert=*/true);
    instr_t *term_inst_copy_bb = instrlist_last_app(copy_bb);
    instr_t *term_label = NULL;
    if (instr_is_cti(term_inst_copy_bb) || instr_is_return(term_inst_copy_bb)) {
        term_label = drstatecmp_insert_labels(drcontext, copy_bb, term_inst_copy_bb,
                                              DRSTATECMP_LABEL_TERM, /*preinsert=*/true);
    } else {
        term_label = drstatecmp_insert_labels(drcontext, copy_bb, term_inst_copy_bb,
                                              DRSTATECMP_LABEL_TERM, /*preinsert=*/false);
    }
    /* Need an operand for the jmp instr targeting this label. */
    opnd_t term_label_opnd = opnd_create_instr(term_label);

    /* Replace the terminating instruction of the original bb with a jmp to the
     * terminating instruction of the bb clone (if any).
     */
    instr_t *term_inst = instrlist_last_app(bb);
    instr_t *jmp_term = XINST_CREATE_jump(drcontext, term_label_opnd);
    if (instr_is_cti(term_inst) || instr_is_return(term_inst)) {
        instrlist_replace(bb, term_inst, jmp_term);
        instr_destroy(drcontext, term_inst);
    } else {
        /* If there is no control transfer, append a jmp at the end of the bb. */
        instrlist_postinsert(bb, term_inst, jmp_term);
    }

    /* Append the instructions of the bb copy to the original bb. */
    instrlist_append(bb, copy_bb_label);
    /* Empty and destroy the bb copy (but not its instrs) since it is not needed anymore.
     */
    instrlist_init(copy_bb);
    instrlist_destroy(drcontext, copy_bb);

    /* We will first process the instructions of the original bb. */
    data->orig_bb_mode = true;

    return DR_EMIT_DEFAULT;
}

/****************************************************************************
 * ANALYSIS PHASE
 *
 * Analysis pass used to maintain the user_data (created in the app2app phase) for the
 * insert instrumentation phase.
 *
 */

static dr_emit_flags_t
drstatecmp_analyze_phase(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                         bool translating, void *user_data)
{
    return DR_EMIT_DEFAULT;
}

/****************************************************************************
 * INSTRUMENTATION INSERTION PHASE
 *
 * In this phase, drstatecmp inserts all the necessary state comparisons.
 */

static void
drstatecmp_save_state_call(int for_cmp)
{
    void *drcontext = dr_get_current_drcontext();
    drstatecmp_saved_states *pt =
        (drstatecmp_saved_states *)drmgr_get_tls_field(drcontext, tls_idx);

    dr_mcontext_t *mcontext = NULL;
    if (for_cmp)
        mcontext = &pt->saved_state_for_cmp;
    else
        mcontext = &pt->saved_state_for_restore;
    mcontext->size = sizeof(*mcontext);
    mcontext->flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, mcontext);
}

static void
drstatecmp_save_state(void *drcontext, instrlist_t *bb, instr_t *instr, bool for_cmp)
{
    dr_insert_clean_call(drcontext, bb, instr, (void *)drstatecmp_save_state_call,
                         false /*fpstate */, 1, OPND_CREATE_INT32((int)for_cmp));
}

static void
drstatecmp_restore_state_call(void)
{
    void *drcontext = dr_get_current_drcontext();
    drstatecmp_saved_states *pt =
        (drstatecmp_saved_states *)drmgr_get_tls_field(drcontext, tls_idx);

    dr_mcontext_t *mcontext = &pt->saved_state_for_restore;
    mcontext->size = sizeof(*mcontext);
    mcontext->flags = DR_MC_ALL;
    dr_set_mcontext(drcontext, mcontext);
}

static void
drstatecmp_restore_state(void *drcontext, instrlist_t *bb, instr_t *instr)
{
    dr_insert_clean_call(drcontext, bb, instr, (void *)drstatecmp_restore_state_call,
                         false /*fpstate */, 0);
}

static void
drstatecmp_check_gpr_value(const char *name, reg_t reg_value, reg_t reg_expected)
{
    DR_ASSERT_MSG(reg_value == reg_expected, name);
}

#ifdef AARCHXX
static void
drstatecmp_check_xflags_value(const char *name, uint reg_value, uint reg_expected)
{
    DR_ASSERT_MSG(reg_value == reg_expected, name);
}
#endif

static void
drstatecmp_check_simd_value
#ifdef X86
    (dr_zmm_t *value, dr_zmm_t *expected)
{
    DR_ASSERT_MSG(!memcmp(value, expected, sizeof(dr_zmm_t)), "SIMD mismatch");
}
#elif defined(AARCHXX)
    (dr_simd_t *value, dr_simd_t *expected)
{
    DR_ASSERT_MSG(!memcmp(value, expected, sizeof(dr_simd_t)), "SIMD mismatch");
}
#endif

#ifdef X86
static void
drstatecmp_check_opmask_value(dr_opmask_t opmask_value, dr_opmask_t opmask_expected)
{
    DR_ASSERT_MSG(opmask_value == opmask_expected, "opmask mismatch");
}
#endif

static void
drstatecmp_compare_state_call(void)
{
    void *drcontext = dr_get_current_drcontext();
    drstatecmp_saved_states *pt =
        (drstatecmp_saved_states *)drmgr_get_tls_field(drcontext, tls_idx);

    dr_mcontext_t *mc_instrumented = &pt->saved_state_for_cmp;
    dr_mcontext_t mc_expected;
    mc_expected.size = sizeof(mc_expected);
    mc_expected.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mc_expected);

#ifdef X86
    drstatecmp_check_gpr_value("xdi", mc_instrumented->xdi, mc_expected.xdi);
    drstatecmp_check_gpr_value("xsi", mc_instrumented->xsi, mc_expected.xsi);
    drstatecmp_check_gpr_value("xbp", mc_instrumented->xbp, mc_expected.xbp);

    drstatecmp_check_gpr_value("xbx", mc_instrumented->xbx, mc_expected.xbx);
    drstatecmp_check_gpr_value("xcx", mc_instrumented->xcx, mc_expected.xcx);
    drstatecmp_check_gpr_value("xdx", mc_instrumented->xdx, mc_expected.xdx);

#    ifdef X64
    drstatecmp_check_gpr_value("xax", mc_instrumented->xax, mc_expected.xax);
#    endif

#    ifdef X64
    drstatecmp_check_gpr_value("r8", mc_instrumented->r8, mc_expected.r8);
    drstatecmp_check_gpr_value("r9", mc_instrumented->r9, mc_expected.r9);
    drstatecmp_check_gpr_value("r10", mc_instrumented->r10, mc_expected.r10);
    drstatecmp_check_gpr_value("r11", mc_instrumented->r11, mc_expected.r11);
    drstatecmp_check_gpr_value("r12", mc_instrumented->r12, mc_expected.r12);
    drstatecmp_check_gpr_value("r13", mc_instrumented->r13, mc_expected.r13);
    drstatecmp_check_gpr_value("r14", mc_instrumented->r14, mc_expected.r14);
    drstatecmp_check_gpr_value("r15", mc_instrumented->r15, mc_expected.r15);
#    endif

    drstatecmp_check_gpr_value("xflags", mc_instrumented->xflags, mc_expected.xflags);
    for (int i = 0; i < MCXT_NUM_OPMASK_SLOTS; i++) {
        drstatecmp_check_opmask_value(mc_instrumented->opmask[i], mc_expected.opmask[i]);
    }

#elif defined(AARCHXX)
    drstatecmp_check_gpr_value("r0", mc_instrumented->r0, mc_expected.r0);
    drstatecmp_check_gpr_value("r1", mc_instrumented->r1, mc_expected.r1);
    drstatecmp_check_gpr_value("r2", mc_instrumented->r2, mc_expected.r2);
    drstatecmp_check_gpr_value("r3", mc_instrumented->r3, mc_expected.r3);
    drstatecmp_check_gpr_value("r4", mc_instrumented->r4, mc_expected.r4);
    drstatecmp_check_gpr_value("r5", mc_instrumented->r5, mc_expected.r5);
    drstatecmp_check_gpr_value("r6", mc_instrumented->r6, mc_expected.r6);
    drstatecmp_check_gpr_value("r7", mc_instrumented->r7, mc_expected.r7);
    drstatecmp_check_gpr_value("r8", mc_instrumented->r8, mc_expected.r8);
    drstatecmp_check_gpr_value("r9", mc_instrumented->r9, mc_expected.r9);
    drstatecmp_check_gpr_value("r10", mc_instrumented->r10, mc_expected.r10);
    drstatecmp_check_gpr_value("r11", mc_instrumented->r11, mc_expected.r11);
    drstatecmp_check_gpr_value("r12", mc_instrumented->r12, mc_expected.r12);

#    ifdef X64
    drstatecmp_check_gpr_value("r13", mc_instrumented->r13, mc_expected.r13);
    drstatecmp_check_gpr_value("r14", mc_instrumented->r14, mc_expected.r14);
    drstatecmp_check_gpr_value("r15", mc_instrumented->r15, mc_expected.r15);
    drstatecmp_check_gpr_value("r16", mc_instrumented->r16, mc_expected.r16);
    drstatecmp_check_gpr_value("r17", mc_instrumented->r17, mc_expected.r17);
    drstatecmp_check_gpr_value("r18", mc_instrumented->r18, mc_expected.r18);
    drstatecmp_check_gpr_value("r19", mc_instrumented->r19, mc_expected.r19);
    drstatecmp_check_gpr_value("r20", mc_instrumented->r20, mc_expected.r20);
    drstatecmp_check_gpr_value("r21", mc_instrumented->r21, mc_expected.r21);
    drstatecmp_check_gpr_value("r22", mc_instrumented->r22, mc_expected.r22);
    drstatecmp_check_gpr_value("r23", mc_instrumented->r23, mc_expected.r23);
    drstatecmp_check_gpr_value("r24", mc_instrumented->r24, mc_expected.r24);
    drstatecmp_check_gpr_value("r25", mc_instrumented->r25, mc_expected.r25);
    drstatecmp_check_gpr_value("r26", mc_instrumented->r26, mc_expected.r26);
    drstatecmp_check_gpr_value("r27", mc_instrumented->r27, mc_expected.r27);
    drstatecmp_check_gpr_value("r28", mc_instrumented->r28, mc_expected.r28);
    drstatecmp_check_gpr_value("r29", mc_instrumented->r29, mc_expected.r29);
#    endif

    drstatecmp_check_gpr_value("lr", mc_instrumented->lr, mc_expected.lr);
    drstatecmp_check_xflags_value("xflags", mc_instrumented->xflags, mc_expected.xflags);

#else
#    error NYI
#endif

    drstatecmp_check_gpr_value("xsp", mc_instrumented->xsp, mc_expected.xsp);
    for (int i = 0; i < MCXT_NUM_SIMD_SLOTS; i++) {
        drstatecmp_check_simd_value(&mc_instrumented->simd[i], &mc_expected.simd[i]);
    }
}

static void
drstatecmp_compare_state(void *drcontext, instrlist_t *bb, instr_t *instr)
{
    dr_insert_clean_call(drcontext, bb, instr, (void *)drstatecmp_compare_state_call,
                         false /*fpstate */, 0);
}

static void
drstatecmp_insert_instrument_bb_with_side_effects(void)
{
    /* TODO i#4678: Add checks for bbs with side effects.  */
}

static void
drstatecmp_insert_instrument_bb_orig_side_effect_free(void *drcontext, instrlist_t *bb,
                                                      instr_t *instr,
                                                      drstatecmp_user_data_t *data)
{
    /* Save state at the beginning of this bb in order to restore it at the end of
     * the bb (to enable re-execution of the bb). */
    if (match_label_val(instr, DRSTATECMP_LABEL_ORIG_BB)) {
        drstatecmp_save_state(drcontext, bb, instr, /*for_cmp=*/false);
    }

    /* Change mode to process the copy bb once we encounter the COPY_BB label.
     * Save the state at the end of this bb for later comparison, and remove the
     * previously inserted jmp instr to the TERM label of the copy bb and let it fall
     * through to the instructions of the copy bb (that will be instrumentation-free).
     * Finally, restore the machine state to the state before executing the original
     * version of this bb (allows us to re-execute this bb).
     */
    else if (match_label_val(instr, DRSTATECMP_LABEL_COPY_BB)) {
        data->orig_bb_mode = false;
        instr_t *prev_instr = instr_get_prev_app(instr);
        instrlist_remove(bb, prev_instr);
        instr_destroy(drcontext, prev_instr);
        drstatecmp_save_state(drcontext, bb, instr, /*for_cmp=*/true);
        drstatecmp_restore_state(drcontext, bb, instr);
    }
}

static void
drstatecmp_insert_instrument_bb_copy_side_effect_free(void *drcontext, instrlist_t *bb,
                                                      instr_t *instr)
{
    /* Remove all instrumentation code in the bb copy to detect any
     * unintended clobbering by instrumentation.
     */
    if (!instr_is_app(instr)) {
        instrlist_remove(bb, instr);
        instr_destroy(drcontext, instr);
    }
    /* Compare current state at the end of the bb with the saved state at the
     * end of the original (instrumented) bb.
     */
    if (instr == instrlist_last_app(bb)) {
        drstatecmp_compare_state(drcontext, bb, instr);
    }
}

static dr_emit_flags_t
drstatecmp_insert_phase(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                        bool for_trace, bool translating, void *user_data)
{
    drstatecmp_user_data_t *data = (drstatecmp_user_data_t *)user_data;
    if (!data->side_effect_free) {
        drstatecmp_insert_instrument_bb_with_side_effects();
    } else {
        if (data->orig_bb_mode) {
            drstatecmp_insert_instrument_bb_orig_side_effect_free(drcontext, bb, instr,
                                                                  data);
        } else {
            drstatecmp_insert_instrument_bb_copy_side_effect_free(drcontext, bb, instr);
        }
    }

    /* Free the user_data when we are done with this bb. */
    if (instr == instrlist_last(bb))
        dr_thread_free(drcontext, user_data, sizeof(drstatecmp_user_data_t));

    return DR_EMIT_DEFAULT;
}

/****************************************************************************
 *  THREAD INIT AND EXIT
 */

static void
drstatecmp_thread_init(void *drcontext)
{
    drstatecmp_saved_states *pt = (drstatecmp_saved_states *)dr_thread_alloc(
        drcontext, sizeof(drstatecmp_saved_states));
    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
    memset(pt, 0, sizeof(*pt));
}

static void
drstatecmp_thread_exit(void *drcontext)
{
    drstatecmp_saved_states *pt =
        (drstatecmp_saved_states *)drmgr_get_tls_field(drcontext, tls_idx);
    ASSERT(pt != NULL, "thread-local storage should not be NULL");
    dr_thread_free(drcontext, pt, sizeof(drstatecmp_saved_states));
}

/***************************************************************************
 * INIT AND EXIT
 */

static int drstatecmp_init_count;

drstatecmp_status_t
drstatecmp_init(void)
{
    int count = dr_atomic_add32_return_sum(&drstatecmp_init_count, 1);
    if (count != 1)
        return DRSTATECMP_ERROR_ALREADY_INITIALISED;

    drmgr_priority_t priority = { sizeof(drmgr_priority_t),
                                  DRMGR_PRIORITY_NAME_DRSTATECMP, NULL, NULL,
                                  DRMGR_PRIORITY_DRSTATECMP };

    drmgr_init();

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1)
        return DRSTATECMP_ERROR;

    drstatecmp_label_init();

    if (!drmgr_register_thread_init_event(drstatecmp_thread_init) ||
        !drmgr_register_thread_exit_event(drstatecmp_thread_exit) ||
        !drmgr_register_bb_instrumentation_ex_event(
            drstatecmp_duplicate_phase, drstatecmp_analyze_phase, drstatecmp_insert_phase,
            NULL, &priority))
        return DRSTATECMP_ERROR;

    return DRSTATECMP_SUCCESS;
}

drstatecmp_status_t
drstatecmp_exit(void)
{
    int count = dr_atomic_add32_return_sum(&drstatecmp_init_count, -1);
    if (count != 0)
        return DRSTATECMP_ERROR_NOT_INITIALIZED;

    if (!drmgr_unregister_thread_init_event(drstatecmp_thread_init) ||
        !drmgr_unregister_thread_exit_event(drstatecmp_thread_exit) ||
        !drmgr_unregister_tls_field(tls_idx) ||
        !drmgr_unregister_bb_instrumentation_ex_event(drstatecmp_duplicate_phase,
                                                      drstatecmp_analyze_phase,
                                                      drstatecmp_insert_phase, NULL))
        return DRSTATECMP_ERROR;

    drmgr_exit();

    return DRSTATECMP_SUCCESS;
}
