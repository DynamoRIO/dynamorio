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

#include "policy_lru.h"

#include <algorithm>
#include <string>

#include "cache_replacement_policy.h"

namespace dynamorio {
namespace drmemtrace {

policy_lru_t::policy_lru_t(int num_sets, int associativity)
    : cache_replacement_policy_t(num_sets, associativity)
{
    // Initialize the LRU list for each set.
    lru_counters_.reserve(num_sets);
    for (int i = 0; i < num_sets; ++i) {
        lru_counters_.emplace_back(associativity, 1);
    }
}

void
policy_lru_t::access_update(int set_idx, int way, cache_access_outcome_t access_type)
{
    int count = lru_counters_[set_idx][way];
    // Optimization: return early if it is a repeated access.
    if (count == 0)
        return;
    // We inc all the counters that are not larger than count for LRU.
    for (int i = 0; i < associativity_; ++i) {
        if (i != way && lru_counters_[set_idx][i] <= count)
            lru_counters_[set_idx][i]++;
    }
    // Clear the counter for LRU.
    lru_counters_[set_idx][way] = 0;
}

void
policy_lru_t::eviction_update(int set_idx, int way)
{
    // Nothing to update, when the way is accessed we will update it -
    // If the way was evicted, it is already at the end of the list.
}

void
policy_lru_t::invalidation_update(int set_idx, int way)
{
    int max_counter =
        *std::max_element(lru_counters_[set_idx].begin(), lru_counters_[set_idx].end());
    lru_counters_[set_idx][way] = max_counter + 1;
}

int
policy_lru_t::get_next_way_to_replace(int set_idx) const
{
    // We implement LRU by picking the slot with the largest counter value.
    int max_counter = 0;
    int max_way = 0;
    for (int way = 0; way < associativity_; ++way) {
        if (lru_counters_[set_idx][way] > max_counter) {
            max_counter = lru_counters_[set_idx][way];
            max_way = way;
        }
    }
    return max_way;
}

std::string
policy_lru_t::get_name() const
{
    return "LRU";
}

} // namespace drmemtrace
} // namespace dynamorio
