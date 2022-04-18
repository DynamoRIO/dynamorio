/* **********************************************************
 * Copyright (c) 2016-2020 Google, Inc.  All rights reserved.
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
    : knob_line_size_(line_size)
    , knob_report_top_(report_top)
{
    line_size_bits_ = compute_log2((int)line_size);
}

histogram_t::~histogram_t()
{
    for (auto &iter : shard_map_) {
        delete iter.second;
    }
}

bool
histogram_t::parallel_shard_supported()
{
    return true;
}

void *
histogram_t::parallel_worker_init(int worker_index)
{
    return nullptr;
}

std::string
histogram_t::parallel_worker_exit(void *worker_data)
{
    return "";
}

void *
histogram_t::parallel_shard_init(int shard_index, void *worker_data)
{
    auto shard = new shard_data_t;
    std::lock_guard<std::mutex> guard(shard_map_mutex_);
    shard_map_[shard_index] = shard;
    return reinterpret_cast<void *>(shard);
}

bool
histogram_t::parallel_shard_exit(void *shard_data)
{
    // Nothing (we read the shard data in print_results).
    return true;
}

bool
histogram_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    if (type_is_instr(memref.instr.type) ||
        memref.instr.type == TRACE_TYPE_PREFETCH_INSTR)
        ++shard->icache_map[memref.instr.addr >> line_size_bits_];
    else if (memref.data.type == TRACE_TYPE_READ ||
             memref.data.type == TRACE_TYPE_WRITE ||
             // We may potentially handle prefetches differently.
             // TRACE_TYPE_PREFETCH_INSTR is handled above.
             type_is_prefetch(memref.data.type))
        ++shard->dcache_map[memref.data.addr >> line_size_bits_];
    return true;
}

std::string
histogram_t::parallel_shard_error(void *shard_data)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    return shard->error;
}

bool
histogram_t::process_memref(const memref_t &memref)
{
    if (!parallel_shard_memref(reinterpret_cast<void *>(&serial_shard_), memref)) {
        error_string_ = serial_shard_.error;
        return false;
    }
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
    shard_data_t total;
    if (shard_map_.empty()) {
        total = serial_shard_;
    } else {
        for (const auto &shard : shard_map_) {
            for (const auto &keyvals : shard.second->icache_map) {
                total.icache_map[keyvals.first] += keyvals.second;
            }
            for (const auto &keyvals : shard.second->dcache_map) {
                total.dcache_map[keyvals.first] += keyvals.second;
            }
        }
    }
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << "icache: " << total.icache_map.size() << " unique cache lines\n";
    std::cerr << "dcache: " << total.dcache_map.size() << " unique cache lines\n";
    std::vector<std::pair<addr_t, uint64_t>> top(knob_report_top_);
    std::partial_sort_copy(total.icache_map.begin(), total.icache_map.end(), top.begin(),
                           top.end(), cmp);
    std::cerr << "icache top " << top.size() << "\n";
    for (std::vector<std::pair<addr_t, uint64_t>>::iterator it = top.begin();
         it != top.end(); ++it) {
        std::cerr << std::setw(18) << std::hex << std::showbase << (it->first << 6)
                  << ": " << std::dec << it->second << "\n";
    }
    top.clear();
    top.resize(knob_report_top_);
    std::partial_sort_copy(total.dcache_map.begin(), total.dcache_map.end(), top.begin(),
                           top.end(), cmp);
    std::cerr << "dcache top " << top.size() << "\n";
    for (std::vector<std::pair<addr_t, uint64_t>>::iterator it = top.begin();
         it != top.end(); ++it) {
        std::cerr << std::setw(18) << std::hex << std::showbase << (it->first << 6)
                  << ": " << std::dec << it->second << "\n";
    }
    // Reset the i/o format for subsequent tool invocations.
    std::cerr << std::dec;
    return true;
}
