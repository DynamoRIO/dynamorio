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

#include "snoop_filter.h"

#include <assert.h>
#include <stdint.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <locale>
#include <string>
#include <unordered_map>
#include <vector>

#include "cache.h"
#include "caching_device_block.h"
#include "caching_device_stats.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

snoop_filter_t::snoop_filter_t(void)
{
}

bool
snoop_filter_t::init(cache_t **caches, int num_snooped_caches)
{
    caches_ = caches;
    num_snooped_caches_ = num_snooped_caches;
    num_writes_ = 0;
    num_writebacks_ = 0;
    num_invalidates_ = 0;

    return true;
}

/*  This function should be called for all misses in snooped caches_ as well as
 *  all writes to coherent caches_.
 */
void
snoop_filter_t::snoop(addr_t tag, int id, bool is_write)
{
    coherence_table_entry_t *coherence_entry = &coherence_table_[tag];
    // Initialize new snoop filter entry.
    if (coherence_entry->sharers.empty()) {
        coherence_entry->sharers.resize(num_snooped_caches_, false);
        coherence_entry->dirty = false;
    }

    auto num_sharers = std::count(coherence_entry->sharers.begin(),
                                  coherence_entry->sharers.end(), true);

    // Check that cache id is valid.
    assert(id >= 0 && id < num_snooped_caches_);
    // Check that tag is valid.
    assert(tag != TAG_INVALID);
    // Check that any dirty line is only held in one snooped cache.
    assert(!coherence_entry->dirty || num_sharers == 1);

    // Check if this request causes a writeback.
    if (!coherence_entry->sharers[id] && coherence_entry->dirty) {
        num_writebacks_++;
        coherence_entry->dirty = false;
    }

    if (is_write) {
        num_writes_++;
        coherence_entry->dirty = true;
        if (num_sharers > 0) {
            // Writes will invalidate other caches_.
            for (int i = 0; i < num_snooped_caches_; i++) {
                if (coherence_entry->sharers[i] && id != i) {
                    caches_[i]->invalidate(tag, INVALIDATION_COHERENCE);
                    num_invalidates_++;
                    coherence_entry->sharers[i] = false;
                }
            }
        }
    }
    coherence_entry->sharers[id] = true;
}

/* This function is called whenever a coherent cache evicts a line. */
void
snoop_filter_t::snoop_eviction(addr_t tag, int id)
{
    coherence_table_entry_t *coherence_entry = &coherence_table_[tag];

    // Check if sharer list is initialized.
    assert(coherence_entry->sharers.size() == (uint64_t)num_snooped_caches_);
    // Check that cache id is valid.
    assert(id >= 0 && id < num_snooped_caches_);
    // Check that tag is valid.
    assert(tag != TAG_INVALID);
    // Check that we currently have this cache marked as a sharer.
    assert(coherence_entry->sharers[id]);

    if (coherence_entry->dirty) {
        num_writebacks_++;
        coherence_entry->dirty = false;
    }

    coherence_entry->sharers[id] = false;
}

void
snoop_filter_t::print_stats(void)
{
    std::cerr.imbue(std::locale("")); // Add commas, at least for my locale.
    std::string prefix = "    ";
    std::cerr << "Coherence stats:" << std::endl;
    std::cerr << prefix << std::setw(18) << std::left << "Total writes:" << std::setw(20)
              << std::right << num_writes_ << std::endl;
    std::cerr << prefix << std::setw(18) << std::left << "Invalidations:" << std::setw(20)
              << std::right << num_invalidates_ << std::endl;
    std::cerr << prefix << std::setw(18) << std::left << "Writebacks:" << std::setw(20)
              << std::right << num_writebacks_ << std::endl;
    std::cerr.imbue(std::locale("C")); // Reset to avoid affecting later prints.
}

} // namespace drmemtrace
} // namespace dynamorio
