/* **********************************************************
 * Copyright (c) 2021-2025 Google, LLC  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, LLC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Test for checks performed by invariant_checker_t that are not tested
 * by the signal_invariants app's prefetch and handler markers.
 * This looks for precise error strings from invariant_checker.cpp: but
 * we will notice if the literals get out of sync as the test will fail.
 */

#include <assert.h>

#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "../tools/schedule_stats.h"
#include "../common/memref.h"
#include "memref_gen.h"
#include "scheduler.h"
#include "test_helpers.h"

namespace dynamorio {
namespace drmemtrace {
namespace {

using ::dynamorio::drmemtrace::default_memtrace_stream_t;
using ::dynamorio::drmemtrace::memref_t;
using ::dynamorio::drmemtrace::memref_tid_t;
using ::dynamorio::drmemtrace::TRACE_MARKER_TYPE_CORE_IDLE;
using ::dynamorio::drmemtrace::TRACE_MARKER_TYPE_CORE_WAIT;
using ::dynamorio::drmemtrace::TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH;
using ::dynamorio::drmemtrace::TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL;
using ::dynamorio::drmemtrace::TRACE_MARKER_TYPE_SYSCALL;
using ::dynamorio::drmemtrace::TRACE_MARKER_TYPE_SYSCALL_TRACE_END;
using ::dynamorio::drmemtrace::TRACE_MARKER_TYPE_SYSCALL_TRACE_START;
using schedule_record_t = ::dynamorio::drmemtrace::schedule_stats_t::schedule_record_t;

static constexpr bool VOLUNTARY = true;
static constexpr bool DIRECT = true;

// Create a class for testing that has reliable timing.
// It assumes it is only used with one thread and parallel operation
// is emulated via lockstep serial walking, so there is no need for locks.
class mock_schedule_stats_t : public schedule_stats_t {
public:
    mock_schedule_stats_t(uint64_t print_every, unsigned int verbose = 0)
        : schedule_stats_t(print_every, verbose)
    {
    }

    uint64_t
    get_current_microseconds() override
    {
        return global_time_;
    }

    bool
    parallel_shard_memref(void *shard_data, const memref_t &memref) override
    {
        // This global time with our lockstep iteration in run_schedule_stats()
        // over-counts as it advances while threads are waiting their
        // serial turn, but that's fine: so long as it's deterministic.
        ++global_time_;
        return schedule_stats_t::parallel_shard_memref(shard_data, memref);
    }

private:
    // Start at 1 to avoid asserts about a time of 0.
    uint64_t global_time_ = 1;
};

class mock_stream_t : public default_memtrace_stream_t {
public:
    uint64_t
    get_version() const override
    {
        return TRACE_ENTRY_VERSION;
    }

