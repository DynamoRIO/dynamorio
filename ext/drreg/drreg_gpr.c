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
#include "drreg_gpr.h"
#include "drreg_aflag.h"
#include "../ext_utils.h"
#include <limits.h>

/***************************************************************************
 * SPILLING AND RESTORING
 */

uint
drreg_internal_find_free_gpr_slot(drreg_internal_per_thread_t *pt)
{
    uint i;
    /* 0 is always reserved for AFLAGS_SLOT */
    ASSERT(AFLAGS_SLOT == 0, "AFLAGS_SLOT is not 0");
    for (i = AFLAGS_SLOT + 1; i < MAX_SPILLS; i++) {
        if (pt->slot_use[i] == DR_REG_NULL)
            return i;
    }
    return MAX_SPILLS;
}

void
drreg_internal_spill_gpr(void *drcontext, drreg_internal_per_thread_t *pt,
                         instrlist_t *ilist, instr_t *where, reg_id_t reg, uint slot)
{
    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX " %s %d\n", __FUNCTION__, pt->live_idx,
        get_where_app_pc(where), get_register_name(reg), slot);
    ASSERT(pt->slot_use[slot] == DR_REG_NULL || pt->slot_use[slot] == reg ||
               /* Aflags can be saved and restored using different regs. */
               slot == AFLAGS_SLOT,
           "internal tracking error");
    if (slot == AFLAGS_SLOT)
        pt->aflags.ever_spilled = true;
    pt->slot_use[slot] = reg;
    if (slot < drreg_internal_ops.num_spill_slots) {
        dr_insert_write_raw_tls(drcontext, ilist, where, drreg_internal_tls_seg,
                                drreg_internal_tls_slot_offs + slot * sizeof(reg_t), reg);
    } else {
        dr_spill_slot_t DR_slot =
            (dr_spill_slot_t)(slot - drreg_internal_ops.num_spill_slots);
        dr_save_reg(drcontext, ilist, where, reg, DR_slot);
    }
    //    #ifdef DEBUG
    //        if (slot > stats_max_slot)
    //            stats_max_slot = slot; /* racy but that's ok */
    //    #endif
}

/*
 * Up to caller to update pt->reg. This routine updates pt->slot_use if release==true.
 */
void
drreg_internal_restore_gpr(void *drcontext, drreg_internal_per_thread_t *pt,
                           instrlist_t *ilist, instr_t *where, reg_id_t reg, uint slot,
                           bool release)
{
    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX " %s slot=%d release=%d\n", __FUNCTION__,
        pt->live_idx, get_where_app_pc(where), get_register_name(reg), slot, release);
    ASSERT(pt->slot_use[slot] == reg ||
               /* aflags can be saved and restored using different regs */
               (slot == AFLAGS_SLOT && pt->slot_use[slot] != DR_REG_NULL),
           "internal tracking error");
    if (release)
        pt->slot_use[slot] = DR_REG_NULL;
    if (slot < drreg_internal_ops.num_spill_slots) {
        dr_insert_read_raw_tls(drcontext, ilist, where, drreg_internal_tls_seg,
                               drreg_internal_tls_slot_offs + slot * sizeof(reg_t), reg);
    } else {
        dr_spill_slot_t DR_slot =
            (dr_spill_slot_t)(slot - drreg_internal_ops.num_spill_slots);
        dr_restore_reg(drcontext, ilist, where, reg, DR_slot);
    }
}

reg_t
drreg_internal_get_spilled_gpr_value(void *drcontext, uint tls_slot_offs, uint slot)
{
    if (slot < drreg_internal_ops.num_spill_slots) {
        drreg_internal_per_thread_t *pt = drreg_internal_get_tls_data(drcontext);
        return *(reg_t *)(pt->tls_seg_base + tls_slot_offs + slot * sizeof(reg_t));
    } else {
        dr_spill_slot_t DR_slot =
            (dr_spill_slot_t)(slot - drreg_internal_ops.num_spill_slots);
        return dr_read_saved_reg(drcontext, DR_slot);
    }
}

/***************************************************************************
 * ANALYSIS AND CROSS-APP-INSTR
 */

void
drreg_internal_increment_app_gpr_use_count(drreg_internal_per_thread_t *pt, opnd_t opnd,
                                           reg_id_t reg)
{
    ASSERT(reg_is_gpr(reg), "register should be a gpr");
    reg = reg_to_pointer_sized(reg);

    ASSERT(reg_is_pointer_sized(reg), "register should be a ptr sized");
    pt->reg[GPR_IDX(reg)].app_uses++;
    /* Tools that instrument memory uses (memtrace, Dr. Memory, etc.)
     * want to double-count memory opnd uses, as they need to restore
     * the app value to get the memory address into a register there.
     * We go ahead and do that for all tools.
     */
    if (opnd_is_memory_reference(opnd))
        pt->reg[GPR_IDX(reg)].app_uses++;
}

