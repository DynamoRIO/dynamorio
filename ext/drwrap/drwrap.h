/* **********************************************************
 * Copyright (c) 2010-2020 Google, Inc.   All rights reserved.
 * **********************************************************/

/* drwrap: DynamoRIO Function Wrapping and Replacing Extension
 * Derived from Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* DynamoRIO Function Wrapping and Replacing Extension */

#ifndef _DRWRAP_H_
#define _DRWRAP_H_ 1

/**
 * @file drwrap.h
 * @brief Header for DynamoRIO Function Wrapping and Replacing Extension
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "drext.h"

/**
 * \addtogroup drwrap Function Wrapping and Replacing
 */
/*@{*/ /* begin doxygen group */

/* Users of drwrap need to use the drmgr versions of these events to ensure
 * that drwrap's actions occur at the right time.
 */
#define dr_register_bb_event DO_NOT_USE_bb_event_USE_drmgr_bb_events_instead
#define dr_unregister_bb_event DO_NOT_USE_bb_event_USE_drmgr_bb_events_instead
#define dr_get_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
#define dr_set_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
#define dr_insert_read_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
#define dr_insert_write_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
#define dr_register_thread_init_event DO_NOT_USE_thread_event_USE_drmgr_events_instead
#define dr_unregister_thread_init_event DO_NOT_USE_thread_event_USE_drmgr_events_instead
#define dr_register_thread_exit_event DO_NOT_USE_thread_event_USE_drmgr_events_instead
#define dr_unregister_thread_exit_event DO_NOT_USE_thread_event_USE_drmgr_events_instead

/***************************************************************************
 * INIT
 */

DR_EXPORT
/**
 * Initializes the drwrap extension.  Must be called prior to any of the
 * other routines.  Can be called multiple times (by separate components,
 * normally) but each call must be paired with a corresponding call to
 * drwrap_exit().
 *
 * \return whether successful.
 */
bool
drwrap_init(void);

DR_EXPORT
/**
 * Cleans up the drwrap extension.
 */
void
drwrap_exit(void);

/***************************************************************************
 * FUNCTION REPLACING
 */

/**
 * Priorities of drmgr instrumentation passes used by drwrap.  Users
 * of drwrap can use the name DRMGR_PRIORITY_NAME_DRWRAP in the
 * drmgr_priority_t.before field or can use these numeric priorities
 * in the drmgr_priority_t.priority field to ensure proper
 * instrumentation pass ordering.
 */
enum {
    DRMGR_PRIORITY_APP2APP_DRWRAP = -500, /**< Priority of drwrap_replace() */
    DRMGR_PRIORITY_INSERT_DRWRAP = 500,   /**< Priority of drwrap_wrap() */
    DRMGR_PRIORITY_FAULT_DRWRAP = 500,    /**< Priority of fault handling event */
};

/**
 * Name of drmgr instrumentation pass priorities for app2app, insert, and
 * exception on Windows.
 */
#define DRMGR_PRIORITY_NAME_DRWRAP "drwrap"

/** Spill slot used to store user_data parameter for drwrap_replace_native() */
#define DRWRAP_REPLACE_NATIVE_DATA_SLOT SPILL_SLOT_2

/**
 * Spill slot used to store application stack address (or DR_REG_LR for ARM)
 * for drwrap_replace_native().
 */
#define DRWRAP_REPLACE_NATIVE_SP_SLOT SPILL_SLOT_3

