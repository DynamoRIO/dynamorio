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

/* syscall_pt_trace.h: header of module for recording kernel PT traces for every syscall.
 */

#ifndef _SYSCALL_PT_TRACE_
#define _SYSCALL_PT_TRACE_ 1

#include <cstddef>
#include <string>

#include "dr_api.h"
#include "drmemtrace.h"
#include "drpttracer.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

/* The auto cleanup wrapper of pttracer handle.
 * This can ensure the pttracer handle is cleaned up when it is out of scope.
 */
struct drpttracer_handle_autoclean_t {
public:
    drpttracer_handle_autoclean_t(void *drcontext, void *handle)
        : drcontext { drcontext }
        , handle { handle }
    {
    }
    ~drpttracer_handle_autoclean_t()
    {
        reset();
    }
    void
    reset()
    {
        ASSERT(drcontext != nullptr, "invalid drcontext");
        if (handle != nullptr) {
            drpttracer_destroy_handle(drcontext, handle);
            handle = nullptr;
        }
    }
    void *drcontext = nullptr;
    void *handle = nullptr;
};

/* The auto cleanup wrapper of drpttracer_output_t.
 * This can ensure the pttracer handle is cleaned up when it is out of scope.
 */
struct drpttracer_output_autoclean_t {
public:
    drpttracer_output_autoclean_t(void *drcontext, drpttracer_output_t *data)
        : drcontext { drcontext }
        , data { data }
    {
    }
    ~drpttracer_output_autoclean_t()
    {
        ASSERT(drcontext != nullptr, "invalid drcontext");
        if (data != nullptr) {
            void *drcontext = dr_get_current_drcontext();
            drpttracer_destroy_output(drcontext, data);
            data = nullptr;
        }
    }
    void *drcontext = nullptr;
    drpttracer_output_t *data = nullptr;
};

#define INVALID_SYSNUM -1

/* This class is not thread-safe: the caller should create a separate instance per thread.
 */
class syscall_pt_trace_t {
public:
    syscall_pt_trace_t();

    ~syscall_pt_trace_t();

    /* Initialize the syscall_pt_trace_t instance for current thread.
     * The instance will dump the kernel PT trace for every syscall. So the caller must
     * pass in the output directory and the file write function.
     */
    bool
    init(void *drcontext, char *pt_dir_name, drmemtrace_open_file_func_t open_file_func,
         drmemtrace_write_file_func_t write_file_func,
         drmemtrace_close_file_func_t close_file_func);

    /* Start the PT tracing for current syscall and store the sysnum of the syscall. */
    bool
    start_syscall_pt_trace(IN int sysnum);

    /* Stop the PT tracing for current syscall and dump the output data to one file. */
    bool
    stop_syscall_pt_trace();

    /* Get the sysnum of current recording syscall. */
    int
    get_cur_recording_sysnum()
    {
        return cur_recording_sysnum_;
    }

    /* Get the index of the traced syscall.
     */
    int
    get_traced_syscall_idx()
    {
        return traced_syscall_idx_;
    }

    /* Check whether the syscall's PT need to be recorded.
     * It can be used to filter out the syscalls that are not interesting or not
     * supported.
     */
    static bool
    is_syscall_pt_trace_enabled(IN int sysnum);

private:
    /* Dump the metadata to a per-thread file. */
    bool
    metadata_dump(pt_metadata_t metadata);

    /* Dump PT trace data to a per-thread file. */
    bool
    trace_data_dump(drpttracer_output_autoclean_t &output);

    /* The shared file open function. */
    drmemtrace_open_file_func_t open_file_func_;

    /* The shared file write function. */
    drmemtrace_write_file_func_t write_file_func_;

    /* The shared file close function. */
    drmemtrace_close_file_func_t close_file_func_;

    /* Indicates whether the syscall_pt_trace instance has been initialized. The init
     * function should be called only once per thread.
     */
    bool is_initialized_;

    /* The pttracer handle held by this instance. */
    drpttracer_handle_autoclean_t pttracer_handle_;

    /* The pttracer output data held by every instance. The output buffer stores PT trace
     * data for each system call. The buffer will be updated when stop_syscall_pt_trace()
     * is invoked.
     */
    drpttracer_output_autoclean_t pttracer_output_buffer_;

    /* The index of the traced syscall. */
    int traced_syscall_idx_;

    /* The sysnum of current recording syscall. */
    int cur_recording_sysnum_;

    /* Flag to indicate if metadata is being dumped. */
    bool is_dumping_metadata_;

    /* The drcontext.
     * We need ensure pass the same context to all drpttracer's APIs.
     */
    void *drcontext_;

    /* The output file.
     * It is a per-thread file. It stores the PT trace data and metadata for
     * every syscall in the current thread.
     */
    file_t output_file_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _SYSCALL_PT_TRACE_ */