    memtrace_stream_t *
    get_input_interface() const override
    {
        return const_cast<mock_stream_t *>(this);
    }
};

// Bypasses the analyzer and scheduler for a controlled test sequence.
// Alternates the per-core memref vectors in lockstep.
static schedule_stats_t::counters_t
run_schedule_stats(
    const std::vector<std::vector<memref_t>> &memrefs,
    const std::vector<std::vector<schedule_record_t>> *expected_record = nullptr)
{
    mock_schedule_stats_t tool(/*print_every=*/1, /*verbosity=*/1);
    struct per_core_t {
        void *worker_data;
        void *shard_data;
        mock_stream_t stream;
        bool finished = false;
        size_t memref_idx = 0;
    };
    std::vector<per_core_t> per_core(memrefs.size());
    for (int cpu = 0; cpu < static_cast<int>(memrefs.size()); ++cpu) {
        per_core[cpu].worker_data = tool.parallel_worker_init(cpu);
        per_core[cpu].stream.set_output_cpuid(cpu);
        per_core[cpu].shard_data = tool.parallel_shard_init_stream(
            cpu, per_core[cpu].worker_data, &per_core[cpu].stream);
    }
    // Walk in lockstep until all are empty.
    int num_finished = 0;
    while (num_finished < static_cast<int>(memrefs.size())) {
        for (size_t cpu = 0; cpu < memrefs.size(); ++cpu) {
            if (per_core[cpu].finished)
                continue;
            memref_t memref = memrefs[cpu][per_core[cpu].memref_idx];
            per_core[cpu].stream.set_tid(memref.instr.tid);
            per_core[cpu].stream.set_workload_id(memref.instr.pid);
            per_core[cpu].stream.set_output_cpuid(cpu);
            if (memref.marker.type == TRACE_TYPE_MARKER) {
                switch (memref.marker.marker_type) {
                case TRACE_MARKER_TYPE_SYSCALL_TRACE_START:
                case TRACE_MARKER_TYPE_CONTEXT_SWITCH_START:
                    per_core[cpu].stream.set_in_kernel_trace(true);
                    break;
                case TRACE_MARKER_TYPE_SYSCALL_TRACE_END:
                case TRACE_MARKER_TYPE_CONTEXT_SWITCH_END:
                    per_core[cpu].stream.set_in_kernel_trace(false);
                    break;
                case TRACE_MARKER_TYPE_TIMESTAMP:
                    per_core[cpu].stream.set_last_timestamp(memref.marker.marker_value);
                    break;
                default:;
                }
            }
            bool res = tool.parallel_shard_memref(per_core[cpu].shard_data, memref);
            assert(res);
            ++per_core[cpu].memref_idx;
            if (per_core[cpu].memref_idx >= memrefs[cpu].size()) {
                per_core[cpu].finished = true;
                ++num_finished;
            }
        }
    }
    std::vector<std::vector<schedule_record_t>> record(memrefs.size());
    for (int cpu = 0; cpu < static_cast<int>(memrefs.size()); ++cpu) {
        tool.parallel_shard_exit(per_core[cpu].shard_data);
        tool.parallel_worker_exit(per_core[cpu].worker_data);
        tool.get_switch_record(cpu, record[cpu]);
    }
    assert(record.size() == memrefs.size());
    if (expected_record != nullptr) {
        for (size_t i = 0; i < record.size(); ++i) {
            assert(record[i].size() == (*expected_record)[i].size());
            for (size_t j = 0; j < record[i].size(); ++j) {
                assert(record[i][j] == (*expected_record)[i][j]);
            }
        }
    }
    return tool.get_total_counts();
}

static bool
test_basic_stats()
{
    static constexpr int64_t TID_A = 42;
    static constexpr int64_t TID_B = 142;
    static constexpr int64_t TID_C = 242;
    std::vector<std::vector<memref_t>> memrefs = {
        {
            gen_instr(TID_A),
            // Involuntary switch.
            gen_instr(TID_B),
            gen_marker(TID_B, TRACE_MARKER_TYPE_TIMESTAMP, 1100),
            gen_marker(TID_B, TRACE_MARKER_TYPE_SYSCALL, 0),
            gen_marker(TID_B, TRACE_MARKER_TYPE_TIMESTAMP, 1600),
            // Voluntary switch, on non-maybe-blocking-marked syscall.
            gen_instr(TID_A),
            gen_instr(TID_A),
            gen_instr(TID_A),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 2100),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 0),
            gen_marker(TID_A, TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
            gen_marker(TID_A, TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH, TID_C),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 2300),
            // Direct switch.
            gen_instr(TID_C),
            // No switch: latency too small.
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 2500),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 2599),
            gen_instr(TID_C),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 3100),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH, TID_A),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 3300),
            // Direct switch requested but failed.
            gen_instr(TID_C),
            gen_exit(TID_C),
            // An exit is a voluntary switch.
            gen_exit(TID_A),
        },
        {
            gen_instr(TID_B),
            // Involuntary switch.
            gen_instr(TID_A),
            // Involuntary switch.
            gen_instr(TID_C),
            gen_instr(TID_C),
            gen_instr(TID_C),
            // Wait.
            gen_marker(TID_C, TRACE_MARKER_TYPE_CORE_WAIT, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CORE_WAIT, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CORE_WAIT, 0),
            // Involuntary switch.
            gen_instr(TID_B),
            gen_instr(TID_B),
            gen_instr(TID_B),
            gen_exit(TID_B),
        },
    };
    const std::vector<std::vector<schedule_record_t>> expected_record = {
        {
            { 0, TID_A, /*ins=*/1, /*sys#=*/-1, /*lat=*/-1 },
            { 0, TID_B, /*ins=*/1, /*sys#=*/0, /*lat=*/500, VOLUNTARY },
            { 0, TID_A, /*ins=*/3, /*sys#=*/0, /*lat=*/200, VOLUNTARY, DIRECT },
            { 0, TID_C, /*ins=*/3, /*sys#=*/-1, /*lat=*/-1, VOLUNTARY },
        },
        {
            { 0, TID_B, /*ins=*/1, /*sys#=*/-1, /*lat=*/-1 },
            { 0, TID_A, /*ins=*/1, /*sys#=*/-1, /*lat=*/-1 },
            { 0, TID_C, /*ins=*/3, /*sys#=*/-1, /*lat=*/-1 },
        },
    };
    auto result = run_schedule_stats(memrefs, &expected_record);
    assert(result.instrs == 16);
    assert(result.total_switches == 7);
    assert(result.voluntary_switches == 3);
    assert(result.direct_switches == 1);
    assert(result.syscalls == 4);
    assert(result.maybe_blocking_syscalls == 3);
    assert(result.direct_switch_requests == 2);
    // 5 migrations: A 0->1; B 1->0; A 1->0; C 0->1; B 0->1.
    assert(result.observed_migrations == 5);
    assert(result.waits == 3);
    assert(result.idle_microseconds == 0);
    assert(result.cpu_microseconds > 20);
    assert(result.wait_microseconds >= 3);

    // Sanity check the histograms.
    // XXX: Parameterize the bin size for more granular unit test results?
    // Right now they're all in the same 0-50K bin.  We're still able to check
    // the count though.
    std::string global_hist = result.instrs_per_switch->to_string();
    std::cerr << "Gobal switches:\n" << global_hist;
    assert(global_hist == "           0..   50000     7\n");
    // XXX i#6672: Upgrade to C++17 for structured bindings.
    for (const auto &keyval : result.tid2instrs_per_switch) {
        std::string hist = keyval.second->to_string();
        assert(keyval.first.workload_id == 0);
        std::cerr << "Tid " << keyval.first.tid << " switches:\n" << hist;
        switch (keyval.first.tid) {
        case TID_A: assert(hist == "           0..   50000     3\n"); break;
        case TID_B: assert(hist == "           0..   50000     2\n"); break;
        case TID_C: assert(hist == "           0..   50000     2\n"); break;
        default: assert(false && "unexpected tid");
        }
    }
    return true;
}