DR_EXPORT
/**
 * Replaces the application function that starts at the address \p original
 * with the code at the address \p replacement.
 *
 * Only one replacement is supported per target address.  If a
 * replacement already exists for \p original, this function fails
 * unless \p override is true, in which case it replaces the prior
 * replacement.  To remove a replacement, pass NULL for \p replacement
 * and \b true for \p override.  When removing or replacing a prior
 * replacement, existing replaced code in the code cache will be
 * flushed lazily: i.e., there may be some execution in other threads
 * after this call is made.
 *
 * Only the first target replacement address in a basic block will be
 * honored.  All code after that address is removed.
 *
 * When replacing a function, it is up to the user to ensure that the
 * replacement mirrors the calling convention and other semantics of the
 * original function.  The replacement code will be executed as application
 * code, NOT as client code.
 *
 * \note The priority of the app2app pass used here is
 * DRMGR_PRIORITY_APP2APP_DRWRAP and its name is
 * DRMGR_PRIORITY_NAME_DRWRAP.
 *
 * \return whether successful.
 */
bool
drwrap_replace(app_pc original, app_pc replacement, bool override);

DR_EXPORT
/**
 * \warning This interface is in flux and is subject to change in the
 * next release.  Consider it experimental in this release.
 *
 * Replaces the application function that starts at the address \p
 * original with the natively-executed (i.e., as the client) code at
 * the address \p replacement.  The replacement should either be
 * the function entry point or a call site for the function, indicated
 * by the \p at_entry parameter.  For a call site, only that particular
 * call will be replaced, rather than every call to \p replacement.
 *
 * The replacement function must call drwrap_replace_native_fini()
 * prior to returning.  If it fails to do so, control will be lost and
 * subsequent application code will not be under DynamoRIO control.
 * The fini routine sets up a continuation function that is used
 * rather than a direct return.  This continuation strategy enables
 * the replacement function to use application locks (if they are
 * marked with dr_mark_safe_to_suspend()) safely, as there is no code
 * cache return point.
 *
 * The replacement function should use the same calling convention as
 * the original with respect to argument access.  In order to match
 * the calling convention return for conventions in which the callee
 * cleans up arguments on the stack, use the \p stack_adjust parameter
 * to request a return that adjusts the stack.  This return will be
 * executed as a regular basic block and thus a stack-tracking client
 * will not observe any missing stack adjustments.  The \p stack_adjust
 * parameter must be a multiple of sizeof(void*).
 *
 * If \p user_data != NULL, it is stored in a scratch slot for access
 * by \p replacement by calling dr_read_saved_reg() and passing
 * DRWRAP_REPLACE_NATIVE_DATA_SLOT.
 *
 * Only one replacement is supported per target address.  If a
 * replacement already exists for \p original, this function fails
 * unless \p override is true, in which case it replaces the prior
 * replacement.  To remove a replacement, pass NULL for \p replacement
 * and \b true for \p override.  When removing or replacing a prior
 * replacement, existing replaced code in the code cache will be
 * flushed lazily: i.e., there may be some execution in other threads
 * after this call is made.
 *
 * Non-native replacements take precedence over native.  I.e., if a
 * drwrap_replace() replacement exists for \p original, then a native
 * replacement request for \p original will never take effect.
 *
 * Only the first target replacement address in a basic block will be
 * honored.  All code after that address is removed.
 *
 * When replacing a function, it is up to the user to ensure that the
 * replacement mirrors the calling convention and other semantics of the
 * original function.
 *
 * The replacement code will be executed as client code, NOT as
 * application code.  However, it will use the application stack and
 * other machine state.  Usually it is good practice to call
 * dr_switch_to_app_state() inside the replacement code, and then
 * dr_switch_to_dr_state() before returning, in particular on Windows.
 * To additionally use a clean DR stack, consider using
 * dr_call_on_clean_stack() from the initial replacement layer (which
 * allows the outer layer to handle stdcall, which
 * dr_call_on_clean_stack does not support).
 *
 * The replacement code is not allowed to invoke dr_flush_region() or
 * dr_delete_fragment() as it has no #dr_mcontext_t with which to
 * invoke dr_redirect_execution(): it must return to the call-out point
 * in the code cache.  If the replacement code does not return to its
 * return address, DR will lose control of the application and not
 * continue executing it properly.
 *
 * @param[in] original  The address of either the application function entry
 *   point (in which case \p at_entry must be true) or of a call site (the
 *   actual call or tailcall/inter-library jump) (in which case \p at_entry
 *   must be false).
 * @param[in] replacement  The function entry to use instead.
 * @param[in] at_entry  Indicates whether \p original is the function entry
 *   point or a call site.
 * @param[in] stack_adjust  The stack adjustment performed at return for the
 *   calling convention used by \p original.
 *   On ARM, this must be zero.
 * @param[in] user_data  Data made available when \p replacement is
 *   executed.
 * @param[in] override  Whether to replace any existing replacement for \p
 *   original.
 *
 * \note The mechanism used for a native replacement results in a \p
 * ret instruction appearing in the code stream with an application
 * address that is different from an execution without a native
 * replacement.  The return address will be identical, however,
 * assuming \p original does not replace its own return address.
 *
 * \note The application stack address at which its return address is
 * stored is available by calling dr_read_saved_reg() and passing
 * DRWRAP_REPLACE_NATIVE_SP_SLOT.
 *
 * \note The priority of the app2app pass used here is
 * DRMGR_PRIORITY_APP2APP_DRWRAP and its name is
 * DRMGR_PRIORITY_NAME_DRWRAP.
 *
 * \note Far calls are not supported.
 *
 * \return whether successful.
 */
