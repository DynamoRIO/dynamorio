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

#include "lfu.h"

#include <algorithm>
#include <vector>
#include <iostream>

namespace dynamorio {
namespace drmemtrace {

lfu_t::lfu_t(int num_blocks, int associativity)
    : cache_replacement_policy_t(num_blocks, associativity)
{
    access_counts_.reserve(num_blocks);
    for (int i = 0; i < num_blocks; ++i) {
        access_counts_.emplace_back(associativity, 0);
    }
}

void
lfu_t::access_update(int block_idx, int way)
{
    block_idx = get_block_index(block_idx);
    access_counts_[block_idx][way]++;
}

void
lfu_t::eviction_update(int block_idx, int way)
{
    block_idx = get_block_index(block_idx);
    access_counts_[block_idx][way] = 0;
}

int
lfu_t::get_next_way_to_replace(int block_idx)
{
    // Find the way with the minimum frequency counter.
    block_idx = get_block_index(block_idx);
    int min_freq = access_counts_[block_idx][0];
    int min_way = 0;
    for (int i = 1; i < associativity_; ++i) {
        if (access_counts_[block_idx][i] < min_freq) {
            min_freq = access_counts_[block_idx][i];
            min_way = i;
        }
    }
    return min_way;
}

std::string
lfu_t::get_name() const
{
    return "LFU";
}

} // namespace drmemtrace
} // namespace dynamorio