static bool
test_basic_stats_with_syscall_trace()
{
    static constexpr int64_t TID_A = 42;
    static constexpr int64_t TID_B = 142;
    static constexpr int64_t TID_C = 242;
    std::vector<std::vector<memref_t>> memrefs = {
        {
            gen_instr(TID_A),
            // Involuntary switch.
            gen_instr(TID_B),
            gen_marker(TID_B, TRACE_MARKER_TYPE_TIMESTAMP, 1100),
            gen_marker(TID_B, TRACE_MARKER_TYPE_SYSCALL, 0),
            gen_marker(TID_B, TRACE_MARKER_TYPE_TIMESTAMP, 1600),
            gen_marker(TID_B, TRACE_MARKER_TYPE_SYSCALL_TRACE_START, 0),
            gen_instr(TID_B),
            gen_marker(TID_B, TRACE_MARKER_TYPE_SYSCALL_TRACE_END, 0),
            // Voluntary switch, on non-maybe-blocking-marked syscall.
            gen_instr(TID_A),
            gen_instr(TID_A),
            gen_instr(TID_A),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 2100),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 0),
            gen_marker(TID_A, TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
            gen_marker(TID_A, TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH, TID_C),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_START, 0),
            gen_instr(TID_A),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_END, 0),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 2300),
            // Direct switch.
            gen_marker(TID_C, TRACE_MARKER_TYPE_CONTEXT_SWITCH_START,
                       switch_type_t::SWITCH_THREAD),
            gen_instr(TID_C),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CONTEXT_SWITCH_END,
                       switch_type_t::SWITCH_THREAD),
            gen_instr(TID_C),
            // No switch: latency too small.
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 2500),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL_TRACE_START, 0),
            gen_instr(TID_C),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL_TRACE_END, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 2599),
            gen_instr(TID_C),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 3100),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH, TID_A),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL_TRACE_START, 0),
            gen_instr(TID_C),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL_TRACE_END, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 3300),
            // Direct switch requested but failed.
            gen_instr(TID_C),
            gen_exit(TID_C),
            // An exit is a voluntary switch.
            gen_exit(TID_A),
        },
        {
            gen_instr(TID_B),
            // Involuntary switch.
            gen_instr(TID_A),
            // Involuntary switch.
            gen_instr(TID_C),
            gen_instr(TID_C),
            gen_instr(TID_C),
            // Wait.
            gen_marker(TID_C, TRACE_MARKER_TYPE_CORE_WAIT, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CORE_WAIT, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CORE_WAIT, 0),
            // Involuntary switch.
            gen_instr(TID_B),
            gen_instr(TID_B),
            gen_instr(TID_B),
            gen_exit(TID_B),
        },
    };
    const std::vector<std::vector<schedule_record_t>> expected_record = {
        {
            { 0, TID_A, /*ins=*/1, /*sys#=*/-1, /*lat=*/-1 },
            { 0, TID_B, /*ins=*/2, /*sys#=*/0, /*lat=*/500, VOLUNTARY },
            { 0, TID_A, /*ins=*/4, /*sys#=*/0, /*lat=*/200, VOLUNTARY, DIRECT },
            { 0, TID_C, /*ins=*/6, /*sys#=*/-1, /*lat=*/-1, VOLUNTARY },
        },
        {
            { 0, TID_B, /*ins=*/1, /*sys#=*/-1, /*lat=*/-1 },
            { 0, TID_A, /*ins=*/1, /*sys#=*/-1, /*lat=*/-1 },
            { 0, TID_C, /*ins=*/3, /*sys#=*/-1, /*lat=*/-1 },
        },
    };
    auto result = run_schedule_stats(memrefs, &expected_record);
    assert(result.instrs == 21);
    // Following are the same as test_basic_stats.
    assert(result.total_switches == 7);
    assert(result.voluntary_switches == 3);
    assert(result.direct_switches == 1);
    assert(result.syscalls == 4);
    assert(result.maybe_blocking_syscalls == 3);
    assert(result.direct_switch_requests == 2);
    // 5 migrations: A 0->1; B 1->0; A 1->0; C 0->1; B 0->1.
    assert(result.observed_migrations == 5);
    assert(result.waits == 3);
    assert(result.idle_microseconds == 0);
    assert(result.cpu_microseconds > 20);
    assert(result.wait_microseconds >= 3);
    return true;
}

