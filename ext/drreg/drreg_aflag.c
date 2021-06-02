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

/* This file contains code specifically for aflags (un)reservation. */

#include "dr_api.h"
#include "drreg_priv.h"
#include "drreg_gpr.h"
#include "drreg_aflag.h"
#include "../ext_utils.h"

/***************************************************************************
 * SPILLING AND RESTORING
 */

/* Note, this function may modify pt->aflags.xchg. */
static drreg_status_t
drreg_internal_spill_aflags(void *drcontext, drreg_internal_per_thread_t *pt,
                            instrlist_t *ilist, instr_t *where)
{
#ifdef X86
    drreg_internal_reg_class_info_t *gpr_info = &pt->gpr_class_info;
    uint aflags = (uint)(ptr_uint_t)drvector_get_entry(&pt->aflags.live, pt->live_idx);
    reg_id_t xax_swap = DR_REG_NULL;
    drreg_status_t res;

    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX "\n", __FUNCTION__, pt->live_idx,
        get_where_app_pc(where));
    /* It may be in-use for ourselves, storing the flags in xax. */
    if (gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].in_use &&
        pt->aflags.xchg != DR_REG_XAX) {
        /* No way to tell whoever is using xax that we need it, so we pick an
         * unreserved reg, spill it, and put xax there temporarily.  We store
         * aflags in our dedicated aflags tls slot and don't try to keep it in
         * this reg.
         */
        res = drreg_internal_reserve_gpr(drcontext, pt, ilist, where, NULL, false,
                                         &xax_swap);
        if (res != DRREG_SUCCESS)
            return res;
        LOG(drcontext, DR_LOG_ALL, 3, "  xax is in use: using %s temporarily\n",
            get_register_name(xax_swap));
        PRE(ilist, where,
            INSTR_CREATE_xchg(drcontext, opnd_create_reg(DR_REG_XAX),
                              opnd_create_reg(xax_swap)));
    }
    if (!gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].native) {
        /* xax is unreserved but not restored */
        ASSERT(gpr_info->slot_use[gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].slot] ==
                   DR_REG_XAX,
               "xax tracking error");
        LOG(drcontext, DR_LOG_ALL, 3, "  using un-restored xax in slot %d\n",
            gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].slot);
    } else if (pt->aflags.xchg != DR_REG_XAX) {
        uint xax_slot = drreg_internal_find_free_gpr_slot(gpr_info);
        if (xax_slot == MAX_SPILLS)
            return DRREG_ERROR_OUT_OF_SLOTS;
        if (drreg_internal_ops.conservative ||
            drvector_get_entry(&gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].live,
                               pt->live_idx) == REG_LIVE)
            drreg_internal_spill_gpr(drcontext, pt, ilist, where, DR_REG_XAX, xax_slot);
        else
            gpr_info->slot_use[xax_slot] = DR_REG_XAX;
        gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].slot = xax_slot;
        ASSERT(gpr_info->slot_use[xax_slot] == DR_REG_XAX, "slot should be for xax");
    }
    PRE(ilist, where, INSTR_CREATE_lahf(drcontext));
    if (TEST(EFLAGS_READ_OF, aflags)) {
        PRE(ilist, where,
            INSTR_CREATE_setcc(drcontext, OP_seto, opnd_create_reg(DR_REG_AL)));
    }
    if (xax_swap != DR_REG_NULL) {
        PRE(ilist, where,
            INSTR_CREATE_xchg(drcontext, opnd_create_reg(xax_swap),
                              opnd_create_reg(DR_REG_XAX)));
        drreg_internal_spill_gpr(drcontext, pt, ilist, where, xax_swap, AFLAGS_SLOT);
        res = drreg_internal_unreserve_gpr(drcontext, pt, ilist, where, xax_swap);
        if (res != DRREG_SUCCESS)
            return res; /* XXX: undo already-inserted instrs? */
    } else {
        /* As an optimization we keep the flags in xax itself until forced to move
         * them to the aflags TLS slot.
         */
        gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].in_use = true;
        gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].native = false;
        gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].ever_spilled = true;
        pt->aflags.xchg = DR_REG_XAX;
    }
