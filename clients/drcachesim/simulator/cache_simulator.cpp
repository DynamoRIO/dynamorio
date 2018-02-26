/* **********************************************************
 * Copyright (c) 2015-2018 Google, Inc.  All rights reserved.
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

analysis_tool_t *
cache_simulator_create(const cache_simulator_knobs_t &knobs)
{
    return new cache_simulator_t(knobs);
}

cache_simulator_t::cache_simulator_t(const cache_simulator_knobs_t &knobs_) :
    simulator_t(knobs_.num_cores, knobs_.skip_refs, knobs_.warmup_refs,
                knobs_.warmup_fraction, knobs_.sim_refs, knobs_.verbose),
    knobs(knobs_),
    icaches(NULL),
    dcaches(NULL),
    is_warmed_up(false)
{
    // XXX i#1703: get defaults from hardware being run on.

    llcache = create_cache(knobs.replace_policy);
    if (llcache == NULL) {
        success = false;
        return;
    }

    if (knobs.data_prefetcher != PREFETCH_POLICY_NEXTLINE &&
        knobs.data_prefetcher != PREFETCH_POLICY_NONE) {
        // Unknown value.
        success = false;
        return;
    }

    bool warmup_enabled = ((knobs.warmup_refs > 0) || (knobs.warmup_fraction > 0.0));

    if (!llcache->init(knobs.LL_assoc, (int)knobs.line_size,
                       (int)knobs.LL_size, NULL,
                       new cache_stats_t(knobs.LL_miss_file, warmup_enabled))) {
        ERRMSG("Usage error: failed to initialize LL cache.  Ensure sizes and "
               "associativity are powers of 2, that the total size is a multiple "
               "of the line size, and that any miss file path is writable.\n");
        success = false;
        return;
    }

    icaches = new cache_t* [knobs.num_cores];
    dcaches = new cache_t* [knobs.num_cores];
    for (unsigned int i = 0; i < knobs.num_cores; i++) {
        icaches[i] = create_cache(knobs.replace_policy);
        if (icaches[i] == NULL) {
            success = false;
            return;
        }
        dcaches[i] = create_cache(knobs.replace_policy);
        if (dcaches[i] == NULL) {
            success = false;
            return;
        }

        if (!icaches[i]->init(knobs.L1I_assoc, (int)knobs.line_size,
                              (int)knobs.L1I_size, llcache,
                              new cache_stats_t("", warmup_enabled)) ||
            !dcaches[i]->init(knobs.L1D_assoc, (int)knobs.line_size,
                              (int)knobs.L1D_size, llcache,
                              new cache_stats_t("", warmup_enabled),
                              knobs.data_prefetcher == PREFETCH_POLICY_NEXTLINE ?
                              new prefetcher_t((int)knobs.line_size) : nullptr)) {
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
    delete llcache->get_prefetcher();
    delete llcache;
    if (icaches == NULL)
        return;
    for (unsigned int i = 0; i < knobs.num_cores; i++) {
        // Try to handle failure during construction.
        if (icaches[i] == NULL)
            return;
        delete icaches[i]->get_stats();
        delete icaches[i]->get_prefetcher();
        delete icaches[i];
        if (dcaches[i] == NULL)
            return;
        delete dcaches[i]->get_stats();
        delete dcaches[i]->get_prefetcher();
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
    if (knobs.skip_refs > 0) {
        knobs.skip_refs--;
        return true;
    }

    // The references after warmup and simulated ones are dropped.
    if (check_warmed_up() && knobs.sim_refs == 0)
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
        memref.instr.type == TRACE_TYPE_PREFETCH_INSTR) {
        if (knobs.verbose >= 3) {
            std::cerr << "::" << memref.data.pid << "." << memref.data.tid << ":: " <<
                " @" << (void *)memref.instr.addr << " instr x" <<
                memref.instr.size << "\n";
        }
        icaches[core]->request(memref);
    } else if (memref.data.type == TRACE_TYPE_READ ||
               memref.data.type == TRACE_TYPE_WRITE ||
               // We may potentially handle prefetches differently.
               // TRACE_TYPE_PREFETCH_INSTR is handled above.
               type_is_prefetch(memref.data.type)) {
        if (knobs.verbose >= 3) {
            std::cerr << "::" << memref.data.pid << "." << memref.data.tid << ":: " <<
                " @" << (void *)memref.data.pc <<
                " " << trace_type_names[memref.data.type] << " " <<
                (void *)memref.data.addr << " x" << memref.data.size << "\n";
        }
        dcaches[core]->request(memref);
    } else if (memref.flush.type == TRACE_TYPE_INSTR_FLUSH) {
        if (knobs.verbose >= 3) {
            std::cerr << "::" << memref.data.pid << "." << memref.data.tid << ":: " <<
                " @" << (void *)memref.data.pc << " iflush " <<
                (void *)memref.data.addr << " x" << memref.data.size << "\n";
        }
        icaches[core]->flush(memref);
    } else if (memref.flush.type == TRACE_TYPE_DATA_FLUSH) {
        if (knobs.verbose >= 3) {
            std::cerr << "::" << memref.data.pid << "." << memref.data.tid << ":: " <<
                " @" << (void *)memref.data.pc << " dflush " <<
                (void *)memref.data.addr << " x" << memref.data.size << "\n";
        }
        dcaches[core]->flush(memref);
    } else if (memref.exit.type == TRACE_TYPE_THREAD_EXIT) {
        handle_thread_exit(memref.exit.tid);
        last_thread = 0;
    } else if (memref.marker.type == TRACE_TYPE_MARKER) {
        if (knobs.verbose >= 3) {
            std::cerr << "::" << memref.data.pid << "." << memref.data.tid << ":: " <<
                "marker type " << memref.marker.marker_type <<
                " value " << memref.marker.marker_value << "\n";
        }
    } else if (memref.marker.type == TRACE_TYPE_INSTR_NO_FETCH) {
        // Just ignore.
        if (knobs.verbose >= 3) {
            std::cerr << "::" << memref.data.pid << "." << memref.data.tid << ":: " <<
                " @" << (void *)memref.instr.addr << " non-fetched instr x" <<
                memref.instr.size << "\n";
        }
    } else {
        ERRMSG("unhandled memref type");
        return false;
    }

    // reset cache stats when warming up is completed
    if (!is_warmed_up && check_warmed_up()) {
        for (unsigned int i = 0; i < knobs.num_cores; i++) {
            icaches[i]->get_stats()->reset();
            dcaches[i]->get_stats()->reset();
        }
        llcache->get_stats()->reset();
        if (knobs.verbose >= 1) {
            std::cerr << "Cache simulation warmed up\n";
        }
    } else {
        knobs.sim_refs--;
    }
    return true;
}

// Return true if the number of warmup references have been executed or if
// specified fraction of the llcache has been loaded. Also return true if the
// cache has already been warmed up.
bool
cache_simulator_t::check_warmed_up()
{
    // If the cache has already been warmed up return true
    if (is_warmed_up)
        return true;

    // If the warmup_fraction option is set then check if the last level has
    // loaded enough data to be warmed up.
    if (knobs.warmup_fraction > 0.0 &&
        llcache->get_loaded_fraction() > knobs.warmup_fraction) {
        is_warmed_up = true;
        return true;
    }

    // If warmup_refs is set then decrement and indicate warmup done when
    // counter hits zero.
    if (knobs.warmup_refs > 0) {
        knobs.warmup_refs--;
        if (knobs.warmup_refs == 0) {
            is_warmed_up = true;
            return true;
        }
    }

    // If we reach here then warmup is not done.
    return false;
}

bool
cache_simulator_t::print_results()
{
    std::cerr << "Cache simulation results:\n";
    for (unsigned int i = 0; i < knobs.num_cores; i++) {
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
