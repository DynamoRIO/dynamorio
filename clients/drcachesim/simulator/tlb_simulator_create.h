/* **********************************************************
 * Copyright (c) 2017-2023 Google, Inc.  All rights reserved.
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

/* tlb simulator creation */

#ifndef _TLB_SIMULATOR_CREATE_H_
#define _TLB_SIMULATOR_CREATE_H_ 1

#include <string>
#include "analysis_tool.h"

namespace dynamorio {
namespace drmemtrace {

/**
 * @file drmemtrace/tlb_simulator_create.h
 * @brief DrMemtrace TLB simulator creation.
 */

/**
 * The options for tlb_simulator_create().
 * The options are currently documented in \ref sec_drcachesim_ops.
 */
// The options are currently documented in ../common/options.cpp.
struct tlb_simulator_knobs_t {
    tlb_simulator_knobs_t()
        : num_cores(4)
        , page_size(4 * 1024)
        , TLB_L1I_entries(32)
        , TLB_L1D_entries(32)
        , TLB_L1I_assoc(32)
        , TLB_L1D_assoc(32)
        , TLB_L2_entries(1024)
        , TLB_L2_assoc(4)
        , TLB_replace_policy("LFU")
        , skip_refs(0)
        , warmup_refs(0)
        , warmup_fraction(0.0)
        , sim_refs(1ULL << 63)
        , cpu_scheduling(false)
        , use_physical(false)
        , verbose(0)
    {
    }
    unsigned int num_cores;
    uint64_t page_size;
    unsigned int TLB_L1I_entries;
    unsigned int TLB_L1D_entries;
    unsigned int TLB_L1I_assoc;
    unsigned int TLB_L1D_assoc;
    unsigned int TLB_L2_entries;
    unsigned int TLB_L2_assoc;
    std::string TLB_replace_policy;
    uint64_t skip_refs;
    uint64_t warmup_refs;
    double warmup_fraction;
    uint64_t sim_refs;
    bool cpu_scheduling;
    bool use_physical;
    unsigned int verbose;
};

/** Creates an instance of a TLB simulator. */
analysis_tool_t *
tlb_simulator_create(const tlb_simulator_knobs_t &knobs);

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _TLB_SIMULATOR_CREATE_H_ */