#elif defined(AARCHXX)
    drreg_status_t res = DRREG_SUCCESS;
    reg_id_t scratch;
    res = drreg_internal_reserve_gpr(drcontext, pt, ilist, where, NULL, false, &scratch);
    if (res != DRREG_SUCCESS)
        return res;
    dr_save_arith_flags_to_reg(drcontext, ilist, where, scratch);
    drreg_internal_spill_gpr(drcontext, pt, ilist, where, scratch, AFLAGS_SLOT);
    res = drreg_internal_unreserve_gpr(drcontext, pt, ilist, where, scratch);
    if (res != DRREG_SUCCESS)
        return res; /* XXX: undo already-inserted instrs? */
#endif
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_internal_restore_aflags(void *drcontext, drreg_internal_per_thread_t *pt,
                              instrlist_t *ilist, instr_t *where, bool release)
{
#ifdef X86
    drreg_internal_reg_class_info_t *gpr_info = &pt->gpr_class_info;
    uint aflags = (uint)(ptr_uint_t)drvector_get_entry(&pt->aflags.live, pt->live_idx);
    uint temp_slot = 0;
    reg_id_t xax_swap = DR_REG_NULL;
    drreg_status_t res;
    LOG(drcontext, DR_LOG_ALL, 3,
        "%s @%d." PFX ": release=%d xax-in-use=%d,slot=%d xchg=%s\n", __FUNCTION__,
        pt->live_idx, get_where_app_pc(where), release,
        gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].in_use,
        gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].slot,
        get_register_name(pt->aflags.xchg));
    if (pt->aflags.native)
        return DRREG_SUCCESS;
    if (pt->aflags.xchg == DR_REG_XAX) {
        ASSERT(gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].in_use,
               "eflags-in-xax error");
    } else {
        temp_slot = drreg_internal_find_free_gpr_slot(gpr_info);
        if (temp_slot == MAX_SPILLS)
            return DRREG_ERROR_OUT_OF_SLOTS;
        if (gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].in_use) {
            /* We pick an unreserved reg, spill it, and put xax there temporarily. */
            res = drreg_internal_reserve_gpr(drcontext, pt, ilist, where, NULL, false,
                                             &xax_swap);
            if (res != DRREG_SUCCESS)
                return res;
            LOG(drcontext, DR_LOG_ALL, 3, "  xax is in use: using %s temporarily\n",
                get_register_name(xax_swap));
            PRE(ilist, where,
                INSTR_CREATE_xchg(drcontext, opnd_create_reg(DR_REG_XAX),
                                  opnd_create_reg(xax_swap)));
        } else if (drreg_internal_ops.conservative ||
                   drvector_get_entry(&gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].live,
                                      pt->live_idx) == REG_LIVE) {
            drreg_internal_spill_gpr(drcontext, pt, ilist, where, DR_REG_XAX, temp_slot);
        }
        drreg_internal_restore_gpr(drcontext, pt, ilist, where, DR_REG_XAX, AFLAGS_SLOT,
                                   release);
    }
    if (TEST(EFLAGS_READ_OF, aflags)) {
        /* i#2351: DR's "add 0x7f, %al" is destructive.  Instead we use a
         * cmp so we can avoid messing up the value in al, which is
         * required for keeping the flags in xax.
         */
        PRE(ilist, where,
            INSTR_CREATE_cmp(drcontext, opnd_create_reg(DR_REG_AL),
                             OPND_CREATE_INT8(-127)));
    }
    PRE(ilist, where, INSTR_CREATE_sahf(drcontext));
    if (xax_swap != DR_REG_NULL) {
        PRE(ilist, where,
            INSTR_CREATE_xchg(drcontext, opnd_create_reg(xax_swap),
                              opnd_create_reg(DR_REG_XAX)));
        res = drreg_internal_unreserve_gpr(drcontext, pt, ilist, where, xax_swap);
        if (res != DRREG_SUCCESS)
            return res; /* XXX: undo already-inserted instrs? */
    } else if (pt->aflags.xchg == DR_REG_XAX) {
        if (release) {
            pt->aflags.xchg = DR_REG_NULL;
            gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].in_use = false;
        }
    } else {
        if (drreg_internal_ops.conservative ||
            drvector_get_entry(&gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].live,
                               pt->live_idx) == REG_LIVE) {
            drreg_internal_restore_gpr(drcontext, pt, ilist, where, DR_REG_XAX, temp_slot,
                                       true);
        }
    }
