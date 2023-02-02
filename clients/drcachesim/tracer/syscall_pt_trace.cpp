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
#include "syscall_pt_trace.h"

#ifndef BUILD_PT_TRACER
#    error "This module requires the drpttracer extension."
#endif

#define PT_DATA_FILE_NAME_SUFFIX ".pt"

syscall_pt_trace_t::syscall_pt_trace_t()
    : open_file_func_(nullptr)
    , write_file_func_(nullptr)
    , close_file_func_(nullptr)
    , pttracer_handle_ { GLOBAL_DCONTEXT, nullptr }
    , pttracer_output_buffer_ { GLOBAL_DCONTEXT, nullptr }
    , recorded_syscall_count_(0)
    , cur_recording_sysnum_(-1)
    , drcontext_(nullptr)
    , output_file_(INVALID_FILE)
{
}

syscall_pt_trace_t::~syscall_pt_trace_t()
{
    if (output_file_ != INVALID_FILE) {
        close_file_func_(output_file_);
    }
}

bool
syscall_pt_trace_t::init(void *drcontext, char* pt_dir_name,
                         drmemtrace_open_file_func_t open_file_func,
                         drmemtrace_write_file_func_t write_file_func,
                         drmemtrace_close_file_func_t close_file_func)
{
    drcontext_ = drcontext;
    open_file_func_ = open_file_func;
    write_file_func_ = write_file_func;
    close_file_func_ = close_file_func;
    pttracer_handle_ = { drcontext, nullptr };
    pttracer_output_buffer_ = { drcontext_, nullptr };
    std::string output_file_name(pt_dir_name);
    output_file_name += "/" +
        std::to_string(dr_get_thread_id(drcontext_)) + PT_DATA_FILE_NAME_SUFFIX;
    output_file_ = open_file_func_(output_file_name.c_str(), DR_FILE_WRITE_REQUIRE_NEW);
#define RING_BUFFER_SIZE_SHIFT 8

    /* To reduce the overhead caused by pttracer initialization, we shared the same
     * pttracer handle for all syscalls per thread.
     */
    if (drpttracer_create_handle(drcontext_, DRPTTRACER_TRACING_ONLY_KERNEL,
                                 RING_BUFFER_SIZE_SHIFT, RING_BUFFER_SIZE_SHIFT,
                                 &pttracer_handle_.handle) != DRPTTRACER_SUCCESS) {
        return false;
    }
    if (drpttracer_create_output(drcontext_, RING_BUFFER_SIZE_SHIFT, 0,
                                 &pttracer_output_buffer_.data) != DRPTTRACER_SUCCESS) {
        return false;
    }

    /* Initialize the header of output buffer. */
    output_buffer_[0].pid.type = SYSCALL_PT_ENTRY_TYPE_PID;
    output_buffer_[0].pid.pid = dr_get_process_id_from_drcontext(drcontext_);
    output_buffer_[1].tid.type = SYSCALL_PT_ENTRY_TYPE_THREAD;
    output_buffer_[1].tid.tid = dr_get_thread_id(drcontext_);

    /* All syscalls in the same thread share the same pttracer handle. So they shared the
     * same pt_metadata.
     */
    pt_metadata_t pt_metadata;
    if (drpttracer_get_pt_metadata(pttracer_handle_.handle, &pt_metadata)) {
        return false;
    }
    if (!metadata_dump(pt_metadata)) {
        return false;
    }

    return true;
}

bool
syscall_pt_trace_t::start_syscall_pt_trace(IN int sysnum)
{
    ASSERT(drcontext_ != nullptr, "drcontext_ is nullptr");
    ASSERT(pttracer_handle_.handle == nullptr, "pttracer_handle_.handle isn't nullptr");
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
    ASSERT(pttracer_output_buffer_.data != nullptr,
           "pttracer_output_buffer_.data is nullptr");
    ASSERT(output_file_ != INVALID_FILE, "output_file_ is INVALID_FILE");

    if (drpttracer_stop_tracing(drcontext_, pttracer_handle_.handle,
                                pttracer_output_buffer_.data) != DRPTTRACER_SUCCESS) {
        return false;
    }

    if (!trace_data_dump(pttracer_output_buffer_)) {
        return false;
    }
    cur_recording_sysnum_ = -1;
    recorded_syscall_count_++;
    return true;
}

bool
syscall_pt_trace_t::metadata_dump(pt_metadata_t metadata)
{
    ASSERT(output_file_ != INVALID_FILE, "output_file_ is INVALID_FILE");
    if (output_file_ == INVALID_FILE) {
        return false;
    }

    memset(output_buffer_ + 2, 0,
           sizeof(syscall_pt_entry_t) * (MAX_NUM_SYSCALL_PT_ENTRIES - 2));

    /* Initialize the metadata boundary. */
    output_buffer_[2].pt_metadata_boundary.type =
        SYSCALL_PT_ENTRY_TYPE_PT_METADATA_BOUNDARY;
    output_buffer_[2].pt_metadata_boundary.data_size = sizeof(pt_metadata_t);

    /* Append the metadata to buffer. */
    memcpy(output_buffer_ + 3, &metadata, sizeof(pt_metadata_t));

    /* Write the buffer to the ourput file */
    if (write_file_func_(output_file_, output_buffer_,
                         MAX_NUM_SYSCALL_PT_ENTRIES * sizeof(syscall_pt_entry_t)) == 0) {
        return false;
    }

    return true;
}