bool
drwrap_replace_native(app_pc original, app_pc replacement, bool at_entry,
                      uint stack_adjust, void *user_data, bool override);

DR_EXPORT
/** \return whether \p func is currently replaced via drwrap_replace() */
bool
drwrap_is_replaced(app_pc func);

DR_EXPORT
/** \return whether \p func is currently replaced via drwrap_replace_native() */
bool
drwrap_is_replaced_native(app_pc func);

DR_EXPORT
/**
 * The replacement function passed to drwrap_replace_native() must
 * call this function prior to returning.  If this function is not called,
 * DynamoRIO will lose control of the application.
 */
void
drwrap_replace_native_fini(void *drcontext);

/***************************************************************************
 * FUNCTION WRAPPING
 */

DR_EXPORT
/**
 * Wraps the application function that starts at the address \p original
 * by calling \p pre_func_cb prior to every invocation of \p original
 * and calling \p post_func_cb after every invocation of \p original.
 * One of the callbacks can be NULL, but not both. Uses the default
 * calling convention for the platform (see DRWRAP_CALLCONV_DEFAULT
 * in #drwrap_callconv_t).
 *
 * Wrap requests should normally be made up front during process
 * initialization or module load (see
 * dr_register_module_load_event()).  If a wrap request is made after
 * the target code may have already been executed by the application,
 * the caller should flush the target code from the cache using the
 * desired flush method after issuing the wrap request.
 *
 * Multiple wrap requests are allowed for one \p original function
 * (unless #DRWRAP_NO_FRILLS is set).
 * Their callbacks are called sequentially in the reverse order of
 * registration.
 *
 * The \p pre_func_cb can examine (drwrap_get_arg()) and set
 * (drwrap_set_arg()) the arguments to \p original and can skip the
 * call to \p original (drwrap_skip_call()).  The \p post_func_cb can
 * examine (drwrap_get_retval()) and set (drwrap_set_retval()) \p
 * original's return value.  The opaque pointer \p wrapcxt passed to
 * each callback should be passed to these routines.
 *
 * When an abnormal stack unwind, such as longjmp or a Windows
 * exception, occurs, drwrap does its best to detect it.  All
 * post-calls that would be missed will still be invoked, but with \p
 * wrapcxt set to NULL.  Since there is no post-call environment, it
 * does not make sense to query the return value or arguments.  The
 * call is invoked to allow for cleanup of state allocated in \p
 * pre_func_cb.  However, detection of a stack unwind is not
 * guaranteed.  When wrapping a series of functions that do not
 * themselves contain exception handlers, pass the
 * DRWRAP_UNWIND_ON_EXCEPTION flag to drwrap_wrap_ex() to ensure that
 * all post-call callbacks will be called on an exception.
 *
 * \note The priority of the app2app pass used here is
 * DRMGR_PRIORITY_INSERT_DRWRAP and its name is
 * DRMGR_PRIORITY_NAME_DRWRAP.
 *
 * \return whether successful.
 */
