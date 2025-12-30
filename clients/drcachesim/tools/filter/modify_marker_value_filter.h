/* **********************************************************
 * Copyright (c) 2024-2025 Google, Inc.  All rights reserved.
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

#ifndef _MODIFY_MARKER_VALUE_FILTER_H_
#define _MODIFY_MARKER_VALUE_FILTER_H_

#include "record_filter.h"
#include "trace_entry.h"

#include <cstring>
#include <unordered_map>

namespace dynamorio {
namespace drmemtrace {

/* This filter takes a list of <TRACE_MARKER_TYPE_,new_value> pairs and modifies the value
 * of all listed markers in the trace with the given new_value.
 */
class modify_marker_value_filter_t : public record_filter_t::record_filter_func_t {
public:
    modify_marker_value_filter_t(std::vector<uint64_t> modify_marker_value_pairs_list)
    {
        size_t list_size = modify_marker_value_pairs_list.size();
        if (list_size == 0) {
            error_string_ = "List of <TRACE_MARKER_TYPE_,new_value> pairs is empty.";
        } else if (list_size % 2 != 0) {
            error_string_ = "List of <TRACE_MARKER_TYPE_,new_value> pairs is missing "
                            "part of a pair as its size is not even";
        } else {
            for (size_t i = 0; i < list_size; i += 2) {
                trace_marker_type_t marker_type =
                    static_cast<trace_marker_type_t>(modify_marker_value_pairs_list[i]);
                uint64_t new_value = modify_marker_value_pairs_list[i + 1];
                // We ignore duplicate pairs and use the last pair in the list.
                marker_to_value_map_[marker_type] = new_value;
            }
        }
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
        // Output any trace_entry_t that's not a marker.
        if (entry_type != TRACE_TYPE_MARKER)
            return true;

        // Check if the TRACE_TYPE_MARKER_ is in the list of markers for which we want to
        // overwrite their value. If not, output the marker unchanged.
        trace_marker_type_t marker_type = static_cast<trace_marker_type_t>(entry.size);
        const auto &it = marker_to_value_map_.find(marker_type);
        if (it == marker_to_value_map_.end())
            return true;

        // Overwrite marker value.
        entry.addr = static_cast<addr_t>(it->second);

        return true;
    }

    bool
    parallel_shard_exit(void *shard_data) override
    {
        return true;
    }

private:
    std::unordered_map<trace_marker_type_t, uint64_t> marker_to_value_map_;
};

} // namespace drmemtrace
} // namespace dynamorio
#endif /* _MODIFY_MARKER_VALUE_FILTER_H_ */
