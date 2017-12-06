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

/* tlb_simulator: controls the multi-level TLB simulation.
 */

#ifndef _TLB_SIMULATOR_H_
#define _TLB_SIMULATOR_H_ 1

#include <unordered_map>
#include "simulator.h"
#include "tlb_stats.h"
#include "tlb.h"

class tlb_simulator_t : public simulator_t
{
 public:
    tlb_simulator_t(unsigned int num_cores,
                    uint64_t page_size,
                    unsigned int TLB_L1I_entries,
                    unsigned int TLB_L1D_entries,
                    unsigned int TLB_L1I_assoc,
                    unsigned int TLB_L1D_assoc,
                    unsigned int TLB_L2_entries,
                    unsigned int TLB_L2_assoc,
                    std::string replace_policy,
                    uint64_t skip_refs,
                    uint64_t warmup_refs,
                    uint64_t sim_refs,
                    unsigned int verbose);
    virtual ~tlb_simulator_t();
    virtual bool process_memref(const memref_t &memref);
    virtual bool print_results();

 protected:
    // Create a tlb_t object with a specific replacement policy.
    virtual tlb_t *create_tlb(std::string policy);

    uint64_t knob_page_size;
    unsigned int knob_TLB_L1I_entries;
    unsigned int knob_TLB_L1D_entries;
    unsigned int knob_TLB_L1I_assoc;
    unsigned int knob_TLB_L1D_assoc;
    unsigned int knob_TLB_L2_entries;
    unsigned int knob_TLB_L2_assoc;
    std::string knob_TLB_replace_policy;

    // Each CPU core contains a L1 ITLB, L1 DTLB and L2 TLB.
    // All of them are private to the core.
    tlb_t **itlbs;
    tlb_t **dtlbs;
    tlb_t **lltlbs;
};

#endif /* _TLB_SIMULATOR_H_ */
