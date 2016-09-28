/* **********************************************************
 * Copyright (c) 2016 Google, Inc.  All rights reserved.
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

#include <algorithm>
#include <iostream>
#include "droption.h"
#include "histogram.h"
#include "../common/options.h"
#include "../common/utils.h"

const std::string histogram_t::TOOL_NAME = "Cache Histogram";

histogram_t::histogram_t()
{
    line_size = op_line_size.get_value();
    line_size_bits = compute_log2((int)line_size);
    report_top = op_report_top.get_value();
}

histogram_t::~histogram_t()
{
}

bool
histogram_t::process_memref(const memref_t &memref)
{
    if (memref.type == TRACE_TYPE_INSTR ||
        memref.type == TRACE_TYPE_PREFETCH_INSTR)
        ++icache_map[memref.addr >> line_size_bits];
    else if (memref.type == TRACE_TYPE_READ ||
             memref.type == TRACE_TYPE_WRITE ||
             // We may potentially handle prefetches differently.
             // TRACE_TYPE_PREFETCH_INSTR is handled above.
             type_is_prefetch(memref.type))
        ++dcache_map[memref.addr >> line_size_bits];
    return true;
}

bool cmp(const std::pair<addr_t, uint64_t> &l,
         const std::pair<addr_t, uint64_t> &r)
{
    return l.second > r.second;
}

bool
histogram_t::print_results()
{
    std::cerr << TOOL_NAME << " result:\n";
    std::cerr << TOOL_NAME << ": icache = " << icache_map.size()
              << " unique cache lines\n";
    std::cerr << TOOL_NAME << ": dcache = " << dcache_map.size()
              << " unique cache lines\n";
    std::vector<std::pair<addr_t, uint64_t> > top(report_top);
    std::partial_sort_copy(icache_map.begin(), icache_map.end(),
                           top.begin(), top.end(), cmp);
    std::cerr << TOOL_NAME << ": icache top " << top.size() << "\n";
    for (std::vector<std::pair<addr_t, uint64_t> >::iterator it = top.begin();
         it != top.end(); ++it) {
        std::cerr << std::setw(18) << std::hex << std::showbase << (it->first << 6)
                  << ": " << std::dec << it->second << "\n";
    }
    top.clear();
    top.resize(report_top);
    std::partial_sort_copy(dcache_map.begin(), dcache_map.end(),
                           top.begin(), top.end(), cmp);
    std::cerr << TOOL_NAME << ": dcache top " << top.size() << "\n";
    for (std::vector<std::pair<addr_t, uint64_t> >::iterator it = top.begin();
         it != top.end(); ++it) {
        std::cerr << std::setw(18) << std::hex << std::showbase << (it->first << 6)
                  << ": " << std::dec << it->second << "\n";
    }
    return true;
}
