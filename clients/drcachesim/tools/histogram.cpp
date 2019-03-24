/* **********************************************************
 * Copyright (c) 2016-2017 Google, Inc.  All rights reserved.
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
#include <iomanip>
#include <iostream>
#include <vector>
#include "histogram.h"
#include "../common/utils.h"

const std::string histogram_t::TOOL_NAME = "Cache line histogram tool";

analysis_tool_t *
histogram_tool_create(unsigned int line_size = 64, unsigned int report_top = 10,
                      unsigned int verbose = 0)
{
    return new histogram_t(line_size, report_top, verbose);
}

histogram_t::histogram_t(unsigned int line_size, unsigned int report_top,
                         unsigned int verbose)
    : knob_line_size(line_size)
    , knob_report_top(report_top)
{
    line_size_bits = compute_log2((int)line_size);
}

histogram_t::~histogram_t()
{
}

bool
histogram_t::process_memref(const memref_t &memref)
{
    if (type_is_instr(memref.instr.type) ||
        memref.instr.type == TRACE_TYPE_PREFETCH_INSTR)
        ++icache_map[memref.instr.addr >> line_size_bits];
    else if (memref.data.type == TRACE_TYPE_READ ||
             memref.data.type == TRACE_TYPE_WRITE ||
             // We may potentially handle prefetches differently.
             // TRACE_TYPE_PREFETCH_INSTR is handled above.
             type_is_prefetch(memref.data.type))
        ++dcache_map[memref.data.addr >> line_size_bits];
    return true;
}

bool
cmp(const std::pair<addr_t, uint64_t> &l, const std::pair<addr_t, uint64_t> &r)
{
    return l.second > r.second;
}

bool
histogram_t::print_results()
{
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << "icache: " << icache_map.size() << " unique cache lines\n";
    std::cerr << "dcache: " << dcache_map.size() << " unique cache lines\n";
    std::vector<std::pair<addr_t, uint64_t>> top(knob_report_top);
    std::partial_sort_copy(icache_map.begin(), icache_map.end(), top.begin(), top.end(),
                           cmp);
    std::cerr << "icache top " << top.size() << "\n";
    for (std::vector<std::pair<addr_t, uint64_t>>::iterator it = top.begin();
         it != top.end(); ++it) {
        std::cerr << std::setw(18) << std::hex << std::showbase << (it->first << 6)
                  << ": " << std::dec << it->second << "\n";
    }
    top.clear();
    top.resize(knob_report_top);
    std::partial_sort_copy(dcache_map.begin(), dcache_map.end(), top.begin(), top.end(),
                           cmp);
    std::cerr << "dcache top " << top.size() << "\n";
    for (std::vector<std::pair<addr_t, uint64_t>>::iterator it = top.begin();
         it != top.end(); ++it) {
        std::cerr << std::setw(18) << std::hex << std::showbase << (it->first << 6)
                  << ": " << std::dec << it->second << "\n";
    }
    return true;
}
