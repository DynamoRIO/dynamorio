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

#include "caching_device.h"

#include <assert.h>
#include <stddef.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "memref.h"
#include "caching_device_block.h"
#include "caching_device_stats.h"
#include "prefetcher.h"
#include "snoop_filter.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

caching_device_t::caching_device_t(const std::string &name)
    : blocks_(NULL)
    , stats_(NULL)
    , prefetcher_(NULL)
    // The tag being hashed is already right-shifted to the cache line and
    // an identity hash is plenty good enough and nice and fast.
    // We set the size and load factor only if being used, in set_hashtable_use().
    , tag2block(0, [](addr_t key) { return static_cast<unsigned long>(key); })
    , name_(name)
{
}

caching_device_t::~caching_device_t()
{
    if (blocks_ == NULL)
        return;
    for (int i = 0; i < num_blocks_; i++)
        delete blocks_[i];
    delete[] blocks_;
}

bool
caching_device_t::init(int associativity, int block_size, int num_blocks,
                       caching_device_t *parent, caching_device_stats_t *stats,
                       prefetcher_t *prefetcher,
                       cache_inclusion_policy_t inclusion_policy, bool coherent_cache,
                       int id, snoop_filter_t *snoop_filter,
                       const std::vector<caching_device_t *> &children)
{
    // Assume cache has nonzero capacity.
    if (associativity < 1 || num_blocks < 1)
        return false;
    // Assume caching device block size is at least 4 bytes.
    if (!IS_POWER_OF_2(block_size) || block_size < 4)
        return false;
    if (stats == NULL)
        return false; // A stats must be provided for perf: avoid conditional code
    else if (!*stats)
        return false;
    associativity_ = associativity;
    block_size_ = block_size;
    num_blocks_ = num_blocks;
    loaded_blocks_ = 0;
    blocks_per_way_ = num_blocks_ / associativity;
    // Make sure num_blocks_ is evenly divisible by associativity
    if (blocks_per_way_ * associativity_ != num_blocks_)
        return false;
    blocks_per_way_mask_ = blocks_per_way_ - 1;
    block_size_bits_ = compute_log2(block_size);
    // Non-power-of-two associativities and total cache sizes are allowed, so
    // long as the number blocks per cache way is a power of two.
    if (block_size_bits_ == -1 || !IS_POWER_OF_2(blocks_per_way_))
        return false;
    parent_ = parent;
    set_stats(stats);
    prefetcher_ = prefetcher;
    id_ = id;
    snoop_filter_ = snoop_filter;
    coherent_cache_ = coherent_cache;

    blocks_ = new caching_device_block_t *[num_blocks_];
    init_blocks();

    last_tag_ = TAG_INVALID; // sentinel

    inclusion_policy_ = inclusion_policy;
    children_ = children;

    return true;
}

std::string
caching_device_t::get_description() const
{
    // One-line human-readable string describing the cache configuration.
    return "size=" + std::to_string(get_size_bytes()) +
        ", assoc=" + std::to_string(get_associativity()) +
        ", block=" + std::to_string(get_block_size()) + ", " + get_replace_policy() +
        (is_coherent() ? ", coherent" : "") +
        (is_inclusive()       ? ", inclusive"
             : is_exclusive() ? ", exclusive"
                              : "");
}