void
drreg_internal_bb_init_gpr_liveness_analysis(drreg_internal_per_thread_t *pt)
{
    for (reg_id_t reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++)
        pt->reg[GPR_IDX(reg)].app_uses = 0;
}

void
drreg_internal_bb_analyse_gpr_liveness(void *drcontext, drreg_internal_per_thread_t *pt,
                                       instr_t *inst, uint index)
{
    for (reg_id_t reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        void *value = REG_LIVE;
        /* DRi#1849: COND_SRCS here includes addressing regs in dsts */
        if (instr_reads_from_reg(inst, reg, DR_QUERY_INCLUDE_COND_SRCS))
            value = REG_LIVE;
        /* make sure we don't consider writes to sub-regs */
        else if (instr_writes_to_exact_reg(inst, reg, DR_QUERY_INCLUDE_COND_SRCS)
                 /* a write to a 32-bit reg for amd64 zeroes the top 32 bits */
                 IF_X86_64(||
                           instr_writes_to_exact_reg(inst, reg_64_to_32(reg),
                                                     DR_QUERY_INCLUDE_COND_SRCS)))
            value = REG_DEAD;
        else if (drreg_internal_is_xfer(inst))
            value = REG_LIVE;
        else if (index > 0)
            value = drvector_get_entry(&pt->reg[GPR_IDX(reg)].live, index - 1);
        LOG(drcontext, DR_LOG_ALL, 3, " %s=%d", get_register_name(reg),
            (int)(ptr_uint_t)value);
        drvector_set_entry(&pt->reg[GPR_IDX(reg)].live, index, value);
    }
}

drreg_status_t
drreg_internal_bb_insert_gpr_restore_all(void *drcontext, drreg_internal_per_thread_t *pt,
                                         instrlist_t *bb, instr_t *inst,
                                         bool force_restore, OUT bool *regs_restored)
{
    instr_t *next = instr_get_next(inst);
    drreg_status_t res;

    for (reg_id_t reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        if (regs_restored != NULL)
            regs_restored[GPR_IDX(reg)] = false;

        if (!pt->reg[GPR_IDX(reg)].native) {
            if (force_restore || instr_reads_from_reg(inst, reg, DR_QUERY_INCLUDE_ALL) ||
                /* Treat a partial write as a read, to restore rest of reg */
                (instr_writes_to_reg(inst, reg, DR_QUERY_INCLUDE_ALL) &&
                 !instr_writes_to_exact_reg(inst, reg, DR_QUERY_INCLUDE_ALL)) ||
                /* Treat a conditional write as a read and a write to handle the
                 * condition failing and our write handling saving the wrong value.
                 */
                (instr_writes_to_reg(inst, reg, DR_QUERY_INCLUDE_ALL) &&
                 !instr_writes_to_reg(inst, reg, DR_QUERY_DEFAULT)) ||
                /* i#1954: for complex bbs we must restore before the next app instr. */
                (!pt->reg[GPR_IDX(reg)].in_use &&
                 ((pt->bb_has_internal_flow &&
                   !TEST(DRREG_IGNORE_CONTROL_FLOW, pt->bb_props)) ||
                  TEST(DRREG_CONTAINS_SPANNING_CONTROL_FLOW, pt->bb_props))) ||
                /* If we're out of our own slots and are using a DR slot, we have to
                 * restore now b/c DR slots are not guaranteed across app instrs.
                 */
                pt->reg[GPR_IDX(reg)].slot >= (int)drreg_internal_ops.num_spill_slots) {
                if (!pt->reg[GPR_IDX(reg)].in_use) {
                    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": lazily restoring %s\n",
                        __FUNCTION__, pt->live_idx, get_where_app_pc(inst),
                        get_register_name(reg));
                    res =
                        drreg_internal_restore_gpr_reg_now(drcontext, pt, bb, inst, reg);
                    if (res != DRREG_SUCCESS) {
                        LOG(drcontext, DR_LOG_ALL, 1,
                            "%s @%d." PFX ": lazy restore failed\n", __FUNCTION__,
                            pt->live_idx, get_where_app_pc(inst));
                        return res;
                    }
                    ASSERT(pt->pending_unreserved > 0, "should not go negative");
                    pt->pending_unreserved--;
                } else if (pt->aflags.xchg == reg) {
                    /* Bail on keeping the flags in the reg. */
                    res = drreg_internal_move_aflags_from_reg(drcontext, pt, bb, inst,
                                                              true);
                    if (res != DRREG_SUCCESS)
                        return res;
                } else {
                    /* We need to move the tool's value somewhere else.
                     * We use a separate slot for that (and we document that
                     * tools should request an extra slot for each cross-app-instr
                     * register).
                     * XXX: optimize via xchg w/ a dead reg.
                     */
                    uint tmp_slot = drreg_internal_find_free_gpr_slot(pt);
                    if (tmp_slot == MAX_SPILLS)
                        return DRREG_ERROR_OUT_OF_SLOTS;

                    /* The approach:
                     *   + spill reg (tool val) to new slot
                     *   + restore to reg (app val) from app slot
                     *   + <app instr>
                     *   + restore to reg (tool val) from new slot
                     * XXX: if we change this, we need to update
                     * drreg_event_restore_state().
                     */
                    LOG(drcontext, DR_LOG_ALL, 3,
                        "%s @%d." PFX ": restoring %s for app read\n", __FUNCTION__,
                        pt->live_idx, get_where_app_pc(inst), get_register_name(reg));
                    drreg_internal_spill_gpr(drcontext, pt, bb, inst, reg, tmp_slot);
                    drreg_internal_restore_gpr(drcontext, pt, bb, inst, reg,
                                               pt->reg[GPR_IDX(reg)].slot,
                                               false /*keep slot*/);
                    drreg_internal_restore_gpr(drcontext, pt, bb, next, reg, tmp_slot,
                                               true);
                    /* Share the tool val spill if this inst writes too */
                    if (regs_restored != NULL)
                        regs_restored[GPR_IDX(reg)] = true;
                    /* We keep .native==false */
                }
            }
        }
    }

    return DRREG_SUCCESS;
}

