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

namespace dynamorio {
namespace drmemtrace {

// A trimming tool to remove records from the start and end of a trace.  To ensure
// alignment across threads, we trim by timestamp.  Since timestamps are inserted
// only at certain points, this necessarily disallows precise trimming at say certain
// instructions, but at the gain of consistent inter-thread trimming.
class trim_filter_t : public record_filter_t::record_filter_func_t {
public:
    trim_filter_t(uint64_t trim_before_timestamp = 0, uint64_t trim_after_timestamp = 0,
                  uint64_t trim_before_instr = 0, uint64_t trim_after_instr = 0)
        : trim_before_timestamp_(trim_before_timestamp)
        , trim_after_timestamp_(trim_after_timestamp)
        , trim_before_instr_(trim_before_instr)
        , trim_after_instr_(trim_after_instr)
    {
        // We don't support trimming by timestamp and instruction ordinal at the same
        // time, as it adds unnecessary complexities.
        if ((trim_before_timestamp_ > 0 || trim_after_timestamp_ > 0) &&
            (trim_before_instr_ > 0 || trim_after_instr_ > 0)) {
            error_string_ = "trim_[before | after]_timestamp and trim_[before | "
                            "after]_instr cannot be used at the same time";
        } else {
            // Support 0 to make it easier for users to have no trim-after.
            if (trim_after_timestamp_ == 0) {
                trim_after_timestamp_ = (std::numeric_limits<uint64_t>::max)();
            }
            if (trim_after_timestamp_ <= trim_before_timestamp_) {
                error_string_ =
                    "trim_before_timestamp = " + std::to_string(trim_before_timestamp_) +
                    " must be less than trim_after_timestamp = " +
                    std::to_string(trim_after_timestamp_) + ". ";
            }
            if (trim_after_instr_ == 0) {
                trim_after_instr_ = (std::numeric_limits<uint64_t>::max)();
            }
            if (trim_after_instr_ <= trim_before_instr_) {
                error_string_ =
                    "trim_before_instr = " + std::to_string(trim_before_instr_) +
                    " must be less than trim_after_instr = " +
                    std::to_string(trim_after_instr_) + ".";
            }
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
        if (type_is_instr(static_cast<trace_type_t>(entry.type))) {
            ++per_shard->instr_counter;
        }
        if (entry.type == TRACE_TYPE_MARKER) {
            switch (entry.size) {
            case TRACE_MARKER_TYPE_TIMESTAMP:
                // While it seems theoretically nice to keep the timestamp,cpuid that
                // is over the threshold so we have a timestamp at the end, that results
                // in large time gaps if across a blocking syscall.  Trying to edit
                // that timestamp a la -align_endpoints is not deal either as it can
                // distort syscall durations.  The least-bad solution seems to be to
                // keep the regular trace content right up to the timestamp and
                // throw away the timestamp.
                if (entry.addr < trim_before_timestamp_ ||
                    entry.addr > trim_after_timestamp_) {
                    per_shard->in_removed_region = true;
                } else {
                    per_shard->in_removed_region = false;
                }
                // We cannot remove records until we see a timestamp, so we have to wait
                // until this TRACE_MARKER_TYPE_TIMESTAMP and start/stop trimming from
                // there. We include trim_after_instr to cover the case where the
                // instruction ordinal is just before a timestamp, so we start trimming
                // from there and not the next timestamp instead, which can come after
                // several other instructions.
                if (per_shard->instr_counter < trim_before_instr_ ||
                    per_shard->instr_counter >= trim_after_instr_) {
                    per_shard->in_removed_region_instr = true;
                } else {
                    per_shard->in_removed_region_instr = false;
                }
                per_shard->in_removed_region =
                    per_shard->in_removed_region || per_shard->in_removed_region_instr;
                break;
            case TRACE_MARKER_TYPE_WINDOW_ID:
                // Always emit the very first TRACE_MARKER_TYPE_WINDOW_ID marker, so no
                // matter where we trim, the trace will always start with it (after the
                // header).
                if (per_shard->window_id == static_cast<addr_t>(-1)) {
                    per_shard->window_id = entry.addr;
                    return true;
                } else {
                    // Check that all window markers in the trace have the same ID.
                    // XXX i#7531: We currently don't support trimming a trace with
                    // multiple windows because we cannot make any assumption on the order
                    // of timestamp and window markers, and currently record_filter
                    // doesn't support adding records or preserving a deleted header,
                    // hence we don't have an easy way to insert a new window marker right
                    // before the region we intend to keep (which would be the last ID
                    // seen before the trace region we want to preserve). This is the
                    // reason why we always emit the first, original window marker, which
                    // we know will have the right ID, since all the window IDs have to be
                    // the same.
                    if (per_shard->window_id != entry.addr) {
                        error_string_ = "Trimming a trace with multiple windows is not "
                                        "supported. Previous window_id = " +
                            std::to_string(per_shard->window_id) +
                            ", current window_id = " + std::to_string(entry.addr);
                        return false;
                    }
                }
                break;
            }
        }
        if (entry.type == TRACE_TYPE_THREAD_EXIT || entry.type == TRACE_TYPE_FOOTER) {
            // Don't throw the footer away.  (The header is always kept because we
            // don't start removing until we see a timestamp marker.)
            // TODO i#6635: For core-sharded there will be multiple thread exits
            // so we need to handle that.  For thread-sharded we assume just one.
            // (We do not support trimming a single-file multi-window trace).
            return true;
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
        bool in_removed_region_instr = false;
        addr_t window_id = static_cast<addr_t>(-1);
        uint64_t instr_counter = 0;
    };

    uint64_t trim_before_timestamp_;
    uint64_t trim_after_timestamp_;
    uint64_t trim_before_instr_;
    uint64_t trim_after_instr_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _TRIM_FILTER_H_ */
