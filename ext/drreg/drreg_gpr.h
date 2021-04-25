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

#ifndef _DRREG_GPR_H_
#define _DRREG_GPR_H_ 1

#include "drreg_priv.h"

/* An internal drreg interface for GPRs.
 * Don't use doxygen since this is an internal interface.
 */

#define GPR_IDX(reg) ((reg)-DR_REG_START_GPR)

/***************************************************************************
 * SPILLING AND RESTORING
 */

/* Returns a free slot for storing the value of a GPR register.
 * If no slots are available, the function returns MAX_SPILLS as an indicator.*/
uint
drreg_internal_find_free_gpr_slot(drreg_internal_per_thread_t *pt);

/* Spill the value of a GPR. */
void
drreg_internal_spill_gpr(void *drcontext, drreg_internal_per_thread_t *pt,
                         instrlist_t *ilist, instr_t *where, reg_id_t reg, uint slot);

/* Restores the value of a GPR. */
void
drreg_internal_restore_gpr(void *drcontext, drreg_internal_per_thread_t *pt,
                           instrlist_t *ilist, instr_t *where, reg_id_t reg, uint slot,
                           bool release);

/* Returns the value of a spilled gpr. */
reg_t
drreg_internal_get_spilled_gpr_value(void *drcontext, uint tls_slot_offs, uint slot);

/***************************************************************************
 * ANALYSIS AND CROSS-APP-INSTR
 */

/* Increments, if required, the use count of the register based on the passed operand. */
void
drreg_internal_increment_app_gpr_use_count(drreg_internal_per_thread_t *pt, opnd_t opnd,
                                           reg_id_t reg);

/* Initialises thread data for liveness analysis of gprs. */
void
drreg_internal_bb_init_gpr_liveness_analysis(drreg_internal_per_thread_t *pt);

/* Updates liveness information of gprs based on the passed instruction. */
void
drreg_internal_bb_analyse_gpr_liveness(void *drcontext, drreg_internal_per_thread_t *pt,
                                       instr_t *inst, uint index);

/* Restores all gprs back to their app values if needed by the app instr or is forced by
 * the caller.
 */
drreg_status_t
drreg_internal_bb_insert_gpr_restore_all(void *drcontext, drreg_internal_per_thread_t *pt,
                                         instrlist_t *bb, instr_t *inst,
                                         bool force_restore, OUT bool *regs_restored);

/* Updates spilled values of reserved (i.e., in use) gprs after app writes. */
drreg_status_t
drreg_internal_insert_gpr_update_spill(void *drcontext, drreg_internal_per_thread_t *pt,
                                       instrlist_t *bb, instr_t *inst,
                                       bool *restored_for_read);

/***************************************************************************
 * USE OUTSIDE INSERT PHASE
 */

/* Performs the necessary initialisations and re-sets to perform forward liveness analysis
 * of grps.
 */
void
drreg_internal_init_forward_gpr_liveness_analysis(drreg_internal_per_thread_t *pt);

/* Does a step of the forward liveness analysis for gprs based on the passed instr. */
void
drreg_internal_forward_analyse_gpr_liveness(drreg_internal_per_thread_t *pt,
                                            instr_t *inst);

/* Does the final processing of the forward liveness analysis, where gprs with
 * an UNKNOWN live state is set to LIVE.
 */
void
drreg_internal_finalise_forward_gpr_liveness_analysis(drreg_internal_per_thread_t *pt);

/***************************************************************************
 * REGISTER RESERVATION
 */

/* Reserves a gpr.
 *
 * Assumes liveness info is already set up in drreg_internal_per_thread_t.
 * Liveness should have either been computed by a forward liveness scan upon
 * every insertion if called outside of insertion phase, see drreg_forward_analysis().
 * Or if called inside insertion phase, at the end of drmgr's analysis phase once,
 * see drreg_event_bb_analysis(). Please note that drreg is not yet able to properly
 * handle multiple users if they use drreg from in and outside of the insertion phase,
 * xref i#3823.
 */
