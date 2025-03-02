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

#include "fifo.h"

#include <list>

namespace dynamorio {
namespace drmemtrace {

fifo_t::fifo_t(int num_blocks, int associativity)
    : cache_replacement_policy_t(num_blocks, associativity)
{
    // Initialize the FIFO list for each block.
    queues_.reserve(num_blocks);
    for (int i = 0; i < num_blocks; ++i) {
        queues_.push_back(std::list<int>());
        for (int j = 0; j < associativity; ++j) {
            queues_[i].push_back(j);
        }
    }
}

void
fifo_t::access_update(int block_idx, int way)
{
    // Nothing to update, FIFO does not change on access.
}

void
fifo_t::eviction_update(int block_idx, int way)
{
    block_idx = get_block_index(block_idx);
    // Move the evicted way to the back of the queue.
    auto &fifo_block = queues_[block_idx];
    fifo_block.remove(way);
    fifo_block.push_back(way);
}

int
fifo_t::get_next_way_to_replace(int block_idx)
{
    block_idx = get_block_index(block_idx);
    // The next way to replace is at the front of the FIFO list.
    return queues_[block_idx].front();
}

std::string
fifo_t::get_name() const
{
    return "FIFO";
}

} // namespace drmemtrace
} // namespace dynamorio