drreg_status_t
drreg_internal_insert_gpr_update_spill(void *drcontext, drreg_internal_per_thread_t *pt,
                                       instrlist_t *bb, instr_t *inst,
                                       bool *restored_for_read)
{
    instr_t *next = instr_get_next(inst);
    drreg_status_t res;

    for (reg_id_t reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        if (pt->reg[GPR_IDX(reg)].in_use) {
            if (instr_writes_to_reg(inst, reg, DR_QUERY_INCLUDE_ALL) &&
                /* Don't bother if reg is dead beyond this write. */
                (drreg_internal_ops.conservative || pt->live_idx == 0 ||
                 drvector_get_entry(&pt->reg[GPR_IDX(reg)].live, pt->live_idx - 1) ==
                     REG_LIVE ||
                 pt->aflags.xchg == reg)) {
                uint tmp_slot = MAX_SPILLS;
                if (pt->aflags.xchg == reg) {
                    /* Bail on keeping the flags in the reg. */
                    res = drreg_internal_move_aflags_from_reg(drcontext, pt, bb, inst,
                                                              true);
                    if (res != DRREG_SUCCESS)
                        return res;

                    continue;
                }
                if (pt->reg[GPR_IDX(reg)].xchg != DR_REG_NULL) {
                    /* XXX i#511: NYI */
                    return DRREG_ERROR_FEATURE_NOT_AVAILABLE;
                }
                /* Approach (we share 1st and last w/ read, if reads and writes):
                 *   + spill reg (tool val) to new slot
                 *   + <app instr>
                 *   + spill reg (app val) to app slot
                 *   + restore to reg from new slot (tool val)
                 * XXX: if we change this, we need to update
                 * drreg_event_restore_state().
                 */
                LOG(drcontext, DR_LOG_ALL, 3,
                    "%s @%d." PFX ": re-spilling %s after app write\n", __FUNCTION__,
                    pt->live_idx, get_where_app_pc(inst), get_register_name(reg));
                if (!restored_for_read[GPR_IDX(reg)]) {
                    tmp_slot = drreg_internal_find_free_gpr_slot(pt);
                    if (tmp_slot == MAX_SPILLS)
                        return DRREG_ERROR_OUT_OF_SLOTS;

                    drreg_internal_spill_gpr(drcontext, pt, bb, inst, reg, tmp_slot);
                }
                drreg_internal_spill_gpr(drcontext, pt, bb,
                                         /* If reads and writes, make sure tool-restore
                                          * and app-spill are in the proper order.
                                          */
                                         restored_for_read[GPR_IDX(reg)]
                                             ? instr_get_prev(next)
                                             : next /*after*/,
                                         reg, pt->reg[GPR_IDX(reg)].slot);
                pt->reg[GPR_IDX(reg)].ever_spilled = true;
                if (!restored_for_read[GPR_IDX(reg)]) {
                    drreg_internal_restore_gpr(drcontext, pt, bb, next /*after*/, reg,
                                               tmp_slot, true);
                }
            }
        } else if (!pt->reg[GPR_IDX(reg)].native &&
                   instr_writes_to_reg(inst, reg, DR_QUERY_INCLUDE_ALL)) {
            /* For an unreserved reg that's written, just drop the slot, even
             * if it was spilled at an earlier reservation point.
             */
            LOG(drcontext, DR_LOG_ALL, 3,
                "%s @%d." PFX ": dropping slot for unreserved reg %s after app write\n",
                __FUNCTION__, pt->live_idx, get_where_app_pc(inst),
                get_register_name(reg));
            if (pt->reg[GPR_IDX(reg)].ever_spilled)
                pt->reg[GPR_IDX(reg)].ever_spilled = false; /* no need to restore */
            res = drreg_internal_restore_gpr_reg_now(drcontext, pt, bb, inst, reg);
            if (res != DRREG_SUCCESS)
                return res;
            pt->pending_unreserved--;
        }
    }

    return DRREG_SUCCESS;
}

