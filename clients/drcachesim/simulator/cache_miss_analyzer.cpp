/* **********************************************************
 * Copyright (c) 2015-2023 Google, LLC  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, LLC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "cache_miss_analyzer.h"

#include <iostream>
#include <stdint.h>

namespace dynamorio {
namespace drmemtrace {

const char *cache_miss_stats_t::kNTA = "nta";
const char *cache_miss_stats_t::kT0 = "t0";

analysis_tool_t *
cache_miss_analyzer_create(const cache_simulator_knobs_t &knobs,
                           unsigned int miss_count_threshold, double miss_frac_threshold,
                           double confidence_threshold)
{
    return new cache_miss_analyzer_t(knobs, miss_count_threshold, miss_frac_threshold,
                                     confidence_threshold);
}

cache_miss_stats_t::cache_miss_stats_t(bool warmup_enabled, unsigned int line_size,
                                       unsigned int miss_count_threshold,
                                       double miss_frac_threshold,
                                       double confidence_threshold)
    : cache_stats_t(line_size, "", warmup_enabled, false)
    , kLineSize(line_size)
    , kMissCountThreshold(miss_count_threshold)
    , kMissFracThreshold(miss_frac_threshold)
    , kConfidenceThreshold(confidence_threshold)
{
    // Setting this variable to true ensures that the dump_miss() function below
    // gets called during cache simulation on a cache miss.
    dump_misses_ = true;
}

void
cache_miss_stats_t::reset()
{
    cache_stats_t::reset();
    pc_cache_misses_.clear();
    total_misses_ = 0;
}

void
cache_miss_stats_t::dump_miss(const memref_t &memref)
{
    // If the operation causing the LLC miss is a memory read (load), insert
    // the miss into the pc_cache_misses_ hash map and update
    // the total_misses_ counter.
    if (memref.data.type != TRACE_TYPE_READ) {
        return;
    }

    const addr_t pc = memref.data.pc;
    const addr_t addr = memref.data.addr / kLineSize;
    pc_cache_misses_[pc].push_back(addr);
    total_misses_++;
}

std::vector<prefetching_recommendation_t *>
cache_miss_stats_t::generate_recommendations()
{
    unsigned int miss_count_threshold =
        static_cast<unsigned int>(kMissFracThreshold * total_misses_);
    if (miss_count_threshold > kMissCountThreshold) {
        miss_count_threshold = kMissCountThreshold;
    }

    // Find loads that should be analyzed and analyze them.
    std::vector<prefetching_recommendation_t *> recommendations;
    for (auto &pc_cache_misses_it : pc_cache_misses_) {
        std::vector<addr_t> &cache_misses = pc_cache_misses_it.second;

        if (cache_misses.size() >= miss_count_threshold) {
            const int stride = check_for_constant_stride(cache_misses);
            if (stride != 0) {
                prefetching_recommendation_t *recommendation =
                    new prefetching_recommendation_t;
                recommendation->pc = pc_cache_misses_it.first;
                recommendation->stride = stride;
                recommendation->locality = kNTA;
                recommendations.push_back(recommendation);
            }
        }
    }

    return recommendations;
}

int
cache_miss_stats_t::check_for_constant_stride(
    const std::vector<addr_t> &cache_misses) const
{
    std::unordered_map<int, int> stride_counts;

    // Find and count all strides in the misses stream.
    for (unsigned int i = 1; i < cache_misses.size(); ++i) {
        int stride = static_cast<int>(cache_misses[i] - cache_misses[i - 1]);
        if (stride != 0) {
            stride_counts[stride]++;
        }
    }

    // Find the most occurring stride.
    int max_count = 0;
    int max_count_stride = 0;
    for (auto &stride_count : stride_counts) {
        if (stride_count.second > max_count) {
            max_count = stride_count.second;
            max_count_stride = stride_count.first;
        }
    }

    // Return the most occurring stride if it meets the confidence threshold.
    stride_counts.clear();
    if (max_count >= static_cast<int>(kConfidenceThreshold * cache_misses.size())) {
        return max_count_stride * kLineSize;
    } else {
        return 0;
    }
}

cache_miss_analyzer_t::cache_miss_analyzer_t(const cache_simulator_knobs_t &knobs,
                                             unsigned int miss_count_threshold,
                                             double miss_frac_threshold,
                                             double confidence_threshold)
    : cache_simulator_t(knobs)
{
    if (!success_) {
        return;
    }
    bool warmup_enabled_ = (knobs.warmup_refs > 0 || knobs.warmup_fraction > 0.0);

    delete llcaches_["LL"]->get_stats();
    ll_stats_ =
        new cache_miss_stats_t(warmup_enabled_, knobs.line_size, miss_count_threshold,
                               miss_frac_threshold, confidence_threshold);
    llcaches_["LL"]->set_stats(ll_stats_);

    if (!knobs.LL_miss_file.empty()) {
        recommendation_file_ = knobs.LL_miss_file;
    }
}

std::vector<prefetching_recommendation_t *>
cache_miss_analyzer_t::generate_recommendations()
{
    return ll_stats_->generate_recommendations();
}

bool
cache_miss_analyzer_t::print_results()
{
    std::vector<prefetching_recommendation_t *> recommendations =
        ll_stats_->generate_recommendations();

    FILE *file = nullptr;
    const bool write_to_file = !recommendation_file_.empty();
    if (write_to_file) {
        file = fopen(recommendation_file_.c_str(), "w");
    }

    std::cerr << "Cache miss analyzer results:\n";
    for (auto &recommendation : recommendations) {
        std::cerr << "pc=0x" << std::hex << recommendation->pc << std::dec
                  << ", stride=" << recommendation->stride
                  << ", locality=" << recommendation->locality << std::endl;

        if (write_to_file) {
            fprintf(file, "0x%lx,%d,%s\n", static_cast<unsigned long>(recommendation->pc),
                    recommendation->stride, recommendation->locality.c_str());
        }
    }

    if (write_to_file) {
        fclose(file);
    }

    // Reset the i/o format for subsequent tool invocations.
    std::cerr << std::dec;
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio
