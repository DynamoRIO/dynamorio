/* **********************************************************
 * Copyright (c) 2013-2017 Google, Inc.   All rights reserved.
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
#    define dr_get_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
#    define dr_set_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
#    define dr_insert_read_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
#    define dr_insert_write_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
#endif

/***************************************************************************
 * INIT
 */

enum {
    /**
     * Priority of drx fault handling event.
     */
    DRMGR_PRIORITY_FAULT_DRX = -7500,
};

/**
 * Name of drx fault handling event.
 */
#define DRMGR_PRIORITY_NAME_DRX_FAULT "drx_fault"

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
    DRX_COUNTER_LOCK = 0x10,  /**< Counter update is atomic. */
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
 * a 32-bit application or the counter crosses cache lines. Currently,
 * #DRX_COUNTER_LOCK is not yet supported on ARM.
 *
 * \note To update multiple counters at the same place, multiple
 * drx_insert_counter_update() invocations should be made in a row with the
 * same \p where instruction and no other instructions should be inserted in
 * between. In that case, \p drx will try to merge the instrumentation for
 * better performance.
 */
bool
drx_insert_counter_update(void *drcontext, instrlist_t *ilist, instr_t *where,
                          dr_spill_slot_t slot,
                          IF_NOT_X86_(dr_spill_slot_t slot2) void *addr, int value,
                          uint flags);

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
 * INSTRUCTION LIST
 */

DR_EXPORT
/**
 * Returns the number of instructions (including meta-instructions) inside a basic block
 * \p ilist.
 *
 * The function iterates over the ilist in order to obtain the count. The result is not
 * cached. Therefore, avoid using this function during the insert stage of the
 * instrumentation process.
 */
size_t
drx_instrlist_size(instrlist_t *ilist);

DR_EXPORT
/**
 * Returns the number of application instructions (excluding meta-instructions) inside
 * a basic block \p ilist.
 *
 * The function iterates over the ilist in order to obtain the count. The result is not
 * cached. Therefore, avoid using this function during the insert stage of the
 * instrumentation process.
 */
size_t
drx_instrlist_app_size(instrlist_t *ilist);

/***************************************************************************
 * LOGGING
 */

/**
 * Flag for use with drx_open_unique_file() or drx_open_unique_appid_file()
 * in \p extra_flags to skip the file open and get the path string only.
 *
 * \note This flag value must not conflict with any DR_FILE_* flag value
 * used by dr_open_file().
 */
#define DRX_FILE_SKIP_OPEN 0x8000

DR_EXPORT
/**
 * Opens a new file with a name constructed from "dir/prefix.xxxx.suffix",
 * where xxxx is a 4-digit number incremented until a unique name is found
 * that does not collide with any existing file.
 *
 * Passes \p extra_flags through to the dr_open_file() call if \p extra_flags
 * is not DRX_FILE_SKIP_OPEN.
 *
 * On success, returns the file handle and optionally the resulting path
 * in \p result.  On failure, returns INVALID_FILE.
 *
 * Skips dr_open_file() if \p extra_flags is DRX_FILE_SKIP_OPEN.
 * Returns INVALID_FILE and optionally the resulting path in \p result.
 * Unique name is not guaranteed and xxxx is set randomly.
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
 * Passes \p extra_flags through to the dr_open_file() call if \p extra_flags
 * is not DRX_FILE_SKIP_OPEN.
 *
 * On success, returns the file handle and optionally the resulting path
 * in \p result.  On failure, returns INVALID_FILE.
 *
 * Skips dr_open_file() if \p extra_flags is DRX_FILE_SKIP_OPEN.
 * Returns INVALID_FILE and optionally the resulting path in \p result.
 * Unique name is not guaranteed and xxxx is set randomly.
 *
 * \note May be called without calling drx_init().
 */
file_t
drx_open_unique_appid_file(const char *dir, ptr_int_t id, const char *prefix,
                           const char *suffix, uint extra_flags, char *result OUT,
                           size_t result_len);