#elif defined(AARCHXX)
    drreg_status_t res = DRREG_SUCCESS;
    reg_id_t scratch;
    res = drreg_internal_reserve_gpr(drcontext, pt, ilist, where, NULL, false, &scratch);
    if (res != DRREG_SUCCESS)
        return res;
    drreg_internal_restore_gpr(drcontext, pt, ilist, where, scratch, AFLAGS_SLOT,
                               release);
    dr_restore_arith_flags_from_reg(drcontext, ilist, where, scratch);
    res = drreg_internal_unreserve_gpr(drcontext, pt, ilist, where, scratch);
    if (res != DRREG_SUCCESS)
        return res; /* XXX: undo already-inserted instrs? */
#endif
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_internal_move_aflags_from_reg(void *drcontext, drreg_internal_per_thread_t *pt,
                                    instrlist_t *ilist, instr_t *where, bool stateful)
{
#ifdef X86
    drreg_internal_reg_class_info_t *gpr_info = &pt->gpr_class_info;
    if (pt->aflags.in_use || !stateful) {
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": moving aflags from xax to slot\n",
            __FUNCTION__, pt->live_idx, get_where_app_pc(where));
        drreg_internal_spill_gpr(drcontext, pt, ilist, where, DR_REG_XAX, AFLAGS_SLOT);
    } else if (!pt->aflags.native) {
        drreg_status_t res;
        LOG(drcontext, DR_LOG_ALL, 3,
            "%s @%d." PFX ": lazily restoring aflags for app xax\n", __FUNCTION__,
            pt->live_idx, get_where_app_pc(where));
        res =
            drreg_internal_restore_aflags(drcontext, pt, ilist, where, true /*release*/);
        if (res != DRREG_SUCCESS) {
            /* Failed to restore flags before app xax. */
            return res;
        }
        pt->aflags.native = true;
        gpr_info->slot_use[AFLAGS_SLOT] = DR_REG_NULL;
    }
    LOG(drcontext, DR_LOG_ALL, 3,
        "%s @%d." PFX ": restoring xax spilled for aflags in slot %d\n", __FUNCTION__,
        pt->live_idx, get_where_app_pc(where),
        gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].slot);
    if (drreg_internal_ops.conservative ||
        drvector_get_entry(&gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].live,
                           pt->live_idx) == REG_LIVE) {
        drreg_internal_restore_gpr(drcontext, pt, ilist, where, DR_REG_XAX,
                                   gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].slot,
                                   stateful);
    } else if (stateful)
        gpr_info->slot_use[gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].slot] =
            DR_REG_NULL;
    if (stateful) {
        gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].in_use = false;
        gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].native = true;
        gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].ever_spilled = false;
        pt->aflags.xchg = DR_REG_NULL;
    }
#endif

    return DRREG_SUCCESS;
}

/***************************************************************************
 * ANALYSIS AND CROSS-APP-INSTR
 */

