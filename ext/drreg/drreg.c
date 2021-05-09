/* **********************************************************
 * Copyright (c) 2013-2021 Google, Inc.   All rights reserved.
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

/* DynamoRIO Register Management Extension: a mediator for
 * selecting, preserving, and using registers among multiple
 * instrumentation components.
 */

/* XXX i#511: currently the whole interface is tied to drmgr.
 * Should we also provide an interface that works on standalone instrlists?
 * Distinguish by name, "drregi_*" or sthg.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "drvector.h"
#include "drreg.h"
#include "drreg_priv.h"
#include "drreg_gpr.h"
#include "drreg_simd.h"
#include "drreg_aflag.h"
#include "../ext_utils.h"
#include <string.h>
#include <limits.h>
#include <stddef.h> /* offsetof */

/* We use this in drreg_internal_per_thread_t.slot_use[] and other places */
#define DR_REG_EFLAGS DR_REG_INVALID

drreg_internal_per_thread_t drreg_internal_init_pt;

drreg_options_t drreg_internal_ops;

int tls_idx = -1;

// Offset for SIMD slots.
uint drreg_internal_tls_simd_offs = 0;
// Offset for GPR slots.
uint drreg_internal_tls_slot_offs = 0;
reg_id_t drreg_internal_tls_seg = DR_REG_NULL;

#ifdef DEBUG
static uint stats_max_slot;
#endif

/***************************************************************************
 * SPILLING AND RESTORING
 */

drreg_status_t
drreg_max_slots_used(OUT uint *max)
{
#ifdef DEBUG
    if (max == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;
    *max = stats_max_slot;
    return DRREG_SUCCESS;
#else
    return DRREG_ERROR_FEATURE_NOT_AVAILABLE;
#endif
}

/***************************************************************************
 * ANALYSIS AND CROSS-APP-INSTR
 */

/* This event has to go last, to handle labels inserted by other components:
 * else our indices get off, and we can't simply skip labels in the
 * per-instr event b/c we need the liveness to advance at the label
 * but not after the label.
 */
static dr_emit_flags_t
drreg_event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                        bool translating, OUT void **user_data)
{
    drreg_internal_per_thread_t *pt = drreg_internal_get_tls_data(drcontext);
    uint index = 0;

    drreg_internal_bb_init_gpr_liveness_analysis(pt);
#ifdef SIMD_SUPPORTED
    drreg_internal_bb_init_simd_liveness_analysis(pt);
#endif

    /* pt->bb_props is set to 0 at thread init and after each bb. */
    pt->bb_has_internal_flow = false;

    /* Reverse scan is more efficient. This means our indices are also reversed. */
    for (instr_t *inst = instrlist_last(bb); inst != NULL; inst = instr_get_prev(inst)) {
        /* We consider both meta and app instrs, to handle rare cases of meta instrs
         * being inserted during app2app for corner cases. An example are app2app
         * emulation functions like drx_expand_scatter_gather().
         */
        if (!pt->bb_has_internal_flow && (instr_is_ubr(inst) || instr_is_cbr(inst)) &&
            opnd_is_instr(instr_get_target(inst))) {
            /* i#1954: We disable some opts in the presence of control flow. */
            pt->bb_has_internal_flow = true;
            LOG(drcontext, DR_LOG_ALL, 2,
                "%s @%d." PFX ": disabling lazy restores due to intra-bb control flow\n",
                __FUNCTION__, index, get_where_app_pc(inst));
        }

        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ":", __FUNCTION__, index,
            get_where_app_pc(inst));

        /* Liveness Analysis */
        drreg_internal_bb_analyse_gpr_liveness(drcontext, pt, inst, index);
#ifdef SIMD_SUPPORTED
        drreg_internal_bb_analyse_simd_liveness(drcontext, pt, inst, index);
#endif
        drreg_internal_bb_analyse_aflag_liveness(drcontext, pt, inst, index);

        /* Keep track of the register app uses as a heuristic for selection. */
        if (instr_is_app(inst)) {
            int i;
            for (i = 0; i < instr_num_dsts(inst); i++)
                count_app_uses(pt, instr_get_dst(inst, i));
            for (i = 0; i < instr_num_srcs(inst); i++)
                count_app_uses(pt, instr_get_src(inst, i));
        }
        index++;
    }

    pt->live_idx = index;

    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