/***************************************************************************
 * USE OUTSIDE INSERT PHASE
 */

void
drreg_internal_init_forward_gpr_liveness_analysis(drreg_internal_per_thread_t *pt)
{
    /* We just use index 0 of the live vectors */
    /* We just use index 0 of the live vectors */
    for (reg_id_t reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        pt->reg[GPR_IDX(reg)].app_uses = 0;
        drvector_set_entry(&pt->reg[GPR_IDX(reg)].live, 0, REG_UNKNOWN);
    }
}

void
drreg_internal_forward_analyse_gpr_liveness(drreg_internal_per_thread_t *pt,
                                            instr_t *inst)
{
    for (reg_id_t reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        void *value = REG_UNKNOWN;
        if (drvector_get_entry(&pt->reg[GPR_IDX(reg)].live, 0) != REG_UNKNOWN)
            continue;
        /* DRi#1849: COND_SRCS here includes addressing regs in dsts */
        if (instr_reads_from_reg(inst, reg, DR_QUERY_INCLUDE_COND_SRCS))
            value = REG_LIVE;
        /* make sure we don't consider writes to sub-regs */
        else if (instr_writes_to_exact_reg(inst, reg, DR_QUERY_INCLUDE_COND_SRCS)
                 /* a write to a 32-bit reg for amd64 zeroes the top 32 bits */
                 IF_X86_64(||
                           instr_writes_to_exact_reg(inst, reg_64_to_32(reg),
                                                     DR_QUERY_INCLUDE_COND_SRCS)))
            value = REG_DEAD;
        if (value != REG_UNKNOWN)
            drvector_set_entry(&pt->reg[GPR_IDX(reg)].live, 0, value);
    }
}

void
drreg_internal_finalise_forward_gpr_liveness_analysis(drreg_internal_per_thread_t *pt)
{
    /* If we could not determine state (i.e. unknown), we set the state to live. */
    for (reg_id_t reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        if (drvector_get_entry(&pt->reg[GPR_IDX(reg)].live, 0) == REG_UNKNOWN)
            drvector_set_entry(&pt->reg[GPR_IDX(reg)].live, 0, REG_LIVE);
    }
}

/***************************************************************************
 * REGISTER RESERVATION
 */

