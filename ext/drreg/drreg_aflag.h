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

#ifndef _DRREG_AFLAG_H_
#define _DRREG_AFLAG_H_ 1

/* An internal drreg interface for aflags.
 * Don't use doxygen since this is an internal interface.
 */

#include "drreg_priv.h"

/***************************************************************************
 * SPILLING AND RESTORING
 */

/* Restores aflags */
drreg_status_t
drreg_internal_restore_aflags(void *drcontext, drreg_internal_per_thread_t *pt,
                              instrlist_t *ilist, instr_t *where, bool release);

/* The caller should only call if aflags are currently in xax.
 * If aflags are in use, moves them to TLS.
 * If not, restores aflags if necessary and restores xax.
 */
drreg_status_t
drreg_internal_move_aflags_from_reg(void *drcontext, drreg_internal_per_thread_t *pt,
                                    instrlist_t *ilist, instr_t *where, bool stateful);

/***************************************************************************
 * ANALYSIS AND CROSS-APP-INSTR
 */

/* Updates liveness information of aflags based on the passed instruction. */
void
drreg_internal_bb_analyse_aflag_liveness(void *drcontext, drreg_internal_per_thread_t *pt,
                                         instr_t *inst, uint index);

/* Restores aflags back to their app values if needed by the app instr or is
 * forced by the caller.
 */
drreg_status_t
drreg_internal_bb_insert_aflag_restore_all(void *drcontext,
                                           drreg_internal_per_thread_t *pt,
                                           instrlist_t *bb, instr_t *inst,
                                           bool force_restore);

/* Updates spilled values of reserved (i.e., in use) aflags after app writes. */
drreg_status_t
drreg_internal_insert_aflag_update_spill(void *drcontext, drreg_internal_per_thread_t *pt,
                                         instrlist_t *bb, instr_t *inst);

/***************************************************************************
 * USE OUTSIDE INSERT PHASE
 */

/* Does a step of the forward liveness analysis for aflags based on the passed
 * instr.
 *
 * Takes in the current flags for aflags, and updates the value based on instr.
 */
void
drreg_internal_forward_analyse_aflag_liveness(instr_t *inst,
                                              INOUT ptr_uint_t *aflags_cur);

/* Does the final processing of the forward liveness analysis, where the flag result is
 * noted.
 */
void
drreg_internal_finalise_forward_aflag_liveness_analysis(drreg_internal_per_thread_t *pt,
                                                        ptr_uint_t aflags_cur);

/***************************************************************************
 * REGISTER RESERVATION
 */

/* Stores the value of aflags back into the XAX register at where_respill to
 * revert the state of registers to that before the statelessly restores.
 */
bool
drreg_internal_aflag_handle_respill_for_statelessly_restore(
    void *drcontext, drreg_internal_per_thread_t *pt, instrlist_t *ilist,
    instr_t *where_respill, reg_id_t reg);

/***************************************************************************
 * RESTORE STATE
 */

/* Handle an aflag spill encountered when walking over a bb's instructions during
 * restoration.
 */
void
drreg_internal_aflag_restore_state_handle_spill(void *drcontext, byte *pc, uint slot,
                                                uint *spilled_to_aflags);

/* Handle an aflag restore encountered when walking over a bb's instructions during
 * restoration.
 */
void
drreg_internal_aflag_restore_state_handle_restore(void *drcontext, byte *pc, uint slot,
                                                  uint *spilled_to_aflags);

/* Responsible for setting the values of aflags during restoration.
 * This is typically one of the last functions called during the restoration process.
 */
void
drreg_internal_aflag_restore_state_set_value(
    void *drcontext, dr_restore_state_info_t *info,
    uint spilled_to_aflags _IF_X86(bool aflags_in_xax));

/***************************************************************************
 * INIT AND EXIT
 */

/* Initialises per thread information related to aflags. */
void
drreg_internal_tls_aflag_data_init(drreg_internal_per_thread_t *pt);

/* Deletes per thread information related to aflags. */
void
drreg_internal_tls_aflag_data_free(drreg_internal_per_thread_t *pt);

#endif /* _DRREG_AFLAG_H_ */