bool
syscall_pt_trace_t::trace_data_dump(drpttracer_output_autoclean_t &output)
{
    ASSERT(output_file_ != INVALID_FILE, "output_file_ is INVALID_FILE");
    if (output_file_ == INVALID_FILE) {
        return false;
    }

    drpttracer_output_t *data = output.data;
    ASSERT(data != nullptr, "output.data is nullptr");
    ASSERT(data->pt_buffer != nullptr, "pt_buffer is nullptr");
    ASSERT(data->pt_size > 0, "pt_size is 0");
    if (data == nullptr || data->pt_buffer == nullptr || data->pt_size == 0) {
        return false;
    }

    memset(output_buffer_ + 2, 0,
           sizeof(syscall_pt_entry_t) * (MAX_NUM_SYSCALL_PT_ENTRIES - 2));

    /* Initialize the syscall metadata boundary. */
    output_buffer_[2].syscall_metadata_boundary.type =
        SYSCALL_PT_ENTRY_TYPE_SYSCALL_METADATA_BOUNDARY;

    /* Initialize the sysnum. */
    output_buffer_[3].sysnum.type = SYSCALL_PT_ENTRY_TYPE_SYSNUM;
    output_buffer_[3].sysnum.sysnum = cur_recording_sysnum_;

    /* Initialize the syscall id. */
    output_buffer_[4].syscall_id.type = SYSCALL_PT_ENTRY_TYPE_SYSCALL_ID;
    output_buffer_[4].syscall_id.id = recorded_syscall_count_;

    /* Initialize the PT data size of this syscall. */
    output_buffer_[5].syscall_pt_data_size.type =
        SYSCALL_PT_ENTRY_TYPE_SYSCALL_PT_DATA_SIZE;
    output_buffer_[5].syscall_pt_data_size.pt_data_size = data->pt_size;

    /* Initialize the the parameter of current record syscall.
     * TODO i#5505: dynamorio doesn't provide a function to get syscall's
     * parameter number. So currently we can't get and dump any syscall's
     * parameters. And we dump the parameter number with a fixed value 0. We
     * should fix this issue by implementing a new function that can get syscall's
     * parameter number.
     */
    output_buffer_[6].syscall_args_boundary.type =
        SYSCALL_PT_ENTRY_TYPE_SYSCALL_ARGS_BOUNDARY;
    output_buffer_[6].syscall_args_boundary.args_num = 0;

    output_buffer_[2].syscall_metadata_boundary.data_size =
        sizeof(syscall_pt_entry_t) * 4 +
        sizeof(syscall_pt_entry_t) * output_buffer_[6].syscall_args_boundary.args_num;

    /* Write the syscall's metadata to the output file. */
    if (write_file_func_(output_file_, output_buffer_,
                         MAX_NUM_SYSCALL_PT_ENTRIES * sizeof(syscall_pt_entry_t)) == 0) {
        return false;
    }

    ssize_t undumped_size = data->pt_size;
    while (undumped_size > 0) {
        memset(output_buffer_ + 2, 0,
               sizeof(syscall_pt_entry_t) * (MAX_NUM_SYSCALL_PT_ENTRIES - 2));

        /* Append undumped PT data to current PT DATA Buffer. */
        int pt_boundary_idx = 2;
        size_t cur_dump_size =
            std::min(undumped_size,
                     (int64_t)sizeof(syscall_pt_entry_t) *
                         (MAX_NUM_SYSCALL_PT_ENTRIES - pt_boundary_idx - 1));
        output_buffer_[pt_boundary_idx].pt_data_boundary.type =
            SYSCALL_PT_ENTRY_TYPE_PT_DATA_BOUNDARY;
        output_buffer_[pt_boundary_idx].pt_data_boundary.data_size = cur_dump_size;
        char *pt_buffer = (char *)data->pt_buffer + (data->pt_size - undumped_size);
        memcpy(output_buffer_ + pt_boundary_idx + 1, pt_buffer, cur_dump_size);
        undumped_size -= cur_dump_size;
        if (undumped_size <= 0) {
            output_buffer_[pt_boundary_idx].pt_data_boundary.is_last = 1;
        } else {
            output_buffer_[pt_boundary_idx].pt_data_boundary.is_last = 0;
        }

        /* Write the buffer to the output file. */
        if (write_file_func_(output_file_, output_buffer_,
                             MAX_NUM_SYSCALL_PT_ENTRIES * sizeof(syscall_pt_entry_t)) ==
            0) {
            return false;
        }
    }
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
