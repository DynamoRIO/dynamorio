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
#include <assert.h>
#include <limits.h>
#include "../common/memref.h"
#include "../common/options.h"
#include "../common/utils.h"
#include "droption.h"
#include "simulator.h"

simulator_t::simulator_t(unsigned int num_cores, uint64_t skip_refs, uint64_t warmup_refs,
                         double warmup_fraction, uint64_t sim_refs, bool cpu_scheduling,
                         unsigned int verbose)
{
    init_knobs(num_cores, skip_refs, warmup_refs, warmup_fraction, sim_refs,
               cpu_scheduling, verbose);
}

simulator_t::~simulator_t()
{
}

void
simulator_t::init_knobs(unsigned int num_cores, uint64_t skip_refs, uint64_t warmup_refs,
                        double warmup_fraction, uint64_t sim_refs, bool cpu_scheduling,
                        unsigned int verbose)
{
    knob_num_cores = num_cores;
    knob_skip_refs = skip_refs;
    knob_warmup_refs = warmup_refs;
    knob_warmup_fraction = warmup_fraction;
    knob_sim_refs = sim_refs;
    knob_cpu_scheduling = cpu_scheduling;
    knob_verbose = verbose;
    last_thread = 0;
    last_core = 0;
    cpu_counts.resize(knob_num_cores, 0);
    thread_counts.resize(knob_num_cores, 0);
    thread_ever_counts.resize(knob_num_cores, 0);

    if (knob_warmup_refs > 0 && (knob_warmup_fraction > 0.0)) {
        ERRMSG("Usage error: Either warmup_refs OR warmup_fraction can be set");
        success = false;
        return;
    }
}

bool
simulator_t::process_memref(const memref_t &memref)
{
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID && knob_cpu_scheduling) {
        int cpu = (int)(intptr_t)memref.marker.marker_value;
        if (cpu < 0)
            return true;
        int min_core;
        auto exists = cpu2core.find(cpu);
        if (exists == cpu2core.end()) {
            min_core = find_emptiest_core(cpu_counts);
            ++cpu_counts[min_core];
            cpu2core[cpu] = min_core;
            if (knob_verbose >= 1) {
                std::cerr << "new cpu " << cpu << " => core " << min_core
                          << " (count=" << cpu_counts[min_core] << ")" << std::endl;
            }
        } else
            min_core = exists->second;
        auto prior = thread2core.find(memref.marker.tid);
        if (prior != thread2core.end())
            --thread_counts[prior->second];
        thread2core[memref.marker.tid] = min_core;
        ++thread_counts[min_core];
        ++thread_ever_counts[min_core];
    }
    return true;
}

int
simulator_t::find_emptiest_core(std::vector<int> &counts) const
{
    // We want to assign to the least-loaded core, measured just by
    // the number of cpus or threads already there.  We assume the #
    // of cores is small and that it's fastest to do a linear search
    // versus maintaining some kind of sorted data structure.
    int min_count = INT_MAX;
    int min_core = 0;
    for (unsigned int i = 0; i < knob_num_cores; i++) {
        if (counts[i] < min_count) {
            min_count = counts[i];
            min_core = i;
        }
    }
    return min_core;
}

int
simulator_t::core_for_thread(memref_tid_t tid)
{
    auto exists = thread2core.find(tid);
    if (exists != thread2core.end())
        return exists->second;
    // Either knob_cpu_scheduling is off and we're ignoring cpu
    // markers, or there has not yet been a cpu marker for this
    // thread.  We fall back to scheduling threads directly to cores.
    int min_core = find_emptiest_core(thread_counts);
    if (!knob_cpu_scheduling && knob_verbose >= 1) {
        std::cerr << "new thread " << tid << " => core " << min_core
                  << " (count=" << thread_counts[min_core] << ")" << std::endl;
    } else if (knob_cpu_scheduling && knob_verbose >= 1) {
        std::cerr << "missing cpu marker, so placing thread " << tid << " => core "
                  << min_core << " (count=" << thread_counts[min_core] << ")"
                  << std::endl;
    }
    ++thread_counts[min_core];
    ++thread_ever_counts[min_core];
    thread2core[tid] = min_core;
    return min_core;
}

void
simulator_t::handle_thread_exit(memref_tid_t tid)
{
    std::unordered_map<memref_tid_t, int>::iterator exists = thread2core.find(tid);
    assert(exists != thread2core.end());
    assert(thread_counts[exists->second] > 0);
    --thread_counts[exists->second];
    if (knob_verbose >= 1) {
        std::cerr << "thread " << tid << " exited from core " << exists->second
                  << " (count=" << thread_counts[exists->second] << ")" << std::endl;
    }
    thread2core.erase(tid);
}

void
simulator_t::print_core(int core) const
{
    if (!knob_cpu_scheduling) {
        std::cerr << "Core #" << core << " (" << thread_ever_counts[core] << " thread(s))"
                  << std::endl;
    } else {
        std::cerr << "Core #" << core;
        if (cpu_counts[core] == 0) {
            // We keep the "(s)" mainly to simplify test templates.
            std::cerr << " (0 traced CPU(s))" << std::endl;
            return;
        }
        std::cerr << " (" << cpu_counts[core] << " traced CPU(s): ";
        bool need_comma = false;
        for (auto iter = cpu2core.begin(); iter != cpu2core.end(); ++iter) {
            if (iter->second == core) {
                if (need_comma)
                    std::cerr << ", ";
                std::cerr << "#" << iter->first;
                need_comma = true;
            }
        }
        std::cerr << ")" << std::endl;
    }
}
