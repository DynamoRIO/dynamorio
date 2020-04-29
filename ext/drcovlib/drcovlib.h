/* **********************************************************
 * Copyright (c) 2016-2017 Google, Inc.   All rights reserved.
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
    DRCOVLIB_SUCCESS,                 /**< Operation succeeded. */
    DRCOVLIB_ERROR,                   /**< Operation failed. */
    DRCOVLIB_ERROR_INVALID_PARAMETER, /**< Operation failed: invalid parameter */
    DRCOVLIB_ERROR_INVALID_SETUP,     /**< Operation failed: invalid DynamoRIO setup */
    DRCOVLIB_ERROR_FEATURE_NOT_AVAILABLE, /**< Operation failed: not available */
    DRCOVLIB_ERROR_NOT_FOUND,             /**< Operation failed: query not found. */
    DRCOVLIB_ERROR_BUF_TOO_SMALL,         /**< Operation failed: buffer too small. */
} drcovlib_status_t;

/** Bitmask flags for use in #drcovlib_options_t.flags. */
typedef enum {
    /**
     * Requests to dump the log file in text format.  By default the log file is
     * in binary format.  When in text format, the log file is \em not readable
     * by the post-processing tool \ref sec_drcov2lcov.
     */
    DRCOVLIB_DUMP_AS_TEXT = 0x0001,
    /**
     * By default, coverage information is dumped in a single process-wide log
     * file.  If DynamoRIO is run with thread-private code caches (i.e.,
     * dr_using_all_private_caches() returns true) and this flag is enabled,
     * then per-thread coverage information will be stored and dumped in \p
     * drcovlib's own thread exit events rather than in drcovlib_exit().
     */
    DRCOVLIB_THREAD_PRIVATE = 0x0002,
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
     * By default, log file names are prefixed with "drcov".  This option overrides
     * that default.
     */
    const char *logprefix;
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
 * XXX i#1842: this is not a sufficient description: special strings for the version,
 * flavor, module header, bb header, and module table entry are assumed in
 * drcov2lcov.  It seems better to move the specific parsing into drcovlib and
 * make drcovlib usable in standalone mode.
 */

/* file format version */
#define DRCOV_VERSION 2

/* i#1532: drsyms can't mix arch for ELF */
#ifdef LINUX
#    ifdef X64
#        define DRCOV_ARCH_FLAVOR "-64"
#    else
#        define DRCOV_ARCH_FLAVOR "-32"
#    endif
#else
#    define DRCOV_ARCH_FLAVOR ""
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
    uint start; /* offset of bb start from the image base */
    ushort size;
    ushort mod_id;
} bb_entry_t;

/***************************************************************************
 * Coverage interface
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

/***************************************************************************
 * Module tracking
 */

/** Information for one module as recorded during execution. */
typedef struct _drmodtrack_info_t {
    /**
     * Used for compatibility purposes for adding new fields, the user must
     * set this value to the size of the structure.
     */
    size_t struct_size;
    /**
     * The unique index of the module segment for the base address of the
     * containing module.  If the module consists of a single contiguous mapping
     * (the typical case), there is only one index for the whole module.
     * If the module has gaps in its mapping, each non-contiguous segment has its
     * own index, with this field pointing to the index of the segment with
     * the lowest base address.
     */
    uint containing_index;
    /**
     * The start address of this segment of the module as it was during
     * execution.  If the module has multiple non-contiguous segments, each
     * segment will have a distinct entry with its own unique index identifier.
     * The \p containing_index field can be used to identify which segments
     * belong to the same module.  They will also all have the same \p path.
     */
    app_pc start;
    /** The size of this segment of the module. */
    size_t size;
    /**
     * The full path to the file backing the module.  This points to a buffer of
     * size #MAXIMUM_PATH.  It can be modified.
     */
    char *path;
#ifdef WINDOWS
    /** The checksum field as stored in the module headers. */
    uint checksum;
    /** The timestamp field as stored in the module headers. */
    uint timestamp;
#endif
    /** The custom field set by the \p load_cb passed to drmodtrack_add_custom_data(). */
    void *custom;
    /**
     * The unique index of this module segment.  This equals the \p index parameter
     * passed to drmodtrack_offline_lookup().
     */
    uint index;
    /**
     * The offset of this segment from the beginning of this backing file.
     */
    uint64 offset;
} drmodtrack_info_t;

DR_EXPORT
/**
 * Initializes drcovlib's module tracking feature.  Must be called
 * prior to any of the other online routines.  Can be called multiple
 * times (by separate components, normally) but each call must be
 * paired with a corresponding call to drmodtrack_exit().
 *
 * @return whether successful or an error code on failure.
 */
drcovlib_status_t
drmodtrack_init(void);

DR_EXPORT
/**
 * Returns the base address in \p mod_base and the unique index identifier in \p
 * mod_index for the module that contains \p pc.  If there is no such module,
 * returns DRCOVLIB_ERROR_NOT_FOUND.  For modules that containing multiple
 * non-contiguous mapped segments, each segment has its own unique identifier, and
 * this routine returns the appropriate identifier, but \p mod_base contains the
 * lowest address of any segment in the module, not the start address of the
 * segment that contains pc.
 */
