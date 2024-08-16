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

#ifndef _FUNC_ID_FILTER_H_
#define _FUNC_ID_FILTER_H_ 1

#include "record_filter.h"
#include "trace_entry.h"
#include "utils.h"

#include <cstring>
#include <unordered_set>
#include <vector>

namespace dynamorio {
namespace drmemtrace {

/* This filter takes a list of function IDs for which it preserves the related
 * TRACE_MARKER_TYPE_FUNC_[ID | ARG | RETVAL | RETADDR] markers. It removes all the other
 * TRACE_MARKER_TYPE_FUNC_ markers related to function IDs that are not in the list.
 */
class func_id_filter_t : public record_filter_t::record_filter_func_t {
public:
    func_id_filter_t(std::vector<uint64_t> keep_func_ids_list)
    {
        keep_func_ids_set_.insert(keep_func_ids_list.cbegin(), keep_func_ids_list.cend());
    }

    void *
    parallel_shard_init(memtrace_stream_t *shard_stream,
                        bool partial_trace_filter) override
    {
        per_shard_t *per_shard = new per_shard_t;
        per_shard->output_func_markers = false;
        return per_shard;
    }

    bool
    parallel_shard_filter(
        trace_entry_t &entry, void *shard_data,
        record_filter_t::record_filter_info_t &record_filter_info) override
    {
        /* Get per_shard private data.
         */
        per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);

        trace_type_t entry_type = static_cast<trace_type_t>(entry.type);
        /* Output any trace_entry_t that it's not a marker.
         */
        if (entry_type != TRACE_TYPE_MARKER)
            return true;

        trace_marker_type_t marker_type = static_cast<trace_marker_type_t>(entry.size);
        switch (marker_type) {
        case TRACE_MARKER_TYPE_FUNC_ID: {
            /* Function markers follow this sequence:
             * TRACE_MARKER_TYPE_FUNC_ID
             * [TRACE_MARKER_TYPE_FUNC_RETADDR]
             * [TRACE_MARKER_TYPE_FUNC_ARG]*
             *
             * [entries (instructions, other function markers, etc.)]*
             *
             * TRACE_MARKER_TYPE_FUNC_ID
             * TRACE_MARKER_TYPE_FUNC_RETVAL
             *
             * ([] = 0 or 1, []* = 0 or more)
             *
             * Because TRACE_MARKER_TYPE_FUNC_ID always precedes the remaining
             * function-related markers, we can simply set
             * per_shard->output_func_markers based on the TRACE_MARKER_TYPE_FUNC_ID
             * marker value to handle nested functions.
             */
            uint64_t func_id = static_cast<uint64_t>(entry.addr);
            per_shard->output_func_markers =
                keep_func_ids_set_.find(func_id) != keep_func_ids_set_.end();
            return per_shard->output_func_markers;
        }
        case TRACE_MARKER_TYPE_FUNC_ARG:
        case TRACE_MARKER_TYPE_FUNC_RETVAL:
        case TRACE_MARKER_TYPE_FUNC_RETADDR:
            /* Output these markers only if they belong to a function whose ID we want
             * to keep.
             */
            return per_shard->output_func_markers;
        /* In func_id_filter_t we only handle TRACE_MARKER_TYPE_FUNC_ID,
         * TRACE_MARKER_TYPE_FUNC_ARG, TRACE_MARKER_TYPE_FUNC_RETVAL,
         * TRACE_MARKER_TYPE_FUNC_RETADDR. By default we output all other markers.
         */
        default: return true;
        }
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
        bool output_func_markers;
    };

    std::unordered_set<uint64_t> keep_func_ids_set_;
};

} // namespace drmemtrace
} // namespace dynamorio
#endif /* _FUNC_ID_FILTER_H_ */