drreg_event_bb_insert_early(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                            bool for_trace, bool translating, void *user_data)
{
    drreg_internal_per_thread_t *pt = drreg_internal_get_tls_data(drcontext);
    pt->cur_instr = inst;
    pt->live_idx--; /* counts backward */
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
drreg_event_bb_insert_late(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                           bool for_trace, bool translating, void *user_data)
{
    drreg_internal_per_thread_t *pt = drreg_internal_get_tls_data(drcontext);
    drreg_status_t res;
    dr_pred_type_t pred = instrlist_get_auto_predicate(bb);

    /* Buffers used to keep track of which registers are restored for app read
     * and need to be set again tool data.
     */
    bool restored_for_read[DR_NUM_GPR_REGS];
#ifdef SIMD_SUPPORTED
    bool restored_for_simd_read[DR_NUM_SIMD_VECTOR_REGS];
#endif

    /* XXX i#2585: drreg should predicate spills and restores as appropriate. */
    instrlist_set_auto_predicate(bb, DR_PRED_NONE);
    /* For unreserved regs still spilled, we lazily do the restore here.  We also
     * update reserved regs wrt app uses.
     * The instruction list presented to us here are app instrs but may contain meta
     * instrs if any were inserted in app2app. Any such meta instr here will be treated
     * like an app instr.
     */
    bool do_last_spill = drmgr_is_last_instr(drcontext, inst) &&
        !TEST(DRREG_USER_RESTORES_AT_BB_END, pt->bb_props);
    res = drreg_internal_bb_insert_restore_all(
        drcontext, bb, inst,
        do_last_spill /* dictates whether to perform a full restore. */,
        restored_for_read _IF_SIMD_SUPPORTED(restored_for_simd_read));
    if (res != DRREG_SUCCESS)
        drreg_internal_report_error(res, "failed to restore for reads");

    /* After aflags write by app, update spilled app value. */
    res = drreg_internal_insert_aflag_update_spill(drcontext, pt, bb, inst);
    if (res != DRREG_SUCCESS)
        drreg_internal_report_error(res, "failed to spill aflags after app write");

        /* After each app write, update spilled app values: */
#ifdef SIMD_SUPPORTED
    res = drreg_internal_bb_insert_simd_update_spill(drcontext, pt, bb, inst,
                                                     restored_for_simd_read);
    if (res != DRREG_SUCCESS)
        drreg_internal_report_error(res, "slot release on app write failed");
#endif
    res = drreg_internal_insert_gpr_update_spill(drcontext, pt, bb, inst,
                                                 restored_for_read);
    if (res != DRREG_SUCCESS)
        drreg_internal_report_error(res, "slot release on app write failed");

    /* Recall, user may call drreg_set_bb_properties() during instrumentation stages.
     * Refresh the flags back to 0 so they are not erronoeusly considered when
     * instrumenting the next basic block.
     */
    if (drmgr_is_last_instr(drcontext, inst))
        pt->bb_props = 0;

#ifdef DEBUG
    reg_id_t reg;
    if (drmgr_is_last_instr(drcontext, inst)) {
        uint i;
        for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
            ASSERT(!pt->aflags.in_use, "user failed to unreserve aflags");
            ASSERT(pt->aflags.native, "user failed to unreserve aflags");
            ASSERT(!pt->reg[GPR_IDX(reg)].in_use, "user failed to unreserve a register");
            ASSERT(pt->reg[GPR_IDX(reg)].native, "user failed to unreserve a register");
        }
        for (i = 0; i < MAX_SPILLS; i++) {
            ASSERT(pt->slot_use[i] == DR_REG_NULL, "user failed to unreserve a register");
        }
        for (i = 0; i < MAX_SIMD_SPILLS; i++) {
            ASSERT(pt->simd_slot_use[i] == DR_REG_NULL,
                   "user failed to unreserve a register");
        }
    }
#endif
    instrlist_set_auto_predicate(bb, pred);
    return DR_EMIT_DEFAULT;
}

drreg_status_t
drreg_restore_all(void *drcontext, instrlist_t *bb, instr_t *where)
{
    return drreg_internal_bb_insert_restore_all(
        drcontext, bb, where, true /* force restoration */,
        NULL _IF_SIMD_SUPPORTED(NULL) /* do not need to track reg restores */);
}

/***************************************************************************
 * REGISTER RESERVATION
 */

drreg_status_t
drreg_init_and_fill_vector_ex(drvector_t *vec, drreg_spill_class_t spill_class,
                              bool allowed)
{
    if (vec == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;

    if (spill_class == DRREG_GPR_SPILL_CLASS)
        return drreg_internal_init_and_fill_gpr_vector(vec, allowed);
    else if (spill_class == DRREG_SIMD_XMM_SPILL_CLASS) {
#ifdef X86
        return drreg_internal_init_and_fill_simd_vector(vec, allowed);
#else
        return DRREG_ERROR_INVALID_PARAMETER;
#endif
    } else if (spill_class == DRREG_SIMD_YMM_SPILL_CLASS ||
               spill_class == DRREG_SIMD_ZMM_SPILL_CLASS) {
#ifdef X86
        /* TODO i#3844: support on x86. */
        return DRREG_ERROR_FEATURE_NOT_AVAILABLE;
#else
        return DRREG_ERROR_INVALID_PARAMETER;
#endif
    }

    return DRREG_ERROR;
}

drreg_status_t
drreg_init_and_fill_vector(drvector_t *vec, bool allowed)
{
    return drreg_internal_init_and_fill_gpr_vector(vec, allowed);
}

drreg_status_t
drreg_set_vector_entry(drvector_t *vec, reg_id_t reg, bool allowed)
{
    if (vec == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;

    if (reg_is_gpr(reg))
        return drreg_internal_set_gpr_vector_entry(vec, reg, allowed);
#ifdef SIMD_SUPPORTED
    else if (reg_is_vector_simd(reg)) {
        return drreg_internal_set_simd_vector_entry(vec, reg, allowed);
    }
#endif

    return DRREG_ERROR;
}

drreg_status_t
drreg_reserve_register_ex(void *drcontext, drreg_spill_class_t spill_class,
                          instrlist_t *ilist, instr_t *where, drvector_t *reg_allowed,
                          OUT reg_id_t *reg_out)
{
    dr_pred_type_t pred = instrlist_get_auto_predicate(ilist);
    drreg_status_t res;
#ifdef ARM
    if (spill_class == DRREG_SIMD_XMM_SPILL_CLASS)
        return DRREG_ERROR_INVALID_PARAMETER;
#endif
    if (spill_class == DRREG_SIMD_YMM_SPILL_CLASS ||
        spill_class == DRREG_SIMD_ZMM_SPILL_CLASS) {
#ifdef X86
        /* TODO i#3844: support on x86. */
        return DRREG_ERROR_FEATURE_NOT_AVAILABLE;
#else
        return DRREG_ERROR_INVALID_PARAMETER;
#endif
    }
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        res = drreg_forward_analysis(drcontext, where);
        if (res != DRREG_SUCCESS)
            return res;
    }
    /* FIXME i#3827: ever_spilled is not being reset. */
    /* XXX i#2585: drreg should predicate spills and restores as appropriate */
    instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
    res = drreg_internal_reserve(drcontext, spill_class, ilist, where, reg_allowed, false,
                                 reg_out);
    instrlist_set_auto_predicate(ilist, pred);
    return res;
}

drreg_status_t
drreg_reserve_register(void *drcontext, instrlist_t *ilist, instr_t *where,
                       drvector_t *reg_allowed, OUT reg_id_t *reg_out)
{
    return drreg_reserve_register_ex(drcontext, DRREG_GPR_SPILL_CLASS, ilist, where,
                                     reg_allowed, reg_out);
}

drreg_status_t
drreg_reserve_dead_register(void *drcontext, instrlist_t *ilist, instr_t *where,
                            drvector_t *reg_allowed, OUT reg_id_t *reg_out)
{
    return drreg_reserve_dead_register_ex(drcontext, DRREG_GPR_SPILL_CLASS, ilist, where,
                                          reg_allowed, reg_out);
}

drreg_status_t
drreg_reserve_dead_register_ex(void *drcontext, drreg_spill_class_t spill_class,
                               instrlist_t *ilist, instr_t *where,
                               drvector_t *reg_allowed, OUT reg_id_t *reg_out)
{
    dr_pred_type_t pred = instrlist_get_auto_predicate(ilist);
    drreg_status_t res;
#ifdef ARM
    if (spill_class == DRREG_SIMD_XMM_SPILL_CLASS ||
        spill_class == DRREG_SIMD_YMM_SPILL_CLASS ||
        spill_class == DRREG_SIMD_ZMM_SPILL_CLASS)
        return DRREG_ERROR_INVALID_PARAMETER;
#endif

    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        res = drreg_forward_analysis(drcontext, where);
        if (res != DRREG_SUCCESS)
            return res;
    }

    /* XXX i#2585: drreg should predicate spills and restores as appropriate. */
    instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
    res = drreg_internal_reserve(drcontext, spill_class, ilist, where, reg_allowed, true,
                                 reg_out);
    instrlist_set_auto_predicate(ilist, pred);

    if (res != DRREG_SUCCESS)
        return res;

    return res;
}

static drreg_status_t
drreg_restore_app_value(void *drcontext, instrlist_t *ilist, instr_t *where,
                        reg_id_t app_reg, reg_id_t dst_reg, bool stateful)
{
    drreg_internal_per_thread_t *pt = drreg_internal_get_tls_data(drcontext);
    dr_pred_type_t pred = instrlist_get_auto_predicate(ilist);
    drreg_status_t res;

    /* XXX i#2585: drreg should predicate spills and restores as appropriate */
    instrlist_set_auto_predicate(ilist, DR_PRED_NONE);

    if (reg_is_gpr(app_reg)) {
        res = drreg_internal_restore_gpr_app_value(drcontext, pt, ilist, where, app_reg,
                                                   dst_reg, stateful);
    }
#ifdef SIMD_SUPPORTED
    else if (reg_is_vector_simd(app_reg)) {
        res = drreg_internal_restore_simd_app_value(drcontext, pt, ilist, where, app_reg,
                                                    dst_reg, stateful);
    }
#endif
    else {
        res = DRREG_ERROR_INVALID_PARAMETER;
    }
    instrlist_set_auto_predicate(ilist, pred);
    return res;
}

drreg_status_t
drreg_get_app_value(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t app_reg,
                    reg_id_t dst_reg)
{
    return drreg_restore_app_value(drcontext, ilist, where, app_reg, dst_reg, true);
}

drreg_status_t
drreg_restore_app_values(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t opnd,
                         INOUT reg_id_t *swap)
{
    drreg_status_t res;
    bool no_app_value = false;
    dr_pred_type_t pred = instrlist_get_auto_predicate(ilist);

    /* XXX i#2585: drreg should predicate spills and restores as appropriate */
    instrlist_set_auto_predicate(ilist, DR_PRED_NONE);

#ifdef SIMD_SUPPORTED
    /* First, restore SIMD registers. */
    res = drreg_internal_restore_simd_app_values(drcontext, ilist, where, opnd,
                                                 &no_app_value);
    if (res != DRREG_SUCCESS) {
        instrlist_set_auto_predicate(ilist, pred);
        return res;
    }
#endif

    /* After SIMD, then restore GPRs. */
    res = drreg_internal_restore_gpr_app_values(drcontext, ilist, where, opnd, swap,
                                                &no_app_value);
    if (res != DRREG_SUCCESS) {
        instrlist_set_auto_predicate(ilist, pred);
        return res;
    }

    instrlist_set_auto_predicate(ilist, pred);
    return (no_app_value ? DRREG_ERROR_NO_APP_VALUE : DRREG_SUCCESS);
}

