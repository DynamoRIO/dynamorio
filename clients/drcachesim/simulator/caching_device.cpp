/* **********************************************************
 * Copyright (c) 2015-2017 Google, Inc.  All rights reserved.
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

#include "caching_device.h"
#include "caching_device_block.h"
#include "caching_device_stats.h"
#include "prefetcher.h"
#include "../common/utils.h"
#include <assert.h>

caching_device_t::caching_device_t()
    : blocks(NULL)
    , stats(NULL)
    , prefetcher(NULL)
{
    /* Empty. */
}

caching_device_t::~caching_device_t()
{
    if (blocks == NULL)
        return;
    for (int i = 0; i < num_blocks; i++)
        delete blocks[i];
    delete[] blocks;
}

bool
caching_device_t::init(int associativity_, int block_size_, int num_blocks_,
                       caching_device_t *parent_, caching_device_stats_t *stats_,
                       prefetcher_t *prefetcher_, bool inclusive_,
                       const std::vector<caching_device_t *> &children_)
{
    if (!IS_POWER_OF_2(associativity_) || !IS_POWER_OF_2(block_size_) ||
        !IS_POWER_OF_2(num_blocks_) ||
        // Assuming caching device block size is at least 4 bytes
        block_size_ < 4)
        return false;
    if (stats_ == NULL)
        return false; // A stats must be provided for perf: avoid conditional code
    else if (!*stats_)
        return false;
    associativity = associativity_;
    block_size = block_size_;
    num_blocks = num_blocks_;
    loaded_blocks = 0;
    blocks_per_set = num_blocks / associativity;
    assoc_bits = compute_log2(associativity);
    block_size_bits = compute_log2(block_size);
    blocks_per_set_mask = blocks_per_set - 1;
    if (assoc_bits == -1 || block_size_bits == -1 || !IS_POWER_OF_2(blocks_per_set))
        return false;
    parent = parent_;
    stats = stats_;
    prefetcher = prefetcher_;

    blocks = new caching_device_block_t *[num_blocks];
    init_blocks();

    last_tag = TAG_INVALID; // sentinel

    inclusive = inclusive_;
    children = children_;

    return true;
}

void
caching_device_t::request(const memref_t &memref_in)
{
    // Unfortunately we need to make a copy for our loop so we can pass
    // the right data struct to the parent and stats collectors.
    memref_t memref;
    // We support larger sizes to improve the IPC perf.
    // This means that one memref could touch multiple blocks.
    // We treat each block separately for statistics purposes.
    addr_t final_addr = memref_in.data.addr + memref_in.data.size - 1 /*avoid overflow*/;
    addr_t final_tag = compute_tag(final_addr);
    addr_t tag = compute_tag(memref_in.data.addr);

    // Optimization: check last tag if single-block
    if (tag == final_tag && tag == last_tag) {
        // Make sure last_tag is properly in sync.
        assert(tag != TAG_INVALID &&
               tag == get_caching_device_block(last_block_idx, last_way).tag);
        stats->access(memref_in, true /*hit*/);
        if (parent != NULL)
            parent->stats->child_access(memref_in, true);
        access_update(last_block_idx, last_way);
        return;
    }

    memref = memref_in;
    for (; tag <= final_tag; ++tag) {
        int way;
        int block_idx = compute_block_idx(tag);
        bool missed = false;

        if (tag + 1 <= final_tag)
            memref.data.size = ((tag + 1) << block_size_bits) - memref.data.addr;

        for (way = 0; way < associativity; ++way) {
            if (get_caching_device_block(block_idx, way).tag == tag) {
                stats->access(memref, true /*hit*/);
                if (parent != NULL)
                    parent->stats->child_access(memref, true);
                break;
            }
        }

        if (way == associativity) {
            stats->access(memref, false /*miss*/);
            missed = true;
            // If no parent we assume we get the data from main memory
            if (parent != NULL) {
                parent->stats->child_access(memref, false);
                parent->request(memref);
            }

            // FIXME i#1726: coherence policy

            way = replace_which_way(block_idx);
            // Check if we are inserting a new block, if we are then increment
            // the block loaded count.
            if (get_caching_device_block(block_idx, way).tag == TAG_INVALID) {
                loaded_blocks++;
            } else if (inclusive && !children.empty()) {
                for (auto &child : children) {
                    child->invalidate(get_caching_device_block(block_idx, way).tag);
                }
            }
            get_caching_device_block(block_idx, way).tag = tag;
        }

        access_update(block_idx, way);

        // Issue a hardware prefetch, if any, before we remember the last tag,
        // so we remember this line and not the prefetched line.
        if (missed && !type_is_prefetch(memref.data.type) && prefetcher != nullptr)
            prefetcher->prefetch(this, memref);

        if (tag + 1 <= final_tag) {
            addr_t next_addr = (tag + 1) << block_size_bits;
            memref.data.addr = next_addr;
            memref.data.size = final_addr - next_addr + 1 /*undo the -1*/;
        }

        // Optimization: remember last tag
        last_tag = tag;
        last_way = way;
        last_block_idx = block_idx;
    }
}

void
caching_device_t::access_update(int block_idx, int way)
{
    // We just inc the counter for LFU.  We live with any blip on overflow.
    get_caching_device_block(block_idx, way).counter++;
}

int
caching_device_t::replace_which_way(int block_idx)
{
    // The base caching device class only implements LFU.
    // A subclass can override this and access_update() to implement
    // some other scheme.
    int min_counter = 0; /* avoid "may be used uninitialized" with GCC 4.4.7 */
    int min_way = 0;
    for (int way = 0; way < associativity; ++way) {
        if (get_caching_device_block(block_idx, way).tag == TAG_INVALID) {
            min_way = way;
            break;
        }
        if (way == 0 || get_caching_device_block(block_idx, way).counter < min_counter) {
            min_counter = get_caching_device_block(block_idx, way).counter;
            min_way = way;
        }
    }
    // Clear the counter for LFU.
    get_caching_device_block(block_idx, min_way).counter = 0;
    return min_way;
}

void
caching_device_t::invalidate(const addr_t tag)
{
    int block_idx = compute_block_idx(tag);

    for (int way = 0; way < associativity; ++way) {
        auto &cache_block = get_caching_device_block(block_idx, way);
        if (cache_block.tag == tag) {
            cache_block.tag = TAG_INVALID;
            cache_block.counter = 0;
            stats->invalidate();
            // Invalidate last_tag if it was this tag.
            if (last_tag == tag) {
                last_tag = TAG_INVALID;
            }
            // Invalidate the block in the children's caches.
            if (inclusive && !children.empty()) {
                for (auto &child : children) {
                    child->invalidate(tag);
                }
            }
            break;
        }
    }
}
