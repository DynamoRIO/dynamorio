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

/* cache_miss_analyzer: finds the load instructions suffering from
 * a significant number of last-level cache (LLC) misses. In addition,
 * it analyzes the data memory addresses accessed by these load instructions
 * and identifies patterns that can be used in SW prefetching.
 */

#ifndef _CACHE_MISS_ANALYZER_H_
#define _CACHE_MISS_ANALYZER_H_ 1

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "cache_simulator.h"
#include "cache_stats.h"
#include "../common/memref.h"

namespace dynamorio {
namespace drmemtrace {

// Represents the SW prefetching recommendation passed to the compiler.
struct prefetching_recommendation_t {
    addr_t pc;            // Load instruction's address.
    int stride;           // Prefetching stride/delta distance.
    std::string locality; // Prefetching locality: one of "nta" or "t0".
};

class cache_miss_stats_t : public cache_stats_t {
public:
    // Constructor - params description:
    // - warmup_enabled: Indicates whether the caches need to be warmed up
    //                   before stats and misses start being collected.
    // - line_size: The cache line size in bytes.
    // - miss_count_threshold: Threshold of misses count by a load instruction
    //                         to be eligible for analysis.
    // - miss_frac_threshold: Threshold of misses fraction by a load
    //                        instruction to be eligible for analysis.
    // - confidence_threshold: Confidence threshold to include a discovered
    //                         pattern in the output results.
    // Confidence in a discovered pattern for a load instruction is calculated
    // as the fraction of the load's misses with the discovered pattern over
    // all the load's misses.
    cache_miss_stats_t(bool warmup_enabled = false, unsigned int line_size = 64,
                       unsigned int miss_count_threshold = 50000,
                       double miss_frac_threshold = 0.005,
                       double confidence_threshold = 0.75);

    cache_miss_stats_t &
    operator=(const cache_miss_stats_t &)
    {
        return *this;
    }

    void
    reset() override;

    std::vector<prefetching_recommendation_t *>
    generate_recommendations();

protected:
    void
    dump_miss(const memref_t &memref) override;

private:
    // Two locality levels for prefetching are supported: nta and t0.
    static const char *kNTA;
    static const char *kT0;

    // Cache line size.
    const unsigned int kLineSize;

    // A load instruction should be analyzed if its total number/fraction of LLC
    // misses is equal to or larger than one of the two threshold values below:
    const unsigned int kMissCountThreshold; // Absolute count.
    const double kMissFracThreshold;        // Fraction of all LLC misses.

    // Confidence threshold for recording a cache misses stride.
    // Confidence in a discovered pattern for a load instruction is calculated
    // as the fraction of the load's misses with the discovered pattern over
    // all the load's misses.
    const double kConfidenceThreshold;

    // A function to analyze cache misses in search of a constant stride.
    // The function returns a nonzero stride value if it finds one that
    // satisfies the confidence threshold and returns 0 otherwise.
    int
    check_for_constant_stride(const std::vector<addr_t> &cache_misses) const;

    // A hash map storing the data cache line addresses accessed by load
    // instructions that miss in the LLC.
    // Key is the PC of the load instruction.
    // Value is a vector of data memory cache line addresses.
    std::unordered_map<addr_t, std::vector<addr_t>> pc_cache_misses_;

    // Total number of LLC misses added to the hash map above.
    int total_misses_ = 0;
};

class cache_miss_analyzer_t : public cache_simulator_t {
public:
    // Constructor:
    // - cache_simulator_knobs_t: Encapsulates the cache simulator params.
    // - miss_count_threshold: Threshold of miss count by a load instruction
    //                         to be eligible for analysis.
    // - miss_frac_threshold: Threshold of miss fraction by a load
    //                        instruction to be eligible for analysis.
    // - confidence_threshold: Confidence threshold to include a discovered
    //                         pattern in the output results.
    // Confidence in a discovered pattern for a load instruction is calculated
    // as the fraction of the load's misses with the discovered pattern over
    // all the load's misses.
    cache_miss_analyzer_t(const cache_simulator_knobs_t &knobs,
                          unsigned int miss_count_threshold = 50000,
                          double miss_frac_threshold = 0.005,
                          double confidence_threshold = 0.75);

    std::vector<prefetching_recommendation_t *>
    generate_recommendations();

    bool
    print_results() override;

private:
    cache_miss_stats_t *ll_stats_;

    // Recommendations are written to this file for use by the LLVM compiler.
    std::string recommendation_file_ = "";
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _CACHE_MISS_ANALYZER_H_ */
