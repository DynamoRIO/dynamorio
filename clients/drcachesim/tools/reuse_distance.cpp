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

#include "reuse_distance.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "analysis_tool.h"
#include "memref.h"
#include "reuse_distance_create.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

const std::string reuse_distance_t::TOOL_NAME = "Reuse distance tool";

unsigned int reuse_distance_t::knob_verbose;

analysis_tool_t *
reuse_distance_tool_create(const reuse_distance_knobs_t &knobs)
{
    return new reuse_distance_t(knobs);
}

reuse_distance_t::reuse_distance_t(const reuse_distance_knobs_t &knobs)
    : knobs_(knobs)
    , line_size_bits_(compute_log2((int)knobs_.line_size))
{
    reuse_distance_t::knob_verbose = knobs.verbose;
    IF_DEBUG_VERBOSE(2,
                     std::cerr << "cache line size " << knobs_.line_size << ", "
                               << "reuse distance threshold " << knobs_.distance_threshold
                               << ", distance limit " << knobs_.distance_limit << "\n");
}

reuse_distance_t::~reuse_distance_t()
{
    for (auto &shard : shard_map_) {
        delete shard.second;
    }
}

reuse_distance_t::shard_data_t::shard_data_t(uint64_t reuse_threshold, uint64_t skip_dist,
                                             uint32_t distance_limit, bool verify)
    : distance_limit(distance_limit)
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
    auto shard = new shard_data_t(knobs_.distance_threshold, knobs_.skip_list_distance,
                                  knobs_.distance_limit, knobs_.verify_skip);
    std::lock_guard<std::mutex> guard(shard_map_mutex_);
    shard_map_[shard_index] = shard;
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
    IF_DEBUG_VERBOSE(3, {
        std::cerr << " ::" << memref.data.pid << "." << memref.data.tid
                  << ":: " << trace_type_names[memref.data.type];
        if (memref.data.type != TRACE_TYPE_THREAD_EXIT) {
            std::cerr << " @ ";
            if (!type_is_instr(memref.data.type))
                std::cerr << (void *)memref.data.pc << " ";
            std::cerr << (void *)memref.data.addr << " x" << memref.data.size;
        }
        std::cerr << "\n";
    });
    if (memref.data.type == TRACE_TYPE_THREAD_EXIT) {
        shard->tid = memref.exit.tid;
        return true;
    }
    bool is_instr_type = type_is_instr(memref.instr.type);
    if (is_instr_type || memref.data.type == TRACE_TYPE_READ ||
        memref.data.type == TRACE_TYPE_WRITE ||
        // We may potentially handle prefetches differently.
        // TRACE_TYPE_PREFETCH_INSTR is handled above.
        type_is_prefetch(memref.data.type)) {
        ++shard->total_refs;
        if (!is_instr_type) {
            ++shard->data_refs;
        }
        addr_t tag = memref.data.addr >> line_size_bits_;
        std::unordered_map<addr_t, line_ref_t *>::iterator it =
            shard->cache_map.find(tag);
        if (it == shard->cache_map.end()) {
            line_ref_t *ref = new line_ref_t(tag);
            // insert into the map
            shard->cache_map.insert(std::pair<addr_t, line_ref_t *>(tag, ref));
            // insert into the list
            shard->ref_list->add_to_front(ref);
            // See if the line we're adding was previously removed.
            if (shard->pruned_addresses.find(tag) != shard->pruned_addresses.end()) {
                ++shard->pruned_address_hits;
                shard->pruned_addresses.erase(tag); // It has been unpruned.
            }
            if (shard->distance_limit > 0 &&
                shard->distance_limit < shard->cache_map.size()) {
                // Distance list is too long, so prune most-distant entry.
                ref = shard->ref_list->tail_; // Get a pointer to the line.
                assert(ref != NULL);
                addr_t tag_to_remove = ref->tag;
                // Move this line from the cache_map to the pruned set.
                shard->cache_map.erase(tag_to_remove);
                shard->pruned_addresses.insert(tag_to_remove);
                ++shard->pruned_address_count;
                // Remove this oldest entry from the reference list.
                shard->ref_list->prune_tail();
                // Delete the no-longer-needed line object
                delete ref;
            }
        } else {
            int64_t dist = shard->ref_list->move_to_front(it->second);
            auto &dist_map = is_instr_type ? shard->dist_map : shard->dist_map_data;
            distance_histogram_t::iterator dist_it = dist_map.find(dist);
            if (dist_it == dist_map.end())
                dist_map.insert(distance_map_pair_t(dist, 1));
            else
                ++dist_it->second;
            IF_DEBUG_VERBOSE(3, std::cerr << "Distance is " << std::dec << dist << "\n");
        }
    }
    return true;
}

