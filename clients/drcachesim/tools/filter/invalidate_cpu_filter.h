/* **********************************************************
 * Copyright (c) 2024 Google, Inc.  All rights reserved.
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

#ifndef _INVALIDATE_CPU_FILTER_H_
#define _INVALIDATE_CPU_FILTER_H_ 1

#include "record_filter.h"
#include "trace_entry.h"

#include <cstring>

#define INVALID_CPU_MARKER_VALUE ((uintptr_t) - 1)

namespace dynamorio {
namespace drmemtrace {

/* This filter invalidates the value of TRACE_MARKER_TYPE_CPU_ID by setting its value to
 * (uintptr_t)-1, which indicates that the CPU could not be determined.
 */
class invalidate_cpu_filter_t : public record_filter_t::record_filter_func_t {
public:
    invalidate_cpu_filter_t()
    {
    }

    void *
    parallel_shard_init(memtrace_stream_t *shard_stream,
                        bool partial_trace_filter) override
    {
        return nullptr;
    }

    bool
    parallel_shard_filter(
        trace_entry_t &entry, void *shard_data,
        record_filter_t::record_filter_info_t &record_filter_info) override
    {
        trace_type_t entry_type = static_cast<trace_type_t>(entry.type);
        // Output any trace_entry_t that it's not a marker.
        if (entry_type != TRACE_TYPE_MARKER)
            return true;

        trace_marker_type_t marker_type = static_cast<trace_marker_type_t>(entry.size);
        // Output any trace_entry_t that it's not a CPU marker.
        if (marker_type != TRACE_MARKER_TYPE_CPU_ID)
            return true;

        // Invalidate CPU marker value.
        entry.addr = INVALID_CPU_MARKER_VALUE;

        return true;
    }

    bool
    parallel_shard_exit(void *shard_data) override
    {
        return true;
    }
};

} // namespace drmemtrace
} // namespace dynamorio
#endif /* _INCALIDATE_CPU_FILTER_H_ */
