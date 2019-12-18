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

/* simulator: represent a simulator of a set of caching devices.
 */

#ifndef _SIMULATOR_H_
#define _SIMULATOR_H_ 1

#include <unordered_map>
#include <vector>
#include "caching_device_stats.h"
#include "caching_device.h"
#include "analysis_tool.h"
#include "memref.h"

class simulator_t : public analysis_tool_t {
public:
    simulator_t()
    {
    }
    simulator_t(unsigned int num_cores, uint64_t skip_refs, uint64_t warmup_refs,
                double warmup_fraction, uint64_t sim_refs, bool cpu_scheduling,
                unsigned int verbose);
    virtual ~simulator_t() = 0;
    virtual bool
    process_memref(const memref_t &memref);

protected:
    // Initialize knobs. Success or failure is indicated by setting/resetting
    // the success variable.
    void
    init_knobs(unsigned int num_cores, uint64_t skip_refs, uint64_t warmup_refs,
               double warmup_fraction, uint64_t sim_refs, bool cpu_scheduling,
               unsigned int verbose);
    void
    print_core(int core) const;
    int
    find_emptiest_core(std::vector<int> &counts) const;
    virtual int
    core_for_thread(memref_tid_t tid);
    virtual void
    handle_thread_exit(memref_tid_t tid);

    unsigned int knob_num_cores;
    uint64_t knob_skip_refs;
    uint64_t knob_warmup_refs;
    double knob_warmup_fraction;
    uint64_t knob_sim_refs;
    bool knob_cpu_scheduling;
    unsigned int knob_verbose;

    memref_tid_t last_thread;
    int last_core;

    // For thread mapping to cores:
    std::unordered_map<int, int> cpu2core;
    std::unordered_map<memref_tid_t, int> thread2core;
    std::vector<int> cpu_counts;
    std::vector<int> thread_counts;
    std::vector<int> thread_ever_counts;
};

#endif /* _SIMULATOR_H_ */
