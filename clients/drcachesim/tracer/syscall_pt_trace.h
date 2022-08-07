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

/* syscall_pt_trace.h: header of module for recording kernel PT traces for every syscall.
 */

#ifndef _SYSCALL_PT_TRACE_
#define _SYSCALL_PT_TRACE_ 1

#include <cstddef>

#include "../common/utils.h"
#include "dr_api.h"
#include "drpttracer.h"

/* The auto cleanup wrapper of pttracer handle.
 * This can ensure the pttracer handle is cleaned up when it is out of scope.
 */
struct drpttracer_handle_cleanup_last_t {
public:
    ~drpttracer_handle_cleanup_last_t()
    {
        if (handle != nullptr) {
            void *drcontext = dr_get_current_drcontext();
            drpttracer_destory_handle(drcontext, handle);
            handle = nullptr;
        }
    }
    void *handle = nullptr;
};

/* The auto cleanup wrapper of drpttracer_output_t.
 * This can ensure the pttracer handle is cleaned up when it is out of scope.
 */
struct drpttracer_output_cleanup_last_t {
public:
    ~drpttracer_output_cleanup_last_t()
    {
        if (data != nullptr) {
            void *drcontext = dr_get_current_drcontext();
            drpttracer_destroy_output(drcontext, data);
            data = nullptr;
        }
    }
    drpttracer_output_t *data = nullptr;
};
/* This class is not thread-safe: the caller should create a separate instance per thread.
 */
class syscall_pt_trace_t {
public:
    syscall_pt_trace_t();

    ~syscall_pt_trace_t();

    /* Initialize the syscall_pt_trace_t instance for current thread.
     * The instance will dump the kernel PT trace for every syscall. So we need to pass
     * the output directory and the file write function to the caller.
     */
    bool
    init(IN void *drcontext, IN char *pt_dir_name, IN size_t pt_dir_name_size,
         IN ssize_t (*write_file_func)(file_t file, const void *data, IN size_t count));

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

    /* Get the record id of the last recorded syscall. */
    int
    get_last_recorded_syscall_id()
    {
        return recorded_syscall_num_;
    }

    static bool
    kcore_dump(IN const char *to_dir);

private:
    /* Dump PT trace and the metadata to files. */
    bool
    trace_data_dump(drpttracer_output_cleanup_last_t &output);

    /* The shared file write function. */
    ssize_t (*write_file_func_)(file_t file, const void *data, size_t count);

    /* The pttracer handle holded by this instance. */
    drpttracer_handle_cleanup_last_t pttracer_handle_;

    /* The number of recorded syscall. */
    int recorded_syscall_num_;

    /* The sysnum of current recording syscall. */
    int cur_recording_sysnum_;

    /* The drcontext.
     * We need ensure pass the same context to all drpttracer's APIs.
     */
    void *drcontext_;

    /* The output directory name. */
    char pt_dir_name_[MAXIMUM_PATH];
};

#endif /* _SYSCALL_PT_TRACE_ */
