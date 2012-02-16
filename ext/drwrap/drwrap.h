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
 * Multiple wrap requests are allowed for one \p original function.
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
 * \return whether successful.
 */
bool
drwrap_wrap(app_pc func,
            void (*pre_func_cb)(void *wrapcxt, OUT void **user_data),
            void (*post_func_cb)(void *wrapcxt, void *user_data));

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


/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRWRAP_H_ */
