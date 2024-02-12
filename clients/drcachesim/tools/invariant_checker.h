/* **********************************************************
 * Copyright (c) 2016-2024 Google, Inc.  All rights reserved.
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

/* invariant_checker: a memory trace invariants checker.
 */

#ifndef _INVARIANT_CHECKER_H_
#define _INVARIANT_CHECKER_H_ 1

#include <stdint.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "analysis_tool.h"
#include "dr_api.h"
#include "memref.h"
#include "memtrace_stream.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

/* The auto cleanup wrapper of instr_t.
 * This can ensure the instance of instr_t is cleaned up when it is out of scope.
 */
struct instr_autoclean_t {
public:
    instr_autoclean_t(void *drcontext)
        : drcontext(drcontext)
    {
        if (drcontext == nullptr) {
            std::cerr << "instr_autoclean_t: invalid drcontext" << std::endl;
            exit(1);
        }
        data = instr_create(drcontext);
    }
    ~instr_autoclean_t()
    {
        if (data != nullptr) {
            instr_destroy(drcontext, data);
            data = nullptr;
        }
    }
    void *drcontext = nullptr;
    instr_t *data = nullptr;
};

class invariant_checker_t : public analysis_tool_t {
public:
    invariant_checker_t(bool offline = true, unsigned int verbose = 0,
                        std::string test_name = "",
                        std::istream *serial_schedule_file = nullptr,
                        std::istream *cpu_schedule_file = nullptr);
    virtual ~invariant_checker_t();
    std::string
    initialize_shard_type(shard_type_t shard_type) override;
    std::string
    initialize_stream(memtrace_stream_t *serial_stream) override;
    bool
    process_memref(const memref_t &memref) override;
    bool
    print_results() override;
    bool
    parallel_shard_supported() override;
    void *
    parallel_shard_init_stream(int shard_index, void *worker_data,
                               memtrace_stream_t *shard_stream) override;
    void *
    parallel_shard_init(int shard_index, void *worker_data) override;
    bool
    parallel_shard_exit(void *shard_data) override;
    bool
    parallel_shard_memref(void *shard_data, const memref_t &memref) override;
    std::string
    parallel_shard_error(void *shard_data) override;

protected:
    // For performance we support parallel analysis.
    // Most checks are thread-local; for thread switch checks we rely
    // on identifying thread switch points via timestamp entries.
    struct per_shard_t {
        per_shard_t()
        {
            // We need a sentinel different from type 0.
            prev_xfer_marker_.marker.marker_type = TRACE_MARKER_TYPE_VERSION;
            last_xfer_marker_.marker.marker_type = TRACE_MARKER_TYPE_VERSION;
        }
        // Provide a virtual destructor to facilitate subclassing.
        virtual ~per_shard_t() = default;

        memref_t last_branch_ = {};
        memtrace_stream_t *stream = nullptr;
        memref_t prev_entry_ = {};
        memref_t prev_prev_entry_ = {};
        memref_t prev_xfer_marker_ = {}; // Cleared on seeing an instr.
        memref_t last_xfer_marker_ = {}; // Not cleared: just the prior xfer marker.
        uintptr_t prev_func_id_ = 0;
        // We keep track of return addresses of nested function calls.
        std::stack<addr_t> retaddr_stack_;
        uintptr_t trace_version_ = 0;
        // Struct to store decoding related attributes.
        struct decoding_info_t {
            bool has_valid_decoding = false;
            bool is_syscall = false;
            bool writes_memory = false;
            bool is_predicated = false;
            uint num_memory_read_access = 0;
            uint num_memory_write_access = 0;
            addr_t branch_target = 0;
        };
        struct instr_info_t {
            memref_t memref = {};
            decoding_info_t decoding;
        };
        std::unordered_map<app_pc, decoding_info_t> decode_cache_;
        // On UNIX generally last_instr_in_cur_context_ should be used instead.
        instr_info_t prev_instr_;
#ifdef UNIX
        // We keep track of some state per nested signal depth.
        struct signal_context {
            addr_t xfer_int_pc;
            instr_info_t pre_signal_instr;
            bool xfer_aborted_rseq;
        };
        // We only support sigreturn-using handlers so we have pairing: no longjmp.
        std::stack<signal_context> signal_stack_;

