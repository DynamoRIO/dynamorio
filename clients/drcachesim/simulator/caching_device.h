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

/* caching_device: represents a hardware caching device.
 */

#ifndef _CACHING_DEVICE_H_
#define _CACHING_DEVICE_H_ 1

#include <vector>

#include "caching_device_block.h"
#include "caching_device_stats.h"
#include "memref.h"
#include "prefetcher.h"

// Statistics collection is abstracted out into the caching_device_stats_t class.

// Different replacement policies are expected to be implemented by
// subclassing caching_device_t.

// We assume we're only invoked from a single thread of control and do
// not need to synchronize data access.

class caching_device_t {
public:
    caching_device_t();
    virtual bool
    init(int associativity, int block_size, int num_blocks, caching_device_t *parent,
         caching_device_stats_t *stats, prefetcher_t *prefetcher = nullptr,
         bool inclusive = false, const std::vector<caching_device_t *> &children = {});
    virtual ~caching_device_t();
    virtual void
    request(const memref_t &memref);
    virtual void
    invalidate(const addr_t tag);

    caching_device_stats_t *
    get_stats() const
    {
        return stats;
    }
    void
    set_stats(caching_device_stats_t *stats_)
    {
        stats = stats_;
    }
    prefetcher_t *
    get_prefetcher() const
    {
        return prefetcher;
    }
    caching_device_t *
    get_parent() const
    {
        return parent;
    }
    inline double
    get_loaded_fraction() const
    {
        return double(loaded_blocks) / num_blocks;
    }

protected:
    virtual void
    access_update(int block_idx, int way);
    virtual int
    replace_which_way(int block_idx);

    inline addr_t
    compute_tag(addr_t addr)
    {
        return addr >> block_size_bits;
    }
    inline int
    compute_block_idx(addr_t tag)
    {
        return (tag & blocks_per_set_mask) << assoc_bits;
    }
    inline caching_device_block_t &
    get_caching_device_block(int block_idx, int way)
    {
        return *(blocks[block_idx + way]);
    }
    // a pure virtual function for subclasses to initialize their own block array
    virtual void
    init_blocks() = 0;

    int associativity;
    int block_size;
    int num_blocks;
    // Current valid blocks in the cache
    int loaded_blocks;

    // Pointers to the caching device's parent and children devices.
    caching_device_t *parent;
    std::vector<caching_device_t *> children;

    // If true, this device is inclusive of its children.
    bool inclusive;

    // This should be an array of caching_device_block_t pointers, otherwise
    // an extended block class which has its own member variables cannot be indexed
    // correctly by base class pointers.
    caching_device_block_t **blocks;
    int blocks_per_set;
    // Optimization fields for fast bit operations
    int blocks_per_set_mask;
    int assoc_bits;
    int block_size_bits;

    caching_device_stats_t *stats;
    prefetcher_t *prefetcher;

    // Optimization: remember last tag
    addr_t last_tag;
    int last_way;
    int last_block_idx;
};

#endif /* _CACHING_DEVICE_H_ */