drcovlib_status_t
drmodtrack_lookup(void *drcontext, app_pc pc, OUT uint *mod_index, OUT app_pc *mod_base);

DR_EXPORT
/**
 * Writes the complete module information to \p file.  The information can be read
 * back in using \p drmodtrack_offline_read().
 */
drcovlib_status_t
drmodtrack_dump(file_t file);

DR_EXPORT
/**
 * Writes the complete module information to \p buf as a null-terminated string.
 * Returns DRCOVLIB_SUCCESS on success and stores the number of bytes written to
 * \p buf (including the terminating null) in \p wrote if \p wrote is not NULL.
 * If the buffer is too small, returns DRCOVLIB_ERROR_BUF_TOO_SMALL.
 */
drcovlib_status_t
drmodtrack_dump_buf(char *buf, size_t size, OUT size_t *wrote);

DR_EXPORT
/**
 * Cleans up the module tracking state.
 */
drcovlib_status_t
drmodtrack_exit(void);

DR_EXPORT
/**
 * Usable from standalone mode (hence the "offline" name).  Reads a file written
 * by drmodtrack_dump().  If \p file is not INVALID_FILE, reads from \p file;
 * otherwise, assumes the target file has been mapped into memory at \p map and
 * reads from there.  If \p next_line is not NULL, this routine reads one
 * character past the final newline of the final module in the list, and returns
 * in \p *next_line a pointer to that character (this is intended for users who
 * are embedding a module list inside a file with further data following the
 * list in the file).  If \p next_line is NULL, this routine stops reading at
 * the final newline: thus, \p map need not be NULL-terminated.
 * Although \p map uses a char type, it cannot be assumed to be a regular string:
 * binary data containing zeroes may be embedded inside it.
 *
 * Returns an identifier in \p handle to use with other offline routines, along
 * with the number of modules read in \p num_mods.  Information on each module
 * can be obtained by calling drmodtrack_offline_lookup() and passing integers
 * from 0 to the number of modules minus one as the \p index.
 */
drcovlib_status_t
drmodtrack_offline_read(file_t file, const char *map, OUT const char **next_line,
                        OUT void **handle, OUT uint *num_mods);

DR_EXPORT
/**
 * Queries the information that was read earlier by
 * drmodtrack_offline_read() into \p handle, returning it in \p info.
 * The caller must initialize the \p size field of \p info before
 * calling.  The \p info.path field can be modified, with the modified
 * version later written out via drmodtrack_offline_write().  The
 * path's containing buffer size is limited to MAXIMUM_PATH.
 */
drcovlib_status_t
drmodtrack_offline_lookup(void *handle, uint index, OUT drmodtrack_info_t *info);

DR_EXPORT
/**
 * Writes the module information that was read by drmodtrack_offline_read(),
 * and potentially modified by drmodtrack_offline_lookup(), to \p buf, whose
 * maximum size is specified in \p size.
 * Returns DRCOVLIB_SUCCESS on success and stores the number of bytes written to
 * \p buf (including the terminating null) in \p wrote if \p wrote is not NULL.
 * If the buffer is too small, returns DRCOVLIB_ERROR_BUF_TOO_SMALL (and does not
 * set \p wrote).
 */
drcovlib_status_t
drmodtrack_offline_write(void *handle, char *buf, size_t buf_size, OUT size_t *wrote);

DR_EXPORT
/**
 * Cleans up the offline module state for \p handle.
 */
drcovlib_status_t
drmodtrack_offline_exit(void *handle);

DR_EXPORT
/**
 * Adds custom data stored with each module, serialized to a buffer or file, and
 * read back in.  The \p load_cb, \p print_cb, and \p free_cb are used during
 * online operation, while \p parse_cb and \p free_cb are used for offline
 * post-processing.  The \p load_cb is called for each new module, and its return value
 * is the data that is stored online.  That data is printed to a string with
 * \p print_cb, which should return the number of characters printed or -1 on error.
 * The data is freed with \p free_cb.  The printed data is read back in with
 * \p parse_cb, which returns the point in the input string past the custom data,
 * and writes the parsed data to its output parameter, which can subsequently be
 * retrieved from drmodtrack_offline_lookup()'s \p custom output parameter.
 *
 * If a module contains non-contiguous segments, \p load_cb is called
 * only once, and the resulting custom field is shared among all
 * separate entries returned by drmodtrack_offline_lookup().
 *
 * Only one value for each callback is supported.  Calling this routine again
 * with a different value will replace the existing callbacks.
 */
drcovlib_status_t
drmodtrack_add_custom_data(void *(*load_cb)(module_data_t *module),
                           int (*print_cb)(void *data, char *dst, size_t max_len),
                           const char *(*parse_cb)(const char *src, OUT void **data),
                           void (*free_cb)(void *data));

/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRCOVLIB_H_ */
