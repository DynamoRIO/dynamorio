/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* DynamoRIO Intel PT Tracing Extension. */

#ifndef _DRPTTRACER_H_
#define _DRPTTRACER_H_ 1

/**
 * @file drpttracer.h
 * @brief Header for DynamoRIO Intel PT Tracing Extension
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef IN
#    define IN // nothing
#endif
#ifndef OUT
#    define OUT // nothing
#endif
#ifndef INOUT
#    define INOUT // nothing
#endif

typedef struct _drpttracer_data_buf_t {
    void *buf;
    int buf_size;
} drpttracer_data_buf_t;

/***************************************************************************
 * INIT
 */

DR_EXPORT
/**
 * Initializes the drpttracer extension.  Must be called prior to any of the
 * other routines.  Can be called multiple times (by separate components,
 * normally) but each call must be paired with a corresponding call to
 * drpttracer_exit().
 *
 * \return whether successful.
 */
bool
drpttracer_init(void);

DR_EXPORT
/**
 * Cleans up the drpttracer extension.
 */
void
drpttracer_exit(void);

/***************************************************************************
 * INTEL PT TRACING
 */

/** Success code for each drpttracer operation. */
typedef enum {
    DRPTTRACER_SUCCESS, /**< Operation succeeded. */
    DRPTTRACER_ERROR,   /**< Operation failed. */
    DRPTTRACER_ERROR_INVALID_ARGUMENT, /**< Operation failed: invalid argument. */
    DRPTTRACER_ERROR_FAILED_TO_OPEN_PERF_EVENT, /**< Operation failed: failed to open perf event. */
    DRPTTRACER_ERROR_FAILED_TO_MMAP_PERF_EVENT, /**< Operation failed: failed to mmap perf event. */
    DRPTTRACER_ERROR_FAILED_TO_MMAP_AUX, /**< Operation failed: failed to mmap aux. */
    DRPTTRACER_ERROR_FAILED_TO_FREE_NULL_BUFFER, /**< Operation failed: failed to free a null buffer. */
    DRPTTRACER_ERROR_FAILED_TO_CREATE_PIPE, /**< Operation failed: failed to create pipe. */
    DRPTTRACER_ERROR_FAILED_TO_DUMP_KCORE_AND_KALLSYMS /**< Operation failed: failed to dump kcore and kallsyms. */
} drpttracer_status_t;

DR_EXPORT
drpttracer_status_t
drpttracer_start_trace(IN bool user, IN bool kernel, OUT void *tracer_handle);

DR_EXPORT
drpttracer_status_t
drpttracer_end_trace(IN void *tracer_handle, OUT drpttracer_data_buf_t *pt_data,
                     OUT drpttracer_data_buf_t *sideband_data);

DR_EXPORT
drpttracer_status_t
drpttracer_create_data_buffer(OUT drpttracer_data_buf_t *data_buf);

DR_EXPORT
drpttracer_status_t
drpttracer_delete_data_buffer(IN drpttracer_data_buf_t *data_buf);

DR_EXPORT
drpttracer_status_t
drpttracer_dump_kcore_and_kallsyms(IN const char *dir);

#ifdef __cplusplus
}
#endif

#endif /* _DRPTTRACER_H_ */