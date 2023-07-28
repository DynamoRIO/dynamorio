/* **********************************************************
 * Copyright (c) 2021-2022 Google, Inc.   All rights reserved.
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
#include "drstatecmp.h"
#include "../ext_utils.h"
#include <string.h>
#include <stdint.h>

#ifdef DEBUG
#    define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#else
#    define ASSERT(x, msg)
#endif

typedef struct {
    bool side_effect_free_bb; /* Denotes whether the bb has side-effects. */
#ifdef X86
    bool dead_aflags; /* Denotes that the aflags must be dead on bb entry. */
#endif
    union {
        instrlist_t *pre_app2app_bb; /* Copy of the pre-app2app bb. */
        instrlist_t *golden_bb_copy; /* Golden copy of the bb (either pre- or post-app2app
                                      * bb), used for state comparison with re-execution.
                                      */
    };
} drstatecmp_user_data_t;

typedef struct {
    dr_mcontext_t saved_state_for_restore; /* Last saved machine state for restoration. */
    dr_mcontext_t saved_state_for_cmp;     /* Last saved machine state for comparison. */
} drstatecmp_saved_states_t;

static drstatecmp_options_t ops;

static int tls_idx = -1; /* For thread local storage info. */

/* Label types. */
typedef enum {
    DRSTATECMP_LABEL_TERM,    /* Denotes the terminator of the original bb. */
    DRSTATECMP_LABEL_ORIG_BB, /* Denotes the beginning of the original bb. */
    DRSTATECMP_LABEL_COPY_BB, /* Denotes the beginning of the bb copy. */
    DRSTATECMP_LABEL_COUNT,
} drstatecmp_label_t;

static ptr_uint_t label_base;
/* Reserve space for the label values. */
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
    ASSERT(label_type < DRSTATECMP_LABEL_COUNT, "invalid label");
    return (ptr_int_t)(label_base + label_type);
}

/* Create and insert bb labels. */
static instr_t *
drstatecmp_insert_label(void *drcontext, instrlist_t *ilist, instr_t *where,
                        drstatecmp_label_t label_type, bool preinsert)
{
    instr_t *label = INSTR_CREATE_label(drcontext);
    instr_set_note(label, (void *)get_label_val(label_type));
    if (preinsert)
        instrlist_meta_preinsert(ilist, where, label);
    else
        instrlist_meta_postinsert(ilist, where, label);
    return label;
}

typedef struct {
    instr_t *orig_bb_start;
    instr_t *copy_bb_start;
    instr_t *term;
} drstatecmp_dup_labels_t;

typedef enum {
    DRSTATECMP_SKIP_CHECK_LR = 0x01,
    DRSTATECMP_SKIP_CHECK_AFLAGS = 0x02
} drstatecmp_check_flags_t;

/* Returns whether or not instr may have side effects. */
static bool
drstatecmp_may_have_side_effects_instr(instr_t *instr)
{
    /* Instructions with side effects include instructions that write to memory,
     * interrupts, and syscalls.
     */
    if (instr_writes_memory(instr) || instr_is_interrupt(instr) ||
        instr_is_syscall(instr))
        return true;

#ifdef X86
    /* Avoid instructions that with identical input state can yield different results on
     * re-executions.
     */
    int opc = instr_get_opcode(instr);
    if (opc == OP_rdtsc || opc == OP_rdtscp)
        return true;
#endif

    return false;
}

/* Returns whether a basic block can be checked by drstatecmp. */
bool
drstatecmp_bb_checks_enabled(instrlist_t *bb)
{
    for (instr_t *inst = instrlist_first_app(bb); inst != NULL;
         inst = instr_get_next_app(inst)) {

        /* Ignore the last instruction if it is a control transfer instruction,
         * because it will not be re-executed.
         */
        if (inst == instrlist_last_app(bb) && instr_is_cti(inst))
            continue;

        if (drstatecmp_may_have_side_effects_instr(inst))
            return false;
    }
    return true;
}

#ifdef X86
/* Returns whether the aflags must be dead on bb entry.
 * Returns true if aflags are first written before read.
 */
