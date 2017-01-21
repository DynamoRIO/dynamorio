/* **********************************************************
 * Copyright (c) 2015-2016 Google, Inc.  All rights reserved.
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

cache_simulator_t::cache_simulator_t()
{
    // XXX i#1703: get defaults from hardware being run on.

    num_cores = op_num_cores.get_value();

    llcache = create_cache(op_replace_policy.get_value());
    if (llcache == NULL) {
        success = false;
        return;
    }

    if (!llcache->init(op_LL_assoc.get_value(), (int)op_line_size.get_value(),
                       (int)op_LL_size.get_value(), NULL, new cache_stats_t)) {
        ERRMSG("Usage error: failed to initialize LL cache.  Ensure sizes and "
               "associativity are powers of 2 "
               "and that the total size is a multiple of the line size.\n");
        success = false;
        return;
    }

    icaches = new cache_t* [num_cores];
    dcaches = new cache_t* [num_cores];
    for (int i = 0; i < num_cores; i++) {
        icaches[i] = create_cache(op_replace_policy.get_value());
        if (icaches[i] == NULL) {
            success = false;
            return;
        }
        dcaches[i] = create_cache(op_replace_policy.get_value());
        if (dcaches[i] == NULL) {
            success = false;
            return;
        }

        if (!icaches[i]->init(op_L1I_assoc.get_value(), (int)op_line_size.get_value(),
                              (int)op_L1I_size.get_value(), llcache, new cache_stats_t) ||
            !dcaches[i]->init(op_L1D_assoc.get_value(), (int)op_line_size.get_value(),
                              (int)op_L1D_size.get_value(), llcache, new cache_stats_t)) {
            ERRMSG("Usage error: failed to initialize L1 caches.  Ensure sizes and "
                   "associativity are powers of 2 "
                   "and that the total sizes are multiples of the line size.\n");
            success = false;
            return;
        }
    }

    thread_counts = new unsigned int[num_cores];
    memset(thread_counts, 0, sizeof(thread_counts[0])*num_cores);
    thread_ever_counts = new unsigned int[num_cores];
    memset(thread_ever_counts, 0, sizeof(thread_ever_counts[0])*num_cores);
}

cache_simulator_t::~cache_simulator_t()
{
    if (llcache == NULL)
        return;
    delete llcache->get_stats();
    delete llcache;
    for (int i = 0; i < num_cores; i++) {
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
    if (skip_refs > 0) {
        skip_refs--;
        return true;
    }

    // The references after warmup and simulated ones are dropped.
    if (warmup_refs == 0 && sim_refs == 0)
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

    if (op_verbose.get_value() >= 3) {
        std::cerr << "::" << memref.data.pid << "." << memref.data.tid << ":: " <<
            " @" << (void *)memref.data.pc <<
            " " << trace_type_names[memref.data.type] << " " <<
            (void *)memref.data.addr << " x" << memref.data.size << std::endl;
    }

    // process counters for warmup and simulated references
    if (warmup_refs > 0) { // warm caches up
        warmup_refs--;
        // reset cache stats when warming up is completed
        if (warmup_refs == 0) {
            for (int i = 0; i < num_cores; i++) {
                icaches[i]->get_stats()->reset();
                dcaches[i]->get_stats()->reset();
            }
            llcache->get_stats()->reset();
        }
    }
    else {
        sim_refs--;
    }
    return true;
}

bool
cache_simulator_t::print_results()
{
    for (int i = 0; i < num_cores; i++) {
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
