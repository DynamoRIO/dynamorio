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
#include "utils.h"
#include "memref.h"
#include "ipc_reader.h"
#include "cache_stats.h"
#include "cache.h"
#include "cache_lru.h"
#include "droption.h"
#include "../common/options.h"
#include "simulator.h"

bool
simulator_t::init()
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

    if (op_replace_lru.get_value())
        llcache = new cache_lru_t;
    else // default LFU
        llcache = new cache_t;

    if (!llcache->init(op_LL_assoc.get_value(), op_line_size.get_value(),
                       op_LL_size.get_value(), NULL, new cache_stats_t)) {
        ERROR("Usage error: failed to initialize LL cache.  Ensure sizes and "
              "associativity are powers of 2 "
              "and that the total size is a multiple of the line size.\n");
        return false;
    }
    if (op_replace_lru.get_value()) {
        icaches = new cache_lru_t[num_cores];
        dcaches = new cache_lru_t[num_cores];
    } else {
        icaches = new cache_t[num_cores];
        dcaches = new cache_t[num_cores];
    }
    for (int i = 0; i < num_cores; i++) {
        if (!icaches[i].init(op_L1I_assoc.get_value(), op_line_size.get_value(),
                             op_L1I_size.get_value(), llcache, new cache_stats_t) ||
            !dcaches[i].init(op_L1D_assoc.get_value(), op_line_size.get_value(),
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

simulator_t::~simulator_t()
{
    delete llcache->get_stats();
    for (int i = 0; i < num_cores; i++) {
        delete icaches[i].get_stats();
        delete dcaches[i].get_stats();
    }
    delete [] icaches;
    delete [] dcaches;
    delete [] thread_counts;
    delete [] thread_ever_counts;
}

int
simulator_t::core_for_thread(memref_tid_t tid)
{
    std::map<memref_tid_t,int>::iterator exists = thread2core.find(tid);
    if (exists != thread2core.end())
        return exists->second;
    // A new thread: we want to assign it to the least-loaded core,
    // measured just by the number of threads.
    // We assume the # of cores is small and that it's fastest to do a
    // linear search versus maintaining some kind of sorted data
    // structure.
    unsigned int min_count = UINT_MAX;
    int min_core = 0;
    for (int i = 0; i < num_cores; i++) {
        if (thread_counts[i] < min_count) {
            min_count = thread_counts[i];
            min_core = i;
        }
    }
    if (op_verbose.get_value() >= 1) {
        std::cerr << "new thread " << tid << " => core " << min_core <<
            " (count=" << thread_counts[min_core] << ")" << std::endl;
    }
    ++thread_counts[min_core];
    ++thread_ever_counts[min_core];
    thread2core[tid] = min_core;
    return min_core;
}

void
simulator_t::handle_thread_exit(memref_tid_t tid)
{
    std::map<memref_tid_t,int>::iterator exists = thread2core.find(tid);
    assert(exists != thread2core.end());
    assert(thread_counts[exists->second] > 0);
    --thread_counts[exists->second];
    if (op_verbose.get_value() >= 1) {
        std::cerr << "thread " << tid << " exited from core " << exists->second <<
            " (count=" << thread_counts[exists->second] << ")" << std::endl;
    }
    thread2core.erase(tid);
}

bool
simulator_t::run()
{
    if (!ipc_iter.init()) {
        ERROR("failed to read from pipe %s", op_ipc_name.get_value().c_str());
        return false;
    }
    memref_tid_t last_thread = 0;
    int last_core = 0;

    // XXX i#1703: add options to select either ipc_reader_t or
    // a recorded trace file reader, and use a base class reader_t
    // here.
    for (; ipc_iter != ipc_end; ++ipc_iter) {
        memref_t memref = *ipc_iter;

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
            icaches[core].request(memref);
        else if (memref.type == TRACE_TYPE_READ ||
                 memref.type == TRACE_TYPE_WRITE ||
                 // We may potentially handle prefetches differently.
                 // TRACE_TYPE_PREFETCH_INSTR is handled above.
                 type_is_prefetch(memref.type))
            dcaches[core].request(memref);
        else if (memref.type == TRACE_TYPE_INSTR_FLUSH)
            icaches[core].flush(memref);
        else if (memref.type == TRACE_TYPE_DATA_FLUSH)
            dcaches[core].flush(memref);
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
    }
    return true;
}

bool
simulator_t::print_stats()
{
    for (int i = 0; i < num_cores; i++) {
        unsigned int threads = thread_ever_counts[i];
        std::cerr << "Core #" << i << " (" << threads << " thread(s))" << std::endl;
        if (threads > 0) {
            std::cerr << "  L1I stats:" << std::endl;
            icaches[i].get_stats()->print_stats("    ");
            std::cerr << "  L1D stats:" << std::endl;
            dcaches[i].get_stats()->print_stats("    ");
        }
    }
    std::cerr << "LL stats:" << std::endl;
    llcache->get_stats()->print_stats("    ");
    return true;
}
