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
#include <iostream>
#include "droption.h"
#include "reuse_distance.h"
#include "../common/options.h"
#include "../common/utils.h"

const std::string reuse_distance_t::TOOL_NAME = "Reuse distance tool";

reuse_distance_t::reuse_distance_t() : total_refs(0)
{
    line_size = op_line_size.get_value();
    line_size_bits = compute_log2((int)line_size);
    ref_list = new line_ref_list_t(op_reuse_distance_threshold.get_value());
    report_top = op_report_top.get_value();
    if (op_verbose.get_value() >= 2) {
        std::cerr << "cache line size " << line_size << ", "
                  << "reuse distance threshold " << ref_list->threshold << std::endl;
    }
}

reuse_distance_t::~reuse_distance_t()
{
}

bool
reuse_distance_t::process_memref(const memref_t &memref)
{
    if (type_is_instr(memref.instr.type) ||
        memref.data.type == TRACE_TYPE_READ ||
        memref.data.type == TRACE_TYPE_WRITE ||
        // We may potentially handle prefetches differently.
        // TRACE_TYPE_PREFETCH_INSTR is handled above.
        type_is_prefetch(memref.data.type)) {
        ++total_refs;
        addr_t tag = memref.data.addr >> line_size_bits;
        std::map<addr_t, line_ref_t*>::iterator it = cache_map.find(tag);
        if (it == cache_map.end()) {
            line_ref_t *ref = new line_ref_t(tag);
            // insert into the map
            cache_map.insert(std::pair<addr_t, line_ref_t*>(tag, ref));
            // insert into the list
            ref_list->add_to_front(ref);
        } else {
            ref_list->move_to_front(it->second);
        }
    }
    if (op_verbose.get_value() >= 3) {
        std::cerr << " ::" << memref.data.pid << "." << memref.data.tid
                  << ":: " << trace_type_names[memref.data.type];
        if (memref.data.type != TRACE_TYPE_THREAD_EXIT) {
            std::cerr << " @ ";
            if (!type_is_instr(memref.data.type))
                std::cerr << (void *)memref.data.pc << " ";
            std::cerr << (void *)memref.data.addr << " x" << memref.data.size;
        }
        std::cerr << std::endl;
    }
    return true;
}

bool cmp_total_refs(const std::pair<addr_t, line_ref_t*> &l,
                    const std::pair<addr_t, line_ref_t*> &r)
{
    if (l.second->total_refs > r.second->total_refs)
        return true;
    if (l.second->total_refs < r.second->total_refs)
        return false;
    if (l.second->distant_refs > r.second->distant_refs)
        return true;
    return false;
}

bool cmp_distant_refs(const std::pair<addr_t, line_ref_t*> &l,
                      const std::pair<addr_t, line_ref_t*> &r)
{
    if (l.second->distant_refs > r.second->distant_refs)
        return true;
    if (l.second->distant_refs < r.second->distant_refs)
        return false;
    if (l.second->total_refs > r.second->total_refs)
        return true;
    return false;
}

bool
reuse_distance_t::print_results()
{
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << "Total accesses: " << total_refs << "\n";
    std::cerr << "Unique accesses: " << ref_list->cur_time << "\n";
    std::cerr << "Unique cache lines accessed: " << ref_list->unique_lines << "\n";
    std::cerr << "\n";
    std::cerr << "Reuse distance threshold = "
              << ref_list->threshold << " cache lines\n";
    std::vector<std::pair<addr_t, line_ref_t*> > top(report_top);
    std::partial_sort_copy(cache_map.begin(), cache_map.end(),
                           top.begin(), top.end(), cmp_total_refs);
    std::cerr << "Top " << top.size() << " frequently referenced cache lines\n";
    std::cerr << std::setw(18) << "cache line"
              << ": " << std::setw(17) << "#references  "
              << std::setw(14) << "#distant refs" << "\n";
    for (std::vector<std::pair<addr_t, line_ref_t*> >::iterator it = top.begin();
         it != top.end(); ++it) {
        if (it->second == NULL) // Very small app.
            break;
        std::cerr << std::setw(18) << std::hex << std::showbase
                  << (it->first << line_size_bits)
                  << ": " << std::setw(12) << std::dec << it->second->total_refs
                  << ", " << std::setw(12) << std::dec << it->second->distant_refs
                  << "\n";
    }
    top.clear();
    top.resize(report_top);
    std::partial_sort_copy(cache_map.begin(), cache_map.end(),
                           top.begin(), top.end(), cmp_distant_refs);
    std::cerr << "Top " << top.size() << " distant repeatedly referenced cache lines\n";
    std::cerr << std::setw(18) << "cache line"
              << ": " << std::setw(17) << "#references  "
              << std::setw(14) << "#distant refs" << "\n";
    for (std::vector<std::pair<addr_t, line_ref_t*> >::iterator it = top.begin();
         it != top.end(); ++it) {
        if (it->second == NULL) // Very small app.
            break;
        std::cerr << std::setw(18) << std::hex << std::showbase
                  << (it->first << line_size_bits)
                  << ": " << std::setw(12) << std::dec << it->second->total_refs
                  << ", " << std::setw(12) << std::dec << it->second->distant_refs
                  << "\n";
    }
    return true;
}
