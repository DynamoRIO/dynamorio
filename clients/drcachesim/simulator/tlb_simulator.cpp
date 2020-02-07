/* **********************************************************
 * Copyright (c) 2015-2020 Google, Inc.  All rights reserved.
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
#include "droption.h"
#include "tlb_stats.h"
#include "tlb.h"
#include "tlb_simulator.h"

analysis_tool_t *
tlb_simulator_create(const tlb_simulator_knobs_t &knobs)
{
    return new tlb_simulator_t(knobs);
}

tlb_simulator_t::tlb_simulator_t(const tlb_simulator_knobs_t &knobs)
    : simulator_t(knobs.num_cores, knobs.skip_refs, knobs.warmup_refs,
                  knobs.warmup_fraction, knobs.sim_refs, knobs.cpu_scheduling,
                  knobs.verbose)
    , knobs_(knobs)
{
    itlbs_ = new tlb_t *[knobs_.num_cores];
    dtlbs_ = new tlb_t *[knobs_.num_cores];
    lltlbs_ = new tlb_t *[knobs_.num_cores];
    for (unsigned int i = 0; i < knobs_.num_cores; i++) {
        itlbs_[i] = NULL;
        dtlbs_[i] = NULL;
        lltlbs_[i] = NULL;
    }
    for (unsigned int i = 0; i < knobs_.num_cores; i++) {
        itlbs_[i] = create_tlb(knobs_.TLB_replace_policy);
        if (itlbs_[i] == NULL) {
            error_string_ = "Failed to create itlbs_";
            success_ = false;
            return;
        }
        dtlbs_[i] = create_tlb(knobs_.TLB_replace_policy);
        if (dtlbs_[i] == NULL) {
            error_string_ = "Failed to create dtlbs_";
            success_ = false;
            return;
        }
        lltlbs_[i] = create_tlb(knobs_.TLB_replace_policy);
        if (lltlbs_[i] == NULL) {
            error_string_ = "Failed to create lltlbs_";
            success_ = false;
            return;
        }

        if (!itlbs_[i]->init(knobs_.TLB_L1I_assoc, (int)knobs_.page_size,
                             knobs_.TLB_L1I_entries, lltlbs_[i], new tlb_stats_t) ||
            !dtlbs_[i]->init(knobs_.TLB_L1D_assoc, (int)knobs_.page_size,
                             knobs_.TLB_L1D_entries, lltlbs_[i], new tlb_stats_t) ||
            !lltlbs_[i]->init(knobs_.TLB_L2_assoc, (int)knobs_.page_size,
                              knobs_.TLB_L2_entries, NULL, new tlb_stats_t)) {
            error_string_ =
                "Usage error: failed to initialize TLbs_. Ensure entry number, "
                "page size and associativity are powers of 2.";
            success_ = false;
            return;
        }
    }
}

tlb_simulator_t::~tlb_simulator_t()
{
    for (unsigned int i = 0; i < knobs_.num_cores; i++) {
        // Try to handle failure during construction.
        if (itlbs_[i] == NULL)
            return;
        delete itlbs_[i]->get_stats();
        delete itlbs_[i];
        if (dtlbs_[i] == NULL)
            return;
        delete dtlbs_[i]->get_stats();
        delete dtlbs_[i];
        if (lltlbs_[i] == NULL)
            return;
        delete lltlbs_[i]->get_stats();
        delete lltlbs_[i];
    }
    delete[] itlbs_;
    delete[] dtlbs_;
    delete[] lltlbs_;
}

bool
tlb_simulator_t::process_memref(const memref_t &memref)
{
    if (knobs_.skip_refs > 0) {
        knobs_.skip_refs--;
        return true;
    }

    // The references after warmup and simulated ones are dropped.
    if (knobs_.warmup_refs == 0 && knobs_.sim_refs == 0)
        return true;

    // Both warmup and simulated references are simulated.

    if (!simulator_t::process_memref(memref))
        return false;

    if (memref.marker.type == TRACE_TYPE_MARKER) {
        // We ignore markers before we ask core_for_thread, to avoid asking
        // too early on a timestamp marker.
        return true;
    }

    // We use a static scheduling of threads to cores, as it is
    // not practical to measure which core each thread actually
    // ran on for each memref.
    int core;
    if (memref.data.tid == last_thread_)
        core = last_core_;
    else {
        core = core_for_thread(memref.data.tid);
        last_thread_ = memref.data.tid;
        last_core_ = core;
    }

    if (type_is_instr(memref.instr.type))
        itlbs_[core]->request(memref);
    else if (memref.data.type == TRACE_TYPE_READ || memref.data.type == TRACE_TYPE_WRITE)
        dtlbs_[core]->request(memref);
    else if (memref.exit.type == TRACE_TYPE_THREAD_EXIT) {
        handle_thread_exit(memref.exit.tid);
        last_thread_ = 0;
    } else if (type_is_prefetch(memref.data.type) ||
               memref.flush.type == TRACE_TYPE_INSTR_FLUSH ||
               memref.flush.type == TRACE_TYPE_DATA_FLUSH ||
               memref.marker.type == TRACE_TYPE_MARKER ||
               memref.marker.type == TRACE_TYPE_INSTR_NO_FETCH) {
        // TLB simulator ignores prefetching, cache flushing, and markers
    } else {
        error_string_ = "Unhandled memref type " + std::to_string(memref.data.type);
        return false;
    }

    if (knobs_.verbose >= 3) {
        std::cerr << "::" << memref.data.pid << "." << memref.data.tid << ":: "
                  << " @" << (void *)memref.data.pc << " "
                  << trace_type_names[memref.data.type] << " " << (void *)memref.data.addr
                  << " x" << memref.data.size << std::endl;
    }

    // process counters for warmup and simulated references
    if (knobs_.warmup_refs > 0) { // warm tlbs up
        knobs_.warmup_refs--;
        // reset tlb stats when warming up is completed
        if (knobs_.warmup_refs == 0) {
            for (unsigned int i = 0; i < knobs_.num_cores; i++) {
                itlbs_[i]->get_stats()->reset();
                dtlbs_[i]->get_stats()->reset();
                lltlbs_[i]->get_stats()->reset();
            }
        }
    } else {
        knobs_.sim_refs--;
    }
    return true;
}

bool
tlb_simulator_t::print_results()
{
    std::cerr << "TLB simulation results:\n";
    for (unsigned int i = 0; i < knobs_.num_cores; i++) {
        print_core(i);
        if (thread_ever_counts_[i] > 0) {
            std::cerr << "  L1I stats:" << std::endl;
            itlbs_[i]->get_stats()->print_stats("    ");
            std::cerr << "  L1D stats:" << std::endl;
            dtlbs_[i]->get_stats()->print_stats("    ");
            std::cerr << "  LL stats:" << std::endl;
            lltlbs_[i]->get_stats()->print_stats("    ");
        }
    }
    return true;
}

tlb_t *
tlb_simulator_t::create_tlb(std::string policy)
{
    // XXX: how to implement different replacement policies?
    // Should we extend tlb_t to tlb_XXX_t so as to avoid multiple inheritence?
    // Or should we adopt multiple inheritence to have caching_device_XXX_t as one base
    // and tlb_t as another base class?
    if (policy == REPLACE_POLICY_NON_SPECIFIED || // default LFU
        policy == REPLACE_POLICY_LFU)             // set to LFU
        return new tlb_t;

    // undefined replacement policy
    ERRMSG("Usage error: undefined replacement policy. "
           "Please choose " REPLACE_POLICY_LFU ".\n");
    return NULL;
}