static bool
test_idle()
{
    static constexpr int64_t TID_A = 42;
    static constexpr int64_t TID_B = 142;
    static constexpr int64_t TID_C = 242;
    std::vector<std::vector<memref_t>> memrefs = {
        {
            gen_instr(TID_B),
            gen_instr(TID_B),
            // A switch to idle w/ no syscall is an involuntary switch.
            gen_marker(IDLE_THREAD_ID, TRACE_MARKER_TYPE_CORE_IDLE, 0),
            gen_marker(IDLE_THREAD_ID, TRACE_MARKER_TYPE_CORE_IDLE, 0),
            gen_marker(IDLE_THREAD_ID, TRACE_MARKER_TYPE_CORE_IDLE, 0),
            // A switch from idle is not counted.
            gen_instr(TID_B),
            gen_instr(TID_B),
            gen_instr(TID_B),
            gen_exit(TID_B),
        },
        {
            // This is a start-idle core.
            gen_marker(IDLE_THREAD_ID, TRACE_MARKER_TYPE_CORE_IDLE, 0),
            // Switch to the first thread from the start-idle state.
            // A switch from idle is not counted.
            gen_instr(TID_C),
            // Involuntary switch.
            gen_instr(TID_A),
            // Involuntary switch.
            gen_instr(TID_C),
            // A switch to idle w/ no syscall is an involuntary switch.
            gen_marker(IDLE_THREAD_ID, TRACE_MARKER_TYPE_CORE_IDLE, 0),
            gen_marker(IDLE_THREAD_ID, TRACE_MARKER_TYPE_CORE_IDLE, 0),
            gen_marker(IDLE_THREAD_ID, TRACE_MARKER_TYPE_CORE_IDLE, 0),
            // A switch from idle is not counted.
            gen_instr(TID_C),
            gen_instr(TID_C),
            // Involuntary switch.
            // Wait.
            gen_marker(TID_C, TRACE_MARKER_TYPE_CORE_WAIT, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CORE_WAIT, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CORE_WAIT, 0),
            gen_instr(TID_A),
            gen_instr(TID_A),
            gen_instr(TID_A),
            gen_exit(TID_A),
            // An exit is a voluntary switch.
            gen_exit(TID_C),
        },
    };
    const std::vector<std::vector<schedule_record_t>> expected_record = {
        {
            { 0, TID_B, /*ins=*/2, /*sys#=*/-1, /*lat=*/-1 },
        },
        {
            { 0, TID_C, /*ins=*/1, /*sys#=*/-1, /*lat=*/-1 },
            { 0, TID_A, /*ins=*/1, /*sys#=*/-1, /*lat=*/-1 },
            { 0, TID_C, /*ins=*/1, /*sys#=*/-1, /*lat=*/-1 },
            { 0, TID_C, /*ins=*/2, /*sys#=*/-1, /*lat=*/-1 },
            { 0, TID_A, /*ins=*/3, /*sys#=*/-1, /*lat=*/-1, VOLUNTARY },
        },
    };
    auto result = run_schedule_stats(memrefs, &expected_record);
    assert(result.instrs == 13);
    assert(result.total_switches == 6);
    assert(result.voluntary_switches == 1);
    assert(result.direct_switches == 0);
    assert(result.syscalls == 0);
    assert(result.maybe_blocking_syscalls == 0);
    assert(result.direct_switch_requests == 0);
    assert(result.observed_migrations == 0);
    assert(result.waits == 3);
    assert(result.idles == 7);
    assert(result.idle_microseconds >= 6);
    assert(result.idle_micros_at_last_instr > 0 &&
           result.idle_micros_at_last_instr <= result.idle_microseconds);
    assert(result.cpu_microseconds > 10);
    assert(result.wait_microseconds >= 3);
    return true;
}

