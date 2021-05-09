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

#ifndef _DRREG_SIMD_H_
#define _DRREG_SIMD_H_ 1

#include "drreg_priv.h"

/* An internal drreg interface for SIMD Vector registers.
 * Don't use doxygen since this is an internal interface.
 */

/* The applicable register range is what's used internally to iterate over all possible
 * SIMD registers for a given build. Regs are resized to zmm when testing via SIMD_IDX().
 */
#define DR_REG_APPLICABLE_START_SIMD DR_REG_START_ZMM
#define DR_REG_APPLICABLE_STOP_SIMD DR_REG_STOP_ZMM
#define SIMD_IDX(reg) ((reg_resize_to_opsz(reg, OPSZ_64)) - DR_REG_START_ZMM)

/* Liveness states for SIMD vector registers (not for mmx).
 * Note that value order, i.e., SIMD_ZMM_DEAD > SIMD_YMM_DEAD > SIMD_XMM_DEAD,
 * is important as drreg relies on it to reason over states.
 */
#define SIMD_XMM_DEAD                                                 \
    ((void *)(ptr_uint_t)0) /* first 16 bytes are dead, rest are live \
                             */
#define SIMD_YMM_DEAD                                                 \
    ((void *)(ptr_uint_t)1) /* first 32 bytes are dead, rest are live \
                             */
#define SIMD_ZMM_DEAD                                                 \
    ((void *)(ptr_uint_t)2) /* first 64 bytes are dead, rest are live \
                             */
#define SIMD_XMM_LIVE                                                 \
    ((void *)(ptr_uint_t)3) /* first 16 bytes are live, rest are dead \
                             */
#define SIMD_YMM_LIVE                                                 \
    ((void *)(ptr_uint_t)4) /* first 32 bytes are live, rest are dead \
                             */
#define SIMD_ZMM_LIVE                                                 \
    ((void *)(ptr_uint_t)5) /* first 64 bytes are live, rest are dead \
                             */

#define SIMD_UNKNOWN ((void *)(ptr_uint_t)6)

#define XMM_REG_SIZE 16
#define YMM_REG_SIZE 32
#define ZMM_REG_SIZE 64
#define SIMD_REG_SIZE ZMM_REG_SIZE

/***************************************************************************
 * SPILLING AND RESTORING
 */

/* Returns the value of a spilled SIMD vector register via the destination buffer. */
void
drreg_internal_get_spilled_simd_value(void *drcontext, drreg_internal_per_thread_t *pt,
                                      reg_id_t reg, uint slot, OUT byte *value_buf,
                                      size_t buf_size);

/***************************************************************************
 * ANALYSIS AND CROSS-APP-INSTR
 */

/* Increments, if required, the use count of the register based on the passed operand. */
void
drreg_internal_increment_app_simd_use_count(drreg_internal_per_thread_t *pt, opnd_t opnd,
                                            reg_id_t reg);

/* Initialises thread data for liveness analysis of SIMD registers. */
void
drreg_internal_bb_init_simd_liveness_analysis(drreg_internal_per_thread_t *pt);

/* Updates liveness information of SIMD registers based on the passed instruction. */
void
drreg_internal_bb_analyse_simd_liveness(void *drcontext, drreg_internal_per_thread_t *pt,
                                        instr_t *inst, uint index);

/* Restores all SIMD registers back to their app values if needed by the app instr or is
 * forced by the caller.
 */
drreg_status_t
drreg_internal_bb_insert_simd_restore_all(void *drcontext,
                                          drreg_internal_per_thread_t *pt,
                                          instrlist_t *bb, instr_t *inst,
                                          bool force_restore,
                                          OUT bool *simd_regs_restored);

/* Updates spilled values of reserved (i.e., in use) SIMD registers after app writes. */
drreg_status_t
drreg_internal_bb_insert_simd_update_spill(void *drcontext,
                                           drreg_internal_per_thread_t *pt,
                                           instrlist_t *bb, instr_t *inst,
                                           bool *simd_restored_for_read);

/***************************************************************************
 * USE OUTSIDE INSERT PHASE
 */

/* Performs the necessary initialisations and re-sets to perform forward liveness analysis
 * of SIMD vector registers.
 */
void
drreg_internal_init_forward_simd_liveness_analysis(drreg_internal_per_thread_t *pt);

/* Does a step of the forward liveness analysis for SIMD vector based on the passed
 * instr.
 */
void
drreg_internal_forward_analyse_simd_liveness(drreg_internal_per_thread_t *pt,
                                             instr_t *inst);

/* Does the final processing of the forward liveness analysis, where SIMD registers with
 * an UNKNOWN live state is set to LIVE.
 */
void
drreg_internal_finalise_forward_simd_liveness_analysis(drreg_internal_per_thread_t *pt);

/***************************************************************************
 * REGISTER RESERVATION
 */

/* Reserves a SIMD vector register.
 *
 * Makes the same assumptions about liveness info being already computed as
 * drreg_reserve_gpr_internal().
 */