drreg_status_t
drreg_statelessly_restore_app_value(void *drcontext, instrlist_t *ilist, reg_id_t reg,
                                    instr_t *where_restore, instr_t *where_respill,
                                    bool *restore_needed OUT, bool *respill_needed OUT)
{
    drreg_internal_per_thread_t *pt = drreg_internal_get_tls_data(drcontext);
    drreg_status_t res;
    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX " %s\n", __FUNCTION__, pt->live_idx,
        get_where_app_pc(where_restore), get_register_name(reg));
    if (where_restore == NULL || where_respill == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;
    if (reg == DR_REG_NULL) {
        res = drreg_internal_restore_aflags(drcontext, pt, ilist, where_restore, false);
    } else {
        if (reg_is_gpr(reg) && (!reg_is_pointer_sized(reg) || reg == dr_get_stolen_reg()))
            return DRREG_ERROR_INVALID_PARAMETER;
        /* Note, we reach here for both GPR and SIMD registers. */
        res = drreg_restore_app_value(drcontext, ilist, where_restore, reg, reg, false);
    }
    if (restore_needed != NULL)
        *restore_needed = (res == DRREG_SUCCESS);
    if (res != DRREG_SUCCESS && res != DRREG_ERROR_NO_APP_VALUE)
        return res;

    /* We now handle respills. */
    if (drreg_internal_aflag_handle_respill_for_statelessly_restore(drcontext, pt, ilist,
                                                                    where_respill, reg)) {
        if (respill_needed != NULL)
            *respill_needed = true;
    }
    return res;
}

drreg_status_t
drreg_unreserve_register(void *drcontext, instrlist_t *ilist, instr_t *where,
                         reg_id_t reg)
{
    drreg_status_t res = DRREG_SUCCESS;
    drreg_internal_per_thread_t *pt = drreg_internal_get_tls_data(drcontext);

    if (reg_is_gpr(reg))
        res = drreg_internal_unreserve_gpr(drcontext, pt, ilist, where, reg);
#ifdef SIMD_SUPPORTED
    else if (reg_is_vector_simd(reg))
        res = drreg_internal_unreserve_simd_reg(drcontext, pt, ilist, where, reg);
#endif
    else {
        ASSERT(false, "internal error: not applicable register");
        res = DRREG_ERROR_INVALID_PARAMETER;
    }
    return res;
}

drreg_status_t
drreg_reservation_info(void *drcontext, reg_id_t reg, opnd_t *opnd OUT,
                       bool *is_dr_slot OUT, uint *tls_offs OUT)
{
    drreg_reserve_info_t info = {
        sizeof(info),
    };
    drreg_internal_per_thread_t *pt = drreg_internal_get_tls_data(drcontext);
    drreg_status_t res;
    if ((reg < DR_REG_START_GPR || reg > DR_REG_STOP_GPR ||
         !pt->reg[GPR_IDX(reg)].in_use))
        return DRREG_ERROR_INVALID_PARAMETER;
#ifdef SIMD_SUPPORTED
    if (reg_is_vector_simd(reg) && !pt->simd_reg[SIMD_IDX(reg)].in_use)
        return DRREG_ERROR_INVALID_PARAMETER;
#endif
    res = drreg_reservation_info_ex(drcontext, reg, &info);
    if (res != DRREG_SUCCESS)
        return res;
    if (opnd != NULL)
        *opnd = info.opnd;
    if (is_dr_slot != NULL)
        *is_dr_slot = info.is_dr_slot;
    if (tls_offs != NULL)
        *tls_offs = info.tls_offs;
    return DRREG_SUCCESS;
}

static void
set_reservation_info(drreg_reserve_info_t *info, drreg_internal_per_thread_t *pt,
                     void *drcontext, reg_id_t reg, drreg_internal_reg_info_t *reg_info)
{
    info->reserved = reg_info->in_use;
    info->holds_app_value = reg_info->native;
    if (reg_info->native) {
        info->app_value_retained = false;
        info->opnd = opnd_create_null();
        info->is_dr_slot = false;
        info->tls_offs = -1;
    } else if (reg_info->xchg != DR_REG_NULL) {
        info->app_value_retained = true;
        info->opnd = opnd_create_reg(reg_info->xchg);
        info->is_dr_slot = false;
        info->tls_offs = -1;
    } else {
        info->app_value_retained = reg_info->ever_spilled;
        uint slot = reg_info->slot;
#ifdef DEBUG
        if (reg == DR_REG_NULL)
            ASSERT(slot == AFLAGS_SLOT, "slot should be aflags");
#endif
        if ((reg == DR_REG_NULL && !reg_info->native &&
             pt->slot_use[slot] != DR_REG_NULL) ||
            (reg_is_gpr(reg) && pt->slot_use[slot] == reg)) {
            if (slot < drreg_internal_ops.num_spill_slots) {
                info->opnd = dr_raw_tls_opnd(drcontext, drreg_internal_tls_seg,
                                             drreg_internal_tls_slot_offs);
                info->is_dr_slot = false;
                info->tls_offs = drreg_internal_tls_slot_offs + slot * sizeof(reg_t);
            } else {
                dr_spill_slot_t DR_slot =
                    (dr_spill_slot_t)(slot - drreg_internal_ops.num_spill_slots);
                if (DR_slot < dr_max_opnd_accessible_spill_slot())
                    info->opnd = dr_reg_spill_slot_opnd(drcontext, DR_slot);
                else {
                    /* Multi-step so no single opnd */
                    info->opnd = opnd_create_null();
                }
                info->is_dr_slot = true;
                info->tls_offs = DR_slot;
            }
        } else {
            /* Note, we reach here also for SIMD vector regs. */
            info->opnd = opnd_create_null();
            info->is_dr_slot = false;
            info->tls_offs = -1;
        }
    }
}

