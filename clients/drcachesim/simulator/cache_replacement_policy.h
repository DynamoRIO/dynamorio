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

#ifndef _CACHE_REPLACEMENT_POLICY_H_
#define _CACHE_REPLACEMENT_POLICY_H_ 1

#include <string>
#include <vector>

namespace dynamorio {
namespace drmemtrace {

/**
 * An interface for cache replacement policies.
 *
 * Holds the necessary information to implement a cache replacement policy,
 * and provides a replacement-specific get_next_way_to_replace() method for
 * `caching_device_t`.
 *
 * The policy receives the following updates:
 *  - When an existing way is accessed, `access_update()` is called.
 *  - When a way is evicted, `eviction_update()` is called on the evicted way, and
 * `access_update()` is called on the new way immediately after. Can be called on invalid
 * ways.
 *  - When a way is invalidated, `invalidation_update()` is called.
 *
 * The policy also provides a `get_next_way_to_replace()` method that returns
 * the next way to replace in the block. This function assumes that all ways are valid,
 * and is called by `caching_device_t` when it cannot just replace an invalid way.
 *
 * Note that the policy receives the set index, not the block index as it is in
 * `caching_device_t`, which is the index of the first way in the set when all ways are
 * stored in a contiguous array. This can be obtained with `compute_set_index()` in
 * caching_device_t.
 */
class cache_replacement_policy_t {
public:
    cache_replacement_policy_t(int num_sets, int associativity)
        : associativity_(associativity)
        , num_sets_(num_sets)
    {
    }
    /// Informs the replacement policy that an access has occurred.
    virtual void
    access_update(int set_idx, int way) = 0;
    /// Informs the replacement policy that an eviction has occurred.
    virtual void
    eviction_update(int set_idx, int way) = 0;
    /// Informs the replacement policy that an invalidation has occurred.
    virtual void
    invalidation_update(int set_idx, int way) = 0;
    /*
     * Returns the next way to replace in the set.
     * Assumes that all ways are valid.
     */
    virtual int
    get_next_way_to_replace(int set_idx) const = 0;
    /// Returns the name of the replacement policy.
    virtual std::string
    get_name() const = 0;

    virtual ~cache_replacement_policy_t() = default;

protected:
    int associativity_;
    int num_sets_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _CACHE_REPLACEMENT_POLICY_H_ */
