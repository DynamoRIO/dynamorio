/* **********************************************************
 * Copyright (c) 2021-2024 Google, LLC  All rights reserved.
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

#undef NDEBUG
#include <assert.h>

#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "../tools/schedule_stats.h"
#include "../common/memref.h"
#include "memref_gen.h"

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

// Bypasses the analyzer and scheduler for a controlled test sequence.
// Alternates the per-core memref vectors in lockstep.
static schedule_stats_t::counters_t
run_schedule_stats(const std::vector<std::vector<memref_t>> &memrefs)
{
    // At verbosity 2+ we'd need to subclass default_memtrace_stream_t
    // and provide a non-null get_input_interface() (point at "this").
    mock_schedule_stats_t tool(/*print_every=*/1, /*verbosity=*/1);
    struct per_core_t {
        void *worker_data;
        void *shard_data;
        default_memtrace_stream_t stream;
        bool finished = false;
        size_t memref_idx = 0;
    };
    std::vector<per_core_t> per_core(memrefs.size());
    for (int cpu = 0; cpu < static_cast<int>(memrefs.size()); ++cpu) {
        per_core[cpu].worker_data = tool.parallel_worker_init(cpu);
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
            bool res = tool.parallel_shard_memref(per_core[cpu].shard_data, memref);
            assert(res);
            ++per_core[cpu].memref_idx;
            if (per_core[cpu].memref_idx >= memrefs[cpu].size()) {
                per_core[cpu].finished = true;
                ++num_finished;
            }
        }
    }
    for (int cpu = 0; cpu < static_cast<int>(memrefs.size()); ++cpu) {
        tool.parallel_shard_exit(per_core[cpu].shard_data);
        tool.parallel_worker_exit(per_core[cpu].worker_data);
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
    auto result = run_schedule_stats(memrefs);
    assert(result.instrs == 16);
    assert(result.total_switches == 7);
    assert(result.voluntary_switches == 3);
    assert(result.direct_switches == 1);
    assert(result.syscalls == 4);
    assert(result.maybe_blocking_syscalls == 3);
    assert(result.direct_switch_requests == 2);
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
            gen_marker(TID_B, TRACE_MARKER_TYPE_CORE_IDLE, 0),
            gen_marker(TID_B, TRACE_MARKER_TYPE_CORE_IDLE, 0),
            gen_marker(TID_B, TRACE_MARKER_TYPE_CORE_IDLE, 0),
            // A switch from idle w/ no syscall is an involuntary switch.
            gen_instr(TID_B),
            gen_instr(TID_B),
            gen_instr(TID_B),
            gen_exit(TID_B),
        },
        {
            gen_instr(TID_C),
            // Involuntary switch.
            gen_instr(TID_A),
            // Involuntary switch.
            gen_instr(TID_C),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CORE_IDLE, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CORE_IDLE, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CORE_IDLE, 0),
            // A switch from idle w/ no syscall is an involuntary switch.
            gen_instr(TID_C),
            gen_instr(TID_C),
            // Wait.
            gen_marker(TID_C, TRACE_MARKER_TYPE_CORE_WAIT, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CORE_WAIT, 0),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CORE_WAIT, 0),
            // Involuntary switch.
            gen_instr(TID_A),
            gen_instr(TID_A),
            gen_instr(TID_A),
            gen_exit(TID_A),
            // An exit is a voluntary switch.
            gen_exit(TID_C),
        },
    };
    auto result = run_schedule_stats(memrefs);
    assert(result.instrs == 13);
    assert(result.total_switches == 6);
    assert(result.voluntary_switches == 1);
    assert(result.direct_switches == 0);
    assert(result.syscalls == 0);
    assert(result.maybe_blocking_syscalls == 0);
    assert(result.direct_switch_requests == 0);
    assert(result.waits == 3);
    assert(result.idles == 6);
    assert(result.idle_microseconds >= 6);
    assert(result.idle_micros_at_last_instr > 0 &&
           result.idle_micros_at_last_instr <= result.idle_microseconds);
    assert(result.cpu_microseconds > 10);
    assert(result.wait_microseconds >= 3);
    return true;
}

} // namespace

int
test_main(int argc, const char *argv[])
{
    if (test_basic_stats() && test_idle()) {
        std::cerr << "schedule_stats_test passed\n";
        return 0;
    }
    std::cerr << "schedule_stats_test FAILED\n";
    exit(1);
}

} // namespace drmemtrace
} // namespace dynamorio