void
drreg_internal_bb_analyse_aflag_liveness(void *drcontext, drreg_internal_per_thread_t *pt,
                                         instr_t *inst, uint index)
{
    ptr_uint_t aflags_cur = 0;

    ptr_uint_t aflags_new = instr_get_arith_flags(inst, DR_QUERY_INCLUDE_COND_SRCS);
    if (drreg_internal_is_xfer(inst))
        aflags_cur = EFLAGS_READ_ARITH; /* assume flags are read before written */
    else {
        uint aflags_read, aflags_w2r;
        if (index == 0)
            aflags_cur = EFLAGS_READ_ARITH; /* assume flags are read before written */
        else {
            aflags_cur =
                (uint)(ptr_uint_t)drvector_get_entry(&pt->aflags.live, index - 1);
        }
        aflags_read = (aflags_new & EFLAGS_READ_ARITH);
        /* if a flag is read by inst, set the read bit */
        aflags_cur |= (aflags_new & EFLAGS_READ_ARITH);
        /* if a flag is written and not read by inst, clear the read bit */
        aflags_w2r = EFLAGS_WRITE_TO_READ(aflags_new & EFLAGS_WRITE_ARITH);
        aflags_cur &= ~(aflags_w2r & ~aflags_read);
    }
    LOG(drcontext, DR_LOG_ALL, 3, " flags=%d\n", aflags_cur);
    drvector_set_entry(&pt->aflags.live, index, (void *)(ptr_uint_t)aflags_cur);
}

drreg_status_t
drreg_internal_bb_insert_aflag_restore_all(void *drcontext,
                                           drreg_internal_per_thread_t *pt,
                                           instrlist_t *bb, instr_t *inst,
                                           bool force_restore)
{
    drreg_internal_reg_class_info_t *gpr_info = &pt->gpr_class_info;

    /* Before each app read, or at end of bb, restore aflags to app value */
    uint aflags = (uint)(ptr_uint_t)drvector_get_entry(&pt->aflags.live, pt->live_idx);
    if (!pt->aflags.native &&
        (force_restore ||
         TESTANY(EFLAGS_READ_ARITH, instr_get_eflags(inst, DR_QUERY_DEFAULT)) ||
         /* Writing just a subset needs to combine with the original unwritten */
         (TESTANY(EFLAGS_WRITE_ARITH, instr_get_eflags(inst, DR_QUERY_INCLUDE_ALL)) &&
          aflags != 0 /*0 means everything is dead*/) ||
         /* DR slots are not guaranteed across app instrs */
         pt->aflags.slot >= (int)drreg_internal_ops.num_spill_slots)) {
        /* Restore aflags to app value */
        LOG(drcontext, DR_LOG_ALL, 3,
            "%s @%d." PFX " aflags=0x%x use=%d: lazily restoring aflags\n", __FUNCTION__,
            pt->live_idx, get_where_app_pc(inst), aflags, pt->aflags.in_use);

        drreg_status_t res =
            drreg_internal_restore_aflags(drcontext, pt, bb, inst, false /*keep slot*/);
        if (res != DRREG_SUCCESS) {
            LOG(drcontext, DR_LOG_ALL, 1,
                "%s @%d." PFX ": failed to restore flags before app read\n", __FUNCTION__,
                pt->live_idx, get_where_app_pc(inst));
            return res;
        }

        if (!pt->aflags.in_use) {
            pt->aflags.native = true;
            gpr_info->slot_use[AFLAGS_SLOT] = DR_REG_NULL;
        }
    }

    return DRREG_SUCCESS;
}

