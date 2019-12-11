/* **********************************************************
 * Copyright (c) 2016-2019 Google, Inc.  All rights reserved.
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
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>
#include "reuse_distance.h"
#include "../common/utils.h"

const std::string reuse_distance_t::TOOL_NAME = "Reuse distance tool";

unsigned int reuse_distance_t::knob_verbose;

analysis_tool_t *
reuse_distance_tool_create(const reuse_distance_knobs_t &knobs)
{
    return new reuse_distance_t(knobs);
}

reuse_distance_t::reuse_distance_t(const reuse_distance_knobs_t &knobs_)
    : knobs(knobs_)
    , line_size_bits(compute_log2((int)knobs.line_size))
{
    if (DEBUG_VERBOSE(2)) {
        std::cerr << "cache line size " << knobs.line_size << ", "
                  << "reuse distance threshold " << knobs.distance_threshold << std::endl;
    }
}

reuse_distance_t::~reuse_distance_t()
{
    for (auto &shard : shard_map) {
        delete shard.second;
    }
}

reuse_distance_t::shard_data_t::shard_data_t(uint64_t reuse_threshold, uint64_t skip_dist,
                                             bool verify)
{
    ref_list = std::unique_ptr<line_ref_list_t>(
        new line_ref_list_t(reuse_threshold, skip_dist, verify));
}

bool
reuse_distance_t::parallel_shard_supported()
{
    return true;
}

void *
reuse_distance_t::parallel_shard_init(int shard_index, void *worker_data)
{
    auto shard = new shard_data_t(knobs.distance_threshold, knobs.skip_list_distance,
                                  knobs.verify_skip);
    std::lock_guard<std::mutex> guard(shard_map_mutex);
    shard_map[shard_index] = shard;
    return reinterpret_cast<void *>(shard);
}

bool
reuse_distance_t::parallel_shard_exit(void *shard_data)
{
    // Nothing (we read the shard data in print_results).
    return true;
}

std::string
reuse_distance_t::parallel_shard_error(void *shard_data)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    return shard->error;
}

bool
reuse_distance_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
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
    if (type_is_instr(memref.instr.type) || memref.data.type == TRACE_TYPE_READ ||
        memref.data.type == TRACE_TYPE_WRITE ||
        // We may potentially handle prefetches differently.
        // TRACE_TYPE_PREFETCH_INSTR is handled above.
        type_is_prefetch(memref.data.type)) {
        ++shard->total_refs;
        addr_t tag = memref.data.addr >> line_size_bits;
        std::unordered_map<addr_t, line_ref_t *>::iterator it =
            shard->cache_map.find(tag);
        if (it == shard->cache_map.end()) {
            line_ref_t *ref = new line_ref_t(tag);
            // insert into the map
            shard->cache_map.insert(std::pair<addr_t, line_ref_t *>(tag, ref));
            // insert into the list
            shard->ref_list->add_to_front(ref);
        } else {
            int_least64_t dist = shard->ref_list->move_to_front(it->second);
            std::unordered_map<int_least64_t, int_least64_t>::iterator dist_it =
                shard->dist_map.find(dist);
            if (dist_it == shard->dist_map.end())
                shard->dist_map.insert(std::pair<int_least64_t, int_least64_t>(dist, 1));
            else
                ++dist_it->second;
            if (DEBUG_VERBOSE(3)) {
                std::cerr << "Distance is " << dist << "\n";
            }
        }
    }
    return true;
}

bool
reuse_distance_t::process_memref(const memref_t &memref)
{
    // For serial operation we index using the tid.
    shard_data_t *shard;
    const auto &lookup = shard_map.find(memref.data.tid);
    if (lookup == shard_map.end()) {
        shard = new shard_data_t(knobs.distance_threshold, knobs.skip_list_distance,
                                 knobs.verify_skip);
        shard_map[memref.data.tid] = shard;
    } else
        shard = lookup->second;
    if (!parallel_shard_memref(reinterpret_cast<void *>(shard), memref)) {
        error_string = shard->error;
        return false;
    }
    return true;
}

static bool
cmp_dist_key(const std::pair<int_least64_t, int_least64_t> &l,
             const std::pair<int_least64_t, int_least64_t> &r)
{
    return l.first < r.first;
}

static bool
cmp_total_refs(const std::pair<addr_t, line_ref_t *> &l,
               const std::pair<addr_t, line_ref_t *> &r)
{
    if (l.second->total_refs > r.second->total_refs)
        return true;
    if (l.second->total_refs < r.second->total_refs)
        return false;
    if (l.second->distant_refs > r.second->distant_refs)
        return true;
    if (l.second->distant_refs < r.second->distant_refs)
        return false;
    return l.first < r.first;
}

static bool
cmp_distant_refs(const std::pair<addr_t, line_ref_t *> &l,
                 const std::pair<addr_t, line_ref_t *> &r)
{
    if (l.second->distant_refs > r.second->distant_refs)
        return true;
    if (l.second->distant_refs < r.second->distant_refs)
        return false;
    if (l.second->total_refs > r.second->total_refs)
        return true;
    if (l.second->total_refs < r.second->total_refs)
        return false;
    return l.first < r.first;
}

void
reuse_distance_t::print_shard_results(const shard_data_t *shard)
{
    std::cerr << "Total accesses: " << shard->total_refs << "\n";
    std::cerr << "Unique accesses: " << shard->ref_list->cur_time << "\n";
    std::cerr << "Unique cache lines accessed: " << shard->cache_map.size() << "\n";
    std::cerr << "\n";

    std::cerr.precision(2);
    std::cerr.setf(std::ios::fixed);

    double sum = 0.0;
    int_least64_t count = 0;
    for (const auto &it : shard->dist_map) {
        sum += it.first * it.second;
        count += it.second;
    }
    double mean = sum / count;
    std::cerr << "Reuse distance mean: " << mean << "\n";
    double sum_of_squares = 0;
    int_least64_t recount = 0;
    bool have_median = false;
    std::vector<std::pair<int_least64_t, int_least64_t>> sorted(shard->dist_map.size());
    std::partial_sort_copy(shard->dist_map.begin(), shard->dist_map.end(), sorted.begin(),
                           sorted.end(), cmp_dist_key);
    for (auto it = sorted.begin(); it != sorted.end(); ++it) {
        double diff = it->first - mean;
        sum_of_squares += (diff * diff) * it->second;
        if (!have_median) {
            recount += it->second;
            if (recount >= count / 2) {
                std::cerr << "Reuse distance median: " << it->first << "\n";
                have_median = true;
            }
        }
    }
    double stddev = std::sqrt(sum_of_squares / count);
    std::cerr << "Reuse distance standard deviation: " << stddev << "\n";

    if (knobs.report_histogram) {
        std::cerr << "Reuse distance histogram:\n";
        std::cerr << "Distance" << std::setw(12) << "Count"
                  << "  Percent  Cumulative\n";
        double cum_percent = 0;
        for (auto it = sorted.begin(); it != sorted.end(); ++it) {
            double percent = it->second / static_cast<double>(count);
            cum_percent += percent;
            std::cerr << std::setw(8) << it->first << std::setw(12) << it->second
                      << std::setw(8) << percent * 100. << "%" << std::setw(8)
                      << cum_percent * 100. << "%\n";
        }
    } else {
        std::cerr << "(Pass -reuse_distance_histogram to see all the data.)\n";
    }

    std::cerr << "\n";
    std::cerr << "Reuse distance threshold = " << knobs.distance_threshold
              << " cache lines\n";
    std::vector<std::pair<addr_t, line_ref_t *>> top(knobs.report_top);
    std::partial_sort_copy(shard->cache_map.begin(), shard->cache_map.end(), top.begin(),
                           top.end(), cmp_total_refs);
    std::cerr << "Top " << top.size() << " frequently referenced cache lines\n";
    std::cerr << std::setw(18) << "cache line"
              << ": " << std::setw(17) << "#references  " << std::setw(14)
              << "#distant refs"
              << "\n";
    for (std::vector<std::pair<addr_t, line_ref_t *>>::iterator it = top.begin();
         it != top.end(); ++it) {
        if (it->second == NULL) // Very small app.
            break;
        std::cerr << std::setw(18) << std::hex << std::showbase
                  << (it->first << line_size_bits) << ": " << std::setw(12) << std::dec
                  << it->second->total_refs << ", " << std::setw(12) << std::dec
                  << it->second->distant_refs << "\n";
    }
    top.clear();
    top.resize(knobs.report_top);
    std::partial_sort_copy(shard->cache_map.begin(), shard->cache_map.end(), top.begin(),
                           top.end(), cmp_distant_refs);
    std::cerr << "Top " << top.size() << " distant repeatedly referenced cache lines\n";
    std::cerr << std::setw(18) << "cache line"
              << ": " << std::setw(17) << "#references  " << std::setw(14)
              << "#distant refs"
              << "\n";
    for (std::vector<std::pair<addr_t, line_ref_t *>>::iterator it = top.begin();
         it != top.end(); ++it) {
        if (it->second == NULL) // Very small app.
            break;
        std::cerr << std::setw(18) << std::hex << std::showbase
                  << (it->first << line_size_bits) << ": " << std::setw(12) << std::dec
                  << it->second->total_refs << ", " << std::setw(12) << std::dec
                  << it->second->distant_refs << "\n";
    }
}

bool
reuse_distance_t::print_results()
{
    // First, aggregate the per-shard data into whole-trace data.
    auto aggregate = std::unique_ptr<shard_data_t>(new shard_data_t(
        knobs.distance_threshold, knobs.skip_list_distance, knobs.verify_skip));
    for (const auto &shard : shard_map) {
        aggregate->total_refs += shard.second->total_refs;
        // We simply sum the unique accesses.
        // If the user wants the unique accesses over the merged trace they
        // can create a single shard and invoke the parallel operations.
        aggregate->ref_list->cur_time += shard.second->ref_list->cur_time;
        // We merge the histogram and the cache_map.
        for (const auto &entry : shard.second->dist_map) {
            aggregate->dist_map[entry.first] += entry.second;
        }
        for (const auto &entry : shard.second->cache_map) {
            const auto &existing = aggregate->cache_map.find(entry.first);
            line_ref_t *ref;
            if (existing == aggregate->cache_map.end()) {
                ref = new line_ref_t(entry.first);
                aggregate->cache_map.insert(
                    std::pair<addr_t, line_ref_t *>(entry.first, ref));
                ref->total_refs = 0;
            } else {
                ref = existing->second;
            }
            ref->total_refs += entry.second->total_refs;
            ref->distant_refs += entry.second->distant_refs;
        }
    }

    std::cerr << TOOL_NAME << " aggregated results:\n";
    print_shard_results(aggregate.get());

    // For regular shards the line_ref_t's are deleted in ~line_ref_list_t.
    for (auto &iter : aggregate->cache_map) {
        delete iter.second;
    }

    if (shard_map.size() > 1) {
        using keyval_t = std::pair<memref_tid_t, shard_data_t *>;
        std::vector<keyval_t> sorted(shard_map.begin(), shard_map.end());
        std::sort(sorted.begin(), sorted.end(), [](const keyval_t &l, const keyval_t &r) {
            return l.second->total_refs > r.second->total_refs;
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
