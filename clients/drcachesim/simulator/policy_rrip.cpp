/* **********************************************************
 * Copyright (c) 2015-2025 Google, Inc.  All rights reserved.
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

#include "policy_rrip.h"

#include <algorithm>
#include <string>

#include "cache_replacement_policy.h"

namespace dynamorio {
namespace drmemtrace {

policy_rrip_t::policy_rrip_t(int num_sets, int associativity)
    : cache_replacement_policy_t(num_sets, associativity)
    , rrpv_bits_(3)
    , rrpv_period_(64)
    , rrpv_long_per_period_(1)
{
    rrpv_distant_ = (1 << rrpv_bits_) - 1;
    rrpv_long_ = (1 << rrpv_bits_) - 2;

    // Initialize the RRPV list for each set with "distant" value.
    rrpv_.reserve(num_sets);
    for (int i = 0; i < num_sets; ++i) {
        rrpv_.emplace_back(associativity, rrpv_distant_);
    }

    // Initialize sequence of RRPV values for cache misses: "long" vs. "distant"
    rrpv_miss_val_.reserve(rrpv_period_);
    size_t long_count = 0;
    for (size_t i = 0; i < rrpv_period_; ++i) {
        if ((i + 1) * rrpv_long_per_period_ > long_count * rrpv_period_) {
            rrpv_miss_val_[i] = rrpv_long_;
            ++long_count;
        } else {
            rrpv_miss_val_[i] = rrpv_distant_;
        }
    }
    rrpv_count_within_period_ = 0;
}

void
policy_rrip_t::access_update(int set_idx, int way, bool is_hit)
{
    // Cache hit: set counter to 0
    // Cache miss: set block counter to "long" or "distant" with specified frequency
    rrpv_[set_idx][way] = is_hit ? 0 : increment_n_get_miss_rrpv();
}

void
policy_rrip_t::eviction_update(int set_idx, int way)
{
    // Following the replacement policy, only cache block with "distant" RRPV can be
    // replaced. If there are no "distant" block, RRPV for all ways should be incremented
    // d_rrpv times, Where:
    //  d_RRPV = RRPV_distant - max_over_set(RRPV), And
    //  max_over_set(RRPV) = rrpv_[set_idx][way]
    int d_rrpv = rrpv_distant_ - rrpv_[set_idx][way];
    if (d_rrpv > 0) {
        for (int w = 0; w < associativity_; ++w) {
            rrpv_[set_idx][w] += d_rrpv;
        }
    }
}

void
policy_rrip_t::invalidation_update(int set_idx, int way)
{
    rrpv_[set_idx][way] = rrpv_distant_;
}

int
policy_rrip_t::get_next_way_to_replace(int set_idx) const
{
    // Following the replacement policy, only cache block with "distant" RRPV can be
    // replaced. If there are no "distant" block, RRPV for all ways should be incremented
    // d_RRPV times, Where:
    //  d_RRPV = RRPV_distant - max_over_set(RRPV)
    // Here let's find the first block with "distant" or max RRPV
    // RRPV will be updated in `eviction_update` function.
    int ret_way = 0;
    rrpv_t rrpv_max = 0;
    for (int way = 0; way < associativity_; ++way) {
        if (rrpv_[set_idx][way] == rrpv_distant_) {
            // Found "distant" RRPV. No need to continue search
            return way;
        }
        if (rrpv_[set_idx][way] > rrpv_max) {
            ret_way = way;
        }
    }
    return ret_way;
}

std::string
policy_rrip_t::get_name() const
{
    return "RRIP";
}

} // namespace drmemtrace
} // namespace dynamorio