bool
drstatecmp_aflags_must_be_dead(instrlist_t *bb)
{
    for (instr_t *inst = instrlist_first_app(bb); inst != NULL;
         inst = instr_get_next_app(inst)) {
        uint aflags = instr_get_arith_flags(inst, DR_QUERY_INCLUDE_COND_SRCS);
        if (aflags & EFLAGS_READ_ARITH)
            return false;
        if (aflags & EFLAGS_WRITE_ARITH)
            return true;
    }
    /* Cannot determine aflags liveness. Neither read nor written in this basic
     * block.
     */
    return false;
}
#endif

/****************************************************************************
 * APPLICATION-TO-APPLICATION PHASE
 *
 * Save a pre-app2app copy of side-effect-free basic blocks.
 * It is assumed that this pass has the highest priority among all app2app passes
 * and thus it is able to capture the pre-app2app state.
 */

static dr_emit_flags_t
drstatecmp_app2app_phase(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                         bool translating, OUT void **user_data)
{
    /* Allocate space for user_data. */
    drstatecmp_user_data_t *data = (drstatecmp_user_data_t *)dr_thread_alloc(
        drcontext, sizeof(drstatecmp_user_data_t));
    memset(data, 0, sizeof(*data));
    *user_data = (void *)data;

    /* Determine whether this basic block can be checked by drstatecmp.
     * In the current implementation, a basic block needs to be side-effect-free
     * (except for the last instruction if it is a control transfer instruction).
     */
    data->side_effect_free_bb = true;
    if (!drstatecmp_bb_checks_enabled(bb)) {
        data->side_effect_free_bb = false;
        return DR_EMIT_DEFAULT;
    }

    /* Current bb is side-effect free. Save a copy of this pre-app2app bb. */
    data->pre_app2app_bb = instrlist_clone(drcontext, bb);
    return DR_EMIT_DEFAULT;
}

/****************************************************************************
 * ANALYSIS PHASE
 *
 * The analysis phase determines which copy of each side-effect bb should be used as the
 * golden copy for comparison. There are two options: i) pre-app2app-phase copy, where
 * the code just contains the original app instructions, and ii) post-app2app-phase copy.
 * The first option can catch bugs in app2app passes and it is selected unless any of
 * the original app instructions requires emulation (true emulation) or application
 * instructions were removed from the block.  In the emulation case, the
 * pre-app2app code is not executable, whereas the post-app2app code contains emulation
 * code and thus can be used for state checks with re-execution. Cases that do
 * not require the emulation sequence for re-execution include instruction
 * refactoring that simplify instrumentation but do not correspond to true
 * emulation (e.g., drutil_expand_rep_string() and drx_expand_scatter_gather()).
 *
 */

static dr_emit_flags_t
drstatecmp_analyze_phase(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                         bool translating, void *user_data)
{
    drstatecmp_user_data_t *data = (drstatecmp_user_data_t *)user_data;

    if (!data->side_effect_free_bb)
        return DR_EMIT_DEFAULT;

#ifdef X86
    data->dead_aflags = drstatecmp_aflags_must_be_dead(bb);
#endif

    /* We detect truncation by counting app instrs.
     * XXX: To handle any client we should compare actual instrs but for now
     * we assume typical clients who do not replace app instrs except when
     * marked as emulation.
     */
    int pre_app2app_count = 0;
    for (instr_t *inst = instrlist_first(data->pre_app2app_bb); inst != NULL;
         inst = instr_get_next(inst)) {
        if (instr_is_app(inst))
            ++pre_app2app_count;
    }

    int post_app2app_count = 0;
    for (instr_t *inst = instrlist_first(bb); inst != NULL; inst = instr_get_next(inst)) {
        if (drmgr_is_emulation_start(inst)) {
            emulated_instr_t emulated_inst;
            drmgr_get_emulated_instr_data(inst, &emulated_inst);
            if (!TEST(DR_EMULATE_INSTR_ONLY, emulated_inst.flags)) {
                /* Emulation sequence required for re-execution and golden bb copy should
                 * be the post-app2app copy.
                 */
                instrlist_clear_and_destroy(drcontext, data->pre_app2app_bb);
                data->golden_bb_copy = instrlist_clone(drcontext, bb);
                return DR_EMIT_DEFAULT;
            }
        }
        if (instr_is_app(inst))
            ++post_app2app_count;
    }

    if (pre_app2app_count != post_app2app_count) {
        /* The app2app phase truncated or otherwise edited the block. */
        instrlist_clear_and_destroy(drcontext, data->pre_app2app_bb);
        data->golden_bb_copy = instrlist_clone(drcontext, bb);
        return DR_EMIT_DEFAULT;
    }

    /* Emulation not required for re-execution and thus it is safe to use the pre-app2app
     * bb. No change needed since data->golden_bb_copy already points to the correct copy.
     */
    return DR_EMIT_DEFAULT;
}

