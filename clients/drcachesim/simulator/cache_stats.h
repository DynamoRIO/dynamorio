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

/* cache_stats: represents a CPU cache.
 */

#ifndef _CACHE_STATS_H_
#define _CACHE_STATS_H_ 1

#include <stdint.h>

#include <string>

#include "caching_device_stats.h"
#include "memref.h"

namespace dynamorio {
namespace drmemtrace {

class cache_stats_t : public caching_device_stats_t {
public:
    explicit cache_stats_t(int block_size, const std::string &miss_file = "",
                           bool warmup_enabled = false, bool is_coherent = false);

    // In addition to caching_device_stats_t::access,
    // cache_stats_t::access processes prefetching requests.
    virtual void
    access(const memref_t &memref, bool hit,
           caching_device_block_t *cache_block) override;

    // process CPU cache flushes
    virtual void
    flush(const memref_t &memref);

    void
    reset() override;

protected:
    // In addition to caching_device_stats_t::print_counts,
    // cache_stats_t::print_counts prints stats for flushes and
    // prefetching requests.
    void
    print_counts(std::string prefix) override;

    // A CPU cache handles flushes and prefetching requests
    // as well as regular memory accesses.
    int64_t num_flushes_;
    int64_t num_prefetch_hits_;
    int64_t num_prefetch_misses_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _CACHE_STATS_H_ */