static bool
test_cpu_footprint()
{
    static constexpr int64_t PID_X = 1234;
    static constexpr int64_t PID_Y = 5678;
    static constexpr int64_t TID_A = 42;
    static constexpr int64_t TID_B = 142;
    static constexpr int64_t TID_C = 242;
    static constexpr addr_t INSTR_PC = 1001;
    static constexpr size_t INSTR_SIZE = 4;
    std::vector<std::vector<memref_t>> memrefs = {
        {
            gen_instr(TID_A, INSTR_PC, INSTR_SIZE, PID_X),
            gen_instr(TID_B, INSTR_PC, INSTR_SIZE, PID_X),
            // Test identical tids in different workloads.
            gen_instr(TID_A, INSTR_PC, INSTR_SIZE, PID_Y),
        },
        {
            gen_instr(TID_A, INSTR_PC, INSTR_SIZE, PID_Y),
        },
        {
            gen_instr(TID_C, INSTR_PC, INSTR_SIZE, PID_Y),
            gen_instr(TID_A, INSTR_PC, INSTR_SIZE, PID_X),
        },
        {
            gen_instr(TID_C, INSTR_PC, INSTR_SIZE, PID_Y),
        },
        {
            gen_instr(TID_C, INSTR_PC, INSTR_SIZE, PID_Y),
        },
    };
    const std::vector<std::vector<schedule_record_t>> expected_record = {
        {
            { 1234, TID_A, /*ins=*/1, /*sys#=*/-1, /*lat=*/-1 },
            { 1234, TID_B, /*ins=*/1, /*sys#=*/-1, /*lat=*/-1 },
        },
        {},
        {
            { 5678, TID_C, /*ins=*/1, /*sys#=*/-1, /*lat=*/-1 },
        },
        {},
        {},
    };
    auto result = run_schedule_stats(memrefs, &expected_record);
    assert(result.instrs == 8);
    std::string hist = result.cores_per_thread->to_string();
    std::cerr << "Cores-per-thread histogram:\n" << hist << "\n";
    // We expect X.A=2, X.B=1, Y.A=2, Y.C=3:
    assert(hist ==
           "           1..       2     1\n"
           "           2..       3     2\n"
           "           3..       4     1\n");
    return true;
}

