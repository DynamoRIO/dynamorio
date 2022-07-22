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
 * The type of PT trace's metadata. This type is used to initialize the PT config and PT
 * sideband config of drpt2ir when decoding a PT trace.
 */
typedef struct __attribute__((__packed__)) _pt_metadata_t {
    /** The CPU family. */
    uint16_t cpu_family;
    /** The CPU mode. */
    uint8_t cpu_model;
    /** The CPU stepping. */
    uint8_t cpu_stepping;
    /**
     * The time shift. It is used to synchronize trace time, and the sideband recodes
     * time.
     * \note time_shift = perf_event_mmap_page.time_shift
     */
    uint16_t time_shift;
    /**
     * The time multiplier. It is used to synchronize trace time, and the sideband
     * recodes time.
     * \note time_mult = perf_event_mmap_page.time_mult
     */
    uint32_t time_mult;

    /**
     * The time zero. It is used to synchronize trace time, and the sideband recodes
     * time.
     * \note time_zero = perf_event_mmap_page.time_zero
     */
    uint64_t time_zero;
} pt_metadata_t;

/**
 * The storage container type of a drpttracer's output.
 * This data struct is used by drpttracer to store PT metadata, PT trace, and
 * sideband data. These data can be dumped into different files. These files can be the
 * inputs of drpt2ir, which decodes the PT data into Dynamorio's IR.
 */
typedef struct _drpttracer_buf_t {
    pt_metadata_t metadata;   /**< The PT trace's metadata. */
    void *pt_buf;             /**< The PT trace's buffer pointer. */
    size_t pt_buf_size;       /**< The buffer size of PT trace. */
    void *sideband_buf;       /**< The PT sideband data's buffer pointer. */
    size_t sideband_buf_size; /**< The buffer size of PT sideband data. */
} drpttracer_output_t;

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
    /** Operation failed: invalid output. */
    DRPTTRACER_ERROR_INVALID_OUTPUT,
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
    /** If drpttracer is set to work in this mode, it will trace in userspace. */
    DRPTTRACER_TRACING_ONLY_USER,
    /** If drpttracer is set to work in this mode, it will trace in kernel. */
    DRPTTRACER_TRACING_ONLY_KERNEL,
    /**
     * If drpttracer is set to work in this mode, it will trace in both userspace and
     * kernel.
     */
    DRPTTRACER_TRACING_USER_AND_KERNEL
} drpttracer_tracing_mode_t;

DR_EXPORT
/**
 * Starts PT tracing. Must be called after drpttracer_init() and before drpttracer_exit().
 *
 * \param[in] drcontext  The context of DynamoRIO.
 * \param[in] mode  The tracing mode.
 * \param[in] pt_size_shift  The size shift of PT trace's buffer.
 * \param[in] sideband_size_shift  The size shift of sideband data's buffer.
 * \param[out] tracer_handle  The tracer handle.
 *
 * \note The size shift is used to control the size of buffers:
 *       sizeof(PT trace's buffer) = 2 ^ pt_size_shift * PAGE_SIZE.
 *       sizeof(Sideband data's buffer) = 2 ^ sideband_size_shift * PAGE_SIZE.
 *
 * \note The PT trace's buffer and sideband data's buffer are all ring buffers. So Intel
 * PT will overwrite the oldest data when the buffer is full.
 *
 * \note Each tracing corresponds to a trace_handle. When calling
 * drpttracer_start_tracing(), the caller will get a tracer_handle. And the caller can
 * stop tracing by passing it to drpttracer_end_tracing().
 *
 * \return the status code.
 */
drpttracer_status_t
drpttracer_start_tracing(IN void *drcontext, IN drpttracer_tracing_mode_t mode,
                         IN uint pt_size_shift, IN uint sideband_size_shift,
                         OUT void **tracer_handle);

DR_EXPORT
/**
 * Stops PT tracing.
 *
 * \param[in] drcontext  The context of DynamoRIO.
 * \param[in] tracer_handle  The tracer handle.
 * \param[out] output  The output of PT tracing. This function will allocate
 * and fill this output. It will copy the PT trace to the pt_buff of the output. When the
 * tracing mode is #DRPTTRACER_TRACING_ONLY_USER or #DRPTTRACER_TRACING_USER_AND_KERNEL,
 * the PT sideband data will be copy to the sideband_buf of the output. When the tracing
 * mode is #DRPTTRACER_TRACING_ONLY_KERNEL, it will not copy the sideband data to the
 * buffer.
 *
 * \note The output data can be dumped into different files. After offline tracings are
 * done, drpt2trace can use these files to decode the offline tracings.
 *
 * \note The caller doesn't need to allocate the output object, but it needs to free the
 * output object.
 *
 * \return the status code.
 */
drpttracer_status_t
drpttracer_end_tracing(IN void *drcontext, IN void *tracer_handle,
                       OUT drpttracer_output_t **output);

DR_EXPORT
/**
 * Destroys a output object of drpttrace.
 *
 * \param[in] drcontext  The context of DynamoRIO.
 * \param[in] output  The output object that will be destroyed.
 */
void
drpttracer_destory_output(IN void *drcontext, IN drpttracer_output_t *output);

#ifdef __cplusplus
}
#endif

#endif /* _DRPTTRACER_H_ */