drreg_status_t
drreg_reservation_info_ex(void *drcontext, reg_id_t reg, drreg_reserve_info_t *info OUT)
{
    drreg_internal_per_thread_t *pt;
    drreg_internal_reg_info_t *reg_info;

    if (info == NULL || info->size != sizeof(drreg_reserve_info_t))
        return DRREG_ERROR_INVALID_PARAMETER;

    pt = drreg_internal_get_tls_data(drcontext);

    if (reg == DR_REG_NULL) {
        reg_info = &pt->aflags;
#ifdef SIMD_SUPPORTED
    } else if (reg_is_vector_simd(reg)) {
        reg_info = &pt->simd_reg[SIMD_IDX(reg)];
#endif
    } else if (reg >= DR_REG_START_GPR && reg <= DR_REG_STOP_GPR) {
        reg_info = &pt->reg[GPR_IDX(reg)];
    } else {
        return DRREG_ERROR_INVALID_PARAMETER;
    }
    set_reservation_info(info, pt, drcontext, reg, reg_info);
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_is_register_dead(void *drcontext, reg_id_t reg, instr_t *inst, bool *dead)
{
    drreg_status_t res;

    drreg_internal_per_thread_t *pt = drreg_internal_get_tls_data(drcontext);
    if (dead == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        res = drreg_forward_analysis(drcontext, inst);
        if (res != DRREG_SUCCESS)
            return res;
        ASSERT(pt->live_idx == 0, "non-drmgr-insert always uses 0 index");
    }

    if (reg_is_gpr(reg))
        return drreg_internal_is_gpr_dead(pt, reg, dead);
#ifdef SIMD_SUPPORTED
    else if (reg_is_vector_simd(reg)) {
        drreg_spill_class_t spill_class;
        if (reg_is_strictly_xmm(reg))
            spill_class = DRREG_SIMD_XMM_SPILL_CLASS;
        else if (reg_is_strictly_ymm(reg))
            spill_class = DRREG_SIMD_YMM_SPILL_CLASS;
        else if (reg_is_strictly_zmm(reg))
            spill_class = DRREG_SIMD_ZMM_SPILL_CLASS;
        else
            return DRREG_ERROR;

        return drreg_internal_is_simd_reg_dead(pt, spill_class, reg, dead);
    }
#endif
    else
        return DRREG_ERROR;

    return DRREG_SUCCESS;
}

drreg_status_t
drreg_set_bb_properties(void *drcontext, drreg_bb_properties_t flags)
{
    drreg_internal_per_thread_t *pt = drreg_internal_get_tls_data(drcontext);
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_APP2APP &&
        drmgr_current_bb_phase(drcontext) == DRMGR_PHASE_ANALYSIS &&
        drmgr_current_bb_phase(drcontext) == DRMGR_PHASE_INSERTION)
        return DRREG_ERROR_FEATURE_NOT_AVAILABLE;
    /* XXX: interactions with multiple callers gets messy...for now we just or-in */
    pt->bb_props |= flags;
    LOG(drcontext, DR_LOG_ALL, 2, "%s: bb flags are now 0x%x\n", __FUNCTION__,
        pt->bb_props);
    return DRREG_SUCCESS;
}

/***************************************************************************
 * RESTORE STATE
 */

static bool
drreg_internal_is_our_spill_or_restore(void *drcontext, instr_t *instr, bool *spill OUT,
                                       reg_id_t *reg_spilled OUT, uint *slot_out OUT,
                                       uint *offs_out OUT,
                                       bool *is_indirectly_spilled OUT)
{
    bool tls;
    uint slot, offs;
    reg_id_t reg;
    /* Flag denoting spill or restore. */
    bool is_spilled;
    /* Flag denoting direct or indirect access. */
    bool is_indirect = false;

    if (is_indirectly_spilled != NULL)
        *is_indirectly_spilled = is_indirect;

    if (!instr_is_reg_spill_or_restore(drcontext, instr, &tls, &is_spilled, &reg, &offs))
        return false;

    /* Checks whether this from our direct raw TLS for gpr registers. */
    if (!drreg_internal_is_gpr_spill_or_restore(drcontext, instr, tls, offs, &slot)) {
#ifdef SIMD_SUPPORTED
        if (drreg_internal_is_simd_spill_or_restore(drcontext, instr, tls, offs,
                                                    &is_spilled, &reg, &slot)) {
            is_indirect = true;
        } else
#endif
            return false; /* not a drreg spill/restore */
    }

    if (spill != NULL)
        *spill = is_spilled;
    if (reg_spilled != NULL)
        *reg_spilled = reg;
    if (slot_out != NULL)
        *slot_out = slot;
    if (offs_out != NULL)
        *offs_out = offs;
    if (is_indirectly_spilled != NULL)
        *is_indirectly_spilled = is_indirect;
    return true;
}

