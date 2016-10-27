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
#include "droption.h"
#include "tlb_stats.h"
#include "tlb.h"
#include "tlb_simulator.h"

tlb_simulator_t::tlb_simulator_t()
{
    num_cores = op_num_cores.get_value();

    itlbs = new tlb_t* [num_cores];
    dtlbs = new tlb_t* [num_cores];
    lltlbs = new tlb_t* [num_cores];
    for (int i = 0; i < num_cores; i++) {
        itlbs[i] = create_tlb(op_TLB_replace_policy.get_value());
        if (itlbs[i] == NULL) {
            success = false;
            return;
        }
        dtlbs[i] = create_tlb(op_TLB_replace_policy.get_value());
        if (dtlbs[i] == NULL) {
            success = false;
            return;
        }
        lltlbs[i] = create_tlb(op_TLB_replace_policy.get_value());
        if (lltlbs[i] == NULL) {
            success = false;
            return;
        }

        if (!itlbs[i]->init(op_TLB_L1I_assoc.get_value(), (int)op_page_size.get_value(),
                            op_TLB_L1I_entries.get_value(), lltlbs[i], new tlb_stats_t) ||
            !dtlbs[i]->init(op_TLB_L1D_assoc.get_value(), (int)op_page_size.get_value(),
                            op_TLB_L1D_entries.get_value(), lltlbs[i], new tlb_stats_t) ||
            !lltlbs[i]->init(op_TLB_L2_assoc.get_value(), (int)op_page_size.get_value(),
                             op_TLB_L2_entries.get_value(), NULL, new tlb_stats_t)) {
            ERRMSG("Usage error: failed to initialize TLBs. Ensure entry number, "
                   "page size and associativity are powers of 2.\n");
            success = false;
            return;
        }
    }

    thread_counts = new unsigned int[num_cores];
    memset(thread_counts, 0, sizeof(thread_counts[0])*num_cores);
    thread_ever_counts = new unsigned int[num_cores];
    memset(thread_ever_counts, 0, sizeof(thread_ever_counts[0])*num_cores);
}

tlb_simulator_t::~tlb_simulator_t()
{
    for (int i = 0; i < num_cores; i++) {
        // Try to handle failure during construction.
        if (itlbs[i] == NULL)
            return;
        delete itlbs[i]->get_stats();
        delete itlbs[i];
        if (dtlbs[i] == NULL)
            return;
        delete dtlbs[i]->get_stats();
        delete dtlbs[i];
        if (lltlbs[i] == NULL)
            return;
        delete lltlbs[i]->get_stats();
        delete lltlbs[i];
    }
    delete [] itlbs;
    delete [] dtlbs;
    delete [] lltlbs;
    delete [] thread_counts;
    delete [] thread_ever_counts;
}

bool
tlb_simulator_t::process_memref(const memref_t &memref)
{
    if (skip_refs > 0) {
        skip_refs--;
        return true;
    }

    // The references after warmup and simulated ones are dropped.
    if (warmup_refs == 0 && sim_refs == 0)
        return true;

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

    if (memref.instr.type == TRACE_TYPE_INSTR)
        itlbs[core]->request(memref);
    else if (memref.data.type == TRACE_TYPE_READ ||
             memref.data.type == TRACE_TYPE_WRITE)
        dtlbs[core]->request(memref);
    else if (memref.exit.type == TRACE_TYPE_THREAD_EXIT) {
        handle_thread_exit(memref.exit.tid);
        last_thread = 0;
    }
    else if (type_is_prefetch(memref.data.type) ||
             memref.flush.type == TRACE_TYPE_INSTR_FLUSH ||
             memref.flush.type == TRACE_TYPE_DATA_FLUSH) {
      // TLB simulator ignores prefetching and cache flushing
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
    if (warmup_refs > 0) { // warm tlbs up
        warmup_refs--;
        // reset tlb stats when warming up is completed
        if (warmup_refs == 0) {
            for (int i = 0; i < num_cores; i++) {
                itlbs[i]->get_stats()->reset();
                dtlbs[i]->get_stats()->reset();
                lltlbs[i]->get_stats()->reset();
            }
        }
    }
    else {
        sim_refs--;
    }
    return true;
}

bool
tlb_simulator_t::print_results()
{
    for (int i = 0; i < num_cores; i++) {
        unsigned int threads = thread_ever_counts[i];
        std::cerr << "Core #" << i << " (" << threads << " thread(s))" << std::endl;
        if (threads > 0) {
            std::cerr << "  L1I stats:" << std::endl;
            itlbs[i]->get_stats()->print_stats("    ");
            std::cerr << "  L1D stats:" << std::endl;
            dtlbs[i]->get_stats()->print_stats("    ");
            std::cerr << "  LL stats:" << std::endl;
            lltlbs[i]->get_stats()->print_stats("    ");
        }
    }
    return true;
}

tlb_t*
tlb_simulator_t::create_tlb(std::string policy)
{
    // XXX: how to implement different replacement policies?
    // Should we extend tlb_t to tlb_XXX_t so as to avoid multiple inheritence?
    // Or should we adopt multiple inheritence to have caching_device_XXX_t as one base
    // and tlb_t as another base class?
    if (policy == REPLACE_POLICY_NON_SPECIFIED || // default LFU
        policy == REPLACE_POLICY_LFU) // set to LFU
        return new tlb_t;

    // undefined replacement policy
    ERRMSG("Usage error: undefined replacement policy. "
           "Please choose " REPLACE_POLICY_LFU".\n");
    return NULL;
}
