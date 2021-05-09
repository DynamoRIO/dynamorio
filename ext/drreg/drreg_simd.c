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
#include "drreg_simd.h"
#include "drreg_gpr.h"
#include "../ext_utils.h"
#include <limits.h>
#include <string.h>

/***************************************************************************
 * SPILLING AND RESTORING
 */

/* Returns a free slot for storing the value of a SIMD vector register.
 * If no slots are available, the function returns MAX_SIMD_SPILLS as an indicator.*/
static uint
drreg_internal_find_simd_free_slot(drreg_internal_per_thread_t *pt)
{
    ASSERT(drreg_internal_ops.num_spill_simd_slots > 0,
           "cannot find free SIMD slots if non were initially requested");
    for (uint i = 0; i < drreg_internal_ops.num_spill_simd_slots; i++) {
        if (pt->simd_slot_use[i] == DR_REG_NULL)
            return i;
    }
    /* Failed to find a free slot. */
    return MAX_SIMD_SPILLS;
}

/* Spill slots for SIMD registers are not directly stored in addressable tls
 * but in an indirect block. The base pointer to this block, is indeed stored in
 * addressable tls. This function simply loads the pointer to a GPR.
 */
static void
drreg_internal_load_base_of_indirect_simd_block(void *drcontext,
                                                drreg_internal_per_thread_t *pt,
                                                instrlist_t *ilist, instr_t *where,
                                                reg_id_t scratch_block_reg)
{
    ASSERT(reg_is_gpr(scratch_block_reg), "base register must be a gpr");

    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX " %s %d\n", __FUNCTION__, pt->live_idx,
        get_where_app_pc(where), get_register_name(scratch_block_reg),
        drreg_internal_tls_simd_offs);
    /* Simply load the pointer of the block to the passed register. */
    dr_insert_read_raw_tls(drcontext, ilist, where, drreg_internal_tls_seg,
                           drreg_internal_tls_simd_offs, scratch_block_reg);
}

/* This routine is used for SIMD spills as such registers are indirectly
 * stored in a separately allocated area pointed to by a hidden tls slot.
 *
 * Up to caller to update pt->simd_reg, including .ever_spilled.
 * This routine updates pt->simd_slot_use.
 */
static drreg_status_t
drreg_internal_spill_simd_reg(void *drcontext, drreg_internal_per_thread_t *pt,
                              instrlist_t *ilist, instr_t *where, reg_id_t reg, uint slot)
{
    reg_id_t scratch_block_gpr = DR_REG_NULL;
    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX " %s %d\n", __FUNCTION__, pt->live_idx,
        get_where_app_pc(where), get_register_name(reg), slot);
    ASSERT(reg_is_vector_simd(reg), "not applicable register");
    ASSERT(pt->simd_slot_use[slot] == DR_REG_NULL || pt->simd_slot_use[slot] == reg,
           "internal tracking error");
    drreg_status_t res = drreg_internal_reserve_gpr(drcontext, pt, ilist, where, NULL,
                                                    false, &scratch_block_gpr);
    /* May fail if drreg runs out of regs to use as a temporary register. */
    if (res != DRREG_SUCCESS)
        return res;

    ASSERT(scratch_block_gpr != DR_REG_NULL, "invalid register");
    ASSERT(pt->simd_spills != NULL, "SIMD spill storage cannot be NULL");
    ASSERT(slot < drreg_internal_ops.num_spill_slots, "slots is out-of-bounds");

    drreg_internal_load_base_of_indirect_simd_block(drcontext, pt, ilist, where,
                                                    scratch_block_gpr);

    /* TODO i#3844: We need to be a bit careful in the future to take into account
     * mixing SIMD extension. Think Skylake, which incurs harsh penalties if you
     * mix SSE and AVX.
     */
    pt->simd_slot_use[slot] = reg;
    if (reg_is_strictly_xmm(reg)) {
        /* XXX: Use of SSE might cause harsh penalty on certain CPUs.
         */
        opnd_t mem_opnd = opnd_create_base_disp(scratch_block_gpr, DR_REG_NULL, 1,
                                                slot * SIMD_REG_SIZE, OPSZ_16);
        opnd_t spill_reg_opnd = opnd_create_reg(reg);
        PRE(ilist, where, INSTR_CREATE_movdqa(drcontext, mem_opnd, spill_reg_opnd));
    } else if (reg_is_strictly_ymm(reg)) {
        opnd_t mem_opnd = opnd_create_base_disp(scratch_block_gpr, DR_REG_NULL, 1,
                                                slot * SIMD_REG_SIZE, OPSZ_32);
        opnd_t spill_reg_opnd = opnd_create_reg(reg);
        PRE(ilist, where, INSTR_CREATE_vmovdqa(drcontext, mem_opnd, spill_reg_opnd));
    } else if (reg_is_strictly_zmm(reg)) {
        opnd_t mem_opnd = opnd_create_base_disp(scratch_block_gpr, DR_REG_NULL, 1,
                                                slot * SIMD_REG_SIZE, OPSZ_64);
        opnd_t spill_reg_opnd = opnd_create_reg(reg);
        PRE(ilist, where, INSTR_CREATE_vmovdqa(drcontext, mem_opnd, spill_reg_opnd));
    } else {
        ASSERT(false, "internal error: not applicable register");
    }
    /* We are done from using the base register. We can now unreserve it! */
    res = drreg_internal_unreserve_gpr(drcontext, pt, ilist, where, scratch_block_gpr);
    if (res != DRREG_SUCCESS)
        return res;

    return DRREG_SUCCESS;
}

/* Restores SIMD reg with spilled value stored in the indirect block.
 *
 * Up to caller to update pt->simd_reg. This routine updates pt->simd_slot_use if
 * release==true.
 */
