/* **********************************************************
 * Copyright (c) 2026 Google, Inc.  All rights reserved.
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

#ifndef _KERNEL_FILTER_H_
#define _KERNEL_FILTER_H_

#include "record_filter.h"

namespace dynamorio {
namespace drmemtrace {

// Removes all entries between and including the pairs of markers representing
// kernel execution: for syscalls (TRACE_MARKER_TYPE_SYSCALL_TRACE_START,
// TRACE_MARKER_TYPE_SYSCALL_TRACE_END), and context switches
// (TRACE_MARKER_TYPE_CONTEXT_SWITCH_START and TRACE_MARKER_TYPE_CONTEXT_SWITCH_END).
class kernel_filter_t : public record_filter_t::record_filter_func_t {
public:
    struct shard_data_t {
        kernel_tracker_tmpl_t<trace_entry_t> kernel_tracker;
    };

    void *
    parallel_shard_init(memtrace_stream_t *shard_stream,
                        bool partial_trace_filter) override
    {
        return new shard_data_t();
    }
    bool
    parallel_shard_filter(
        trace_entry_t &entry, void *shard_data,
        record_filter_t::record_filter_info_t &record_filter_info) override
    {
        shard_data_t *data = reinterpret_cast<shard_data_t *>(shard_data);
        data->kernel_tracker.update(entry);
        return !data->kernel_tracker.in_kernel_trace();
    }
    bool
    parallel_shard_exit(void *shard_data) override
    {
        delete reinterpret_cast<shard_data_t *>(shard_data);
        return true;
    }
    uint64_t
    update_filetype(uint64_t filetype) override
    {
        filetype &= ~OFFLINE_FILE_TYPE_KERNEL_SYSCALLS;
        return filetype;
    }
};

} // namespace drmemtrace
} // namespace dynamorio
#endif /* _KERNEL_FILTER_H_ */
