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

/* caching_device: represents a hardware caching device.
 */

#ifndef _CACHING_DEVICE_H_
#define _CACHING_DEVICE_H_ 1

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "caching_device_block.h"
#include "caching_device_stats.h"
#include "memref.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

// Statistics collection is abstracted out into the caching_device_stats_t class.

// Different replacement policies are expected to be implemented by
// subclassing caching_device_t.

// We assume we're only invoked from a single thread of control and do
// not need to synchronize data access.

class snoop_filter_t;
class prefetcher_t;

class caching_device_t {
public:
    explicit caching_device_t(const std::string &name = "caching_device");
    virtual bool
    init(int associativity, int block_size, int num_blocks, caching_device_t *parent,
         caching_device_stats_t *stats, prefetcher_t *prefetcher = nullptr,
         bool inclusive = false, bool coherent_cache = false, int id_ = -1,
         snoop_filter_t *snoop_filter_ = nullptr,
         const std::vector<caching_device_t *> &children = {});
    virtual ~caching_device_t();
    virtual void
    request(const memref_t &memref);
    virtual void
    invalidate(addr_t tag, invalidation_type_t invalidation_type_);
    bool
    contains_tag(addr_t tag);
    void
    propagate_eviction(addr_t tag, const caching_device_t *requester);
    void
    propagate_write(addr_t tag, const caching_device_t *requester);

    caching_device_stats_t *
    get_stats() const
    {
        return stats_;
    }
    void
    set_stats(caching_device_stats_t *stats)
    {
        stats_ = stats;
        if (stats != nullptr) {
            stats->set_caching_device(this);
        }
    }
    prefetcher_t *
    get_prefetcher() const
    {
        return prefetcher_;
    }
    caching_device_t *
    get_parent() const
    {
        return parent_;
    }
    inline double
    get_loaded_fraction() const
    {
        return double(loaded_blocks_) / num_blocks_;
    }
    // Must be called prior to any call to request().
    virtual inline void
    set_hashtable_use(bool use_hashtable)
    {
        if (!use_tag2block_table_ && use_hashtable) {
            // Resizing from an initial small table causes noticeable overhead, so we
            // start with a relatively large table.
            tag2block.reserve(1 << 16);
            // Even with the large initial size, for large caches we want to keep the
            // load factor small.
            tag2block.max_load_factor(0.5);
        }
        use_tag2block_table_ = use_hashtable;
    }
    int
    get_block_index(const addr_t addr) const
    {
        addr_t tag = compute_tag(addr);
        int block_idx = compute_block_idx(tag);
        return block_idx;
    }

    // Accessors for cache parameters.
    virtual int
    get_associativity() const
    {
        return associativity_;
    }
    virtual int
    get_block_size() const
    {
        return block_size_;
    }
    virtual int
    get_num_blocks() const
    {
        return num_blocks_;
    }
    virtual bool
    is_inclusive() const
    {
        return inclusive_;
    }
    virtual bool
    is_coherent() const
    {
        return coherent_cache_;
    }
    virtual int
    get_size_bytes() const
    {
        return num_blocks_ * block_size_;
    }
    virtual std::string
    get_replace_policy() const
    {
        return "LFU";
    }
    virtual const std::string &
    get_name() const
    {
        return name_;
    }
    // Return a one-line string describing the cache configuration.
    virtual std::string
    get_description() const;

protected:
    virtual void
    access_update(int block_idx, int way);
    virtual int
    replace_which_way(int block_idx);
    virtual int
    get_next_way_to_replace(const int block_idx) const;
    virtual void
    record_access_stats(const memref_t &memref, bool hit,
                        caching_device_block_t *cache_block);

    inline addr_t
    compute_tag(addr_t addr) const
    {
        return addr >> block_size_bits_;
    }
    inline int
    compute_block_idx(addr_t tag) const
    {
        return (tag & blocks_per_way_mask_) * associativity_;
    }
    inline caching_device_block_t &
    get_caching_device_block(int block_idx, int way) const
    {
        return *(blocks_[block_idx + way]);
    }

    inline void
    invalidate_caching_device_block(caching_device_block_t *block)
    {
        if (use_tag2block_table_)
            tag2block.erase(block->tag_);
        block->tag_ = TAG_INVALID;
    }

    inline void
    update_tag(caching_device_block_t *block, int way, addr_t new_tag)
    {
        if (use_tag2block_table_) {
            if (block->tag_ != TAG_INVALID)
                tag2block.erase(block->tag_);
            tag2block[new_tag] = std::make_pair(block, way);
        }
        block->tag_ = new_tag;
    }

    // Returns the block (and its way) whose tag equals `tag`.
    // Returns <nullptr,0> if there is no such block.
    std::pair<caching_device_block_t *, int>
    find_caching_device_block(addr_t tag);

    // a pure virtual function for subclasses to initialize their own block array
    virtual void
    init_blocks() = 0;

    int associativity_;
    int block_size_; // Also known as line length.
    int num_blocks_; // Total number of lines in cache = size / block_size.
    bool coherent_cache_;
    // This is an index into snoop filter's array of caches.
    int id_;

    // Current valid blocks in the cache
    int loaded_blocks_;

    // Pointers to the caching device's parent and children devices.
    caching_device_t *parent_;
    std::vector<caching_device_t *> children_;

    snoop_filter_t *snoop_filter_;

    // If true, this device is inclusive of its children.
    bool inclusive_;

    // This should be an array of caching_device_block_t pointers, otherwise
    // an extended block class which has its own member variables cannot be indexed
    // correctly by base class pointers.
    caching_device_block_t **blocks_;
    int blocks_per_way_;
    // Optimization fields for fast bit operations
    int blocks_per_way_mask_;
    int block_size_bits_;

    caching_device_stats_t *stats_;
    prefetcher_t *prefetcher_;

    // Optimization: remember last tag
    addr_t last_tag_;
    int last_way_;
    int last_block_idx_;
    // Optimization: keep a hashtable for quick lookup of {block,way}
    // given a tag, if using a large cache hierarchy where serial
    // walks over the associativity end up as bottlenecks.
    // We can't easily remove the blocks_ array and replace with just
    // the hashtable as replace_which_way(), etc. want quick access to
    // every way for a given line index.
    std::unordered_map<addr_t, std::pair<caching_device_block_t *, int>,
                       std::function<unsigned long(addr_t)>>
        tag2block;
    bool use_tag2block_table_ = false;

    // Name for this cache.
    const std::string name_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _CACHING_DEVICE_H_ */