bool
drwrap_wrap(app_pc func, void (*pre_func_cb)(void *wrapcxt, OUT void **user_data),
            void (*post_func_cb)(void *wrapcxt, void *user_data));

/**
 * Values for the flags parameter to drwrap_wrap_ex(), which may also be
 * combined with at most one value from #drwrap_callconv_t (using bitwise OR).
 */
typedef enum {
    /** Provided for convenience when calling drwrap_wrap_ex() with no flags. */
    DRWRAP_FLAGS_NONE = 0x00,
    /**
     * When a Windows exception occurs, all
     * post-call callbacks for all live wrapped functions on the wrap
     * stack for which this flag is set are called.  If
     * this flag is not set (the default), each post-call callback
     * will still be called if drwrap's heuristics later detect that
     * that particular callback has been bypassed, but those
     * heuristics are not guaranteed.
     */
    DRWRAP_UNWIND_ON_EXCEPTION = 0x01,
    /**
     * If this flag is set, then post-call callbacks are only invoked from return
     * sites that can be identified statically.  Static identification happens in
     * two ways: from observing a CALL instruction, and from drwrap_mark_as_post_call().
     * Dynamically observing return addresses from inside callees incurs overhead
     * due to synchronization costs, with further overhead to replace existing
     * code with instrumented code.  When this flag is set, some post-call callbacks
     * may be missed.
     */
    DRWRAP_NO_DYNAMIC_RETADDRS = 0x02,
    /**
     * If this flag is set, then post-call points are identified by changing the
     * application return address upon entering the callee.  This is more efficient than
     * the default method, which requires shared storage and locks and flushing.
     * However, this does violate transparency, and may cause some applications to fail.
     * In particular, detaching on AArchXX requires scanning the stack to find where the
     * return address was stored, which could conceivably replace an integer or
     * non-pointer value that happens to match the sentinel used.  Use this at your own
     * risk.
     */
    DRWRAP_REPLACE_RETADDR = 0x04,
} drwrap_wrap_flags_t;

/* offset of drwrap_callconv_t in drwrap_wrap_flags_t */
#define DRWRAP_CALLCONV_FLAG_SHIFT 0x18

/**
 * Values to specify the calling convention of the wrapped function. Pass one of
 * these values to drwrap_wrap_ex() in the flags parameter using bitwise OR, e.g.:
 * DRWRAP_UNWIND_ON_EXCEPTION | DRWRAP_CALLCONV_DEFAULT (see #drwrap_wrap_flags_t).
 */
