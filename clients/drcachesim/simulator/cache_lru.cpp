/* **********************************************************
 * Copyright (c) 2015-2023 Google, Inc.  All rights reserved.
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

#include "cache_lru.h"

#include <vector>

#include "cache.h"
#include "caching_device.h"
#include "caching_device_block.h"
#include "caching_device_stats.h"
#include "prefetcher.h"
#include "snoop_filter.h"

namespace dynamorio {
namespace drmemtrace {

// For LRU implementation, we use the cache line counter to represent
// how recently a cache line is accessed.
// The count value 0 means the most recent access, and the cache line with the
// highest counter value will be picked for replacement in replace_which_way.

bool
cache_lru_t::init(int associativity, int block_size, int total_size,
                  caching_device_t *parent, caching_device_stats_t *stats,
                  prefetcher_t *prefetcher, bool inclusive, bool coherent_cache, int id,
                  snoop_filter_t *snoop_filter,
                  const std::vector<caching_device_t *> &children)
{
    // Works in the same way as the base class,
    // except that the counters are initialized in a different way.

    bool ret_val =
        cache_t::init(associativity, block_size, total_size, parent, stats, prefetcher,
                      inclusive, coherent_cache, id, snoop_filter, children);
    if (ret_val == false)
        return false;

    // Initialize line counters with 0, 1, 2, ..., associativity - 1.
    for (int i = 0; i < blocks_per_way_; i++) {
        for (int way = 0; way < associativity_; ++way) {
            get_caching_device_block(i * associativity_, way).counter_ = way;
        }
    }
    return true;
}

void
cache_lru_t::access_update(int block_idx, int way)
{
    int cnt = get_caching_device_block(block_idx, way).counter_;
    // Optimization: return early if it is a repeated access.
    if (cnt == 0)
        return;
    // We inc all the counters that are not larger than cnt for LRU.
    for (int i = 0; i < associativity_; ++i) {
        if (i != way && get_caching_device_block(block_idx, i).counter_ <= cnt)
            get_caching_device_block(block_idx, i).counter_++;
    }
    // Clear the counter for LRU.
    get_caching_device_block(block_idx, way).counter_ = 0;
}

int
cache_lru_t::replace_which_way(int block_idx)
{
    return get_next_way_to_replace(block_idx);
}

int
cache_lru_t::get_next_way_to_replace(int block_idx) const
{
    // We implement LRU by picking the slot with the largest counter value.
    int max_counter = 0;
    int max_way = 0;
    for (int way = 0; way < associativity_; ++way) {
        if (get_caching_device_block(block_idx, way).tag_ == TAG_INVALID) {
            max_way = way;
            break;
        }
        if (get_caching_device_block(block_idx, way).counter_ > max_counter) {
            max_counter = get_caching_device_block(block_idx, way).counter_;
            max_way = way;
        }
    }
    return max_way;
}

} // namespace drmemtrace
} // namespace dynamorio
