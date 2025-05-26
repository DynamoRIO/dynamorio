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

#ifndef _LFU_H_
#define _LFU_H_

#include <vector>

#include "cache_replacement_policy.h"

namespace dynamorio {
namespace drmemtrace {

/**
 * A Least-Frequently-Used (LFU) cache replacement policy.
 *
 * Count all access to each way, the way with the least accesses is evicted.
 */
class policy_lfu_t : public cache_replacement_policy_t {
public:
    policy_lfu_t(int num_sets, int associativity);
    void
    access_update(int set_idx, int way) override;
    void
    eviction_update(int set_idx, int way) override;
    void
    invalidation_update(int set_idx, int way) override;
    int
    get_next_way_to_replace(int set_idx) const override;
    std::string
    get_name() const override;

    ~policy_lfu_t() override = default;

private:
    // Frequency counters for each way in each line.
    std::vector<std::vector<int>> access_counts_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif // _LFU_H_