static bool
test_syscall_latencies()
{
    static constexpr int64_t TID_A = 42;
    static constexpr int64_t TID_B = 142;
    static constexpr int64_t TID_C = 242;
    static constexpr int64_t TID_D = 199;
    static constexpr int64_t TID_E = 222;
    static constexpr uintptr_t SYSNUM_X = 12;
    static constexpr uintptr_t SYSNUM_Y = 167;
    std::vector<std::vector<memref_t>> memrefs = {
        {
            gen_instr(TID_A),
            // Involuntary switch: ignored for syscall latencies.
            gen_instr(TID_B),
            gen_marker(TID_B, TRACE_MARKER_TYPE_TIMESTAMP, 1100),
            gen_marker(TID_B, TRACE_MARKER_TYPE_SYSCALL, SYSNUM_X),
            gen_marker(TID_B, TRACE_MARKER_TYPE_TIMESTAMP, 1600),
            // Voluntary switch: latency 500.
            gen_instr(TID_A),
            gen_instr(TID_A),
            gen_instr(TID_A),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 2100),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, SYSNUM_Y),
            gen_marker(TID_A, TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
            gen_marker(TID_A, TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH, TID_C),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 2300),
            // Direct switch: latency 200.
            gen_instr(TID_C),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 2500),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL, SYSNUM_X),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 2599),
            // No switch: latency 99 too small.
            gen_instr(TID_C),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 3100),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL, SYSNUM_Y),
            gen_marker(TID_C, TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH, TID_A),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 3300),
            // Direct switch requested but failed: latency 200.
            gen_instr(TID_C),
            gen_exit(TID_C),
            gen_exit(TID_A),
        },
        {
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 3400),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL, SYSNUM_X),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 4400),
            // Voluntary switch: latency 1000.
            // Test a thread starting at a recorded syscall after another thread on a
            // core.
            gen_marker(TID_E, TRACE_MARKER_TYPE_TIMESTAMP, 4500),
            gen_marker(TID_E, TRACE_MARKER_TYPE_FUNC_ID, 202),
            gen_marker(TID_E, TRACE_MARKER_TYPE_FUNC_RETVAL, static_cast<uintptr_t>(-4)),
            gen_marker(TID_E, TRACE_MARKER_TYPE_SYSCALL_FAILED, 4),
            gen_marker(TID_E, TRACE_MARKER_TYPE_TIMESTAMP, 5200),
            // Preempt to D so no latency should be recorded here, despite
            // there being a syscall with no regular instr records in between.
            gen_instr(TID_D),
            gen_exit(TID_E),
            gen_exit(TID_B),
        },
        {
            // Test a thread starting at a recorded syscall first thing on a core.
            gen_marker(TID_D, TRACE_MARKER_TYPE_TIMESTAMP, 1500),
            gen_marker(TID_D, TRACE_MARKER_TYPE_FUNC_ID, 202),
            gen_marker(TID_D, TRACE_MARKER_TYPE_FUNC_RETVAL, static_cast<uintptr_t>(-4)),
            gen_marker(TID_D, TRACE_MARKER_TYPE_SYSCALL_FAILED, 4),
            gen_marker(TID_D, TRACE_MARKER_TYPE_TIMESTAMP, 3200),
            // Preempt to E so no latency should be recorded here.
            gen_instr(TID_E),
            gen_exit(TID_D),
        },
    };
    const std::vector<std::vector<schedule_record_t>> expected_record = {
        {
            { 0, TID_A, /*ins=*/1, /*sys#=*/-1, /*lat=*/-1 },
            { 0, TID_B, /*ins=*/1, /*sys#=*/12, /*lat=*/500, VOLUNTARY },
            { 0, TID_A, /*ins=*/3, /*sys#=*/167, /*lat=*/200, VOLUNTARY, DIRECT },
            { 0, TID_C, /*ins=*/3, /*sys#=*/-1, /*lat=*/-1, VOLUNTARY },
        },
        {
            { 0, TID_C, /*ins=*/0, /*sys#=*/12, /*lat=*/1000, VOLUNTARY },
            { 0, TID_E, /*ins=*/0, /*sys#=*/-1, /*lat=*/-1, VOLUNTARY },
            { 0, TID_D, /*ins=*/1, /*sys#=*/-1, /*lat=*/-1 },
            { 0, TID_E, /*ins=*/0, /*sys#=*/-1, /*lat=*/-1, VOLUNTARY },
        },
        {
            { 0, TID_D, /*ins=*/0, /*sys#=*/-1, /*lat=*/-1 },
            { 0, TID_E, /*ins=*/1, /*sys#=*/-1, /*lat=*/-1 },
        },
    };
    auto result = run_schedule_stats(memrefs, &expected_record);
    auto it_x_switch = result.sysnum_switch_latency.find(SYSNUM_X);
    assert(it_x_switch != result.sysnum_switch_latency.end());
    std::string hist_x_switch = it_x_switch->second->to_string();
    std::cerr << "Sysnum switch latency for " << SYSNUM_X << ":\n"
              << hist_x_switch << "\n";
    assert(hist_x_switch ==
           "         500..     505     1\n        1000..    1005     1\n");
    auto it_x_noswitch = result.sysnum_noswitch_latency.find(SYSNUM_X);
    assert(it_x_noswitch != result.sysnum_noswitch_latency.end());
    std::string hist_x_noswitch = it_x_noswitch->second->to_string();
    std::cerr << "Sysnum noswitch latency for " << SYSNUM_X << ":\n"
              << hist_x_noswitch << "\n";
    assert(hist_x_noswitch == "          95..     100     1\n");
    auto it_y_switch = result.sysnum_switch_latency.find(SYSNUM_Y);
    assert(it_y_switch != result.sysnum_switch_latency.end());
    std::string hist_y_switch = it_y_switch->second->to_string();
    std::cerr << "Sysnum switch latency for " << SYSNUM_Y << ":\n"
              << hist_y_switch << "\n";
    assert(hist_y_switch == "         200..     205     1\n");
    auto it_y_noswitch = result.sysnum_noswitch_latency.find(SYSNUM_Y);
    assert(it_y_noswitch != result.sysnum_noswitch_latency.end());
    std::string hist_y_noswitch = it_y_noswitch->second->to_string();
    std::cerr << "Sysnum noswitch latency for " << SYSNUM_Y << ":\n"
              << hist_y_noswitch << "\n";
    assert(hist_y_noswitch == "         200..     205     1\n");
    return true;
}