/****************************************************************************
 * INSTRUMENTATION INSERTION PHASE
 *
 * Instrumentation insertion pass is used to maintain the user_data (created in the
 * app2app phase) for the post-instrumentation phase.
 *
 */

static dr_emit_flags_t
drstatecmp_insert_phase(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                        bool for_trace, bool translating, void *user_data)
{
    return DR_EMIT_DEFAULT;
}

/****************************************************************************
 * INSTRU2INSTRU PHASE
 *
 * Instru2instru pass is used to maintain the user_data (created in the
 * app2app phase) for the post-instrumentation phase.
 *
 */

static dr_emit_flags_t
drstatecmp_instru2instru_phase(void *drcontext, void *tag, instrlist_t *bb,
                               bool for_trace, bool translating, void *user_data)
{
    return DR_EMIT_DEFAULT;
}

/****************************************************************************
 * POST-INSTRUMENTATION PHASE
 *
 * In this phase, drstatecmp inserts all the necessary state comparisons.
 */

static void
drstatecmp_duplicate_bb(void *drcontext, instrlist_t *bb, drstatecmp_user_data_t *data,
                        drstatecmp_dup_labels_t *labels)
{
    /* Duplication process.
     * Consider the following example bb:
     *   instr1
     *   meta_instr
     *   instr2
     *   term_instr
     *
     * In this stage, we just duplicate the bb (except for its terminating
     * instruction and meta instructions) and add special labels to the original and
     * duplicated blocks. Note that there might be no term_instr (no control
     * transfer instruction) and the bb just falls-through. Also note that we use for the
     * bb copy the golden_copy that was determined in the analysis phase. This golden copy
     * contains the app instructions of the bb without any of the instrumentation. It
     * might also contain app2app changes if emulation was required.
     *
     * The example bb is transformed, in this stage, as follows:
     * ORIG_BB:
     *   instr1
     *   meta_instr
     *   instr2
     *
     * COPY_BB:
     *   instr1
     *   instr2
     *
     * TERM:
     *   term_instr
     *
     */

    /* Get an instrumentation-free copy of the bb which is the golden copy kept
     * in the user_data.
     */
    instrlist_t *copy_bb = data->golden_bb_copy;

    /* Create and insert the labels. */
    labels->orig_bb_start = drstatecmp_insert_label(drcontext, bb, instrlist_first(bb),
                                                    DRSTATECMP_LABEL_ORIG_BB,
                                                    /*preinsert=*/true);
    labels->copy_bb_start =
        drstatecmp_insert_label(drcontext, copy_bb, instrlist_first(copy_bb),
                                DRSTATECMP_LABEL_COPY_BB, /*preinsert=*/true);
    /* Insert the TERM label before the terminating instruction or after the
     * last instruction if the bb falls through.
     */
    instr_t *term_inst_copy_bb = instrlist_last_app(copy_bb);
    bool preinsert = true;
    if (!instr_is_cti(term_inst_copy_bb))
        preinsert = false;
    labels->term = drstatecmp_insert_label(drcontext, copy_bb, term_inst_copy_bb,
                                           DRSTATECMP_LABEL_TERM, preinsert);

    /* Delete the terminating instruction of the original bb (if any) to let the original
     * bb fall through to its copy for re-execution.
     */
    instr_t *term_inst = instrlist_last_app(bb);
    if (instr_is_cti(term_inst)) {
        instrlist_remove(bb, term_inst);
        instr_destroy(drcontext, term_inst);
    }

    /* Append the instructions of the bb copy to the original bb. */
    instrlist_append(bb, labels->copy_bb_start);
    /* Empty and destroy the bb copy (but not its instructions) since it is not needed
     * anymore.
     */
    instrlist_init(copy_bb);
    instrlist_destroy(drcontext, copy_bb);
}