static drreg_status_t
drreg_internal_restore_simd_reg(void *drcontext, drreg_internal_per_thread_t *pt,
                                instrlist_t *ilist, instr_t *where, reg_id_t reg,
                                uint slot, bool release)
{
    drreg_status_t res;
    reg_id_t scratch_block_gpr = DR_REG_NULL;
    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX " %s slot=%d release=%d\n", __FUNCTION__,
        pt->live_idx, get_where_app_pc(where), get_register_name(reg), slot, release);
    ASSERT(reg_is_vector_simd(reg), "not applicable register");
    ASSERT(pt->simd_slot_use[slot] == reg, "internal tracking error");
    res = drreg_internal_reserve_gpr(drcontext, pt, ilist, where, NULL, false,
                                     &scratch_block_gpr);
    // Check whether we failed to reserve scratch block register.
    if (res != DRREG_SUCCESS)
        return res;

    ASSERT(scratch_block_gpr != DR_REG_NULL, "invalid register");
    ASSERT(pt->simd_spills != NULL, "SIMD spill storage cannot be NULL");
    ASSERT(slot < drreg_internal_ops.num_spill_simd_slots, "slot is out-of-bounds");
    /* Load base register of indirect block. */
    drreg_internal_load_base_of_indirect_simd_block(drcontext, pt, ilist, where,
                                                    scratch_block_gpr);
    if (release && pt->simd_slot_use[slot] == reg)
        pt->simd_slot_use[slot] = DR_REG_NULL;

    opnd_t restore_reg_opnd = opnd_create_reg(reg);
    if (reg_is_strictly_xmm(reg)) {
        opnd_t mem_opnd = opnd_create_base_disp(scratch_block_gpr, DR_REG_NULL, 0,
                                                slot * SIMD_REG_SIZE, OPSZ_16);
        PRE(ilist, where, INSTR_CREATE_movdqa(drcontext, restore_reg_opnd, mem_opnd));
    } else if (reg_is_strictly_ymm(reg)) {
        opnd_t mem_opnd = opnd_create_base_disp(scratch_block_gpr, DR_REG_NULL, 0,
                                                slot * SIMD_REG_SIZE, OPSZ_32);
        PRE(ilist, where, INSTR_CREATE_vmovdqa(drcontext, restore_reg_opnd, mem_opnd));
    } else if (reg_is_strictly_zmm(reg)) {
        opnd_t mem_opnd = opnd_create_base_disp(scratch_block_gpr, DR_REG_NULL, 0,
                                                slot * SIMD_REG_SIZE, OPSZ_64);
        PRE(ilist, where, INSTR_CREATE_vmovdqa(drcontext, restore_reg_opnd, mem_opnd));
    } else {
        ASSERT(false, "internal error: not an applicable register.");
    }
    res = drreg_internal_unreserve_gpr(drcontext, pt, ilist, where, scratch_block_gpr);
    if (res != DRREG_SUCCESS)
        return res;

    return DRREG_SUCCESS;
}

void
drreg_internal_get_spilled_simd_value(void *drcontext, drreg_internal_per_thread_t *pt,
                                      reg_id_t reg, uint slot, OUT byte *value_buf,
                                      size_t buf_size)
{
    ASSERT(value_buf != NULL, "value buffer not initialised");
    ASSERT(reg_is_vector_simd(reg), "must be SIMD vector register");
    ASSERT(pt->simd_spills != NULL, "SIMD spill storage cannot be NULL");
    ASSERT(slot < drreg_internal_ops.num_spill_simd_slots, "slot is out-of-bounds");

    /* Get the size of the register so we can ensure that the buffer size is adequate. */
    size_t reg_size = opnd_size_in_bytes(reg_get_size(reg));
    ASSERT(buf_size >= reg_size, "value buffer too small in size");

    memcpy(value_buf, pt->simd_spills + (slot * SIMD_REG_SIZE), reg_size);
}

/***************************************************************************
 * ANALYSIS AND CROSS-APP-INSTR
 */

void
drreg_internal_increment_app_simd_use_count(drreg_internal_per_thread_t *pt, opnd_t opnd,
                                            reg_id_t reg)
{
    ASSERT(reg_is_vector_simd(reg), "register should be a vector SIMD");
    pt->simd_reg[SIMD_IDX(reg)].app_uses++;

    /* TODO i#3844: Increment uses again if SIMD register is used in scatter or gather
     * operations (i.e., VSIB operands).
     */
}

/* Returns true if instruction partially reads a SIMD register. */
static bool
drreg_internal_is_partial_simd_read(instr_t *instr, reg_id_t cmp_reg)
{
    ASSERT(reg_is_vector_simd(cmp_reg), "register should be a vector SIMD");

    for (int i = 0; i < instr_num_srcs(instr); i++) {
        opnd_t opnd = instr_get_src(instr, i);
        if (opnd_is_reg(opnd) && opnd_get_reg(opnd) == cmp_reg &&
            opnd_size_in_bytes(opnd_get_size(opnd)) <
                opnd_size_in_bytes(reg_get_size(cmp_reg)))
            return true;
    }
    return false;
}

