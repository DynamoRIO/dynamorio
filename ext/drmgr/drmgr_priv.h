/* **********************************************************
 * Copyright (c) 2020 Google, Inc.   All rights reserved.
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

#ifndef _DRMGR_PRIV_H_
#define _DRMGR_PRIV_H_ 1

/* An internal interface that acts as a beachhead for other extensions, e.g. drbbdup,
 * to integrate their functionalities with drmgr. The interface should only be used
 * when tight integration with drmgr is required.
 */

/* Don't use doxygen since this is an internal interface. */

#ifdef __cplusplus
extern "C" {
#endif

#include "dr_defines.h"
#include "drmgr.h"

/***************************************************************************
 * For DRBBDUP
 */

/* Responsible for duplicating basic blocks. Returns local info
 * used to iterate over basic block copies.
 */
typedef bool (*drmgr_bbdup_duplicate_bb_cb_t)(void *drcontext, void *tag, instrlist_t *bb,
                                              bool for_trace, bool translating,
                                              OUT void **local_info);

/* Extracts and returns a pending basic block copy from the main basic block. Returns
 * NULL, if no further copies are pending.
 */
typedef instrlist_t *(*drmgr_bbdup_extract_cb_t)(void *drcontext, void *tag,
                                                 instrlist_t *bb, bool for_trace,
                                                 bool translating, void *local_info);

/* Stitches an extracted basic block copy back to the main basic block. */
typedef void (*drmgr_bbdup_stitch_cb_t)(void *drcontext, void *tag, instrlist_t *bb,
                                        instrlist_t *case_bb, bool for_trace,
                                        bool translating, void *local_dup_info_opaque);

/* Finalises the iteration process and inserts the encoder at the top of the main basic
 * block.
 */
typedef void (*drmgr_bbdup_insert_encoding_cb_t)(void *drcontext, void *tag,
                                                 instrlist_t *bb, bool for_trace,
                                                 bool translating, void *local_info);

DR_EXPORT
/* Used by drbbdup so that drmgr maintains basic block duplication.
 * BBDUP events can only be registered once at the same time.
 * Returns true on success. Use drmgr_unregister_bbdup_event() to unregister.
 */
bool
drmgr_register_bbdup_event(drmgr_bbdup_duplicate_bb_cb_t bb_dup_func,
                           drmgr_bbdup_insert_encoding_cb_t insert_encoding,
                           drmgr_bbdup_extract_cb_t extract_func,
                           drmgr_bbdup_stitch_cb_t stitch_func);

DR_EXPORT
/* Unregisters drbbdup events set by the prior calling of drmgr_register_bbdup_event().
 * Returns true on success.
 */
bool
drmgr_unregister_bbdup_event();

DR_EXPORT
/* Registers a callback that is triggered prior to basic block duplication. This gives
 * drbbdup the opportunity to modify and analyse the basic block before proceeding with
 * the generation of multiple copies.
 *
 * Note, we cannot use app2app events, because those are granular to per basic block copy.
 *
 * Returns true on success.
 */
bool
drmgr_register_bbdup_pre_event(drmgr_xform_cb_t func, drmgr_priority_t *priority);

DR_EXPORT
/* Unregisters a callback function triggered prior basic block duplication.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
drmgr_unregister_bbdup_pre_event(drmgr_xform_cb_t func);

#ifdef __cplusplus
}
#endif

#endif /* _DRMGR_PRIV_H_ */
