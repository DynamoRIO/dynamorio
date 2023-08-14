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

/* cache: represents a single hardware cache.
 */

#ifndef _CACHE_H_
#define _CACHE_H_ 1

#include <string>
#include <vector>

#include "cache_line.h"
#include "cache_stats.h"
#include "caching_device.h"
#include "memref.h"
#include "prefetcher.h"

namespace dynamorio {
namespace drmemtrace {

class snoop_filter_t;

class cache_t : public caching_device_t {
public:
    explicit cache_t(const std::string &name = "cache")
        : caching_device_t(name)
    {
    }
    // Size, line size and associativity are generally used
    // to describe a CPU cache.
    // The id is an index into the snoop filter's array of caches for coherent caches.
    // If this is a coherent cache, id should be in the range [0,num_snooped_caches).
    bool
    init(int associativity, int line_size, int total_size, caching_device_t *parent,
         caching_device_stats_t *stats, prefetcher_t *prefetcher = nullptr,
         bool inclusive = false, bool coherent_cache = false, int id_ = -1,
         snoop_filter_t *snoop_filter_ = nullptr,
         const std::vector<caching_device_t *> &children = {}) override;
    void
    request(const memref_t &memref) override;
    virtual void
    flush(const memref_t &memref);

protected:
    void
    init_blocks() override;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _CACHE_H_ */
