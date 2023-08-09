/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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

#include "histogram.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "analysis_tool.h"
#include "memref.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

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

static inline addr_t
back_align(addr_t addr, addr_t align)
{
    return addr & ~(align - 1);
}

bool
histogram_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    std::unordered_map<addr_t, uint64_t> *cache_map = nullptr;
    addr_t start_addr;
    size_t size;
    if (type_is_instr(memref.instr.type) ||
        memref.instr.type == TRACE_TYPE_PREFETCH_INSTR) {
        cache_map = &shard->icache_map;
        start_addr = memref.instr.addr;
        size = memref.instr.size;
    } else if (memref.data.type == TRACE_TYPE_READ ||
               memref.data.type == TRACE_TYPE_WRITE ||
               // We may potentially handle prefetches differently.
               // TRACE_TYPE_PREFETCH_INSTR is handled above.
               type_is_prefetch(memref.data.type)) {
        cache_map = &shard->dcache_map;
        start_addr = memref.instr.addr;
        size = memref.instr.size;
    } else
        return true;
    for (addr_t addr = back_align(start_addr, knob_line_size_);
         addr < start_addr + size && addr < addr + knob_line_size_ /* overflow */;
         addr += knob_line_size_) {
        ++(*cache_map)[addr >> line_size_bits_];
    }
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
histogram_t::reduce_results(uint64_t *unique_icache_lines, uint64_t *unique_dcache_lines)
{
    if (shard_map_.empty()) {
        reduced_ = serial_shard_;
    } else {
        for (const auto &shard : shard_map_) {
            for (const auto &keyvals : shard.second->icache_map) {
                reduced_.icache_map[keyvals.first] += keyvals.second;
            }
            for (const auto &keyvals : shard.second->dcache_map) {
                reduced_.dcache_map[keyvals.first] += keyvals.second;
            }
        }
    }
    if (unique_icache_lines != nullptr)
        *unique_icache_lines = reduced_.icache_map.size();
    if (unique_dcache_lines != nullptr)
        *unique_dcache_lines = reduced_.dcache_map.size();
    return true;
}

bool
histogram_t::print_results()
{
    if (!reduce_results())
        return false;

    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << "icache: " << reduced_.icache_map.size() << " unique cache lines\n";
    std::cerr << "dcache: " << reduced_.dcache_map.size() << " unique cache lines\n";
    std::vector<std::pair<addr_t, uint64_t>> top(knob_report_top_);
    std::partial_sort_copy(reduced_.icache_map.begin(), reduced_.icache_map.end(),
                           top.begin(), top.end(), cmp);
    std::cerr << "icache top " << top.size() << "\n";
    for (std::vector<std::pair<addr_t, uint64_t>>::iterator it = top.begin();
         it != top.end(); ++it) {
        std::cerr << std::setw(18) << std::hex << std::showbase << (it->first << 6)
                  << ": " << std::dec << it->second << "\n";
    }
    top.clear();
    top.resize(knob_report_top_);
    std::partial_sort_copy(reduced_.dcache_map.begin(), reduced_.dcache_map.end(),
                           top.begin(), top.end(), cmp);
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

} // namespace drmemtrace
} // namespace dynamorio
