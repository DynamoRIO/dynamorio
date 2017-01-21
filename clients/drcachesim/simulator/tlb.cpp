/* **********************************************************
 * Copyright (c) 2015-2016 Google, Inc.  All rights reserved.
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
#include "../common/utils.h"
#include <assert.h>

void
tlb_t::init_blocks()
{
    for (int i = 0; i < num_blocks; i++) {
        blocks[i] = new tlb_entry_t;
    }
}

void
tlb_t::request(const memref_t &memref_in)
{
    // XXX: any better way to derive caching_device_t::request?
    // Since pid is needed in a lot of places from the beginning to the end,
    // it might also not be a good way to write a lot of helper functions
    // to isolate them.

    // Unfortunately we need to make a copy for our loop so we can pass
    // the right data struct to the parent and stats collectors.
    memref_t memref;
    // We support larger sizes to improve the IPC perf.
    // This means that one memref could touch multiple blocks.
    // We treat each block separately for statistics purposes.
    addr_t final_addr = memref_in.data.addr + memref_in.data.size - 1/*avoid overflow*/;
    addr_t final_tag = compute_tag(final_addr);
    addr_t tag = compute_tag(memref_in.data.addr);
    memref_pid_t pid = memref_in.data.pid;

    // Optimization: check last tag and pid if single-block
    if (tag == final_tag && tag == last_tag && pid == last_pid) {
        // Make sure last_tag and pid are properly in sync.
        assert(tag != TAG_INVALID &&
               tag == get_caching_device_block(last_block_idx, last_way).tag &&
               pid == ((tlb_entry_t &)get_caching_device_block(
                       last_block_idx, last_way)).pid);
        stats->access(memref_in, true/*hit*/);
        if (parent != NULL)
            parent->get_stats()->child_access(memref_in, true);
        access_update(last_block_idx, last_way);
        return;
    }

    memref = memref_in;
    for (; tag <= final_tag; ++tag) {
        int way;
        int block_idx = compute_block_idx(tag);

        if (tag + 1 <= final_tag)
            memref.data.size = ((tag + 1) << block_size_bits) - memref.data.addr;

        for (way = 0; way < associativity; ++way) {
            if (get_caching_device_block(block_idx, way).tag == tag &&
                ((tlb_entry_t &)get_caching_device_block(block_idx, way)).pid == pid) {
                stats->access(memref, true/*hit*/);
                if (parent != NULL)
                    parent->get_stats()->child_access(memref, true);
                break;
            }
        }

        if (way == associativity) {
            stats->access(memref, false/*miss*/);
            // If no parent we assume we get the data from main memory
            if (parent != NULL) {
                parent->get_stats()->child_access(memref, false);
                parent->request(memref);
            }

            // XXX: do we need to handle TLB coherency?

            way = replace_which_way(block_idx);
            get_caching_device_block(block_idx, way).tag = tag;
            ((tlb_entry_t &)get_caching_device_block(block_idx, way)).pid = pid;
        }

        access_update(block_idx, way);

        if (tag + 1 <= final_tag) {
            addr_t next_addr = (tag + 1) << block_size_bits;
            memref.data.addr = next_addr;
            memref.data.size = final_addr - next_addr + 1/*undo the -1*/;
        }
        // Optimization: remember last tag and pid
        last_tag = tag;
        last_way = way;
        last_block_idx = block_idx;
        last_pid = pid;
    }
}
