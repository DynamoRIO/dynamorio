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

#include "bit_plru.h"

#include <random>
#include <vector>

namespace dynamorio {
namespace drmemtrace {

bit_plru_t::bit_plru_t(int num_blocks, int associativity, int seed)
    : cache_replacement_policy_t(num_blocks, associativity)
    , block_set_counts_(num_blocks, 0)
    , gen_(seed == -1 ? std::random_device()() : seed)
{
    // Initialize the bit vector for each block.
    block_bits_.reserve(num_blocks);
    for (int i = 0; i < num_blocks; ++i) {
        block_bits_.emplace_back(associativity, false);
    }
}

void
bit_plru_t::access_update(int block_idx, int way)
{
    block_idx = get_block_index(block_idx);
    // Set the bit for the accessed way.
    if (!block_bits_[block_idx][way]) {
        block_bits_[block_idx][way] = true;
        block_set_counts_[block_idx]++;
    }
    if (block_set_counts_[block_idx] < associativity_) {
        // Finished.
        return;
    }
    // If all bits are set, reset them.
    for (int i = 0; i < associativity_; ++i) {
        block_bits_[block_idx][i] = false;
    }
    block_set_counts_[block_idx] = 1;
    block_bits_[block_idx][way] = true;
}

void
bit_plru_t::eviction_update(int block_idx, int way)
{
    // Nothing to update, when the way is accessed we will update it.
}

int
bit_plru_t::get_next_way_to_replace(int block_idx)
{
    block_idx = get_block_index(block_idx);
    std::vector<int> unset_bits;
    for (int i = 0; i < associativity_; ++i) {
        if (block_bits_[block_idx][i] == 0) {
            unset_bits.push_back(i);
        }
    }

    if (unset_bits.empty()) {
        // Should not reach here.
        return -1;
    }
    return unset_bits[gen_() % unset_bits.size()];
}

std::string
bit_plru_t::get_name() const
{
    return "BIT_PLRU";
}

} // namespace drmemtrace
} // namespace dynamorio