drreg_status_t
drreg_is_instr_spill_or_restore(void *drcontext, instr_t *instr, bool *spill OUT,
                                bool *restore OUT, reg_id_t *reg_spilled OUT)
{
    bool is_spill;
    if (drreg_internal_is_our_spill_or_restore(drcontext, instr, &is_spill, reg_spilled,
                                               NULL, NULL, NULL)) {
        if (spill != NULL)
            *spill = is_spill;
        if (restore != NULL)
            *restore = !is_spill;
    } else {
        if (spill != NULL)
            *spill = false;
        if (restore != NULL)
            *restore = false;
        if (reg_spilled != NULL)
            *reg_spilled = DR_REG_NULL;
    }

    return DRREG_SUCCESS;
}

static bool
drreg_event_restore_state(void *drcontext, bool restore_memory,
                          dr_restore_state_info_t *info)
{
    drreg_internal_per_thread_t *pt = drreg_internal_get_tls_data(drcontext);

    /* To achieve a clean and simple reserve-and-unreserve interface w/o specifying
     * up front how many cross-app-instr scratch regs (and then limited to whole-bb
     * regs with stored per-bb info, like Dr. Memory does), we have to pay with a
     * complex state xl8 scheme.  We need to decode the in-cache fragment and walk
     * it, recognizing our own spills and restores.  We distinguish a tool value
     * spill to a temp slot (from drreg_event_bb_insert_late()) by watching for
     * a spill of an already-spilled reg to a different slot.
     */
    uint spilled_to[DR_NUM_GPR_REGS];
    uint spilled_to_aflags = MAX_SPILLS;
#ifdef SIMD_SUPPORTED
    uint spilled_simd_to[DR_NUM_SIMD_VECTOR_REGS];
    reg_id_t simd_slot_use[MAX_SIMD_SPILLS];
#endif
    reg_id_t reg;
    instr_t inst;
    instr_t next_inst; /* used to analyse the load to an indirect block. */
    byte *prev_pc, *pc = info->fragment_info.cache_start_pc;
    uint offs;
    bool is_spill;
    bool is_indirect;
#ifdef X86
    bool prev_xax_spill = false;
    bool aflags_in_xax = false;
#endif
    uint slot;
    if (pc == NULL)
        return true; /* fault not in cache */

    drreg_internal_gpr_restore_state_init(spilled_to);
#ifdef SIMD_SUPPORTED
    drreg_internal_simd_restore_state_init(spilled_simd_to, simd_slot_use);
#endif

    LOG(drcontext, DR_LOG_ALL, 3,
        "%s: processing fault @" PFX ": decoding from " PFX "\n", __FUNCTION__,
        info->raw_mcontext->pc, pc);
    instr_init(drcontext, &inst);
    instr_init(drcontext, &next_inst);
    while (pc < info->raw_mcontext->pc) {
        instr_reset(drcontext, &inst);
        instr_reset(drcontext, &next_inst);
        prev_pc = pc;
        pc = decode(drcontext, pc, &inst);
        if (pc == NULL) {
            LOG(drcontext, DR_LOG_ALL, 3, "%s @" PFX " \n", __FUNCTION__, prev_pc,
                "PC decoding returned NULL during state restoration");
            instr_free(drcontext, &inst);
            instr_free(drcontext, &next_inst);
            return true;
        }
        if (decode(drcontext, pc, &next_inst) == NULL) {
            LOG(drcontext, DR_LOG_ALL, 3, "%s @" PFX " \n", __FUNCTION__, prev_pc,
                "PC decoding returned NULL during state restoration");
            instr_free(drcontext, &inst);
            instr_free(drcontext, &next_inst);
            return true;
        }
        if (drreg_internal_is_our_spill_or_restore(drcontext, &inst, &is_spill, &reg,
                                                   &slot, &offs, &is_indirect)) {
            LOG(drcontext, DR_LOG_ALL, 3,
                "%s @" PFX " found %s to %s offs=0x%x => slot %d\n", __FUNCTION__,
                prev_pc, is_spill ? "is_spill" : "restore", get_register_name(reg), offs,
                slot);
            if (is_spill) {
                if (slot == AFLAGS_SLOT) {
                    drreg_internal_aflag_restore_state_handle_spill(drcontext, pc, slot,
                                                                    &spilled_to_aflags);
#ifdef SIMD_SUPPORTED
                } else if (is_indirect) {
                    drreg_internal_simd_restore_state_handle_spill(
                        drcontext, pc, slot, reg, spilled_simd_to, simd_slot_use);
#endif
                } else {
                    drreg_internal_gpr_restore_state_handle_spill(drcontext, pc, slot,
                                                                  reg, spilled_to);
                }
            } else {
                /* Not a spill, but a restore. */
                if (slot == AFLAGS_SLOT && spilled_to_aflags == slot) {
                    drreg_internal_aflag_restore_state_handle_restore(drcontext, pc, slot,
                                                                      &spilled_to_aflags);
#ifdef SIMD_SUPPORTED
                } else if (is_indirect) {
                    drreg_internal_simd_restore_state_handle_restore(
                        drcontext, pc, slot, reg, spilled_simd_to, simd_slot_use);
#endif
                } else {
                    drreg_internal_gpr_restore_state_handle_restore(drcontext, pc, slot,
                                                                    reg, spilled_to);
                }
            }
            /* SNIFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFf */
#ifdef X86
            if (reg == DR_REG_XAX) {
                prev_xax_spill = true;
                if (aflags_in_xax)
                    aflags_in_xax = false;
            }
#endif
        }
#ifdef X86
        else if (prev_xax_spill && instr_get_opcode(&inst) == OP_lahf && is_spill)
            aflags_in_xax = true;
        else if (aflags_in_xax && instr_get_opcode(&inst) == OP_sahf)
            aflags_in_xax = false;
#endif
    }
    instr_free(drcontext, &inst);
    instr_free(drcontext, &next_inst);

    drreg_internal_aflag_restore_state_set_value(
        drcontext, info, spilled_to_aflags _IF_X86(aflags_in_xax));
    drreg_internal_gpr_restore_state_set_values(drcontext, info, spilled_to);
#ifdef SIMD_SUPPORTED
    drreg_internal_simd_restore_state_set_values(drcontext, pt, info, spilled_simd_to,
                                                 simd_slot_use);
#endif
    return true;
}