drreg_status_t
drreg_internal_insert_aflag_update_spill(void *drcontext, drreg_internal_per_thread_t *pt,
                                         instrlist_t *bb, instr_t *inst)
{
    drreg_internal_reg_class_info_t *gpr_info = &pt->gpr_class_info;

    instr_t *next = instr_get_next(inst);

    /* After aflags write by app, update spilled app value */
    if (TESTANY(EFLAGS_WRITE_ARITH, instr_get_eflags(inst, DR_QUERY_INCLUDE_ALL)) &&
        /* Is everything written later? */
        (pt->live_idx == 0 ||
         (ptr_uint_t)drvector_get_entry(&pt->aflags.live, pt->live_idx - 1) != 0)) {
        if (pt->aflags.in_use) {
            LOG(drcontext, DR_LOG_ALL, 3,
                "%s @%d." PFX ": re-spilling aflags after app write\n", __FUNCTION__,
                pt->live_idx, get_where_app_pc(inst));

            drreg_status_t res =
                drreg_internal_spill_aflags(drcontext, pt, bb, next /*after*/);
            if (res != DRREG_SUCCESS)
                return res;

            pt->aflags.native = false;
        } else if (!pt->aflags.native ||
                   gpr_info->slot_use[AFLAGS_SLOT] !=
                       DR_REG_NULL IF_X86(
                           ||
                           (gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].in_use &&
                            pt->aflags.xchg == DR_REG_XAX))) {
            /* give up slot */
            LOG(drcontext, DR_LOG_ALL, 3,
                "%s @%d." PFX ": giving up aflags slot after app write\n", __FUNCTION__,
                pt->live_idx, get_where_app_pc(inst));
#ifdef X86
            if (gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].in_use &&
                pt->aflags.xchg == DR_REG_XAX) {
                drreg_status_t res =
                    drreg_internal_move_aflags_from_reg(drcontext, pt, bb, inst, true);
                if (res != DRREG_SUCCESS)
                    return res;
            }

#endif
            gpr_info->slot_use[AFLAGS_SLOT] = DR_REG_NULL;
            pt->aflags.native = true;
        }
    }

    return DRREG_SUCCESS;
}

/***************************************************************************
 * USE OUTSIDE INSERT PHASE
 */

void
drreg_internal_forward_analyse_aflag_liveness(instr_t *inst, INOUT ptr_uint_t *aflags_cur)
{
    ptr_uint_t aflags_new = instr_get_arith_flags(inst, DR_QUERY_INCLUDE_COND_SRCS);
    /* reading and writing counts only as reading */
    aflags_new &= (~(EFLAGS_READ_TO_WRITE(aflags_new)));
    /* reading doesn't count if already written */
    aflags_new &= (~(EFLAGS_WRITE_TO_READ(*aflags_cur)));
    *aflags_cur |= aflags_new;
}

void
drreg_internal_finalise_forward_aflag_liveness_analysis(drreg_internal_per_thread_t *pt,
                                                        ptr_uint_t aflags_cur)
{
    drvector_set_entry(&pt->aflags.live, 0,
                       (void *)(ptr_uint_t)
                       /* set read bit if not written */
                       (EFLAGS_READ_ARITH & (~(EFLAGS_WRITE_TO_READ(aflags_cur)))));
}

/***************************************************************************
 * REGISTER RESERVATION
 */

bool
drreg_internal_aflag_handle_respill_for_statelessly_restore(
    void *drcontext, drreg_internal_per_thread_t *pt, instrlist_t *ilist,
    instr_t *where_respill, reg_id_t reg)
{
    /* XXX i#511: if we add .xchg support for GPR's we'll need to check them as similarly
     * done below.
     */
#ifdef X86
    drreg_internal_reg_class_info_t *gpr_info = &pt->gpr_class_info;
    if (reg != DR_REG_NULL && pt->aflags.xchg == reg) {
        gpr_info->slot_use[AFLAGS_SLOT] = DR_REG_XAX; /* appease assert */
        drreg_internal_restore_gpr(drcontext, pt, ilist, where_respill, DR_REG_XAX,
                                   AFLAGS_SLOT, false);
        gpr_info->slot_use[AFLAGS_SLOT] = DR_REG_NULL;
        return true;
    }
#endif
    return false;
}

/***************************************************************************
 * RESTORE STATE
 */

void
drreg_internal_aflag_restore_state_handle_spill(void *drcontext, byte *pc, uint slot,
                                                uint *spilled_to_aflags)
{
    if (slot == AFLAGS_SLOT)
        *spilled_to_aflags = slot;
    else {
        ASSERT(false, "slot should be of aflags");
        LOG(drcontext, DR_LOG_ALL, 3, "%s @" PFX ": ignoring restore\n", __FUNCTION__,
            pc);
    }
}