typedef enum {
    /** The AMD64 ABI calling convention. */
    DRWRAP_CALLCONV_AMD64 = 0x01000000,
    /** The Microsoft x64 calling convention. */
    DRWRAP_CALLCONV_MICROSOFT_X64 = 0x02000000,
    /** The ARM calling convention. */
    DRWRAP_CALLCONV_ARM = 0x03000000,
    /** The IA-32 cdecl calling convention. */
    DRWRAP_CALLCONV_CDECL = 0x04000000,
    /* For the purposes of drwrap, stdcall is an alias to cdecl, since the
     * only difference is whether the caller or callee cleans up the stack.
     */
    /** The Microsoft IA-32 stdcall calling convention. */
    DRWRAP_CALLCONV_STDCALL = DRWRAP_CALLCONV_CDECL,
    /** The IA-32 fastcall calling convention. */
    DRWRAP_CALLCONV_FASTCALL = 0x05000000,
    /** The Microsoft IA-32 thiscall calling convention. */
    DRWRAP_CALLCONV_THISCALL = 0x06000000,
    /** The ARM AArch64 calling convention. */
    DRWRAP_CALLCONV_AARCH64 = 0x07000000,
#ifdef X64
#    ifdef AARCH64
    /** Default calling convention for the platform. */
    DRWRAP_CALLCONV_DEFAULT = DRWRAP_CALLCONV_AARCH64,
#    elif defined(UNIX) /* x64 */
    /** Default calling convention for the platform. */
    DRWRAP_CALLCONV_DEFAULT = DRWRAP_CALLCONV_AMD64,
#    else               /* WINDOWS x64 */
    /** Default calling convention for the platform. */
    DRWRAP_CALLCONV_DEFAULT = DRWRAP_CALLCONV_MICROSOFT_X64,
#    endif
#else /* 32-bit */
#    ifdef ARM
    /** Default calling convention for the platform. */
    DRWRAP_CALLCONV_DEFAULT = DRWRAP_CALLCONV_ARM,
#    else /* x86: UNIX or WINDOWS */
    /** Default calling convention for the platform. */
    DRWRAP_CALLCONV_DEFAULT = DRWRAP_CALLCONV_CDECL,
#    endif
#endif
    /** The platform-specific calling convention for a vararg function. */
    DRWRAP_CALLCONV_VARARG = DRWRAP_CALLCONV_DEFAULT,
    /* Mask for isolating the calling convention from other flags. */
    DRWRAP_CALLCONV_MASK = 0xff000000
} drwrap_callconv_t;

DR_EXPORT
/**
 * Identical to drwrap_wrap() except for two additional parameters: \p
 * user_data, which is passed as the initial value of *user_data to \p
 * pre_func_cb, and \p flags, which are the bitwise combination of the
 * #drwrap_wrap_flags_t and at most one #drwrap_callconv_t.
 *
 * Specify the calling convention by combining one of the DRWRAP_CALLCONV_*
 * values (of #drwrap_callconv_t) with the flags. It is not allowed to specify
 * multiple calling conventions. If the specified calling convention is
 * incorrect for \p func, the wrap will succeed, but calls to drwrap_set_arg()
 * and drwrap_get_arg() for \p func will either access the wrong argument
 * value, or will access a register or stack slot that does not contain
 * any argument value. If no calling convention is specified, defaults
 * to DRWRAP_CALLCONV_DEFAULT.
 */
bool
drwrap_wrap_ex(app_pc func, void (*pre_func_cb)(void *wrapcxt, INOUT void **user_data),
               void (*post_func_cb)(void *wrapcxt, void *user_data), void *user_data,
               uint flags);

DR_EXPORT
/**
 * Removes a previously-requested wrap for the function \p func
 * and the callback pair \p pre_func_cb and \p post_func_cb.
 * This must be the same pair that was passed to \p dr_wrap.
 *
 * This routine can be called from \p pre_func_cb or \p post_func_cb.
 *
 * \return whether successful.
 */
bool
drwrap_unwrap(app_pc func, void (*pre_func_cb)(void *wrapcxt, OUT void **user_data),
              void (*post_func_cb)(void *wrapcxt, void *user_data));

DR_EXPORT
/**
 * Returns the DynamoRIO context.  This routine can be faster than
 * dr_get_current_drcontext() but should return the same result.
 */
app_pc
drwrap_get_drcontext(void *wrapcxt);

DR_EXPORT
/**
 * Returns the address of the wrapped function represented by
 * \p wrapcxt.
 */
app_pc
drwrap_get_func(void *wrapcxt);

DR_EXPORT
/**
 * Returns the machine context of the wrapped function represented by
 * \p wrapcxt corresponding to the application state at the time
 * of the pre-function or post-function wrap callback.
 * In order for any changes to the returned context to take
 * effect, drwrap_set_mcontext() must be called.
 *
 * \note if the #DRWRAP_FAST_CLEANCALLS flag is set, caller-saved
 * register values in the fields controlled by #DR_MC_MULTIMEDIA will
 * not contain valid values.
 */