bool
reuse_distance_t::process_memref(const memref_t &memref)
{
    // For serial operation we index using the tid.
    shard_data_t *shard;
    const auto &lookup = shard_map_.find(memref.data.tid);
    if (lookup == shard_map_.end()) {
        shard = new shard_data_t(knobs_.distance_threshold, knobs_.skip_list_distance,
                                 knobs_.distance_limit, knobs_.verify_skip);
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
cmp_dist_key(const reuse_distance_t::distance_map_pair_t &l,
             const reuse_distance_t::distance_map_pair_t &r)
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
    std::cerr << std::dec;
    std::cerr << "Total accesses: " << shard->total_refs << "\n";
    // If no accesses were processed, there's nothing more to report.
    if (shard->total_refs == 0)
        return;
    std::cerr << "Instruction accesses: " << shard->total_refs - shard->data_refs << "\n";
    std::cerr << "Data accesses: " << shard->data_refs << "\n";
    std::cerr << "Unique accesses: " << shard->ref_list->cur_time_ << "\n";
    std::cerr << "Unique cache lines accessed: "
              << shard->cache_map.size() + shard->pruned_addresses.size() << "\n";
    std::cerr << "Distance limit: " << shard->distance_limit << "\n";
    std::cerr << "Pruned addresses: " << shard->pruned_address_count << "\n";
    std::cerr << "Pruned address hits: " << shard->pruned_address_hits << "\n";
    std::cerr << "\n";

    std::cerr.precision(2);
    std::cerr.setf(std::ios::fixed);

    double sum = 0.0;
    int64_t count = 0;
    for (const auto &it : shard->dist_map) {
        sum += it.first * it.second;
        count += it.second;
    }
    double mean = sum / count;
    std::cerr << "Reuse distance mean: " << mean << "\n";
    double sum_of_squares = 0;
    int64_t recount = 0;
    bool have_median = false;
    std::vector<distance_map_pair_t> sorted(shard->dist_map.size());
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

    if (knobs_.report_histogram) {
        print_histogram(std::cerr, count, sorted, shard->dist_map_data);
    } else {
        std::cerr << "(Pass -reuse_distance_histogram to see all the data.)\n";
    }

    std::cerr << "\n";
    std::cerr << "Reuse distance threshold = " << knobs_.distance_threshold
              << " cache lines\n";
    std::vector<std::pair<addr_t, line_ref_t *>> top(knobs_.report_top);
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
                  << (it->first << line_size_bits_) << ": " << std::setw(12) << std::dec
                  << it->second->total_refs << ", " << std::setw(12) << std::dec
                  << it->second->distant_refs << "\n";
    }
    top.clear();
    top.resize(knobs_.report_top);
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
                  << (it->first << line_size_bits_) << ": " << std::setw(12) << std::dec
                  << it->second->total_refs << ", " << std::setw(12) << std::dec
                  << it->second->distant_refs << "\n";
    }
}

void
reuse_distance_t::print_histogram(std::ostream &out, int64_t total_count,
                                  const std::vector<distance_map_pair_t> &sorted,
                                  const distance_histogram_t &dist_map_data)
{
    std::ios_base::fmtflags saved_flags(out.flags());
    double bin_multiplier = knobs_.histogram_bin_multiplier;
    assert(bin_multiplier >= 1.0);
    bool show_bin_range = bin_multiplier > 1.0;
    out << "Reuse distance histogram bin multiplier: " << bin_multiplier << "\n";
    out << "Reuse distance histogram:\n";
    std::string header_str =
        "                      All References       :            Data References";
    if (show_bin_range) {
        out << "           " << header_str << "\n";
        out << "Distance [min-max] ";
    } else {
        out << header_str << "\n";
        out << "Distance";
    }
    out << std::setw(12) << "Count"
        << "  Percent  Cumulative"
        << "  :       Count  Percent  Cumulative\n";
    int64_t max_distance = sorted.empty() ? 0 : sorted.back().first;
    double cum_percent = 0;
    double data_cum_percent = 0;
    int64_t bin_count = 0;
    int64_t data_bin_count = 0;
    int64_t bin_size = 1;
    double bin_size_float = 1.0;
    int64_t bin_start = 0;
    int64_t bin_next_start = bin_start + bin_size;
    for (auto it = sorted.begin(); it != sorted.end(); ++it) {
        const auto this_bin_number = it->first;
        auto data_it = dist_map_data.find(this_bin_number);
        int64_t this_bin_count = it->second;
        int64_t this_data_bin_count =
            data_it == dist_map_data.end() ? 0 : data_it->second;
        // The last bin needs to force an output.
        bool last_bin = *it == sorted.back();
        // If the new bin number is after the end of the current bin
        // range, output the prior bin info and update the bin range.
        // Repeat until the bin range includes the new bin.
        while (this_bin_number >= bin_next_start || last_bin) {
            if (last_bin && this_bin_number < bin_next_start) {
                bin_count += this_bin_count;
                data_bin_count += this_data_bin_count;
                last_bin = false;
            }
            double percent =
                total_count > 0 ? bin_count / static_cast<double>(total_count) : 0.0;
            double data_percent =
                total_count > 0 ? data_bin_count / static_cast<double>(total_count) : 0.0;
            cum_percent += percent;
            data_cum_percent += data_percent;
            // Don't output empty bins.
            if (bin_count > 0) {
                out << std::setw(8) << std::right << bin_start;
                if (show_bin_range) {
                    out << " - " << std::setw(8)
                        << std::min(max_distance, bin_next_start - 1);
                }
                out << std::setw(12) << bin_count << std::setw(8) << percent * 100. << "%"
                    << std::setw(8) << cum_percent * 100. << "%";
                out << "     : " << std::setw(11) << data_bin_count << std::setw(8)
                    << data_percent * 100. << "%" << std::setw(8)
                    << data_cum_percent * 100. << "%\n";
            }
            bin_count = 0;
            data_bin_count = 0;
            bin_start = bin_next_start;
            bin_size_float *= bin_multiplier;
            // Use floor() to favor smaller bin sizes.
            bin_size = static_cast<int64_t>(std::floor(bin_size_float));
            bin_next_start = bin_start + bin_size;
        }
        bin_count += this_bin_count;
        data_bin_count += this_data_bin_count;
    }
    out.flags(saved_flags);
}

