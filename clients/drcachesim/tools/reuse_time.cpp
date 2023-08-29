/* **********************************************************
 * Copyright (c) 2017-2023 Google, Inc.  All rights reserved.
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

#include "reuse_time.h"

#include <stdint.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <memory>
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

#ifdef DEBUG
#    define DEBUG_VERBOSE(level) (knob_verbose_ >= (level))
#else
#    define DEBUG_VERBOSE(level) (false)
#endif

const std::string reuse_time_t::TOOL_NAME = "Reuse time tool";

analysis_tool_t *
reuse_time_tool_create(unsigned int line_size, unsigned int verbose)
{
    return new reuse_time_t(line_size, verbose);
}

reuse_time_t::reuse_time_t(unsigned int line_size, unsigned int verbose)
    : knob_verbose_(verbose)
    , knob_line_size_(line_size)
    , line_size_bits_(compute_log2((int)knob_line_size_))
{
}

reuse_time_t::~reuse_time_t()
{
    for (auto &shard : shard_map_) {
        delete shard.second;
    }
}

bool
reuse_time_t::parallel_shard_supported()
{
    return true;
}

void *
reuse_time_t::parallel_shard_init(int shard_index, void *worker_data)
{
    auto shard = new shard_data_t();
    std::lock_guard<std::mutex> guard(shard_map_mutex_);
    shard_map_[shard_index] = shard;
    return reinterpret_cast<void *>(shard);
}

bool
reuse_time_t::parallel_shard_exit(void *shard_data)
{
    // Nothing (we need to access the shard data in print_results; we free in
    // the destructor).
    return true;
}

std::string
reuse_time_t::parallel_shard_error(void *shard_data)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    return shard->error;
}

bool
reuse_time_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    if (DEBUG_VERBOSE(3)) {
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

    if (memref.data.type == TRACE_TYPE_THREAD_EXIT) {
        shard->tid = memref.exit.tid;
        return true;
    }

    // Only care about data for now.
    if (type_is_instr(memref.instr.type)) {
        shard->total_instructions++;
        return true;
    }

    // Ignore thread events and other tracing metadata.
    if (memref.data.type != TRACE_TYPE_READ && memref.data.type != TRACE_TYPE_WRITE &&
        !type_is_prefetch(memref.data.type)) {
        return true;
    }

    shard->time_stamp++;
    addr_t line = memref.data.addr >> line_size_bits_;
    if (shard->time_map.count(line) > 0) {
        int64_t reuse_time = shard->time_stamp - shard->time_map[line];
        if (DEBUG_VERBOSE(3)) {
            std::cerr << "Reuse " << reuse_time << std::endl;
        }
        shard->reuse_time_histogram[reuse_time]++;
    }
    shard->time_map[line] = shard->time_stamp;
    return true;
}

bool
reuse_time_t::process_memref(const memref_t &memref)
{
    // For serial operation we index using the tid.
    shard_data_t *shard;
    const auto &lookup = shard_map_.find(memref.data.tid);
    if (lookup == shard_map_.end()) {
        shard = new shard_data_t();
        shard_map_[memref.data.tid] = shard;
    } else
        shard = lookup->second;
    if (!parallel_shard_memref(reinterpret_cast<void *>(shard), memref)) {
        error_string_ = shard->error;
        return false;
    }
    return true;
}

static bool
cmp_dist_key(const std::pair<int64_t, int64_t> &l, const std::pair<int64_t, int64_t> &r)
{
    return (l.first < r.first);
}

void
reuse_time_t::print_shard_results(const shard_data_t *shard)
{
    std::cerr << "Total accesses: " << shard->time_stamp << "\n";
    std::cerr << "Total instructions: " << shard->total_instructions << "\n";
    std::cerr.precision(2);
    std::cerr.setf(std::ios::fixed);

    int64_t count = 0;
    int64_t sum = 0;
    for (const auto &it : shard->reuse_time_histogram) {
        count += it.second;
        sum += it.first * it.second;
    }
    std::cerr << "Mean reuse time: " << sum / static_cast<double>(count) << "\n";

    std::cerr << "Reuse time histogram:\n";
    std::cerr << std::setw(8) << "Distance" << std::setw(12) << "Count" << std::setw(9)
              << "Percent" << std::setw(12) << "Cumulative";
    std::cerr << std::endl;
    double cum_percent = 0.0;
    std::vector<std::pair<int64_t, int64_t>> sorted(shard->reuse_time_histogram.size());
    std::partial_sort_copy(shard->reuse_time_histogram.begin(),
                           shard->reuse_time_histogram.end(), sorted.begin(),
                           sorted.end(), cmp_dist_key);
    for (auto it = sorted.begin(); it != sorted.end(); ++it) {
        double percent = it->second / static_cast<double>(count);
        cum_percent += percent;
        std::cerr << std::setw(8) << it->first << std::setw(12) << it->second
                  << std::setw(8) << percent * 100.0 << "%" << std::setw(11)
                  << cum_percent * 100.0 << "%";
        std::cerr << std::endl;
    }
}

bool
reuse_time_t::print_results()
{
    // First, aggregate the per-shard data into whole-trace data.
    auto aggregate = std::unique_ptr<shard_data_t>(new shard_data_t());
    for (const auto &shard : shard_map_) {
        aggregate->total_instructions += shard.second->total_instructions;
        // We simply sum the accesses.
        aggregate->time_stamp += shard.second->time_stamp;
        // Merge the histograms.
        for (const auto &entry : shard.second->reuse_time_histogram) {
            aggregate->reuse_time_histogram[entry.first] += entry.second;
        }
    }

    std::cerr << TOOL_NAME << " aggregated results:\n";
    print_shard_results(aggregate.get());

    if (shard_map_.size() > 1) {
        using keyval_t = std::pair<memref_tid_t, shard_data_t *>;
        std::vector<keyval_t> sorted(shard_map_.begin(), shard_map_.end());
        std::sort(sorted.begin(), sorted.end(), [](const keyval_t &l, const keyval_t &r) {
            return l.second->time_stamp > r.second->time_stamp;
        });
        for (const auto &shard : sorted) {
            std::cerr << "\n==================================================\n"
                      << TOOL_NAME << " results for shard " << shard.first << " (thread "
                      << shard.second->tid << "):\n";
            print_shard_results(shard.second);
        }
    }

    return true;
}

} // namespace drmemtrace
} // namespace dynamorio
