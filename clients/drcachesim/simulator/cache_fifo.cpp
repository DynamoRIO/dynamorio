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

#include "cache_fifo.h"

#include <assert.h>

#include <vector>

#include "cache.h"
#include "caching_device.h"
#include "caching_device_block.h"
#include "caching_device_stats.h"
#include "prefetcher.h"
#include "snoop_filter.h"

namespace dynamorio {
namespace drmemtrace {

// For the FIFO/Round-Robin implementation, all the cache blocks in a set are organized
// as a FIFO. The counters of a set of blocks simulate the replacement pointer.
// The counter of the victim block is 1, and others are 0.
// While replacing happens, the victim block will be replaced and its counter will
// be cleared. The counter of the next block will be set to 1.

bool
cache_fifo_t::init(int associativity, int block_size, int total_size,
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

    // Create a replacement pointer for each set, and
    // initialize it to point to the first block.
    for (int i = 0; i < blocks_per_way_; i++) {
        get_caching_device_block(i * associativity_, 0).counter_ = 1;
    }
    return true;
}

void
cache_fifo_t::access_update(int block_idx, int way)
{
    // Since the FIFO replacement policy is independent of cache hit,
    // we do not need to do anything here.
    return;
}

// This method replaces the current victim way and returns the next victim way
// to be replaced. As opposed to get_next_way_to_replace() which just returns the
// next way to be replaced without updating the cache state, replace_which_way()
// updates the cache state.
int
cache_fifo_t::replace_which_way(int block_idx)
{
    int victim_way = get_next_way_to_replace(block_idx);
    // clear the counter of the victim block
    get_caching_device_block(block_idx, victim_way).counter_ = 0;
    // set the next block as victim
    get_caching_device_block(block_idx, (victim_way + 1) & (associativity_ - 1))
        .counter_ = 1;
    return victim_way;
}

// This method just returns the next way to be replaced without actually
// replacing it, hence doesn't have a side-effect.
int
cache_fifo_t::get_next_way_to_replace(const int block_idx) const
{
    for (int i = 0; i < associativity_; i++) {
        // We return the block whose counter is 1.
        if (get_caching_device_block(block_idx, i).counter_ == 1) {
            return i;
        }
    }

    assert(false);
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