drreg_status_t
drreg_internal_reserve_gpr(void *drcontext, drreg_internal_per_thread_t *pt,
                           instrlist_t *ilist, instr_t *where, drvector_t *reg_allowed,
                           bool only_if_no_spill, OUT reg_id_t *reg_out);

/* Restores a GPR back to its app value. It is mainly used as a
 * restoration barrier.
 */
drreg_status_t
drreg_internal_restore_gpr_app_value(void *drcontext, drreg_internal_per_thread_t *pt,
                                     instrlist_t *ilist, instr_t *where, reg_id_t app_reg,
                                     reg_id_t dst_reg, bool stateful);

/* Restores all GPR regs used in the passed operand, thus triggering a lazy restoration
 * barrier.
 */
drreg_status_t
drreg_internal_restore_gpr_app_values(void *drcontext, instrlist_t *ilist, instr_t *where,
                                      opnd_t opnd, INOUT reg_id_t *swap,
                                      OUT bool *no_app_value);

/* Restores a passed SIMD register back to its native value. Internally, invokes
 * drreg_internal_restore_gpr().
 */
drreg_status_t
drreg_internal_restore_gpr_reg_now(void *drcontext, drreg_internal_per_thread_t *pt,
                                   instrlist_t *ilist, instr_t *inst, reg_id_t reg);

/* Unreserves a gpr register. */
drreg_status_t
drreg_internal_unreserve_gpr(void *drcontext, drreg_internal_per_thread_t *pt,
                             instrlist_t *ilist, instr_t *where, reg_id_t reg);

/* As the name implies, returns whether the passed GPR is dead. */
drreg_status_t
drreg_internal_is_gpr_dead(drreg_internal_per_thread_t *pt, reg_id_t reg, bool *dead);

/* Initialises and fills a vector of flags denoting which GPRs are allowed for
 * reservation.
 */
drreg_status_t
drreg_internal_init_and_fill_gpr_vector(drvector_t *vec, bool allowed);

/* Sets a flag denoting whether a GPR is allowed for reservation. */
drreg_status_t
drreg_internal_set_gpr_vector_entry(drvector_t *vec, reg_id_t reg, bool allowed);

/***************************************************************************
 * RESTORE STATE
 */

/* Returns whether the passed instruction is a drreg spill or restore of a GPR.
 * Slot data is also returned via slot_out.
 *
 * It is assumed that instr_is_reg_spill_or_restore() has already been called and returned
 * true.
 */
bool
drreg_internal_is_gpr_spill_or_restore(void *drcontext, instr_t *instr, bool is_tls,
                                       uint offs, uint *slot_out OUT);

/* Initialises analysis data structures to be used when walking over a bb's instructions
 * to restore the states of GPRs.
 */
void
drreg_internal_gpr_restore_state_init(uint *spilled_to);

/* Handle a GPR spill encountered when walking over a bb's instructions during
 * restoration event.
 */
void
drreg_internal_gpr_restore_state_handle_spill(void *drcontext, byte *pc, uint slot,
                                              reg_id_t reg, uint *spilled_to);

/* Handle a GPR restore encountered when walking over a bb's instructions during
 * restoration.
 */
void
drreg_internal_gpr_restore_state_handle_restore(void *drcontext, byte *pc, uint slot,
                                                reg_id_t reg, uint *spilled_to);

/* Responsible for setting the values of GPRs during restoration.
 * This is typically one of the last functions called during the restoration process.
 */
void
drreg_internal_gpr_restore_state_set_values(void *drcontext,
                                            dr_restore_state_info_t *info,
                                            uint *spilled_to);

/***************************************************************************
 * INIT AND EXIT
 */

/* Initialises per thread information related to gprs. */
void
drreg_internal_tls_gpr_data_init(drreg_internal_per_thread_t *pt);

/* Deletes per thread information related to gprs. */
void
drreg_internal_tls_gpr_data_free(drreg_internal_per_thread_t *pt);

#endif /* _DRREG_GPR_H_ */