drreg_status_t
drreg_internal_reserve_gpr(void *drcontext, drreg_internal_per_thread_t *pt,
                           instrlist_t *ilist, instr_t *where, drvector_t *reg_allowed,
                           bool only_if_no_spill, OUT reg_id_t *reg_out)
{
    uint slot = MAX_SPILLS;
    uint min_uses = UINT_MAX;
    reg_id_t reg = DR_REG_STOP_GPR + 1, best_reg = DR_REG_NULL;
    bool already_spilled = false;
    if (reg_out == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;

    /* First, try to use a previously unreserved but not yet lazily restored reg.
     * This must be first to avoid accumulating slots beyond the requested max.
     * Because we drop an unreserved reg when the app writes to it, we should
     * never pick an unreserved and unspilled yet not currently dead reg over
     * some other dead reg.
     */
    if (pt->pending_unreserved > 0) {
        for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
            uint idx = GPR_IDX(reg);
            if (!pt->reg[idx].native && !pt->reg[idx].in_use &&
                (reg_allowed == NULL || drvector_get_entry(reg_allowed, idx) != NULL) &&
                (!only_if_no_spill || pt->reg[idx].ever_spilled ||
                 drvector_get_entry(&pt->reg[idx].live, pt->live_idx) == REG_DEAD)) {
                slot = pt->reg[idx].slot;
                pt->pending_unreserved--;
                already_spilled = pt->reg[idx].ever_spilled;
                LOG(drcontext, DR_LOG_ALL, 3,
                    "%s @%d." PFX ": using un-restored %s slot %d\n", __FUNCTION__,
                    pt->live_idx, get_where_app_pc(where), get_register_name(reg), slot);
                break;
            }
        }
    }

    if (reg > DR_REG_STOP_GPR) {
        /* Look for a dead register, or the least-used register */
        for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
            uint idx = GPR_IDX(reg);
            if (pt->reg[idx].in_use)
                continue;
            if (reg ==
                dr_get_stolen_reg() IF_ARM(|| reg == DR_REG_PC)
                /* Avoid xsp, even if it appears dead in things like OP_sysenter.
                 * On AArch64 use of SP is very restricted.
                 */
                IF_NOT_ARM(|| reg == DR_REG_XSP))
                continue;
            if (reg_allowed != NULL && drvector_get_entry(reg_allowed, idx) == NULL)
                continue;
            /* If we had a hint as to local vs whole-bb we could downgrade being
             * dead right now as a priority
             */
            if (drvector_get_entry(&pt->reg[idx].live, pt->live_idx) == REG_DEAD)
                break;
            if (only_if_no_spill)
                continue;
            if (pt->reg[idx].app_uses < min_uses) {
                best_reg = reg;
                min_uses = pt->reg[idx].app_uses;
            }
        }
    }
    if (reg > DR_REG_STOP_GPR) {
        if (best_reg != DR_REG_NULL)
            reg = best_reg;
        else {
#ifdef X86
            /* If aflags was unreserved but is still in xax, give it up rather than
             * fail to reserve a new register.
             */
            if (!pt->aflags.in_use && pt->reg[GPR_IDX(DR_REG_XAX)].in_use &&
                pt->aflags.xchg == DR_REG_XAX &&
                (reg_allowed == NULL ||
                 drvector_get_entry(reg_allowed, GPR_IDX(DR_REG_XAX)) != NULL)) {
                LOG(drcontext, DR_LOG_ALL, 3,
                    "%s @%d." PFX ": taking xax from unreserved aflags\n", __FUNCTION__,
                    pt->live_idx, get_where_app_pc(where));
                drreg_status_t res = drreg_internal_move_aflags_from_reg(
                    drcontext, pt, ilist, where, true);
                if (res != DRREG_SUCCESS)
                    return res;

                reg = DR_REG_XAX;
            } else
#endif
                return DRREG_ERROR_REG_CONFLICT;
        }
    }
    if (slot == MAX_SPILLS) {
        slot = drreg_internal_find_free_gpr_slot(pt);
        if (slot == MAX_SPILLS)
            return DRREG_ERROR_OUT_OF_SLOTS;
    }

    ASSERT(!pt->reg[GPR_IDX(reg)].in_use, "overlapping uses");
    pt->reg[GPR_IDX(reg)].in_use = true;
    if (!already_spilled) {
        /* Even if dead now, we need to own a slot in case reserved past dead point */
        if (drreg_internal_ops.conservative ||
            drvector_get_entry(&pt->reg[GPR_IDX(reg)].live, pt->live_idx) == REG_LIVE) {
            LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": spilling %s to slot %d\n",
                __FUNCTION__, pt->live_idx, get_where_app_pc(where),
                get_register_name(reg), slot);
            drreg_internal_spill_gpr(drcontext, pt, ilist, where, reg, slot);
            pt->reg[GPR_IDX(reg)].ever_spilled = true;
        } else {
            LOG(drcontext, DR_LOG_ALL, 3,
                "%s @%d." PFX ": no need to spill %s to slot %d\n", __FUNCTION__,
                pt->live_idx, get_where_app_pc(where), get_register_name(reg), slot);
            pt->slot_use[slot] = reg;
            pt->reg[GPR_IDX(reg)].ever_spilled = false;
        }
    } else {
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": %s already spilled to slot %d\n",
            __FUNCTION__, pt->live_idx, get_where_app_pc(where), get_register_name(reg),
            slot);
    }
    pt->reg[GPR_IDX(reg)].native = false;
    pt->reg[GPR_IDX(reg)].xchg = DR_REG_NULL;
    pt->reg[GPR_IDX(reg)].slot = slot;
    *reg_out = reg;
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_internal_restore_gpr_app_value(void *drcontext, drreg_internal_per_thread_t *pt,
                                     instrlist_t *ilist, instr_t *where, reg_id_t app_reg,
                                     reg_id_t dst_reg, bool stateful)
{
    if (!reg_is_gpr(app_reg) || !reg_is_pointer_sized(app_reg) ||
        !reg_is_pointer_sized(dst_reg))
        return DRREG_ERROR_INVALID_PARAMETER;

    /* check if app_reg is stolen reg */
    if (app_reg == dr_get_stolen_reg()) {
        /* DR will refuse to load into the same reg (the caller must use
         * opnd_replace_reg() with a scratch reg in that case).
         */
        if (dst_reg == app_reg) {
            return DRREG_ERROR_INVALID_PARAMETER;
        }
        if (dr_insert_get_stolen_reg_value(drcontext, ilist, where, dst_reg)) {
            return DRREG_SUCCESS;
        }
        ASSERT(false, "internal error on getting stolen reg app value");
        return DRREG_ERROR;
    }

    if (reg_is_gpr(app_reg)) {
        /* Check if app_reg is an unspilled reg. */
        if (pt->reg[GPR_IDX(app_reg)].native) {
            LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": reg %s already native\n",
                __FUNCTION__, pt->live_idx, get_where_app_pc(where),
                get_register_name(app_reg));
            if (dst_reg != app_reg) {
                PRE(ilist, where,
                    XINST_CREATE_move(drcontext, opnd_create_reg(dst_reg),
                                      opnd_create_reg(app_reg)));
            }
            return DRREG_SUCCESS;
        }
        /* We may have lost the app value for a dead reg. */
        if (!pt->reg[GPR_IDX(app_reg)].ever_spilled) {
            LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": reg %s never spilled\n",
                __FUNCTION__, pt->live_idx, get_where_app_pc(where),
                get_register_name(app_reg));
            return DRREG_ERROR_NO_APP_VALUE;
        }
        /* Restore the app value back to app_reg. */
        if (pt->reg[GPR_IDX(app_reg)].xchg != DR_REG_NULL) {
            /* XXX i#511: NYI */
            return DRREG_ERROR_FEATURE_NOT_AVAILABLE;
        }
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": getting app value for %s\n",
            __FUNCTION__, pt->live_idx, get_where_app_pc(where),
            get_register_name(app_reg));
        /* XXX i#511: if we add .xchg support for GPR's we'll need to check them all
         * here.
         */
        if (pt->aflags.xchg == app_reg) {
            /* Bail on keeping the flags in the reg. */
            drreg_status_t res = drreg_internal_move_aflags_from_reg(drcontext, pt, ilist,
                                                                     where, stateful);
            if (res != DRREG_SUCCESS)
                return res;
        } else {
            drreg_internal_restore_gpr(drcontext, pt, ilist, where, app_reg,
                                       pt->reg[GPR_IDX(app_reg)].slot,
                                       stateful && !pt->reg[GPR_IDX(app_reg)].in_use);
            if (stateful && !pt->reg[GPR_IDX(app_reg)].in_use)
                pt->reg[GPR_IDX(app_reg)].native = true;
        }
    }

    return DRREG_SUCCESS;
}

drreg_status_t
drreg_internal_restore_gpr_app_values(void *drcontext, instrlist_t *ilist, instr_t *where,
                                      opnd_t opnd, INOUT reg_id_t *swap,
                                      OUT bool *no_app_value)
{
    drreg_status_t res;
    int num_op = opnd_num_regs_used(opnd);

    DR_ASSERT(no_app_value != NULL);

    /* Now restore GPRs. */
    for (int i = 0; i < num_op; i++) {
        reg_id_t reg = opnd_get_reg_used(opnd, i);
        if (!reg_is_gpr(reg))
            continue;

        reg = reg_to_pointer_sized(reg);
        reg_id_t dst = reg;
        if (reg == dr_get_stolen_reg()) {
            if (swap == NULL) {
                return DRREG_ERROR_INVALID_PARAMETER;
            }
            if (*swap == DR_REG_NULL) {
                res = drreg_reserve_register(drcontext, ilist, where, NULL, &dst);
                if (res != DRREG_SUCCESS) {
                    return res;
                }
            } else
                dst = *swap;
            if (!opnd_replace_reg(&opnd, reg, dst)) {
                return DRREG_ERROR;
            }
            *swap = dst;
        }

        res = drreg_get_app_value(drcontext, ilist, where, reg, dst);
        if (res == DRREG_ERROR_NO_APP_VALUE)
            *no_app_value = true;
        else if (res != DRREG_SUCCESS) {
            return res;
        }
    }

    return DRREG_SUCCESS;
}

drreg_status_t
drreg_internal_restore_gpr_reg_now(void *drcontext, drreg_internal_per_thread_t *pt,
                                   instrlist_t *ilist, instr_t *inst, reg_id_t reg)
{
    if (!reg_is_gpr(reg))
        return DRREG_ERROR_INVALID_PARAMETER;

    if (pt->reg[GPR_IDX(reg)].ever_spilled) {
        if (pt->reg[GPR_IDX(reg)].xchg != DR_REG_NULL) {
            /* XXX i#511: NYI */
            return DRREG_ERROR_FEATURE_NOT_AVAILABLE;
        }
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": restoring %s\n", __FUNCTION__,
            pt->live_idx, get_where_app_pc(inst), get_register_name(reg));

        drreg_internal_restore_gpr(drcontext, pt, ilist, inst, reg,
                                   pt->reg[GPR_IDX(reg)].slot, true);
    } else {
        /* still need to release slot */
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": %s never spilled\n", __FUNCTION__,
            pt->live_idx, get_where_app_pc(inst), get_register_name(reg));
        pt->slot_use[pt->reg[GPR_IDX(reg)].slot] = DR_REG_NULL;
    }
    pt->reg[GPR_IDX(reg)].native = true;

    return DRREG_SUCCESS;
}

