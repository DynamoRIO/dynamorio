/* **********************************************************
 * Copyright (c) 2018-2022 Google, Inc.  All rights reserved.
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

#ifndef _VIEW_H_
#define _VIEW_H_ 1

#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "analysis_tool.h"
#include "raw2trace.h"
#include "raw2trace_directory.h"

class view_t : public analysis_tool_t {
public:
    view_t(const std::string &module_file_path, memref_tid_t thread, uint64_t skip_refs,
           uint64_t sim_refs, const std::string &syntax, unsigned int verbose,
           const std::string &alt_module_dir = "");
    std::string
    initialize() override;
    bool
    parallel_shard_supported() override;
    void *
    parallel_shard_init(int shard_index, void *worker_data) override;
    bool
    parallel_shard_exit(void *shard_data) override;
    bool
    parallel_shard_memref(void *shard_data, const memref_t &memref) override;
    std::string
    parallel_shard_error(void *shard_data) override;
    bool
    process_memref(const memref_t &memref) override;
    bool
    print_results() override;

protected:
    struct dcontext_cleanup_last_t {
    public:
        ~dcontext_cleanup_last_t()
        {
            if (dcontext != nullptr)
                dr_standalone_exit();
        }
        void *dcontext = nullptr;
    };

    bool
    should_skip(const memref_t &memref);

    inline void
    print_header()
    {
        std::cerr << std::setw(9) << "Output format:\n<record#>"
                  << ": T<tid> <record details>\n"
                  << "------------------------------------------------------------\n";
    }

    inline void
    print_prefix(const memref_t &memref, int ref_adjust = 0,
                 std::ostream &stream = std::cerr)
    {
        if (prev_tid_ != -1 && prev_tid_ != memref.instr.tid)
            stream << "------------------------------------------------------------\n";
        prev_tid_ = memref.instr.tid;
        stream << std::setw(9) << (num_refs_ + ref_adjust) << ": T" << memref.marker.tid
               << " ";
    }

    /* We make this the first field so that dr_standalone_exit() is called after
     * destroying the other fields which may use DR heap.
     */
    dcontext_cleanup_last_t dcontext_;
    std::string module_file_path_;
    std::unique_ptr<module_mapper_t> module_mapper_;
    raw2trace_directory_t directory_;
    unsigned int knob_verbose_;
    int trace_version_;
    static const std::string TOOL_NAME;
    memref_tid_t knob_thread_;
    uint64_t knob_skip_refs_;
    uint64_t skip_refs_left_;
    uint64_t knob_sim_refs_;
    uint64_t sim_refs_left_;
    bool refs_limited_;
    std::string knob_syntax_;
    std::string knob_alt_module_dir_;
    uint64_t num_disasm_instrs_;
    std::unordered_map<app_pc, std::string> disasm_cache_;
    memref_tid_t prev_tid_;
    intptr_t filetype_;
    std::unordered_set<memref_tid_t> printed_header_;
    uint64_t num_refs_;
    std::unordered_map<memref_tid_t, uintptr_t> last_window_;
    uintptr_t timestamp_;
    bool has_modules_;
};

#endif /* _VIEW_H_ */
