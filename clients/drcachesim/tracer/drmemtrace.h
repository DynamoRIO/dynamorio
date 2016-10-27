/* **********************************************************
 * (c) 2016 Google, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *
 * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 *
 * Redistributions in binary form must reproduce the above copyright notice,
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* drmemtrace: APIs for applications that are statically linked with drmemtrace
 * interacting with drmemtrace.
 */

#ifndef _DRMEMTRACE_H_
#define _DRMEMTRACE_H_ 1

/**
 * @file drmemtrace.h
 * @brief Header for DynamoRIO Tracer Library
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DRMEMTRACE_SUCCESS,                  /**< Operation succeeded. */
    DRMEMTRACE_ERROR,                    /**< Operation failed. */
    DRMEMTRACE_ERROR_INVALID_PARAMETER,  /**< Operation failed: invalid parameter */
} drmemtrace_status_t;

/**
 * Function for file open.
 * The file access mode is set by the \p mode_flags argument which is drawn from
 * the DR_FILE_* defines ORed together.  Returns INVALID_FILE if unsuccessful.
 * The example behavior is described in dr_open_file();
 *
 * @param[in] fname       The filename to open.
 * @param[in] mode_flags  The DR_FILE_* flags for file open.
 *
 * \return the opened file id.
 */
typedef file_t (*drmemtrace_open_file_func_t)(const char *fname, uint mode_flag);

/**
 * Function for file read.
 * Reads up to \p count bytes from file \p file into \p buf.
 * Returns the actual number read.
 * The example behavior is described in dr_read_file();
 *
 * @param[in] file   The file to be read from.
 * @param[in] buf    The buffer to be read to.
 * @param[in] count  The number of bytes to be read.
 *
 * \return the actual number of bytes read.
 */
typedef ssize_t (*drmemtrace_read_file_func_t)(file_t file,
                                               void *buf,
                                               size_t count);

/**
 * Function for file write.
 * Writes \p count bytes from \p data to file \p file.
 * Returns the actual number written.
 * The example behavior is described in dr_write_file();
 *
 * @param[in] file   The file to be written.
 * @param[in] data   The data to be written.
 * @param[in] count  The number of bytes to be written.
 *
 * \return the actual number of bytes writte.
 */
typedef ssize_t (*drmemtrace_write_file_func_t)(file_t file,
                                                const void *data,
                                                size_t count);

/**
 * Function for file close.
 * The example behavior is described in dr_close_file();
 *
 * @param[in] file  The file to be closed.
 */
typedef void  (*drmemtrace_close_file_func_t)(file_t file);

/**
 * Function for directory creation.
 * The example behavior is described in dr_create_dir();
 *
 * @param[in] dir  The directory name for creation.
 *
 * \return whether successful.
 */
typedef bool (*drmemtrace_create_dir_func_t)(const char *dir);


DR_EXPORT
/**
 * Registers functions to replace the default file operations for offline tracing.
 *
 * \note The caller is responsible for the transparency and isolation of using
 * those functions, which will be called in the middle of arbitrary
 * application code.
 */
drmemtrace_status_t
drmemtrace_replace_file_ops(drmemtrace_open_file_func_t  open_file_func,
                            drmemtrace_read_file_func_t  read_file_func,
                            drmemtrace_write_file_func_t write_file_func,
                            drmemtrace_close_file_func_t close_file_func,
                            drmemtrace_create_dir_func_t create_dir_func);

#ifdef __cplusplus
}
#endif
#endif  /* _DRMEMTRACE_H */
