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

#ifndef _TYPE_FILTER_H_
#define _TYPE_FILTER_H_ 1

#include "record_filter.h"
#include "trace_entry.h"

#include <vector>
#include <unordered_set>

#ifdef MACOS
// Provide trace_type_t and trace_marker_type_t specializations to
// allow declaration of unordered_set<_> below. This was needed for
// OSX particularly. In C++14, std::hash works as expected for enums
// too: https://cplusplus.com/forum/general/238538/.
namespace std {
template <> struct hash<trace_type_t> {
    size_t
    operator()(trace_type_t t) const
    {
        return static_cast<size_t>(t);
    }
};
template <> struct hash<trace_marker_type_t> {
    size_t
    operator()(trace_marker_type_t t) const
    {
        return static_cast<size_t>(t);
    }
};
} // namespace std
#endif

namespace dynamorio {
namespace drmemtrace {

class type_filter_t : public record_filter_t::record_filter_func_t {
public:
    type_filter_t(std::vector<trace_type_t> remove_trace_types,
                  std::vector<trace_marker_type_t> remove_marker_types)
    {
        for (auto trace_type : remove_trace_types) {
            remove_trace_types_.insert(trace_type);
        }
        for (auto marker_type : remove_marker_types) {
            remove_marker_types_.insert(marker_type);
        }
    }
    void *
    parallel_shard_init(memtrace_stream_t *shard_stream) override
    {
        return nullptr;
    }
    bool
    parallel_shard_filter(const trace_entry_t &entry, void *shard_data) override
    {
        if (remove_trace_types_.find(static_cast<trace_type_t>(entry.type)) !=
            remove_trace_types_.end()) {
            return false;
        }
        if (entry.type == TRACE_TYPE_MARKER) {
            return remove_marker_types_.find(static_cast<trace_marker_type_t>(
                       entry.size)) == remove_marker_types_.end();
        }
        return true;
    }
    bool
    parallel_shard_exit(void *shard_data) override
    {
        return true;
    }

private:
    std::unordered_set<trace_type_t> remove_trace_types_;
    std::unordered_set<trace_marker_type_t> remove_marker_types_;
};

} // namespace drmemtrace
} // namespace dynamorio
#endif /* _TYPE_FILTER_H_ */
