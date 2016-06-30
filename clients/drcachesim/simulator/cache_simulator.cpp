/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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
#include "utils.h"
#include "memref.h"
#include "ipc_reader.h"
#include "cache_stats.h"
#include "cache.h"
#include "cache_lru.h"
#include "cache_fifo.h"
#include "droption.h"
#include "../common/options.h"
#include "cache_simulator.h"

bool
cache_simulator_t::init()
{
    // XXX: add a "required" flag to droption to avoid needing this here
    if (op_ipc_name.get_value().empty()) {
        ERROR("Usage error: ipc name is required\nUsage:\n%s",
              droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        return false;
    }
    ipc_iter = ipc_reader_t(op_ipc_name.get_value().c_str());

    // XXX i#1703: get defaults from hardware being run on.

    num_cores = op_num_cores.get_value();

    llcache = create_cache(op_replace_policy.get_value());
    if (llcache == NULL)
        return false;

    if (!llcache->init(op_LL_assoc.get_value(), op_line_size.get_value(),
                       op_LL_size.get_value(), NULL, new cache_stats_t)) {
        ERROR("Usage error: failed to initialize LL cache.  Ensure sizes and "
              "associativity are powers of 2 "
              "and that the total size is a multiple of the line size.\n");
        return false;
    }

    icaches = new cache_t* [num_cores];
    dcaches = new cache_t* [num_cores];
    for (int i = 0; i < num_cores; i++) {
        icaches[i] = create_cache(op_replace_policy.get_value());
        if (icaches[i] == NULL)
            return false;
        dcaches[i] = create_cache(op_replace_policy.get_value());
        if (dcaches[i] == NULL)
            return false;

        if (!icaches[i]->init(op_L1I_assoc.get_value(), op_line_size.get_value(),
                              op_L1I_size.get_value(), llcache, new cache_stats_t) ||
            !dcaches[i]->init(op_L1D_assoc.get_value(), op_line_size.get_value(),
                              op_L1D_size.get_value(), llcache, new cache_stats_t)) {
            ERROR("Usage error: failed to initialize L1 caches.  Ensure sizes and "
                  "associativity are powers of 2 "
                  "and that the total sizes are multiples of the line size.\n");
            return false;
        }
    }

    thread_counts = new unsigned int[num_cores];
    memset(thread_counts, 0, sizeof(thread_counts[0])*num_cores);
    thread_ever_counts = new unsigned int[num_cores];
    memset(thread_ever_counts, 0, sizeof(thread_ever_counts[0])*num_cores);

    return true;
}

cache_simulator_t::~cache_simulator_t()
{
    delete llcache->get_stats();
    for (int i = 0; i < num_cores; i++) {
        delete icaches[i]->get_stats();
        delete dcaches[i]->get_stats();
        delete icaches[i];
        delete dcaches[i];
    }
    delete [] icaches;
    delete [] dcaches;
    delete [] thread_counts;
    delete [] thread_ever_counts;
}

bool
cache_simulator_t::run()
{
    if (!ipc_iter.init()) {
        ERROR("failed to read from pipe %s", op_ipc_name.get_value().c_str());
        return false;
    }
    memref_tid_t last_thread = 0;
    int last_core = 0;

    uint64_t skip_refs = op_skip_refs.get_value();
    uint64_t warmup_refs = op_warmup_refs.get_value();
    uint64_t sim_refs = op_sim_refs.get_value();

    // XXX i#1703: add options to select either ipc_reader_t or
    // a recorded trace file reader, and use a base class reader_t
    // here.
    for (; ipc_iter != ipc_end; ++ipc_iter) {
        memref_t memref = *ipc_iter;
        if (skip_refs > 0) {
            skip_refs--;
            continue;
        }

        // the references after warmup and simulated ones are dropped
        if (warmup_refs == 0 && sim_refs == 0)
            continue;

        // both warmup and simulated references are simulated

        // We use a static scheduling of threads to cores, as it is
        // not practical to measure which core each thread actually
        // ran on for each memref.
        int core;
        if (memref.tid == last_thread)
            core = last_core;
        else {
            core = core_for_thread(memref.tid);
            last_thread = memref.tid;
            last_core = core;
        }

        if (memref.type == TRACE_TYPE_INSTR ||
            memref.type == TRACE_TYPE_PREFETCH_INSTR)
            icaches[core]->request(memref);
        else if (memref.type == TRACE_TYPE_READ ||
                 memref.type == TRACE_TYPE_WRITE ||
                 // We may potentially handle prefetches differently.
                 // TRACE_TYPE_PREFETCH_INSTR is handled above.
                 type_is_prefetch(memref.type))
            dcaches[core]->request(memref);
        else if (memref.type == TRACE_TYPE_INSTR_FLUSH)
            icaches[core]->flush(memref);
        else if (memref.type == TRACE_TYPE_DATA_FLUSH)
            dcaches[core]->flush(memref);
        else if (memref.type == TRACE_TYPE_THREAD_EXIT) {
            handle_thread_exit(memref.tid);
            last_thread = 0;
        } else {
            ERROR("unhandled memref type");
            return false;
        }

        if (op_verbose.get_value() >= 3) {
            std::cerr << "::" << memref.pid << "." << memref.tid << ":: " <<
                " @" << (void *)memref.pc <<
                " " << trace_type_names[memref.type] << " " <<
                (void *)memref.addr << " x" << memref.size << std::endl;
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
    }
    return true;
}

bool
cache_simulator_t::print_stats()
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
    ERROR("Usage error: undefined replacement policy. "
          "Please choose " REPLACE_POLICY_LRU" or " REPLACE_POLICY_LFU".\n");
    return NULL;
}
