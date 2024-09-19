/* **********************************************************
 * Copyright (c) 2022-2024 Google, Inc.  All rights reserved.
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

#ifndef _TRIM_FILTER_H_
#define _TRIM_FILTER_H_ 1

#include "record_filter.h"
#include "trace_entry.h"

#include <limits>
#include <vector>
#include <unordered_set>

namespace dynamorio {
namespace drmemtrace {

// A trimming tool to remove records from the start and end of a trace.  To ensure
// alignment across threads, we trim by timestamp.  Since timestamps are inserted
// only at certain points, this necessarily disallows precise trimming at say certain
// instructions, but at the gain of consistent inter-thread trimming.
class trim_filter_t : public record_filter_t::record_filter_func_t {
public:
    trim_filter_t(uint64_t trim_before_timestamp, uint64_t trim_after_timestamp)
        : trim_before_timestamp_(trim_before_timestamp)
        , trim_after_timestamp_(trim_after_timestamp)
    {
        // Support 0 to make it easier for users to have no trim-after.
        if (trim_after_timestamp_ == 0) {
            trim_after_timestamp_ = (std::numeric_limits<uint64_t>::max)();
        }
        if (trim_after_timestamp_ <= trim_before_timestamp_) {
            error_string_ = "Invalid parameters: end must be > start";
        }
    }
    void *
    parallel_shard_init(memtrace_stream_t *shard_stream,
                        bool partial_trace_filter) override
    {
        per_shard_t *per_shard = new per_shard_t;
        return per_shard;
    }
    bool
    parallel_shard_filter(
        trace_entry_t &entry, void *shard_data,
        record_filter_t::record_filter_info_t &record_filter_info) override
    {
        per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);
        if (entry.type == TRACE_TYPE_MARKER &&
            entry.size == TRACE_MARKER_TYPE_TIMESTAMP) {
            // While it seems theoretically nice to keep the timestamp,cpuid that
            // is over the threshold so we have a timestamp at the end, that results
            // in large time gaps if across a blocking syscall.  Trying to edit
            // that timestamp a la -align_endpoints is not deal either as it can
            // distort syscall durations.  The least-bad solution seems to be to
            // keep the regular trace content right up to the timestamp and
            // throw away the timestamp.
            if (entry.addr < trim_before_timestamp_ || entry.addr > trim_after_timestamp_)
                per_shard->in_removed_region = true;
            else
                per_shard->in_removed_region = false;
        }
        if (entry.type == TRACE_TYPE_THREAD_EXIT || entry.type == TRACE_TYPE_FOOTER) {
            // Don't throw the footer away.  (The header is always kept because we
            // don't start removing until we see a timestamp marker.)
            // TODO i#6635: For core-sharded there will be multiple thread exits
            // so we need to handle that.  For thread-sharded we assume just one.
            // (We do not support trimming a single-file multi-window trace).
            return true;
        }
        if (entry.type == TRACE_TYPE_MARKER &&
            entry.size == TRACE_MARKER_TYPE_WINDOW_ID) {
            error_string_ = "Trimming WINDOW_ID markers is not supported";
        }
        return !per_shard->in_removed_region;
    }
    bool
    parallel_shard_exit(void *shard_data) override
    {
        per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);
        delete per_shard;
        return true;
    }

private:
    struct per_shard_t {
        bool in_removed_region = false;
    };

    uint64_t trim_before_timestamp_;
    uint64_t trim_after_timestamp_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _TRIM_FILTER_H_ */
