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

/* syscall_pt_tracer.h: header of module for recording kernel PT traces for every syscall.
 */

#ifndef _SYSCALL_PT_TRACER_
#define _SYSCALL_PT_TRACER_ 1

#include <cstddef>
#include "dr_api.h"

/* This class is not thread-safe: the caller should create a separate instance per thread.
 */
class syscall_pt_tracer_t {
public:
    syscall_pt_tracer_t();
    ~syscall_pt_tracer_t();

    bool
    init(char* pt_dir_name, size_t pt_dir_name_size,
         ssize_t (*write_file_func)(file_t file, const void *data, size_t count));

    bool
    start_syscall_pt_trace(int sysnum);

    bool
    stop_syscall_pt_trace();

    int
    get_recording_sysnum()
    {
        return recording_sysnum_;
    }

    int
    get_last_recorded_syscall_id()
    {
        return recorded_syscall_num_;
    }

private:
    bool
    trace_data_dump(void *pt, size_t pt_size, void *pt_meta);

    ssize_t (*write_file_func_)(file_t file, const void *data, size_t count);
    void *drpttracer_handle_;
    int recorded_syscall_num_;
    int recording_sysnum_;
    void *drcontext_;
    char pt_dir_name_[MAXIMUM_PATH];
};

#endif /* _SYSCALL_PT_TRACER_ */
