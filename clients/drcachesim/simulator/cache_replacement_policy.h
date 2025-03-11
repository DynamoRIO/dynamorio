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
 * `access_update()` is called on the new way immediately after.
 *  - When a way is invalidated, `invalidation_update()` is called.
 *  - When a way is made valid, `validation_update()` is called. this can happen
 * when a snooping is used.
 *
 * The policy also provides a `get_next_way_to_replace()` method that returns
 * the next way to replace in the block.
 */
class cache_replacement_policy_t {
public:
    cache_replacement_policy_t(int num_lines, int associativity)
        : associativity_(associativity)
        , num_lines_(num_lines)
    {
    }
    /// Informs the replacement policy that an access has occurred.
    virtual void
    access_update(int block_idx, int way) = 0;
    /// Informs the replacement policy that an eviction has occurred.
    virtual void
    eviction_update(int block_idx, int way) = 0;
    /// Informs the replacement policy that an invalidation has occurred.
    virtual void
    invalidation_update(int block_idx, int way) = 0;
    /// Informs the replacement policy that a way is now valid.
    virtual void
    validation_update(int block_idx, int way) = 0;
    /*
     * Returns the next way to replace in the block.
     * valid_ways is a vector of booleans, where valid_ways[way] is true if the way
     * is currently valid.
     */
    virtual int
    get_next_way_to_replace(int block_idx) const = 0;
    /// Returns the name of the replacement policy.
    virtual std::string
    get_name() const = 0;

    virtual ~cache_replacement_policy_t() = default;

protected:
    virtual int
    get_line_index(int block_idx) const
    {
        // The block index points to the first way in the block, and the ways are stored
        // in a contiguous array, so we divide by the associativity to get the block
        // index.
        return block_idx / associativity_;
    }
    /// Returns the first invalid way in the block, or -1 if all ways are valid.
    virtual int
    get_first_invalid_way(const std::vector<bool> &valid_ways) const
    {
        for (int way = 0; way < associativity_; ++way) {
            if (!valid_ways[way]) {
                return way;
            }
        }
        return -1;
    }
    int associativity_;
    int num_lines_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _CACHE_REPLACEMENT_POLICY_H_ */
