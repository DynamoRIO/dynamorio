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

#include "policy_bit_plru.h"

#include <random>
#include <string>
#include <vector>

#include "cache_replacement_policy.h"

namespace dynamorio {
namespace drmemtrace {

policy_bit_plru_t::policy_bit_plru_t(int num_sets, int associativity, int seed)
    : cache_replacement_policy_t(num_sets, associativity)
    , num_ones_(num_sets, 0)
    , gen_(seed == -1 ? std::random_device()() : seed)
{
    // Initialize the bit vector for each set.
    plru_bits_.reserve(num_sets);
    for (int i = 0; i < num_sets; ++i) {
        plru_bits_.emplace_back(associativity, false);
    }
}

void
policy_bit_plru_t::access_update(int set_idx, int way, cache_access_type_t access_type)
{
    // Set the bit for the accessed way.
    if (!plru_bits_[set_idx][way]) {
        plru_bits_[set_idx][way] = true;
        num_ones_[set_idx]++;
    }
    if (num_ones_[set_idx] < associativity_) {
        // Finished.
        return;
    }
    // If all bits are set, reset them.
    for (int i = 0; i < associativity_; ++i) {
        plru_bits_[set_idx][i] = false;
    }
    num_ones_[set_idx] = 1;
    plru_bits_[set_idx][way] = true;
}

void
policy_bit_plru_t::eviction_update(int set_idx, int way)
{
    // Nothing to update, when the way is accessed we will update it.
}

void
policy_bit_plru_t::invalidation_update(int set_idx, int way)
{
    // Nothing to update, when the way is accessed we will update it.
}

int
policy_bit_plru_t::get_next_way_to_replace(int set_idx) const
{
    std::vector<int> unset_bits;
    for (int i = 0; i < associativity_; ++i) {
        if (!plru_bits_[set_idx][i]) {
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
policy_bit_plru_t::get_name() const
{
    return "BIT_PLRU";
}

} // namespace drmemtrace
} // namespace dynamorio
