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

#include <iostream>
#include <iterator>
#include <string>
#include <assert.h>
#include <limits.h>
#include <stdint.h> /* for supporting 64-bit integers*/
#include "../common/memref.h"
#include "../common/options.h"
#include "../common/utils.h"
#include "../reader/file_reader.h"
#include "../reader/ipc_reader.h"
#include "cache_stats.h"
#include "cache.h"
#include "cache_lru.h"
#include "cache_fifo.h"
#include "cache_simulator.h"
#include "droption.h"

// XXX i#2006: making this a library means that these options or knobs are
// duplicated in multiple places as we pass them through the various layers.
// We lose the convenience of adding a new option and its default value
// in a single place.
// It is also error-prone to pass in this long list if we want to set
// just one option in the middle: should we switch to a struct of options
// where we can set by field name?
analysis_tool_t *
cache_simulator_create(unsigned int num_cores,
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
                       unsigned int verbose)
{
    return new cache_simulator_t(num_cores, line_size, L1I_size, L1D_size,
                                 L1I_assoc, L1D_assoc, LL_size, LL_assoc,
                                 replace_policy, data_prefetcher, skip_refs,warmup_refs,
                                 sim_refs, verbose);
}

cache_simulator_t::cache_simulator_t(unsigned int num_cores,
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
                                     unsigned int verbose) :
    simulator_t(num_cores, skip_refs,warmup_refs, sim_refs, verbose),
    knob_line_size(line_size),
    knob_L1I_size(L1I_size),
    knob_L1D_size(L1D_size),
    knob_L1I_assoc(L1I_assoc),
    knob_L1D_assoc(L1D_assoc),
    knob_LL_size(LL_size),
    knob_LL_assoc(LL_assoc),
    knob_replace_policy(replace_policy),
    knob_data_prefetcher(data_prefetcher)
{
    // XXX i#1703: get defaults from hardware being run on.

    llcache = create_cache(knob_replace_policy);
    if (llcache == NULL) {
        success = false;
        return;
    }

    if (data_prefetcher != PREFETCH_POLICY_NEXTLINE &&
        data_prefetcher != PREFETCH_POLICY_NONE) {
        // Unknown value.
        success = false;
        return;
    }

    if (!llcache->init(knob_LL_assoc, (int)knob_line_size,
                       (int)knob_LL_size, NULL, new cache_stats_t)) {
        ERRMSG("Usage error: failed to initialize LL cache.  Ensure sizes and "
               "associativity are powers of 2 "
               "and that the total size is a multiple of the line size.\n");
        success = false;
        return;
    }

    icaches = new cache_t* [knob_num_cores];
    dcaches = new cache_t* [knob_num_cores];
    for (int i = 0; i < knob_num_cores; i++) {
        icaches[i] = create_cache(knob_replace_policy);
        if (icaches[i] == NULL) {
            success = false;
            return;
        }
        dcaches[i] = create_cache(knob_replace_policy);
        if (dcaches[i] == NULL) {
            success = false;
            return;
        }

        if (!icaches[i]->init(knob_L1I_assoc, (int)knob_line_size,
                              (int)knob_L1I_size, llcache, new cache_stats_t) ||
            !dcaches[i]->init(knob_L1D_assoc, (int)knob_line_size,
                              (int)knob_L1D_size, llcache, new cache_stats_t,
                              data_prefetcher == PREFETCH_POLICY_NEXTLINE ?
                              new prefetcher_t((int)knob_line_size) : nullptr)) {
            ERRMSG("Usage error: failed to initialize L1 caches.  Ensure sizes and "
                   "associativity are powers of 2 "
                   "and that the total sizes are multiples of the line size.\n");
            success = false;
            return;
        }
    }

    thread_counts = new unsigned int[knob_num_cores];
    memset(thread_counts, 0, sizeof(thread_counts[0])*knob_num_cores);
    thread_ever_counts = new unsigned int[knob_num_cores];
    memset(thread_ever_counts, 0, sizeof(thread_ever_counts[0])*knob_num_cores);
}

