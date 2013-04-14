/* **********************************************************
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
#  ifdef DR_APP_EXPORTS
#    define DR_APP_API __declspec(dllexport)
#  else
#    define DR_APP_API __declspec(dllimport)
#  endif
#else /* UNIX */
#  if defined(DR_APP_EXPORTS) && defined(USE_VISIBILITY_ATTRIBUTES)
#    define DR_APP_API __attribute__ ((visibility ("default")))
#  else
#    define DR_APP_API
#  endif
#endif

/****************************************************************************
 * DR Application Interface
 */

/**
 * Application-wide initialization. Must be called before any other
 * API function. Returns zero on success.
 */
DR_APP_API int dr_app_setup(void);

/**
 * Application-wide cleanup.  Prints statistics. Returns zero on success.
 */
DR_APP_API int dr_app_cleanup(void);

/**
 * Causes application to run under DR control upon return from this call.
 * Attempts to take over any existing threads in the application.
 *
 * \warning On Linux, DR detects threads by listing thread ids in the current
 * process's thread group.  This, and other queries about the current process
 * may fail if the main thread has quit.  DR also assumes the threads all share
 * signal handlers, as is the case for pthreads.  Violating these assumptions
 * will lead to unpredictable behavior.
 *
 * \warning Windows does not yet attempt to take over existing threads.
 */
DR_APP_API void dr_app_start(void);

/**
 * Causes the application's current thread to run directly on the machine upon
 * return from this call; no effect if application is not currently running
 * under DR control.
 *
 * \note This only affects the current thread.  Other threads will still be
 * under DR's control.  This behavior may change in the future.
 */
DR_APP_API void dr_app_stop(void);

/**
 * Causes application to run under DR control upon return from 
 * this call.  DR never releases control. Useful for overriding 
 * dr_app_start/dr_app_stop calls in the rest of a program.
 */
DR_APP_API void dr_app_take_over(void);

/**
 * Calls dr_app_setup() and, if it succeeds, calls dr_app_start().  Returns the
 * result of dr_app_setup(), which returns zero on success.  This routine is
 * intended as a convenient single point of entry for callers who are using
 * dlsym() or GetProcAddress() to access the app API.
 */
DR_APP_API int dr_app_setup_and_start(void);

#endif /* _DR_APP_H_ */
