/* **********************************************************
 * Copyright (c) 2017 Google, Inc.  All rights reserved.
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

// These options are currently documented in ../common/options.cpp.
analysis_tool_t *
tlb_simulator_create(unsigned int num_cores = 4,
                     uint64_t page_size = 4*1024,
                     unsigned int TLB_L1I_entries = 32,
                     unsigned int TLB_L1D_entries = 32,
                     unsigned int TLB_L1I_assoc = 32,
                     unsigned int TLB_L1D_assoc = 32,
                     unsigned int TLB_L2_entries = 1024,
                     unsigned int TLB_L2_assoc = 4,
                     std::string replace_policy = "LFU",
                     uint64_t skip_refs = 0,
                     uint64_t warmup_refs = 0,
                     uint64_t sim_refs = 1ULL << 63,
                     unsigned int verbose = 0);

#endif /* _TLB_SIMULATOR_CREATE_H_ */