static void
drstatecmp_save_state_call(int for_cmp)
{
    void *drcontext = dr_get_current_drcontext();
    drstatecmp_saved_states_t *pt =
        (drstatecmp_saved_states_t *)drmgr_get_tls_field(drcontext, tls_idx);

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
                         false /* fpstate */, 1, OPND_CREATE_INT32((int)for_cmp));
}

static void
drstatecmp_restore_state_call(void)
{
    void *drcontext = dr_get_current_drcontext();
    drstatecmp_saved_states_t *pt =
        (drstatecmp_saved_states_t *)drmgr_get_tls_field(drcontext, tls_idx);

    dr_mcontext_t *mcontext = &pt->saved_state_for_restore;
    mcontext->size = sizeof(*mcontext);
    mcontext->flags = DR_MC_ALL;
    dr_set_mcontext(drcontext, mcontext);
}

static void
drstatecmp_restore_state(void *drcontext, instrlist_t *bb, instr_t *instr)
{
    dr_insert_clean_call(drcontext, bb, instr, (void *)drstatecmp_restore_state_call,
                         false /* fpstate */, 0);
}

static void
drstatecmp_report_error(const char *msg, void *tag)
{
    if (ops.error_callback != NULL) {
        (*ops.error_callback)(msg, tag);
    } else {
        byte *bb_start_pc = dr_fragment_app_pc(tag);
        void *drcontext = dr_get_current_drcontext();
        instrlist_t *bb = decode_as_bb(drcontext, bb_start_pc);
        /* XXX: would also be useful to emit the mcontext values. */
        dr_fprintf(STDERR, "Application basic block where mismatch detected: \n");
        instrlist_disassemble(drcontext, bb_start_pc, bb, STDERR);
        DR_ASSERT_MSG(false, msg);
    }
}

static void
drstatecmp_check_gpr_value(const char *name, void *tag, reg_t reg_value,
                           reg_t reg_expected)
{
    if (reg_value != reg_expected)
        drstatecmp_report_error(name, tag);
}

#ifdef AARCHXX
static void
drstatecmp_check_xflags_value(const char *name, void *tag, uint reg_value,
                              uint reg_expected)
{
    if (reg_value != reg_expected)
        drstatecmp_report_error(name, tag);
}
#endif

static void
drstatecmp_check_simd_value
#ifdef X86
    (void *tag, dr_zmm_t *value, dr_zmm_t *expected)
{
    if (memcmp(value, expected, sizeof(dr_zmm_t)))
        drstatecmp_report_error("SIMD mismatch", tag);
}
#elif defined(AARCHXX)
    (void *tag, dr_simd_t *value, dr_simd_t *expected)
{
    size_t vl = proc_get_vector_length_bytes();
    if (memcmp(value, expected, vl))
        drstatecmp_report_error("SIMD mismatch", tag);
}
#elif defined(RISCV64)
    (void *tag, dr_simd_t *value, dr_simd_t *expected)
{
    /* FIXME i#3544: Not implemented */
    ASSERT(false, "Not implemented");
}
#endif

#ifdef X86
static void
drstatecmp_check_opmask_value(void *tag, dr_opmask_t opmask_value,
                              dr_opmask_t opmask_expected)
{
    if (opmask_value != opmask_expected)
        drstatecmp_report_error("opmask mismatch", tag);
}
#endif

