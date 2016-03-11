/* **********************************************************
 * Copyright (c) 2013-2015 Google, Inc.   All rights reserved.
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

/* i#1531: drx uses drmgr internally, so a client using drx cannot use
 * DR's TLS field routines directly.
 */
#ifndef dr_set_tls_field
# define dr_get_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
# define dr_set_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
# define dr_insert_read_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
# define dr_insert_write_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
#endif

/***************************************************************************
 * INIT
 */

DR_EXPORT
/**
 * Initializes the drx extension.  Must be called prior to any drx routine
 * that does not explicitly state otherwise.
 * Can be called multiple times (by separate components,
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
 *
 * \note May be called without calling drx_init().
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
 *
 * When used with drmgr, this routine uses the drreg extension.  It must be
 * called from drmgr's insertion phase.  The drreg extension will be used to
 * spill the arithmetic flags and any scratch registers needed.  It is up to the
 * caller to ensure that enough spill slots are available, through drreg's
 * initialization.  The slot and slot2 parameters must be set to
 * SPILL_SLOT_MAX+1.
 *
 * When used without drmgr, the spill slot \p slot is used for storing
 * arithmetic flags or a scratch register if necessary.  The spill
 * slot \p slot2 is used only on ARM for spilling a second scratch
 * register.
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
 *
 * \note May be called without calling drx_init().
 */
bool
drx_insert_counter_update(void *drcontext, instrlist_t *ilist, instr_t *where,
                          dr_spill_slot_t slot, IF_ARM_(dr_spill_slot_t slot2)
                          void *addr, int value, uint flags);

/***************************************************************************
 * SOFT KILLS
 */

DR_EXPORT
/**
 * Registers for the "soft kills" event, which helps to execute process
 * exit events when a process is terminated by another process.
 *
 * The callback's return value indicates whether to skip the
 * termination action by the application: i.e., true indicates to skip
 * it (the usual case) and false indicates to continue with the
 * application action.  In some cases, individually continuing
 * requires emulation when the original application action involved
 * multiple processes.
 *
 * When there are multiple registered callbacks, if any callback
 * returns true, the application action is skipped.
 *
 * In normal usage, upon receiving this callback the client will send
 * a nudge (see dr_nudge_client_ex()) to the targeted process.  The
 * nudge handler then performs any shutdown actions, such as
 * instrumentation result output.  The handler then terminates the
 * target process from within, allowing the callback in the targeting
 * process to skip the termination action.  Passing the exit code to
 * the nudge handler is recommended to preserve the intended
 * application exit code.
 *
 * The nudge handler should support being invoked multiple times
 * (typically by having only the first one take effect) as in some
 * cases a parent process will terminate child processes in multiple
 * ways.
 *
 * This event must be registered for during process initialization, in
 * order to properly track per-thread information.  Un-registering is
 * not supported: soft kills cannot be in effect for only part of the
 * process lifetime.
 *
 * Soft kills can be risky.  If the targeted process is not under
 * DynamoRIO control, the nudge might terminate it, but in a different
 * manner than would have occurred.  If the nudge fails for some
 * reason but the targeter's termination is still skipped, the child
 * process might be left alive, causing the application to behave
 * incorrectly.
 *
 * The implementation of this event uses drmgr's CLS
 * (drmgr_register_cls_field()), which conflicts with
 * dr_get_tls_field().  A client using this event must use
 * drmgr_register_tls_field() instead of dr_get_tls_field().
 *
 * \return whether successful.
 */
bool
drx_register_soft_kills(bool (*event_cb)(process_id_t pid, int exit_code));

/***************************************************************************
 * LOGGING
 */

DR_EXPORT
/**
 * Opens a new file with a name constructed from "dir/prefix.xxxx.suffix",
 * where xxxx is a 4-digit number incremented until a unique name is found
 * that does not collide with any existing file.
 *
 * Passes \p extra_flags through to the dr_open_file() call.
 *
 * On success, returns the file handle and optionally the resulting path
 * in \p result.  On failure, returns INVALID_FILE.
 *
 * \note May be called without calling drx_init().
 */
file_t
drx_open_unique_file(const char *dir, const char *prefix, const char *suffix,
                     uint extra_flags, char *result OUT, size_t result_len);

DR_EXPORT
/**
 * Opens a new file with a name constructed from "dir/prefix.appname.id.xxxx.suffix",
 * where xxxx is a 4-digit number incremented until a unique name is found
 * that does not collide with any existing file.  The appname string comes
 * from dr_get_application_name().  The id portion of the string is from \p id,
 * which is meant to be either the process id or the thread id.
 *
 * Passes \p extra_flags through to the dr_open_file() call.
 *
 * On success, returns the file handle and optionally the resulting path
 * in \p result.  On failure, returns INVALID_FILE.
 *
 * \note May be called without calling drx_init().
 */
file_t
drx_open_unique_appid_file(const char *dir, ptr_int_t id,
                           const char *prefix, const char *suffix,
                           uint extra_flags, char *result OUT, size_t result_len);

/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRX_H_ */
