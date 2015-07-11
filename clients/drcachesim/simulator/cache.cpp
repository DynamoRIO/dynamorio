/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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
#include "cache_line.h"
#include "cache_stats.h"
#include "utils.h"
#include <assert.h>

cache_t::cache_t()
{
    lines = 0;
}

bool
cache_t::init(int associativity_, int line_size_, int total_size,
              cache_t *parent_, cache_stats_t *stats_)
{
    if (!IS_POWER_OF_2(associativity_) ||
        !IS_POWER_OF_2(line_size_) ||
        total_size % line_size_ != 0 ||
        // Assuming cache line size is at least 4 bytes
        line_size_ < 4)
        return false;
    if (stats_ == NULL)
        return false; // A stats must be provided for perf: avoid conditional code
    associativity = associativity_;
    line_size = line_size_;
    num_lines = total_size / line_size;
    lines_per_set = num_lines / associativity;
    assoc_bits = compute_log2(associativity);
    line_size_bits = compute_log2(line_size);
    lines_per_set_mask = lines_per_set - 1;
    if (assoc_bits == -1 || line_size_bits == -1 || !IS_POWER_OF_2(lines_per_set))
        return false;
    parent = parent_;
    stats = stats_;

    lines = new cache_line_t[num_lines];
    last_tag = TAG_INVALID; // sentinel
    return true;
}

cache_t::~cache_t()
{
    delete [] lines;
}

void
cache_t::request(const memref_t &memref_in)
{
    // Unfortunately we need to make a copy for our loop so we can pass
    // the right data struct to the parent and stats collectors.
    memref_t memref;
    // We support larger sizes to improve the IPC perf.
    // This means that one memref could touch multiple lines.
    // We treat each line separately for statistics purposes.
    addr_t final_addr = memref_in.addr + memref_in.size - 1/*avoid overflow*/;
    addr_t final_tag = compute_tag(final_addr);
    addr_t tag = compute_tag(memref_in.addr);

    // FIXME i#1726: if the request is a data write, we should check the
    // instr cache and invalidate the cache line there if necessary on x86.

    // Optimization: check last tag if single-line
    if (tag == final_tag && tag == last_tag) {
        // Make sure last_tag is properly in sync.
        assert(tag != TAG_INVALID &&
               tag == get_cache_line(last_line_idx, last_way).tag);
        stats->access(memref_in, true/*hit*/);
        if (parent != NULL)
            parent->stats->child_access(memref_in, true);
        access_update(last_line_idx, last_way);
        return;
    }

    memref = memref_in;
    for (; tag <= final_tag; ++tag) {
        int way;
        int line_idx = compute_line_idx(tag);

        if (tag + 1 <= final_tag)
            memref.size = ((tag + 1) << line_size_bits) - memref.addr;

        for (way = 0; way < associativity; ++way) {
            if (get_cache_line(line_idx, way).tag == tag) {
                stats->access(memref, true/*hit*/);
                if (parent != NULL)
                    parent->stats->child_access(memref, true);
                break;
            }
        }

        if (way == associativity) {
            stats->access(memref, false/*miss*/);
            // If no parent we assume we get the data from main memory
            if (parent != NULL) {
                parent->stats->child_access(memref, false);
                parent->request(memref);
            }

            // FIXME i#1726: coherence policy

            way = replace_which_way(line_idx);
            get_cache_line(line_idx, way).tag = tag;
        }

        access_update(line_idx, way);

        if (tag + 1 <= final_tag) {
            addr_t next_addr = (tag + 1) << line_size_bits;
            memref.addr = next_addr;
            memref.size = final_addr - next_addr + 1/*undo the -1*/;
        }
        // Optimization: remember last tag
        last_tag = tag;
        last_way = way;
        last_line_idx = line_idx;
    }
}

void
cache_t::access_update(int line_idx, int way)
{
    // We just inc the counter for LFU.  We live with any blip on overflow.
    get_cache_line(line_idx, way).counter++;
}

int
cache_t::replace_which_way(int line_idx)
{
    // The base cache class only implements LFU.
    // A subclass can override this and access_update() to implement
    // some other scheme.
    int min_counter;
    int min_way = 0;
    for (int way = 0; way < associativity; ++way) {
        if (get_cache_line(line_idx, way).tag == TAG_INVALID) {
            min_way = way;
            break;
        }
        if (way == 0 || get_cache_line(line_idx, way).counter < min_counter) {
            min_counter = get_cache_line(line_idx, way).counter;
            min_way = way;
        }
    }
    // Clear the counter for LFU.
    get_cache_line(line_idx, min_way).counter = 0;
    return min_way;
}

void
cache_t::flush(const memref_t &memref)
{
    addr_t tag = compute_tag(memref.addr);
    addr_t final_tag = compute_tag(memref.addr + memref.size - 1/*no overflow*/);
    last_tag = TAG_INVALID;
    for (; tag <= final_tag; ++tag) {
        int line_idx = compute_line_idx(tag);
        for (int way = 0; way < associativity; ++way) {
            if (get_cache_line(line_idx, way).tag == tag) {
                get_cache_line(line_idx, way).tag = TAG_INVALID;
                // Xref cache_line_t constructor about why we set counter to 0.
                get_cache_line(line_idx, way).counter = 0;
            }
        }
    }
    // We flush parent's code cache here.
    // XXX: should L1 data cache be flushed when L1 instr cache is flushed?
    if (parent != NULL)
        parent->flush(memref);
    if (stats != NULL)
        stats->flush(memref);
}