/* Returns true if state has been set to value. */
static bool
drreg_internal_get_simd_liveness_state(instr_t *inst, reg_id_t reg, OUT void **value)
{
    ASSERT(value != NULL, "cannot be NULL");
    ASSERT(reg_is_vector_simd(reg), "must be a vector SIMD register");

    /* Reason over partial registers in SIMD case to achieve efficient spilling. */
    reg_id_t xmm_reg = reg_resize_to_opsz(reg, OPSZ_16);
    reg_id_t ymm_reg = reg_resize_to_opsz(reg, OPSZ_32);
    reg_id_t zmm_reg = reg_resize_to_opsz(reg, OPSZ_64);

    /* It is important to give precedence to bigger registers.
     * If both ZMM0 and YMM0 are read and therefore live, then
     * SIMD_ZMM_LIVE must be assigned and not SIMD_YMM_LIVE.
     *
     * The same applies for dead registers. If both
     * ZMM0 and YMM0 are dead, then SIMD_ZMM_DEAD must be
     * assigned and not SIMD_YMM_DEAD.
     *
     * If XMM is live but the upper bits of the YMM/ZMM register are dead,
     * potentially due to zero clearance, then SIMD_XMM_LIVE is assigned.
     *
     * This is important in order to achieve efficient spilling/restoring.
     */
    if (instr_reads_from_reg(inst, zmm_reg, DR_QUERY_INCLUDE_COND_SRCS)) {
        if ((instr_reads_from_exact_reg(inst, zmm_reg, DR_QUERY_INCLUDE_COND_SRCS) ||
             drreg_internal_is_partial_simd_read(inst, zmm_reg)) &&
            (*value <= SIMD_ZMM_LIVE || *value == SIMD_UNKNOWN)) {
            *value = SIMD_ZMM_LIVE;
        } else if ((instr_reads_from_exact_reg(inst, ymm_reg,
                                               DR_QUERY_INCLUDE_COND_SRCS) ||
                    drreg_internal_is_partial_simd_read(inst, ymm_reg)) &&
                   (*value <= SIMD_YMM_LIVE || *value == SIMD_UNKNOWN)) {
            *value = SIMD_YMM_LIVE;
        } else if ((instr_reads_from_exact_reg(inst, xmm_reg,
                                               DR_QUERY_INCLUDE_COND_SRCS) ||
                    drreg_internal_is_partial_simd_read(inst, xmm_reg)) &&
                   (*value <= SIMD_XMM_LIVE || *value == SIMD_UNKNOWN)) {
            *value = SIMD_XMM_LIVE;
        } else {
            DR_ASSERT_MSG(false, "failed to handle SIMD read");
            *value = SIMD_ZMM_LIVE;
        }
        return true;
    }

    if (instr_writes_to_reg(inst, zmm_reg, DR_QUERY_INCLUDE_COND_SRCS)) {
        if (instr_writes_to_exact_reg(inst, zmm_reg, DR_QUERY_INCLUDE_COND_SRCS)) {
            *value = SIMD_ZMM_DEAD;
            return true;
        } else if (instr_writes_to_exact_reg(inst, ymm_reg, DR_QUERY_INCLUDE_COND_SRCS) &&
                   (*value < SIMD_YMM_DEAD || *value >= SIMD_XMM_LIVE)) {

            /* The instr should be VEX/EVEX encoding, where upper bits are cleared.
             * Therefore, SIMD_ZMM_DEAD should be assigned.
             */
            *value = SIMD_YMM_DEAD;
            return true;
        } else if (instr_writes_to_exact_reg(inst, xmm_reg, DR_QUERY_INCLUDE_COND_SRCS) &&
                   *value >= SIMD_XMM_LIVE) {

            if (instr_zeroes_ymmh(inst))
                *value = SIMD_YMM_DEAD;
            else
                *value = SIMD_XMM_DEAD;

            return true;
        }
        /* Note, we may partially write to above registers, which does not make them dead.
         */
    }
    return false;
}

void
drreg_internal_bb_init_simd_liveness_analysis(drreg_internal_per_thread_t *pt)
{
    for (reg_id_t reg = DR_REG_APPLICABLE_START_SIMD; reg <= DR_REG_APPLICABLE_STOP_SIMD;
         reg++)
        pt->simd_reg[SIMD_IDX(reg)].app_uses = 0;
}

void
drreg_internal_bb_analyse_simd_liveness(void *drcontext, drreg_internal_per_thread_t *pt,
                                        instr_t *inst, uint index)
{
    for (reg_id_t reg = DR_REG_APPLICABLE_START_SIMD; reg <= DR_REG_APPLICABLE_STOP_SIMD;
         reg++) {
        void *value = SIMD_UNKNOWN;
        if (!drreg_internal_get_simd_liveness_state(inst, reg, &value)) {
            if (drreg_internal_is_xfer(inst))
                value = SIMD_ZMM_LIVE;
            else if (index > 0) {
                value = drvector_get_entry(&pt->simd_reg[SIMD_IDX(reg)].live, index - 1);
            }
        }
        LOG(drcontext, DR_LOG_ALL, 3, " %s=%d", get_register_name(reg),
            (int)(ptr_uint_t)value);
        drvector_set_entry(&pt->simd_reg[SIMD_IDX(reg)].live, index, value);
    }
}

drreg_status_t
drreg_internal_bb_insert_simd_restore_all(void *drcontext,
                                          drreg_internal_per_thread_t *pt,
                                          instrlist_t *bb, instr_t *inst,
                                          bool force_restore,
                                          OUT bool *simd_regs_restored)
{
    instr_t *next = instr_get_next(inst);
    drreg_status_t res;

    for (reg_id_t reg = DR_REG_APPLICABLE_START_SIMD; reg <= DR_REG_APPLICABLE_STOP_SIMD;
         reg++) {
        if (simd_regs_restored != NULL)
            simd_regs_restored[SIMD_IDX(reg)] = false;

        if (!pt->simd_reg[SIMD_IDX(reg)].native) {
            ASSERT(drreg_internal_ops.num_spill_simd_slots > 0,
                   "requested SIMD slots cannot be zero");

            if (force_restore ||
                /* This covers reads from all simds, because the applicable range
                 * resembles zmm, and all other x86 simds are included in zmm.
                 */
                instr_reads_from_reg(inst, reg, DR_QUERY_INCLUDE_ALL) ||
                /* FIXME i#3844: For ymm and zmm support, we're missing support to restore
                 * upon a partial simd write. For example a write to xmm while zmm is
                 * clobbered, or a partial write with an evex mask.
                 */
                /* i#1954: for complex bbs we must restore before the next app instr. */
                (!pt->simd_reg[SIMD_IDX(reg)].in_use &&
                 ((pt->bb_has_internal_flow &&
                   !TEST(DRREG_IGNORE_CONTROL_FLOW, pt->bb_props)) ||
                  TEST(DRREG_CONTAINS_SPANNING_CONTROL_FLOW, pt->bb_props)))) {

                if (!pt->simd_reg[SIMD_IDX(reg)].in_use) {
                    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": lazily restoring %s\n",
                        __FUNCTION__, pt->live_idx, get_where_app_pc(inst),
                        get_register_name(reg));
                    res =
                        drreg_internal_restore_simd_reg_now(drcontext, pt, bb, inst, reg);
                    if (res != DRREG_SUCCESS)
                        return res;
                    ASSERT(pt->simd_pending_unreserved > 0, "should not go negative");
                    pt->simd_pending_unreserved--;
                } else {
                    uint slot = pt->simd_reg[SIMD_IDX(reg)].slot;
                    ASSERT(drreg_internal_ops.num_spill_simd_slots,
                           "slot is out-of-bounds");
                    reg_id_t spilled_reg = pt->simd_slot_use[slot];
                    ASSERT(spilled_reg != DR_REG_NULL, "invalid spilled reg");
                    uint tmp_slot = drreg_internal_find_simd_free_slot(pt);
                    if (tmp_slot == MAX_SIMD_SPILLS) {
                        return DRREG_ERROR_OUT_OF_SLOTS;
                    }

                    LOG(drcontext, DR_LOG_ALL, 3,
                        "%s @%d." PFX ": restoring %s for app read\n", __FUNCTION__,
                        pt->live_idx, get_where_app_pc(inst), get_register_name(reg));
                    drreg_internal_spill_simd_reg(drcontext, pt, bb, inst, spilled_reg,
                                                  tmp_slot);
                    drreg_internal_restore_simd_reg(drcontext, pt, bb, inst, spilled_reg,
                                                    slot, false /*keep slot*/);
                    drreg_internal_restore_simd_reg(drcontext, pt, bb, next, spilled_reg,
                                                    tmp_slot, true /*don't keep slot*/);

                    /* We keep .native==false */
                    /* Share the tool val spill if this inst writes, too. */
                    if (simd_regs_restored != NULL)
                        simd_regs_restored[SIMD_IDX(reg)] = true;
                }
            }
        }
    }

    return DRREG_SUCCESS;
}