static void
drstatecmp_check_machine_state(dr_mcontext_t *mc_instrumented, dr_mcontext_t *mc_expected,
                               int flags, void *tag)
{
#ifdef X86
    drstatecmp_check_gpr_value("xdi", tag, mc_instrumented->xdi, mc_expected->xdi);
    drstatecmp_check_gpr_value("xsi", tag, mc_instrumented->xsi, mc_expected->xsi);
    drstatecmp_check_gpr_value("xbp", tag, mc_instrumented->xbp, mc_expected->xbp);

    drstatecmp_check_gpr_value("xax", tag, mc_instrumented->xax, mc_expected->xax);
    drstatecmp_check_gpr_value("xbx", tag, mc_instrumented->xbx, mc_expected->xbx);
    drstatecmp_check_gpr_value("xcx", tag, mc_instrumented->xcx, mc_expected->xcx);
    drstatecmp_check_gpr_value("xdx", tag, mc_instrumented->xdx, mc_expected->xdx);

#    ifdef X64
    drstatecmp_check_gpr_value("r8", tag, mc_instrumented->r8, mc_expected->r8);
    drstatecmp_check_gpr_value("r9", tag, mc_instrumented->r9, mc_expected->r9);
    drstatecmp_check_gpr_value("r10", tag, mc_instrumented->r10, mc_expected->r10);
    drstatecmp_check_gpr_value("r11", tag, mc_instrumented->r11, mc_expected->r11);
    drstatecmp_check_gpr_value("r12", tag, mc_instrumented->r12, mc_expected->r12);
    drstatecmp_check_gpr_value("r13", tag, mc_instrumented->r13, mc_expected->r13);
    drstatecmp_check_gpr_value("r14", tag, mc_instrumented->r14, mc_expected->r14);
    drstatecmp_check_gpr_value("r15", tag, mc_instrumented->r15, mc_expected->r15);
#    endif

    if (!TEST(DRSTATECMP_SKIP_CHECK_AFLAGS, flags)) {
        drstatecmp_check_gpr_value("xflags", tag, mc_instrumented->xflags,
                                   mc_expected->xflags);
    }

    for (int i = 0; i < MCXT_NUM_OPMASK_SLOTS; i++) {
        drstatecmp_check_opmask_value(tag, mc_instrumented->opmask[i],
                                      mc_expected->opmask[i]);
    }
#elif defined(AARCHXX)
    drstatecmp_check_gpr_value("r0", tag, mc_instrumented->r0, mc_expected->r0);
    drstatecmp_check_gpr_value("r1", tag, mc_instrumented->r1, mc_expected->r1);
    drstatecmp_check_gpr_value("r2", tag, mc_instrumented->r2, mc_expected->r2);
    drstatecmp_check_gpr_value("r3", tag, mc_instrumented->r3, mc_expected->r3);
    drstatecmp_check_gpr_value("r4", tag, mc_instrumented->r4, mc_expected->r4);
    drstatecmp_check_gpr_value("r5", tag, mc_instrumented->r5, mc_expected->r5);
    drstatecmp_check_gpr_value("r6", tag, mc_instrumented->r6, mc_expected->r6);
    drstatecmp_check_gpr_value("r7", tag, mc_instrumented->r7, mc_expected->r7);
    drstatecmp_check_gpr_value("r8", tag, mc_instrumented->r8, mc_expected->r8);
    drstatecmp_check_gpr_value("r9", tag, mc_instrumented->r9, mc_expected->r9);
    drstatecmp_check_gpr_value("r10", tag, mc_instrumented->r10, mc_expected->r10);
    drstatecmp_check_gpr_value("r11", tag, mc_instrumented->r11, mc_expected->r11);
    drstatecmp_check_gpr_value("r12", tag, mc_instrumented->r12, mc_expected->r12);

#    ifdef X64
    drstatecmp_check_gpr_value("r13", tag, mc_instrumented->r13, mc_expected->r13);
    drstatecmp_check_gpr_value("r14", tag, mc_instrumented->r14, mc_expected->r14);
    drstatecmp_check_gpr_value("r15", tag, mc_instrumented->r15, mc_expected->r15);
    drstatecmp_check_gpr_value("r16", tag, mc_instrumented->r16, mc_expected->r16);
    drstatecmp_check_gpr_value("r17", tag, mc_instrumented->r17, mc_expected->r17);
    drstatecmp_check_gpr_value("r18", tag, mc_instrumented->r18, mc_expected->r18);
    drstatecmp_check_gpr_value("r19", tag, mc_instrumented->r19, mc_expected->r19);
    drstatecmp_check_gpr_value("r20", tag, mc_instrumented->r20, mc_expected->r20);
    drstatecmp_check_gpr_value("r21", tag, mc_instrumented->r21, mc_expected->r21);
    drstatecmp_check_gpr_value("r22", tag, mc_instrumented->r22, mc_expected->r22);
    drstatecmp_check_gpr_value("r23", tag, mc_instrumented->r23, mc_expected->r23);
    drstatecmp_check_gpr_value("r24", tag, mc_instrumented->r24, mc_expected->r24);
    drstatecmp_check_gpr_value("r25", tag, mc_instrumented->r25, mc_expected->r25);
    drstatecmp_check_gpr_value("r26", tag, mc_instrumented->r26, mc_expected->r26);
    drstatecmp_check_gpr_value("r27", tag, mc_instrumented->r27, mc_expected->r27);
    drstatecmp_check_gpr_value("r28", tag, mc_instrumented->r28, mc_expected->r28);
    drstatecmp_check_gpr_value("r29", tag, mc_instrumented->r29, mc_expected->r29);
#    endif

    if (!TEST(DRSTATECMP_SKIP_CHECK_LR, flags))
        drstatecmp_check_gpr_value("lr", tag, mc_instrumented->lr, mc_expected->lr);

    drstatecmp_check_xflags_value("xflags", tag, mc_instrumented->xflags,
                                  mc_expected->xflags);
#elif defined(RISCV64)
    drstatecmp_check_gpr_value("x0", tag, mc_instrumented->x0, mc_expected->x0);
    drstatecmp_check_gpr_value("x1", tag, mc_instrumented->x1, mc_expected->x1);
    drstatecmp_check_gpr_value("x2", tag, mc_instrumented->x2, mc_expected->x2);
    drstatecmp_check_gpr_value("x3", tag, mc_instrumented->x3, mc_expected->x3);
    drstatecmp_check_gpr_value("x4", tag, mc_instrumented->x4, mc_expected->x4);
    drstatecmp_check_gpr_value("x5", tag, mc_instrumented->x5, mc_expected->x5);
    drstatecmp_check_gpr_value("x6", tag, mc_instrumented->x6, mc_expected->x6);
    drstatecmp_check_gpr_value("x7", tag, mc_instrumented->x7, mc_expected->x7);
    drstatecmp_check_gpr_value("x8", tag, mc_instrumented->x8, mc_expected->x8);
    drstatecmp_check_gpr_value("x9", tag, mc_instrumented->x9, mc_expected->x9);
    drstatecmp_check_gpr_value("x10", tag, mc_instrumented->x10, mc_expected->x10);
    drstatecmp_check_gpr_value("x11", tag, mc_instrumented->x11, mc_expected->x11);
    drstatecmp_check_gpr_value("x12", tag, mc_instrumented->x12, mc_expected->x12);
    drstatecmp_check_gpr_value("x13", tag, mc_instrumented->x13, mc_expected->x13);
    drstatecmp_check_gpr_value("x14", tag, mc_instrumented->x14, mc_expected->x14);
    drstatecmp_check_gpr_value("x15", tag, mc_instrumented->x15, mc_expected->x15);
    drstatecmp_check_gpr_value("x16", tag, mc_instrumented->x16, mc_expected->x16);
    drstatecmp_check_gpr_value("x17", tag, mc_instrumented->x17, mc_expected->x17);
    drstatecmp_check_gpr_value("x18", tag, mc_instrumented->x18, mc_expected->x18);
    drstatecmp_check_gpr_value("x19", tag, mc_instrumented->x19, mc_expected->x19);
    drstatecmp_check_gpr_value("x20", tag, mc_instrumented->x20, mc_expected->x20);
    drstatecmp_check_gpr_value("x21", tag, mc_instrumented->x21, mc_expected->x21);
    drstatecmp_check_gpr_value("x22", tag, mc_instrumented->x22, mc_expected->x22);
    drstatecmp_check_gpr_value("x23", tag, mc_instrumented->x23, mc_expected->x23);
    drstatecmp_check_gpr_value("x24", tag, mc_instrumented->x24, mc_expected->x24);
    drstatecmp_check_gpr_value("x25", tag, mc_instrumented->x25, mc_expected->x25);
    drstatecmp_check_gpr_value("x26", tag, mc_instrumented->x26, mc_expected->x26);
    drstatecmp_check_gpr_value("x27", tag, mc_instrumented->x27, mc_expected->x27);
    drstatecmp_check_gpr_value("x28", tag, mc_instrumented->x28, mc_expected->x28);
    drstatecmp_check_gpr_value("x29", tag, mc_instrumented->x29, mc_expected->x29);
    drstatecmp_check_gpr_value("x30", tag, mc_instrumented->x30, mc_expected->x30);
    drstatecmp_check_gpr_value("x31", tag, mc_instrumented->x31, mc_expected->x31);
#else
#    error NYI
#endif

    drstatecmp_check_gpr_value("xsp", tag, mc_instrumented->xsp, mc_expected->xsp);
#ifdef AARCH64
    for (int i = 0; i < MCXT_NUM_SIMD_SVE_SLOTS; i++) {
#else
    for (int i = 0; i < MCXT_NUM_SIMD_SLOTS; i++) {
#endif
        drstatecmp_check_simd_value(tag, &mc_instrumented->simd[i],
                                    &mc_expected->simd[i]);
    }
}

static void
drstatecmp_compare_state_call(int flags, void *tag)
{
    void *drcontext = dr_get_current_drcontext();
    drstatecmp_saved_states_t *pt =
        (drstatecmp_saved_states_t *)drmgr_get_tls_field(drcontext, tls_idx);

    dr_mcontext_t *mc_instrumented = &pt->saved_state_for_cmp;
    dr_mcontext_t mc_expected;
    mc_expected.size = sizeof(mc_expected);
    mc_expected.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mc_expected);

    drstatecmp_check_machine_state(mc_instrumented, &mc_expected, flags, tag);
}

