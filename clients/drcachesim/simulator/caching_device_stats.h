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

/* caching_device_stats: represents a hardware caching device.
 */

#ifndef _CACHING_DEVICE_STATS_H_
#define _CACHING_DEVICE_STATS_H_ 1

#include <stdint.h>
#ifdef HAS_ZLIB
#    include <zlib.h>
#endif

#include <iterator>
#include <limits>
#include <map>
#include <string>
#include <utility>

#include "caching_device_block.h"
#include "trace_entry.h"
#include "utils.h"
#include "memref.h"

namespace dynamorio {
namespace drmemtrace {

enum invalidation_type_t {
    INVALIDATION_INCLUSIVE,
    INVALIDATION_COHERENCE,
};

enum class metric_name_t {
    HITS,
    MISSES,
    HITS_AT_RESET,
    MISSES_AT_RESET,
    COMPULSORY_MISSES,
    CHILD_HITS,
    CHILD_HITS_AT_RESET,
    INCLUSIVE_INVALIDATES,
    COHERENCE_INVALIDATES,
    PREFETCH_HITS,
    PREFETCH_MISSES,
    FLUSHES
};

struct bound {
    addr_t beg;
    addr_t end;
};

class access_count_t {
public:
    access_count_t(int block_size)
        : block_size_(block_size)
    {
        if (!IS_POWER_OF_2(block_size)) {
            ERRMSG("Block size should be a power of 2.");
            return;
        }
        int block_size_bits = compute_log2(block_size);
        block_size_mask_ = ~((1 << block_size_bits) - 1);
    }

    // Takes non-aligned address and inserts bound consisting of the nearest multiples
    // of the block_size.
    void
    insert(addr_t addr_beg, std::map<addr_t, addr_t>::iterator next_it)
    {
        // Round the address down to the nearest multiple of the block_size.
        addr_beg &= block_size_mask_;
        addr_t addr_end = addr_beg + block_size_;

        // Detect the overflow and assign maximum possible value to the addr_end.
        if (addr_beg > addr_end) {
            addr_end = std::numeric_limits<addr_t>::max();
        }

        std::map<addr_t, addr_t>::reverse_iterator prev_it(next_it);

        // Current bound -> (addr_beg...addr_end) connects previous and
        // next bound
        if (prev_it != bounds.rend() && prev_it->second == addr_beg &&
            next_it != bounds.end() && next_it->first == addr_end) {
            prev_it->second = next_it->second;
            bounds.erase(next_it);
            // Current bound extends previous bound
        } else if (prev_it != bounds.rend() && prev_it->second == addr_beg) {
            prev_it->second = addr_end;
            // Current bound extends next bound
        } else if (next_it != bounds.end() && next_it->first == addr_end) {
            addr_t bound_end = next_it->second;
            // We need to reinsert the element when changing key value.
            // Iterator hint should provide costant complexity of this operation.
            bounds.erase(next_it++);
            bounds.emplace_hint(next_it, addr_beg, bound_end);
        } else {
            bounds.emplace_hint(next_it, addr_beg, addr_end);
        }
    }

    // Takes non-aligned address. Returns:
    // - boolean value indicating whether the address has ever been accessed
    // - iterator to the bound where the address is located or the element which should
    //   be provided as a hint when inserting new bound with given address.
    std::pair<bool, std::map<addr_t, addr_t>::iterator>
    lookup(addr_t addr)
    {
        // Function upper_bound returns bound which beginning is larger
        // then the addr.
        auto next_it = bounds.upper_bound(addr);
        std::map<addr_t, addr_t>::reverse_iterator prev_it(next_it);

        if (prev_it != bounds.rend() && addr >= prev_it->first &&
            addr < prev_it->second) {
            return std::make_pair(true, prev_it.base());
        } else {
            return std::make_pair(false, next_it);
        }
    }

private:
    // Bounds are members of the std::map. The beginning of the bound is stored
    // as a key and the end as a value.
    std::map<addr_t, addr_t> bounds;
    int block_size_mask_ = 0;
    int block_size_;
};

class caching_device_t;

class caching_device_stats_t {
public:
    explicit caching_device_stats_t(const std::string &miss_file, int block_size,
                                    bool warmup_enabled = false,
                                    bool is_coherent = false);
    virtual ~caching_device_stats_t();

    // Called on each access.
    // A multi-block memory reference invokes this routine
    // separately for each block touched.
    virtual void
    access(const memref_t &memref, bool hit, caching_device_block_t *cache_block);

    // Called on each access by a child caching device.
    virtual void
    child_access(const memref_t &memref, bool hit, caching_device_block_t *cache_block);

    virtual void
    print_stats(std::string prefix);

    virtual void
    reset();

    virtual bool
    operator!()
    {
        return !success_;
    }

    // Process invalidations due to cache inclusions or external writes.
    virtual void
    invalidate(invalidation_type_t invalidation_type);

    int64_t
    get_metric(metric_name_t metric) const
    {
        if (stats_map_.find(metric) != stats_map_.end()) {
            return stats_map_.at(metric);
        } else {
            ERRMSG("Wrong metric name.\n");
            return 0;
        }
    }

    // Returns the address of the caching device last linked to this stats
    // object.  It is up to the user to ensure the caching device still exists
    // before dereferencing this pointer.
    caching_device_t *
    get_caching_device() const
    {
        return caching_device_;
    }

    void
    set_caching_device(caching_device_t *caching_device)
    {
        caching_device_ = caching_device;
    }

protected:
    bool success_;

    // print different groups of information, beneficial for code reuse
    virtual void
    print_warmup(std::string prefix);
    virtual void
    print_counts(std::string prefix); // hit/miss numbers
    virtual void
    print_rates(std::string prefix); // hit/miss rates
    virtual void
    print_child_stats(std::string prefix); // child/total info

    virtual void
    dump_miss(const memref_t &memref);

    void
    check_compulsory_miss(addr_t addr);

    int64_t num_hits_;
    int64_t num_misses_;
    int64_t num_compulsory_misses_;
    int64_t num_child_hits_;

    int64_t num_inclusive_invalidates_;
    int64_t num_coherence_invalidates_;

    // Stats saved when the last reset was called. This helps us get insight
    // into what the stats were when the cache was warmed up.
    int64_t num_hits_at_reset_;
    int64_t num_misses_at_reset_;
    int64_t num_child_hits_at_reset_;
    // Enabled if options warmup_refs > 0 || warmup_fraction > 0
    bool warmup_enabled_;

    // Print out write invalidations if cache is coherent.
    bool is_coherent_;

    // References to the properties with statistics are held in the map with the
    // statistic name as the key. Sample map element: {HITS, num_hits_}
    std::map<metric_name_t, int64_t &> stats_map_;

    // We provide a feature of dumping misses to a file.
    bool dump_misses_;

    access_count_t access_count_;
#ifdef HAS_ZLIB
    gzFile file_;
#else
    FILE *file_;
#endif

    // Convenience pointer to the caching_device last linked to this stats object.
    caching_device_t *caching_device_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _CACHING_DEVICE_STATS_H_ */