void
drreg_internal_aflag_restore_state_handle_restore(void *drcontext, byte *pc, uint slot,
                                                  uint *spilled_to_aflags)
{
    if (slot == AFLAGS_SLOT && *spilled_to_aflags == slot) {
        *spilled_to_aflags = MAX_SPILLS;
    } else {
        ASSERT(false, "slot should be of aflags");
        LOG(drcontext, DR_LOG_ALL, 3, "%s @" PFX ": ignoring restore\n", __FUNCTION__,
            pc);
    }
}

void
drreg_internal_aflag_restore_state_set_value(
    void *drcontext, dr_restore_state_info_t *info,
    uint spilled_to_aflags _IF_X86(bool aflags_in_xax))
{
    if (spilled_to_aflags < MAX_SPILLS IF_X86(|| aflags_in_xax)) {
        reg_t newval = info->mcontext->xflags;
        reg_t val;
#ifdef X86
        if (aflags_in_xax)
            val = info->mcontext->xax;
        else
#endif
            val = drreg_internal_get_spilled_gpr_value(
                drcontext, drreg_internal_tls_slot_offs, spilled_to_aflags);

        newval = dr_merge_arith_flags(newval, val);
        LOG(drcontext, DR_LOG_ALL, 3, "%s: restoring aflags from " PFX " to " PFX "\n",
            __FUNCTION__, info->mcontext->xflags, newval);
        info->mcontext->xflags = newval;
    }
}

/***************************************************************************
 * INIT AND EXIT
 */

void
drreg_internal_tls_aflag_data_init(drreg_internal_per_thread_t *pt)
{
    pt->aflags.native = true;
    drvector_init(&pt->aflags.live, 20, false /*!synch*/, NULL);
}

void
drreg_internal_tls_aflag_data_free(drreg_internal_per_thread_t *pt)
{
    drvector_delete(&pt->aflags.live);
}

/***************************************************************************
 * AFLAG API
 */