std::pair<caching_device_block_t *, int>
caching_device_t::find_caching_device_block(addr_t tag)
{
    if (use_tag2block_table_) {
        auto it = tag2block.find(tag);
        if (it == tag2block.end())
            return std::make_pair(nullptr, 0);
        assert(it->second.first->tag_ == tag);
        return it->second;
    }
    int block_idx = compute_block_idx(tag);
    for (int way = 0; way < associativity_; ++way) {
        caching_device_block_t &block = get_caching_device_block(block_idx, way);
        if (block.tag_ == tag)
            return std::make_pair(&block, way);
    }
    return std::make_pair(nullptr, 0);
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
    if (tag == final_tag && tag == last_tag_ && memref_in.data.type != TRACE_TYPE_WRITE) {
        // Make sure last_tag_ is properly in sync.
        caching_device_block_t *cache_block =
            &get_caching_device_block(last_block_idx_, last_way_);
        assert(tag != TAG_INVALID && tag == cache_block->tag_);
        record_access_stats(memref_in, true /*hit*/, cache_block);
        access_update(last_block_idx_, last_way_);
        return;
    }

    memref = memref_in;
    for (; tag <= final_tag; ++tag) {
        int way = associativity_;
        int block_idx = compute_block_idx(tag);
        bool missed = false;

        if (tag + 1 <= final_tag)
            memref.data.size = ((tag + 1) << block_size_bits_) - memref.data.addr;

        auto block_way = find_caching_device_block(tag);
        if (block_way.first != nullptr) {
            // Access is a hit.
            caching_device_block_t *cache_block = block_way.first;
            way = block_way.second;
            record_access_stats(memref, true /*hit*/, cache_block);
            if (coherent_cache_ && memref.data.type == TRACE_TYPE_WRITE) {
                // On a hit, we must notify the snoop filter of the write or propagate
                // the write to a snooped cache.
                if (snoop_filter_ != NULL) {
                    snoop_filter_->snoop(tag, id_,
                                         (memref.data.type == TRACE_TYPE_WRITE));
                } else if (parent_ != NULL) {
                    // On a miss, the parent access will inherently propagate the write.
                    parent_->propagate_write(tag, this);
                }
            }
            // If this is an exclusive cache with children, a hit transfers the
            // line to the child that missed and our copy is no longer needed.
            // It seems pointless to have an exclusive cache without children,
            // but who are we to judge.
            if (is_exclusive() && !children_.empty()) {
                // We don't need to tell the snoop filter about this eviction since
                // we know the child cache contains the evicted line.
                invalidate(tag, INVALIDATION_EXCLUSIVE);
                // Done with this line.
                continue;
            }
        } else {
            // Access is a miss.
            way = replace_which_way(block_idx);
            caching_device_block_t *cache_block =
                &get_caching_device_block(block_idx, way);

            record_access_stats(memref, false /*miss*/, cache_block);
            missed = true;
            // If no parent we assume we get the data from main memory.
            if (parent_ != nullptr) {
                parent_->request(memref);
            }
            // Exclusive caches only insert lines that have been evicted
            // by a child cache.  So a regular miss does nothing more.
            if (is_exclusive()) {
                continue;
            }
            insert_tag(tag, (memref.data.type == TRACE_TYPE_WRITE), way, block_idx);
        }

        access_update(block_idx, way);

        // Issue a hardware prefetch, if any, before we remember the last tag,
        // so we remember this line and not the prefetched line.
        if (missed && !type_is_prefetch(memref.data.type) && prefetcher_ != nullptr)
            prefetcher_->prefetch(this, memref);

        if (tag + 1 <= final_tag) {
            addr_t next_addr = (tag + 1) << block_size_bits_;
            memref.data.addr = next_addr;
            memref.data.size = final_addr - next_addr + 1 /*undo the -1*/;
        }

        // Optimization: remember last tag
        last_tag_ = tag;
        last_way_ = way;
        last_block_idx_ = block_idx;
    }
}

void
caching_device_t::access_update(int block_idx, int way)
{
    // We just inc the counter for LFU.  We live with any blip on overflow.
    get_caching_device_block(block_idx, way).counter_++;
}

int
caching_device_t::replace_which_way(int block_idx)
{
    int min_way = get_next_way_to_replace(block_idx);
    // Clear the counter for LFU.
    get_caching_device_block(block_idx, min_way).counter_ = 0;
    return min_way;
}

int
caching_device_t::get_next_way_to_replace(const int block_idx) const
{
    // The base caching device class only implements LFU.
    // A subclass can override this and access_update() to implement
    // some other scheme.
    int min_counter = 0; /* avoid "may be used uninitialized" with GCC 4.4.7 */
    int min_way = 0;
    for (int way = 0; way < associativity_; ++way) {
        if (get_caching_device_block(block_idx, way).tag_ == TAG_INVALID) {
            min_way = way;
            break;
        }
        if (way == 0 || get_caching_device_block(block_idx, way).counter_ < min_counter) {
            min_counter = get_caching_device_block(block_idx, way).counter_;
            min_way = way;
        }
    }
    return min_way;
}