drreg_status_t
drreg_internal_bb_insert_simd_update_spill(void *drcontext,
                                           drreg_internal_per_thread_t *pt,
                                           instrlist_t *bb, instr_t *inst,
                                           bool *simd_restored_for_read)
{
    instr_t *next = instr_get_next(inst);
    drreg_status_t res;

    for (reg_id_t reg = DR_REG_APPLICABLE_START_SIMD; reg <= DR_REG_APPLICABLE_STOP_SIMD;
         reg++) {
        if (pt->simd_reg[SIMD_IDX(reg)].in_use) {
            void *state =
                drvector_get_entry(&pt->simd_reg[SIMD_IDX(reg)].live, pt->live_idx - 1);
            uint slot = pt->simd_reg[SIMD_IDX(reg)].slot;
            ASSERT(slot < drreg_internal_ops.num_spill_simd_slots,
                   "slot is out-of-bounds");
            reg_id_t spilled_reg = pt->simd_slot_use[slot];
            ASSERT(spilled_reg != DR_REG_NULL, "invalid spilled reg");

            if (instr_writes_to_reg(inst, reg, DR_QUERY_INCLUDE_ALL) &&
                /* Don't bother if reg is dead beyond this write. */
                (drreg_internal_ops.conservative || pt->live_idx == 0 ||
                 !((reg_is_strictly_xmm(spilled_reg) && state >= SIMD_XMM_DEAD &&
                    state <= SIMD_ZMM_DEAD) ||
                   (reg_is_strictly_ymm(spilled_reg) && state >= SIMD_YMM_DEAD &&
                    state <= SIMD_ZMM_DEAD) ||
                   (reg_is_strictly_zmm(spilled_reg) && state == SIMD_ZMM_DEAD)))) {
                ASSERT(drreg_internal_ops.num_spill_simd_slots > 0,
                       "requested SIMD slots cannot be zero");
                uint tmp_slot = MAX_SIMD_SPILLS;
                if (!simd_restored_for_read[SIMD_IDX(reg)]) {
                    tmp_slot = drreg_internal_find_simd_free_slot(pt);
                    if (tmp_slot == MAX_SIMD_SPILLS)
                        return DRREG_ERROR_OUT_OF_SLOTS;

                    drreg_internal_spill_simd_reg(drcontext, pt, bb, inst, spilled_reg,
                                                  tmp_slot);
                }

                instr_t *where =
                    simd_restored_for_read[SIMD_IDX(reg)] ? instr_get_prev(next) : next;
                drreg_internal_spill_simd_reg(drcontext, pt, bb, where, spilled_reg,
                                              slot);
                pt->simd_reg[SIMD_IDX(reg)].ever_spilled = true;
                if (!simd_restored_for_read[SIMD_IDX(reg)]) {
                    drreg_internal_restore_simd_reg(drcontext, pt, bb, next /*after*/,
                                                    spilled_reg, tmp_slot,
                                                    true /*don't keep slot*/);
                }
            }
        } else if (!pt->simd_reg[SIMD_IDX(reg)].native &&
                   instr_writes_to_reg(inst, reg, DR_QUERY_INCLUDE_ALL)) {
            /* For an unreserved reg that's written, just drop the slot, even
             * if it was spilled at an earlier reservation point.
             */
            if (pt->simd_reg[SIMD_IDX(reg)].ever_spilled)
                pt->simd_reg[SIMD_IDX(reg)].ever_spilled = false; /* no need to restore */
            res = drreg_internal_restore_simd_reg_now(drcontext, pt, bb, inst, reg);
            if (res != DRREG_SUCCESS)
                return res;
            pt->simd_pending_unreserved--;
        }
    }

    return DRREG_SUCCESS;
}

drreg_status_t
drreg_internal_restore_simd_app_value(void *drcontext, drreg_internal_per_thread_t *pt,
                                      instrlist_t *ilist, instr_t *where,
                                      reg_id_t app_reg, reg_id_t dst_reg, bool stateful)
{
    if (reg_is_strictly_xmm(app_reg) || reg_is_strictly_xmm(dst_reg))
        return DRREG_ERROR_INVALID_PARAMETER;

    /* Check if app_reg is an unspilled reg. */
    if (pt->simd_reg[SIMD_IDX(app_reg)].native) {
        if (dst_reg != app_reg) {
            PRE(ilist, where,
                INSTR_CREATE_movdqa(drcontext, opnd_create_reg(dst_reg),
                                    opnd_create_reg(app_reg)));
        }
        return DRREG_SUCCESS;
    }
    /* We may have lost the app value for a dead reg. */
    if (!pt->simd_reg[SIMD_IDX(app_reg)].ever_spilled) {
        return DRREG_ERROR_NO_APP_VALUE;
    }
    /* Restore the app value back to app_reg. */
    if (pt->simd_reg[SIMD_IDX(app_reg)].xchg != DR_REG_NULL) {
        /* XXX i#511: NYI */
        return DRREG_ERROR_FEATURE_NOT_AVAILABLE;
    }
    drreg_internal_restore_simd_reg(drcontext, pt, ilist, where, app_reg,
                                    pt->simd_reg[SIMD_IDX(app_reg)].slot,
                                    stateful && !pt->simd_reg[SIMD_IDX(app_reg)].in_use);
    if (stateful && !pt->simd_reg[SIMD_IDX(app_reg)].in_use)
        pt->simd_reg[SIMD_IDX(app_reg)].native = true;

    return DRREG_SUCCESS;
}