/***************************************************************************
 * INIT AND EXIT
 */

static int drreg_init_count;

static void
tls_data_init(void *drcontext, drreg_internal_per_thread_t *pt)
{
    memset(pt, 0, sizeof(*pt));

    drreg_internal_tls_gpr_data_init(pt);
#ifdef SIMD_SUPPORTED
    drreg_internal_tls_simd_data_init(drcontext, pt);
#endif
    drreg_internal_tls_aflag_data_init(pt);
}

static void
tls_data_free(void *drcontext, drreg_internal_per_thread_t *pt)
{
    drreg_internal_tls_gpr_data_free(pt);
#ifdef SIMD_SUPPORTED
    drreg_internal_tls_simd_data_free(drcontext, pt);
#endif
    drreg_internal_tls_aflag_data_free(pt);
}

static void
drreg_thread_init(void *drcontext)
{
    drreg_internal_per_thread_t *pt =
        (drreg_internal_per_thread_t *)dr_thread_alloc(drcontext, sizeof(*pt));
    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
    tls_data_init(drcontext, pt);
    pt->tls_seg_base = dr_get_dr_segment_base(drreg_internal_tls_seg);
    /* Place the pointer to the SIMD block inside a hidden slot.
     * XXX: We could get API to access raw TLS slots like this.
     */
    void **addr = (void **)(pt->tls_seg_base + drreg_internal_tls_simd_offs);
    *addr = pt->simd_spills;
}

