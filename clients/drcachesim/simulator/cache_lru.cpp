/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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

// For LRU implementation, we use the cache line counter to represent
// how recently a cache line is accessed.
// The count value 0 means the most recent access, and the cache line with the
// highest counter value will be picked for replacement in replace_which_way.

void
cache_lru_t::access_update(int line_idx, int way)
{
    int cnt = get_caching_device_block(line_idx, way).counter;
    // Optimization: return early if it is a repeated access.
    if (cnt == 0)
        return;
    // We inc all the counters that are not larger than cnt for LRU.
    for (int i = 0; i < associativity; ++i) {
        if (i != way && get_caching_device_block(line_idx, i).counter <= cnt)
            get_caching_device_block(line_idx, i).counter++;
    }
    // Clear the counter for LRU.
    get_caching_device_block(line_idx, way).counter = 0;
}

int
cache_lru_t::replace_which_way(int line_idx)
{
    // We implement LRU by picking the slot with the largest counter value.
    int max_counter = 0;
    int max_way = 0;
    for (int way = 0; way < associativity; ++way) {
        if (get_caching_device_block(line_idx, way).tag == TAG_INVALID) {
            max_way = way;
            break;
        }
        if (get_caching_device_block(line_idx, way).counter > max_counter) {
            max_counter = get_caching_device_block(line_idx, way).counter;
            max_way = way;
        }
    }
    // Set to non-zero for later access_update optimization on repeated access
    get_caching_device_block(line_idx, max_way).counter = 1;
    return max_way;
}
