/* **********************************************************
 * Copyright (c) 2016-2018 Google, Inc.  All rights reserved.
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
 * @brief Header for customizing the DrMemtrace tracer.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Status return values from drmemtrace functions. */
typedef enum {
    DRMEMTRACE_SUCCESS,                  /**< Operation succeeded. */
    DRMEMTRACE_ERROR,                    /**< Operation failed. */
    DRMEMTRACE_ERROR_INVALID_PARAMETER,  /**< Operation failed: invalid parameter. */
    DRMEMTRACE_ERROR_NOT_IMPLEMENTED,    /**< Operation failed: not implemented. */
} drmemtrace_status_t;


/**
 * Name of drmgr instrumentation pass priorities for app2app, analysis, insert,
 * and instru2instru.
 */
#define DRMGR_PRIORITY_NAME_MEMTRACE "memtrace"

DR_EXPORT
/**
 * To support statically linking multiple clients on UNIX, dr_client_main() inside
 * drmemtrace is a weak symbol which just calls the real initializer
 * drmemtrace_client_main().  An enclosing application can override dr_client_main()
 * and invoke drmemtrace_client_main() explicitly at a time of its choosing.
 */
void
drmemtrace_client_main(client_id_t id, int argc, const char *argv[]);

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
 * \return the actual number of bytes written.
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

/**
 * Function for buffer handoff.  Rather than writing a buffer to a
 * file when it is full, instead this handoff function gives ownership
 * to the callee.  The tracer allocates a new buffer and uses it for
 * further tracing.  The callee is responsible for writing out the
 * buffer and for freeing it by calling dr_raw_mem_free().
 *
 * @param[in] file  The file identifier returned by \p open_file_func or
 *  or drmemtrace_replace_file_ops() was not called then from dr_open_file()
 *  for the per-thread trace file.
 * @param[in] data        The start address of the buffer.
 * @param[in] data_size   The size of valid trace data in the buffer.
 * @param[in] alloc_size  The allocated size of the buffer.
 *
 * \return whether successful.  Failure is considered unrecoverable.
 */
typedef bool (*drmemtrace_handoff_func_t)(file_t file, void *data,
                                          size_t data_size, size_t alloc_size);

/**
 * Function for process exit.  This is called during the tracer shutdown, giving
 * a control point where DR memory may be accessed, which is not possible when
 * acting after dr_app_stop_and_cleanup().
 *
 * @param[in] arg  The \p exit_func_arg passed to drmemtrace_buffer_handoff().
 */
typedef void (*drmemtrace_exit_func_t)(void *arg);

DR_EXPORT
/**
 * Registers a function to replace the default file write operation
 * for offline tracing and requests that buffer ownership be
 * transfered.  The regular file open and close routines (or their
 * replacements from drmemtrace_replace_file_ops()) will be called,
 * but instead of writing to the files (or calling the \p
 * write_file_func), the provided \p handoff_func will be called
 * instead.  The callee is responsible for writing out the buffer and
 * for freeing it by calling dr_raw_mem_free().  The amount of
 * legitimate data is in \p data_size and the total allocated size of
 * the buffer is in \p alloc_size.  Any space in between is available
 * for use by the callee.  The return value of \p handoff_cb indicates
 * whether successful or not: failure will be treated as fatal and
 * unrecoverable.
 *
 * The module list data, written to the first file opened, is not
 * subject to this ownership transfer and uses the \p write_file_func.
 *
 * Because DR memory will be freed in dr_app_stop_and_cleanup(), an
 * exit callback is provided for a control point to process and free
 * the buffers.  When dr_app_stop_and_cleanup() is used, \p exit_func
 * will be called (and passed \p exit_func_arg) after the other
 * application threads are already native.
 *
 * \note The caller is responsible for the transparency and isolation of using
 * these functions, which will be called in the middle of arbitrary
 * application code.
 */
drmemtrace_status_t
drmemtrace_buffer_handoff(drmemtrace_handoff_func_t handoff_func,
                          drmemtrace_exit_func_t exit_func,
                          void *exit_func_arg);

/**
 * The name of the file in -offline mode where module data is written.
 * Its creation can be customized using drmemtrace_custom_module_data()
 * and then modified before passing to raw2trace via
 * drmodtrack_add_custom_data() and drmodtrack_offline_write().
 */
#define DRMEMTRACE_MODULE_LIST_FILENAME "modules.log"

DR_EXPORT
/**
 * Retrieves the full path to the output directory in -offline mode
 * where data is being written.
 */
drmemtrace_status_t
drmemtrace_get_output_path(OUT const char **path);

DR_EXPORT
/**
 * Retrieves the full path to the file in -offline mode where module data is written.
 * Its creation can be customized using drmemtrace_custom_module_data() with
 * corresponding post-processing with raw2trace_t::handle_custom_data().
 */
drmemtrace_status_t
drmemtrace_get_modlist_path(OUT const char **path);

DR_EXPORT
/**
 * Adds custom data stored with each module in the module list produced for
 * offline trace post-processing.  The \p load_cb is called for each new module,
 * and its return value is the data that is stored.  That data is later printed
 * to a string with \p print_cb, which should return the number of characters
 * printed or -1 on error.  The data is freed with \p free_cb.
 *
 * On the post-processing side, the user should create a custom post-processor
 * by linking with raw2trace and calling raw2trace_t::handle_custom_data() to provide
 * parsing and processing routines for the custom data.
 */
drmemtrace_status_t
drmemtrace_custom_module_data(void * (*load_cb)(module_data_t *module),
                              int (*print_cb)(void *data, char *dst, size_t max_len),
                              void (*free_cb)(void *data));

/**
 * Activates thread filtering.  The \p should_trace_thread_cb will be
 * called once for each new thread, with \p user_value passed in for \p
 * user_data.  If it returns false, that thread will *not* be traced at
 * all; if it returns true, that thread will be traced normally.  Returns
 * whether the filter was successfully installed.  \note This feature is
 * currently only supported for x86.
 * This routine should be called during initialization, before any
 * instrumentation is added.  To filter out the calling thread (the initial
 * application thread) this should be called prior to DR initialization
 * (via the start/stop API).  Only a single call to this routine is
 * supported.
 */
drmemtrace_status_t
drmemtrace_filter_threads(bool (*should_trace_thread_cb)(thread_id_t tid,
                                                         void *user_data),
                          void *user_value);

#ifdef __cplusplus
}
#endif
#endif  /* _DRMEMTRACE_H */