/***************************************************************************
 * USE OUTSIDE INSERT PHASE
 */

void
drreg_internal_init_forward_simd_liveness_analysis(drreg_internal_per_thread_t *pt)
{
    /* If we could not determine state (i.e. unknown), we set the state to live. */
    for (reg_id_t reg = DR_REG_APPLICABLE_START_SIMD; reg <= DR_REG_APPLICABLE_STOP_SIMD;
         reg++) {
        pt->simd_reg[SIMD_IDX(reg)].app_uses = 0;
        drvector_set_entry(&pt->simd_reg[SIMD_IDX(reg)].live, 0, SIMD_UNKNOWN);
    }
}

void
drreg_internal_forward_analyse_simd_liveness(drreg_internal_per_thread_t *pt,
                                             instr_t *inst)
{
    for (reg_id_t reg = DR_REG_APPLICABLE_START_SIMD; reg <= DR_REG_APPLICABLE_STOP_SIMD;
         reg++) {
        void *value = SIMD_UNKNOWN;
        if (drvector_get_entry(&pt->simd_reg[SIMD_IDX(reg)].live, 0) != SIMD_UNKNOWN)
            continue;

        drreg_internal_get_simd_liveness_state(inst, reg, &value);
        if (value != SIMD_UNKNOWN)
            drvector_set_entry(&pt->simd_reg[SIMD_IDX(reg)].live, 0, value);
    }
}

void
drreg_internal_finalise_forward_simd_liveness_analysis(drreg_internal_per_thread_t *pt)
{
    /* If we could not determine state (i.e. unknown), we set the state to live. */
    for (reg_id_t reg = DR_REG_APPLICABLE_START_SIMD; reg <= DR_REG_APPLICABLE_STOP_SIMD;
         reg++) {
        if (drvector_get_entry(&pt->simd_reg[SIMD_IDX(reg)].live, 0) == SIMD_UNKNOWN)
            drvector_set_entry(&pt->simd_reg[SIMD_IDX(reg)].live, 0, SIMD_ZMM_LIVE);
    }
}

/***************************************************************************
 * REGISTER RESERVATION
 */

/* Reflects that any simd live state has to be considered live for a given spill_class. */
static bool
is_simd_live(void *live_state, const drreg_spill_class_t spill_class /* unused */)
{
    return live_state >= SIMD_XMM_LIVE && live_state <= SIMD_ZMM_LIVE;
}

drreg_status_t
drreg_internal_is_simd_reg_dead(drreg_internal_per_thread_t *pt,
                                drreg_spill_class_t spill_class, reg_id_t reg, bool *dead)
{
    if (dead == NULL || !reg_is_vector_simd(reg) ||
        spill_class < DRREG_SIMD_XMM_SPILL_CLASS ||
        DRREG_SIMD_ZMM_SPILL_CLASS < spill_class)
        return DRREG_ERROR_INVALID_PARAMETER;

    void *cur_state = drvector_get_entry(&pt->simd_reg[SIMD_IDX(reg)].live, pt->live_idx);

    void *cmp_dead_state = SIMD_UNKNOWN;

    switch (spill_class) {
    case DRREG_SIMD_XMM_SPILL_CLASS: cmp_dead_state = SIMD_XMM_DEAD; break;
    case DRREG_SIMD_YMM_SPILL_CLASS: cmp_dead_state = SIMD_YMM_DEAD; break;
    case DRREG_SIMD_ZMM_SPILL_CLASS: cmp_dead_state = SIMD_ZMM_DEAD; break;
    default: return DRREG_ERROR;
    }

    *dead = cur_state >= cmp_dead_state && cur_state <= SIMD_ZMM_DEAD;
    return DRREG_SUCCESS;
}

/* Makes the same assumptions about liveness info being already computed as
 * drreg_reserve_gpr_internal().
 */