drreg_status_t
drreg_internal_reserve_simd_reg(void *drcontext, drreg_internal_per_thread_t *pt,
                                drreg_spill_class_t spill_class, instrlist_t *ilist,
                                instr_t *where, drvector_t *reg_allowed,
                                bool only_if_no_spill, OUT reg_id_t *reg_out);

/* Restores a SIMD vector register back to its app value. It is mainly used as a
 * restoration barrier.
 */
drreg_status_t
drreg_internal_restore_simd_app_value(void *drcontext, drreg_internal_per_thread_t *pt,
                                      instrlist_t *ilist, instr_t *where,
                                      reg_id_t app_reg, reg_id_t dst_reg, bool stateful);

/* Restores all SIMD vector register used in the passed operand, thus triggering a lazy
 * restoration barrier.
 */
drreg_status_t
drreg_internal_restore_simd_app_values(void *drcontext, instrlist_t *ilist,
                                       instr_t *where, opnd_t opnd,
                                       OUT bool *no_app_value);

/* Unreserves the passed SIMD register. */
drreg_status_t
drreg_internal_unreserve_simd_reg(void *drcontext, drreg_internal_per_thread_t *pt,
                                  instrlist_t *ilist, instr_t *where, reg_id_t reg);

/* Restores a passed SIMD register back to its native value. */
drreg_status_t
drreg_internal_restore_simd_reg_now(void *drcontext, drreg_internal_per_thread_t *pt,
                                    instrlist_t *ilist, instr_t *inst, reg_id_t reg);

/* As the name implies, returns whether the passed SIMD register is dead. */
drreg_status_t
drreg_internal_is_simd_reg_dead(drreg_internal_per_thread_t *pt,
                                drreg_spill_class_t spill_class, reg_id_t reg,
                                bool *dead);

/* Initialises and fills a vector of flags denoting which SIMD registers are allowed for
 * reservation.
 */
drreg_status_t
drreg_internal_init_and_fill_simd_vector(drvector_t *vec, bool allowed);

/* Sets a flag denoting whether a SIMD register is allowed for reservation. */
drreg_status_t
drreg_internal_set_simd_vector_entry(drvector_t *vec, reg_id_t reg, bool allowed);

/***************************************************************************
 * RESTORE STATE
 */

/* Returns whether the passed instruction is a drreg spill or restore of a SIMD register.
 * The register that is spilled/restored by the instruction is returned via reg_spilled.
 * Slot data is also returned in a similar fashion via slot_out.
 *
 * It is assumed that instr_is_reg_spill_or_restore() has already been called and returned
 * true.
 */
bool
drreg_internal_is_simd_spill_or_restore(void *drcontext, instr_t *instr, bool is_tls,
                                        uint offs, bool *is_spilled INOUT,
                                        reg_id_t *reg_spilled OUT, uint *slot_out OUT);

/* Initialises analysis data structures to be used when walking over a bb's instructions
 * to restore the states of SIMD registers.
 */
void
drreg_internal_simd_restore_state_init(uint *spilled_simd_to, reg_id_t *simd_slot_use);

/* Handle a SIMD spill encountered when walking over a bb's instructions during
 * restoration.
 */
void
drreg_internal_simd_restore_state_handle_spill(void *drcontext, byte *pc, uint slot,
                                               reg_id_t reg, uint *spilled_simd_to,
                                               reg_id_t *simd_slot_use);

/* Handle a SIMD restore encountered when walking over a bb's instructions during
 * restoration.
 */
void
drreg_internal_simd_restore_state_handle_restore(void *drcontext, byte *pc, uint slot,
                                                 reg_id_t reg, uint *spilled_simd_to,
                                                 reg_id_t *simd_slot_use);

/* Responsible for setting the values of SIMD vector registers during restoration.
 * This is typically one of the last functions called during the restoration process.
 */
void
drreg_internal_simd_restore_state_set_values(void *drcontext,
                                             drreg_internal_per_thread_t *pt,
                                             dr_restore_state_info_t *info,
                                             uint *spilled_simd_to,
                                             reg_id_t *simd_slot_use);

/***************************************************************************
 * INIT AND EXIT
 */

/* Returns an aligned block for SIMD reg spillage. Note, simd_spill_start is the start
 * of the block, returned by the allocator, while simd_spills is an aligned basic block.
 */
void
drreg_internal_tls_alloc_simd_slots(void *drcontext, OUT byte **simd_spill_start,
                                    OUT byte **simd_spills, uint num_slots);

/* Frees the SIMD spill block. This function is typically called upon process exit. */
void
drreg_internal_tls_free_simd_slots(void *drcontext, byte *simd_spill_start,
                                   uint num_slots);

/* Initialises per thread information related to SIMD registers. */
void
drreg_internal_tls_simd_data_init(void *drcontext, drreg_internal_per_thread_t *pt);

/* Deletes per thread information related to SIMD registers. */
void
drreg_internal_tls_simd_data_free(void *drcontext, drreg_internal_per_thread_t *pt);

#endif /* _DRREG_SIMD_H_ */