        // When we see a TRACE_MARKER_TYPE_KERNEL_XFER we pop the top entry from
        // the above stack into the following. This is required because some of
        // our signal-related checks happen after the above stack is already popped
        // at the TRACE_MARKER_TYPE_KERNEL_XFER marker.
        // The defaults are set to skip various signal-related checks in case we
        // see a signal-return before a signal-start (which happens when the trace
        // starts inside the app signal handler).
        signal_context last_signal_context_ = { 0, {}, false };

        // For the outer-most scope, like other nested signal scopes, we start with an
        // empty memref_t to denote absence of any pre-signal instr.
        instr_info_t last_instr_in_cur_context_;

        bool saw_rseq_abort_ = false;
        // These are only available via annotations in signal_invariants.cpp.
        int instrs_until_interrupt_ = -1;
        int memrefs_until_interrupt_ = -1;
#endif
        bool saw_thread_exit_ = false;
        bool saw_timestamp_but_no_instr_ = false;
        bool found_cache_line_size_marker_ = false;
        bool found_instr_count_marker_ = false;
        bool found_page_size_marker_ = false;
        bool found_syscall_marker_ = false;
        bool prev_was_syscall_marker_ = false;
        int last_syscall_marker_value_ = 0;
        bool found_blocking_marker_ = false;
        uint64_t syscall_count_ = 0;
        uint64_t last_instr_count_marker_ = 0;
        std::string error_;
        // Track the location of errors.
        memref_tid_t tid_ = -1;
        uint64_t ref_count_ = 0;
        // We do not expect these to vary by thread but it is simpler to keep
        // separate values per thread as we discover their values during parallel
        // operation.
        addr_t app_handler_pc_ = 0;
        offline_file_type_t file_type_ = OFFLINE_FILE_TYPE_DEFAULT;
        uintptr_t last_window_ = 0;
        bool window_transition_ = false;
        uint64_t chunk_instr_count_ = 0;
        uint64_t instr_count_ = 0;
        uint64_t last_timestamp_ = 0;
        uint64_t instr_count_since_last_timestamp_ = 0;
        std::vector<schedule_entry_t> sched_;
        std::unordered_map<uint64_t, std::vector<schedule_entry_t>> cpu2sched_;
        bool skipped_instrs_ = false;
        // Rseq region state.
        bool in_rseq_region_ = false;
        addr_t rseq_start_pc_ = 0;
        addr_t rseq_end_pc_ = 0;
        bool saw_filter_endpoint_marker_ = false;
        // Used to check markers after each system call.
        bool expect_syscall_marker_ = false;
        // Counters for expected read and write records.
        int expected_read_records_ = 0;
        int expected_write_records_ = 0;
        bool between_kernel_syscall_trace_markers_ = false;
        instr_info_t pre_syscall_trace_instr_;
    };

    // We provide this for subclasses to run these invariants with custom
    // failure reporting.
    virtual void
    report_if_false(per_shard_t *shard, bool condition,
                    const std::string &invariant_name);
    // This must be called at the end (typically from print_results) and passed in
    // an empty shard structure.
    virtual void
    check_schedule_data(per_shard_t *global_shard);

    virtual bool
    is_a_unit_test(per_shard_t *shard);

    // Check for invariant violations caused by PC discontinuities. Return an error string
    // for such violations.
    std::string
    check_for_pc_discontinuity(per_shard_t *shard,
                               const per_shard_t::instr_info_t &prev_instr_info,
                               const per_shard_t::instr_info_t &cur_memref_info,
                               bool expect_encoding, bool at_kernel_event);

    void *drcontext_ = dr_standalone_init();
    std::unordered_map<int, std::unique_ptr<per_shard_t>> shard_map_;
    // This mutex is only needed in parallel_shard_init.  In all other accesses to
    // shard_map (process_memref, print_results) we are single-threaded.
    std::mutex shard_map_mutex_;

    bool knob_offline_;
    unsigned int knob_verbose_;
    std::string knob_test_name_;
    bool has_annotations_ = false;

    std::istream *serial_schedule_file_ = nullptr;
    std::istream *cpu_schedule_file_ = nullptr;

    memtrace_stream_t *serial_stream_ = nullptr;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _INVARIANT_CHECKER_H_ */
