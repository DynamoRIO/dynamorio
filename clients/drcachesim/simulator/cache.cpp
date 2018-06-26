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

#include "cache.h"
#include "../common/utils.h"
#include <assert.h>

bool
cache_t::init(int associativity_, int line_size_, int total_size,
              caching_device_t *parent_, caching_device_stats_t *stats_,
              prefetcher_t *prefetcher_, bool inclusive_,
              const std::vector<caching_device_t *> &children_)
{
    // convert total_size to num_blocks to fit for caching_device_t::init
    int num_lines = total_size / line_size_;

    return caching_device_t::init(associativity_, line_size_, num_lines, parent_, stats_,
                                  prefetcher_, inclusive_, children_);
}

void
cache_t::init_blocks()
{
    for (int i = 0; i < num_blocks; i++) {
        blocks[i] = new cache_line_t;
    }
}

void
cache_t::request(const memref_t &memref_in)
{
    // FIXME i#1726: if the request is a data write, we should check the
    // instr cache and invalidate the cache line there if necessary on x86.
    caching_device_t::request(memref_in);
}

void
cache_t::flush(const memref_t &memref)
{
    addr_t tag = compute_tag(memref.flush.addr);
    addr_t final_tag =
        compute_tag(memref.flush.addr + memref.flush.size - 1 /*no overflow*/);
    last_tag = TAG_INVALID;
    for (; tag <= final_tag; ++tag) {
        int block_idx = compute_block_idx(tag);
        for (int way = 0; way < associativity; ++way) {
            if (get_caching_device_block(block_idx, way).tag == tag) {
                get_caching_device_block(block_idx, way).tag = TAG_INVALID;
                // Xref cache_block_t constructor about why we set counter to 0.
                get_caching_device_block(block_idx, way).counter = 0;
            }
        }
    }
    // We flush parent's code cache here.
    // XXX: should L1 data cache be flushed when L1 instr cache is flushed?
    if (parent != NULL)
        ((cache_t *)parent)->flush(memref);
    if (stats != NULL)
        ((cache_stats_t *)stats)->flush(memref);
}