drreg_status_t
drreg_internal_unreserve_gpr(void *drcontext, drreg_internal_per_thread_t *pt,
                             instrlist_t *ilist, instr_t *where, reg_id_t reg)
{
    if (!reg_is_gpr(reg) || !pt->reg[GPR_IDX(reg)].in_use)
        return DRREG_ERROR_INVALID_PARAMETER;

    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX " %s\n", __FUNCTION__, pt->live_idx,
        get_where_app_pc(where), get_register_name(reg));
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        /* We have no way to lazily restore. We do not bother at this point
         * to try and eliminate back-to-back spill/restore pairs.
         */
        dr_pred_type_t pred = instrlist_get_auto_predicate(ilist);
        drreg_status_t res;

        /* XXX i#2585: drreg should predicate spills and restores as appropriate. */
        instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
        res = drreg_internal_restore_gpr_reg_now(drcontext, pt, ilist, where, reg);
        instrlist_set_auto_predicate(ilist, pred);
        if (res != DRREG_SUCCESS)
            return res;
    } else {
        /* We lazily restore in drreg_event_bb_insert_late(), in case
         * someone else wants a local scratch.
         */
        pt->pending_unreserved++;
    }
    pt->reg[GPR_IDX(reg)].in_use = false;

    return DRREG_SUCCESS;
}

