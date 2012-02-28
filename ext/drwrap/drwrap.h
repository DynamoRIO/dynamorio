/* **********************************************************
 * Copyright (c) 2010-2012 Google, Inc.   All rights reserved.
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

/**
 * \addtogroup drwrap Function Wrapping and Replacing
 */
/*@{*/ /* begin doxygen group */

/***************************************************************************
 * INIT
 */

DR_EXPORT
/**
 * Initializes the drwrap extension.  Must be called prior to any of the
 * other routines, and should only be called once.
 *
 * \return whether successful.  Will return false if called a second time.
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
    DRMGR_PRIORITY_APP2APP_DRWRAP  = -500, /**< Priority of drwap_replace() */
    DRMGR_PRIORITY_INSERT_DRWRAP   =  500, /**< Priority of drwrap_wrap() */
};

/** Name of drmgr instrumentation pass priorities for both app2app and insert */
#define DRMGR_PRIORITY_NAME_DRWRAP "drwrap"

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


/***************************************************************************
 * FUNCTION WRAPPING
 */

DR_EXPORT
/**
 * Wraps the application function that starts at the address \p original
 * by calling \p pre_func_cb prior to every invocation of \p original
 * and calling \p post_func_cb after every invocation of \p original.
 * One of the callbacks can be NULL, but not both.
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
drwrap_wrap(app_pc func,
            void (*pre_func_cb)(void *wrapcxt, OUT void **user_data),
            void (*post_func_cb)(void *wrapcxt, void *user_data));

/** Values for the flags parameter to drwrap_wrap_ex() */
typedef enum {
    /**
     * If this flag is set, then when a Windows exception occurs, all
     * post-call callbacks for all live wrapped functions on the wrap
     * stack for which \p unwind_on_exception is true are called.  If
     * this flag is not set (the default), each post-call callback
     * will still be called if drwrap's heuristics later detect that
     * that particular callback has been bypassed, but those
     * heuristics are not guaranteed.
     */
    DRWRAP_UNWIND_ON_EXCEPTION    = 0x01,
} drwrap_wrap_flags_t;

DR_EXPORT
/**
 * Identical to drwrap_wrap() except for two additional parameters: \p
 * user_data, which is passed as the initial value of *user_data to \p
 * pre_func_cb, and \p flags.
 */
bool
drwrap_wrap_ex(app_pc func,
               void (*pre_func_cb)(void *wrapcxt, INOUT void **user_data),
               void (*post_func_cb)(void *wrapcxt, void *user_data),
               void *user_data, drwrap_wrap_flags_t flags);

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
drwrap_unwrap(app_pc func,
              void (*pre_func_cb)(void *wrapcxt, OUT void **user_data),
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
 * Assumes the regular C calling convention (i.e., no fastcall).
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
 * Assumes the regular C calling convention (i.e., no fastcall).
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
    DRWRAP_SAFE_READ_RETADDR    = 0x01,
    /**
     * By default function arguments stored in memory are read and
     * written directly.  A more conservative and safe approach would
     * use a safe read or write to avoid crashing when the stack is
     * unsafe to access.  This flag will cause all arguments in
     * memory to be read and written safely.  If any call to
     * drwrap_set_global_flags() sets this flag, no later call can
     * remove it.
     */
    DRWRAP_SAFE_READ_ARGS       = 0x02,
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
    DRWRAP_NO_FRILLS            = 0x04,
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
    DRWRAP_FAST_CLEANCALLS      = 0x08,
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
drwrap_is_wrapped(app_pc func,
                  void (*pre_func_cb)(void *wrapcxt, OUT void **user_data),
                  void (*post_func_cb)(void *wrapcxt, void *user_data));

DR_EXPORT
/**
 * \return whether \p pc is currently considered a post-wrap point, for any
 * wrap request.
 */
bool
drwrap_is_post_wrap(app_pc pc);

/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRWRAP_H_ */
