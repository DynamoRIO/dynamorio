/* **********************************************************
 * Copyright (c) 2016-2021 Google, Inc.  All rights reserved.
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

/* trace_invariants: a memory trace invariants checker.
 */

#ifndef _TRACE_INVARIANTS_H_
#define _TRACE_INVARIANTS_H_ 1

#include "../analysis_tool.h"
#include "../common/memref.h"
#include <stack>
#include <unordered_map>

class trace_invariants_t : public analysis_tool_t {
public:
    trace_invariants_t(bool offline = true, unsigned int verbose = 0,
                       std::string test_name = "");
    virtual ~trace_invariants_t();
    bool
    process_memref(const memref_t &memref) override;
    bool
    print_results() override;

protected:
    bool knob_offline_;
    unsigned int knob_verbose_;
    std::string knob_test_name_;
    memref_t prev_interleaved_instr_;
    std::unordered_map<memref_tid_t, memref_t> prev_instr_;
    std::unordered_map<memref_tid_t, memref_t> prev_xfer_marker_;
#ifdef UNIX
    std::unordered_map<memref_tid_t, memref_t> prev_entry_;
    std::unordered_map<memref_tid_t, memref_t> prev_prev_entry_;
    std::unordered_map<memref_tid_t, std::stack<memref_t>> pre_signal_instr_;
    // These are only available via annotations in signal_invariants.cpp.
    std::unordered_map<memref_tid_t, int> instrs_until_interrupt_;
    std::unordered_map<memref_tid_t, int> memrefs_until_interrupt_;
#endif
    addr_t app_handler_pc_;
    offline_file_type_t file_type_ = OFFLINE_FILE_TYPE_DEFAULT;
    std::unordered_map<memref_tid_t, bool> thread_exited_;
    std::unordered_map<memref_tid_t, bool> found_cache_line_size_marker_;
    std::unordered_map<memref_tid_t, bool> found_instr_count_marker_;
    std::unordered_map<memref_tid_t, uint64_t> last_instr_count_marker_;
};

#endif /* _TRACE_INVARIANTS_H_ */
