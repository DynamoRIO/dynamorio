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
#include "../common/memref.h"
#include "../common/options.h"
#include "../common/utils.h"
#include "../reader/ipc_reader.h"
#include "droption.h"
#include "tlb_stats.h"
#include "tlb.h"
#include "tlb_simulator.h"

bool
tlb_simulator_t::init()
{
    // XXX: add a "required" flag to droption to avoid needing this here
    if (op_ipc_name.get_value().empty()) {
        ERROR("Usage error: ipc name is required\nUsage:\n%s",
              droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        return false;
    }
    ipc_iter = ipc_reader_t(op_ipc_name.get_value().c_str());

    num_cores = op_num_cores.get_value();

    itlbs = new tlb_t* [num_cores];
    dtlbs = new tlb_t* [num_cores];
    lltlbs = new tlb_t* [num_cores];
    for (int i = 0; i < num_cores; i++) {
        itlbs[i] = create_tlb(op_TLB_replace_policy.get_value());
        if (itlbs[i] == NULL)
            return false;
        dtlbs[i] = create_tlb(op_TLB_replace_policy.get_value());
        if (dtlbs[i] == NULL)
            return false;
        lltlbs[i] = create_tlb(op_TLB_replace_policy.get_value());
        if (lltlbs[i] == NULL)
            return false;

        if (!itlbs[i]->init(op_TLB_L1I_assoc.get_value(), op_page_size.get_value(),
                            op_TLB_L1I_entries.get_value(), lltlbs[i], new tlb_stats_t) ||
            !dtlbs[i]->init(op_TLB_L1D_assoc.get_value(), op_page_size.get_value(),
                            op_TLB_L1D_entries.get_value(), lltlbs[i], new tlb_stats_t) ||
            !lltlbs[i]->init(op_TLB_L2_assoc.get_value(), op_page_size.get_value(),
                             op_TLB_L2_entries.get_value(), NULL, new tlb_stats_t)) {
            ERROR("Usage error: failed to initialize TLBs. Ensure entry number, "
                  "page size and associativity are powers of 2.\n");
            return false;
        }
    }

    thread_counts = new unsigned int[num_cores];
    memset(thread_counts, 0, sizeof(thread_counts[0])*num_cores);
    thread_ever_counts = new unsigned int[num_cores];
    memset(thread_ever_counts, 0, sizeof(thread_ever_counts[0])*num_cores);

    return true;
}

tlb_simulator_t::~tlb_simulator_t()
{
    for (int i = 0; i < num_cores; i++) {
        delete itlbs[i]->get_stats();
        delete dtlbs[i]->get_stats();
        delete lltlbs[i]->get_stats();
        delete itlbs[i];
        delete dtlbs[i];
        delete lltlbs[i];
    }
    delete [] itlbs;
    delete [] dtlbs;
    delete [] lltlbs;
    delete [] thread_counts;
    delete [] thread_ever_counts;
}

bool
tlb_simulator_t::run()
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

        if (memref.type == TRACE_TYPE_INSTR)
            itlbs[core]->request(memref);
        else if (memref.type == TRACE_TYPE_READ ||
                 memref.type == TRACE_TYPE_WRITE)
            dtlbs[core]->request(memref);
        else if (memref.type == TRACE_TYPE_THREAD_EXIT) {
            handle_thread_exit(memref.tid);
            last_thread = 0;
        }
        else if (type_is_prefetch(memref.type) ||
                 memref.type == TRACE_TYPE_INSTR_FLUSH ||
                 memref.type == TRACE_TYPE_DATA_FLUSH) {
            // TLB simulator ignores prefetching and cache flushing
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
    }
    return true;
}

bool
tlb_simulator_t::print_stats()
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
    ERROR("Usage error: undefined replacement policy. "
          "Please choose " REPLACE_POLICY_LFU".\n");
    return NULL;
}
