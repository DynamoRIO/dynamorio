/* **********************************************************
 * Copyright (c) 2017-2018 Google, Inc.  All rights reserved.
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

#ifndef _BASIC_COUNTS_H_
#define _BASIC_COUNTS_H_ 1

#include <unordered_map>
#include <string>

#include "analysis_tool.h"

class basic_counts_t : public analysis_tool_t
{
 public:
    basic_counts_t(unsigned int verbose);
    virtual ~basic_counts_t();
    virtual bool process_memref(const memref_t &memref);
    virtual bool print_results();

 protected:
    int_least64_t total_threads;
    int_least64_t total_instrs;
    int_least64_t total_instrs_nofetch;
    int_least64_t total_prefetches;
    int_least64_t total_loads;
    int_least64_t total_stores;
    int_least64_t total_sched_markers;
    int_least64_t total_xfer_markers;
    int_least64_t total_func_markers;
    int_least64_t total_other_markers;
    std::unordered_map<memref_tid_t, int_least64_t> thread_instrs;
    std::unordered_map<memref_tid_t, int_least64_t> thread_instrs_nofetch;
    std::unordered_map<memref_tid_t, int_least64_t> thread_prefetches;
    std::unordered_map<memref_tid_t, int_least64_t> thread_loads;
    std::unordered_map<memref_tid_t, int_least64_t> thread_stores;
    std::unordered_map<memref_tid_t, int_least64_t> thread_sched_markers;
    std::unordered_map<memref_tid_t, int_least64_t> thread_xfer_markers;
    std::unordered_map<memref_tid_t, int_least64_t> thread_func_markers;
    std::unordered_map<memref_tid_t, int_least64_t> thread_other_markers;

    unsigned int knob_verbose;

    static const std::string TOOL_NAME;
};

#endif /* _BASIC_COUNTS_H_ */