DR_EXPORT
/**
 * Creates a new directory with a name constructed from
 * "dir/prefix.appname.id.xxxx.suffix",
 * where xxxx is a 4-digit number incremented until a unique name is found
 * that does not collide with any existing file.  The appname string comes
 * from dr_get_application_name().  The id portion of the string is from \p id,
 * which is meant to be either the process id or the thread id.
 *
 * Returns whether successful.
 * On success, optionally returns the resulting path in \p result.
 *
 * \note May be called without calling drx_init().
 */
bool
drx_open_unique_appid_dir(const char *dir, ptr_int_t id, const char *prefix,
                          const char *suffix, char *result OUT, size_t result_len);

/***************************************************************************
 * BUFFER FILLING LIBRARY
 */

/**
 * Callback for \p drx_buf_init_trace_buffer(), called when the buffer has
 * been filled. The valid buffer data is contained within the interval
 * [buf_base..buf_base+size).
 */
typedef void (*drx_buf_full_cb_t)(void *drcontext, void *buf_base, size_t size);

struct _drx_buf_t;

/** Opaque handle which represents a buffer for use by the drx_buf framework. */
typedef struct _drx_buf_t drx_buf_t;

enum {
    /**
     * Buffer size to be specified in drx_buf_create_circular_buffer() in order
     * to make use of the fast circular buffer optimization.
     */
    DRX_BUF_FAST_CIRCULAR_BUFSZ = (1 << 16)
};

/**
 * Priorities of drmgr instrumentation passes used by drx_buf. Users
 * of drx_buf can use the names #DRMGR_PRIORITY_NAME_DRX_BUF_INIT and
 * #DRMGR_PRIORITY_NAME_DRX_BUF_EXIT in the drmgr_priority_t.before field
 * or can use these numeric priorities in the drmgr_priority_t.priority
 * field to ensure proper instrumentation pass ordering.
 */
enum {
    /** Priority of drx_buf thread init event */
    DRMGR_PRIORITY_THREAD_INIT_DRX_BUF = -7500,
    /** Priority of drx_buf thread exit event */
    DRMGR_PRIORITY_THREAD_EXIT_DRX_BUF = -7500,
};

/** Name of drx_buf thread init priority for buffer initialization. */
#define DRMGR_PRIORITY_NAME_DRX_BUF_INIT "drx_buf.init"

/** Name of drx_buf thread exit priority for buffer cleanup and full_cb callback. */
#define DRMGR_PRIORITY_NAME_DRX_BUF_EXIT "drx_buf.exit"

DR_EXPORT
/**
 * Initializes the drx_buf extension with a circular buffer which wraps
 * around when full.
 *
 * \note All buffer sizes are supported. However, a buffer size of
 * #DRX_BUF_FAST_CIRCULAR_BUFSZ bytes is specially optimized for performance.
 * This buffer is referred to explicitly in the documentation as the "fast
 * circular buffer".
 *
 * \return NULL if unsuccessful, a valid opaque struct pointer if successful.
 */
drx_buf_t *
drx_buf_create_circular_buffer(size_t buf_size);

DR_EXPORT
/**
 * Initializes the drx_buf extension with a buffer; \p full_cb is called
 * when the buffer becomes full.
 *
 * \return NULL if unsuccessful, a valid opaque struct pointer if successful.
 */
drx_buf_t *
drx_buf_create_trace_buffer(size_t buffer_size, drx_buf_full_cb_t full_cb);

DR_EXPORT
/** Cleans up the buffer associated with \p buf. \returns whether successful. */
bool
drx_buf_free(drx_buf_t *buf);

DR_EXPORT
/**
 * Inserts instructions to load the address of the TLS buffer at \p where
 * into \p buf_ptr.
 */
void
drx_buf_insert_load_buf_ptr(void *drcontext, drx_buf_t *buf, instrlist_t *ilist,
                            instr_t *where, reg_id_t buf_ptr);

DR_EXPORT
/**
 * Inserts instructions to increment the buffer pointer by \p stride to accommodate
 * the writes that occurred since the last time the base pointer was loaded.
 *
 * \note \p scratch is only necessary on ARM, in the case of the fast circular
 * buffer. On x86 scratch is completely unused.
 */