dr_mcontext_t *
drwrap_get_mcontext(void *wrapcxt);

DR_EXPORT
/**
 * Identical to drwrap_get_mcontext() but only fills in the state
 * indicated by \p flags.
 */
dr_mcontext_t *
drwrap_get_mcontext_ex(void *wrapcxt, dr_mcontext_flags_t flags);

DR_EXPORT
/**
 * Propagates any changes made to the dr_mcontext_t pointed by
 * drwrap_get_mcontext() back to the application.
 *
 * \note if the #DRWRAP_FAST_CLEANCALLS flag is set, caller-saved
 * register values in the fields controlled by #DR_MC_MULTIMEDIA will
 * not contain valid values, but this should be fine because
 * their values were scratch according to the ABI at the
 * wrap point..
 */
bool
drwrap_set_mcontext(void *wrapcxt);

DR_EXPORT
/**
 * Returns the return address of the wrapped function represented by
 * \p wrapcxt.
 *
 * This routine may de-reference application memory directly, so the
 * caller should wrap in DR_TRY_EXCEPT if crashes must be avoided.
 */
app_pc
drwrap_get_retaddr(void *wrapcxt);

DR_EXPORT
/**
 * Returns the value of the \p arg-th argument (0-based)
 * to the wrapped function represented by \p wrapcxt.
 * Uses the calling convention set by drwrap_wrap_ex(), or for drwrap_wrap()
 * assumes the regular C calling convention.
 * May only be called from a \p drwrap_wrap pre-function callback.
 * To access argument values in a post-function callback,
 * store them in the \p user_data parameter passed between
 * the pre and post functions.
 *
 * This routine may de-reference application memory directly, so the
 * caller should wrap in DR_TRY_EXCEPT if crashes must be avoided.
 */
void *
drwrap_get_arg(void *wrapcxt, int arg);

DR_EXPORT
/**
 * Sets the the \p arg-th argument (0-based) to the wrapped function
 * represented by \p wrapcxt to \p val.
 * Uses the calling convention set by drwrap_wrap_ex(), or for drwrap_wrap()
 * assumes the regular C calling convention.
 * May only be called from a \p drwrap_wrap pre-function callback.
 * To access argument values in a post-function callback,
 * store them in the \p user_data parameter passed between
 * the pre and post functions.
 *
 * This routine may write to application memory directly, so the
 * caller should wrap in DR_TRY_EXCEPT if crashes must be avoided.
 * \return whether successful.
 */
bool
drwrap_set_arg(void *wrapcxt, int arg, void *val);

DR_EXPORT
/**
 * Returns the return value of the wrapped function
 * represented by \p wrapcxt.
 * Assumes a pointer-sized return value.
 * May only be called from a \p drwrap_wrap post-function callback.
 */
void *
drwrap_get_retval(void *wrapcxt);

DR_EXPORT
/**
 * Sets the return value of the wrapped function
 * represented by \p wrapcxt to \p val.
 * Assumes a pointer-sized return value.
 * May only be called from a \p drwrap_wrap post-function callback.
 * \return whether successful.
 */
bool
drwrap_set_retval(void *wrapcxt, void *val);

DR_EXPORT
/**
 * May only be called from a \p drwrap_wrap pre-function callback.
 * Skips execution of the original function and returns to the
 * function's caller with a return value of \p retval.
 * The post-function callback will not be invoked; nor will any
 * pre-function callbacks (if multiple were registered) that
 * have not yet been called.
 * If the original function uses the \p stdcall calling convention,
 * the total size of its arguments must be supplied.
 * The return value is set regardless of whether the original function
 * officially returns a value or not.
 * Further state changes may be made with drwrap_get_mcontext() and
 * drwrap_set_mcontext() prior to calling this function.
 *
 * \note It is up to the client to ensure that the application behaves
 * as desired when the original function is skipped.
 *
 * \return whether successful.
 */
