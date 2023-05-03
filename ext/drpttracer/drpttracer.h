/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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

/* DynamoRIO Intel PT Tracing Extension.
 * TODO i#5505: Currently, this extension incurs significant overhead. We will reduce the
 * overhead in the next PR.
 */

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

#define END_PACKED_STRUCTURE __attribute__((__packed__))

/**
 * The type of PT trace's metadata.
 *
 * \note drpttracer uses the cpuid instruction to get the cpu_family, cpu_model and
 * cpu_stepping. The cpu_family, cpu_model and cpu_stepping are used to initialize the PT
 * config of pt2ir_t when decoding a PT trace.
 *
 * \note drpttracer gets the time_shift, time_mult and time_zero from the opened perf
 * event file's head. The time_shift, time_mult and time_zero are used to initialize the
 * PT sideband config of pt2ir_t when decoding a PT trace.
 */
typedef struct _pt_metadata_t {
    uint16_t cpu_family;  /**< The CPU family. */
    uint8_t cpu_model;    /**< The CPU mode. */
    uint8_t cpu_stepping; /**< The CPU stepping. */

    /**
     * The time shift. pt2ir_t uses it to synchronize the time of the PT trace and
     * sideband data.
     * \note time_shift = perf_event_mmap_page.time_shift
     */
    uint16_t time_shift;

    /**
     * The time multiplier. pt2ir_t uses it to synchronize the time of the PT trace and
     * sideband data.
     * \note time_mult = perf_event_mmap_page.time_mult
     */
    uint32_t time_mult;

    /**
     * The time zero. pt2ir_t uses it to synchronize the time of the PT trace and
     * sideband data. \note time_zero = perf_event_mmap_page.time_zero
     */
    uint64_t time_zero;
} END_PACKED_STRUCTURE pt_metadata_t;

/**
 * The storage container type of drpttracer's output.
 * This data struct is used by drpttracer to store PT trace, and sideband data. These data
 * can be dumped into different files by the client. These files can be the inputs of
 * pt2ir_t, which decodes the PT data into Dynamorio's IR.
 * \note This buffer can be shared across different tracing sessions. Thus, we allocate a
 * buffer with the maximum trace size. The pt_size and sideband_size variables indicate
 * the valid data size within the buffer.
 */
typedef struct _drpttracer_output_t {
    void *pt_buffer;             /**< The buffer of PT trace. */
    size_t pt_buffer_size;       /**< The size of PT trace's buffer. */
    size_t pt_size;              /**< The size of vaild PT trace stored in PT buffer. */
    void *sideband_buffer;       /**< The PT sideband data's buffer. */
    size_t sideband_buffer_size; /**< The size of PT sideband data's buffer. */
    size_t sideband_size; /**< The size of vaild PT sideband data stored in PT buffer. */
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
    /** Operation failed: invalid parameter. */
    DRPTTRACER_ERROR_INVALID_PARAMETER,
    /** Operation failed: failed to open perf event. */
    DRPTTRACER_ERROR_FAILED_TO_OPEN_PERF_EVENT,
    /** Operation failed: failed to mmap perf data. */
    DRPTTRACER_ERROR_FAILED_TO_MMAP_PERF_DATA,
    /** Operation failed: failed to mmap PT data. */
    DRPTTRACER_ERROR_FAILED_TO_MMAP_PT_DATA,
    /** Operation failed: failed to allocate output buffer. */
    DRPTTRACER_ERROR_FAILED_TO_ALLOCATE_OUTPUT_BUFFER,
    /** Operation failed: failed to start tracing. */
    DRPTTRACER_ERROR_FAILED_TO_START_TRACING,
    /** Operation failed: failed to stop tracing. */
    DRPTTRACER_ERROR_FAILED_TO_STOP_TRACING,
    /** Operation failed: overwritten PT trace. */
    DRPTTRACER_ERROR_OVERWRITTEN_PT_TRACE,
    /** Operation failed: failed to read PT data from PT ring buffer. */
    DRPTTRACER_ERROR_FAILED_TO_READ_PT_DATA,
    /** Operation failed: overwritten sideband data. */
    DRPTTRACER_ERROR_OVERWRITTEN_SIDEBAND_DATA,
    /** Operation failed: failed to read SIDEBAND data from perf data ring buffer. */
    DRPTTRACER_ERROR_FAILED_TO_READ_SIDEBANBD_DATA,
} drpttracer_status_t;

/**
 * The tracing modes for drpttracer_start_tracing().
 *
 * XXX: The #DRPTTRACER_TRACING_ONLY_USER and #DRPTTRACER_TRACING_USER_AND_KERNEL modes
 * are not completely supported yet. The sideband data collected in these two modes do not
 * include the initial mmap2 event recording. Therefore, if the user uses pt2ir_t in
 * sideband converter mode, pt2ir_t cannot find the image and cannot decode the PT trace.
 */
typedef enum {
    /** Trace only userspace instructions. */
    DRPTTRACER_TRACING_ONLY_USER,
    /** Trace only kernel instructions. */
    DRPTTRACER_TRACING_ONLY_KERNEL,
    /** Trace both userspace and kernel instructions. */
    DRPTTRACER_TRACING_USER_AND_KERNEL
} drpttracer_tracing_mode_t;

