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

#ifndef _RRIP_H_
#define _RRIP_H_

#include <cassert>
#include <list>
#include <string>
#include <vector>

#include "cache_replacement_policy.h"

namespace dynamorio {
namespace drmemtrace {

// Default values for RRIP-based cache replacement policy
const size_t RRPV_BITS_DEFAULT = 3; // Bits to store RRPV for a cache block
// The following two parameter defines the default frequency of "long" and "distant" RRPV
// for cache misses. The default frequency of "long" values is: use the value "long"
// for RRPV_LONG_PER_PERIOD_DEFAULT of each RRPV_PERIOD_DEFAULT cache misses.
const size_t RRPV_PERIOD_DEFAULT = 64;
const size_t RRPV_LONG_PER_PERIOD_DEFAULT = 1;

/**
 * Re-Reference Interval Prediction (RRIP)-based cache replacement policy.
 *
 * Model RRIP and Not Recently Used (NRU) replacement policy.
 * Replacement policy behavior:
 * - Static RRIP (always use "long" rrpv=rrpv_max-1):
 *      rrpv_long_per_period=0
 *      rrpv_period=0
 * - Bi-modal RRIP (use "long" rrpv=rrpv_max-1 with frequency m/n; "distant" otherwise):
 *      rrpv_long_per_period=m
 *      rrpv_period=n
 * - NRU (1-bit RRPV, always use "distant" rrpv==1):
 *      rrpv_bits=1
 *      rrpv_period=1
 *      rrpv_long_per_period=0
 *
 * Currently these values are hardcoded:
 *   rrpv_bits=3
 *   rrpv_period=64
 *   rrpv_long_per_period=1
 * TODO i#7590: Read them from cache configuration file
 */
class policy_rrip_t : public cache_replacement_policy_t {
public:
    policy_rrip_t(int num_sets, int associativity, size_t rrpv_bits = RRPV_BITS_DEFAULT,
                  size_t rrpv_period = RRPV_PERIOD_DEFAULT,
                  size_t rrpv_long_per_period = RRPV_LONG_PER_PERIOD_DEFAULT);
    void
    access_update(int set_idx, int way, cache_access_outcome_t access_type) override;
    void
    eviction_update(int set_idx, int way) override;
    void
    invalidation_update(int set_idx, int way) override;
    int
    get_next_way_to_replace(int set_idx) const override;
    std::string
    get_name() const override;

    ~policy_rrip_t() override = default;

private:
    typedef unsigned rrpv_t;

    std::vector<std::vector<rrpv_t>> rrpv_;

    // How many bits are used for re-reference reuse interval
    // With the value of 1 RRIP cache is equal to NRU (Not Recently Used)
    const size_t rrpv_bits_;
    // Most new cache lines are inserted with rrpv_distant_ and few
    // (rrpv_long_per_period of rrpv_period) lines are inserted with rrpv_long_.
    const rrpv_t rrpv_distant_; // "Distant" RRPV equals to maxRRPV = 2**rrpv_bits_ - 1
    const rrpv_t rrpv_long_;    // "Long" RRPV equals to maxRRPV-1

    // Frequency of "long" RRPV is rrpv_long_per_period_ / rrpv_period_
    size_t rrpv_period_;
    size_t rrpv_long_per_period_;
    // The following values used to switch between "distant" and "long" at cache miss
    std::vector<rrpv_t> rrpv_seed_val_at_miss_;
    size_t at_rrpv_seed_idx_;
    // The next RRPV for cache miss
    inline rrpv_t
    increment_n_get_miss_rrpv()
    {
        if (rrpv_long_per_period_ >= rrpv_period_)
            return rrpv_long_;
        if (rrpv_long_per_period_ == 0)
            return rrpv_distant_;
        if (at_rrpv_seed_idx_ >= rrpv_period_)
            at_rrpv_seed_idx_ = 0;
        return rrpv_seed_val_at_miss_[at_rrpv_seed_idx_++];
    }
};

} // namespace drmemtrace
} // namespace dynamorio

#endif // _RRIP_H_