bool
drwrap_skip_call(void *wrapcxt, void *retval, size_t stdcall_args_size);

DR_EXPORT
/**
 * May only be called from a drwrap_wrap() post-function callback.
 * Redirects execution to the \p pc specified in the #dr_mcontext_t of the
 * \p wrapcxt after executing all remaining post-function callbacks.
 * Automatically calls \p drwrap_set_mcontext to make the redirection
 * to \p pc effective; calls to drwrap_set_mcontext() from subsequent
 * post-function callbacks will be denied to prevent clobbering the
 * redirection mcontext. Redirecting execution from nested
 * invocations of a recursive function is not supported.
 *
 * \note It is the client's responsibility to adjust the register
 * state and/or memory to accommodate the redirection target;
 * otherwise the application may behave in unexpected ways. If the
 * client intends to repeat execution of the wrapped function, the
 * stack pointer must be adjusted accordingly during the
 * post-function callback so that the correct return address is
 * in the conventional location before execution enters the wrapped
 * function. This is necessary because the pre-function callback
 * occurs at the beginning of the wrapped function (i.e., after the
 * call instruction has executed), while the post-function callback
 * occurs after the return instruction has executed (as if inserted
 * following the call instruction).
 *
 * \return DREXT_SUCCESS if the redirect request is accepted;
 * DREXT_ERROR_STATE_INCOMPATIBLE if this function was called outside
 * of a post-function callback, or DREXT_ERROR if the redirect could
 * not be fulfilled for any other reason.
 */
drext_status_t
drwrap_redirect_execution(void *wrapcxt);

DR_EXPORT
/**
 * May only be called from a \p drwrap_wrap post-function callback.
 * This function queries the drwrap state to determine whether a prior
 * post-function callback has requested redirection to another \p pc
 * (in which case the #dr_mcontext_t in the \p wrapcxt may no longer be changed).
 *
 * \return true if a prior post-function callback has requested a redirect
 */
bool
drwrap_is_redirect_requested(void *wrapcxt);

DR_EXPORT
/**
 * Registers a callback \p cb to be called every time a new post-call
 * address is encountered.  The intended use is for tools that want
 * faster start-up time by avoiding flushes for inserting wrap
 * instrumentation at post-call sites.  A tool can use this callback
 * to record all of the post-call addresses to disk, and use
 * drwrap_mark_as_post_call() during module load of the next
 * execution.  It is up to the tool to verify that the module has not
 * changed since its addresses were recorded.

 * \return whether successful.
 */
bool
drwrap_register_post_call_notify(void (*cb)(app_pc pc));

DR_EXPORT
/**
 * Unregisters a callback registered with drwrap_register_post_call_notify().
 * \return whether successful.
 */
bool
drwrap_unregister_post_call_notify(void (*cb)(app_pc pc));

DR_EXPORT
/**
 * Records the address \p pc as a post-call address for
 * instrumentation for post-call function wrapping purposes.
 *
 * \note Only call this when the code leading up to \p pc is
 * legitimate, as that code will be stored for consistency purposes
 * and the post-call entry will be invalidated if it changes.  This
 * means that when using this routine for the performance purposes
 * described in the drwrap_register_post_call_notify() documentation,
 * the tool should wait for a newly loaded module to be relocated
 * before calling this routine.  A good approach is to wait for the
 * first execution of code from the new module.
 *
 * \return whether successful.
 */
bool
drwrap_mark_as_post_call(app_pc pc);

