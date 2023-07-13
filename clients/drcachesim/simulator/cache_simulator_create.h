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

/* cache simulator creation */

#ifndef _CACHE_SIMULATOR_CREATE_H_
#define _CACHE_SIMULATOR_CREATE_H_ 1

#include <string>
#include "analysis_tool.h"

namespace dynamorio {
namespace drmemtrace {
// NOCHECK does this order matter vs @file?
/**
 * @file drmemtrace/cache_simulator_create.h
 * @brief DrMemtrace cache simulator creation.
 */

/**
 * The options for cache_simulator_create().
 * The options are currently documented in \ref sec_drcachesim_ops.
 */
// The options are currently documented in ../common/options.cpp.
struct cache_simulator_knobs_t {
    cache_simulator_knobs_t()
        : num_cores(4)
        , line_size(64)
        , L1I_size(32 * 1024U)
        , L1D_size(32 * 1024U)
        , L1I_assoc(8)
        , L1D_assoc(8)
        , LL_size(8 * 1024 * 1024)
        , LL_assoc(16)
        , LL_miss_file("")
        , model_coherence(false)
        , replace_policy("LRU")
        , data_prefetcher("nextline")
        , skip_refs(0)
        , warmup_refs(0)
        , warmup_fraction(0.0)
        , sim_refs(1ULL << 63)
        , cpu_scheduling(false)
        , use_physical(false)
        , verbose(0)
    {
    }
    unsigned int num_cores;
    unsigned int line_size;
    uint64_t L1I_size;
    uint64_t L1D_size;
    unsigned int L1I_assoc;
    unsigned int L1D_assoc;
    uint64_t LL_size;
    unsigned int LL_assoc;
    std::string LL_miss_file;
    bool model_coherence;
    std::string replace_policy;
    std::string data_prefetcher;
    uint64_t skip_refs;
    uint64_t warmup_refs;
    double warmup_fraction;
    uint64_t sim_refs;
    bool cpu_scheduling;
    bool use_physical;
    unsigned int verbose;
};

/** Creates an instance of a cache simulator with a 2-level hierarchy. */
analysis_tool_t *
cache_simulator_create(const cache_simulator_knobs_t &knobs);

/**
 * Creates an instance of a cache simulator using a cache hierarchy defined
 * in a configuration file.
 */
analysis_tool_t *
cache_simulator_create(const std::string &config_file);

/** Creates an instance of a cache miss analyzer. */
analysis_tool_t *
cache_miss_analyzer_create(const cache_simulator_knobs_t &knobs,
                           unsigned int miss_count_threshold, double miss_frac_threshold,
                           double confidence_threshold);

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _CACHE_SIMULATOR_CREATE_H_ */
