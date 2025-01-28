/* **********************************************************
 * Copyright (c) 2015-2025 Google, Inc.  All rights reserved.
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

#include "tlb_simulator.h"

#include <stddef.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "analysis_tool.h"
#include "memref.h"
#include "options.h"
#include "utils.h"
#include "caching_device_stats.h"
#include "simulator.h"
#include "tlb.h"
#include "tlb_simulator_create.h"
#include "tlb_stats.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

analysis_tool_t *
tlb_simulator_create(const tlb_simulator_knobs_t &knobs)
{
    if (knobs.v2p_file.empty())
        return new tlb_simulator_t(knobs);

    std::ifstream fin;
    fin.open(knobs.v2p_file);
    if (!fin.is_open()) {
        ERRMSG("Failed to open the v2p file '%s'\n", knobs.v2p_file.c_str());
        return nullptr;
    }

    tlb_simulator_t *sim = new tlb_simulator_t(knobs);
    std::string error_str = sim->create_v2p_from_file(fin);
    fin.close();

    if (!error_str.empty()) {
        delete sim;
        ERRMSG("ERROR: v2p_reader failed with: %s\n", error_str.c_str());
        return nullptr;
    }

    return sim;
}

tlb_simulator_t::tlb_simulator_t(const tlb_simulator_knobs_t &knobs)
    : simulator_t(knobs.num_cores, knobs.skip_refs, knobs.warmup_refs,
                  knobs.warmup_fraction, knobs.sim_refs, knobs.cpu_scheduling,
                  knobs.use_physical, knobs.verbose)
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
                             knobs_.TLB_L1I_entries, lltlbs_[i],
                             new tlb_stats_t((int)knobs_.page_size)) ||
            !dtlbs_[i]->init(knobs_.TLB_L1D_assoc, (int)knobs_.page_size,
                             knobs_.TLB_L1D_entries, lltlbs_[i],
                             new tlb_stats_t((int)knobs_.page_size)) ||
            !lltlbs_[i]->init(knobs_.TLB_L2_assoc, (int)knobs_.page_size,
                              knobs_.TLB_L2_entries, NULL,
                              new tlb_stats_t((int)knobs_.page_size))) {
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

std::string
tlb_simulator_t::create_v2p_from_file(std::istream &v2p_file)
{
    // If we are not using physical addresses, we don't need a virtual to physical mapping
    // at all.
    if (!knobs_.use_physical)
        return "";

    std::string error_str = simulator_t::create_v2p_from_file(v2p_file);
    if (!error_str.empty()) {
        return error_str;
    }
    // Overwrite tlb_simulator_t.knobs_.page size with simulator_t.page_size, which is
    // set to be the page size in v2p_file.
    knobs_.page_size = page_size_;
    return "";
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
        // TODO i#7230: This causes -warmup_refs and -sim_refs to count non-marker
        // records while -skip_refs counts all records.  We should either say
        // that in the docs or change the behavior here.
        // Plus, this skips the TRACE_MARKER_TYPE_CPU_ID handling below.
        return true;
    }

    // We use a static scheduling of threads to cores, as it is
    // not practical to measure which core each thread actually
    // ran on for each memref.
    int core_index;
    if (memref.data.tid == last_thread_)
        core_index = last_core_index_;
    else {
        core_index = core_for_thread(memref.data.tid);
        last_thread_ = memref.data.tid;
        last_core_index_ = core_index;
    }

    // To support swapping to physical addresses without modifying the passed-in
    // memref (which is also passed to other tools run at the same time) we use
    // indirection.
    const memref_t *simref = &memref;
    memref_t phys_memref;
    if (knobs_.use_physical) {
        phys_memref = memref2phys(memref);
        simref = &phys_memref;
    }

    if (type_is_instr(simref->instr.type))
        itlbs_[core_index]->request(*simref);
    else if (simref->data.type == TRACE_TYPE_READ ||
             simref->data.type == TRACE_TYPE_WRITE)
        dtlbs_[core_index]->request(*simref);
    else if (simref->exit.type == TRACE_TYPE_THREAD_EXIT) {
        handle_thread_exit(simref->exit.tid);
        last_thread_ = 0;
    } else if (type_is_prefetch(simref->data.type) ||
               simref->flush.type == TRACE_TYPE_INSTR_FLUSH ||
               simref->flush.type == TRACE_TYPE_DATA_FLUSH ||
               simref->marker.type == TRACE_TYPE_MARKER ||
               simref->marker.type == TRACE_TYPE_INSTR_NO_FETCH) {
        // TLB simulator ignores prefetching, cache flushing, and markers
    } else {
        error_string_ = "Unhandled memref type " + std::to_string(simref->data.type);
        return false;
    }

    if (knobs_.verbose >= 3) {
        std::cerr << "::" << simref->data.pid << "." << simref->data.tid << ":: "
                  << " @" << (void *)simref->data.pc << " "
                  << trace_type_names[simref->data.type] << " "
                  << (void *)simref->data.addr << " x" << simref->data.size << std::endl;
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
        if (print_core(i)) {
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
    // Should we extend tlb_t to tlb_XXX_t so as to avoid multiple inheritance?
    // Or should we adopt multiple inheritance to have caching_device_XXX_t as one base
    // and tlb_t as another base class?
    if (policy == REPLACE_POLICY_NON_SPECIFIED || // default LFU
        policy == REPLACE_POLICY_LFU)             // set to LFU
        return new tlb_t;

    // undefined replacement policy
    ERRMSG("Usage error: undefined replacement policy. "
           "Please choose " REPLACE_POLICY_LFU ".\n");
    return NULL;
}

} // namespace drmemtrace
} // namespace dynamorio
