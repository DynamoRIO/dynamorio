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

cache_t::cache_t()
{
    lines = 0;
}

bool
cache_t::init(int associativity_, int line_size_, int num_lines_,
              cache_t *parent_, cache_stats_t *stats_)
{
    if (!IS_POWER_OF_2(associativity_) ||
        !IS_POWER_OF_2(line_size_) ||
        !IS_POWER_OF_2(num_lines_))
        return false;
    associativity = associativity_;
    line_size = line_size_;
    num_lines = num_lines_;
    parent = parent_;
    stats = stats_;

    lines = new cache_line_t[num_lines];
    lines_per_set = num_lines / associativity;
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
    memref_t memref = memref_in;
    // We support larger sizes to improve the IPC perf.
    // This means that one memref could touch multiple lines.
    // We treat each line separately for statistics purposes.
    addr_t final_addr = memref.addr + memref.size - 1/*avoid overflow*/;
    addr_t final_tag = final_addr / line_size;
    for (addr_t tag = memref.addr / line_size; tag <= final_tag; ++tag) {
        bool hit = false;
        int final_way = 0;
        int line_idx = tag % lines_per_set;

        if (tag + 1 <= final_tag)
            memref.size = ((tag + 1) * line_size) - memref.addr;

        for (int way = 0; way < associativity; ++way) {
            if (lines[line_idx * way].tag == tag &&
                lines[line_idx * way].valid) {
                hit = true;
                final_way = way;
                break;
            }
        }
        if (!hit) {
            // If no parent we assume we get the data from main memory
            if (parent != NULL)
                parent->request(memref);

            // FIXME i#1703: coherence policy

            final_way = replace_which_way(line_idx);
            lines[line_idx * final_way].tag = tag;
            lines[line_idx * final_way].valid = true;
        }

        access_update(line_idx, final_way);

        if (stats != NULL)
            stats->access(memref, hit);

        if (tag + 1 <= final_tag) {
            addr_t next_addr = (tag + 1) * line_size;
            memref.addr = next_addr;
            memref.size = final_addr - next_addr + 1/*undo the -1*/;
        }
   }
}

void
cache_t::access_update(int line_idx, int way)
{
    // We just inc the counter for LRU.  We live with any blip on overflow.
    lines[line_idx * way].counter++;
}

int
cache_t::replace_which_way(int line_idx)
{
    // We only implement LRU.  A subclass can override this and replace_update()
    // to implement some other scheme.
    int_least64_t min_counter = 0;
    int min_way = 0;
    for (int way = 0; way < associativity; ++way) {
        if (way == 0 || lines[line_idx * way].counter < min_counter) {
            min_counter = lines[line_idx * way].counter;
            min_way = way;
        }
    }
    return min_way;
}