drreg_status_t
drreg_reserve_aflags(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    drreg_internal_per_thread_t *pt = drreg_internal_get_tls_data(drcontext);
    drreg_internal_reg_class_info_t *gpr_info = &pt->gpr_class_info;

    dr_pred_type_t pred = instrlist_get_auto_predicate(ilist);
    drreg_status_t res;
    uint aflags;
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        res = drreg_forward_analysis(drcontext, where);
        if (res != DRREG_SUCCESS)
            return res;
        ASSERT(pt->live_idx == 0, "non-drmgr-insert always uses 0 index");
    }
    aflags = (uint)(ptr_uint_t)drvector_get_entry(&pt->aflags.live, pt->live_idx);
    /* Just like scratch regs, flags are exclusively owned */
    if (pt->aflags.in_use)
        return DRREG_ERROR_IN_USE;
    if (!TESTANY(EFLAGS_READ_ARITH, aflags)) {
        /* If the flags were not yet lazily restored and are now dead, clear the slot */
        if (!pt->aflags.native)
            gpr_info->slot_use[AFLAGS_SLOT] = DR_REG_NULL;
        pt->aflags.in_use = true;
        pt->aflags.native = true;
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": aflags are dead\n", __FUNCTION__,
            pt->live_idx, get_where_app_pc(where));
        return DRREG_SUCCESS;
    }
    /* Check for a prior reservation not yet lazily restored */
    if (!pt->aflags.native IF_X86(||
                                  (gpr_info->reg[DR_REG_XAX - DR_REG_START_GPR].in_use &&
                                   pt->aflags.xchg == DR_REG_XAX))) {
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": using un-restored aflags\n",
            __FUNCTION__, pt->live_idx, get_where_app_pc(where));
        ASSERT(pt->aflags.xchg != DR_REG_NULL || gpr_info->slot_use[AFLAGS_SLOT] != DR_REG_NULL,
               "lost slot reservation");
        pt->aflags.native = false;
        pt->aflags.in_use = true;
        return DRREG_SUCCESS;
    }

    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": spilling aflags\n", __FUNCTION__,
        pt->live_idx, get_where_app_pc(where));
    /* drreg_spill_aflags writes to this, so clear first.  The inconsistent combo
     * xchg-null but xax-in-use won't happen b/c we'll use un-restored above.
     */
    pt->aflags.xchg = DR_REG_NULL;
    /* XXX i#2585: drreg should predicate spills and restores as appropriate */
    instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
    res = drreg_internal_spill_aflags(drcontext, pt, ilist, where);
    instrlist_set_auto_predicate(ilist, pred);
    if (res != DRREG_SUCCESS)
        return res;
    pt->aflags.in_use = true;
    pt->aflags.native = false;
    pt->aflags.slot = AFLAGS_SLOT;
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_unreserve_aflags(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    drreg_internal_per_thread_t *pt = drreg_internal_get_tls_data(drcontext);
    drreg_internal_reg_class_info_t *gpr_info = &pt->gpr_class_info;

    if (!pt->aflags.in_use)
        return DRREG_ERROR_INVALID_PARAMETER;
    pt->aflags.in_use = false;
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        dr_pred_type_t pred = instrlist_get_auto_predicate(ilist);
        /* We have no way to lazily restore.  We do not bother at this point
         * to try and eliminate back-to-back spill/restore pairs.
         */
        /* XXX i#2585: drreg should predicate spills and restores as appropriate */
        instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
        if (pt->aflags.xchg != DR_REG_NULL) {
            drreg_status_t res =
                drreg_internal_move_aflags_from_reg(drcontext, pt, ilist, where, true);
            if (res != DRREG_SUCCESS)
                return res;
        }

        else if (!pt->aflags.native) {
            drreg_internal_restore_aflags(drcontext, pt, ilist, where, true /*release*/);
            pt->aflags.native = true;
        }
        instrlist_set_auto_predicate(ilist, pred);
        gpr_info->slot_use[AFLAGS_SLOT] = DR_REG_NULL;
    }
    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX "\n", __FUNCTION__, pt->live_idx,
        get_where_app_pc(where));
    /* We lazily restore in drreg_event_bb_insert_late(), in case
     * someone else wants the aflags locally.
     */
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_aflags_liveness(void *drcontext, instr_t *inst, OUT uint *value)
{
    drreg_internal_per_thread_t *pt = drreg_internal_get_tls_data(drcontext);
    if (value == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        drreg_status_t res = drreg_forward_analysis(drcontext, inst);
        if (res != DRREG_SUCCESS)
            return res;
        ASSERT(pt->live_idx == 0, "non-drmgr-insert always uses 0 index");
    }
    *value = (uint)(ptr_uint_t)drvector_get_entry(&pt->aflags.live, pt->live_idx);
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_are_aflags_dead(void *drcontext, instr_t *inst, bool *dead)
{
    uint flags;
    drreg_status_t res = drreg_aflags_liveness(drcontext, inst, &flags);
    if (res != DRREG_SUCCESS)
        return res;
    if (dead == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;
    *dead = !TESTANY(EFLAGS_READ_ARITH, flags);
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_restore_app_aflags(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    drreg_internal_per_thread_t *pt = drreg_internal_get_tls_data(drcontext);
    drreg_status_t res = DRREG_SUCCESS;
    if (!pt->aflags.native) {
        dr_pred_type_t pred = instrlist_get_auto_predicate(ilist);
        LOG(drcontext, DR_LOG_ALL, 3,
            "%s @%d." PFX ": restoring app aflags as requested\n", __FUNCTION__,
            pt->live_idx, get_where_app_pc(where));
        /* XXX i#2585: drreg should predicate spills and restores as appropriate */
        instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
        res = drreg_internal_restore_aflags(drcontext, pt, ilist, where,
                                            !pt->aflags.in_use);
        instrlist_set_auto_predicate(ilist, pred);
        if (!pt->aflags.in_use)
            pt->aflags.native = true;
    }
    return res;
}
