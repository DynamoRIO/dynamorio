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

/* syscall_pt_trace.cpp: module for recording kernel PT traces for every syscall. */

#include <cstring>

/* For SYS_exit,SYS_exit_group. */
#include "../../../core/unix/include/syscall_linux_x86.h"
#include "dr_api.h"
#include "drpttracer.h"
#include "kcore_copy.h"
#include "syscall_pt_trace.h"

#ifndef BUILD_PT_TRACER
#    error "This module requires the drpttracer extension."
#endif

#define PT_DATA_FILE_NAME_SUFFIX ".pt"
#define PT_METADATA_FILE_NAME_SUFFIX ".metadata"

syscall_pt_trace_t::syscall_pt_trace_t()
    : open_file_func_(nullptr)
    , write_file_func_(nullptr)
    , close_file_func_(nullptr)
    , pttracer_handle_ { GLOBAL_DCONTEXT, nullptr }
    , recorded_syscall_count_(0)
    , cur_recording_sysnum_(-1)
    , drcontext_(nullptr)
    , pt_dir_name_ { '\0' }
{
}

syscall_pt_trace_t::~syscall_pt_trace_t()
{
}

bool
syscall_pt_trace_t::init(void *drcontext, char *pt_dir_name, size_t pt_dir_name_size,
                         drmemtrace_open_file_func_t open_file_func,
                         drmemtrace_write_file_func_t write_file_func,
                         drmemtrace_close_file_func_t close_file_func)
{
    drcontext_ = drcontext;
    memcpy(pt_dir_name_, pt_dir_name, pt_dir_name_size);
    open_file_func_ = open_file_func;
    write_file_func_ = write_file_func;
    close_file_func_ = close_file_func;
    pttracer_handle_ = { drcontext, nullptr };
    return true;
}

bool
syscall_pt_trace_t::start_syscall_pt_trace(int sysnum)
{
#define RING_BUFFER_SIZE_SHIFT 8
    ASSERT(drcontext_ != nullptr, "drcontext_ is nullptr");
    ASSERT(pttracer_handle_.handle == nullptr, "pttracer_handle_.handle isn't nullptr");

    /* TODO i#5505: To reduce the overhead caused by pttracer initialization, we should
     * share the same pttracer handle for all syscalls. We will do this when we upgrade
     * drpt2ir to support decoding PT fragments that don't have PSB packets.
     */
    if (drpttracer_create_handle(drcontext_, DRPTTRACER_TRACING_ONLY_KERNEL,
                                 RING_BUFFER_SIZE_SHIFT, RING_BUFFER_SIZE_SHIFT,
                                 &pttracer_handle_.handle) != DRPTTRACER_SUCCESS) {
        return false;
    }
    if (drpttracer_start_tracing(drcontext_, pttracer_handle_.handle) !=
        DRPTTRACER_SUCCESS) {
        return false;
    }
    cur_recording_sysnum_ = sysnum;
    return true;
}

bool
syscall_pt_trace_t::stop_syscall_pt_trace()
{
    ASSERT(drcontext_ != nullptr, "drcontext_ is nullptr");
    ASSERT(pttracer_handle_.handle != nullptr, "pttracer_handle_.handle is nullptr");

    drpttracer_output_autoclean_t output = { drcontext_, nullptr };
    if (drpttracer_stop_tracing(drcontext_, pttracer_handle_.handle, &output.data) !=
        DRPTTRACER_SUCCESS) {
        return false;
    }

    /* TODO i#5505: If we can share the same pttracer handle for all syscalls, we can
     * avoid resetting the handle.
     */
    pttracer_handle_.reset();
    cur_recording_sysnum_ = -1;
    recorded_syscall_count_++;
    return trace_data_dump(output);
}

bool
syscall_pt_trace_t::trace_data_dump(drpttracer_output_autoclean_t &output)
{
    ASSERT(drcontext_ != nullptr, "drcontext_ is nullptr");

    drpttracer_output_t *data = output.data;

    ASSERT(data->pt != nullptr, "pt is nullptr");
    ASSERT(data->pt_size > 0, "pt_size is 0");
    if (data->pt == nullptr || data->pt_size == 0) {
        return false;
    }

    /* TODO i#5505: To reduce the overhead caused by IO, we better use a buffer to store
     * multiple PT trace data. And only dump the buffer when it is full.
     */

    /* Dump PT trace data to file '{thread_id}.{syscall_id}.pt'. */
    char pt_filename[MAXIMUM_PATH];
    dr_snprintf(pt_filename, BUFFER_SIZE_ELEMENTS(pt_filename), "%s%s%d.%d%s",
                pt_dir_name_, DIRSEP, dr_get_thread_id(drcontext_),
                recorded_syscall_count_, PT_DATA_FILE_NAME_SUFFIX);
    file_t pt_file = open_file_func_(pt_filename, DR_FILE_WRITE_REQUIRE_NEW);
    write_file_func_(pt_file, data->pt, data->pt_size);
    close_file_func_(pt_file);

    /* Dump PT trace data to file '{thread_id}.{syscall_id}.pt.meta'. */
    char pt_metadata_filename[MAXIMUM_PATH];
    dr_snprintf(pt_metadata_filename, BUFFER_SIZE_ELEMENTS(pt_metadata_filename), "%s%s",
                pt_filename, PT_METADATA_FILE_NAME_SUFFIX);
    file_t pt_metadata_file =
        open_file_func_(pt_metadata_filename, DR_FILE_WRITE_OVERWRITE);
    write_file_func_(pt_metadata_file, &data->metadata, sizeof(pt_metadata_t));
    close_file_func_(pt_metadata_file);
    return true;
}

bool
syscall_pt_trace_t::is_syscall_pt_trace_enabled(IN int sysnum)
{
    /* The following syscall's post syscall callback can't be triggered. So we don't
     * support to recording the kernel PT of them.
     */
    if (sysnum == SYS_exit || sysnum == SYS_exit_group || sysnum == SYS_execve) {
        return false;
    }
    return true;
}