/** Values for the flags parameter to drwrap_set_global_flags() */
typedef enum {
    /**
     * By default the return address is read directly.  A more
     * conservative and safe approach would use a safe read to avoid
     * crashing when the stack is unsafe to access.  This flag will
     * cause the return address to be read safely.  If any call to
     * drwrap_set_global_flags() sets this flag, no later call can
     * remove it.
     */
    DRWRAP_SAFE_READ_RETADDR = 0x01,
    /**
     * By default function arguments stored in memory are read and
     * written directly.  A more conservative and safe approach would
     * use a safe read or write to avoid crashing when the stack is
     * unsafe to access.  This flag will cause all arguments in
     * memory to be read and written safely.  If any call to
     * drwrap_set_global_flags() sets this flag, no later call can
     * remove it.
     */
    DRWRAP_SAFE_READ_ARGS = 0x02,
    /**
     * If this flag is set, then a leaner wrapping mechanism is used
     * with lower overhead.  However, several features are not
     * supported with this flag:
     * - Only one wrap request per address is allowed.  A second request
     *   will fail, even if an earlier request was unwrapped, unless
     *   the same pre and post callback functions are used.
     * - Wrapping should occur prior to any execution (e.g., at startup
     *   or module load time).  A new wrap request that occurs between
     *   the pre and post wrap points may have its post callback called
     *   even though its pre callback was never called.
     * - Unwrapping should only happen on module unload.  It is not
     *   supported between a pre and post callback.
     * Only set this flag if you are certain that all uses of wrapping
     * in your client and all libraries it uses can abide the above
     * restrictions.
     * Once set, this flag cannot be unset.
     */
    DRWRAP_NO_FRILLS = 0x04,
    /**
     * If this flag is set, then a leaner clean call is used to invoke
     * wrap pre callbacks.  This clean call assumes that all wrap requests
     * are for function entrance points and that standard ABI
     * calling conventions are used for those functions.
     * This means that caller-saved registers may not be saved and
     * thus will have invalid values in drwrap_get_mcontext().  When
     * using this setting and skipping a function via
     * drwrap_skip_call() (or calling dr_redirect_execution()
     * directly), setting xmm registers (in particular those used as
     * return values) will work correctly (of course, be sure to
     * retrieve the existing xmm values via drwrap_get_mcontext() or
     * drwrap_get_mcontext_ex(DR_MC_ALL) first).
     *
     * Only set this flag if you are certain that all uses of wrapping
     * in your client and all libraries it uses can abide the above
     * restrictions.
     * Once set, this flag cannot be unset.
     */
    DRWRAP_FAST_CLEANCALLS = 0x08,
} drwrap_global_flags_t;

DR_EXPORT
/**
 * Sets flags that affect the global behavior of the drwrap module.
 * This can be called at any time and it will affect future behavior.
 * \return whether the flags were changed.
 */
bool
drwrap_set_global_flags(drwrap_global_flags_t flags);

DR_EXPORT
/**
 * \return whether \p func is currently wrapped with \p pre_func_cb
 * and \p post_func_cb.
 */
bool
drwrap_is_wrapped(app_pc func, void (*pre_func_cb)(void *wrapcxt, OUT void **user_data),
                  void (*post_func_cb)(void *wrapcxt, void *user_data));

DR_EXPORT
/**
 * \return whether \p pc is currently considered a post-wrap point, for any
 * wrap request.
 */
bool
drwrap_is_post_wrap(app_pc pc);

/** An integer sized for support by dr_atomic_addX_return_sum(). */
typedef ptr_int_t atomic_int_t;

/**
 * Contains statistics retrievable by drwrap_get_stats().
 */
typedef struct _drwrap_stats_t {
    /** The size of this structure. Set this to sizeof(drwrap_stats_t). */
    size_t size;
    /**
     * The total number of code cache flushes.  These occur due to return points
     * already existing in the cache; replacing existing instrumentation; and
     * removing wrap or replace instrumentation.
     */
    atomic_int_t flush_count;
} drwrap_stats_t;

DR_EXPORT
/**
 * Retrieves various statistics exported by DR as global, process-wide values.
 * The API is not thread-safe.
 * The caller is expected to pass a pointer to a valid, initialized dr_stats_t
 * value, with the size field set (see dr_stats_t).
 * Returns false if stats are not enabled.
 */
bool
drwrap_get_stats(INOUT drwrap_stats_t *stats);

/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRWRAP_H_ */
