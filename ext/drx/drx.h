/* **********************************************************
 * Copyright (c) 2013 Google, Inc.   All rights reserved.
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

/* DynamoRio eXtension utilities */

#ifndef _DRX_H_
#define _DRX_H_ 1

/**
 * @file drx.h
 * @brief Header for DynamoRIO eXtension utilities (drx)
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup drx DynamoRIO eXtension utilities
 */
/*@{*/ /* begin doxygen group */

/***************************************************************************
 * INIT
 */

DR_EXPORT
/**
 * Initializes the drx extension.  Must be called prior to any of the
 * other routines.  Can be called multiple times (by separate components,
 * normally) but each call must be paired with a corresponding call to
 * drx_exit().
 *
 * \return whether successful.
 */
bool
drx_init(void);

DR_EXPORT
/**
 * Cleans up the drx extension.
 */
void
drx_exit(void);

/***************************************************************************
 * INSTRUCTION NOTE FIELD
 */

enum {
    DRX_NOTE_NONE = 0, /* == DRMGR_NOTE_NONE */
};

DR_EXPORT
/**
 * Reserves \p size values in the namespace for use in the \p note
 * field of instructions.  The reserved range starts at the return
 * value and is contiguous.  Returns DRX_NOTE_NONE on failure.
 * Un-reserving is not supported.
 */
ptr_uint_t
drx_reserve_note_range(size_t size);

/***************************************************************************
 * ANALYSIS
 */

DR_EXPORT
/**
 * Analyze if arithmetic flags are dead after (including) instruction \p where.
 */
bool
drx_aflags_are_dead(instr_t *where);

/***************************************************************************
 * INSTRUMENTATION
 */

/** Flags for \p drx_insert_counter_update */
enum {
    DRX_COUNTER_64BIT = 0x01, /**< 64-bit counter is used for update. */
    DRX_COUNTER_LOCK  = 0x10, /**< Counter update is atomic. */
};

DR_EXPORT
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to add the
 * constant \p value to the counter located at \p addr.
 * Different DRX_COUNTER_* options can be specified by \p flags.
 * The spill slot \p slot is used for storing arithmetic flags if necessary.
 *
 * \return whether successful.
 *
 * \note The counter update is racy (i.e., not synchronized among threads)
 * unless #DRX_COUNTER_LOCK is specified in \p flags. When #DRX_COUNTER_LOCK
 * is set, the instrumentation may fail if a 64-bit counter is updated in
 * a 32-bit application or the counter crosses cache lines.
 *
 * \note To update multiple counters at the same place, multiple
 * drx_insert_counter_update() invocations should be made in a row with the
 * same \p where instruction and no other instructions should be inserted in
 * between. In that case, \p drx will try to merge the instrumentation for
 * better performance.
 */
bool
drx_insert_counter_update(void *drcontext, instrlist_t *ilist, instr_t *where,
                          dr_spill_slot_t slot, void *addr, int value,
                          uint flags);

/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRX_H_ */
