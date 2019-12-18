/* **********************************************************
 * Copyright (c) 2013-2017 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2008 VMware, Inc.  All rights reserved.
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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/**
 * @file dr_app.h
 * @brief DR's application interface for running portions
 * of a program under its control.
 */

#ifndef _DR_APP_H_
#define _DR_APP_H_ 1

#ifdef WINDOWS
#    ifdef DR_APP_EXPORTS
#        define DR_APP_API __declspec(dllexport)
#    else
#        define DR_APP_API __declspec(dllimport)
#    endif
#else /* UNIX */
#    if defined(DR_APP_EXPORTS) && defined(USE_VISIBILITY_ATTRIBUTES)
#        define DR_APP_API __attribute__((visibility("default")))
#    else
#        define DR_APP_API
#    endif
#endif

/****************************************************************************
 * DR Application Interface
 */

/**
 * Application-wide initialization. Must be called before any other
 * API function. Returns zero on success.
 */
DR_APP_API int
dr_app_setup(void);

/**
 * Application-wide cleanup.  Prints statistics. Returns zero on success.
 * Once this is invoked, calling dr_app_start() or dr_app_setup() is not supported.
 * This should be invoked at application exit, after joining with
 * application threads.  If the application wants to continue
 * executing significant code or executing additional threads after
 * cleanup, it should use dr_app_stop_and_cleanup() instead.
 */
DR_APP_API int
dr_app_cleanup(void);

/**
 * Causes the application to run under DR control upon return from this call.
 * Attempts to take over any existing threads in the application.
 *
 * \warning On Linux, DR detects threads by listing thread ids in the current
 * process's thread group.  This, and other queries about the current process
 * may fail if the main thread has quit.  DR also assumes the threads all share
 * signal handlers, as is the case for pthreads.  Violating these assumptions
 * will lead to unpredictable behavior.
 */
DR_APP_API void
dr_app_start(void);

/**
 * Causes all of the application's threads to run directly on the machine upon
 * return from this call; no effect if the application is not currently running
 * under DR control.
 */
DR_APP_API void
dr_app_stop(void);

/**
 * Causes application to run under DR control upon return from
 * this call.  DR never releases control. Useful for overriding
 * dr_app_start/dr_app_stop calls in the rest of a program.
 */
DR_APP_API void
dr_app_take_over(void);

/**
 * Calls dr_app_setup() and, if it succeeds, calls dr_app_start().  Returns the
 * result of dr_app_setup(), which returns zero on success.  This routine is
 * intended as a convenient single point of entry for callers who are using
 * dlsym() or GetProcAddress() to access the app API.
 */
DR_APP_API int
dr_app_setup_and_start(void);

/**
 * Causes all of the application's threads to run directly on the machine upon
 * return from this call, and additionally frees the resources used by DR.
 * Once this is invoked, calling dr_app_start() is not supported until
 * dr_app_setup() or dr_app_setup_and_start() is called for a re-attach.
 * Re-attach, however, is considered an experimental feature and is
 * not guaranteed to be without problems, in particular when DR or
 * extension libraries are static and there is no simple way to reset
 * the state of global variables.
 *
 * This call has no effect if the application is not currently running
 * under DR control.
 */
DR_APP_API void
dr_app_stop_and_cleanup(void);

/**
 * Same as dr_app_stop_and_cleanup, additionally filling in the provided
 * dr_stats_t object, after all threads have been detached and
 * right before clearing stats. The parameter may be NULL, in which case
 * stats are not collected, the API behaving identically to
 * dr_app_stop_and_cleanup().
 */
DR_APP_API void
dr_app_stop_and_cleanup_with_stats(dr_stats_t *drstats);

/**
 * Indicates whether the current thread is running within the DynamoRIO code
 * cache. Returns \p true only if the current thread is running within the
 * DynamoRIO code cache and returns false othrewise.
 *
 * \note This routines returns \p false if the current thread is running
 * within the DynamoRIO probe mode.
 */
DR_APP_API bool
dr_app_running_under_dynamorio(void);

#ifdef LINUX
/**
 * DynamoRIO's (experimental) native execution mode supports running
 * some modules natively while the others run under DynamoRIO.
 * When a module is running natively, it may jump to a module that should be
 * executed under DynamoRIO directly without going through DynamoRIO.
 * To handle this situation, the application code should call this routine and
 * use the returned stub pc as the branch target instead.
 * \note Linux only.
 * \note Native execution mode only.
 * \note Experimental support.
 */
DR_APP_API void *
dr_app_handle_mbr_target(void *target);
#endif /* LINUX */

#endif /* _DR_APP_H_ */
