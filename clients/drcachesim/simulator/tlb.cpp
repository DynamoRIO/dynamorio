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

#include "tlb.h"

#include <assert.h>
#include <stddef.h>

#include "memref.h"
#include "caching_device.h"
#include "caching_device_block.h"
#include "tlb_entry.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

void
tlb_t::init_blocks()
{
    for (int i = 0; i < num_blocks_; i++) {
        blocks_[i] = new tlb_entry_t;
    }
}

void
tlb_t::request(const memref_t &memref_in)
{
    // XXX: any better way to derive caching_device_t::request?
    // Since pid is needed in a lot of places from the beginning to the end,
    // it might also not be a good way to write a lot of helper functions
    // to isolate them.
    // TODO i#4816: This tag,pid pair lookup needs to be imposed on the parent
    // methods invalidate(), contains_tag(), and propagate_eviction() by overriding
    // them.

    // Unfortunately we need to make a copy for our loop so we can pass
    // the right data struct to the parent and stats collectors.
    memref_t memref;
    // We support larger sizes to improve the IPC perf.
    // This means that one memref could touch multiple blocks.
    // We treat each block separately for statistics purposes.
    addr_t final_addr = memref_in.data.addr + memref_in.data.size - 1 /*avoid overflow*/;
    addr_t final_tag = compute_tag(final_addr);
    addr_t tag = compute_tag(memref_in.data.addr);
    memref_pid_t pid = memref_in.data.pid;

    // Optimization: check last tag and pid if single-block
    if (tag == final_tag && tag == last_tag_ && pid == last_pid_) {
        // Make sure last_tag_ and pid are properly in sync.
        caching_device_block_t *tlb_entry =
            &get_caching_device_block(last_block_idx_, last_way_);
        assert(tag != TAG_INVALID && tag == tlb_entry->tag_ &&
               pid == ((tlb_entry_t *)tlb_entry)->pid_);
        record_access_stats(memref_in, true /*hit*/, tlb_entry);
        access_update(last_block_idx_, last_way_);
        return;
    }

    memref = memref_in;
    for (; tag <= final_tag; ++tag) {
        int way;
        int block_idx = compute_block_idx(tag);

        if (tag + 1 <= final_tag)
            memref.data.size = ((tag + 1) << block_size_bits_) - memref.data.addr;

        for (way = 0; way < associativity_; ++way) {
            caching_device_block_t *tlb_entry = &get_caching_device_block(block_idx, way);
            if (tlb_entry->tag_ == tag && ((tlb_entry_t *)tlb_entry)->pid_ == pid) {
                record_access_stats(memref, true /*hit*/, tlb_entry);
                break;
            }
        }

        if (way == associativity_) {
            way = replace_which_way(block_idx);
            caching_device_block_t *tlb_entry = &get_caching_device_block(block_idx, way);

            record_access_stats(memref, false /*miss*/, tlb_entry);
            // If no parent we assume we get the data from main memory
            if (parent_ != NULL)
                parent_->request(memref);

            // XXX: do we need to handle TLB coherency?

            tlb_entry->tag_ = tag;
            ((tlb_entry_t *)tlb_entry)->pid_ = pid;
        }

        access_update(block_idx, way);

        if (tag + 1 <= final_tag) {
            addr_t next_addr = (tag + 1) << block_size_bits_;
            memref.data.addr = next_addr;
            memref.data.size = final_addr - next_addr + 1 /*undo the -1*/;
        }
        // Optimization: remember last tag and pid
        last_tag_ = tag;
        last_way_ = way;
        last_block_idx_ = block_idx;
        last_pid_ = pid;
    }
}

} // namespace drmemtrace
} // namespace dynamorio