static void
drstatecmp_compare_state(void *drcontext, void *tag, instrlist_t *bb,
                         drstatecmp_user_data_t *data, instr_t *instr)
{
    int flags = 0;
#ifdef AARCHXX
    /* Avoid false positives by not checking LR when it is dead just before the
     * terminating instruction. This is necessary, for example, when the terminating
     * instruction (not re-executed and follows the state comparison) is 'blr'. The 'blr'
     * instruction overwrites the lr register and instrumentation could have clobbered
     * this register earlier if the register was dead at the time of clobbering.
     */
    instr_t *term = instrlist_last_app(bb);
    if (instr_is_cti(term) &&
        !instr_reads_from_reg(term, DR_REG_LR, DR_QUERY_INCLUDE_COND_SRCS) &&
        instr_writes_to_exact_reg(term, DR_REG_LR, DR_QUERY_INCLUDE_COND_SRCS))
        flags |= DRSTATECMP_SKIP_CHECK_LR;
#endif

#ifdef X86
    /* Avoid false positives due to mismatches for undefined effect on flags by
     * some x86 instructions. In DR undefined effect on flags is considered a
     * write to the flags to render the flags dead in more occasions and thus
     * allow for less saving/restoration. However, drstatecmp may detect mismatches on
     * those cases.
     * XXX: limit this constraint to only cases with partial overwriting of flags
     * and undefined behavior instead of all cases of considered-dead-by-DR flags.
     */
    if (data->dead_aflags)
        flags |= DRSTATECMP_SKIP_CHECK_AFLAGS;
#endif

    dr_insert_clean_call(drcontext, bb, instr, (void *)drstatecmp_compare_state_call,
                         false /* fpstate */, 2, OPND_CREATE_INT32(flags),
                         OPND_CREATE_INTPTR(tag));
}