void
drx_buf_insert_update_buf_ptr(void *drcontext, drx_buf_t *buf, instrlist_t *ilist,
                              instr_t *where, reg_id_t buf_ptr, reg_id_t scratch,
                              ushort stride);

DR_EXPORT
/**
 * Inserts instructions to store \p opsz bytes of \p opnd at \p offset bytes
 * from \p buf_ptr. \p opnd must be a register or an immediate opnd of some
 * appropriate size.  \return whether successful.
 *
 * \note \p opsz must be either \p OPSZ_1, \p OPSZ_2, \p OPSZ_4 or \p OPSZ_8.
 *
 * \note \p scratch is only necessary on ARM when storing an immediate operand.
 *
 * \note This method simply wraps a store that also sets an app translation. Make
 * sure that \p where has a translation set.
 */
bool
drx_buf_insert_buf_store(void *drcontext, drx_buf_t *buf, instrlist_t *ilist,
                         instr_t *where, reg_id_t buf_ptr, reg_id_t scratch, opnd_t opnd,
                         opnd_size_t opsz, short offset);

DR_EXPORT
/**
 * Retrieves a pointer to the top of the buffer, that is, returns the
 * same value as would an invocation of drx_buf_insert_load_buf_ptr().
 */
void *
drx_buf_get_buffer_ptr(void *drcontext, drx_buf_t *buf);

DR_EXPORT
/**
 * Allows one to set the buffer pointer so that subsequent invocations
 * of drx_buf_insert_load_buf_ptr() will use this new value instead.
 */
void
drx_buf_set_buffer_ptr(void *drcontext, drx_buf_t *buf, void *new_ptr);

DR_EXPORT
/** Retrieves a pointer to the base of the buffer. */
void *
drx_buf_get_buffer_base(void *drcontext, drx_buf_t *buf);

DR_EXPORT
/** Retrieves the capacity of the buffer. */
size_t
drx_buf_get_buffer_size(void *drcontext, drx_buf_t *buf);

DR_EXPORT
/**
 * Pads a basic block with a label at the end for routines which rely on inserting
 * instrumentation after every instruction. Note that users of this routine must act on
 * the previous instruction in basic block events before skipping non-app instructions
 * because the label is not marked as an app instruction.
 *
 * \note the padding label is not introduced if the basic block is already branch
 * terminated.
 *
 * \returns whether padding was introduced.
 */
bool
drx_tail_pad_block(void *drcontext, instrlist_t *ilist);

DR_EXPORT
/**
 * Constructs a memcpy-like operation that is compatible with drx_buf.
 *
 * \note drx_buf_insert_buf_memcpy() will increment the buffer pointer internally.
 */
void
drx_buf_insert_buf_memcpy(void *drcontext, drx_buf_t *buf, instrlist_t *ilist,
                          instr_t *where, reg_id_t dst, reg_id_t src, ushort len);

DR_EXPORT
/**
 * Expands AVX2 gather and AVX-512 gather and scatter instructions to a sequence of
 * equivalent scalar load and stores, mask register bit tests, and mask register bit
 * updates.
 *
 * \warning This function is not fully supported yet. Do not use.
 *
 * \warning The added multi-instruction sequence contains several control-transfer
 * instructions and is not straight-line code, which can complicate subsequent analysis
 * routines.
 *
 * The client must use the \p drmgr Extension to order its instrumentation in order to
 * use this function.  This function must be called from the application-to-application
 * ("app2app") stage (see drmgr_register_bb_app2app_event()).
 *
 * This transformation is deterministic, so the caller can return
 * DR_EMIT_DEFAULT from its event.
 *
 * The *dq, *qd, *qq, *dpd, *qps, and *qpd opcodes are not yet supported in 32-bit mode.
 * In this case, the function will return false and no expansion will occur.
 *
 * @param[in]  drcontext   The opaque context.
 * @param[in]  bb          Instruction list passed to the app2app event.
 * @param[out] expanded    Whether any expansion occurred.
 *
 * \return whether successful.
 */
bool
drx_expand_scatter_gather(void *drcontext, instrlist_t *bb, OUT bool *expanded);

/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRX_H_ */
