/* **********************************************************
 * Copyright (c) 2022-2023 Google, Inc.  All rights reserved.
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
template <> struct hash<dynamorio::drmemtrace::trace_type_t> {
    size_t
    operator()(dynamorio::drmemtrace::trace_type_t t) const
    {
        return static_cast<size_t>(t);
    }
};
template <> struct hash<dynamorio::drmemtrace::trace_marker_type_t> {
    size_t
    operator()(dynamorio::drmemtrace::trace_marker_type_t t) const
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
    parallel_shard_init(memtrace_stream_t *shard_stream,
                        bool partial_trace_filter) override
    {
        per_shard_t *per_shard = new per_shard_t;
        per_shard->partial_trace_filter = partial_trace_filter;
        return per_shard;
    }
    bool
    parallel_shard_filter(trace_entry_t &entry, void *shard_data) override
    {
        per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);
        if (entry.type == TRACE_TYPE_MARKER && entry.size == TRACE_MARKER_TYPE_FILETYPE) {
            if (TESTANY(entry.addr, OFFLINE_FILE_TYPE_ENCODINGS) &&
                !per_shard->partial_trace_filter &&
                remove_trace_types_.find(TRACE_TYPE_ENCODING) !=
                    remove_trace_types_.end()) {
                entry.addr &= ~OFFLINE_FILE_TYPE_ENCODINGS;
            }
            for (trace_type_t type : remove_trace_types_) {
                // Not modifying file type for filtering of prefetch/flush entries.
                if (type_is_instr(type))
                    entry.addr |= OFFLINE_FILE_TYPE_IFILTERED;
                else if (type == TRACE_TYPE_READ || type == TRACE_TYPE_WRITE)
                    entry.addr |= OFFLINE_FILE_TYPE_DFILTERED;
            }
            return true;
        }
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
        per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);
        delete per_shard;
        return true;
    }

private:
    struct per_shard_t {
        bool partial_trace_filter;
    };

    std::unordered_set<trace_type_t> remove_trace_types_;
    std::unordered_set<trace_marker_type_t> remove_marker_types_;
};

} // namespace drmemtrace
} // namespace dynamorio
#endif /* _TYPE_FILTER_H_ */