static void
drreg_thread_exit(void *drcontext)
{
    drreg_internal_per_thread_t *pt =
        (drreg_internal_per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    tls_data_free(drcontext, pt);
    dr_thread_free(drcontext, pt, sizeof(*pt));
}

static uint
get_updated_num_slots(bool do_not_sum_slots, uint cur_slots, uint new_slots)
{
    if (do_not_sum_slots) {
        if (new_slots > cur_slots)
            return new_slots;
        else
            return cur_slots;
    } else
        return cur_slots + new_slots;
}

drreg_status_t
drreg_init(drreg_options_t *ops_in)
{
    uint prior_slots = drreg_internal_ops.num_spill_slots;
#ifdef SIMD_SUPPORTED
    uint prior_simd_slots = drreg_internal_ops.num_spill_simd_slots;
#endif

    drmgr_priority_t high_priority = { sizeof(high_priority),
                                       DRMGR_PRIORITY_NAME_DRREG_HIGH, NULL, NULL,
                                       DRMGR_PRIORITY_INSERT_DRREG_HIGH };
    drmgr_priority_t low_priority = { sizeof(low_priority), DRMGR_PRIORITY_NAME_DRREG_LOW,
                                      NULL, NULL, DRMGR_PRIORITY_INSERT_DRREG_LOW };
    drmgr_priority_t fault_priority = { sizeof(fault_priority),
                                        DRMGR_PRIORITY_NAME_DRREG_FAULT, NULL, NULL,
                                        DRMGR_PRIORITY_FAULT_DRREG };

    int count = dr_atomic_add32_return_sum(&drreg_init_count, 1);
    if (count == 1) {
        drmgr_init();

        if (!drmgr_register_thread_init_event(drreg_thread_init) ||
            !drmgr_register_thread_exit_event(drreg_thread_exit))
            return DRREG_ERROR;
        tls_idx = drmgr_register_tls_field();
        if (tls_idx == -1)
            return DRREG_ERROR;

        if (!drmgr_register_bb_instrumentation_event(NULL, drreg_event_bb_insert_early,
                                                     &high_priority) ||
            !drmgr_register_bb_instrumentation_event(
                drreg_event_bb_analysis, drreg_event_bb_insert_late, &low_priority) ||
            !drmgr_register_restore_state_ex_event_ex(drreg_event_restore_state,
                                                      &fault_priority))
            return DRREG_ERROR;
#ifdef X86
        /* We get an extra slot for aflags xax, rather than just documenting that
         * clients should add 2 instead of just 1, as there are many existing clients.
         */
        drreg_internal_ops.num_spill_slots = 1;
#endif
        /* Request no SIMD slots at the beginning */
        drreg_internal_ops.num_spill_simd_slots = 0;

        /* Support use during init when there is no TLS (i#2910). */
        tls_data_init(GLOBAL_DCONTEXT, &drreg_internal_init_pt);
    }

    if (ops_in->struct_size < offsetof(drreg_options_t, error_callback))
        return DRREG_ERROR_INVALID_PARAMETER;

    /* Instead of allowing only one drreg_init() and all other components to be
     * passed in scratch regs by a master, which is not always an easy-to-use
     * model, we instead consider all callers' requests, combining the option
     * fields.  We don't shift init to drreg_thread_init() or sthg b/c we really
     * want init-time error codes returning from drreg_init().
     */

    /* Sum the spill slots, honoring a new or prior do_not_sum_slots by taking
     * the max instead of summing.
     */

    DR_ASSERT(!(ops_in->struct_size <= offsetof(drreg_options_t, do_not_sum_slots) &&
                ops_in->struct_size > offsetof(drreg_options_t, num_spill_simd_slots)));
    if (ops_in->struct_size > offsetof(drreg_options_t, do_not_sum_slots)) {
        drreg_internal_ops.num_spill_slots = get_updated_num_slots(
            ops_in->do_not_sum_slots, drreg_internal_ops.num_spill_slots,
            ops_in->num_spill_slots);
        if (ops_in->struct_size > offsetof(drreg_options_t, num_spill_simd_slots)) {
            drreg_internal_ops.num_spill_simd_slots = get_updated_num_slots(
                ops_in->do_not_sum_slots, drreg_internal_ops.num_spill_simd_slots,
                ops_in->num_spill_simd_slots);
        }
        drreg_internal_ops.do_not_sum_slots = ops_in->do_not_sum_slots;
    } else {
        drreg_internal_ops.num_spill_slots = get_updated_num_slots(
            drreg_internal_ops.do_not_sum_slots, drreg_internal_ops.num_spill_slots,
            ops_in->num_spill_slots);
        if (ops_in->struct_size > offsetof(drreg_options_t, num_spill_simd_slots)) {
            drreg_internal_ops.num_spill_simd_slots = get_updated_num_slots(
                drreg_internal_ops.do_not_sum_slots,
                drreg_internal_ops.num_spill_simd_slots, ops_in->num_spill_simd_slots);
        }
        drreg_internal_ops.do_not_sum_slots = false;
    }

    /* If anyone wants to be conservative, then be conservative. */
    drreg_internal_ops.conservative =
        drreg_internal_ops.conservative || ops_in->conservative;

    /* The first callback wins. */
    if (ops_in->struct_size > offsetof(drreg_options_t, error_callback) &&
        drreg_internal_ops.error_callback == NULL)
        drreg_internal_ops.error_callback = ops_in->error_callback;

    if (prior_slots > 0) {
        /* +1 for the pointer to the indirect spill block, see below. */
        if (!dr_raw_tls_cfree(drreg_internal_tls_simd_offs, prior_slots + 1))
            return DRREG_ERROR;
    }

    /* 0 spill slots is supported, which would just fills in tls_seg for us.
     * However, we are allocating an additional slot for the pointer to the indirect
     * spill block.
     */
    if (!dr_raw_tls_calloc(&drreg_internal_tls_seg, &drreg_internal_tls_simd_offs,
                           drreg_internal_ops.num_spill_slots + 1, 0))
        return DRREG_ERROR_OUT_OF_SLOTS;

#ifdef SIMD_SUPPORTED
    if (prior_simd_slots < drreg_internal_ops.num_spill_simd_slots) {
        /* Refresh init_pt */

        byte *simd_spill_start;
        byte *simd_spills;
        drreg_internal_tls_alloc_simd_slots(GLOBAL_DCONTEXT, &simd_spill_start,
                                            &simd_spills,
                                            drreg_internal_ops.num_spill_simd_slots);

        if (prior_simd_slots > 0) {
            memcpy(simd_spills, drreg_internal_init_pt.simd_spills,
                   (SIMD_REG_SIZE * prior_simd_slots));
            drreg_internal_tls_free_simd_slots(GLOBAL_DCONTEXT,
                                               drreg_internal_init_pt.simd_spill_start,
                                               prior_simd_slots);
        }
        drreg_internal_init_pt.simd_spill_start = simd_spill_start;
        drreg_internal_init_pt.simd_spills = simd_spills;
    }
#endif

    /* Increment offset so that we now directly point to GPR slots, skipping the
     * pointer to the indirect SIMD block. We are treating this extra slot differently
     * from the aflags slot, because its offset is distinctly used for spilling and
     * restoring indirectly vs. directly.
     */
    drreg_internal_tls_slot_offs = drreg_internal_tls_simd_offs + sizeof(void *);

    return DRREG_SUCCESS;
}

drreg_status_t
drreg_exit(void)
{
    int count = dr_atomic_add32_return_sum(&drreg_init_count, -1);
    if (count != 0)
        return DRREG_SUCCESS;

    tls_data_free(GLOBAL_DCONTEXT, &drreg_internal_init_pt);

    if (!drmgr_unregister_thread_init_event(drreg_thread_init) ||
        !drmgr_unregister_thread_exit_event(drreg_thread_exit))
        return DRREG_ERROR;

    drmgr_unregister_tls_field(tls_idx);
    if (!drmgr_unregister_bb_insertion_event(drreg_event_bb_insert_early) ||
        !drmgr_unregister_bb_instrumentation_event(drreg_event_bb_analysis) ||
        !drmgr_unregister_restore_state_ex_event(drreg_event_restore_state))
        return DRREG_ERROR;

    drmgr_exit();

    /* +1 for the pointer to the indirect spill block. */
    if (!dr_raw_tls_cfree(drreg_internal_tls_simd_offs,
                          drreg_internal_ops.num_spill_slots + 1))
        return DRREG_ERROR;

    /* Support re-attach */
    memset(&drreg_internal_ops, 0, sizeof(drreg_internal_ops));

    return DRREG_SUCCESS;
}
