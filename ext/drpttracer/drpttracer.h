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

#include <inttypes.h>

#ifndef IN
#    define IN // nothing
#endif
#ifndef OUT
#    define OUT // nothing
#endif
#ifndef INOUT
#    define INOUT // nothing
#endif

/**
 * The storage container type of a drpttracer's output buffer.
 * The drpttracer_buf_t is used for the drpttracer to store the PT trace or Sideband data.
 */
typedef struct _drpttracer_buf_t {
    void *buf;    /**< The buffer pointer */
    int buf_size; /**< The buffer size */
} drpttracer_buf_t;

/**
 * The type of PT trace's metadata. This type is used to initialize the PT config and PT
 * sideband config.
 */
typedef struct _pt_meta_data_t {
    /** The CPU family. */
    uint16_t cpu_family;
    /** The CPU mode. */
    uint8_t cpu_model;
    /** The CPU stepping. */
    uint8_t cpu_stepping;
    /** perf_event_mmap_page.time_shift */
    uint16_t time_shift;
    /** perf_event_mmap_page.time_mult */
    uint32_t time_mult;
    /** perf_event_mmap_page.time_zero */
    uint64_t time_zero;
} pt_metadata_t;

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
 * PT TRACING APIS
 */

/**
 * Success code for each drpttracer operation.
 */
typedef enum {
    /** Operation succeeded. */
    DRPTTRACER_SUCCESS,
    /** Operation failed. */
    DRPTTRACER_ERROR,
    /** Operation failed: invalid tracing handle. */
    DRPTTRACER_ERROR_INVALID_TRACING_HANDLE,
    /** Operation failed: invalid tracing mode. */
    DRPTTRACER_ERROR_INVALID_TRACING_MODE,
    /** Operation failed: failed to open perf event. */
    DRPTTRACER_ERROR_FAILED_TO_OPEN_PERF_EVENT,
    /** Operation failed: failed to create tracer handle. */
    DRPTTRACER_ERROR_FAILED_TO_CREATE_PTTRACER_HANDLE,
    /** Operation failed: failed to start tracing. */
    DRPTTRACER_ERROR_FAILED_TO_START_TRACING,
    /** Operation failed: failed to stop tracing. */
    DRPTTRACER_ERROR_FAILED_TO_STOP_TRACING
} drpttracer_status_t;

/**
 * The tracing modes for drpttracer_start_tracing().
 */
typedef enum {
    /* drpttracer only traces in user space. */
    DRPTTRACER_TRACING_ONLY_USER,
    /* drpttracer only traces in kernel. */
    DRPTTRACER_TRACING_ONLY_KERNEL,
    /* drpttracer traces both user and kernel. */
    DRPTTRACER_TRACING_USER_AND_KERNEL
} drpttracer_tracing_mode_t;

DR_EXPORT
/**
 * Starts PT tracing. Must be called after drpttracer_init() and before drpttracer_exit().
 *
 * \param[in] drcontext  The context of DynamoRIO.
 * \param[in] type  The tracing type.
 * \param[out] tracer_handle  The tracer handle.
 *
 * \note Each tracing corresponds to a trace_handle. When calling
 * drpttracer_start_tracing(), the caller will get a tracer_handle. And the caller can
 * stop tracing by passing it to drpttracer_stop_tracing().
 *
 * \return the status code.
 */
drpttracer_status_t
drpttracer_start_tracing(IN void *drcontext, IN drpttracer_tracing_mode_t mode,
                         OUT void **tracer_handle);

DR_EXPORT
/**
 * Stops PT tracing.
 *
 * \param[in] drcontext  The context of DynamoRIO.
 * \param[in] tracer_handle  The tracer handle.
 * \param[out] meta_data  The metadata of the PT trace. If NULL, the metadata will not be
 * return.
 * \param[out] pt_data  The buffer of the PT trace. If NULL, the PT trace will not be
 * return.
 * \param[out] sideband_data  The buffer of the PT sideband data. If NULL, the PT sideband
 * data will not be return.
 *
 * \return the status code.
 */
drpttracer_status_t
drpttracer_end_tracing(IN void *drcontext, IN void *tracer_handle,
                       OUT pt_metadata_t *meta_data, OUT drpttracer_buf_t *pt_data,
                       OUT drpttracer_buf_t *sideband_data);

DR_EXPORT
/**
 * Creates a new drpttracer_buf_t.
 *
 * \param[in] drcontext  The context of DynamoRIO.
 *
 * \return the created buffer.
 */
drpttracer_buf_t *
drpttracer_create_buffer(IN void *drcontext);

DR_EXPORT
/**
 * Destroys a drpttracer_buf_t.
 *
 * \param[in] drcontext  The context of DynamoRIO.
 * \param[in] buf  The buffer to be destroyed.
 */
void
drpttracer_delete_buffer(IN void *drcontext, IN drpttracer_buf_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* _DRPTTRACER_H_ */