static drreg_status_t
drreg_internal_find_for_simd_reservation(void *drcontext, drreg_internal_per_thread_t *pt,
                                         drreg_spill_class_t spill_class,
                                         instrlist_t *ilist, instr_t *where,
                                         drvector_t *reg_allowed, bool only_if_no_spill,
                                         OUT uint *slot_out, OUT reg_id_t *reg_out,
                                         OUT bool *already_spilled_out)
{
    drreg_status_t res;
    uint min_uses = UINT_MAX;
    uint slot = MAX_SIMD_SPILLS;
    reg_id_t best_reg = DR_REG_NULL;
    bool already_spilled = false;
    bool is_dead = false;
    if (drreg_internal_ops.num_spill_simd_slots == 0)
        return DRREG_ERROR;
    reg_id_t reg = (reg_id_t)DR_REG_APPLICABLE_STOP_SIMD + 1;
    if (pt->simd_pending_unreserved > 0) {

        /* Iterate through not-in-use reserved, to see whether we can use an existing
         * slot. */
        for (reg = DR_REG_APPLICABLE_START_SIMD; reg <= DR_REG_APPLICABLE_STOP_SIMD;
             reg++) {

            reg_id_t real_reg;
            if (spill_class == DRREG_SIMD_XMM_SPILL_CLASS) {
                real_reg = reg_resize_to_opsz(reg, OPSZ_16);
            } else if (spill_class == DRREG_SIMD_YMM_SPILL_CLASS) {
                real_reg = reg_resize_to_opsz(reg, OPSZ_32);
            } else if (spill_class == DRREG_SIMD_ZMM_SPILL_CLASS) {
                real_reg = reg_resize_to_opsz(reg, OPSZ_64);
            } else {
                return DRREG_ERROR;
            }

            res = drreg_internal_is_simd_reg_dead(pt, spill_class, reg, &is_dead);
            if (res != DRREG_SUCCESS)
                return res;

            uint idx = SIMD_IDX(reg);
            if (!pt->simd_reg[idx].native && !pt->simd_reg[idx].in_use &&
                (reg_allowed == NULL || drvector_get_entry(reg_allowed, idx) != NULL) &&
                (!only_if_no_spill || pt->simd_reg[idx].ever_spilled || is_dead)) {
                /* Slot found. We can break out of the loop. */
                slot = pt->simd_reg[idx].slot;
                pt->simd_pending_unreserved--;
                ASSERT(slot < drreg_internal_ops.num_spill_simd_slots,
                       "slot is out-of-bounds");
                reg_id_t spilled_reg = pt->simd_slot_use[slot];
                already_spilled =
                    pt->simd_reg[idx].ever_spilled && spilled_reg == real_reg;
                break;
            }
        }
    }

    /* If we failed, look for a dead register, or the least-used register. */
    if (reg > DR_REG_APPLICABLE_STOP_SIMD) {
        for (reg = DR_REG_APPLICABLE_START_SIMD; reg <= DR_REG_APPLICABLE_STOP_SIMD;
             ++reg) {
            uint idx = SIMD_IDX(reg);
            if (pt->simd_reg[idx].in_use)
                continue;
            if (reg_allowed != NULL && drvector_get_entry(reg_allowed, idx) == NULL)
                continue;

            res = drreg_internal_is_simd_reg_dead(pt, spill_class, reg, &is_dead);
            if (res != DRREG_SUCCESS)
                return res;

            if (is_dead)
                break;
            if (only_if_no_spill)
                continue;
            /* Keep track of least used. */
            if (pt->simd_reg[idx].app_uses < min_uses) {
                best_reg = reg;
                min_uses = pt->simd_reg[idx].app_uses;
            }
        }
    }

    if (reg > DR_REG_APPLICABLE_STOP_SIMD) {
        if (best_reg != DR_REG_NULL)
            reg = best_reg;
        else
            return DRREG_ERROR_REG_CONFLICT;
    }
    if (slot == MAX_SIMD_SPILLS) {
        slot = drreg_internal_find_simd_free_slot(pt);
        if (slot == MAX_SIMD_SPILLS)
            return DRREG_ERROR_OUT_OF_SLOTS;
    }
    ASSERT(reg_is_vector_simd(reg), "register must be a simd vector");
    if (spill_class == DRREG_SIMD_XMM_SPILL_CLASS) {
        reg = reg_resize_to_opsz(reg, OPSZ_16);
    } else if (spill_class == DRREG_SIMD_YMM_SPILL_CLASS) {
        reg = reg_resize_to_opsz(reg, OPSZ_32);
    } else if (spill_class == DRREG_SIMD_ZMM_SPILL_CLASS) {
        reg = reg_resize_to_opsz(reg, OPSZ_64);
    } else {
        return DRREG_ERROR;
    }
    *slot_out = slot;
    *reg_out = reg;
    *already_spilled_out = already_spilled;
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_internal_reserve_simd_reg(void *drcontext, drreg_internal_per_thread_t *pt,
                                drreg_spill_class_t spill_class, instrlist_t *ilist,
                                instr_t *where, drvector_t *reg_allowed,
                                bool only_if_no_spill, OUT reg_id_t *reg_out)
{
    uint slot = 0;
    reg_id_t reg = DR_REG_NULL;
    bool already_spilled = false;
    drreg_status_t res;
    res = drreg_internal_find_for_simd_reservation(drcontext, pt, spill_class, ilist,
                                                   where, reg_allowed, only_if_no_spill,
                                                   &slot, &reg, &already_spilled);
    if (res != DRREG_SUCCESS)
        return res;

    /* We found a suitable reg. We now need to spill. */
    ASSERT(!pt->simd_reg[SIMD_IDX(reg)].in_use, "overlapping uses");
    pt->simd_reg[SIMD_IDX(reg)].in_use = true;
    if (!already_spilled) {
        /* Even if dead now, we need to own a slot in case reserved past dead point. */
        if (drreg_internal_ops.conservative ||
            is_simd_live(
                drvector_get_entry(&pt->simd_reg[SIMD_IDX(reg)].live, pt->live_idx),
                spill_class)) {
            LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": spilling %s to slot %d\n",
                __FUNCTION__, pt->live_idx, get_where_app_pc(where),
                get_register_name(reg), slot);
            drreg_internal_spill_simd_reg(drcontext, pt, ilist, where, reg, slot);
            pt->simd_reg[SIMD_IDX(reg)].ever_spilled = true;
        } else {
            LOG(drcontext, DR_LOG_ALL, 3,
                "%s @%d." PFX ": no need to spill %s to slot %d\n", __FUNCTION__,
                pt->live_idx, get_where_app_pc(where), get_register_name(reg), slot);
            pt->simd_slot_use[slot] = reg;
            pt->simd_reg[SIMD_IDX(reg)].ever_spilled = false;
        }
    } else {
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": %s already spilled to slot %d\n",
            __FUNCTION__, pt->live_idx, get_where_app_pc(where), get_register_name(reg),
            slot);
    }

    pt->simd_reg[SIMD_IDX(reg)].native = false;
    pt->simd_reg[SIMD_IDX(reg)].xchg = DR_REG_NULL;
    pt->simd_reg[SIMD_IDX(reg)].slot = slot;
    *reg_out = reg;
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_internal_restore_simd_app_values(void *drcontext, instrlist_t *ilist,
                                       instr_t *where, opnd_t opnd,
                                       OUT bool *no_app_value)
{
    drreg_status_t res;
    int num_op = opnd_num_regs_used(opnd);

    for (int i = 0; i < num_op; i++) {
        reg_id_t reg = opnd_get_reg_used(opnd, i);
        reg_id_t dst;
        if (!reg_is_vector_simd(reg))
            continue;
        dst = reg;
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
drreg_internal_restore_simd_reg_now(void *drcontext, drreg_internal_per_thread_t *pt,
                                    instrlist_t *ilist, instr_t *inst, reg_id_t reg)
{
    if (!reg_is_vector_simd(reg))
        return DRREG_ERROR_INVALID_PARAMETER;

    if (pt->simd_reg[SIMD_IDX(reg)].ever_spilled) {
        reg_id_t spilled_reg = pt->simd_slot_use[pt->simd_reg[SIMD_IDX(reg)].slot];
        drreg_status_t res = drreg_internal_restore_simd_reg(
            drcontext, pt, ilist, inst, spilled_reg, pt->simd_reg[SIMD_IDX(reg)].slot,
            true /*don't keep slot*/);
        if (res != DRREG_SUCCESS)
            return res;

    } else {
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": %s never spilled\n", __FUNCTION__,
            pt->live_idx, get_where_app_pc(inst), get_register_name(reg));
        pt->simd_slot_use[pt->simd_reg[SIMD_IDX(reg)].slot] = DR_REG_NULL;
    }

    /* SIMD register is now restored. We therefore set native flag. */
    pt->simd_reg[SIMD_IDX(reg)].native = true;

    return DRREG_SUCCESS;
}

drreg_status_t
drreg_internal_unreserve_simd_reg(void *drcontext, drreg_internal_per_thread_t *pt,
                                  instrlist_t *ilist, instr_t *where, reg_id_t reg)
{
    if (!reg_is_vector_simd(reg) || !pt->simd_reg[SIMD_IDX(reg)].in_use)
        return DRREG_ERROR_INVALID_PARAMETER;

    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        /* We have no way to lazily restore.  We do not bother at this point
         * to try and eliminate back-to-back spill/restore pairs.
         */
        dr_pred_type_t pred = instrlist_get_auto_predicate(ilist);
        drreg_status_t res;
        /* XXX i#2585: drreg should predicate spills and restores as appropriate */
        instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
        res = drreg_internal_restore_simd_reg_now(drcontext, pt, ilist, where, reg);
        instrlist_set_auto_predicate(ilist, pred);
        if (res != DRREG_SUCCESS)
            return res;
    } else {
        /* We lazily restore in drreg_event_bb_insert_late(), in case
         * someone else wants a local scratch.
         */
        pt->simd_pending_unreserved++;
    }
    pt->simd_reg[SIMD_IDX(reg)].in_use = false;

    return DRREG_SUCCESS;
}

