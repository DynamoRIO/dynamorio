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

#include "cache_filter.h"

#include <string>

#include "cache_lru.h"
#include "memref.h"
#include "memtrace_stream.h"
#include "cache_stats.h"
#include "caching_device_block.h"
#include "caching_device_stats.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

class cache_filter_stats_t : public cache_stats_t {
public:
    cache_filter_stats_t(int block_size)
        : cache_stats_t(block_size)
        , did_last_access_hit_(false)
    {
    }
    void
    access(const memref_t &memref, bool hit, caching_device_block_t *cache_block) override
    {
        did_last_access_hit_ = hit;
        cache_stats_t::access(memref, hit, cache_block);
    }
    // Returns whether the last access to the cache was a hit.
    bool
    did_last_access_hit()
    {
        return did_last_access_hit_;
    }

private:
    bool did_last_access_hit_;
};

struct per_shard_t {
    cache_lru_t cache;
};

void *
cache_filter_t::parallel_shard_init(memtrace_stream_t *shard_stream,
                                    bool partial_trace_filter)
{
    per_shard_t *per_shard = new per_shard_t;
    if (!(per_shard->cache.init(cache_associativity_, cache_line_size_, cache_size_,
                                nullptr, new cache_filter_stats_t(cache_line_size_),
                                nullptr))) {
        error_string_ = "Failed to initialize cache.";
        return nullptr;
    }
    return per_shard;
}
bool
cache_filter_t::parallel_shard_filter(trace_entry_t &entry, void *shard_data)
{
    if (entry.type == TRACE_TYPE_MARKER && entry.size == TRACE_MARKER_TYPE_FILETYPE) {
        if (filter_instrs_)
            entry.addr |= OFFLINE_FILE_TYPE_IFILTERED;
        if (filter_data_)
            entry.addr |= OFFLINE_FILE_TYPE_DFILTERED;
        return true;
    }
    per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);
    bool output = true;
    // We don't process flush entries here.
    if ((filter_data_ &&
         (entry.type == TRACE_TYPE_READ || entry.type == TRACE_TYPE_WRITE ||
          type_is_prefetch(static_cast<trace_type_t>(entry.type)))) ||
        (filter_instrs_ && type_is_instr(static_cast<trace_type_t>(entry.type)))) {
        memref_t ref;
        ref.data.type = static_cast<trace_type_t>(entry.type);
        ref.data.size = entry.size;
        ref.data.addr = entry.addr;
        per_shard->cache.request(ref);
        output = !reinterpret_cast<cache_filter_stats_t *>(per_shard->cache.get_stats())
                      ->did_last_access_hit();
    }
    return output;
}
bool
cache_filter_t::parallel_shard_exit(void *shard_data)
{
    per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);
    delete per_shard->cache.get_stats();
    delete per_shard;
    return true;
}
} // namespace drmemtrace
} // namespace dynamorio