cache_simulator_t::~cache_simulator_t()
{
    if (llcache == NULL)
        return;
    delete llcache->get_stats();
    delete llcache;
    for (int i = 0; i < knob_num_cores; i++) {
        // Try to handle failure during construction.
        if (icaches[i] == NULL)
            return;
        delete icaches[i]->get_stats();
        delete icaches[i];
        if (dcaches[i] == NULL)
            return;
        delete dcaches[i]->get_stats();
        delete dcaches[i];
    }
    delete [] icaches;
    delete [] dcaches;
    delete [] thread_counts;
    delete [] thread_ever_counts;
}

bool
cache_simulator_t::process_memref(const memref_t &memref)
{
    if (knob_skip_refs > 0) {
        knob_skip_refs--;
        return true;
    }

    // The references after warmup and simulated ones are dropped.
    if (knob_warmup_refs == 0 && knob_sim_refs == 0)
        return true;;

    // Both warmup and simulated references are simulated.

    // We use a static scheduling of threads to cores, as it is
    // not practical to measure which core each thread actually
    // ran on for each memref.
    int core;
    if (memref.data.tid == last_thread)
        core = last_core;
    else {
        core = core_for_thread(memref.data.tid);
        last_thread = memref.data.tid;
        last_core = core;
    }

    if (type_is_instr(memref.instr.type) ||
        memref.instr.type == TRACE_TYPE_PREFETCH_INSTR)
        icaches[core]->request(memref);
    else if (memref.data.type == TRACE_TYPE_READ ||
             memref.data.type == TRACE_TYPE_WRITE ||
             // We may potentially handle prefetches differently.
             // TRACE_TYPE_PREFETCH_INSTR is handled above.
             type_is_prefetch(memref.data.type))
        dcaches[core]->request(memref);
    else if (memref.flush.type == TRACE_TYPE_INSTR_FLUSH)
        icaches[core]->flush(memref);
    else if (memref.flush.type == TRACE_TYPE_DATA_FLUSH)
        dcaches[core]->flush(memref);
    else if (memref.exit.type == TRACE_TYPE_THREAD_EXIT) {
        handle_thread_exit(memref.exit.tid);
        last_thread = 0;
    } else {
        ERRMSG("unhandled memref type");
        return false;
    }

    if (knob_verbose >= 3) {
        std::cerr << "::" << memref.data.pid << "." << memref.data.tid << ":: " <<
            " @" << (void *)memref.data.pc <<
            " " << trace_type_names[memref.data.type] << " " <<
            (void *)memref.data.addr << " x" << memref.data.size << std::endl;
    }

    // process counters for warmup and simulated references
    if (knob_warmup_refs > 0) { // warm caches up
        knob_warmup_refs--;
        // reset cache stats when warming up is completed
        if (knob_warmup_refs == 0) {
            for (int i = 0; i < knob_num_cores; i++) {
                icaches[i]->get_stats()->reset();
                dcaches[i]->get_stats()->reset();
            }
            llcache->get_stats()->reset();
        }
    }
    else {
        knob_sim_refs--;
    }
    return true;
}

bool
cache_simulator_t::print_results()
{
    std::cerr << "Cache simulation results:\n";
    for (int i = 0; i < knob_num_cores; i++) {
        unsigned int threads = thread_ever_counts[i];
        std::cerr << "Core #" << i << " (" << threads << " thread(s))" << std::endl;
        if (threads > 0) {
            std::cerr << "  L1I stats:" << std::endl;
            icaches[i]->get_stats()->print_stats("    ");
            std::cerr << "  L1D stats:" << std::endl;
            dcaches[i]->get_stats()->print_stats("    ");
        }
    }
    std::cerr << "LL stats:" << std::endl;
    llcache->get_stats()->print_stats("    ");
    return true;
}

cache_t*
cache_simulator_t::create_cache(std::string policy)
{
    if (policy == REPLACE_POLICY_NON_SPECIFIED || // default LRU
        policy == REPLACE_POLICY_LRU) // set to LRU
        return new cache_lru_t;
    if (policy == REPLACE_POLICY_LFU) // set to LFU
        return new cache_t;
    if (policy == REPLACE_POLICY_FIFO) // set to FIFO
        return new cache_fifo_t;

    // undefined replacement policy
    ERRMSG("Usage error: undefined replacement policy. "
           "Please choose " REPLACE_POLICY_LRU" or " REPLACE_POLICY_LFU".\n");
    return NULL;
}