drreg_status_t
drreg_internal_init_and_fill_simd_vector(drvector_t *vec, bool allowed)
{
    uint size = DR_NUM_SIMD_VECTOR_REGS;

    if (vec == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;

    drvector_init(vec, size, false /*!synch*/, NULL);
    for (reg_id_t reg = 0; reg < size; reg++)
        drvector_set_entry(vec, reg, allowed ? (void *)(ptr_uint_t)1 : NULL);
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_internal_set_simd_vector_entry(drvector_t *vec, reg_id_t reg, bool allowed)
{
    reg_id_t start_reg = DR_REG_APPLICABLE_START_SIMD;

    /* We assume that the SIMD range is contiguous and no further out of range checks
     * are performed. In part, this assumption is made as we
     * resize the SIMD register.
     */
    if (vec == NULL || !reg_is_vector_simd(reg))
        return DRREG_ERROR_INVALID_PARAMETER;

    reg = reg_resize_to_opsz(reg, OPSZ_64);

    drvector_set_entry(vec, reg - start_reg, allowed ? (void *)(ptr_uint_t)1 : NULL);
    return DRREG_SUCCESS;
}

/***************************************************************************
 * RESTORE STATE
 */

bool
drreg_internal_is_simd_spill_or_restore(void *drcontext, instr_t *instr, bool is_tls,
                                        uint offs, bool *is_spilled INOUT,
                                        reg_id_t *reg_spilled OUT, uint *slot_out OUT)
{
    ASSERT(is_spilled != NULL, "invalid parameter");
    ASSERT(reg_spilled != NULL, "invalid parameter");
    ASSERT(slot_out != NULL, "invalid parameter");

#ifdef DEBUG
    bool dbg_is_tls;
    uint dbg_offs;
    bool dbg_is_spilled;
    if (!instr_is_reg_spill_or_restore(drcontext, instr, &dbg_is_tls, &dbg_is_spilled,
                                       reg_spilled, &dbg_offs))
        ASSERT(false, "instr should be a spill or restore");
    ASSERT(dbg_is_tls == is_tls, "is_tls should match");
    ASSERT(dbg_offs == offs, "offs should match");
    ASSERT(*is_spilled == dbg_is_spilled, "is_spilled should match");
#endif

    if (is_tls && offs == drreg_internal_tls_simd_offs &&
        !(*is_spilled) /* Can't be a spill bc loading block. */) {
        /* In order to detect indirect spills, the loading of the pointer
         * to the indirect block must be done exactly prior to the spill.
         * We assume that nobody else can interfere with our indirect load
         * sequence for SIMD registers.
         */
        instr_t *next_instr = instr_get_next(instr);
        ASSERT(next_instr != NULL, "next_instr cannot be NULL");
        /* FIXME i#3844: Might need to change this assert when
         * supporting other register spillage.
         */
        ASSERT(instr_get_opcode(next_instr) == OP_movdqa,
               "next instruction needs to be a mov");
        opnd_t dst = instr_get_dst(next_instr, 0);
        opnd_t src = instr_get_src(next_instr, 0);

        if (opnd_is_reg(dst) && reg_is_vector_simd(opnd_get_reg(dst)) &&
            opnd_is_base_disp(src)) {
            *reg_spilled = opnd_get_reg(dst);
            *is_spilled = false;
            int disp = opnd_get_disp(src);
            /* Each slot here is of size SIMD_REG_SIZE. We perform a division to get the
             * slot based on the displacement.
             */
            *slot_out = disp / SIMD_REG_SIZE;
        } else if (opnd_is_reg(src) && reg_is_vector_simd(opnd_get_reg(src)) &&
                   opnd_is_base_disp(dst)) {
            *reg_spilled = opnd_get_reg(src);
            *is_spilled = true;
            int disp = opnd_get_disp(dst);
            *slot_out = disp / SIMD_REG_SIZE;
        } else {
            ASSERT(false, "use of block must involve a load/store");
            return false;
        }

        return true;
    }

    return false;
}

void
drreg_internal_simd_restore_state_init(uint *spilled_simd_to, reg_id_t *simd_slot_use)
{
    ASSERT(spilled_simd_to != NULL, "shouldn't be NULL");
    ASSERT(simd_slot_use != NULL, "shouldn't be NULL");

    for (reg_id_t reg = DR_REG_APPLICABLE_START_SIMD; reg <= DR_REG_APPLICABLE_STOP_SIMD;
         reg++)
        spilled_simd_to[SIMD_IDX(reg)] = MAX_SIMD_SPILLS;
    for (uint slot = 0; slot < drreg_internal_ops.num_spill_simd_slots; ++slot)
        simd_slot_use[slot] = DR_REG_NULL;
}

void
drreg_internal_simd_restore_state_handle_spill(void *drcontext, byte *pc, uint slot,
                                               reg_id_t reg, uint *spilled_simd_to,
                                               reg_id_t *simd_slot_use)
{
    ASSERT(reg_is_vector_simd(reg), "indirect spill must be for SIMD reg");
    ASSERT(slot < drreg_internal_ops.num_spill_simd_slots, "slot is out-of-bounds");
    if (spilled_simd_to[SIMD_IDX(reg)] < MAX_SIMD_SPILLS &&
        /* allow redundant spill */
        spilled_simd_to[SIMD_IDX(reg)] != slot) {
        /* This reg is already spilled: we assume that this new
         * spill is to a tmp slot for preserving the tool's value.
         */
        LOG(drcontext, DR_LOG_ALL, 3, "%s @" PFX ": ignoring tool is_spill\n",
            __FUNCTION__, pc);
    } else {
        spilled_simd_to[SIMD_IDX(reg)] = slot;
        simd_slot_use[slot] = reg;
    }
}

void
drreg_internal_simd_restore_state_handle_restore(void *drcontext, byte *pc, uint slot,
                                                 reg_id_t reg, uint *spilled_simd_to,
                                                 reg_id_t *simd_slot_use)
{
    if (spilled_simd_to[SIMD_IDX(reg)] == slot) {
        spilled_simd_to[SIMD_IDX(reg)] = MAX_SIMD_SPILLS;
        simd_slot_use[slot] = DR_REG_NULL;

    } else {
        LOG(drcontext, DR_LOG_ALL, 3, "%s @" PFX ": ignoring restore\n", __FUNCTION__,
            pc);
    }
}

void
drreg_internal_simd_restore_state_set_values(void *drcontext,
                                             drreg_internal_per_thread_t *pt,
                                             dr_restore_state_info_t *info,
                                             uint *spilled_simd_to,
                                             reg_id_t *simd_slot_use)
{
    uint slot;
    byte simd_buf[SIMD_REG_SIZE];

    for (reg_id_t reg = DR_REG_APPLICABLE_START_SIMD; reg <= DR_REG_APPLICABLE_STOP_SIMD;
         reg++) {
        slot = spilled_simd_to[SIMD_IDX(reg)];
        if (slot < MAX_SIMD_SPILLS) {
            ASSERT(slot < drreg_internal_ops.num_spill_simd_slots,
                   "slots is out-of-bounds");
            reg_id_t actualreg = simd_slot_use[slot];
            ASSERT(actualreg != DR_REG_NULL, "internal error, register should be valid");
            ASSERT(actualreg != DR_REG_NULL, "register should be a SIMD vector register");
            size_t reg_size = opnd_size_in_bytes(reg_get_size(actualreg));
            drreg_internal_get_spilled_simd_value(drcontext, pt, actualreg, slot,
                                                  simd_buf, reg_size);
            reg_set_value_ex(reg, info->mcontext, simd_buf);
        }
    }
}

/***************************************************************************
 * INIT AND EXIT
 */

void
drreg_internal_tls_alloc_simd_slots(void *drcontext, OUT byte **simd_spill_start,
                                    OUT byte **simd_spills, uint num_slots)
{
    *simd_spill_start = NULL;
    *simd_spills = NULL;

    if (num_slots > 0) {
        if (drcontext == GLOBAL_DCONTEXT) {
            *simd_spill_start = dr_global_alloc((SIMD_REG_SIZE * num_slots) + 63);
        } else {
            *simd_spill_start =
                dr_thread_alloc(drcontext, (SIMD_REG_SIZE * num_slots) + 63);
        }
        *simd_spills = (byte *)ALIGN_FORWARD(*simd_spill_start, 64);
    }
}

void
drreg_internal_tls_free_simd_slots(void *drcontext, byte *simd_spill_start,
                                   uint num_slots)
{
    if (drreg_internal_ops.num_spill_simd_slots > 0) {
        ASSERT(simd_spill_start != NULL, "SIMD slot storage cannot be NULL");
        if (drcontext == GLOBAL_DCONTEXT) {
            dr_global_free(simd_spill_start, (SIMD_REG_SIZE * num_slots) + 63);
        } else {
            dr_thread_free(drcontext, simd_spill_start, (SIMD_REG_SIZE * num_slots) + 63);
        }
    }
}

void
drreg_internal_tls_simd_data_init(void *drcontext, drreg_internal_per_thread_t *pt)
{
    reg_id_t reg;

    for (reg = DR_REG_APPLICABLE_START_SIMD; reg <= DR_REG_APPLICABLE_STOP_SIMD; reg++) {
        drvector_init(&pt->simd_reg[SIMD_IDX(reg)].live, DR_NUM_SIMD_VECTOR_REGS,
                      false /*!synch*/, NULL);
        pt->simd_reg[SIMD_IDX(reg)].native = true;
    }
    /* We align the block on a 64-byte boundary. */
    drreg_internal_tls_alloc_simd_slots(drcontext, &(pt->simd_spill_start),
                                        &(pt->simd_spills),
                                        drreg_internal_ops.num_spill_simd_slots);
}

void
drreg_internal_tls_simd_data_free(void *drcontext, drreg_internal_per_thread_t *pt)
{
    reg_id_t reg;
    for (reg = DR_REG_APPLICABLE_START_SIMD; reg <= DR_REG_APPLICABLE_STOP_SIMD; reg++) {
        drvector_delete(&pt->simd_reg[SIMD_IDX(reg)].live);
    }
    drreg_internal_tls_free_simd_slots(drcontext, pt->simd_spill_start,
                                       drreg_internal_ops.num_spill_simd_slots);
}