static void
drstatecmp_check_reexecution(void *drcontext, void *tag, instrlist_t *bb,
                             drstatecmp_user_data_t *data,
                             drstatecmp_dup_labels_t *labels)
{
    /* Save state at the beginning of the original bb in order to restore it at the
     * end of it (to enable re-execution of the bb). */
    drstatecmp_save_state(drcontext, bb, labels->orig_bb_start, /*for_cmp=*/false);

    /* Save the state at the end of the original bb (or alternatively before the start of
     * the copy bb) for later comparison and restore the machine state to the state before
     * executing the original bb (allows re-execution).
     */
    drstatecmp_save_state(drcontext, bb, labels->copy_bb_start, /*for_cmp=*/true);
    drstatecmp_restore_state(drcontext, bb, labels->copy_bb_start);

    /* Compare the state at the end of the copy bb (uninstrumented) with the saved state
     * at the end of the original (instrumented) bb to detect clobbering by the
     * instrumentation.
     */
    drstatecmp_compare_state(drcontext, tag, bb, data, labels->term);
}

/* Duplicate the side-effect-free basic block for re-execution and add saving/restoring of
 * machine state and state comparison to check for instrumentation-induced clobbering of
 * machine state.
 */
static void
drstatecmp_postprocess_side_effect_free_bb(void *drcontext, void *tag, instrlist_t *bb,
                                           drstatecmp_user_data_t *data)
{
    drstatecmp_dup_labels_t labels;
    drstatecmp_duplicate_bb(drcontext, bb, data, &labels);
    drstatecmp_check_reexecution(drcontext, tag, bb, data, &labels);
}