void
caching_device_t::invalidate(addr_t tag, invalidation_type_t invalidation_type)
{
    auto block_way = find_caching_device_block(tag);
    if (block_way.first != nullptr) {
        invalidate_caching_device_block(block_way.first);
        loaded_blocks_--;
        stats_->invalidate(invalidation_type);
        // Invalidate last_tag_ if it was this tag.
        if (last_tag_ == tag) {
            last_tag_ = TAG_INVALID;
        }
        // Invalidate the block in the children's caches.
        if (invalidation_type == INVALIDATION_INCLUSIVE && is_inclusive() &&
            !children_.empty()) {
            for (auto &child : children_) {
                child->invalidate(tag, invalidation_type);
            }
        }
    }
    // If this is a coherence invalidation, we must invalidate children caches.
    if (invalidation_type == INVALIDATION_COHERENCE && !children_.empty()) {
        for (auto &child : children_) {
            child->invalidate(tag, invalidation_type);
        }
    }
}

// This function checks if this cache or any child caches hold a tag.
bool
caching_device_t::contains_tag(addr_t tag)
{
    auto block_way = find_caching_device_block(tag);
    if (block_way.first != nullptr)
        return true;
    // If there are no children or all their lines are included in
    // this cache, there's nothing more to check.
    if (children_.empty() || is_inclusive()) {
        return false;
    }
    for (auto &child : children_) {
        if (child->contains_tag(tag)) {
            return true;
        }
    }
    return false;
}

// The next two methods propagate writes and evictions as part of the coherence
// logic.  When the cache hierarchy coherence logic is enabled:
//   * A snoop filter is attached to the highest cache hierarchy level that
//     contains multiple caches.
//       * All caches *below* (child) the snooped level are marked "coherent".
//       * All caches *above* (parent) the snooped level are marked
//         "non_coherent".
//    * On HITS, writes get propagated up to the snoop filter for coherent
//        unsnooped caches, via propagate_write().  This is to invalidate
//        any copies of the line in other caches.
//    * On MISSES:
//      * the miss request gets propagated to parent as normal.
//      * if cache is snooped, update snoop filter for the new line.
//      * if the new line evicts an old line:
//        * if cache is inclusive, invalidate evicted line from all children.
//        * if coherent and no children have a copy of the evicted line:
//          * propagate eviction upstream to snoop filter via
//            propagate_eviction().
//        * if parent is exclusive and no children have a copy of the evicted line:
//          * push evicted line to parent via propagate_eviction().

// A child has evicted this tag, we propagate this notification to the snoop filter
// or first exclusive cache, *unless* this cache or one of its other children holds
// this line.
void
caching_device_t::propagate_eviction(addr_t tag, const caching_device_t *requester)
{
    // Check our own cache for this line.  If we find it, we're done.
    auto block_way = find_caching_device_block(tag);
    if (block_way.first != nullptr)
        return;

    // Check if other children contain this line.
    if (children_.size() != 1) {
        // If another child contains the line, we don't need to do anything.
        // We don't need to check the requesting child, because it just
        // evicted the line.
        for (auto &child : children_) {
            if (child != requester && child->contains_tag(tag)) {
                return;
            }
        }
    }

    // If we're exclusive, insert this line and possibly evict something else,
    // ending the eviction propagation for this tag.
    // Snoop info does not need to be propagated further, because
    // we're inserting this tag which means any higher-level snoop filters
    // or inclusive caches see no change.
    if (is_exclusive()) {
        int block_idx = compute_block_idx(tag);
        int way = replace_which_way(block_idx);
        // Insert line and update snoop filter if appropriate.
        insert_tag(tag, /*is_write=*/false, way, block_idx);
        return;
    }

    // Neither this cache nor its children hold the tag, so continue propagating
    // eviction towards the snoop filter.
    if (snoop_filter_ != NULL) {
        snoop_filter_->snoop_eviction(tag, id_);
    } else if (parent_ != NULL) {
        parent_->propagate_eviction(tag, this);
    }
}

