/* **********************************************************
 * Copyright (c) 2010 VMware, Inc.  All rights reserved.
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

#ifndef _DR_INJECT_H_
#define _DR_INJECT_H_ 1

/* DR_API EXPORT TOFILE dr_inject.h */
/* DR_API EXPORT BEGIN */
/****************************************************************************
 * Injection API
 */
/**
 * @file dr_inject.h
 * @brief Injection API.  Use these functions to launch processes under the
 * control of DynamoRIO.
 */

/**
 * Creates a new process for the executable and command line specified.
 * The initial thread in the process is suspended.
 * Use dr_inject_process_inject() to inject DynamoRIO into the process
 * (first calling dr_register_process() to configure the process, for
 * one-time targeted configuration), dr_inject_process_run() to resume
 * the thread, and dr_inject_process_exit() to finish and free
 * resources.
 *
 * \param[in]   app_name       The path to the target executable.  The caller
 *                             must ensure this data is valid until the
 *                             inject data is disposed.
 *
 * \param[in]   app_cmdline    A NULL-terminated array of strings representing
 *                             the app's command line.  This should match what
 *                             the app will receive as \p argv in main().  The
 *                             caller must ensure this data is valid until the
 *                             inject data is disposed.
 *
 * \param[out]  data           An opaque pointer that should be passed to
 *                             subsequent dr_inject_* routines to refer to
 *                             this process.
 * \return  Returns 0 on success.  On failure, returns a system error code.
 *          Regardless of success, caller must call dr_inject_process_exit()
 *          when finished to clean up internally-allocated resources.
 */
int
dr_inject_process_create(const char *app_name, const char **app_cmdline,
                         void **data);

#ifdef LINUX
/**
 * Prepare to exec() the provided command from the current process.  Use
 * dr_inject_process_inject() to perform the exec() under DR.
 *
 * \note Only available on Linux.
 *
 * \param[in]   app_name       The path to the target executable.  The caller
 *                             must ensure this data is valid until the
 *                             inject data is disposed.
 *
 * \param[in]   app_cmdline    A NULL-terminated array of strings representing
 *                             the app's command line.  This should match what
 *                             the app will receive as \p argv in main().  The
 *                             caller must ensure this data is valid until the
 *                             inject data is disposed.
 *
 * \param[out]  data           An opaque pointer that should be passed to
 *                             subsequent dr_inject_* routines to refer to
 *                             this process.
 * \return  Returns 0 on success.  On failure, returns a system error code.
 *          Regardless of success, caller must call dr_inject_process_exit()
 *          when finished to clean up internally-allocated resources.
 */
int
dr_inject_prepare_to_exec(const char *app_name, const char **app_cmdline,
                          void **data);
#endif /* LINUX */

/**
 * Injects DynamoRIO into a process created by dr_inject_process_create(), or
 * the current process if using dr_inject_prepare_to_exec() on Linux.
 *
 * \param[in]   data           The pointer returned by dr_inject_process_create()
 *
 * \param[in]   force_injection  Requests injection even if the process is
 *                               configured to not be run under DynamoRIO.
 *
 * \param[in]   library_path    The path to the DynamoRIO library to use.  If
 *                              NULL, the library that the target process is
 *                              configured for will be used.
 *
 * \return  Whether successful.
 */
bool
dr_inject_process_inject(void *data, bool force_injection,
                         const char *library_path);

/**
 * Resumes the suspended thread in a process created by dr_inject_process_create().
 *
 * \param[in]   data           The pointer returned by dr_inject_process_create()
 *
 * \return  Whether successful.
 */
bool
dr_inject_process_run(void *data);

/**
 * Frees resources used by dr_inject_process_create().
 *
 * \param[in]   data           The pointer returned by dr_inject_process_create()
 *
 * \param[in]   terminate      If true, the process is forcibly terminated.
 *
 * \return  Returns the exit code of the process.  If the caller did not wait
 *          for the process to finish before calling this, the code will be
 *          STILL_ACTIVE.
 */
int
dr_inject_process_exit(void *data, bool terminate);

/**
 * Returns the process name of a process created by dr_inject_process_create().
 *
 * \param[in]   data           The pointer returned by dr_inject_process_create()
 *
 * \return  Returns the process name of the process.  This is the file name
 *          without the path, suitable for passing to dr_register_process().
 */
char *
dr_inject_get_image_name(void *data);

#ifdef WINDOWS
/**
 * Returns a handle to a process created by dr_inject_process_create().
 *
 * \param[in]   data           The pointer returned by dr_inject_process_create()
 *
 * \note Windows only.
 *
 * \return  Returns the handle used by drinjectlib.  Do not close the handle: it
 *          will be closed in dr_inject_process_exit().
 */
HANDLE
dr_inject_get_process_handle(void *data);
#endif /* WINDOWS */

/**
 * Returns the pid of a process created by dr_inject_process_create().
 *
 * \param[in]   data           The pointer returned by dr_inject_process_create()
 *
 * \return  Returns the pid of the process.
 */
process_id_t
dr_inject_get_process_id(void *data);

/* Deliberately not documented: not fully supported */
bool
dr_inject_using_debug_key(void *data);

/**
 * Prints statistics for a process created by dr_inject_process_create().
 *
 * \param[in]   data           The pointer returned by dr_inject_process_create()
 *
 * \param[in]   elapsed_secs   Elapsed time recorded by the caller that will be
 *                             printed by this routine if showstats is true.
 *
 * \param[in]   showstats      If true, elapsed_secs and resource usage is printed.
 *
 * \param[in]   showmem        If true, memory usage statistics are printed.
 */
void
dr_inject_print_stats(void *data, int elapsed_secs, bool showstats, bool showmem);

/* DR_API EXPORT END */

#endif /* _DR_INJECT_H_ */