static void
drstatecmp_postprocess_bb_with_side_effects(void)
{
    /* TODO i#4678: Add checks for bbs with side effects.  */
}

static dr_emit_flags_t
drstatecmp_post_instru_phase(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                             bool translating, void *user_data)
{
    drstatecmp_user_data_t *data = (drstatecmp_user_data_t *)user_data;

    if (data->side_effect_free_bb) {
        drstatecmp_postprocess_side_effect_free_bb(drcontext, tag, bb, data);
    } else {
        /* Basic blocks with side-effects not handled yet. */
        drstatecmp_postprocess_bb_with_side_effects();
    }

    /* Free the user_data since we are done with this bb. */
    dr_thread_free(drcontext, user_data, sizeof(drstatecmp_user_data_t));

    return DR_EMIT_DEFAULT;
}

/****************************************************************************
 *  THREAD INIT AND EXIT
 */

static void
drstatecmp_thread_init(void *drcontext)
{
    drstatecmp_saved_states_t *pt = (drstatecmp_saved_states_t *)dr_thread_alloc(
        drcontext, sizeof(drstatecmp_saved_states_t));
    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
    memset(pt, 0, sizeof(*pt));
}

static void
drstatecmp_thread_exit(void *drcontext)
{
    drstatecmp_saved_states_t *pt =
        (drstatecmp_saved_states_t *)drmgr_get_tls_field(drcontext, tls_idx);
    ASSERT(pt != NULL, "thread-local storage should not be NULL");
    dr_thread_free(drcontext, pt, sizeof(drstatecmp_saved_states_t));
}

/***************************************************************************
 * INIT AND EXIT
 */

static int drstatecmp_init_count;

drstatecmp_status_t
drstatecmp_init(drstatecmp_options_t *ops_in)
{
    int count = dr_atomic_add32_return_sum(&drstatecmp_init_count, 1);
    if (count != 1)
        return DRSTATECMP_ERROR_ALREADY_INITIALIZED;

    memcpy(&ops, ops_in, sizeof(drstatecmp_options_t));

    drmgr_priority_t priority = { sizeof(drmgr_priority_t),
                                  DRMGR_PRIORITY_NAME_DRSTATECMP, NULL, NULL,
                                  DRMGR_PRIORITY_DRSTATECMP };
    drmgr_init();

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1)
        return DRSTATECMP_ERROR;

    drstatecmp_label_init();

    drmgr_instru_events_t events = { sizeof(events),
                                     drstatecmp_app2app_phase,
                                     drstatecmp_analyze_phase,
                                     drstatecmp_insert_phase,
                                     drstatecmp_instru2instru_phase,
                                     drstatecmp_post_instru_phase };

    if (!drmgr_register_thread_init_event(drstatecmp_thread_init) ||
        !drmgr_register_thread_exit_event(drstatecmp_thread_exit) ||
        !drmgr_register_bb_instrumentation_all_events(&events, &priority))
        return DRSTATECMP_ERROR;

    return DRSTATECMP_SUCCESS;
}

drstatecmp_status_t
drstatecmp_exit(void)
{
    int count = dr_atomic_add32_return_sum(&drstatecmp_init_count, -1);
    if (count != 0)
        return DRSTATECMP_ERROR_NOT_INITIALIZED;

    drmgr_instru_events_t events = { sizeof(events),
                                     drstatecmp_app2app_phase,
                                     drstatecmp_analyze_phase,
                                     drstatecmp_insert_phase,
                                     drstatecmp_instru2instru_phase,
                                     drstatecmp_post_instru_phase };

    if (!drmgr_unregister_thread_init_event(drstatecmp_thread_init) ||
        !drmgr_unregister_thread_exit_event(drstatecmp_thread_exit) ||
        !drmgr_unregister_tls_field(tls_idx) ||
        !drmgr_unregister_bb_instrumentation_all_events(&events))
        return DRSTATECMP_ERROR;

    drmgr_exit();

    return DRSTATECMP_SUCCESS;
}