drreg_status_t
drreg_internal_is_gpr_dead(drreg_internal_per_thread_t *pt, reg_id_t reg, bool *dead)
{
    if (dead == NULL || !reg_is_gpr(reg))
        return DRREG_ERROR_INVALID_PARAMETER;

    *dead = drvector_get_entry(&pt->reg[GPR_IDX(reg)].live, pt->live_idx) == REG_DEAD;
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_internal_init_and_fill_gpr_vector(drvector_t *vec, bool allowed)
{
    uint size = DR_NUM_GPR_REGS;

    if (vec == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;

    drvector_init(vec, size, false /*!synch*/, NULL);
    for (reg_id_t reg = 0; reg < size; reg++)
        drvector_set_entry(vec, reg, allowed ? (void *)(ptr_uint_t)1 : NULL);
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_internal_set_gpr_vector_entry(drvector_t *vec, reg_id_t reg, bool allowed)
{
    reg_id_t start_reg = DR_REG_START_GPR;

    if (vec == NULL || !reg_is_gpr(reg) || reg < DR_REG_START_GPR ||
        reg > DR_REG_STOP_GPR)
        return DRREG_ERROR_INVALID_PARAMETER;

    drvector_set_entry(vec, reg - start_reg, allowed ? (void *)(ptr_uint_t)1 : NULL);
    return DRREG_SUCCESS;
}

/***************************************************************************
 * RESTORE STATE
 */

bool
drreg_internal_is_gpr_spill_or_restore(void *drcontext, instr_t *instr, bool is_tls,
                                       uint offs, uint *slot_out OUT)
{
    ASSERT(slot_out != NULL, "invalid parameter");

#ifdef DEBUG
    bool dbg_is_tls;
    uint dbg_offs;
    reg_id_t dbg_reg_spilled;
    bool dbg_is_spilled;
    if (!instr_is_reg_spill_or_restore(drcontext, instr, &dbg_is_tls, &dbg_is_spilled,
                                       &dbg_reg_spilled, &dbg_offs))
        ASSERT(false, "instr should be a spill or restore");
    ASSERT(dbg_is_tls == is_tls, "is_tls should match");
    ASSERT(dbg_offs == offs, "offs should match");
#endif

    if (is_tls) {
        if (offs >= drreg_internal_tls_slot_offs &&
            offs < (drreg_internal_tls_slot_offs +
                    drreg_internal_ops.num_spill_slots * sizeof(reg_t))) {
            *slot_out = (offs - drreg_internal_tls_slot_offs) / sizeof(reg_t);
            return true;
        }
        /* We assume a DR spill slot, in TLS or thread-private mcontext */

        /* We assume the DR slots are either low-to-high or high-to-low. */
        uint DR_min_offs = opnd_get_disp(dr_reg_spill_slot_opnd(drcontext, SPILL_SLOT_1));
        uint DR_max_offs = opnd_get_disp(
            dr_reg_spill_slot_opnd(drcontext, dr_max_opnd_accessible_spill_slot()));
        uint max_DR_slot = (uint)dr_max_opnd_accessible_spill_slot();
        if (DR_min_offs > DR_max_offs) {
            if (offs > DR_min_offs) {
                *slot_out = (offs - DR_min_offs) / sizeof(reg_t);
            } else if (offs < DR_max_offs) {
                /* Fix hidden slot regardless of low-to-high or vice versa. */
                *slot_out = max_DR_slot + 1;
            } else {
                *slot_out = (DR_min_offs - offs) / sizeof(reg_t);
            }
        } else {
            if (offs > DR_max_offs) {
                *slot_out = (offs - DR_max_offs) / sizeof(reg_t);
            } else if (offs < DR_min_offs) {
                /* Fix hidden slot regardless of low-to-high or vice versa. */
                *slot_out = max_DR_slot + 1;
            } else {
                *slot_out = (offs - DR_min_offs) / sizeof(reg_t);
            }
        }
        if (*slot_out > max_DR_slot) {
            /* This is not a drreg spill, but some TLS access by
             * tool instrumentation (i#2035).
             */
            return false;
        }
#ifdef X86
        if (*slot_out > max_DR_slot - 1) {
            /* FIXME i#2933: We rule out the 3rd DR TLS slot b/c it's used by
             * DR for purposes where there's no restore paired with a spill.
             * Another tool component could also use the other slots that way,
             * though: we need a more foolproof solution.  For now we have a hole
             * and tools should allocate enough dedicated drreg TLS slots to
             * ensure robustness.
             */
            return false;
        }
#endif
    } else {
        /* We assume mcontext spill offs is 0-based. */
        *slot_out = offs / sizeof(reg_t);
    }
    *slot_out += drreg_internal_ops.num_spill_slots;

    return true;
}

void
drreg_internal_gpr_restore_state_init(uint *spilled_to)
{
    ASSERT(spilled_to != NULL, "shouldn't be NULL");

    for (reg_id_t reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++)
        spilled_to[GPR_IDX(reg)] = MAX_SPILLS;
}

void
drreg_internal_gpr_restore_state_handle_spill(void *drcontext, byte *pc, uint slot,
                                              reg_id_t reg, uint *spilled_to)
{
    ASSERT(spilled_to != NULL, "cannot be NULL");
    ASSERT(reg_is_gpr(reg), "spill must be for GPR reg");
    ASSERT(reg_is_pointer_sized(reg), "spill must be for GPR reg");

    if (spilled_to[GPR_IDX(reg)] < MAX_SPILLS &&
        /* allow redundant is_spill */
        spilled_to[GPR_IDX(reg)] != slot) {
        /* This reg is already spilled: we assume that this new spill
         * is to a tmp slot for preserving the tool's value.
         */
        LOG(drcontext, DR_LOG_ALL, 3, "%s @" PFX ": ignoring tool is_spill\n",
            __FUNCTION__, pc);
    } else {
        spilled_to[GPR_IDX(reg)] = slot;
    }
}

void
drreg_internal_gpr_restore_state_handle_restore(void *drcontext, byte *pc, uint slot,
                                                reg_id_t reg, uint *spilled_to)
{
    if (spilled_to[GPR_IDX(reg)] == slot)
        spilled_to[GPR_IDX(reg)] = MAX_SPILLS;
    else {
        LOG(drcontext, DR_LOG_ALL, 3, "%s @" PFX ": ignoring restore\n", __FUNCTION__,
            pc);
    }
}

void
drreg_internal_gpr_restore_state_set_values(void *drcontext,
                                            dr_restore_state_info_t *info,
                                            uint *spilled_to)
{
    for (reg_id_t reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        if (spilled_to[GPR_IDX(reg)] < MAX_SPILLS) {
            reg_t val = drreg_internal_get_spilled_gpr_value(
                drcontext, drreg_internal_tls_slot_offs, spilled_to[GPR_IDX(reg)]);
            LOG(drcontext, DR_LOG_ALL, 3,
                "%s: restoring %s from slot %d from " PFX " to " PFX "\n", __FUNCTION__,
                get_register_name(reg), spilled_to[GPR_IDX(reg)],
                reg_get_value(reg, info->mcontext), val);
            reg_set_value(reg, info->mcontext, val);
        }
    }
}

/***************************************************************************
 * INIT AND EXIT
 */

void
drreg_internal_tls_gpr_data_init(drreg_internal_per_thread_t *pt)
{
    for (reg_id_t reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        drvector_init(&pt->reg[GPR_IDX(reg)].live, 20, false /*!synch*/, NULL);
        pt->reg[GPR_IDX(reg)].native = true;
    }
}

void
drreg_internal_tls_gpr_data_free(drreg_internal_per_thread_t *pt)
{
    for (reg_id_t reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        drvector_delete(&pt->reg[GPR_IDX(reg)].live);
    }
}
