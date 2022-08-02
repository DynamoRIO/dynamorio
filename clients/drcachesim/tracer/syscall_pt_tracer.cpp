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

#include "dr_api.h"
#include "drpttracer.h"
#include "../common/utils.h"
#include "syscall_pt_tracer.h"

#if !defined(X86_64) && !defined(LINUX)
#    error "This is only for Linux x86_64."
#endif

#define RING_BUFFER_SIZE_SHIFT 8

struct drpttracer_output_cleanup_last_t {
public:
    ~drpttracer_output_cleanup_last_t()
    {
        if (output != nullptr) {
            void *drcontext = dr_get_current_drcontext();
            drpttracer_destroy_output(drcontext, output);
            output = nullptr;
        }
    }
    drpttracer_output_t *output;
};

syscall_pt_tracer_t::syscall_pt_tracer_t()
    : write_file_func_(nullptr)
    , drpttracer_handle_(nullptr)
    , recorded_syscall_num_(0)
    , recording_sysnum_(-1)
    , drcontext_(nullptr)
    , pt_dir_name_{'\0'}
{
}

syscall_pt_tracer_t::~syscall_pt_tracer_t()
{
    if (drpttracer_handle_ != nullptr) {
        ASSERT(drcontext_ != nullptr, "drcontext_ is nullptr");
        drpttracer_destory_tracer(drcontext_, drpttracer_handle_);
    }
}

bool
syscall_pt_tracer_t::init(char* pt_dir_name, size_t pt_dir_name_size,
                          ssize_t (*write_file_func)(file_t file, const void *data,
                                                     size_t count))
{
    drcontext_ = dr_get_current_drcontext();
    if (drpttracer_create_tracer(drcontext_, DRPTTRACER_TRACING_ONLY_KERNEL,
                                 RING_BUFFER_SIZE_SHIFT, RING_BUFFER_SIZE_SHIFT,
                                 &drpttracer_handle_) != DRPTTRACER_SUCCESS) {
        drpttracer_handle_ = NULL;
        return false;
    }
    memcpy(pt_dir_name_, pt_dir_name, pt_dir_name_size);
    write_file_func_ = write_file_func;
    return true;
}

bool
syscall_pt_tracer_t::start_syscall_pt_trace(int sysnum)
{
    ASSERT(drpttracer_handle_ != nullptr, "drpttracer_handle_ is nullptr");
    ASSERT(drcontext_ != nullptr, "drcontext_ is nullptr");
    if (drpttracer_start_tracing(drcontext_, drpttracer_handle_) != DRPTTRACER_SUCCESS) {
        return false;
    }
    recording_sysnum_ = sysnum;
    return true;
}

bool
syscall_pt_tracer_t::stop_syscall_pt_trace()
{
    ASSERT(drpttracer_handle_ != nullptr, "drpttracer_handle_ is nullptr");
    ASSERT(drcontext_ != nullptr, "drcontext_ is nullptr");

    drpttracer_output_cleanup_last_t output;
    if (drpttracer_stop_tracing(drcontext_, drpttracer_handle_, &output.output) !=
        DRPTTRACER_SUCCESS) {
        return false;
    }
    recording_sysnum_ = -1;
    recorded_syscall_num_++;
    return trace_data_dump(output.output->pt, output.output->pt_size,
                           &output.output->metadata);
}

bool
syscall_pt_tracer_t::trace_data_dump(void *pt, size_t pt_size, void *pt_meta)
{
    ASSERT(drcontext_ != nullptr, "drcontext_ is nullptr");
    ASSERT(pt != nullptr, "pt is nullptr");
    ASSERT(pt_size > 0, "pt_size is 0");
    ASSERT(pt_meta != nullptr, "pt_meta is nullptr");

    if (pt == nullptr || pt_size == 0 || pt_meta == nullptr) {
        return false;
    }
    char pt_filename[MAXIMUM_PATH];
    dr_snprintf(pt_filename, BUFFER_SIZE_ELEMENTS(pt_filename), "%s%s%d.%d.pt",
                pt_dir_name_, DIRSEP, dr_get_thread_id(drcontext_),
                recorded_syscall_num_);
    file_t pt_file = dr_open_file(pt_filename, DR_FILE_WRITE_OVERWRITE);
    write_file_func_(pt_file, pt, pt_size);
    dr_close_file(pt_file);

    char pt_metadata_filename[MAXIMUM_PATH];
    dr_snprintf(pt_metadata_filename, BUFFER_SIZE_ELEMENTS(pt_metadata_filename),
                "%s.metadata", pt_filename);
    file_t pt_metadata_file = dr_open_file(pt_metadata_filename, DR_FILE_WRITE_OVERWRITE);
    write_file_func_(pt_metadata_file, pt_meta, sizeof(pt_metadata_t));
    dr_close_file(pt_metadata_file);
    return true;
}