/* This function is called by a coherent child performing a write.
 * This cache must forward this write to the snoop filter and invalidate this line
 * in any other children.
 */
void
caching_device_t::propagate_write(addr_t tag, const caching_device_t *requester)
{
    // Invalidate other children.
    for (auto &child : children_) {
        if (child != requester) {
            child->invalidate(tag, INVALIDATION_COHERENCE);
        }
    }

    // Propagate write to snoop filter or parent.
    if (snoop_filter_ != NULL) {
        snoop_filter_->snoop(tag, id_, true);
    } else if (parent_ != NULL) {
        parent_->propagate_write(tag, this);
    }
}

void
caching_device_t::record_access_stats(const memref_t &memref, bool hit,
                                      caching_device_block_t *cache_block)
{
    stats_->access(memref, hit, cache_block);
    // We propagate hits all the way up the hierarchy.
    // But to avoid over-counting we only propagate misses one level up.
    if (hit) {
        for (caching_device_t *up = parent_; up != nullptr; up = up->parent_)
            up->stats_->child_access(memref, hit, cache_block);
    } else if (parent_ != nullptr)
        parent_->stats_->child_access(memref, hit, cache_block);
}

// Inserts a tag into the cache, updating the snoop filter and dealing with
// evictions as needed.
void
caching_device_t::insert_tag(addr_t tag, bool is_write, int way, int block_idx)
{
    caching_device_block_t *cache_block = &get_caching_device_block(block_idx, way);
    if (snoop_filter_ != nullptr) {
        // Update snoop filter to mark tag as present in this cache.
        snoop_filter_->snoop(tag, id_, is_write);
    }
    addr_t victim_tag = cache_block->tag_;
    if (victim_tag == TAG_INVALID) {
        // Lucky for us, nothing needs to be evicted.
        loaded_blocks_++;
    } else {
        // Evict the victim tag.
        // First, if this cache is inclusive we must flush the victim
        // tag from all child caches.
        if (!children_.empty() && is_inclusive()) {
            for (auto &child : children_) {
                child->invalidate(victim_tag, INVALIDATION_INCLUSIVE);
            }
        }
        // Handle parental notifications for coherence and exclusivity.
        bool push_victim_to_parent = parent_ != nullptr && parent_->is_exclusive();
        if (coherent_cache_ || push_victim_to_parent) {
            bool child_holds_tag = false;
            if (!children_.empty()) {
                /* We must check child caches to find out if the snoop filter
                 * should clear the ownership bit for this evicted tag.
                 * If any of this cache's children contain the evicted tag the
                 * snoop filter should still consider this cache an owner.
                 *
                 * Similarly, evicted lines are pushed to a parent exclusive
                 * cache only if no child caches contain the line.  The only
                 * difference from snoop checking is that a single LLC can
                 * be exclusive but cannot be snooped, so the eviction for
                 * exclusive caches may propagate higher.
                 *
                 * This is why we don't tell the snoop filter when a child
                 * cache contains the line:
                 * * Only the highest coherent cache level with >1 cache is snooped.
                 * * Inner coherent caches rely on their snooped (grand)parent
                 *   for snoop cache updates.  So invalidates and writes in a
                 *   coherent cache get propagated up through parents until a
                 *   snooped cache is found, and then the snoop filter is
                 *   updated.
                 *
                 */
                for (auto &child : children_) {
                    if (child->contains_tag(victim_tag)) {
                        child_holds_tag = true;
                        break;
                    }
                }
            }
            if (!child_holds_tag) {
                bool notify_parent_for_snoop = false;
                if (coherent_cache_) {
                    if (snoop_filter_ != nullptr) {
                        // Inform snoop filter of evicted line.
                        snoop_filter_->snoop_eviction(victim_tag, id_);
                    } else {
                        // If there's a parent, keep forwarding the eviction.
                        notify_parent_for_snoop = parent_ != nullptr;
                    }
                }
                if (notify_parent_for_snoop || push_victim_to_parent) {
                    parent_->propagate_eviction(victim_tag, this);
                }
            }
        }
    }
    update_tag(cache_block, way, tag);
}

} // namespace drmemtrace
} // namespace dynamorio
