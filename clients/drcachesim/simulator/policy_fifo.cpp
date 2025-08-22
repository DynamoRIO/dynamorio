#/* **********************************************************
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

#include "policy_fifo.h"

#include <list>
#include <string>

#include "cache_replacement_policy.h"

namespace dynamorio {
namespace drmemtrace {

policy_fifo_t::policy_fifo_t(int num_sets, int associativity)
    : cache_replacement_policy_t(num_sets, associativity)
{
    // Initialize the FIFO list for each set.
    queues_.reserve(num_sets);
    for (int i = 0; i < num_sets; ++i) {
        queues_.push_back(std::list<int>());
        for (int j = 0; j < associativity; ++j) {
            queues_[i].push_back(j);
        }
    }
}

void
policy_fifo_t::access_update(int set_idx, int way, cache_access_type_t access_type)
{
    // Nothing to update, FIFO does not change on access.
}

void
policy_fifo_t::eviction_update(int set_idx, int way)
{
    // Move the evicted way to the back of the queue.
    auto &fifo_set = queues_[set_idx];
    fifo_set.remove(way);
    fifo_set.push_back(way);
}

void
policy_fifo_t::invalidation_update(int set_idx, int way)
{
    // Nothing to update, FIFO does not change on invalidation.
}

int
policy_fifo_t::get_next_way_to_replace(int set_idx) const
{
    // The next way to replace is at the front of the FIFO list.
    return queues_[set_idx].front();
}

std::string
policy_fifo_t::get_name() const
{
    return "FIFO";
}

} // namespace drmemtrace
} // namespace dynamorio
