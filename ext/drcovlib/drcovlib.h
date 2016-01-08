/* **********************************************************
 * Copyright (c) 2016 Google, Inc.   All rights reserved.
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

/* DynamoRIO Code Coverage Library */

#ifndef _DRCOVLIB_H_
#define _DRCOVLIB_H_ 1

#include "drmgr.h"

/**
 * @file drcovlib.h
 * @brief Header for DynamoRIO Code Coverage Library
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup drcovlib Code Coverage Library
 */
/*@{*/ /* begin doxygen group */

/** Success code for each drcovlib operation */
typedef enum {
    DRCOVLIB_SUCCESS,                  /**< Operation succeeded. */
    DRCOVLIB_ERROR,                    /**< Operation failed. */
    DRCOVLIB_ERROR_INVALID_PARAMETER,  /**< Operation failed: invalid parameter */
    DRCOVLIB_ERROR_INVALID_SETUP,      /**< Operation failed: invalid DynamoRIO setup */
    DRCOVLIB_ERROR_FEATURE_NOT_AVAILABLE, /**< Operation failed: not available */
} drcovlib_status_t;

/** Bitmask flags for use in #drcovlib_options_t.flags. */
typedef enum {
    /**
     * Requests to dump the log file in text format.  By default the log file is
     * in binary format.  When in text format, the log file is \em not readable
     * by the post-processing tool \ref sec_drcov2lcov.
     */
    DRCOVLIB_DUMP_AS_TEXT,
    /**
     * By default, coverage information is dumped in a single process-wide log
     * file.  If DynamoRIO is run with thread-private code caches (i.e.,
     * dr_using_all_private_caches() returns true) and this flag is enabled,
     * then per-thread coverage information will be stored and dumped in \p
     * drcovlib's own thread exit events rather than in drcovlib_exit().
     */
    DRCOVLIB_THREAD_PRIVATE,
} drcovlib_flags_t;

/** Specifies the options when initializing drcovlib. */
typedef struct _drcovlib_options_t {
    /** Set this to the size of this structure. */
    size_t struct_size;
    /** Sets options specified by the bitmask values described in #drcovlib_flags_t. */
    drcovlib_flags_t flags;
    /**
     * By default, log files are stored in the current directory.  This option
     * overrides that default.
     */
    const char *logdir;
    /**
     * This is an experimental option for running natively (i.e., not under
     * DynamoRIO control) until the nth thread, where n is the value of this
     * option, is created.  This option only works under Windows.
     */
    int native_until_thread;
} drcovlib_options_t;

/***************************************************************************
 * Coverage log file format for use in postprocessing.
 *
 * XXX: this is not a sufficient description: special strings for the version,
 * flavor, module header, bb header, and module table entry are assumed in
 * drcov2lcov.  It seems better to move the specific parsing into drcovlib and
 * make drcovlib usable in standalone mode.
 */

/* file format version */
#define DRCOV_VERSION 2

/* i#1532: drsyms can't mix arch for ELF */
#ifdef LINUX
# ifdef X64
#  define DRCOV_ARCH_FLAVOR "-64"
# else
#  define DRCOV_ARCH_FLAVOR "-32"
# endif
#else
# define DRCOV_ARCH_FLAVOR ""
#endif

/* The bb_entry_t is used by both drcov client and post processing drcov2lcov.
 * It has different sizes, and different members with other types of coverage
 * (xref the now-removed CBR_COVERAGE).
 * We use different flavor markers to make sure the drcov2lcov process has the
 * right log file generated from corrsponding drcov client.
 */
#define DRCOV_FLAVOR "drcov" DRCOV_ARCH_FLAVOR

/* Data structure for the coverage info itself */
typedef struct _bb_entry_t {
    uint   start;      /* offset of bb start from the image base */
    ushort size;
    ushort mod_id;
} bb_entry_t;

/***************************************************************************
 * Exported functions
 */

DR_EXPORT
/**
 * Initializes the drcovlib extension.  Must be called prior to any of the
 * other routines.  Can be called multiple times (by separate components,
 * normally) but each call must be paired with a corresponding call to
 * drcovlib_exit().
 *
 * Once this routine is called, drcovlib's operation goes into effect and it
 * starts collecting coverage information.
 *
 * @param[in] ops  Specifies the optional parameters that control how
 *   drcovlib operates.  Only the first caller's options are honored.
 *
 * @return whether successful or an error code on failure.
 */
drcovlib_status_t
drcovlib_init(drcovlib_options_t *ops);

DR_EXPORT
/**
 * Dumps the coverage information for this process into its log file and cleans
 * up all resources allocated by the \p drcovlib extension.
 * If #DRCOVLIB_THREAD_PRIVATE is in effect, coverage information is instead dumped
 * in \p drcovlib's own thread exit events.
 *
 * @return whether successful or an error code on failure.
 */
drcovlib_status_t
drcovlib_exit(void);

DR_EXPORT
/**
 * Returns the name of the log file for this process (or for the thread
 * specified by \p drcontext, if #DRCOVLIB_THREAD_PRIVATE is in effect).
 *
 * @param[in] drcontext  If #DRCOVLIB_THREAD_PRIVATE is in effect, specifies which
 *   thread's log file name should be returned.  Otherwise this must be NULL.
 * @param[out] path  The full path to the requested log file.
 *
 * @return whether successful or an error code on failure.
 */
drcovlib_status_t
drcovlib_logfile(void *drcontext, OUT const char **path);

DR_EXPORT
/**
 * Requests that coverage information be dumped to the log file for this process
 * (or for the thread specified by \p drcontext, if #DRCOVLIB_THREAD_PRIVATE is
 * in effect use).  Normally this happens during drcovlib_exit(), unless some
 * unusual termination prevents it.  If this routine is called and
 * drcovlib_exit() is also later called (or for #DRCOVLIB_THREAD_PRIVATE, \p
 * drcovlib's own thread exit events are invoked by DynamoRIO), duplicate
 * information will be dumped to the log files.  Thus, this routine should only
 * be called when the regular dump will not occur.
 *
 * @return whether successful or an error code on failure.
 */
drcovlib_status_t
drcovlib_dump(void *drcontext);


/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRCOVLIB_H_ */