const reuse_distance_t::shard_data_t *
reuse_distance_t::get_aggregated_results()
{
    // If the results have been aggregated already, just return a pointer.
    if (aggregated_results_)
        return aggregated_results_.get();

    // Otherwise, aggregate the per-shard data to get whole-trace data.
    aggregated_results_ = std::unique_ptr<shard_data_t>(
        new shard_data_t(knobs_.distance_threshold, knobs_.skip_list_distance,
                         knobs_.distance_limit, knobs_.verify_skip));
    for (auto &shard : shard_map_) {
        aggregated_results_->total_refs += shard.second->total_refs;
        aggregated_results_->data_refs += shard.second->data_refs;
        aggregated_results_->pruned_address_hits += shard.second->pruned_address_hits;
        aggregated_results_->pruned_address_count += shard.second->pruned_address_count;
        // We simply sum the unique accesses.
        // If the user wants the unique accesses over the merged trace they
        // can create a single shard and invoke the parallel operations.
        aggregated_results_->ref_list->cur_time_ += shard.second->ref_list->cur_time_;
        // We merge the pruned_addresses, histogram, and cache_map.
        for (const auto &entry : shard.second->pruned_addresses) {
            aggregated_results_->pruned_addresses.insert(entry);
        }
        // Merge dist_map_data with aggregated dist_map_data, and also
        // merge it into the shard's dist_map if it needs merging.
        bool shard_needs_merge = shard.second->dist_map_is_instr_only;
        for (const auto &entry : shard.second->dist_map_data) {
            aggregated_results_->dist_map_data[entry.first] += entry.second;
            if (shard_needs_merge) {
                shard.second->dist_map[entry.first] += entry.second;
            }
        }
        // If it didn't include data already, it does now.
        shard.second->dist_map_is_instr_only = false;
        // Merge the unified histogram data.
        for (const auto &entry : shard.second->dist_map) {
            aggregated_results_->dist_map[entry.first] += entry.second;
        }
        for (const auto &entry : shard.second->cache_map) {
            const auto &existing = aggregated_results_->cache_map.find(entry.first);
            line_ref_t *ref;
            if (existing == aggregated_results_->cache_map.end()) {
                ref = new line_ref_t(entry.first);
                aggregated_results_->cache_map.insert(
                    std::pair<addr_t, line_ref_t *>(entry.first, ref));
                ref->total_refs = 0;
            } else {
                ref = existing->second;
            }
            ref->total_refs += entry.second->total_refs;
            ref->distant_refs += entry.second->distant_refs;
        }
    }
    aggregated_results_->dist_map_is_instr_only = false;
    return aggregated_results_.get();
}

bool
reuse_distance_t::print_results()
{
    std::cerr << TOOL_NAME << " aggregated results:\n";
    print_shard_results(get_aggregated_results());

    // For regular shards the line_ref_t's are deleted in ~line_ref_list_t.
    for (auto &iter : get_aggregated_results()->cache_map) {
        delete iter.second;
    }

    if (shard_map_.size() > 1) {
        using keyval_t = std::pair<memref_tid_t, shard_data_t *>;
        std::vector<keyval_t> sorted(shard_map_.begin(), shard_map_.end());
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

    // Reset the i/o format for subsequent tool invocations.
    std::cerr << std::dec;
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio
