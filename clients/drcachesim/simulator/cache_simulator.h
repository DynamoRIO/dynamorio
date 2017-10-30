/* **********************************************************
 * Copyright (c) 2015-2017 Google, Inc.  All rights reserved.
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

/* cache_simulator: controls the multi-cache-level simulation.
 */

#ifndef _CACHE_SIMULATOR_H_
#define _CACHE_SIMULATOR_H_ 1

#include <unordered_map>
#include "simulator.h"
#include "cache_stats.h"
#include "cache.h"

class cache_simulator_t : public simulator_t
{
 public:
    cache_simulator_t(unsigned int num_cores,
                      unsigned int line_size,
                      uint64_t L1I_size,
                      uint64_t L1D_size,
                      unsigned int L1I_assoc,
                      unsigned int L1D_assoc,
                      uint64_t LL_size,
                      unsigned int LL_assoc,
                      std::string replace_policy,
                      std::string data_prefetcher,
                      uint64_t skip_refs,
                      uint64_t warmup_refs,
                      uint64_t sim_refs,
                      unsigned int verbose);
    virtual ~cache_simulator_t();
    virtual bool process_memref(const memref_t &memref);
    virtual bool print_results();

 protected:
    // Create a cache_t object with a specific replacement policy.
    virtual cache_t *create_cache(std::string policy);

    // Currently we only support a simple 2-level hierarchy.
    // XXX i#1715: add support for arbitrary cache layouts.

    unsigned int knob_line_size;
    uint64_t knob_L1I_size;
    uint64_t knob_L1D_size;
    unsigned int knob_L1I_assoc;
    unsigned int knob_L1D_assoc;
    uint64_t knob_LL_size;
    unsigned int knob_LL_assoc;
    std::string knob_replace_policy;
    std::string knob_data_prefetcher;

    // Implement a set of ICaches and DCaches with pointer arrays.
    // This is useful for implementing polymorphism correctly.
    cache_t **icaches;
    cache_t **dcaches;

    cache_t *llcache;

};

#endif /* _CACHE_SIMULATOR_H_ */
