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
#include <assert.h>
#include <limits.h>
#include "../common/memref.h"
#include "../common/options.h"
#include "../common/utils.h"
#include "droption.h"
#include "simulator.h"

simulator_t::simulator_t(unsigned int num_cores,
                         uint64_t skip_refs,
                         uint64_t warmup_refs,
                         uint64_t sim_refs,
                         unsigned int verbose) :
    knob_num_cores(num_cores),
    knob_skip_refs(skip_refs),
    knob_warmup_refs(warmup_refs),
    knob_sim_refs(sim_refs),
    knob_verbose(verbose),
    last_thread(0),
    last_core(0)
{
}

simulator_t::~simulator_t() {}

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
    for (int i = 0; i < knob_num_cores; i++) {
        if (thread_counts[i] < min_count) {
            min_count = thread_counts[i];
            min_core = i;
        }
    }
    if (knob_verbose >= 1) {
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
    if (knob_verbose >= 1) {
        std::cerr << "thread " << tid << " exited from core " << exists->second <<
            " (count=" << thread_counts[exists->second] << ")" << std::endl;
    }
    thread2core.erase(tid);
}