static bool
test_syscall_latencies_with_kernel_trace()
{
    static constexpr int64_t TID_A = 42;
    static constexpr int64_t TID_B = 142;
    static constexpr int64_t TID_C = 242;
    static constexpr int64_t TID_D = 199;
    static constexpr int64_t TID_E = 222;
    static constexpr uintptr_t SYSNUM_X = 12;
    static constexpr uintptr_t SYSNUM_Y = 167;
    std::vector<std::vector<memref_t>> memrefs = {
        {
            gen_instr(TID_A),
            // Involuntary switch: ignored for syscall latencies.
            gen_instr(TID_B),
            gen_marker(TID_B, TRACE_MARKER_TYPE_TIMESTAMP, 1100),
            gen_marker(TID_B, TRACE_MARKER_TYPE_SYSCALL, SYSNUM_X),
            gen_marker(TID_B, TRACE_MARKER_TYPE_SYSCALL_TRACE_START, SYSNUM_X),
            gen_instr(TID_B),
            gen_marker(TID_B, TRACE_MARKER_TYPE_SYSCALL_TRACE_END, SYSNUM_X),
            gen_marker(TID_B, TRACE_MARKER_TYPE_TIMESTAMP, 1600),
            // Voluntary switch: latency 500.
            gen_instr(TID_A),
            gen_instr(TID_A),
            gen_instr(TID_A),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 2100),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, SYSNUM_Y),
            gen_marker(TID_A, TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
            gen_marker(TID_A, TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH, TID_C),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_START, SYSNUM_Y),
            gen_instr(TID_A),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_END, SYSNUM_Y),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 2300),
            // Direct switch: latency 200.
            gen_marker(TID_C, TRACE_MARKER_TYPE_CONTEXT_SWITCH_START,
                       switch_type_t::SWITCH_THREAD),
            gen_instr(TID_C),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CONTEXT_SWITCH_END,
                       switch_type_t::SWITCH_THREAD),
            gen_instr(TID_C),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 2500),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL, SYSNUM_X),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL_TRACE_START, SYSNUM_X),
            gen_instr(TID_C),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL_TRACE_END, SYSNUM_X),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 2599),
            // No switch: latency 99 too small.
            gen_instr(TID_C),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 3100),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL, SYSNUM_Y),
            gen_marker(TID_C, TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH, TID_A),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL_TRACE_START, SYSNUM_Y),
            gen_instr(TID_C),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL_TRACE_END, SYSNUM_Y),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 3300),
            // Direct switch requested but failed: latency 200.
            gen_instr(TID_C),
            gen_exit(TID_C),
            gen_exit(TID_A),
        },
        {
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 3400),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL, SYSNUM_X),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL_TRACE_START, SYSNUM_X),
            gen_instr(TID_C),
            gen_marker(TID_C, TRACE_MARKER_TYPE_SYSCALL_TRACE_END, SYSNUM_X),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, 4400),
            // Voluntary switch: latency 1000.
            // Test a thread starting at a recorded syscall after another thread on a
            // core.
            gen_marker(TID_E, TRACE_MARKER_TYPE_TIMESTAMP, 4500),

            gen_marker(TID_E, TRACE_MARKER_TYPE_FUNC_ID, 202),
            gen_marker(TID_E, TRACE_MARKER_TYPE_FUNC_RETVAL, static_cast<uintptr_t>(-4)),
            gen_marker(TID_E, TRACE_MARKER_TYPE_SYSCALL_FAILED, 4),
            gen_marker(TID_E, TRACE_MARKER_TYPE_TIMESTAMP, 5200),
            // Preempt to D so no latency should be recorded here, despite
            // there being a syscall with no regular instr records in between.
            gen_instr(TID_D),
            gen_exit(TID_E),
            gen_exit(TID_B),
        },
        {
            // Test a thread starting at a recorded syscall first thing on a core.
            gen_marker(TID_D, TRACE_MARKER_TYPE_TIMESTAMP, 1500),
            gen_marker(TID_D, TRACE_MARKER_TYPE_FUNC_ID, 202),
            gen_marker(TID_D, TRACE_MARKER_TYPE_FUNC_RETVAL, static_cast<uintptr_t>(-4)),
            gen_marker(TID_D, TRACE_MARKER_TYPE_SYSCALL_FAILED, 4),
            gen_marker(TID_D, TRACE_MARKER_TYPE_TIMESTAMP, 3200),
            // Preempt to E so no latency should be recorded here.
            gen_instr(TID_E),
            gen_exit(TID_D),
        },
    };
    const std::vector<std::vector<schedule_record_t>> expected_record = {
        {
            { 0, TID_A, /*ins=*/1, /*sys#=*/-1, /*lat=*/-1 },
            { 0, TID_B, /*ins=*/2, /*sys#=*/12, /*lat=*/500, VOLUNTARY },
            { 0, TID_A, /*ins=*/4, /*sys#=*/167, /*lat=*/200, VOLUNTARY, DIRECT },
            { 0, TID_C, /*ins=*/6, /*sys#=*/-1, /*lat=*/-1, VOLUNTARY },
        },
        {
            { 0, TID_C, /*ins=*/1, /*sys#=*/12, /*lat=*/1000, VOLUNTARY },
            { 0, TID_E, /*ins=*/0, /*sys#=*/-1, /*lat=*/-1, VOLUNTARY },
            { 0, TID_D, /*ins=*/1, /*sys#=*/-1, /*lat=*/-1 },
            { 0, TID_E, /*ins=*/0, /*sys#=*/-1, /*lat=*/-1, VOLUNTARY },
        },
        {
            { 0, TID_D, /*ins=*/0, /*sys#=*/-1, /*lat=*/-1 },
            { 0, TID_E, /*ins=*/1, /*sys#=*/-1, /*lat=*/-1 },
        },
    };
    auto result = run_schedule_stats(memrefs, &expected_record);
    // Following are the same as test_syscall_latencies.
    auto it_x_switch = result.sysnum_switch_latency.find(SYSNUM_X);
    assert(it_x_switch != result.sysnum_switch_latency.end());
    assert(it_x_switch->second->to_string() ==
           "         500..     505     1\n        1000..    1005     1\n");
    auto it_x_noswitch = result.sysnum_noswitch_latency.find(SYSNUM_X);
    assert(it_x_noswitch != result.sysnum_noswitch_latency.end());
    assert(it_x_noswitch->second->to_string() == "          95..     100     1\n");
    auto it_y_switch = result.sysnum_switch_latency.find(SYSNUM_Y);
    assert(it_y_switch != result.sysnum_switch_latency.end());
    assert(it_y_switch->second->to_string() == "         200..     205     1\n");
    auto it_y_noswitch = result.sysnum_noswitch_latency.find(SYSNUM_Y);
    assert(it_y_noswitch != result.sysnum_noswitch_latency.end());
    assert(it_y_noswitch->second->to_string() == "         200..     205     1\n");
    return true;
}

} // namespace

int
test_main(int argc, const char *argv[])
{
    if (test_basic_stats() && test_basic_stats_with_syscall_trace() && test_idle() &&
        test_cpu_footprint() && test_syscall_latencies() &&
        test_syscall_latencies_with_kernel_trace()) {
        std::cerr << "schedule_stats_test passed\n";
        return 0;
    }
    std::cerr << "schedule_stats_test FAILED\n";
    exit(1);
}

} // namespace drmemtrace
} // namespace dynamorio