DR_EXPORT
/**
 * Create a pttracer handle. Must be called after drpttracer_init() and before
 * drpttracer_exit(). The handle is used to start and stop PT tracing.
 *
 * \param[in] drcontext  The context of DynamoRIO.
 * \param[in] tracing_mode  The tracing mode.
 * \param[in] pt_size_shift  The size shift of PT trace's ring buffer.
 * \param[in] sideband_size_shift  The size shift of sideband data's ring buffer.
 * \param[out] tracer_handle  The pttracer handle.
 *
 * \note The size offset is used to control the size of the buffer allocated by the perf:
 *       sizeof(PT trace's buffer) = 2 ^ pt_size_shift * PAGE_SIZE.
 *       sizeof(Sideband data's buffer) = 2 ^ sideband_size_shift * PAGE_SIZE.
 * Additionally, perf sets the buffer size to 4MiB by default. Therefore, when the client
 * uses drpttracer to trace, it is best to set the trace and sideband buffer larger than
 * 4Mib.
 *
 * \note The client must ensure that the buffer is large enough to hold the PT data.
 * Insufficient buffer size will lead to lost data, which may cause issues in pt2ir_t
 * decoding. If we detect an overflow, drpttracer_stop_tracing() will return an error code
 * #DRPTTRACER_ERROR_OVERWRITTEN_PT_TRACE or
 * #DRPTTRACER_ERROR_OVERWRITTEN_SIDEBAND_DATA.
 *
 * \note Each thread corresponds to a trace handle. When calling
 * drpttracer_create_handle(), the client will get a tracer_handle. The client can start
 * and stop tracing by passing it to drpttracer_start_tracing() and
 * drpttracer_stop_tracing(). And if the thread hold a tracer_handle, the client needs to
 * call drpttracer_destroy_handle() to destroy the corresponding tracer_handle and release
 * the resources before the thread end.
 *
 * \note For one thread, only one tracing can execute at the same time. So the client
 * needs to ensure one thread only owns one pttracer handle.
 *
 * \return the status code.
 */
drpttracer_status_t
drpttracer_create_handle(IN void *drcontext, IN drpttracer_tracing_mode_t tracing_mode,
                         IN uint pt_size_shift, IN uint sideband_size_shift,
                         OUT void **tracer_handle);

DR_EXPORT
/**
 * Destroy a pttracer handle and release the resources. Must be called after
 * drpttracer_init() and before drpttracer_exit().
 *
 * \param[in] drcontext  The context of DynamoRIO.
 * \param[in] tracer_handle  The pttracer handle.
 *
 * \return the status code.
 */
drpttracer_status_t
drpttracer_destroy_handle(IN void *drcontext, INOUT void *tracer_handle);

DR_EXPORT
/**
 * Start a PT tracing session. Must pass a valid pttracer handle to this function.
 *
 * \param[in] drcontext  The context of DynamoRIO.
 * \param[in] tracer_handle  The pttracer handle.
 *
 * \return the status code.
 */
drpttracer_status_t
drpttracer_start_tracing(IN void *drcontext, IN void *tracer_handle);

DR_EXPORT
/**
 * Stops a PT tracing session. Must pass a valid pttracer handle to this function.
 *
 * \param[in] drcontext  The context of DynamoRIO.
 * \param[in] tracer_handle  The pttracer handle.
 * \param[out] output  The output of PT tracing. This function will allocate
 * and fill this output. It will copy the PT trace to the pt_buf of the output. When the
 * tracing mode is #DRPTTRACER_TRACING_ONLY_USER or #DRPTTRACER_TRACING_USER_AND_KERNEL,
 * the PT sideband data will be copied to the sideband_buf of the output. When the tracing
 * mode is #DRPTTRACER_TRACING_ONLY_KERNEL, it will not copy the sideband data to the
 * buffer.
 *
 * \note If the buffer size that was set in drpttracer_start_tracing() is not enough,
 * this function will return an error status code:
 *  - Return #DRPTTRACER_ERROR_OVERWRITTEN_PT_TRACE if the PT trace is overwritten.
 *  - Return #DRPTTRACER_ERROR_OVERWRITTEN_SIDEBAND_DATA if the sideband data is
 * overwritten.
 *
 * \note The client can dump the output data to files. After online tracing is done,
 * pt2ir_t can use these files to decode the online PT trace.
 *
 * \note The client doesn't need to allocate the output object, but it needs to free the
 * output object using drpttracer_destroy_output().
 *
 * \return the status code.
 */
drpttracer_status_t
drpttracer_stop_tracing(IN void *drcontext, IN void *tracer_handle,
                        OUT drpttracer_output_t *output);

DR_EXPORT
/**
 * Get the PT metadata of the given pttracer handle.
 *
 * \param[in] tracer_handle The pttracer handle.
 * \param[out] pt_metadata  The PT metadata of the current pttracer handle.
 *
 * \return the status code.
 */
drpttracer_status_t
drpttracer_get_pt_metadata(IN void *tracer_handle, OUT pt_metadata_t *pt_metadata);

DR_EXPORT
/**
 * Creates an output object of drpttracer.
 *
 * \param[in] drcontext  The context of DynamoRIO.
 * \param[in] pt_buf_size_shift  The size shift of the PT trace's output buffer.
 * \param[in] sd_buf_size_shift  The size shift of the sideband data's output buffer.
 * \param[out] output  The output object that will be created.
 *
 * \return the status code.
 */
drpttracer_status_t
drpttracer_create_output(IN void *drcontext, IN uint pt_buf_size_shift,
                         IN size_t sd_buf_size_shift, OUT drpttracer_output_t **output);

DR_EXPORT
/**
 * Destroys an output object of drpttracer.
 *
 * \param[in] drcontext  The context of DynamoRIO.
 * \param[in] output  The output object that will be destroyed.
 *
 * \return the status code.
 */
drpttracer_status_t
drpttracer_destroy_output(IN void *drcontext, IN drpttracer_output_t *output);

#ifdef __cplusplus
}
#endif

#endif /* _DRPTTRACER_H_ */
