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

#include "dr_api.h"
#include "drreg_priv.h"
#include "drreg_aflag.h"
#include "drreg_gpr.h"
#include "drreg_simd.h"
#include "../ext_utils.h"

/***************************************************************************
 * SPILLING AND RESTORING
 */

drreg_status_t
drreg_internal_bb_insert_restore_all(
    void *drcontext, instrlist_t *bb, instr_t *inst, bool force_restore,
    OUT bool *regs_restored _IF_SIMD_SUPPORTED(OUT bool *simd_regs_restored))
{
    drreg_internal_per_thread_t *pt = drreg_internal_get_tls_data(drcontext);

    /* Before each app read, or at end of bb, restore aflags to app value. */
    drreg_status_t res = drreg_internal_bb_insert_aflag_restore_all(drcontext, pt, bb,
                                                                    inst, force_restore);
    if (res != DRREG_SUCCESS)
        return res;

        /* Before each app read, or at end of bb, restore SIMD registers to app values: */
#ifdef SIMD_SUPPORTED
    res = drreg_internal_bb_insert_simd_restore_all(drcontext, pt, bb, inst,
                                                    force_restore, simd_regs_restored);
    if (res != DRREG_SUCCESS)
        return res;
#endif

    /* Before each app read, or at end of bb, restore GPR registers to app values: */
    res = drreg_internal_bb_insert_gpr_restore_all(drcontext, pt, bb, inst, force_restore,
                                                   regs_restored);
    return res;
}

/***************************************************************************
 * ANALYSIS AND CROSS-APP-INSTR
 */

void
count_app_uses(drreg_internal_per_thread_t *pt, opnd_t opnd)
{
    int i;
    for (i = 0; i < opnd_num_regs_used(opnd); i++) {
        reg_id_t reg = opnd_get_reg_used(opnd, i);
        if (reg_is_gpr(reg))
            drreg_internal_increment_app_gpr_use_count(pt, opnd, reg);
#ifdef SIMD_SUPPORTED
        else if (reg_is_vector_simd(reg))
            drreg_internal_increment_app_simd_use_count(pt, opnd, reg);
#endif
    }
}

/***************************************************************************
 * USE OUTSIDE INSERT PHASE
 */

drreg_status_t
drreg_forward_analysis(void *drcontext, instr_t *start)
{
    drreg_internal_per_thread_t *pt = drreg_internal_get_tls_data(drcontext);
    ptr_uint_t aflags_cur = 0;

    /* Initialise and prepare for forward analysis. */
    drreg_internal_init_forward_gpr_liveness_analysis(pt);
#ifdef SIMD_SUPPORTED
    drreg_internal_init_forward_simd_liveness_analysis(pt);
#endif

    /* Note, our analysis also has to consider meta instructions as well. */
    for (instr_t *inst = start; inst != NULL; inst = instr_get_next(inst)) {
        if (drreg_internal_is_xfer(inst))
            break;

        drreg_internal_forward_analyse_gpr_liveness(pt, inst);
#ifdef SIMD_SUPPORTED
        drreg_internal_forward_analyse_simd_liveness(pt, inst);
#endif
        drreg_internal_forward_analyse_aflag_liveness(inst, &aflags_cur);

        if (instr_is_app(inst)) {
            int i;
            for (i = 0; i < instr_num_dsts(inst); i++)
                count_app_uses(pt, instr_get_dst(inst, i));
            for (i = 0; i < instr_num_srcs(inst); i++)
                count_app_uses(pt, instr_get_src(inst, i));
        }
    }

    pt->live_idx = 0;

    /* Finalise forward analysis. */
    drreg_internal_finalise_forward_gpr_liveness_analysis(pt);
#ifdef SIMD_SUPPORTED
    drreg_internal_finalise_forward_simd_liveness_analysis(pt);
#endif
    drreg_internal_finalise_forward_aflag_liveness_analysis(pt, aflags_cur);

    return DRREG_SUCCESS;
}

/***************************************************************************
 * REGISTER RESERVATION
 */

drreg_status_t
drreg_internal_reserve(void *drcontext, drreg_spill_class_t spill_class,
                       instrlist_t *ilist, instr_t *where, drvector_t *reg_allowed,
                       bool only_if_no_spill, OUT reg_id_t *reg_out)
{
    drreg_internal_per_thread_t *pt = drreg_internal_get_tls_data(drcontext);

    if (reg_out == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;

    if (spill_class == DRREG_GPR_SPILL_CLASS) {
        return drreg_internal_reserve_gpr(drcontext, pt, ilist, where, reg_allowed,
                                          only_if_no_spill, reg_out);
    }
#ifdef SIMD_SUPPORTED
    else if (spill_class == DRREG_SIMD_XMM_SPILL_CLASS ||
             spill_class == DRREG_SIMD_YMM_SPILL_CLASS ||
             spill_class == DRREG_SIMD_ZMM_SPILL_CLASS) {
        return drreg_internal_reserve_simd_reg(drcontext, pt, spill_class, ilist, where,
                                               reg_allowed, only_if_no_spill, reg_out);
    }
#else
    /* FIXME i#3844: NYI on ARM */
#endif
    else {
        /* The caller should have catched this and return an error or invalid parameter
         * error.
         */
        ASSERT(false, "internal error: invalid spill class");
    }
    return DRREG_ERROR;
}

/***************************************************************************
 * HELPER FUNCTIONS
 */

bool
drreg_internal_is_xfer(instr_t *inst)
{
    /* Return true if the passed instruction is control transferring. */
    return instr_is_cti(inst) || instr_is_interrupt(inst) || instr_is_syscall(inst);
}

#ifdef DEBUG
app_pc
get_where_app_pc(instr_t *where)
{
    if (where == NULL)
        return NULL;
    return instr_get_app_pc(where);
}
#endif

drreg_internal_per_thread_t *
drreg_internal_get_tls_data(void *drcontext)
{
    drreg_internal_per_thread_t *pt =
        (drreg_internal_per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    /* Support use during init (i#2910). */
    if (pt == NULL)
        return &drreg_internal_init_pt;
    return pt;
}

void
drreg_internal_report_error(drreg_status_t res, const char *msg)
{
    if (drreg_internal_ops.error_callback != NULL) {
        if ((*drreg_internal_ops.error_callback)(res))
            return;
    }
    ASSERT(false, msg);
    DISPLAY_ERROR(msg);
    dr_abort();
}
