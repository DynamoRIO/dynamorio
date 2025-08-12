/* **********************************************************
 * Copyright (c) 2016-2025 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#define NOMINMAX // Avoid windows.h messing up std::min.
#include <assert.h>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <random>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>
#include <utility>

#include "dr_api.h"
#include "scheduler.h"
#include "scheduler_impl.h"
#include "mock_reader.h"
#include "memref.h"
#include "trace_entry.h"
#include "noise_generator.h"
#ifdef HAS_ZIP
#    include "zipfile_istream.h"
#    include "zipfile_ostream.h"
#endif
#include "test_helpers.h"

namespace dynamorio {
namespace drmemtrace {

using ::dynamorio::drmemtrace::memtrace_stream_t;

#if (defined(X86_64) || defined(ARM_64)) && defined(HAS_ZIP)
struct trace_position_t {
    trace_position_t(uint64_t record, uint64_t instr, uint64_t timestamp)
        : record_ordinal(record)
        , instruction_ordinal(instr)
        , last_timestamp(timestamp)
    {
    }
    bool
    operator==(const trace_position_t &rhs)
    {
        return record_ordinal == rhs.record_ordinal &&
            instruction_ordinal == rhs.instruction_ordinal &&
            last_timestamp == rhs.last_timestamp;
    }
    uint64_t record_ordinal;
    uint64_t instruction_ordinal;
    uint64_t last_timestamp;
};

struct context_switch_t {
    context_switch_t(memref_tid_t prev_tid, memref_tid_t new_tid, trace_position_t output,
                     trace_position_t prev, trace_position_t next)
        : prev_tid(prev_tid)
        , new_tid(new_tid)
        , output_position(output)
        , prev_input_position(prev)
        , new_input_position(next)
    {
    }
    bool
    operator==(const context_switch_t &rhs)
    {
        return prev_tid == rhs.prev_tid && new_tid == rhs.new_tid &&
            output_position == rhs.output_position &&
            prev_input_position == rhs.prev_input_position &&
            new_input_position == rhs.new_input_position;
    }
    memref_tid_t prev_tid;
    memref_tid_t new_tid;
    trace_position_t output_position;
    trace_position_t prev_input_position;
    trace_position_t new_input_position;
};

static std::ostream &
operator<<(std::ostream &o, const trace_position_t &tp)
{
    // We are deliberately terse to keep the output on one line.
    return o << "<" << tp.record_ordinal << "," << tp.instruction_ordinal << ","
             << tp.last_timestamp << ">";
}

static std::ostream &
operator<<(std::ostream &o, const context_switch_t &cs)
{
    if (cs.prev_tid == INVALID_THREAD_ID) {
        // Initial thread: omit the transition and all the positions.
        return o << cs.new_tid;
    }
    return o << cs.prev_tid << " => " << cs.new_tid << " @ " << cs.output_position << " ("
             << cs.prev_input_position << " => " << cs.new_input_position << ")";
}
#endif /* (defined(X86_64) || defined(ARM_64)) && defined(HAS_ZIP) */

static bool
memref_is_nop_instr(memref_t &record)
{
    if (!type_is_instr(record.instr.type))
        return false;
    instr_noalloc_t noalloc;
    instr_noalloc_init(GLOBAL_DCONTEXT, &noalloc);
    instr_t *instr = instr_from_noalloc(&noalloc);
    app_pc pc =
        decode(GLOBAL_DCONTEXT, reinterpret_cast<app_pc>(record.instr.encoding), instr);
    return pc != nullptr && instr_is_nop(instr);
}

static void
verify_scheduler_stats(scheduler_t::stream_t *stream, int64_t switch_input_to_input,
                       int64_t switch_input_to_idle, int64_t switch_idle_to_input,
                       int64_t switch_nop, int64_t preempts, int64_t direct_attempts,
                       int64_t direct_successes, int64_t migrations)
{
    // We assume our counts fit in the get_schedule_statistic()'s double's 54-bit
    // mantissa and thus we can safely use "==".
    assert(stream->get_schedule_statistic(
               memtrace_stream_t::SCHED_STAT_SWITCH_INPUT_TO_INPUT) ==
           switch_input_to_input);
    assert(stream->get_schedule_statistic(
               memtrace_stream_t::SCHED_STAT_SWITCH_INPUT_TO_IDLE) ==
           switch_input_to_idle);
    assert(stream->get_schedule_statistic(
               memtrace_stream_t::SCHED_STAT_SWITCH_IDLE_TO_INPUT) ==
           switch_idle_to_input);
    assert(stream->get_schedule_statistic(memtrace_stream_t::SCHED_STAT_SWITCH_NOP) ==
           switch_nop);
    assert(stream->get_schedule_statistic(
               memtrace_stream_t::SCHED_STAT_QUANTUM_PREEMPTS) == preempts);
    assert(stream->get_schedule_statistic(
               memtrace_stream_t::SCHED_STAT_DIRECT_SWITCH_ATTEMPTS) == direct_attempts);
    assert(stream->get_schedule_statistic(
               memtrace_stream_t::SCHED_STAT_DIRECT_SWITCH_SUCCESSES) ==
           direct_successes);
    assert(stream->get_schedule_statistic(memtrace_stream_t::SCHED_STAT_MIGRATIONS) ==
           migrations);
}

// Returns a vector of strings, one per output, where each string has one char per input
// showing the order of inputs scheduled onto that output.
// Assumes the input threads are all tid_base plus an offset < 26.
// When send_time=true, the record count is passed to the scheduler as the current
// time, to avoid relying on wall-clock time.  For this use case of send_time=true,
// typically time_units_per_us should be set to 1 to avoid any scaling of the record
// count for simpler small tests.
static std::vector<std::string>
run_lockstep_simulation(scheduler_t &scheduler, int num_outputs, memref_tid_t tid_base,
                        bool send_time = false, bool print_markers = true,
                        bool skip_simultaenous_checks = false)
{
    // Walk the outputs in lockstep for crude but deterministic concurrency.
    std::vector<scheduler_t::stream_t *> outputs(num_outputs, nullptr);
    std::vector<bool> eof(num_outputs, false);
    for (int i = 0; i < num_outputs; i++)
        outputs[i] = scheduler.get_stream(i);
    int num_eof = 0;
    int64_t meta_records = 0;
    // Record the threads, one char each.
    std::vector<std::string> sched_as_string(num_outputs);
    static constexpr char THREAD_LETTER_START_USER = 'A';
    static constexpr char THREAD_LETTER_START_KERNEL = 'a';
    static constexpr char WAIT_SYMBOL = '-';
    static constexpr char IDLE_SYMBOL = '_';
    static constexpr char NON_INSTR_SYMBOL = '.';
    while (num_eof < num_outputs) {
        for (int i = 0; i < num_outputs; i++) {
            if (eof[i])
                continue;
            memref_t memref;
            scheduler_t::stream_status_t status;
            if (send_time) {
                // We assume IPC=1 and so send the instruction count (+1 to avoid an
                // invalid time of 0) which allows apples-to-apples comparisons with
                // instruction quanta.  This is a per-output time which technically
                // violates the globally-increasing requirement, so this will not work
                // perfectly with i/o waits, but should work fine for basic tests.
                // We add the wait and idle records to make progress with idle time.
                status = outputs[i]->next_record(
                    memref, outputs[i]->get_instruction_ordinal() + 1 + meta_records);
            } else {
                status = outputs[i]->next_record(memref);
            }
            if (status == scheduler_t::STATUS_EOF) {
                ++num_eof;
                eof[i] = true;
                continue;
            }
            if (status == scheduler_t::STATUS_WAIT) {
                sched_as_string[i] += WAIT_SYMBOL;
                ++meta_records;
                continue;
            }
            if (status == scheduler_t::STATUS_IDLE) {
                sched_as_string[i] += IDLE_SYMBOL;
                ++meta_records;
                continue;
            }
            assert(status == scheduler_t::STATUS_OK);
            // Ensure stream API and the trace records are consistent.
            assert(outputs[i]->get_input_interface()->get_tid() == IDLE_THREAD_ID ||
                   outputs[i]->get_input_interface()->get_tid() ==
                       tid_from_memref_tid(memref.instr.tid));
            assert(outputs[i]->get_input_interface()->get_workload_id() == INVALID_PID ||
                   outputs[i]->get_input_interface()->get_workload_id() ==
                       workload_from_memref_tid(memref.instr.tid));
            if (type_is_instr(memref.instr.type)) {
                bool is_kernel = outputs[i]->is_record_kernel();
                sched_as_string[i] +=
                    (is_kernel ? THREAD_LETTER_START_KERNEL : THREAD_LETTER_START_USER) +
                    static_cast<char>(memref.instr.tid - tid_base);
            } else {
                // While this makes the string longer, it is just too confusing
                // with the same letter seemingly on 2 cores at once without these
                // fillers to line everything up in time.
                sched_as_string[i] += NON_INSTR_SYMBOL;
            }
            assert(outputs[i]->get_shard_index() ==
                   outputs[i]->get_output_stream_ordinal());
        }
    }
    // Ensure we never see the same output on multiple cores in the same timestep.
    if (!skip_simultaenous_checks) {
        size_t max_size = 0;
        for (int i = 0; i < num_outputs; ++i)
            max_size = std::max(max_size, sched_as_string[i].size());
        for (int step = 0; step < static_cast<int>(max_size); ++step) {
            std::set<char> inputs;
            for (int out = 0; out < num_outputs; ++out) {
                if (static_cast<int>(sched_as_string[out].size()) <= step)
                    continue;
                if (sched_as_string[out][step] < 'A' || sched_as_string[out][step] > 'Z')
                    continue;
                assert(inputs.find(sched_as_string[out][step]) == inputs.end());
                inputs.insert(sched_as_string[out][step]);
            }
        }
    }
    if (!print_markers) {
        // We kept the dots internally for our same-timestep check above.
        for (int i = 0; i < num_outputs; ++i) {
            sched_as_string[i].erase(std::remove(sched_as_string[i].begin(),
                                                 sched_as_string[i].end(),
                                                 NON_INSTR_SYMBOL),
                                     sched_as_string[i].end());
        }
    }
    return sched_as_string;
}

static void
test_serial()
{
    std::cerr << "\n----------------\nTesting serial\n";
    static constexpr memref_tid_t TID_A = 42;
    static constexpr memref_tid_t TID_B = 99;
    std::vector<trace_entry_t> refs_A = {
        /* clang-format off */
        test_util::make_thread(TID_A),
        test_util::make_pid(1),
        // Include a header to test the scheduler queuing it.
        test_util::make_version(4),
        // Each timestamp is followed by an instr whose PC==time.
        test_util::make_timestamp(10),
        test_util::make_instr(10),
        test_util::make_timestamp(30),
        test_util::make_instr(30),
        test_util::make_timestamp(50),
        test_util::make_instr(50),
        test_util::make_exit(TID_A),
        /* clang-format on */
    };
    std::vector<trace_entry_t> refs_B = {
        /* clang-format off */
        test_util::make_thread(TID_B),
        test_util::make_pid(1),
        test_util::make_version(4),
        test_util::make_timestamp(20),
        test_util::make_instr(20),
        test_util::make_timestamp(40),
        test_util::make_instr(40),
        test_util::make_timestamp(60),
        test_util::make_instr(60),
        test_util::make_exit(TID_B),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(refs_A)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), TID_A);
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(refs_B)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), TID_B);
    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    if (scheduler.init(sched_inputs, 1,
                       scheduler_t::make_scheduler_serial_options(/*verbosity=*/4)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    auto *stream = scheduler.get_stream(0);
    memref_t memref;
    uint64_t last_timestamp = 0;
    memref_tid_t last_timestamp_tid = INVALID_THREAD_ID;
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
        // There is just one workload so we expect to always see 0 as the ordinal.
        assert(stream->get_input_workload_ordinal() == 0);
        if (memref.marker.type == TRACE_TYPE_MARKER &&
            memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP) {
            assert(memref.marker.marker_value > last_timestamp);
            last_timestamp = memref.marker.marker_value;
            // In our test case we have alternating threads.
            assert(last_timestamp_tid != memref.marker.tid);
            last_timestamp_tid = memref.marker.tid;
        }
    }
}

static void
test_parallel()
{
    std::cerr << "\n----------------\nTesting parallel\n";
    std::vector<trace_entry_t> input_sequence = {
        test_util::make_thread(1),
        test_util::make_pid(1),
        test_util::make_instr(42),
        test_util::make_exit(1),
    };
    static constexpr int NUM_INPUTS = 3;
    static constexpr int NUM_OUTPUTS = 2;
    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = 100 + i;
        inputs[i] = input_sequence;
        for (auto &record : inputs[i]) {
            if (record.type == TRACE_TYPE_THREAD || record.type == TRACE_TYPE_THREAD_EXIT)
                record.addr = static_cast<addr_t>(tid);
        }
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(inputs[i])),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            tid);
        sched_inputs.emplace_back(std::move(readers));
    }
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS,
                       scheduler_t::make_scheduler_parallel_options(/*verbosity=*/4)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::unordered_map<memref_tid_t, int> tid2stream;
    int count = 0;
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        auto *stream = scheduler.get_stream(i);
        memref_t memref;
        for (scheduler_t::stream_status_t status = stream->next_record(memref);
             status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
            assert(status == scheduler_t::STATUS_OK);
            ++count;
            // Ensure one input thread is only in one output stream.
            if (tid2stream.find(memref.instr.tid) == tid2stream.end())
                tid2stream[memref.instr.tid] = i;
            else
                assert(tid2stream[memref.instr.tid] == i);
            // Ensure the ordinals do not accumulate across inputs.
            assert(stream->get_record_ordinal() ==
                       scheduler
                           .get_input_stream_interface(stream->get_input_stream_ordinal())
                           ->get_record_ordinal() ||
                   // Relax for early on where the scheduler has read ahead.
                   stream->get_last_timestamp() == 0);
            assert(
                stream->get_instruction_ordinal() ==
                scheduler.get_input_stream_interface(stream->get_input_stream_ordinal())
                    ->get_instruction_ordinal());
            // Test other queries in parallel mode.
            assert(stream->get_tid() == memref.instr.tid);
            assert(stream->get_shard_index() == stream->get_input_stream_ordinal());
        }
    }
    // We expect just 2 records (instr and exit) for each.
    assert(count == 2 * NUM_INPUTS);
}

static void
test_invalid_regions()
{
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), 1);
    std::vector<scheduler_t::range_t> regions;
    // Instr counts are 1-based so 0 is an invalid start.
    regions.emplace_back(0, 2);
    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    sched_inputs[0].thread_modifiers.push_back(scheduler_t::input_thread_info_t(regions));
    assert(
        scheduler.init(sched_inputs, 1, scheduler_t::make_scheduler_serial_options()) ==
        scheduler_t::STATUS_ERROR_INVALID_PARAMETER);

    // Test stop > start.
    sched_inputs[0].thread_modifiers[0].regions_of_interest[0].start_instruction = 2;
    sched_inputs[0].thread_modifiers[0].regions_of_interest[0].stop_instruction = 1;
    assert(
        scheduler.init(sched_inputs, 1, scheduler_t::make_scheduler_serial_options()) ==
        scheduler_t::STATUS_ERROR_INVALID_PARAMETER);

    // Test overlapping regions.
    sched_inputs[0].thread_modifiers[0].regions_of_interest[0].start_instruction = 2;
    sched_inputs[0].thread_modifiers[0].regions_of_interest[0].stop_instruction = 10;
    sched_inputs[0].thread_modifiers[0].regions_of_interest.emplace_back(10, 20);
    assert(
        scheduler.init(sched_inputs, 1, scheduler_t::make_scheduler_serial_options()) ==
        scheduler_t::STATUS_ERROR_INVALID_PARAMETER);
    sched_inputs[0].thread_modifiers[0].regions_of_interest[0].start_instruction = 2;
    sched_inputs[0].thread_modifiers[0].regions_of_interest[0].stop_instruction = 10;
    sched_inputs[0].thread_modifiers[0].regions_of_interest[1].start_instruction = 4;
    sched_inputs[0].thread_modifiers[0].regions_of_interest[1].stop_instruction = 12;
    assert(
        scheduler.init(sched_inputs, 1, scheduler_t::make_scheduler_serial_options()) ==
        scheduler_t::STATUS_ERROR_INVALID_PARAMETER);
}

static void
test_legacy_fields()
{
    std::cerr << "\n----------------\nTesting legacy fields\n";
    static constexpr int NUM_INPUTS = 7;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int NUM_INSTRS = 9;
    static constexpr int QUANTUM_DURATION = 3;
    // We do not want to block for very long.
    static constexpr uint64_t BLOCK_LATENCY = 200;
    static constexpr uint64_t BLOCK_THRESHOLD = 100;
    static constexpr double BLOCK_SCALE = 0.01;
    static constexpr uint64_t BLOCK_MAX = 50;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr uint64_t START_TIME = 20;
    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = TID_BASE + i;
        inputs[i].push_back(test_util::make_thread(tid));
        inputs[i].push_back(test_util::make_pid(1));
        inputs[i].push_back(test_util::make_version(TRACE_ENTRY_VERSION));
        inputs[i].push_back(
            test_util::make_timestamp(START_TIME)); // All the same time priority.
        for (int j = 0; j < NUM_INSTRS; j++) {
            inputs[i].push_back(test_util::make_instr(42 + j * 4));
            // Including blocking syscalls.
            if ((i == 0 || i == 1) && j == 1) {
                inputs[i].push_back(test_util::make_timestamp(START_TIME * 2));
                inputs[i].push_back(
                    test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 42));
                inputs[i].push_back(
                    test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0));
                inputs[i].push_back(
                    test_util::make_timestamp(START_TIME * 2 + BLOCK_LATENCY));
            }
        }
        inputs[i].push_back(test_util::make_exit(tid));
    }
    {
        // Test invalid quantum.
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(inputs[0])),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_BASE);
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS);
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.quantum_duration = QUANTUM_DURATION;
        scheduler_t scheduler;
        assert(scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) ==
               scheduler_t::STATUS_ERROR_INVALID_PARAMETER);
    }
    {
        // Test invalid block scale.
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(inputs[0])),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_BASE);
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS);
        sched_ops.block_time_scale = BLOCK_SCALE;
        scheduler_t scheduler;
        assert(scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) ==
               scheduler_t::STATUS_ERROR_INVALID_PARAMETER);
    }
    {
        // Test invalid block max.
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(inputs[0])),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_BASE);
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS);
        sched_ops.block_time_max = BLOCK_MAX;
        scheduler_t scheduler;
        assert(scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) ==
               scheduler_t::STATUS_ERROR_INVALID_PARAMETER);
    }
    {
        // Test valid legacy fields.
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        for (int i = 0; i < NUM_INPUTS; i++) {
            std::vector<scheduler_t::input_reader_t> readers;
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs[i])),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                TID_BASE + i);
            sched_inputs.emplace_back(std::move(readers));
        }
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/4);
        // Simulate binary compatibility with a legacy struct.
        sched_ops.struct_size =
            offsetof(scheduler_t::scheduler_options_t, time_units_per_us);
        sched_ops.quantum_duration_us = QUANTUM_DURATION;
        // This was tuned with a 100us threshold: so avoid scheduler.h defaults
        // changes from affecting our output.
        sched_ops.blocking_switch_threshold = BLOCK_THRESHOLD;
        sched_ops.block_time_scale = BLOCK_SCALE;
        sched_ops.block_time_max = BLOCK_MAX;
        // To do our test we use instrs-as-time for deterministic block times.
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.time_units_per_us = 1.;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
        // Hardcoding here for the 2 outputs and 7 inputs.
        // We expect 3 letter sequences (our quantum) alternating every-other as each
        // core alternates. The dots are markers and thread exits.
        // A and B have a voluntary switch after their 1st 2 letters, but we expect
        // the usage to persist to their next scheduling which should only have
        // a single letter.
        static const char *const CORE0_SCHED_STRING =
            "..AA......CCC..EEE..GGGACCCEEEGGGAAACCC.EEGGAAE.G.A.";
        static const char *const CORE1_SCHED_STRING =
            "..BB......DDD..FFFBDDDFFFBBBDDD.FFF.BBB.____________";
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
        assert(sched_as_string[1] == CORE1_SCHED_STRING);
    }
}

static void
test_param_checks()
{
    test_invalid_regions();
    test_legacy_fields();
}

// Tests regions without timestamps for a simple, direct test.
static void
test_regions_bare()
{
    std::cerr << "\n----------------\nTesting bare regions\n";
    std::vector<trace_entry_t> memrefs = {
        /* clang-format off */
        test_util::make_thread(1),
        test_util::make_pid(1),
        test_util::make_marker(TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
        test_util::make_instr(1),
        test_util::make_instr(2), // Region 1 is just this instr.
        test_util::make_instr(3),
        test_util::make_instr(4), // Region 2 is just this instr.
        test_util::make_instr(5), // Region 3 is just this instr.
        test_util::make_instr(6),
        test_util::make_instr(7),
        test_util::make_instr(8), // Region 4 starts here.
        test_util::make_instr(9), // Region 4 ends here.
        test_util::make_instr(10),
        test_util::make_exit(1),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(memrefs)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), 1);

    std::vector<scheduler_t::range_t> regions;
    // Instr counts are 1-based.
    regions.emplace_back(2, 2);
    regions.emplace_back(4, 4);
    regions.emplace_back(5, 5);
    regions.emplace_back(8, 9);

    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    sched_inputs[0].thread_modifiers.push_back(scheduler_t::input_thread_info_t(regions));
    // Without timestmaps we can't use the serial options.
    if (scheduler.init(sched_inputs, 1,
                       scheduler_t::scheduler_options_t(
                           scheduler_t::MAP_TO_ANY_OUTPUT, scheduler_t::DEPENDENCY_IGNORE,
                           scheduler_t::SCHEDULER_DEFAULTS,
                           /*verbosity=*/4)) != scheduler_t::STATUS_SUCCESS)
        assert(false);
    int ordinal = 0;
    auto *stream = scheduler.get_stream(0);
    memref_t memref;
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
        switch (ordinal) {
        case 0:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 2);
            break;
        case 1:
            assert(memref.marker.type == TRACE_TYPE_MARKER);
            assert(memref.marker.marker_type == TRACE_MARKER_TYPE_WINDOW_ID);
            assert(memref.marker.marker_value == 1);
            break;
        case 2:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 4);
            break;
        case 3:
            assert(memref.marker.type == TRACE_TYPE_MARKER);
            assert(memref.marker.marker_type == TRACE_MARKER_TYPE_WINDOW_ID);
            assert(memref.marker.marker_value == 2);
            break;
        case 4:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 5);
            break;
        case 5:
            assert(memref.marker.type == TRACE_TYPE_MARKER);
            assert(memref.marker.marker_type == TRACE_MARKER_TYPE_WINDOW_ID);
            assert(memref.marker.marker_value == 3);
            break;
        case 6:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 8);
            break;
        case 7:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 9);
            break;
        default: assert(ordinal == 8); assert(memref.exit.type == TRACE_TYPE_THREAD_EXIT);
        }
        ++ordinal;
    }
    assert(ordinal == 9);
}

// Tests regions without timestamps with an instr at the very front of the trace.
static void
test_regions_bare_no_marker()
{
    std::cerr << "\n----------------\nTesting bare regions with no marker\n";
    std::vector<trace_entry_t> memrefs = {
        /* clang-format off */
        test_util::make_thread(1),
        test_util::make_pid(1),
        test_util::make_marker(TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
        // This would not happen in a real trace, only in tests.  But it does
        // match a dynamic skip from the middle when an instruction has already
        // been read but not yet passed to the output stream.
        test_util::make_instr(1),
        test_util::make_instr(2), // The region skips the 1st instr.
        test_util::make_instr(3),
        test_util::make_instr(4),
        test_util::make_exit(1),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(memrefs)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), 1);

    std::vector<scheduler_t::range_t> regions;
    // Instr counts are 1-based.
    regions.emplace_back(2, 0);

    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    sched_inputs[0].thread_modifiers.push_back(scheduler_t::input_thread_info_t(regions));
    // Without timestmaps we can't use the serial options.
    if (scheduler.init(sched_inputs, 1,
                       scheduler_t::scheduler_options_t(
                           scheduler_t::MAP_TO_ANY_OUTPUT, scheduler_t::DEPENDENCY_IGNORE,
                           scheduler_t::SCHEDULER_DEFAULTS,
                           /*verbosity=*/4)) != scheduler_t::STATUS_SUCCESS)
        assert(false);
    int ordinal = 0;
    auto *stream = scheduler.get_stream(0);
    memref_t memref;
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
        switch (ordinal) {
        case 0:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 2);
            break;
        case 1:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 3);
            break;
        case 2:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 4);
            break;
        default: assert(ordinal == 3); assert(memref.exit.type == TRACE_TYPE_THREAD_EXIT);
        }
        ++ordinal;
    }
    assert(ordinal == 4);
}

static void
test_regions_timestamps()
{
    std::cerr << "\n----------------\nTesting regions\n";
    std::vector<trace_entry_t> memrefs = {
        /* clang-format off */
        test_util::make_thread(1),
        test_util::make_pid(1),
        test_util::make_marker(TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
        test_util::make_timestamp(10),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 1),
        test_util::make_instr(1),
        test_util::make_instr(2), // Region 1 is just this instr.
        test_util::make_instr(3),
        test_util::make_timestamp(20),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 2),
        test_util::make_timestamp(30),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 3),
        test_util::make_instr(4),
        test_util::make_timestamp(40),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 4),
        test_util::make_instr(5),
        test_util::make_instr(6), // Region 2 starts here.
        test_util::make_timestamp(50),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 5),
        test_util::make_instr(7), // Region 2 ends here.
        test_util::make_instr(8),
        test_util::make_exit(1),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(memrefs)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), 1);

    std::vector<scheduler_t::range_t> regions;
    // Instr counts are 1-based.
    regions.emplace_back(2, 2);
    regions.emplace_back(6, 7);

    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    sched_inputs[0].thread_modifiers.push_back(scheduler_t::input_thread_info_t(regions));
    if (scheduler.init(sched_inputs, 1,
                       scheduler_t::make_scheduler_serial_options(/*verbosity=*/4)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    int ordinal = 0;
    auto *stream = scheduler.get_stream(0);
    memref_t memref;
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
        switch (ordinal) {
        case 0:
            assert(memref.marker.type == TRACE_TYPE_MARKER);
            assert(memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP);
            assert(memref.marker.marker_value == 10);
            break;
        case 1:
            assert(memref.marker.type == TRACE_TYPE_MARKER);
            assert(memref.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID);
            assert(memref.marker.marker_value == 1);
            break;
        case 2:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 2);
            break;
        case 3:
            assert(memref.marker.type == TRACE_TYPE_MARKER);
            assert(memref.marker.marker_type == TRACE_MARKER_TYPE_WINDOW_ID);
            assert(memref.marker.marker_value == 1);
            break;
        case 4:
            assert(memref.marker.type == TRACE_TYPE_MARKER);
            assert(memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP);
            assert(memref.marker.marker_value == 40);
            break;
        case 5:
            assert(memref.marker.type == TRACE_TYPE_MARKER);
            assert(memref.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID);
            assert(memref.marker.marker_value == 4);
            break;
        case 6:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 6);
            break;
        case 7:
            assert(memref.marker.type == TRACE_TYPE_MARKER);
            assert(memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP);
            assert(memref.marker.marker_value == 50);
            break;
        case 8:
            assert(memref.marker.type == TRACE_TYPE_MARKER);
            assert(memref.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID);
            assert(memref.marker.marker_value == 5);
            break;
        case 9:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 7);
            break;
        default:
            assert(ordinal == 10);
            assert(memref.exit.type == TRACE_TYPE_THREAD_EXIT);
        }
        ++ordinal;
    }
    assert(ordinal == 11);
}

static void
test_regions_start()
{
    std::cerr << "\n----------------\nTesting region at start\n";
    std::vector<trace_entry_t> memrefs = {
        /* clang-format off */
        test_util::make_thread(1),
        test_util::make_pid(1),
        test_util::make_marker(TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
        test_util::make_timestamp(10),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 1),
        test_util::make_instr(1), // Region 1 starts at the start.
        test_util::make_instr(2),
        test_util::make_exit(1),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(memrefs)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), 1);

    std::vector<scheduler_t::range_t> regions;
    // Instr counts are 1-based.
    regions.emplace_back(1, 0);

    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    sched_inputs[0].thread_modifiers.push_back(scheduler_t::input_thread_info_t(regions));
    if (scheduler.init(sched_inputs, 1,
                       scheduler_t::make_scheduler_serial_options(/*verbosity=*/5)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    int ordinal = 0;
    auto *stream = scheduler.get_stream(0);
    memref_t memref;
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
        // Because we skipped, even if not very far, we do not see the page marker.
        switch (ordinal) {
        case 0:
            assert(memref.marker.type == TRACE_TYPE_MARKER);
            assert(memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP);
            break;
        case 1:
            assert(memref.marker.type == TRACE_TYPE_MARKER);
            assert(memref.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID);
            break;
        case 2:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 1);
            break;
        case 3:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 2);
            break;
        default: assert(ordinal == 4); assert(memref.exit.type == TRACE_TYPE_THREAD_EXIT);
        }
        ++ordinal;
    }
    assert(ordinal == 5);
}

static void
test_regions_too_far()
{
    std::cerr << "\n----------------\nTesting region going too far\n";
    std::vector<trace_entry_t> memrefs = {
        /* clang-format off */
        test_util::make_thread(1),
        test_util::make_pid(1),
        test_util::make_marker(TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
        test_util::make_timestamp(10),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 1),
        test_util::make_instr(1),
        test_util::make_instr(2),
        test_util::make_exit(1),
        test_util::make_footer(),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(memrefs)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), 1);

    std::vector<scheduler_t::range_t> regions;
    // Start beyond the last instruction.
    regions.emplace_back(3, 0);

    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    sched_inputs[0].thread_modifiers.push_back(scheduler_t::input_thread_info_t(regions));
    auto status = scheduler.init(
        sched_inputs, 1, scheduler_t::make_scheduler_serial_options(/*verbosity=*/4));
    assert(status == scheduler_t::STATUS_ERROR_RANGE_INVALID);
}

static void
test_regions_core_sharded()
{
    std::cerr << "\n----------------\nTesting region on core-sharded-on-disk trace\n";
    static constexpr memref_tid_t TID_A = 42;
    static constexpr memref_tid_t TID_B = 99;
    static constexpr addr_t PC_POST_FOOTER = 101;
    std::vector<trace_entry_t> memrefs = {
        /* clang-format off */
        test_util::make_thread(TID_A),
        test_util::make_pid(1),
        test_util::make_marker(TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
        test_util::make_timestamp(10),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 1),
        test_util::make_instr(1),
        test_util::make_instr(2),
        test_util::make_exit(TID_A),
        // Test skipping across a footer.
        test_util::make_footer(),
        test_util::make_thread(TID_B),
        test_util::make_pid(1),
        test_util::make_marker(TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
        test_util::make_timestamp(10),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 1),
        test_util::make_instr(PC_POST_FOOTER),
        test_util::make_instr(PC_POST_FOOTER + 1),
        test_util::make_exit(TID_B),
        test_util::make_footer(),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(memrefs)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), 1);

    std::vector<scheduler_t::range_t> regions;
    // Start beyond the footer.
    regions.emplace_back(3, 0);

    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    sched_inputs[0].thread_modifiers.push_back(scheduler_t::input_thread_info_t(regions));
    if (scheduler.init(sched_inputs, 1,
                       scheduler_t::make_scheduler_serial_options(/*verbosity=*/5)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    int ordinal = 0;
    auto *stream = scheduler.get_stream(0);
    memref_t memref;
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
        // Because we skipped, even if not very far, we do not see the page marker.
        switch (ordinal) {
        case 0:
            assert(memref.marker.type == TRACE_TYPE_MARKER);
            assert(memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP);
            break;
        case 1:
            assert(memref.marker.type == TRACE_TYPE_MARKER);
            assert(memref.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID);
            break;
        case 2:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == PC_POST_FOOTER);
            break;
        case 3:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == PC_POST_FOOTER + 1);
            break;
        default: assert(ordinal == 4); assert(memref.exit.type == TRACE_TYPE_THREAD_EXIT);
        }
        ++ordinal;
    }
    assert(ordinal == 5);
}

static void
test_regions_by_shard()
{
    std::cerr << "\n----------------\nTesting ROI specified by shard\n";
    static constexpr int NUM_WORKLOADS = 2;
    static constexpr int NUM_CORES_PER_WORKLOAD = 2;
    static constexpr int NUM_OUTPUTS = NUM_WORKLOADS * NUM_CORES_PER_WORKLOAD;
    static constexpr int NUM_ORIGINAL_INPUTS = 3;
    static constexpr int NUM_INSTRS = 9;
    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    // This is core-sharded with interleaved threads on each core.
    for (int workload_idx = 0; workload_idx < NUM_WORKLOADS; workload_idx++) {
        std::vector<scheduler_t::input_reader_t> readers;
        for (int core_idx = 0; core_idx < NUM_CORES_PER_WORKLOAD; core_idx++) {
            std::vector<trace_entry_t> inputs;
            for (int input_idx = 0; input_idx < NUM_ORIGINAL_INPUTS; input_idx++) {
                inputs.push_back(test_util::make_thread(TID_BASE + input_idx));
                inputs.push_back(
                    test_util::make_pid(1)); // Test the same pid across workloads.
            }
            // Deliberately interleave all threads on every core.
            for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
                for (int input_idx = 0; input_idx < NUM_ORIGINAL_INPUTS; input_idx++) {
                    inputs.push_back(test_util::make_thread(TID_BASE + input_idx));
                    inputs.push_back(test_util::make_instr(42 + instr_idx * 4));
                }
            }
            for (int input_idx = 0; input_idx < NUM_ORIGINAL_INPUTS; input_idx++) {
                inputs.push_back(test_util::make_exit(TID_BASE + input_idx));
            }
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs)),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                -1 /*sentinel*/);
        }
        sched_inputs.emplace_back(std::move(readers));
    }
    // Set up different skips on each input, increasing by one as we go.
    for (int workload_idx = 0; workload_idx < NUM_WORKLOADS; workload_idx++) {
        std::vector<scheduler_t::input_reader_t> readers;
        for (int core_idx = 0; core_idx < NUM_CORES_PER_WORKLOAD; core_idx++) {
            std::vector<scheduler_t::range_t> regions;
            regions.emplace_back(
                1 /*1-based*/ + workload_idx * NUM_CORES_PER_WORKLOAD + core_idx, 0);
            scheduler_t::input_thread_info_t mod(regions);
            mod.shards = { core_idx };
            sched_inputs[workload_idx].thread_modifiers.push_back(mod);
        }
    }
    // Now run pre-scheduled.
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_CONSISTENT_OUTPUT,
                                               scheduler_t::DEPENDENCY_IGNORE,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/1);
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::string> sched_as_string = run_lockstep_simulation(
        scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/false, /*print_markers=*/true,
        /*skip_simultaenous_checks=*/true);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    // Each core was the same length but we've skipped ahead further in each
    // so they get shorter as the output ordinal increases:
    assert(sched_as_string[0] == "BCABCABCABCABCABCABCABCABC...");
    assert(sched_as_string[1] == "CABCABCABCABCABCABCABCABC...");
    assert(sched_as_string[2] == "ABCABCABCABCABCABCABCABC...");
    assert(sched_as_string[3] == "BCABCABCABCABCABCABCABC...");
}

static void
test_regions()
{
    test_regions_timestamps();
    test_regions_bare();
    test_regions_bare_no_marker();
    test_regions_start();
    test_regions_too_far();
    test_regions_core_sharded();
    test_regions_by_shard();
}

static void
test_only_threads()
{
    std::cerr << "\n----------------\nTesting thread filters\n";
    // Test with synthetic streams and readers.
    // The test_real_file_queries_and_filters() tests real files.
    static constexpr memref_tid_t TID_A = 42;
    static constexpr memref_tid_t TID_B = 99;
    static constexpr memref_tid_t TID_C = 7;
    std::vector<trace_entry_t> refs_A = {
        test_util::make_thread(TID_A),
        test_util::make_pid(1),
        test_util::make_instr(50),
        test_util::make_exit(TID_A),
    };
    std::vector<trace_entry_t> refs_B = {
        test_util::make_thread(TID_B),
        test_util::make_pid(1),
        test_util::make_instr(60),
        test_util::make_exit(TID_B),
    };
    std::vector<trace_entry_t> refs_C = {
        test_util::make_thread(TID_C),
        test_util::make_pid(1),
        test_util::make_instr(60),
        test_util::make_exit(TID_C),
    };
    auto create_readers = [&]() {
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_B)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_B);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_C)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_C);
        return readers;
    };

    {
        // Test valid only_threads.
        std::vector<scheduler_t::input_reader_t> readers = create_readers();
        scheduler_t scheduler;
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        sched_inputs[0].only_threads.insert(TID_B);
        if (scheduler.init(sched_inputs, 1,
                           scheduler_t::make_scheduler_serial_options(/*verbosity=*/4)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        auto *stream = scheduler.get_stream(0);
        memref_t memref;
        bool read_something = false;
        for (scheduler_t::stream_status_t status = stream->next_record(memref);
             status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
            assert(status == scheduler_t::STATUS_OK);
            assert(memref.instr.tid == TID_B);
            read_something = true;
        }
        assert(read_something);
    }
    {
        // Test invalid only_threads.
        std::vector<scheduler_t::input_reader_t> readers = create_readers();
        scheduler_t scheduler;
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        sched_inputs[0].only_threads = { TID_A, TID_B + 1, TID_C };
        if (scheduler.init(sched_inputs, 1,
                           scheduler_t::make_scheduler_serial_options(/*verbosity=*/4)) !=
            scheduler_t::STATUS_ERROR_INVALID_PARAMETER)
            assert(false);
    }
    {
        // Test valid only_shards.
        std::vector<scheduler_t::input_reader_t> readers = create_readers();
        scheduler_t scheduler;
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        sched_inputs[0].only_shards = { 0, 2 };
        if (scheduler.init(sched_inputs, 1,
                           scheduler_t::make_scheduler_parallel_options(
                               /*verbosity=*/4)) != scheduler_t::STATUS_SUCCESS)
            assert(false);
        auto *stream = scheduler.get_stream(0);
        memref_t memref;
        for (scheduler_t::stream_status_t status = stream->next_record(memref);
             status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
            assert(status == scheduler_t::STATUS_OK);
            assert(memref.instr.tid == TID_A || memref.instr.tid == TID_C);
        }
    }
    {
        // Test too-large only_shards.
        std::vector<scheduler_t::input_reader_t> readers = create_readers();
        scheduler_t scheduler;
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        sched_inputs[0].only_shards = { 1, 3 };
        if (scheduler.init(sched_inputs, 1,
                           scheduler_t::make_scheduler_serial_options(/*verbosity=*/4)) !=
            scheduler_t::STATUS_ERROR_INVALID_PARAMETER)
            assert(false);
    }
    {
        // Test too-small only_shards.
        std::vector<scheduler_t::input_reader_t> readers = create_readers();
        scheduler_t scheduler;
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        sched_inputs[0].only_shards = { 0, -1, 2 };
        if (scheduler.init(sched_inputs, 1,
                           scheduler_t::make_scheduler_serial_options(/*verbosity=*/4)) !=
            scheduler_t::STATUS_ERROR_INVALID_PARAMETER)
            assert(false);
    }
    {
        // Test starts-idle with only_shards.
        std::vector<trace_entry_t> refs_D = {
            test_util::make_version(TRACE_ENTRY_VERSION),
            test_util::make_thread(IDLE_THREAD_ID),
            test_util::make_pid(INVALID_PID),
            test_util::make_timestamp(static_cast<uint64_t>(-1)),
            test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, static_cast<uintptr_t>(-1)),
            test_util::make_marker(TRACE_MARKER_TYPE_CORE_IDLE, 0),
            test_util::make_marker(TRACE_MARKER_TYPE_CORE_IDLE, 0),
            test_util::make_marker(TRACE_MARKER_TYPE_CORE_IDLE, 0),
            test_util::make_footer(),
        };
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_B)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_B);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_D)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_C);
        scheduler_t scheduler;
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        sched_inputs[0].only_shards = { 0, 2 };
        if (scheduler.init(sched_inputs, 1,
                           scheduler_t::make_scheduler_parallel_options(
                               /*verbosity=*/4)) != scheduler_t::STATUS_SUCCESS)
            assert(false);
        auto *stream = scheduler.get_stream(0);
        memref_t memref;
        int idle_count = 0;
        for (scheduler_t::stream_status_t status = stream->next_record(memref);
             status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
            if (status == scheduler_t::STATUS_IDLE) {
                ++idle_count;
                continue;
            }
            assert(status == scheduler_t::STATUS_OK);
            assert(memref.instr.tid == TID_A || memref.instr.tid == IDLE_THREAD_ID ||
                   // In 32-bit the -1 is unsigned so the 64-bit .tid field is not
                   // sign-extended.
                   static_cast<uint64_t>(memref.instr.tid) ==
                       static_cast<addr_t>(IDLE_THREAD_ID) ||
                   memref.instr.tid == INVALID_THREAD_ID);
        }
        assert(idle_count == 3);
    }
}

static void
test_real_file_queries_and_filters(const char *testdir)
{
    std::cerr << "\n----------------\nTesting real files\n";
    // Test with real files as that is a separate code path in the scheduler.
    // Since 32-bit memref_t is a different size we limit these to 64-bit builds.
#if (defined(X86_64) || defined(ARM_64)) && defined(HAS_ZIP) && defined(HAS_SNAPPY)
    std::string trace1 = std::string(testdir) + "/drmemtrace.chase-snappy.x64.tracedir";
    // This trace has 2 threads: we pick the smallest one.
    static constexpr memref_tid_t TID_1_A = 23699;
    std::string trace2 = std::string(testdir) + "/drmemtrace.threadsig.x64.tracedir";
    // This trace has many threads: we pick 2 of the smallest.
    static constexpr memref_tid_t TID_2_A = 872905;
    static constexpr memref_tid_t TID_2_B = 872906;
    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(trace1);
    sched_inputs[0].only_threads.insert(TID_1_A);
    sched_inputs.emplace_back(trace2);
    sched_inputs[1].only_threads.insert(TID_2_A);
    sched_inputs[1].only_threads.insert(TID_2_B);
    if (scheduler.init(sched_inputs, 1,
                       scheduler_t::make_scheduler_serial_options(/*verbosity=*/1)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    auto *stream = scheduler.get_stream(0);
    memref_t memref;
    int max_workload_index = 0;
    int max_input_index = 0;
    std::set<memref_tid_t> tids_seen;
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
        assert(memref.instr.tid == TID_1_A || memref.instr.tid == TID_2_A ||
               memref.instr.tid == TID_2_B);
        tids_seen.insert(memref.instr.tid);
        if (stream->get_input_workload_ordinal() > max_workload_index)
            max_workload_index = stream->get_input_workload_ordinal();
        if (stream->get_input_stream_ordinal() > max_input_index)
            max_input_index = stream->get_input_stream_ordinal();
        if (stream->get_input_stream_ordinal() == 0)
            assert(stream->get_input_workload_ordinal() == 0);
        else
            assert(stream->get_input_workload_ordinal() == 1);
        // Interface sanity checks for the memtrace_stream_t versions.
        assert(stream->get_workload_id() == stream->get_input_workload_ordinal());
        assert(stream->get_input_id() == stream->get_input_stream_ordinal());
        assert(stream->get_input_interface() ==
               scheduler.get_input_stream_interface(stream->get_input_stream_ordinal()));
        assert(stream->get_tid() == memref.instr.tid);
        assert(stream->get_shard_index() == stream->get_input_stream_ordinal());
    }
    // Ensure 2 input workloads with 3 streams with proper names.
    assert(max_workload_index == 1);
    assert(max_input_index == 2);
    assert(scheduler.get_input_stream_count() == 3);
    assert(scheduler.get_input_stream_name(0) ==
           "chase.20190225.185346.23699.memtrace.sz");
    // These could be in any order (dir listing determines that).
    assert(scheduler.get_input_stream_name(1) ==
               "drmemtrace.threadsig.872905.5783.trace.zip" ||
           scheduler.get_input_stream_name(1) ==
               "drmemtrace.threadsig.872906.1041.trace.zip");
    assert(scheduler.get_input_stream_name(2) ==
               "drmemtrace.threadsig.872905.5783.trace.zip" ||
           scheduler.get_input_stream_name(2) ==
               "drmemtrace.threadsig.872906.1041.trace.zip");
    // Ensure all tids were seen.
    assert(tids_seen.size() == 3);
    assert(tids_seen.find(TID_1_A) != tids_seen.end() &&
           tids_seen.find(TID_2_A) != tids_seen.end() &&
           tids_seen.find(TID_2_B) != tids_seen.end());
#endif
}

static void
test_synthetic()
{
    std::cerr << "\n----------------\nTesting synthetic\n";
    static constexpr int NUM_INPUTS = 7;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int NUM_INSTRS = 9;
    static constexpr int QUANTUM_DURATION = 3;
    // We do not want to block for very long.
    static constexpr double BLOCK_SCALE = 0.01;
    static constexpr uint64_t BLOCK_THRESHOLD = 100;
    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = TID_BASE + i;
        inputs[i].push_back(test_util::make_thread(tid));
        inputs[i].push_back(test_util::make_pid(1));
        inputs[i].push_back(test_util::make_version(TRACE_ENTRY_VERSION));
        inputs[i].push_back(test_util::make_timestamp(10)); // All the same time priority.
        for (int j = 0; j < NUM_INSTRS; j++) {
            inputs[i].push_back(test_util::make_instr(42 + j * 4));
            // Test accumulation of usage across voluntary switches.
            if ((i == 0 || i == 1) && j == 1) {
                inputs[i].push_back(test_util::make_timestamp(20));
                inputs[i].push_back(
                    test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 42));
                inputs[i].push_back(
                    test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0));
                inputs[i].push_back(test_util::make_timestamp(120));
            }
        }
        inputs[i].push_back(test_util::make_exit(tid));
    }
    // Hardcoding here for the 2 outputs and 7 inputs.
    // We make assumptions on the scheduler's initial runqueue assignment
    // being round-robin, resulting in 4 on core0 (odd parity letters) and 3 on
    // core1 (even parity letters).
    // We expect 3 letter sequences (our quantum).
    // The dots are markers and thread exits.
    // A and B have a voluntary switch after their 1st 2 letters, but we expect
    // their cpu usage to persist to their next scheduling which should only have
    // a single letter.
    // Since core0 has an extra input, core1 finishes its runqueue first and then
    // steals G from core0 (migration threshold is 0) and finishes it off.
    static const char *const CORE0_SCHED_STRING =
        "..AA......CCC..EEE..GGGACCCEEEGGGAAACCC.EEE.AAA.";
    static const char *const CORE1_SCHED_STRING =
        "..BB......DDD..FFFBDDDFFFBBBDDD.FFF.BBB.GGG.____";
    {
        // Test instruction quanta.
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        for (int i = 0; i < NUM_INPUTS; i++) {
            std::vector<scheduler_t::input_reader_t> readers;
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs[i])),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                TID_BASE + i);
            sched_inputs.emplace_back(std::move(readers));
        }
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        sched_ops.quantum_duration_instrs = QUANTUM_DURATION;
        // This was tuned with a 100us threshold: so avoid scheduler.h defaults
        // changes from affecting our output.
        sched_ops.blocking_switch_threshold = BLOCK_THRESHOLD;
        sched_ops.block_time_multiplier = BLOCK_SCALE;
        sched_ops.time_units_per_us = 1.;
        // Migration is measured in wall-clock-time for instr quanta
        // so avoid non-determinism by having no threshold.
        sched_ops.migration_threshold_us = 0;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        // Check scheduler stats.  # switches is the # of letter transitions; # preempts
        // is the instances where the same letter appears 3 times without another letter
        // appearing in between (and ignoring the last letter for an input: EOF doesn't
        // count as a preempt).
        verify_scheduler_stats(scheduler.get_stream(0), /*switch_input_to_input=*/11,
                               /*switch_input_to_idle=*/0, /*switch_idle_to_input=*/0,
                               /*switch_nop=*/0, /*preempts=*/8, /*direct_attempts=*/0,
                               /*direct_successes=*/0, /*migrations=*/1);
        verify_scheduler_stats(scheduler.get_stream(1), /*switch_input_to_input=*/10,
                               /*switch_input_to_idle=*/1, /*switch_idle_to_input=*/0,
                               /*switch_nop=*/0, /*preempts=*/6, /*direct_attempts=*/0,
                               /*direct_successes=*/0, /*migrations=*/0);
        assert(scheduler.get_stream(0)->get_schedule_statistic(
                   memtrace_stream_t::SCHED_STAT_RUNQUEUE_STEALS) == 0);
        assert(scheduler.get_stream(1)->get_schedule_statistic(
                   memtrace_stream_t::SCHED_STAT_RUNQUEUE_STEALS) == 1);
#ifndef WIN32
        // XXX: Windows microseconds on test VMs are very coarse and stay the same
        // for long periods.  Instruction quanta use wall-clock idle times, so
        // the result is extreme variations here.  We try to adjust by handling
        // any schedule with singleton 'A' and 'B', but in some cases on Windows
        // we see the A and B delayed all the way to the very end where they
        // are adjacent to their own letters.  We just give up on checking the
        // precise output for this test on Windows.
        if (sched_as_string[0] != CORE0_SCHED_STRING ||
            sched_as_string[1] != CORE1_SCHED_STRING) {
            bool found_single_A = false, found_single_B = false;
            for (int cpu = 0; cpu < NUM_OUTPUTS; ++cpu) {
                for (size_t i = 1; i < sched_as_string[cpu].size() - 1; ++i) {
                    if (sched_as_string[cpu][i] == 'A' &&
                        sched_as_string[cpu][i - 1] != 'A' &&
                        sched_as_string[cpu][i + 1] != 'A')
                        found_single_A = true;
                    if (sched_as_string[cpu][i] == 'B' &&
                        sched_as_string[cpu][i - 1] != 'B' &&
                        sched_as_string[cpu][i + 1] != 'B')
                        found_single_B = true;
                }
            }
            assert(found_single_A && found_single_B);
        }
#endif
    }
    {
        // Test time quanta.
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        for (int i = 0; i < NUM_INPUTS; i++) {
            std::vector<scheduler_t::input_reader_t> readers;
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs[i])),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                TID_BASE + i);
            sched_inputs.emplace_back(std::move(readers));
        }
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.time_units_per_us = 1.;
        // This was tuned with a 100us threshold: so avoid scheduler.h defaults
        // changes from affecting our output.
        sched_ops.blocking_switch_threshold = BLOCK_THRESHOLD;
        sched_ops.quantum_duration_us = QUANTUM_DURATION;
        sched_ops.block_time_multiplier = BLOCK_SCALE;
        sched_ops.migration_threshold_us = 0;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
        assert(sched_as_string[1] == CORE1_SCHED_STRING);
    }
}

static void
test_synthetic_with_syscall_seq()
{
    std::cerr << "\n----------------\nTesting synthetic with syscall sequences\n";
    static constexpr int NUM_INPUTS = 7;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int NUM_INSTRS = 9;
    static constexpr int QUANTUM_DURATION = 3;
    // We do not want to block for very long.
    static constexpr double BLOCK_SCALE = 0.01;
    static constexpr uint64_t BLOCK_THRESHOLD = 100;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr uint64_t KERNEL_CODE_OFFSET = 123456;
    static constexpr uint64_t SYSTRACE_NUM = 84;
    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = TID_BASE + i;
        inputs[i].push_back(test_util::make_thread(tid));
        inputs[i].push_back(test_util::make_pid(1));
        inputs[i].push_back(test_util::make_version(TRACE_ENTRY_VERSION));
        inputs[i].push_back(test_util::make_timestamp(10)); // All the same time priority.
        for (int j = 0; j < NUM_INSTRS; j++) {
            inputs[i].push_back(test_util::make_instr(42 + j * 4));
            // Test a syscall sequence starting at each offset within a quantum
            // of instrs.
            if (i <= QUANTUM_DURATION && i == j) {
                inputs[i].push_back(test_util::make_timestamp(20));
                inputs[i].push_back(
                    test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, SYSTRACE_NUM));
                if (i < 2) {
                    // Thresholds for only blocking syscalls are low enough to
                    // cause a context switch. So only A and B will try a voluntary
                    // switch (which may be delayed due to the syscall trace) after
                    // 1 or 2 instrs respectively.
                    inputs[i].push_back(test_util::make_marker(
                        TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0));
                }
                inputs[i].push_back(test_util::make_timestamp(120));
                inputs[i].push_back(test_util::make_marker(
                    TRACE_MARKER_TYPE_SYSCALL_TRACE_START, SYSTRACE_NUM));
                // A has just one syscall seq instr to show that it still does the
                // voluntary switch after the syscall trace is done, even though there
                // is still room for one more instr in its quantum.
                // D has just one syscall seq instr to show that it will continue
                // on without a switch after the syscall trace is done because more
                // instrs were left in the same quantum.
                // B and C have longer syscall seq to show that they will not be
                // preempted by voluntary or quantum switches respectively.
                int count_syscall_instrs = (i == 0 || i == 3) ? 1 : QUANTUM_DURATION;
                for (int k = 1; k <= count_syscall_instrs; ++k)
                    inputs[i].push_back(test_util::make_instr(KERNEL_CODE_OFFSET + k));
                inputs[i].push_back(test_util::make_marker(
                    TRACE_MARKER_TYPE_SYSCALL_TRACE_END, SYSTRACE_NUM));
            }
        }
        inputs[i].push_back(test_util::make_exit(tid));
    }
    // A has a syscall sequence at [2,2], B has it at [3,5], C has it at [4,6],
    // D has it at [5,5].

    // Hardcoding here for the 2 outputs and 7 inputs.
    // We make assumptions on the scheduler's initial runqueue assignment
    // being round-robin, resulting in 4 on core0 (odd parity letters) and 3 on
    // core1 (even parity letters).
    // The dots are markers and thread exits.
    //
    // A has a voluntary switch after its first two letters, prompted by its
    // first instr which is a blocking syscall with latency that exceeds switch
    // threshold, but not before its 2nd instr which is from the syscall trace
    // and must be shown before the switch happens. Despite there being room for
    // 1 more instr left in the quantum, the voluntary switch still happens.
    // When scheduled next, A has room to execute only one instr left in its
    // quantum limit (which was carried over after the voluntary switch).
    //
    // B has a voluntary switch after its first 5 letters, prompted by its 2nd
    // instr which is a blocking system call with latency that exceeds switch
    // threshold, but not before its next three instrs which are from the
    // syscall trace and must be shown before the switch happens. B ends up
    // executing more instrs than its quantum limit because of the syscall
    // trace.
    //
    // C has a syscall at its third letter (but it doesn't cause a switch
    // because it doesn't have sufficiently high latency), followed by the
    // syscall trace of three additional letters. C ends up
    // executing more instrs than the quantum limit because of the syscall
    // trace.
    //
    // D has a syscall at its 4th letter, followed by a 1-instr syscall
    // trace. D continues with its regular instrs without a context switch
    // at its 6th letter because there is still room for more instrs left in
    // the quantum.
    //
    // Since core0 has an extra input, core1 finishes
    // its runqueue first and then steals G from core0 (migration threshold is 0)
    // and finishes it off.
    static const char *const CORE0_SCHED_STRING =
        "..A.....a...CCC....ccc...EEE..GGGACCCEEEGGGAAACCC.EEE.AAAA.";
    static const char *const CORE1_SCHED_STRING =
        "..BB.....bbb...DDD..FFFBBBD....d.DFFFBBBDDDFFF.B.D.GGG.____";
    {
        // Test instruction quanta.
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        for (int i = 0; i < NUM_INPUTS; i++) {
            std::vector<scheduler_t::input_reader_t> readers;
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs[i])),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                TID_BASE + i);
            sched_inputs.emplace_back(std::move(readers));
        }
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/4);
        sched_ops.quantum_duration_instrs = QUANTUM_DURATION;
        // This was tuned with a 100us threshold: so avoid scheduler.h defaults
        // changes from affecting our output.
        sched_ops.blocking_switch_threshold = BLOCK_THRESHOLD;
        sched_ops.block_time_multiplier = BLOCK_SCALE;
        sched_ops.time_units_per_us = 1.;
        // Migration is measured in wall-clock-time for instr quanta
        // so avoid non-determinism by having no threshold.
        sched_ops.migration_threshold_us = 0;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        // Check scheduler stats.  # switches is the # of letter transitions; # preempts
        // is the instances where the same letter appears 3 times without another letter
        // appearing in between (and ignoring the last letter for an input: EOF doesn't
        // count as a preempt). # nops are the instances where the same input is picked
        // to run because nothing else is waiting.
        verify_scheduler_stats(scheduler.get_stream(0), /*switch_input_to_input=*/11,
                               /*switch_input_to_idle=*/0, /*switch_idle_to_input=*/0,
                               /*switch_nop=*/1, /*preempts=*/9, /*direct_attempts=*/0,
                               /*direct_successes=*/0, /*migrations=*/1);
        verify_scheduler_stats(scheduler.get_stream(1), /*switch_input_to_input=*/11,
                               /*switch_input_to_idle=*/1, /*switch_idle_to_input=*/0,
                               /*switch_nop=*/0, /*preempts=*/8, /*direct_attempts=*/0,
                               /*direct_successes=*/0, /*migrations=*/0);
        assert(scheduler.get_stream(0)->get_schedule_statistic(
                   memtrace_stream_t::SCHED_STAT_RUNQUEUE_STEALS) == 0);
        assert(scheduler.get_stream(1)->get_schedule_statistic(
                   memtrace_stream_t::SCHED_STAT_RUNQUEUE_STEALS) == 1);
#ifndef WIN32
        // XXX: Windows microseconds on test VMs are very coarse and stay the same
        // for long periods.  Instruction quanta use wall-clock idle times, so
        // the result is extreme variations here.  We try to adjust by handling
        // any schedule with below specific patterns.  We just give up on checking the
        // precise output for this test on Windows.
        if (sched_as_string[0] != CORE0_SCHED_STRING ||
            sched_as_string[1] != CORE1_SCHED_STRING) {
            // XXX: These bools could potentially be made into ints, but then
            // maybe our check will become too strict, defeating the purpose of
            // this relaxation.
            bool found_single_A = false, found_single_B = false, found_single_D = false;
            for (int cpu = 0; cpu < NUM_OUTPUTS; ++cpu) {
                for (size_t i = 1; i < sched_as_string[cpu].size() - 1; ++i) {
                    // We expect a single 'A' for the first instr executed by 'A',
                    // which will be followed by a marker ('.') for the syscall,
                    // and the third instr executed by it which will be the only
                    // instruction executed by it during that scheduling because
                    // prior bookkeeping for that quantum exhaused all-but-one
                    // instruction.
                    if (sched_as_string[cpu][i] == 'A' &&
                        sched_as_string[cpu][i - 1] != 'A' &&
                        sched_as_string[cpu][i + 1] != 'A')
                        found_single_A = true;
                    // We expect a single 'B' for the last instr executed by B
                    // which will have to be in its own separate 3-instr quantum.
                    if (sched_as_string[cpu][i] == 'B' &&
                        sched_as_string[cpu][i - 1] != 'B' &&
                        sched_as_string[cpu][i + 1] != 'B')
                        found_single_B = true;
                    // We expect a single 'D' for the one quantum where the
                    // 1st and 3rd instrs executed by D were regular, and the
                    // 2nd one was from a syscall (which is 'd'). Also, the
                    // last (10th) instr executed by D will have to be in its
                    // own separate 3-instr quantum.
                    if (sched_as_string[cpu][i] == 'D' &&
                        sched_as_string[cpu][i - 1] != 'D' &&
                        sched_as_string[cpu][i + 1] != 'D')
                        found_single_D = true;
                }
            }
            bool found_syscall_a = false, found_syscall_b = false,
                 found_syscall_c = false, found_syscall_d = false;
            for (int cpu = 0; cpu < NUM_OUTPUTS; ++cpu) {
                // The '.' at beginning and end of each of the searched sequences
                // below is for the syscall trace start and end markers.
                if (sched_as_string[cpu].find(".a.") != std::string::npos) {
                    found_syscall_a = true;
                }
                if (sched_as_string[cpu].find(".bbb.") != std::string::npos) {
                    found_syscall_b = true;
                }
                if (sched_as_string[cpu].find(".ccc.") != std::string::npos) {
                    found_syscall_c = true;
                }
                if (sched_as_string[cpu].find(".d.") != std::string::npos) {
                    found_syscall_d = true;
                }
            }
            assert(found_single_A && found_single_B && found_single_D);
            assert(found_syscall_a && found_syscall_b && found_syscall_c &&
                   found_syscall_d);
        }
#endif
    }
    {
        // Test time quanta.
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        for (int i = 0; i < NUM_INPUTS; i++) {
            std::vector<scheduler_t::input_reader_t> readers;
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs[i])),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                TID_BASE + i);
            sched_inputs.emplace_back(std::move(readers));
        }
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/4);
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.time_units_per_us = 1.;
        // This was tuned with a 100us threshold: so avoid scheduler.h defaults
        // changes from affecting our output.
        sched_ops.blocking_switch_threshold = BLOCK_THRESHOLD;
        sched_ops.quantum_duration_us = QUANTUM_DURATION;
        sched_ops.block_time_multiplier = BLOCK_SCALE;
        sched_ops.migration_threshold_us = 0;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
        assert(sched_as_string[1] == CORE1_SCHED_STRING);
    }
}

static void
test_synthetic_time_quanta()
{
    std::cerr << "\n----------------\nTesting time quanta\n";
#ifdef HAS_ZIP
    static constexpr memref_tid_t TID_BASE = 42;
    static constexpr memref_tid_t TID_A = TID_BASE;
    static constexpr memref_tid_t TID_B = TID_A + 1;
    static constexpr memref_tid_t TID_C = TID_A + 2;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int NUM_INPUTS = 3;
    static constexpr uint64_t BLOCK_THRESHOLD = 100;
    static constexpr int PRE_BLOCK_TIME = 20;
    static constexpr int POST_BLOCK_TIME = 220;
    std::vector<trace_entry_t> refs[NUM_INPUTS];
    for (int i = 0; i < NUM_INPUTS; ++i) {
        refs[i].push_back(test_util::make_thread(TID_BASE + i));
        refs[i].push_back(test_util::make_pid(1));
        refs[i].push_back(test_util::make_version(TRACE_ENTRY_VERSION));
        refs[i].push_back(test_util::make_timestamp(10));
        refs[i].push_back(test_util::make_instr(10));
        refs[i].push_back(test_util::make_instr(30));
        if (i == 0) {
            refs[i].push_back(test_util::make_timestamp(PRE_BLOCK_TIME));
            refs[i].push_back(test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 42));
            refs[i].push_back(
                test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0));
            refs[i].push_back(test_util::make_timestamp(POST_BLOCK_TIME));
        }
        refs[i].push_back(test_util::make_instr(50));
        refs[i].push_back(test_util::make_exit(TID_BASE + i));
    }
    std::string record_fname = "tmp_test_replay_time.zip";
    {
        // Record.
        std::vector<scheduler_t::input_reader_t> readers;
        for (int i = 0; i < NUM_INPUTS; ++i) {
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(refs[i])),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                TID_BASE + i);
        }
        scheduler_t scheduler;
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/4);
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.time_units_per_us = 1.;
        sched_ops.quantum_duration_us = 3;
        // This was tuned with a 100us threshold: so avoid scheduler.h defaults
        // changes from affecting our output.
        sched_ops.blocking_switch_threshold = BLOCK_THRESHOLD;
        // Ensure it waits 10 steps.
        sched_ops.block_time_multiplier = 10. / (POST_BLOCK_TIME - PRE_BLOCK_TIME);
        // Ensure steals happen in this short test.
        sched_ops.migration_threshold_us = 0;
        zipfile_ostream_t outfile(record_fname);
        sched_ops.schedule_record_ostream = &outfile;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        auto check_next = [](scheduler_t::stream_t *stream, uint64_t time,
                             scheduler_t::stream_status_t expect_status,
                             memref_tid_t expect_tid = INVALID_THREAD_ID,
                             trace_type_t expect_type = TRACE_TYPE_READ) {
            memref_t memref;
            scheduler_t::stream_status_t status = stream->next_record(memref, time);
            if (status != expect_status) {
                std::cerr << "Expected status " << expect_status << " != " << status
                          << " at time " << time << "\n";
                assert(false);
            }
            if (status == scheduler_t::STATUS_OK) {
                if (memref.marker.tid != expect_tid) {
                    std::cerr << "Expected tid " << expect_tid
                              << " != " << memref.marker.tid << " at time " << time
                              << "\n";
                    assert(false);
                }
                if (memref.marker.type != expect_type) {
                    std::cerr << "Expected type " << expect_type
                              << " != " << memref.marker.type << " at time " << time
                              << "\n";
                    assert(false);
                }
            }
        };
        uint64_t time = 1;
        auto *cpu0 = scheduler.get_stream(0);
        auto *cpu1 = scheduler.get_stream(1);
        // Advance cpu0 to its 1st instr at time 2.
        check_next(cpu0, time, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_MARKER);
        check_next(cpu0, time, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_MARKER);
        check_next(cpu0, ++time, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_INSTR);
        // Advance cpu1 to its 1st instr at time 3.
        check_next(cpu1, time, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_MARKER);
        check_next(cpu1, time, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_MARKER);
        check_next(cpu1, ++time, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
        // Advance cpu0 which with ++ is at its quantum end at time 4 and picks up TID_C.
        check_next(cpu0, ++time, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_MARKER);
        check_next(cpu0, time, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_MARKER);
        check_next(cpu0, ++time, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_INSTR);
        // Advance cpu1 which is now at its quantum end at time 6 and should switch.
        // However, there's no one else in cpu1's runqueue, so it proceeds with TID_B.
        check_next(cpu1, ++time, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
        check_next(cpu1, ++time, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
        check_next(cpu1, time, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_THREAD_EXIT);
        // cpu1 should now steal TID_A from cpu0.
        check_next(cpu1, ++time, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_INSTR);
        check_next(cpu1, time, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_MARKER);
        check_next(cpu1, time, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_MARKER);
        check_next(cpu1, time, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_MARKER);
        check_next(cpu1, time, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_MARKER);
        // We just hit a blocking syscall in A but there is nothing else to run.
        check_next(cpu1, ++time, scheduler_t::STATUS_IDLE);
        // Finish off C on cpu 0.  This hits a quantum end but there's no one else.
        check_next(cpu0, ++time, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_INSTR);
        check_next(cpu0, ++time, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_INSTR);
        check_next(cpu0, time, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_THREAD_EXIT);
        // Both cpus wait until A is unblocked.
        check_next(cpu1, ++time, scheduler_t::STATUS_IDLE);
        check_next(cpu0, ++time, scheduler_t::STATUS_IDLE);
        check_next(cpu1, ++time, scheduler_t::STATUS_IDLE);
        check_next(cpu0, ++time, scheduler_t::STATUS_IDLE);
        check_next(cpu1, ++time, scheduler_t::STATUS_IDLE);
        check_next(cpu0, ++time, scheduler_t::STATUS_IDLE);
        check_next(cpu1, ++time, scheduler_t::STATUS_IDLE);
        check_next(cpu1, ++time, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_INSTR);
        check_next(cpu1, time, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_THREAD_EXIT);
        check_next(cpu1, ++time, scheduler_t::STATUS_EOF);
        check_next(cpu0, ++time, scheduler_t::STATUS_EOF);
        if (scheduler.write_recorded_schedule() != scheduler_t::STATUS_SUCCESS)
            assert(false);
        // Check scheduler stats.  2 nops (quantum end but no one else); 1 migration
        // (the steal).
        verify_scheduler_stats(scheduler.get_stream(0), /*switch_input_to_input=*/1,
                               /*switch_input_to_idle=*/1, /*switch_idle_to_input=*/0,
                               /*switch_nop=*/1, /*preempts=*/2, /*direct_attempts=*/0,
                               /*direct_successes=*/0, /*migrations=*/1);
        verify_scheduler_stats(scheduler.get_stream(1), /*switch_input_to_input=*/1,
                               /*switch_input_to_idle=*/1, /*switch_idle_to_input=*/1,
                               /*switch_nop=*/1, /*preempts=*/1, /*direct_attempts=*/0,
                               /*direct_successes=*/0, /*migrations=*/0);
    }
    {
        replay_file_checker_t checker;
        zipfile_istream_t infile(record_fname);
        std::string res = checker.check(&infile);
        if (!res.empty())
            std::cerr << "replay file checker failed: " << res;
        assert(res.empty());
    }
    {
        // Replay.
        std::vector<scheduler_t::input_reader_t> readers;
        for (int i = 0; i < NUM_INPUTS; ++i) {
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(refs[i])),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                TID_BASE + i);
        }
        scheduler_t scheduler;
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_AS_PREVIOUSLY,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/4);
        zipfile_istream_t infile(record_fname);
        sched_ops.schedule_replay_istream = &infile;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_A);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        // For replay the scheduler has to use wall-clock instead of passed-in time,
        // so the idle portions at the end here can have variable idle and wait
        // record counts.  We thus just check the start.
        assert(sched_as_string[0].substr(0, 10) == "..A..CCC._");
        assert(sched_as_string[1].substr(0, 12) == "..BBB.A...._");
    }
#endif
}

static void
test_synthetic_with_timestamps()
{
    std::cerr << "\n----------------\nTesting synthetic with timestamps\n";
    static constexpr int NUM_WORKLOADS = 3;
    static constexpr int NUM_INPUTS_PER_WORKLOAD = 3;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int NUM_INSTRS = 9;
    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    for (int workload_idx = 0; workload_idx < NUM_WORKLOADS; workload_idx++) {
        std::vector<scheduler_t::input_reader_t> readers;
        for (int input_idx = 0; input_idx < NUM_INPUTS_PER_WORKLOAD; input_idx++) {
            memref_tid_t tid =
                TID_BASE + workload_idx * NUM_INPUTS_PER_WORKLOAD + input_idx;
            std::vector<trace_entry_t> inputs;
            inputs.push_back(test_util::make_thread(tid));
            inputs.push_back(test_util::make_pid(1));
            for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
                // Sprinkle timestamps every other instruction.
                if (instr_idx % 2 == 0) {
                    // We have different base timestamps per workload, and we have the
                    // later-ordered inputs in each with the earlier timestamps to
                    // better test scheduler ordering.
                    inputs.push_back(test_util::make_timestamp(
                        1000 * workload_idx +
                        100 * (NUM_INPUTS_PER_WORKLOAD - input_idx) + 10 * instr_idx));
                }
                inputs.push_back(test_util::make_instr(42 + instr_idx * 4));
            }
            inputs.push_back(test_util::make_exit(tid));
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs)),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                tid);
        }
        sched_inputs.emplace_back(std::move(readers));
    }
    // We have one input with lower timestamps than everyone, to
    // test that it never gets switched out.
    memref_tid_t tid = TID_BASE + NUM_WORKLOADS * NUM_INPUTS_PER_WORKLOAD;
    std::vector<trace_entry_t> inputs;
    inputs.push_back(test_util::make_thread(tid));
    inputs.push_back(test_util::make_pid(1));
    for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
        if (instr_idx % 2 == 0)
            inputs.push_back(test_util::make_timestamp(1 + instr_idx));
        inputs.push_back(test_util::make_instr(42 + instr_idx * 4));
    }
    inputs.push_back(test_util::make_exit(tid));
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(inputs)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), tid);
    sched_inputs.emplace_back(std::move(readers));

    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/3);
    sched_ops.quantum_duration_instrs = 3;
    // Test dropping a final "_" from core0.
    sched_ops.exit_if_fraction_inputs_left = 0.1;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    // Hardcoding here for the 3x3+1 inputs where the inverted timestamps mean the
    // priorities are {C,B,A},{F,E,D},{I,H,G},{J} within the workloads.  Across
    // workloads we should start with {C,F,I,J} and then move on to {B,E,H} and finish
    // with {A,D,G}.  The scheduler's initial round-robin-in-priority-order allocation
    // to runqueues means it will alternate in the priority order C,F,I,J,B,E,H,A,D,G:
    // thus core0 has C,I,B,H,D and core1 has F,J,E,A,G.
    // We should interleave within each group -- except once we reach J
    // we should completely finish it.  There should be no migrations.
    assert(sched_as_string[0] ==
           ".CC.C.II.IC.CC.I.II.CC.C.II.I..BB.B.HH.HB.BB.H.HH.BB.B.HH.H..DD.DD.DD.DD.D.");
    assert(sched_as_string[1] ==
           ".FF.F.JJ.JJ.JJ.JJ.J.F.FF.FF.F..EE.EE.EE.EE.E..AA.A.GG.GA.AA.G.GG.AA.A.GG.G.");
    // Check scheduler stats.  # switches is the # of letter transitions; # preempts
    // is the instances where the same letter appears 3 times without another letter
    // appearing in between (and ignoring the last letter for an input: EOF doesn't
    // count as a preempt).
    verify_scheduler_stats(scheduler.get_stream(0), /*switch_input_to_input=*/12,
                           /*switch_input_to_idle=*/0, /*switch_idle_to_input=*/0,
                           /*switch_nop=*/2, /*preempts=*/10, /*direct_attempts=*/0,
                           /*direct_successes=*/0, /*migrations=*/0);
    verify_scheduler_stats(scheduler.get_stream(1), /*switch_input_to_input=*/9,
                           /*switch_input_to_idle=*/0, /*switch_idle_to_input=*/0,
                           /*switch_nop=*/5, /*preempts=*/10, /*direct_attempts=*/0,
                           /*direct_successes=*/0, /*migrations=*/0);
}

static void
test_synthetic_with_priorities()
{
    std::cerr << "\n----------------\nTesting synthetic with priorities\n";
    static constexpr int NUM_WORKLOADS = 3;
    static constexpr int NUM_INPUTS_PER_WORKLOAD = 3;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int NUM_INSTRS = 9;
    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    auto get_tid = [&](int workload_idx, int input_idx) {
        return TID_BASE + workload_idx * NUM_INPUTS_PER_WORKLOAD + input_idx;
    };
    for (int workload_idx = 0; workload_idx < NUM_WORKLOADS; workload_idx++) {
        std::vector<scheduler_t::input_reader_t> readers;
        for (int input_idx = 0; input_idx < NUM_INPUTS_PER_WORKLOAD; input_idx++) {
            memref_tid_t tid = get_tid(workload_idx, input_idx);
            std::vector<trace_entry_t> inputs;
            inputs.push_back(test_util::make_thread(tid));
            inputs.push_back(test_util::make_pid(1));
            for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
                // Sprinkle timestamps every other instruction.
                if (instr_idx % 2 == 0) {
                    // We have different base timestamps per workload, and we have the
                    // later-ordered inputs in each with the earlier timestamps to
                    // better test scheduler ordering.
                    inputs.push_back(test_util::make_timestamp(
                        1000 * workload_idx +
                        100 * (NUM_INPUTS_PER_WORKLOAD - input_idx) + 10 * instr_idx));
                }
                inputs.push_back(test_util::make_instr(42 + instr_idx * 4));
            }
            inputs.push_back(test_util::make_exit(tid));
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs)),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                tid);
        }
        sched_inputs.emplace_back(std::move(readers));
        // Set some different priorities for the middle threads.
        // The others retain the default 0 priority.
        sched_inputs.back().thread_modifiers.emplace_back(
            get_tid(workload_idx, /*input_idx=*/1), /*priority=*/1);
    }
    // We have one input with lower timestamps than everyone, to test that it never gets
    // switched out once we get to it among the default-priority inputs.
    memref_tid_t tid = TID_BASE + NUM_WORKLOADS * NUM_INPUTS_PER_WORKLOAD;
    std::vector<trace_entry_t> inputs;
    inputs.push_back(test_util::make_thread(tid));
    inputs.push_back(test_util::make_pid(1));
    for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
        if (instr_idx % 2 == 0)
            inputs.push_back(test_util::make_timestamp(1 + instr_idx));
        inputs.push_back(test_util::make_instr(42 + instr_idx * 4));
    }
    inputs.push_back(test_util::make_exit(tid));
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(inputs)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), tid);
    sched_inputs.emplace_back(std::move(readers));

    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/3);
    sched_ops.quantum_duration_instrs = 3;
    // Test dropping a final "_" from core0.
    sched_ops.exit_if_fraction_inputs_left = 0.1;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    // See the test_synthetic_with_timestamps() test which has our base sequence.
    // We've elevated B, E, and H to higher priorities so they go
    // first.  J remains uninterrupted due to lower timestamps.
    assert(sched_as_string[0] ==
           ".BB.B.HH.HB.BB.H.HH.BB.B.HH.H..FF.F.JJ.JJ.JJ.JJ.J.F.FF.FF.F..DD.DD.DD.DD.D.");
    assert(sched_as_string[1] ==
           ".EE.EE.EE.EE.E..CC.C.II.IC.CC.I.II.CC.C.II.I..AA.A.GG.GA.AA.G.GG.AA.A.GG.G.");
    // Check scheduler stats.  # switches is the # of letter transitions; # preempts
    // is the instances where the same letter appears 3 times without another letter
    // appearing in between (and ignoring the last letter for an input: EOF doesn't
    // count as a preempt).
    verify_scheduler_stats(scheduler.get_stream(0), /*switch_input_to_input=*/9,
                           /*switch_input_to_idle=*/0, /*switch_idle_to_input=*/0,
                           /*switch_nop=*/5, /*preempts=*/10, /*direct_attempts=*/0,
                           /*direct_successes=*/0, /*migrations=*/0);
    verify_scheduler_stats(scheduler.get_stream(1), /*switch_input_to_input=*/12,
                           /*switch_input_to_idle=*/0, /*switch_idle_to_input=*/0,
                           /*switch_nop=*/2, /*preempts=*/10, /*direct_attempts=*/0,
                           /*direct_successes=*/0, /*migrations=*/0);
}

static void
test_synthetic_with_bindings_time(bool time_deps)
{
    std::cerr << "\n----------------\nTesting synthetic with bindings (deps=" << time_deps
              << ")\n";
    static constexpr int NUM_WORKLOADS = 3;
    static constexpr int NUM_INPUTS_PER_WORKLOAD = 3;
    static constexpr int NUM_OUTPUTS = 5;
    static constexpr int NUM_INSTRS = 9;
    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    auto get_tid = [&](int workload_idx, int input_idx) {
        return TID_BASE + workload_idx * NUM_INPUTS_PER_WORKLOAD + input_idx;
    };
    for (int workload_idx = 0; workload_idx < NUM_WORKLOADS; workload_idx++) {
        std::vector<scheduler_t::input_reader_t> readers;
        for (int input_idx = 0; input_idx < NUM_INPUTS_PER_WORKLOAD; input_idx++) {
            memref_tid_t tid = get_tid(workload_idx, input_idx);
            std::vector<trace_entry_t> inputs;
            inputs.push_back(test_util::make_thread(tid));
            inputs.push_back(test_util::make_pid(1));
            for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
                // Include timestamps but keep each workload with the same time to
                // avoid complicating the test.
                if (instr_idx % 2 == 0) {
                    inputs.push_back(test_util::make_timestamp(10 * (instr_idx + 1)));
                }
                inputs.push_back(test_util::make_instr(42 + instr_idx * 4));
            }
            inputs.push_back(test_util::make_exit(tid));
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs)),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                tid);
        }
        sched_inputs.emplace_back(std::move(readers));
        // We do a static partitionining of the cores for our workloads with one
        // of them overlapping the others.
        std::set<scheduler_t::output_ordinal_t> cores;
        switch (workload_idx) {
        case 0: cores.insert({ 2, 4 }); break;
        case 1: cores.insert({ 0, 1 }); break;
        case 2: cores.insert({ 1, 2, 3 }); break;
        default: assert(false);
        }
        sched_inputs.back().thread_modifiers.emplace_back(cores);
    }
    scheduler_t::scheduler_options_t sched_ops(
        scheduler_t::MAP_TO_ANY_OUTPUT,
        // We expect the same output with time deps.  We include it as a regression
        // test for i#6874 which caused threads to start out on cores not on their
        // binding lists, which fails the schedule string checks below.
        time_deps ? scheduler_t::DEPENDENCY_TIMESTAMPS : scheduler_t::DEPENDENCY_IGNORE,
        scheduler_t::SCHEDULER_DEFAULTS,
        /*verbosity=*/3);
    sched_ops.quantum_duration_instrs = 3;
    // Migration is measured in wall-clock-time for instr quanta
    // so avoid non-determinism by having no threshold.
    sched_ops.migration_threshold_us = 0;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    // We have {A,B,C} on {2,4}, {D,E,F} on {0,1}, and {G,H,I} on {1,2,3}.
    // We should *not* see cores stealing inputs that can't run on them: so we
    // should see tail idle time.  We should see allowed steals with no migration
    // threshold.
    assert(sched_as_string[0] == ".DD.D.EE.E.FF.FD.DD.E.EE.F.FF.EE.E.FF.F.");
    assert(sched_as_string[1] == ".GG.G.HH.HG.GG.H.HH.HH.H.DD.D.__________");
    assert(sched_as_string[2] == ".AA.A.BB.BA.AA.B.BB.BB.B._______________");
    assert(sched_as_string[3] == ".II.II.II.II.I.GG.G.____________________");
    assert(sched_as_string[4] == ".CC.CC.CC.CC.C.AA.A.____________________");
}

static void
test_synthetic_with_bindings_more_out()
{
    std::cerr << "\n----------------\nTesting synthetic with bindings and #out>#in\n";
    static constexpr int NUM_INPUTS = 3;
    static constexpr int NUM_OUTPUTS = 4;
    static constexpr int NUM_INSTRS = 9;
    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    for (int input_idx = 0; input_idx < NUM_INPUTS; input_idx++) {
        std::vector<scheduler_t::input_reader_t> readers;
        memref_tid_t tid = TID_BASE + input_idx;
        std::vector<trace_entry_t> inputs;
        inputs.push_back(test_util::make_thread(tid));
        inputs.push_back(test_util::make_pid(1));
        inputs.push_back(test_util::make_timestamp(10 + input_idx));
        for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
            inputs.push_back(test_util::make_instr(42 + instr_idx * 4));
        }
        inputs.push_back(test_util::make_exit(tid));
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(inputs)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            tid);
        sched_inputs.emplace_back(std::move(readers));
        // Bind the 1st 2 inputs to the same core to ensure the 3rd
        // input gets scheduled even after an initially-unscheduled input.
        if (input_idx < 2) {
            std::set<scheduler_t::output_ordinal_t> cores;
            cores.insert(0);
            scheduler_t::input_thread_info_t info(tid, cores);
            sched_inputs.back().thread_modifiers.emplace_back(info);
        } else {
            // Specify all outputs for the 3rd to ensure that works.
            std::set<scheduler_t::output_ordinal_t> cores;
            cores.insert({ 0, 1, 2, 3 });
            scheduler_t::input_thread_info_t info(tid, cores);
            sched_inputs.back().thread_modifiers.emplace_back(info);
        }
    }
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_IGNORE,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/3);
    sched_ops.quantum_duration_instrs = 3;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    // We have {A,B} on 0 and C anywhere.
    assert(sched_as_string[0] == ".AAA.BBBAAABBBAAA.BBB.");
    assert(sched_as_string[1] == ".CCCCCCCCC.___________");
    assert(sched_as_string[2] == "______________________");
    assert(sched_as_string[3] == "______________________");
}

static void
test_synthetic_with_bindings_weighted()
{
    std::cerr << "\n----------------\nTesting synthetic with bindings and diff stamps\n";
    static constexpr int NUM_WORKLOADS = 3;
    static constexpr int NUM_INPUTS_PER_WORKLOAD = 3;
    static constexpr int NUM_OUTPUTS = 5;
    static constexpr int NUM_INSTRS = 9;
    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    auto get_tid = [&](int workload_idx, int input_idx) {
        return TID_BASE + workload_idx * NUM_INPUTS_PER_WORKLOAD + input_idx;
    };
    for (int workload_idx = 0; workload_idx < NUM_WORKLOADS; workload_idx++) {
        std::vector<scheduler_t::input_reader_t> readers;
        for (int input_idx = 0; input_idx < NUM_INPUTS_PER_WORKLOAD; input_idx++) {
            memref_tid_t tid = get_tid(workload_idx, input_idx);
            std::vector<trace_entry_t> inputs;
            inputs.push_back(test_util::make_thread(tid));
            inputs.push_back(test_util::make_pid(1));
            for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
                // Use the same inverted timestamps as test_synthetic_with_timestamps()
                // to cover different code paths; in particular it has a case where
                // the last entry in the queue is the only one that fits on an output.
                if (instr_idx % 2 == 0) {
                    inputs.push_back(test_util::make_timestamp(
                        1000 * workload_idx +
                        100 * (NUM_INPUTS_PER_WORKLOAD - input_idx) + 10 * instr_idx));
                }
                inputs.push_back(test_util::make_instr(42 + instr_idx * 4));
            }
            inputs.push_back(test_util::make_exit(tid));
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs)),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                tid);
        }
        sched_inputs.emplace_back(std::move(readers));
        // We do a static partitionining of the cores for our workloads with one
        // of them overlapping the others.
        std::set<scheduler_t::output_ordinal_t> cores;
        switch (workload_idx) {
        case 0: cores.insert({ 2, 4 }); break;
        case 1: cores.insert({ 0, 1 }); break;
        case 2: cores.insert({ 1, 2, 3 }); break;
        default: assert(false);
        }
        sched_inputs.back().thread_modifiers.emplace_back(cores);
    }

    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/3);
    sched_ops.quantum_duration_instrs = 3;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    // We have {A,B,C} on {2,4}, {D,E,F} on {0,1}, and {G,H,I} on {1,2,3}:
    assert(sched_as_string[0] == ".FF.FF.FF.FF.F..EE.EE.EE.EE.E..DD.DD.DD.DD.D.");
    assert(sched_as_string[1] == ".II.II.II.II.I..HH.HH.HH.HH.H._______________");
    assert(sched_as_string[2] == ".CC.CC.CC.CC.C..BB.BB.BB.BB.B._______________");
    assert(sched_as_string[3] == ".GG.GG.GG.GG.G.______________________________");
    assert(sched_as_string[4] == ".AA.AA.AA.AA.A.______________________________");
}

static void
test_synthetic_with_bindings_invalid()
{
    std::cerr << "\n----------------\nTesting synthetic with invalid bindings\n";
    static constexpr memref_tid_t TID_A = 42;
    std::vector<trace_entry_t> refs_A = {
        /* clang-format off */
        test_util::make_thread(TID_A),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_timestamp(1),
        test_util::make_instr(10),
        test_util::make_exit(TID_A),
        /* clang-format on */
    };
    {
        // Test negative bindings.
        static constexpr int NUM_OUTPUTS = 2;
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        std::set<scheduler_t::output_ordinal_t> cores;
        cores.insert({ 1, -1 });
        sched_inputs.back().thread_modifiers.emplace_back(cores);
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        scheduler_t scheduler;
        assert(scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) ==
               scheduler_t::STATUS_ERROR_INVALID_PARAMETER);
    }
    {
        // Test too-large bindings.
        static constexpr int NUM_OUTPUTS = 2;
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        std::set<scheduler_t::output_ordinal_t> cores;
        cores.insert({ 1, 2 });
        sched_inputs.back().thread_modifiers.emplace_back(cores);
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        scheduler_t scheduler;
        assert(scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) ==
               scheduler_t::STATUS_ERROR_INVALID_PARAMETER);
    }
}

static void
test_synthetic_with_bindings_overrides()
{
    std::cerr << "\n----------------\nTesting modifer overrides\n";
    static constexpr int NUM_INPUTS = 4;
    static constexpr int NUM_OUTPUTS = 3;
    static constexpr int NUM_INSTRS = 9;
    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    std::vector<scheduler_t::input_reader_t> readers;
    for (int input_idx = 0; input_idx < NUM_INPUTS; input_idx++) {
        memref_tid_t tid = TID_BASE + input_idx;
        std::vector<trace_entry_t> inputs;
        inputs.push_back(test_util::make_thread(tid));
        inputs.push_back(test_util::make_pid(1));
        inputs.push_back(test_util::make_timestamp(10 + input_idx));
        for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
            inputs.push_back(test_util::make_instr(42 + instr_idx * 4));
        }
        inputs.push_back(test_util::make_exit(tid));
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(inputs)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            tid);
    }
    sched_inputs.emplace_back(std::move(readers));

    // Test modifier tids colliding.
    std::set<scheduler_t::output_ordinal_t> core0 = { 0 };
    std::set<scheduler_t::output_ordinal_t> core1 = { 1 };
    std::set<scheduler_t::output_ordinal_t> core2 = { 2 };
    // First, put the 1st 3 threads (A,B,C) on core0.
    scheduler_t::input_thread_info_t infoA(core0);
    infoA.tids = { TID_BASE + 0, TID_BASE + 1, TID_BASE + 2 };
    sched_inputs.back().thread_modifiers.push_back(infoA);
    // Try to put the same tids onto a different core: should override.
    scheduler_t::input_thread_info_t infoB(core1);
    infoB.tids = { TID_BASE + 0, TID_BASE + 1, TID_BASE + 2 };
    sched_inputs.back().thread_modifiers.push_back(infoB);
    // Set a default which should apply to just the 4th input (D) as the other
    // 3 appear in modifiers (the 3rd below).
    scheduler_t::input_thread_info_t infoC(core2);
    sched_inputs.back().thread_modifiers.push_back(infoC);
    // Put the 3rd thread (C) onto core0: should override.
    scheduler_t::input_thread_info_t infoD(core0);
    infoD.tids = { TID_BASE + 2 };
    sched_inputs.back().thread_modifiers.push_back(infoD);

    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_IGNORE,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/3);
    sched_ops.quantum_duration_instrs = 3;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    // C is alone on core0; D alone on core2; and A+B are on core1.
    assert(sched_as_string[0] == ".CCCCCCCCC.____________");
    assert(sched_as_string[1] == ".AAA.BBBAAABBBAAA.BBB.");
    assert(sched_as_string[2] == ".DDDDDDDDD.___________");
}

static void
test_synthetic_with_bindings()
{
    test_synthetic_with_bindings_time(/*time_deps=*/true);
    test_synthetic_with_bindings_time(/*time_deps=*/false);
    test_synthetic_with_bindings_more_out();
    test_synthetic_with_bindings_weighted();
    test_synthetic_with_bindings_invalid();
    test_synthetic_with_bindings_overrides();
}

static void
test_synthetic_with_syscalls_multiple()
{
    std::cerr << "\n----------------\nTesting synthetic with blocking syscalls\n";
    static constexpr int NUM_WORKLOADS = 3;
    static constexpr int NUM_INPUTS_PER_WORKLOAD = 3;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int NUM_INSTRS = 9;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr int BLOCK_LATENCY = 100;
    static constexpr double BLOCK_SCALE = 1. / (BLOCK_LATENCY);
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    auto get_tid = [&](int workload_idx, int input_idx) {
        return TID_BASE + workload_idx * NUM_INPUTS_PER_WORKLOAD + input_idx;
    };
    for (int workload_idx = 0; workload_idx < NUM_WORKLOADS; workload_idx++) {
        std::vector<scheduler_t::input_reader_t> readers;
        for (int input_idx = 0; input_idx < NUM_INPUTS_PER_WORKLOAD; input_idx++) {
            memref_tid_t tid = get_tid(workload_idx, input_idx);
            std::vector<trace_entry_t> inputs;
            inputs.push_back(test_util::make_thread(tid));
            inputs.push_back(test_util::make_pid(1));
            inputs.push_back(test_util::make_version(TRACE_ENTRY_VERSION));
            uint64_t stamp =
                10000 * workload_idx + 1000 * (NUM_INPUTS_PER_WORKLOAD - input_idx);
            for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
                // Sprinkle timestamps every other instruction.  We use a similar
                // priority scheme as test_synthetic_with_priorities() but we leave
                // room for blocking syscall timestamp gaps.
                if (instr_idx % 2 == 0 &&
                    (inputs.back().type != TRACE_TYPE_MARKER ||
                     inputs.back().size != TRACE_MARKER_TYPE_TIMESTAMP)) {
                    inputs.push_back(test_util::make_timestamp(stamp));
                }
                inputs.push_back(test_util::make_instr(42 + instr_idx * 4));
                // Insert some blocking syscalls in the high-priority (see below)
                // middle threads.
                if (input_idx == 1 && instr_idx % (workload_idx + 1) == workload_idx) {
                    inputs.push_back(test_util::make_timestamp(stamp + 10));
                    inputs.push_back(
                        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 42));
                    inputs.push_back(test_util::make_marker(
                        TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0));
                    // Blocked for 10 time units with our BLOCK_SCALE.
                    inputs.push_back(
                        test_util::make_timestamp(stamp + 10 + 10 * BLOCK_LATENCY));
                } else {
                    // Insert meta records to keep the locksteps lined up.
                    inputs.push_back(test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
                    inputs.push_back(test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
                    inputs.push_back(test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
                    inputs.push_back(test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
                }
                stamp += 10;
            }
            inputs.push_back(test_util::make_exit(tid));
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs)),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                tid);
        }
        sched_inputs.emplace_back(std::move(readers));
        // Set some different priorities for the middle threads.
        // The others retain the default 0 priority.
        sched_inputs.back().thread_modifiers.emplace_back(
            get_tid(workload_idx, /*input_idx=*/1), /*priority=*/1);
    }
    // We have one input 'J' with lower timestamps than everyone, to test that it never
    // gets switched out once we get to it among the default-priority inputs.
    memref_tid_t tid = TID_BASE + NUM_WORKLOADS * NUM_INPUTS_PER_WORKLOAD;
    std::vector<trace_entry_t> inputs;
    inputs.push_back(test_util::make_thread(tid));
    inputs.push_back(test_util::make_pid(1));
    for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
        if (instr_idx % 2 == 0)
            inputs.push_back(test_util::make_timestamp(1 + instr_idx));
        inputs.push_back(test_util::make_instr(42 + instr_idx * 4));
    }
    inputs.push_back(test_util::make_exit(tid));
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(inputs)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), tid);
    sched_inputs.emplace_back(std::move(readers));

    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/3);
    sched_ops.quantum_duration_us = 3;
    // We use our mock's time==instruction count for a deterministic result.
    sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
    sched_ops.time_units_per_us = 1.;
    sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
    sched_ops.block_time_multiplier = BLOCK_SCALE;
    // Test dropping a bunch of final "_" from core1.
    sched_ops.exit_if_fraction_inputs_left = 0.1;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    // We omit the "." marker chars to keep the strings short enough to be readable.
    std::vector<std::string> sched_as_string = run_lockstep_simulation(
        scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true, /*print_markers=*/false);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    // See the test_synthetic_with_priorities() test which has our base sequence.
    // But now B hits a syscall every instr, and E every other instr, so neither
    // reaches its 3-instr quantum.  (H's syscalls are every 3rd instr coinciding with its
    // quantum.)  Furthermore, B, E, and H are blocked long enough that we see
    // the lower-priority C and F getting scheduled.  We end up with idle cores
    // while we wait for B.
    // We've omitted the "." marker records so these are not precisely simultaneous,
    // so the view here may show 2 on the same core at once: but we check for that
    // with the "." in run_lockstep_simulation().  The omitted "." markers also
    // explains why the two strings are different lengths.
    assert(sched_as_string[0] ==
           "BHHHFFFJJJJJJBHHHJJJFFFFFFBHHHDDDDDDDDDB__________B__________B__________B____"
           "______B__________B");
    assert(sched_as_string[1] == "EECCCIIICCCIIIEECCCIIIAAAGGGEEAAAGGEEGAAEGGAG");
    // Check scheduler stats.  # switches is the # of letter transitions; # preempts
    // is the instances where the same letter appears 3 times without another letter
    // appearing in between (and ignoring the last letter for an input: EOF doesn't
    // count as a preempt).
    verify_scheduler_stats(scheduler.get_stream(0), /*switch_input_to_input=*/11,
                           /*switch_input_to_idle=*/5, /*switch_idle_to_input=*/5,
                           /*switch_nop=*/4, /*preempts=*/10, /*direct_attempts=*/0,
                           /*direct_successes=*/0, /*migrations=*/0);
    verify_scheduler_stats(scheduler.get_stream(1), /*switch_input_to_input=*/19,
                           /*switch_input_to_idle=*/0, /*switch_idle_to_input=*/0,
                           /*switch_nop=*/3, /*preempts=*/16, /*direct_attempts=*/0,
                           /*direct_successes=*/0, /*migrations=*/0);
}

static void
test_synthetic_with_syscalls_single()
{
    std::cerr
        << "\n----------------\nTesting synthetic single-input with blocking syscalls\n";
    // We just want to make sure that if there's only one input at a blocking
    // syscall it will get scheduled and we won't just hang.
    static constexpr int NUM_WORKLOADS = 1;
    static constexpr int NUM_INPUTS_PER_WORKLOAD = 1;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int NUM_INSTRS = 9;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr int BLOCK_LATENCY = 100;
    static constexpr double BLOCK_SCALE = 1. / (BLOCK_LATENCY);
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    auto get_tid = [&](int workload_idx, int input_idx) {
        return TID_BASE + workload_idx * NUM_INPUTS_PER_WORKLOAD + input_idx;
    };
    for (int workload_idx = 0; workload_idx < NUM_WORKLOADS; workload_idx++) {
        std::vector<scheduler_t::input_reader_t> readers;
        for (int input_idx = 0; input_idx < NUM_INPUTS_PER_WORKLOAD; input_idx++) {
            memref_tid_t tid = get_tid(workload_idx, input_idx);
            std::vector<trace_entry_t> inputs;
            inputs.push_back(test_util::make_thread(tid));
            inputs.push_back(test_util::make_pid(1));
            inputs.push_back(test_util::make_version(TRACE_ENTRY_VERSION));
            uint64_t stamp =
                10000 * workload_idx + 1000 * (NUM_INPUTS_PER_WORKLOAD - input_idx);
            for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
                // Sprinkle timestamps every other instruction.  We use a similar
                // priority scheme as test_synthetic_with_priorities() but we leave
                // room for blocking syscall timestamp gaps.
                if (instr_idx % 2 == 0 &&
                    (inputs.back().type != TRACE_TYPE_MARKER ||
                     inputs.back().size != TRACE_MARKER_TYPE_TIMESTAMP)) {
                    inputs.push_back(test_util::make_timestamp(stamp));
                }
                inputs.push_back(test_util::make_instr(42 + instr_idx * 4));
                // Insert some blocking syscalls.
                if (instr_idx % 3 == 1) {
                    inputs.push_back(test_util::make_timestamp(stamp + 10));
                    inputs.push_back(
                        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 42));
                    inputs.push_back(test_util::make_marker(
                        TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0));
                    // Blocked for 3 time units.
                    inputs.push_back(
                        test_util::make_timestamp(stamp + 10 + 3 * BLOCK_LATENCY));
                } else {
                    // Insert meta records to keep the locksteps lined up.
                    inputs.push_back(test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
                    inputs.push_back(test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
                    inputs.push_back(test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
                    inputs.push_back(test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
                }
                stamp += 10;
            }
            inputs.push_back(test_util::make_exit(tid));
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs)),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                tid);
        }
        sched_inputs.emplace_back(std::move(readers));
    }
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/4);
    sched_ops.quantum_duration_us = 3;
    // We use our mock's time==instruction count for a deterministic result.
    sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
    sched_ops.time_units_per_us = 1.;
    sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
    sched_ops.block_time_multiplier = BLOCK_SCALE;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    // We expect an idle CPU every 3 instrs but starting at the 2nd (1-based % 3).
    // With per-output runqueues, cpu1 is idle the whole time.
    assert(sched_as_string[0] ==
           "..A....A....__A....A.....A....__A.....A....A....__A.....");
    assert(sched_as_string[1] ==
           "________________________________________________________");
}

static bool
check_ref(std::vector<memref_t> &refs, int &idx, memref_tid_t expected_tid,
          trace_type_t expected_type,
          trace_marker_type_t expected_marker = TRACE_MARKER_TYPE_RESERVED_END,
          uintptr_t expected_marker_or_branch_target_value = 0)
{
    if (expected_tid != refs[idx].instr.tid || expected_type != refs[idx].instr.type) {
        std::cerr << "Record " << idx << " has tid " << refs[idx].instr.tid
                  << " and type " << refs[idx].instr.type << " != expected tid "
                  << expected_tid << " and expected type " << expected_type << "\n";
        return false;
    }
    if (type_is_instr_branch(expected_type) &&
        !type_is_instr_direct_branch(expected_type) &&
        expected_marker_or_branch_target_value != 0 &&
        refs[idx].instr.indirect_branch_target !=
            expected_marker_or_branch_target_value) {
        std::cerr << "Record " << idx << " has ib target value "
                  << refs[idx].instr.indirect_branch_target << " but expected "
                  << expected_marker_or_branch_target_value << "\n";
        return false;
    }
    if (expected_type == TRACE_TYPE_MARKER) {
        if (expected_marker != refs[idx].marker.marker_type) {
            std::cerr << "Record " << idx << " has marker type "
                      << refs[idx].marker.marker_type << " but expected "
                      << expected_marker << "\n";
            return false;
        }
        if (expected_marker_or_branch_target_value != 0 &&
            expected_marker_or_branch_target_value != refs[idx].marker.marker_value) {
            std::cerr << "Record " << idx << " has marker value "
                      << refs[idx].marker.marker_value << " but expected "
                      << expected_marker_or_branch_target_value << "\n";
            return false;
        }
    }
    ++idx;
    return true;
}

static void
test_synthetic_with_syscalls_precise()
{
    std::cerr << "\n----------------\nTesting blocking syscall precise switch points\n";
    static constexpr memref_tid_t TID_A = 42;
    static constexpr memref_tid_t TID_B = 99;
    static constexpr int SYSNUM = 202;
    static constexpr uint64_t INITIAL_TIMESTAMP = 20;
    static constexpr uint64_t PRE_SYS_TIMESTAMP = 120;
    static constexpr uint64_t BLOCK_THRESHOLD = 500;
    std::vector<trace_entry_t> refs_A = {
        /* clang-format off */
        test_util::make_thread(TID_A),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_timestamp(INITIAL_TIMESTAMP),
        test_util::make_instr(10),
        test_util::make_timestamp(PRE_SYS_TIMESTAMP),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, SYSNUM),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_FUNC_ID, 100),
        test_util::make_marker(TRACE_MARKER_TYPE_FUNC_ARG, 42),
        test_util::make_timestamp(PRE_SYS_TIMESTAMP + BLOCK_THRESHOLD),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 1),
        test_util::make_marker(TRACE_MARKER_TYPE_FUNC_ID, 100),
        test_util::make_marker(TRACE_MARKER_TYPE_FUNC_RETVAL, 0),
        test_util::make_instr(12),
        test_util::make_exit(TID_A),
        /* clang-format on */
    };
    std::vector<trace_entry_t> refs_B = {
        /* clang-format off */
        test_util::make_thread(TID_B),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_timestamp(120),
        test_util::make_instr(20),
        test_util::make_instr(21),
        test_util::make_exit(TID_B),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(refs_A)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), TID_A);
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(refs_B)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), TID_B);
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/3);
    sched_ops.blocking_switch_threshold = BLOCK_THRESHOLD;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, 1, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    auto *stream = scheduler.get_stream(0);
    memref_t memref;
    std::vector<memref_t> refs;
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        if (status == scheduler_t::STATUS_WAIT || status == scheduler_t::STATUS_IDLE)
            continue;
        assert(status == scheduler_t::STATUS_OK);
        refs.push_back(memref);
    }
    int idx = 0;
    bool res = true;
    res = res &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_INSTR) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_SYSCALL) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER,
                  TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ID) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ARG) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ID) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_RETVAL) &&
        // Shouldn't switch until after all the syscall's markers.
        check_ref(refs, idx, TID_B, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_ref(refs, idx, TID_B, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_ref(refs, idx, TID_B, TRACE_TYPE_INSTR) &&
        check_ref(refs, idx, TID_B, TRACE_TYPE_INSTR) &&
        check_ref(refs, idx, TID_B, TRACE_TYPE_THREAD_EXIT) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_INSTR) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_THREAD_EXIT);
    assert(res);
}

static void
test_synthetic_with_syscalls_latencies()
{
    std::cerr << "\n----------------\nTesting syscall latency switches\n";
    static constexpr memref_tid_t TID_A = 42;
    static constexpr memref_tid_t TID_B = 99;
    static constexpr int SYSNUM = 202;
    static constexpr int BLOCK_LATENCY = 100;
    static constexpr double BLOCK_SCALE = 1. / (BLOCK_LATENCY);
    std::vector<trace_entry_t> refs_A = {
        /* clang-format off */
        test_util::make_thread(TID_A),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_timestamp(20),
        test_util::make_instr(10),
        // Test 0 latency.
        test_util::make_timestamp(120),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, SYSNUM),
        test_util::make_timestamp(120),
        test_util::make_instr(10),
        // Test large but too-short latency.
        test_util::make_timestamp(200),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, SYSNUM),
        test_util::make_timestamp(699),
        test_util::make_instr(10),
        // Test just large enough latency, with func markers in between.
        test_util::make_timestamp(1000),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, SYSNUM),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_FUNC_ID, 100),
        test_util::make_marker(TRACE_MARKER_TYPE_FUNC_ARG, 42),
        test_util::make_timestamp(1000 + BLOCK_LATENCY),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 1),
        test_util::make_marker(TRACE_MARKER_TYPE_FUNC_ID, 100),
        test_util::make_marker(TRACE_MARKER_TYPE_FUNC_RETVAL, 0),
        test_util::make_instr(12),
        test_util::make_exit(TID_A),
        /* clang-format on */
    };
    std::vector<trace_entry_t> refs_B = {
        /* clang-format off */
        test_util::make_thread(TID_B),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_timestamp(2000),
        test_util::make_instr(20),
        test_util::make_instr(21),
        test_util::make_exit(TID_B),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(refs_A)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), TID_A);
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(refs_B)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), TID_B);
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/3);
    // We use a mock time for a deterministic result.
    sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
    sched_ops.time_units_per_us = 1.;
    sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
    sched_ops.block_time_multiplier = BLOCK_SCALE;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, 1, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    auto *stream = scheduler.get_stream(0);
    memref_t memref;
    std::vector<memref_t> refs;
    int step = 0;
    for (scheduler_t::stream_status_t status = stream->next_record(memref, ++step);
         status != scheduler_t::STATUS_EOF;
         status = stream->next_record(memref, ++step)) {
        if (status == scheduler_t::STATUS_WAIT)
            continue;
        assert(status == scheduler_t::STATUS_OK);
        refs.push_back(memref);
    }
    int idx = 0;
    bool res = true;
    res = res &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_INSTR) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_SYSCALL) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_INSTR) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_SYSCALL) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_INSTR) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_SYSCALL) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER,
                  TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ID) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ARG) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ID) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_RETVAL) &&
        // Shouldn't switch until after all the syscall's markers.
        check_ref(refs, idx, TID_B, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_ref(refs, idx, TID_B, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_ref(refs, idx, TID_B, TRACE_TYPE_INSTR) &&
        check_ref(refs, idx, TID_B, TRACE_TYPE_INSTR) &&
        check_ref(refs, idx, TID_B, TRACE_TYPE_THREAD_EXIT) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_INSTR) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_THREAD_EXIT);
    assert(res);
}

static void
test_synthetic_with_syscalls_idle()
{
    std::cerr << "\n----------------\nTesting syscall idle time duration\n";
    // We test that a blocked input is put to the back of the queue on each retry.
    static constexpr int NUM_INPUTS = 4;
    static constexpr int NUM_OUTPUTS = 1;
    static constexpr int NUM_INSTRS = 12;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr int BLOCK_LATENCY = 100;
    static constexpr double BLOCK_SCALE = 1. / (BLOCK_LATENCY);
    static constexpr int BLOCK_UNITS = 27;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    std::vector<scheduler_t::input_reader_t> readers;
    for (int input_idx = 0; input_idx < NUM_INPUTS; input_idx++) {
        memref_tid_t tid = TID_BASE + input_idx;
        std::vector<trace_entry_t> inputs;
        inputs.push_back(test_util::make_thread(tid));
        inputs.push_back(test_util::make_pid(1));
        inputs.push_back(test_util::make_version(TRACE_ENTRY_VERSION));
        uint64_t stamp = 10000 * NUM_INPUTS;
        inputs.push_back(test_util::make_timestamp(stamp));
        for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
            inputs.push_back(test_util::make_instr(42 + instr_idx * 4));
            if (instr_idx == 1) {
                // Insert a blocking syscall in one input.
                if (input_idx == 0) {
                    inputs.push_back(test_util::make_timestamp(stamp + 10));
                    inputs.push_back(
                        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 42));
                    inputs.push_back(test_util::make_marker(
                        TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0));
                    // Blocked for BLOCK_UNITS time units with BLOCK_SCALE, but
                    // after each queue rejection it should go to the back of
                    // the queue and all the other inputs should be selected
                    // before another retry.
                    inputs.push_back(test_util::make_timestamp(
                        stamp + 10 + BLOCK_UNITS * BLOCK_LATENCY));
                } else {
                    // Insert a timestamp to match the blocked input so the inputs
                    // are all at equal priority in the queue.
                    inputs.push_back(test_util::make_timestamp(
                        stamp + 10 + BLOCK_UNITS * BLOCK_LATENCY));
                }
            }
        }
        inputs.push_back(test_util::make_exit(tid));
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(inputs)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            tid);
    }
    sched_inputs.emplace_back(std::move(readers));
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/3);
    sched_ops.quantum_duration_us = 3;
    // We use a mock time for a deterministic result.
    sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
    sched_ops.time_units_per_us = 1.;
    sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
    sched_ops.block_time_multiplier = BLOCK_SCALE;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    // The timestamps provide the ABCD ordering, but A's blocking syscall after its
    // 2nd instr makes it delayed for 3 full queue cycles of BBBCCCDDD (27 instrs,
    // which is BLOCK_UNITS): A's is finally schedulable after the 3rd, when it just gets
    // 1 instruction in before its (accumulated) count equals the quantum.
    assert(sched_as_string[0] ==
           "..AA......BB.B..CC.C..DD.DBBBCCCDDDBBBCCCDDDABBB.CCC.DDD.AAAAAAAAA.");
}

static void
test_synthetic_with_syscalls()
{
    test_synthetic_with_syscalls_multiple();
    test_synthetic_with_syscalls_single();
    test_synthetic_with_syscalls_precise();
    test_synthetic_with_syscalls_latencies();
    test_synthetic_with_syscalls_idle();
}

#if (defined(X86_64) || defined(ARM_64)) && defined(HAS_ZIP)
static void
simulate_core(scheduler_t::stream_t *stream)
{
    memref_t record;
    for (scheduler_t::stream_status_t status = stream->next_record(record);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(record)) {
        if (status == scheduler_t::STATUS_WAIT || status == scheduler_t::STATUS_IDLE) {
            std::this_thread::yield();
            continue;
        }
        assert(status == scheduler_t::STATUS_OK);
    }
}
#endif

static void
test_synthetic_multi_threaded(const char *testdir)
{
    std::cerr << "\n----------------\nTesting synthetic multi-threaded\n";
    // We want a larger input trace to better stress synchronization across
    // output threads.
#if (defined(X86_64) || defined(ARM_64)) && defined(HAS_ZIP)
    std::string path = std::string(testdir) + "/drmemtrace.threadsig.x64.tracedir";
    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(path);
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/2);
    static constexpr int NUM_OUTPUTS = 4;
    static constexpr int QUANTUM_DURATION = 2000;
    sched_ops.quantum_duration_instrs = QUANTUM_DURATION;
    // Keep the test short.
    static constexpr uint64_t BLOCK_MAX = 50;
    sched_ops.block_time_max_us = BLOCK_MAX;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::thread> threads;
    threads.reserve(NUM_OUTPUTS);
    for (int i = 0; i < NUM_OUTPUTS; ++i) {
        threads.emplace_back(std::thread(&simulate_core, scheduler.get_stream(i)));
    }
    for (std::thread &thread : threads)
        thread.join();
#endif
}

static void
test_synthetic_with_output_limit()
{
    std::cerr << "\n----------------\nTesting synthetic with output limits\n";
    static constexpr int NUM_WORKLOADS = 3;
    static constexpr int NUM_INPUTS_PER_WORKLOAD = 4;
    static constexpr int NUM_OUTPUTS = 8;
    static constexpr int NUM_INSTRS = 6;
    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    auto get_tid = [&](int workload_idx, int input_idx) {
        return TID_BASE + workload_idx * NUM_INPUTS_PER_WORKLOAD + input_idx;
    };
    for (int workload_idx = 0; workload_idx < NUM_WORKLOADS; workload_idx++) {
        std::vector<scheduler_t::input_reader_t> readers;
        for (int input_idx = 0; input_idx < NUM_INPUTS_PER_WORKLOAD; input_idx++) {
            memref_tid_t tid = get_tid(workload_idx, input_idx);
            std::vector<trace_entry_t> inputs;
            inputs.push_back(test_util::make_thread(tid));
            inputs.push_back(test_util::make_pid(1));
            for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
                // Sprinkle timestamps every other instruction.
                if (instr_idx % 2 == 0) {
                    // Like test_synthetic_with_priorities(), we have different base
                    // timestamps per workload, and we have the later-ordered inputs in
                    // each with the earlier timestamps to better test scheduler ordering.
                    inputs.push_back(test_util::make_timestamp(
                        1000 * workload_idx +
                        100 * (NUM_INPUTS_PER_WORKLOAD - input_idx) + 10 * instr_idx));
                }
                inputs.push_back(test_util::make_instr(42 + instr_idx * 4));
            }
            inputs.push_back(test_util::make_exit(tid));
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs)),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                tid);
        }
        sched_inputs.emplace_back(std::move(readers));
        // Set a cap on some of the workloads.
        sched_inputs.back().output_limit = workload_idx;
    }
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/2);
    // Run everything.
    sched_ops.exit_if_fraction_inputs_left = 0.;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
    int64_t limits = 0;
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        limits += static_cast<int64_t>(scheduler.get_stream(i)->get_schedule_statistic(
            memtrace_stream_t::SCHED_STAT_HIT_OUTPUT_LIMIT));
    }
    assert(limits > 0);
    // We have ABCD with no limits so they all run at once.
    // EFGH have a max 1 core so they run serially.
    // IJKL have a max of 2: we see KL, then IJ.
    assert(sched_as_string[0] == ".DD.DD.DD._.JJ.JJ.JJ.____________________");
    assert(sched_as_string[1] == ".HH.HH.HH._______________________________");
    assert(sched_as_string[2] == ".LL.LL.LL..EE.EE.EE._____________________");
    assert(sched_as_string[3] == ".CC.CC.CC..II.II.II._____________________");
    assert(sched_as_string[4] == ".KK.KK.KK.__________.GG.GG.GG.___________");
    assert(sched_as_string[5] == ".BB.BB.BB._______________________________");
    assert(sched_as_string[6] == ".AA.AA.AA._______________________________");
    assert(sched_as_string[7] == "______________________________.FF.FF.FF.");
}

static void
test_speculation()
{
    std::cerr << "\n----------------\nTesting speculation\n";
    std::vector<trace_entry_t> memrefs = {
        /* clang-format off */
        test_util::make_thread(1),
        test_util::make_pid(1),
        test_util::make_marker(TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
        test_util::make_timestamp(10),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 1),
        // Conditional branch.
        test_util::make_instr(1, TRACE_TYPE_INSTR_CONDITIONAL_JUMP),
        // It fell through in the trace.
        test_util::make_instr(2),
        // Another conditional branch.
        test_util::make_instr(3, TRACE_TYPE_INSTR_CONDITIONAL_JUMP),
        // It fell through in the trace.
        test_util::make_instr(4),
        test_util::make_instr(5),
        test_util::make_exit(1),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(memrefs)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), 1);

    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    scheduler_t::scheduler_options_t sched_ops =
        scheduler_t::make_scheduler_serial_options(/*verbosity=*/4);
    sched_ops.flags = static_cast<scheduler_t::scheduler_flags_t>(
        static_cast<int>(sched_ops.flags) |
        static_cast<int>(scheduler_t::SCHEDULER_SPECULATE_NOPS));
    if (scheduler.init(sched_inputs, 1, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    int ordinal = 0;
    auto *stream = scheduler.get_stream(0);
    memref_t memref;
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
        switch (ordinal) {
        case 0:
            assert(memref.marker.type == TRACE_TYPE_MARKER);
            assert(memref.marker.marker_type == TRACE_MARKER_TYPE_PAGE_SIZE);
            break;
        case 1:
            assert(memref.marker.type == TRACE_TYPE_MARKER);
            assert(memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP);
            break;
        case 2:
            assert(memref.marker.type == TRACE_TYPE_MARKER);
            assert(memref.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID);
            break;
        case 3:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 1);
            break;
        case 4:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 2);
            // We realize now that we mispredicted that the branch would be taken.
            // We ask to queue this record for post-speculation.
            status = stream->start_speculation(100, true);
            assert(status == scheduler_t::STATUS_OK);
            // Ensure unread_last_record() fails during speculation.
            assert(stream->unread_last_record() == scheduler_t::STATUS_INVALID);
            break;
        case 5:
            // We should now see nops from the speculator.
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 100);
            assert(memref_is_nop_instr(memref));
            break;
        case 6:
            // Another nop before we abandon this path.
            assert(type_is_instr(memref.instr.type));
            assert(memref_is_nop_instr(memref));
#ifdef AARCH64
            assert(memref.instr.addr == 104);
#elif defined(X86_64) || defined(X86_32)
            assert(memref.instr.addr == 101);
#elif defined(ARM)
            assert(memref.instr.addr == 102 || memref.instr.addr == 104);
#endif
            status = stream->stop_speculation();
            assert(status == scheduler_t::STATUS_OK);
            break;
        case 7:
            // Back to the trace, to the queued record
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 2);
            break;
        case 8:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 3);
            break;
        case 9:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 4);
            // We realize now that we mispredicted that the branch would be taken.
            // This time we do *not* ask to queue this record for post-speculation.
            status = stream->start_speculation(200, false);
            assert(status == scheduler_t::STATUS_OK);
            break;
        case 10:
            // We should now see nops from the speculator.
            assert(type_is_instr(memref.instr.type));
            assert(memref_is_nop_instr(memref));
            assert(memref.instr.addr == 200);
            // Test a nested start_speculation().
            status = stream->start_speculation(300, false);
            assert(status == scheduler_t::STATUS_OK);
            // Ensure unread_last_record() fails during nested speculation.
            assert(stream->unread_last_record() == scheduler_t::STATUS_INVALID);
            break;
        case 11:
            assert(type_is_instr(memref.instr.type));
            assert(memref_is_nop_instr(memref));
            assert(memref.instr.addr == 300);
            status = stream->stop_speculation();
            assert(status == scheduler_t::STATUS_OK);
            break;
        case 12:
            // Back to the outer speculation layer's next PC.
            assert(type_is_instr(memref.instr.type));
            assert(memref_is_nop_instr(memref));
#ifdef AARCH64
            assert(memref.instr.addr == 204);
#elif defined(X86_64) || defined(X86_32)
            assert(memref.instr.addr == 201);
#elif defined(ARM)
            assert(memref.instr.addr == 202 || memref.instr.addr == 204);
#endif
            // Test a nested start_speculation(), saving the current record.
            status = stream->start_speculation(400, true);
            assert(status == scheduler_t::STATUS_OK);
            break;
        case 13:
            assert(type_is_instr(memref.instr.type));
            assert(memref_is_nop_instr(memref));
            assert(memref.instr.addr == 400);
            status = stream->stop_speculation();
            assert(status == scheduler_t::STATUS_OK);
            break;
        case 14:
            // Back to the outer speculation layer's prior PC.
            assert(type_is_instr(memref.instr.type));
            assert(memref_is_nop_instr(memref));
#ifdef AARCH64
            assert(memref.instr.addr == 204);
#elif defined(X86_64) || defined(X86_32)
            assert(memref.instr.addr == 201);
#elif defined(ARM)
            assert(memref.instr.addr == 202 || memref.instr.addr == 204);
#endif
            status = stream->stop_speculation();
            assert(status == scheduler_t::STATUS_OK);
            break;
        case 15:
            // Back to the trace, but skipping what we already read.
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 5);
            break;
        default:
            assert(ordinal == 16);
            assert(memref.exit.type == TRACE_TYPE_THREAD_EXIT);
        }
        ++ordinal;
    }
    assert(ordinal == 17);
}

static void
test_replay()
{
#ifdef HAS_ZIP
    std::cerr << "\n----------------\nTesting replay\n";
    static constexpr int NUM_INPUTS = 7;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int NUM_INSTRS = 9;
    static constexpr int QUANTUM_INSTRS = 3;
    // For our 2 outputs and 7 inputs:
    // We expect 3 letter sequences (our quantum) alternating every-other with
    // odd parity letters on core0 (A,C,E,G) and even parity on core1 (B,D,F).
    // With a smaller runqueue, the 2nd core finishes early and steals E.
    static const char *const CORE0_SCHED_STRING = "AAACCCEEEGGGAAACCCEEEGGGAAA.CCC.GGG.";
    static const char *const CORE1_SCHED_STRING = "BBBDDDFFFBBBDDDFFFBBB.DDD.FFF.EEE.__";

    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = TID_BASE + i;
        inputs[i].push_back(test_util::make_thread(tid));
        inputs[i].push_back(test_util::make_pid(1));
        for (int j = 0; j < NUM_INSTRS; j++)
            inputs[i].push_back(test_util::make_instr(42 + j * 4));
        inputs[i].push_back(test_util::make_exit(tid));
    }
    std::string record_fname = "tmp_test_replay_record.zip";

    // Record.
    {
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        for (int i = 0; i < NUM_INPUTS; i++) {
            memref_tid_t tid = TID_BASE + i;
            std::vector<scheduler_t::input_reader_t> readers;
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs[i])),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                tid);
            sched_inputs.emplace_back(std::move(readers));
        }
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        sched_ops.quantum_duration_instrs = QUANTUM_INSTRS;
        // Migration is measured in wall-clock-time for instr quanta
        // so avoid non-determinism by having no threshold.
        sched_ops.migration_threshold_us = 0;

        zipfile_ostream_t outfile(record_fname);
        sched_ops.schedule_record_ostream = &outfile;

        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
        assert(sched_as_string[1] == CORE1_SCHED_STRING);
        if (scheduler.write_recorded_schedule() != scheduler_t::STATUS_SUCCESS)
            assert(false);
    }
    {
        replay_file_checker_t checker;
        zipfile_istream_t infile(record_fname);
        std::string res = checker.check(&infile);
        if (!res.empty())
            std::cerr << "replay file checker failed: " << res;
        assert(res.empty());
    }
    // Now replay the schedule several times to ensure repeatability.
    for (int outer = 0; outer < 5; ++outer) {
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        for (int i = 0; i < NUM_INPUTS; i++) {
            memref_tid_t tid = TID_BASE + i;
            std::vector<scheduler_t::input_reader_t> readers;
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs[i])),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                tid);
            sched_inputs.emplace_back(std::move(readers));
        }
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_AS_PREVIOUSLY,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/2);
        zipfile_istream_t infile(record_fname);
        sched_ops.schedule_replay_istream = &infile;

        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
        assert(sched_as_string[1] == CORE1_SCHED_STRING);
    }
#endif // HAS_ZIP
}

#if (defined(X86_64) || defined(ARM_64)) && defined(HAS_ZIP)
static void
simulate_core_and_record_schedule(scheduler_t::stream_t *stream,
                                  const scheduler_t &scheduler,
                                  std::vector<context_switch_t> &thread_sequence)
{
    memref_t record;
    memref_tid_t prev_tid = INVALID_THREAD_ID;
    memtrace_stream_t *prev_stream = nullptr;
    for (scheduler_t::stream_status_t status = stream->next_record(record);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(record)) {
        if (status == scheduler_t::STATUS_WAIT || status == scheduler_t::STATUS_IDLE) {
            std::this_thread::yield();
            continue;
        }
        assert(status == scheduler_t::STATUS_OK);
        if (record.instr.tid != prev_tid && prev_tid != INVALID_THREAD_ID) {
            auto *new_stream =
                scheduler.get_input_stream_interface(stream->get_input_stream_ordinal());
            assert(new_stream != nullptr);
            assert(prev_stream != nullptr);
            thread_sequence.emplace_back(
                prev_tid, record.instr.tid,
                trace_position_t(stream->get_record_ordinal(),
                                 stream->get_instruction_ordinal(),
                                 stream->get_last_timestamp()),
                trace_position_t(prev_stream->get_record_ordinal(),
                                 prev_stream->get_instruction_ordinal(),
                                 prev_stream->get_last_timestamp()),
                trace_position_t(new_stream->get_record_ordinal(),
                                 new_stream->get_instruction_ordinal(),
                                 new_stream->get_last_timestamp()));
        }
        prev_tid = record.instr.tid;
        prev_stream =
            scheduler.get_input_stream_interface(stream->get_input_stream_ordinal());
    }
    if (thread_sequence.empty()) {
        // Create a single-thread entry.
        thread_sequence.emplace_back(INVALID_THREAD_ID, prev_tid,
                                     trace_position_t(0, 0, 0), trace_position_t(0, 0, 0),
                                     trace_position_t(0, 0, 0));
    }
}
#endif

static void
test_replay_multi_threaded(const char *testdir)
{
    std::cerr << "\n----------------\nTesting synthetic multi-threaded replay\n";
    // We want a larger input trace to better stress the scheduler.
#if (defined(X86_64) || defined(ARM_64)) && defined(HAS_ZIP)
    std::string path = std::string(testdir) + "/drmemtrace.threadsig.x64.tracedir";
    std::string record_fname = "tmp_test_replay_multi_record.zip";
    static constexpr int NUM_OUTPUTS = 4;
    std::vector<std::vector<context_switch_t>> thread_sequence(NUM_OUTPUTS);
    {
        // Record.
        scheduler_t scheduler;
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(path);
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/1);
        zipfile_ostream_t outfile(record_fname);
        sched_ops.schedule_record_ostream = &outfile;
        static constexpr int QUANTUM_DURATION = 2000;
        sched_ops.quantum_duration_instrs = QUANTUM_DURATION;
        // Keep the test short.
        static constexpr uint64_t BLOCK_MAX = 50;
        sched_ops.block_time_max_us = BLOCK_MAX;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::thread> threads;
        threads.reserve(NUM_OUTPUTS);
        for (int i = 0; i < NUM_OUTPUTS; ++i) {
            threads.emplace_back(std::thread(&simulate_core_and_record_schedule,
                                             scheduler.get_stream(i), std::ref(scheduler),
                                             std::ref(thread_sequence[i])));
        }
        for (std::thread &thread : threads)
            thread.join();
        if (scheduler.write_recorded_schedule() != scheduler_t::STATUS_SUCCESS)
            assert(false);
    }
    {
        replay_file_checker_t checker;
        zipfile_istream_t infile(record_fname);
        std::string res = checker.check(&infile);
        if (!res.empty())
            std::cerr << "replay file checker failed: " << res;
        assert(res.empty());
    }
    {
        // Replay.
        scheduler_t scheduler;
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(path);
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_AS_PREVIOUSLY,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/1);
        zipfile_istream_t infile(record_fname);
        sched_ops.schedule_replay_istream = &infile;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::vector<context_switch_t>> replay_sequence(NUM_OUTPUTS);
        std::vector<std::thread> threads;
        threads.reserve(NUM_OUTPUTS);
        for (int i = 0; i < NUM_OUTPUTS; ++i) {
            threads.emplace_back(std::thread(&simulate_core_and_record_schedule,
                                             scheduler.get_stream(i), std::ref(scheduler),
                                             std::ref(replay_sequence[i])));
        }
        for (std::thread &thread : threads)
            thread.join();
        std::cerr << "Recorded:\n";
        for (int i = 0; i < NUM_OUTPUTS; ++i) {
            std::cerr << "Core #" << i << ": ";
            for (const auto &cs : thread_sequence[i])
                std::cerr << "  " << cs << "\n";
            std::cerr << "\n";
        }
        std::cerr << "Replayed:\n";
        for (int i = 0; i < NUM_OUTPUTS; ++i) {
            std::cerr << "Core #" << i << ": ";
            for (const auto &cs : replay_sequence[i])
                std::cerr << "  " << cs << "\n";
            std::cerr << "\n";
        }
        for (int i = 0; i < NUM_OUTPUTS; ++i) {
            assert(thread_sequence[i].size() == replay_sequence[i].size());
            for (size_t j = 0; j < thread_sequence[i].size(); ++j) {
                assert(thread_sequence[i][j] == replay_sequence[i][j]);
            }
        }
    }
#endif
}

#ifdef HAS_ZIP
// We subclass scheduler_impl_t to access its record struct and functions.
// First we fill in pure-virtuals to share with a similar class below:
class test_scheduler_base_t : public scheduler_impl_t {
public:
    scheduler_status_t
    set_initial_schedule() override
    {
        return sched_type_t::STATUS_ERROR_NOT_IMPLEMENTED;
    }
    stream_status_t
    swap_out_input(output_ordinal_t output, input_ordinal_t input,
                   bool caller_holds_input_lock) override
    {
        return sched_type_t::STATUS_NOT_IMPLEMENTED;
    }
    stream_status_t
    swap_in_input(output_ordinal_t output, input_ordinal_t input) override
    {
        return sched_type_t::STATUS_NOT_IMPLEMENTED;
    }
    stream_status_t
    pick_next_input_for_mode(output_ordinal_t output, uint64_t blocked_time,
                             input_ordinal_t prev_index, input_ordinal_t &index) override
    {
        return sched_type_t::STATUS_NOT_IMPLEMENTED;
    }
    stream_status_t
    check_for_input_switch(output_ordinal_t output, memref_t &record, input_info_t *input,
                           uint64_t cur_time, bool &need_new_input, bool &preempt,
                           uint64_t &blocked_time) override
    {
        return sched_type_t::STATUS_NOT_IMPLEMENTED;
    }
    stream_status_t
    eof_or_idle_for_mode(output_ordinal_t output, input_ordinal_t prev_input) override
    {
        return sched_type_t::STATUS_NOT_IMPLEMENTED;
    }
};
class test_scheduler_t : public test_scheduler_base_t {
public:
    void
    write_test_schedule(std::string record_fname)
    {
        // This is hardcoded for 4 inputs and 2 outputs and a 3-instruction
        // scheduling quantum.
        // The 1st output's consumer was very slow and only managed 2 segments.
        scheduler_t scheduler;
        std::vector<scheduler_impl_t::schedule_record_t> sched0;
        sched0.emplace_back(scheduler_impl_t::schedule_record_t::VERSION, 0, 0, 0, 0);
        sched0.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 0, 0, 4, 11);
        // There is a huge time gap here.
        sched0.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 2, 7,
                            0xffffffffffffffffUL, 91);
        sched0.emplace_back(scheduler_impl_t::schedule_record_t::FOOTER, 0, 0, 0, 0);
        std::vector<schedule_record_t> sched1;
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::VERSION, 0, 0, 0, 0);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 1, 0, 4, 10);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 2, 0, 4, 20);
        // Input 2 advances early so core 0 is no longer waiting on it but only
        // the timestamp.
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 2, 4, 7, 60);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 3, 0, 4, 30);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 0, 4, 7, 40);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 1, 4, 7, 50);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 3, 4, 7, 70);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 0, 7,
                            0xffffffffffffffffUL, 80);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 1, 7,
                            0xffffffffffffffffUL, 90);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 3, 7,
                            0xffffffffffffffffUL, 110);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::FOOTER, 0, 0, 0, 0);
        zipfile_ostream_t outfile(record_fname);
        std::string err = outfile.open_new_component(recorded_schedule_component_name(0));
        assert(err.empty());
        if (!outfile.write(reinterpret_cast<char *>(sched0.data()),
                           sched0.size() * sizeof(sched0[0])))
            assert(false);
        err = outfile.open_new_component(recorded_schedule_component_name(1));
        assert(err.empty());
        if (!outfile.write(reinterpret_cast<char *>(sched1.data()),
                           sched1.size() * sizeof(sched1[0])))
            assert(false);
    }
};
#endif

static void
test_replay_timestamps()
{
#ifdef HAS_ZIP
    std::cerr << "\n----------------\nTesting replay timestamp ordering\n";
    static constexpr int NUM_INPUTS = 4;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int NUM_INSTRS = 9;
    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = TID_BASE + i;
        inputs[i].push_back(test_util::make_thread(tid));
        inputs[i].push_back(test_util::make_pid(1));
        // We need a timestamp so the scheduler will find one for initial
        // input processing.  We do not try to duplicate the timestamp
        // sequences in the stored file and just use a dummy timestamp here.
        inputs[i].push_back(test_util::make_timestamp(10 + i));
        for (int j = 0; j < NUM_INSTRS; j++)
            inputs[i].push_back(test_util::make_instr(42 + j * 4));
        inputs[i].push_back(test_util::make_exit(tid));
    }

    // Create a record file with timestamps requiring waiting.
    // We cooperate with the test_scheduler_t class which constructs this schedule:
    static const char *const CORE0_SCHED_STRING = ".AAA-------------------------CCC.____";
    static const char *const CORE1_SCHED_STRING = ".BBB.CCCCCC.DDDAAABBBDDDAAA.BBB.DDD.";
    std::string record_fname = "tmp_test_replay_timestamp.zip";
    test_scheduler_t test_scheduler;
    test_scheduler.write_test_schedule(record_fname);

    // Replay the recorded schedule.
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = TID_BASE + i;
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(inputs[i])),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            tid);
        sched_inputs.emplace_back(std::move(readers));
    }
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_AS_PREVIOUSLY,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/4);
    zipfile_istream_t infile(record_fname);
    sched_ops.schedule_replay_istream = &infile;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    assert(sched_as_string[0] == CORE0_SCHED_STRING);
    assert(sched_as_string[1] == CORE1_SCHED_STRING);
#endif // HAS_ZIP
}

#ifdef HAS_ZIP
// We subclass scheduler_impl_t to access its record struct and functions.
class test_noeof_scheduler_t : public test_scheduler_base_t {
public:
    void
    write_test_schedule(std::string record_fname)
    {
        // We duplicate test_scheduler_t but we have one input ending early before
        // eof.
        scheduler_t scheduler;
        std::vector<scheduler_impl_t::schedule_record_t> sched0;
        sched0.emplace_back(scheduler_impl_t::schedule_record_t::VERSION, 0, 0, 0, 0);
        sched0.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 0, 0, 4, 11);
        // There is a huge time gap here.
        // Max numeric value means continue until EOF.
        sched0.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 2, 7,
                            0xffffffffffffffffUL, 91);
        sched0.emplace_back(scheduler_impl_t::schedule_record_t::FOOTER, 0, 0, 0, 0);
        std::vector<schedule_record_t> sched1;
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::VERSION, 0, 0, 0, 0);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 1, 0, 4, 10);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 2, 0, 4, 20);
        // Input 2 advances early so core 0 is no longer waiting on it but only
        // the timestamp.
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 2, 4, 7, 60);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 3, 0, 4, 30);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 0, 4, 7, 40);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 1, 4, 7, 50);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 3, 4, 7, 70);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 0, 7,
                            0xffffffffffffffffUL, 80);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 1, 7,
                            0xffffffffffffffffUL, 90);
        // Input 3 never reaches EOF (end is exclusive: so it stops at 8 with the
        // real end at 9).
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::DEFAULT, 3, 7, 9, 110);
        sched1.emplace_back(scheduler_impl_t::schedule_record_t::FOOTER, 0, 0, 0, 0);
        zipfile_ostream_t outfile(record_fname);
        std::string err = outfile.open_new_component(recorded_schedule_component_name(0));
        assert(err.empty());
        if (!outfile.write(reinterpret_cast<char *>(sched0.data()),
                           sched0.size() * sizeof(sched0[0])))
            assert(false);
        err = outfile.open_new_component(recorded_schedule_component_name(1));
        assert(err.empty());
        if (!outfile.write(reinterpret_cast<char *>(sched1.data()),
                           sched1.size() * sizeof(sched1[0])))
            assert(false);
    }
};
#endif

static void
test_replay_noeof()
{
#ifdef HAS_ZIP
    std::cerr << "\n----------------\nTesting replay with no eof\n";
    static constexpr int NUM_INPUTS = 4;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int NUM_INSTRS = 9;
    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = TID_BASE + i;
        inputs[i].push_back(test_util::make_thread(tid));
        inputs[i].push_back(test_util::make_pid(1));
        // We need a timestamp so the scheduler will find one for initial
        // input processing.  We do not try to duplicate the timestamp
        // sequences in the stored file and just use a dummy timestamp here.
        inputs[i].push_back(test_util::make_timestamp(10 + i));
        for (int j = 0; j < NUM_INSTRS; j++)
            inputs[i].push_back(test_util::make_instr(42 + j * 4));
        inputs[i].push_back(test_util::make_exit(tid));
    }

    // Create a record file with timestamps requiring waiting.
    // We cooperate with the test_noeof_scheduler_t class which constructs this schedule:
    static const char *const CORE0_SCHED_STRING = ".AAA-------------------------CCC.__";
    static const char *const CORE1_SCHED_STRING = ".BBB.CCCCCC.DDDAAABBBDDDAAA.BBB.DD";
    std::string record_fname = "tmp_test_replay_noeof_timestamp.zip";
    test_noeof_scheduler_t test_scheduler;
    test_scheduler.write_test_schedule(record_fname);

    // Replay the recorded schedule.
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = TID_BASE + i;
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(inputs[i])),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            tid);
        sched_inputs.emplace_back(std::move(readers));
    }
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_AS_PREVIOUSLY,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/4);
    zipfile_istream_t infile(record_fname);
    sched_ops.schedule_replay_istream = &infile;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    assert(sched_as_string[0] == CORE0_SCHED_STRING);
    assert(sched_as_string[1] == CORE1_SCHED_STRING);
#endif // HAS_ZIP
}

static void
test_replay_skip()
{
#ifdef HAS_ZIP
    std::cerr << "\n----------------\nTesting replay of skips\n";
    std::vector<trace_entry_t> memrefs = {
        /* clang-format off */
        test_util::make_thread(1),
        test_util::make_pid(1),
        test_util::make_marker(TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
        test_util::make_timestamp(10),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 1),
        test_util::make_instr(1),
        test_util::make_instr(2), // Region 1 is just this instr.
        test_util::make_instr(3),
        test_util::make_timestamp(20),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 2),
        test_util::make_timestamp(30),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 3),
        test_util::make_instr(4),
        test_util::make_timestamp(40),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 4),
        test_util::make_instr(5),
        test_util::make_instr(6), // Region 2 starts here.
        test_util::make_timestamp(50),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 5),
        test_util::make_instr(7), // Region 2 ends here.
        test_util::make_instr(8),
        test_util::make_exit(1),
        /* clang-format on */
    };

    std::vector<scheduler_t::range_t> regions;
    // Instr counts are 1-based.
    regions.emplace_back(2, 2);
    regions.emplace_back(6, 7);

    std::string record_fname = "tmp_test_replay_skip.zip";

    {
        // Record.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(memrefs)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), 1);
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        sched_inputs[0].thread_modifiers.push_back(
            scheduler_t::input_thread_info_t(regions));
        scheduler_t scheduler;
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/4);
        zipfile_ostream_t outfile(record_fname);
        sched_ops.schedule_record_ostream = &outfile;
        if (scheduler.init(sched_inputs, 1, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        auto *stream = scheduler.get_stream(0);
        memref_t memref;
        for (scheduler_t::stream_status_t status = stream->next_record(memref);
             status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
            assert(status == scheduler_t::STATUS_OK);
        }
        if (scheduler.write_recorded_schedule() != scheduler_t::STATUS_SUCCESS)
            assert(false);
    }
    {
        replay_file_checker_t checker;
        zipfile_istream_t infile(record_fname);
        std::string res = checker.check(&infile);
        if (!res.empty())
            std::cerr << "replay file checker failed: " << res;
        assert(res.empty());
    }
    {
        // Replay.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(memrefs)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), 1);
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        sched_inputs[0].thread_modifiers.push_back(
            scheduler_t::input_thread_info_t(regions));
        scheduler_t scheduler;
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_AS_PREVIOUSLY,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/4);
        zipfile_istream_t infile(record_fname);
        sched_ops.schedule_replay_istream = &infile;
        if (scheduler.init(sched_inputs, 1, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        int ordinal = 0;
        auto *stream = scheduler.get_stream(0);
        memref_t memref;
        for (scheduler_t::stream_status_t status = stream->next_record(memref);
             status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
            assert(status == scheduler_t::STATUS_OK);
            switch (ordinal) {
            case 0:
                assert(memref.marker.type == TRACE_TYPE_MARKER);
                assert(memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP);
                assert(memref.marker.marker_value == 10);
                break;
            case 1:
                assert(memref.marker.type == TRACE_TYPE_MARKER);
                assert(memref.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID);
                // The value should be replaced by the shard id == 0.
                assert(memref.marker.marker_value == 0);
                break;
            case 2:
                assert(type_is_instr(memref.instr.type));
                assert(memref.instr.addr == 2);
                break;
            case 3:
                assert(memref.marker.type == TRACE_TYPE_MARKER);
                assert(memref.marker.marker_type == TRACE_MARKER_TYPE_WINDOW_ID);
                assert(memref.marker.marker_value == 1);
                break;
            case 4:
                assert(memref.marker.type == TRACE_TYPE_MARKER);
                assert(memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP);
                // The value should be replaced by a synthetic value: the initial (10)
                // won't have advanced to the next microsecond.
                assert(memref.marker.marker_value == 10);
                break;
            case 5:
                assert(memref.marker.type == TRACE_TYPE_MARKER);
                assert(memref.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID);
                assert(memref.marker.marker_value == 0);
                break;
            case 6:
                assert(type_is_instr(memref.instr.type));
                assert(memref.instr.addr == 6);
                break;
            case 7:
                assert(memref.marker.type == TRACE_TYPE_MARKER);
                assert(memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP);
                assert(memref.marker.marker_value == 10);
                break;
            case 8:
                assert(memref.marker.type == TRACE_TYPE_MARKER);
                assert(memref.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID);
                assert(memref.marker.marker_value == 0);
                break;
            case 9:
                assert(type_is_instr(memref.instr.type));
                assert(memref.instr.addr == 7);
                break;
            default:
                assert(ordinal == 10);
                assert(memref.exit.type == TRACE_TYPE_THREAD_EXIT);
            }
            ++ordinal;
        }
        assert(ordinal == 11);
    }
#endif
}

static void
test_replay_limit()
{
#ifdef HAS_ZIP
    std::cerr << "\n----------------\nTesting replay of ROI-limited inputs\n";

    std::vector<trace_entry_t> input_sequence;
    input_sequence.push_back(test_util::make_thread(/*tid=*/1));
    input_sequence.push_back(test_util::make_pid(/*pid=*/1));
    input_sequence.push_back(test_util::make_marker(TRACE_MARKER_TYPE_PAGE_SIZE, 4096));
    input_sequence.push_back(test_util::make_timestamp(10));
    input_sequence.push_back(test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 1));
    static constexpr int NUM_INSTRS = 1000;
    for (int i = 0; i < NUM_INSTRS; ++i) {
        input_sequence.push_back(test_util::make_instr(/*pc=*/i));
    }
    input_sequence.push_back(test_util::make_exit(/*tid=*/1));

    std::vector<scheduler_t::range_t> regions;
    // Instr counts are 1-based.  We stop just before the end, which has hit corner
    // cases in the past (i#6336).
    regions.emplace_back(1, NUM_INSTRS - 10);

    static constexpr int NUM_INPUTS = 3;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int BASE_TID = 100;
    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    for (int i = 0; i < NUM_INPUTS; i++) {
        inputs[i] = input_sequence;
        for (auto &record : inputs[i]) {
            if (record.type == TRACE_TYPE_THREAD || record.type == TRACE_TYPE_THREAD_EXIT)
                record.addr = static_cast<addr_t>(BASE_TID + i);
        }
    }

    std::string record_fname = "tmp_test_replay_limit.zip";
    std::vector<uint64_t> record_instr_count(NUM_OUTPUTS, 0);
    std::vector<std::string> record_schedule(NUM_OUTPUTS, "");

    auto simulate_core = [](scheduler_t::stream_t *stream, uint64_t *count,
                            std::string *schedule) {
        memref_t memref;
        for (scheduler_t::stream_status_t status = stream->next_record(memref);
             status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
            if (status == scheduler_t::STATUS_WAIT ||
                status == scheduler_t::STATUS_IDLE) {
                std::this_thread::yield();
                continue;
            }
            assert(status == scheduler_t::STATUS_OK);
            if (type_is_instr(memref.instr.type)) {
                ++(*count);
                *schedule += 'A' + static_cast<char>(memref.instr.tid - BASE_TID);
            }
        }
    };

    // First, test without interleaving (because the default quantum is long).
    // This triggers clear bugs like failing to run one entire input as its
    // reader is not initialized.
    std::cerr << "==== Record-replay with no interleaving ====\n";
    {
        // Record.
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        for (int i = 0; i < NUM_INPUTS; i++) {
            std::vector<scheduler_t::input_reader_t> readers;
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs[i])),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                BASE_TID + i);
            sched_inputs.emplace_back(std::move(readers));
            sched_inputs.back().thread_modifiers.push_back(
                scheduler_t::input_thread_info_t(regions));
        }
        scheduler_t scheduler;
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/2);
        zipfile_ostream_t outfile(record_fname);
        sched_ops.schedule_record_ostream = &outfile;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::thread> threads;
        threads.reserve(NUM_OUTPUTS);
        for (int i = 0; i < NUM_OUTPUTS; ++i) {
            threads.emplace_back(std::thread(simulate_core, scheduler.get_stream(i),
                                             &record_instr_count[i],
                                             &record_schedule[i]));
        }
        for (std::thread &thread : threads)
            thread.join();
        if (scheduler.write_recorded_schedule() != scheduler_t::STATUS_SUCCESS)
            assert(false);
    }
    {
        replay_file_checker_t checker;
        zipfile_istream_t infile(record_fname);
        std::string res = checker.check(&infile);
        if (!res.empty())
            std::cerr << "replay file checker failed: " << res;
        assert(res.empty());
        for (int i = 0; i < NUM_OUTPUTS; ++i) {
            std::cerr << "Output #" << i << " schedule: " << record_schedule[i] << "\n";
        }
    }
    // We create a function here as it is identical for the second test case below.
    auto replay_func = [&]() {
        std::cerr << "== Replay. ==\n";
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        for (int i = 0; i < NUM_INPUTS; i++) {
            std::vector<scheduler_t::input_reader_t> readers;
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs[i])),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                BASE_TID + i);
            sched_inputs.emplace_back(std::move(readers));
            sched_inputs.back().thread_modifiers.push_back(
                scheduler_t::input_thread_info_t(regions));
        }
        scheduler_t scheduler;
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_AS_PREVIOUSLY,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/2);
        zipfile_istream_t infile(record_fname);
        sched_ops.schedule_replay_istream = &infile;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<uint64_t> replay_instr_count(NUM_OUTPUTS, 0);
        std::vector<std::string> replay_schedule(NUM_OUTPUTS);
        std::vector<std::thread> threads;
        threads.reserve(NUM_OUTPUTS);
        for (int i = 0; i < NUM_OUTPUTS; ++i) {
            threads.emplace_back(std::thread(simulate_core, scheduler.get_stream(i),
                                             &replay_instr_count[i],
                                             &replay_schedule[i]));
        }
        for (std::thread &thread : threads)
            thread.join();
        for (int i = 0; i < NUM_OUTPUTS; ++i) {
            std::cerr << "Output #" << i << " recorded " << record_instr_count[i]
                      << " instrs vs replay " << replay_instr_count[i] << " instrs\n";
            assert(replay_instr_count[i] == record_instr_count[i]);
            std::cerr << "Output #" << i << " schedule: " << replay_schedule[i] << "\n";
            assert(replay_schedule[i] == record_schedule[i]);
        }
    };
    // Replay.
    replay_func();

    // Now use a smaller quantum with interleaving.
    std::cerr << "==== Record-replay with smaller quantum ====\n";
    record_instr_count = std::vector<uint64_t>(NUM_OUTPUTS, 0);
    record_schedule = std::vector<std::string>(NUM_OUTPUTS);
    {
        // Record.
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        for (int i = 0; i < NUM_INPUTS; i++) {
            std::vector<scheduler_t::input_reader_t> readers;
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs[i])),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                BASE_TID + i);
            sched_inputs.emplace_back(std::move(readers));
            sched_inputs.back().thread_modifiers.push_back(
                scheduler_t::input_thread_info_t(regions));
        }
        scheduler_t scheduler;
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/2);
        zipfile_ostream_t outfile(record_fname);
        sched_ops.schedule_record_ostream = &outfile;
        sched_ops.quantum_duration_instrs = NUM_INSTRS / 10;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::thread> threads;
        threads.reserve(NUM_OUTPUTS);
        for (int i = 0; i < NUM_OUTPUTS; ++i) {
            threads.emplace_back(std::thread(simulate_core, scheduler.get_stream(i),
                                             &record_instr_count[i],
                                             &record_schedule[i]));
        }
        for (std::thread &thread : threads)
            thread.join();
        if (scheduler.write_recorded_schedule() != scheduler_t::STATUS_SUCCESS)
            assert(false);
    }
    {
        replay_file_checker_t checker;
        zipfile_istream_t infile(record_fname);
        std::string res = checker.check(&infile);
        if (!res.empty())
            std::cerr << "replay file checker failed: " << res;
        assert(res.empty());
        int switches = 0;
        for (int i = 0; i < NUM_OUTPUTS; ++i) {
            std::cerr << "Output #" << i << " schedule: " << record_schedule[i] << "\n";
            for (size_t pos = 1; pos < record_schedule[i].size(); ++pos) {
                if (record_schedule[i][pos] != record_schedule[i][pos - 1])
                    ++switches;
            }
        }
        // The schedule varies by machine load and other factors so we don't
        // check for any precise ordering.
        // We do ensure we saw interleaving on at least one output.
        assert(switches > 0);
    }
    // Replay.
    replay_func();
#endif
}

static void
test_replay_as_traced()
{
#ifdef HAS_ZIP
    std::cerr << "\n----------------\nTesting replay as-traced\n";

    static constexpr int NUM_INPUTS = 5;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int NUM_INSTRS = 9;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr int CPU0 = 6;
    static constexpr int CPU1 = 9;
    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = TID_BASE + i;
        inputs[i].push_back(test_util::make_thread(tid));
        inputs[i].push_back(test_util::make_pid(1));
        // The last input will be earlier than all others. It will execute
        // 3 instrs on each core. This is to test the case when an output
        // begins in the wait state.
        for (int j = 0; j < (i == NUM_INPUTS - 1 ? 6 : NUM_INSTRS); j++)
            inputs[i].push_back(test_util::make_instr(42 + j * 4));
        inputs[i].push_back(test_util::make_exit(tid));
    }

    // Synthesize a cpu-schedule file.
    std::string cpu_fname = "tmp_test_cpu_as_traced.zip";
    static const char *const CORE0_SCHED_STRING = "EEE-AAA-CCCAAACCCBBB.DDD.___";
    static const char *const CORE1_SCHED_STRING = "---EEE.BBBDDDBBBDDDAAA.CCC.";
    {
        std::vector<schedule_entry_t> sched0;
        sched0.emplace_back(TID_BASE + 4, 10, CPU0, 0);
        sched0.emplace_back(TID_BASE, 101, CPU0, 0);
        sched0.emplace_back(TID_BASE + 2, 103, CPU0, 0);
        sched0.emplace_back(TID_BASE, 105, CPU0, 4);
        sched0.emplace_back(TID_BASE + 2, 107, CPU0, 4);
        sched0.emplace_back(TID_BASE + 1, 109, CPU0, 7);
        sched0.emplace_back(TID_BASE + 3, 111, CPU0, 7);
        std::vector<schedule_entry_t> sched1;
        sched1.emplace_back(TID_BASE + 4, 20, CPU1, 4);
        sched1.emplace_back(TID_BASE + 1, 102, CPU1, 0);
        sched1.emplace_back(TID_BASE + 3, 104, CPU1, 0);
        sched1.emplace_back(TID_BASE + 1, 106, CPU1, 4);
        sched1.emplace_back(TID_BASE + 3, 108, CPU1, 4);
        sched1.emplace_back(TID_BASE, 110, CPU1, 7);
        sched1.emplace_back(TID_BASE + 2, 112, CPU1, 7);
        std::ostringstream cpu0_string;
        cpu0_string << CPU0;
        std::ostringstream cpu1_string;
        cpu1_string << CPU1;
        zipfile_ostream_t outfile(cpu_fname);
        std::string err = outfile.open_new_component(cpu0_string.str());
        assert(err.empty());
        if (!outfile.write(reinterpret_cast<char *>(sched0.data()),
                           sched0.size() * sizeof(sched0[0])))
            assert(false);
        err = outfile.open_new_component(cpu1_string.str());
        assert(err.empty());
        if (!outfile.write(reinterpret_cast<char *>(sched1.data()),
                           sched1.size() * sizeof(sched1[0])))
            assert(false);
    }

    // Replay the recorded schedule.
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = TID_BASE + i;
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(inputs[i])),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            tid);
        sched_inputs.emplace_back(std::move(readers));
    }
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_RECORDED_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/3);
    zipfile_istream_t infile(cpu_fname);
    sched_ops.replay_as_traced_istream = &infile;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    // Test that we can find the mappings from as-traced cpuid to output stream,
    // even before calling next_record().
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        int64_t cpu = scheduler.get_stream(i)->get_output_cpuid();
        assert(cpu >= 0);
        if (i == 0)
            assert(cpu == CPU0);
        else
            assert(cpu == CPU1);
        assert(scheduler.get_output_cpuid(i) == cpu);
    }
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    assert(sched_as_string[0] == CORE0_SCHED_STRING);
    assert(sched_as_string[1] == CORE1_SCHED_STRING);
#endif
}

static void
test_replay_as_traced_i6107_workaround()
{
#ifdef HAS_ZIP
    std::cerr << "\n----------------\nTesting replay as-traced i#6107 workaround\n";

    // The i#6107 workaround applies to 10M-insruction chunks.
    static constexpr int NUM_INPUTS = 2;
    static constexpr int NUM_OUTPUTS = 1;
    static constexpr int CHUNK_NUM_INSTRS = 10 * 1000 * 1000;
    static constexpr int SCHED_STEP_INSTRS = 1000 * 1000;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr uint64_t TIMESTAMP_BASE = 100;
    static constexpr int CPU = 6;

    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    for (int input_idx = 0; input_idx < NUM_INPUTS; input_idx++) {
        memref_tid_t tid = TID_BASE + input_idx;
        inputs[input_idx].push_back(test_util::make_thread(tid));
        inputs[input_idx].push_back(test_util::make_pid(1));
        for (int step_idx = 0; step_idx <= CHUNK_NUM_INSTRS / SCHED_STEP_INSTRS;
             ++step_idx) {
            inputs[input_idx].push_back(test_util::make_timestamp(101 + step_idx));
            for (int instr_idx = 0; instr_idx < SCHED_STEP_INSTRS; ++instr_idx) {
                inputs[input_idx].push_back(test_util::make_instr(42 + instr_idx));
            }
        }
        inputs[input_idx].push_back(test_util::make_exit(tid));
    }

    // Synthesize a cpu-schedule file with the i#6107 bug.
    // Interleave the two inputs to test handling that.
    std::string cpu_fname = "tmp_test_cpu_i6107.zip";
    {
        std::vector<schedule_entry_t> sched;
        for (int step_idx = 0; step_idx <= CHUNK_NUM_INSTRS / SCHED_STEP_INSTRS;
             ++step_idx) {
            for (int input_idx = 0; input_idx < NUM_INPUTS; input_idx++) {
                sched.emplace_back(TID_BASE + input_idx, TIMESTAMP_BASE + step_idx, CPU,
                                   // The bug has modulo chunk count as the count.
                                   step_idx * SCHED_STEP_INSTRS % CHUNK_NUM_INSTRS);
            }
        }
        std::ostringstream cpu_string;
        cpu_string << CPU;
        zipfile_ostream_t outfile(cpu_fname);
        std::string err = outfile.open_new_component(cpu_string.str());
        assert(err.empty());
        if (!outfile.write(reinterpret_cast<char *>(sched.data()),
                           sched.size() * sizeof(sched[0])))
            assert(false);
    }

    // Replay the recorded schedule.
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    for (int input_idx = 0; input_idx < NUM_INPUTS; input_idx++) {
        memref_tid_t tid = TID_BASE + input_idx;
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(inputs[input_idx])),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            tid);
        sched_inputs.emplace_back(std::move(readers));
    }
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_RECORDED_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/2);
    zipfile_istream_t infile(cpu_fname);
    sched_ops.replay_as_traced_istream = &infile;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    // Since it initialized we didn't get an invalid schedule order.
    // Make sure the stream works too.
    auto *stream = scheduler.get_stream(0);
    memref_t memref;
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
    }
#endif
}

static void
test_replay_as_traced_dup_start()
{
#ifdef HAS_ZIP
    // Test what i#6712 fixes: duplicate start entries.
    std::cerr << "\n----------------\nTesting replay as-traced dup starts\n";

    static constexpr int NUM_INPUTS = 3;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int NUM_INSTRS = 6;
    static constexpr memref_tid_t TID_A = 100;
    static constexpr memref_tid_t TID_B = TID_A + 1;
    static constexpr memref_tid_t TID_C = TID_A + 2;
    static constexpr int CPU_0 = 6;
    static constexpr int CPU_1 = 7;
    static constexpr uint64_t TIMESTAMP_BASE = 100;

    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    for (int input_idx = 0; input_idx < NUM_INPUTS; input_idx++) {
        memref_tid_t tid = TID_A + input_idx;
        inputs[input_idx].push_back(test_util::make_thread(tid));
        inputs[input_idx].push_back(test_util::make_pid(1));
        // These timestamps do not line up with the schedule file but
        // that does not cause problems and leaving it this way
        // simplifies the testdata construction.
        inputs[input_idx].push_back(test_util::make_timestamp(TIMESTAMP_BASE));
        for (int instr_idx = 0; instr_idx < NUM_INSTRS; ++instr_idx) {
            inputs[input_idx].push_back(test_util::make_instr(42 + instr_idx));
        }
        inputs[input_idx].push_back(test_util::make_exit(tid));
    }

    // Synthesize a cpu-schedule file with duplicate starts.
    std::string cpu_fname = "tmp_test_cpu_i6712.zip";
    {
        zipfile_ostream_t outfile(cpu_fname);
        {
            std::vector<schedule_entry_t> sched;
            sched.emplace_back(TID_A, TIMESTAMP_BASE, CPU_0, 0);
            sched.emplace_back(TID_B, TIMESTAMP_BASE + 2, CPU_0, 0);
            // Simple dup start: non-consecutive but in same output.
            sched.emplace_back(TID_A, TIMESTAMP_BASE + 4, CPU_0, 0);
            sched.emplace_back(TID_B, TIMESTAMP_BASE + 5, CPU_0, 4);
            std::ostringstream cpu_string;
            cpu_string << CPU_0;
            std::string err = outfile.open_new_component(cpu_string.str());
            assert(err.empty());
            if (!outfile.write(reinterpret_cast<char *>(sched.data()),
                               sched.size() * sizeof(sched[0])))
                assert(false);
        }
        {
            std::vector<schedule_entry_t> sched;
            // More complex dup start across outputs.
            sched.emplace_back(TID_B, TIMESTAMP_BASE + 1, CPU_1, 0);
            sched.emplace_back(TID_C, TIMESTAMP_BASE + 3, CPU_1, 0);
            sched.emplace_back(TID_A, TIMESTAMP_BASE + 6, CPU_1, 4);
            std::ostringstream cpu_string;
            cpu_string << CPU_1;
            std::string err = outfile.open_new_component(cpu_string.str());
            assert(err.empty());
            if (!outfile.write(reinterpret_cast<char *>(sched.data()),
                               sched.size() * sizeof(sched[0])))
                assert(false);
        }
    }

    // Replay the recorded schedule.
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    for (int input_idx = 0; input_idx < NUM_INPUTS; input_idx++) {
        memref_tid_t tid = TID_A + input_idx;
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(inputs[input_idx])),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            tid);
        sched_inputs.emplace_back(std::move(readers));
    }
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_RECORDED_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/4);
    zipfile_istream_t infile(cpu_fname);
    sched_ops.replay_as_traced_istream = &infile;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    auto *stream0 = scheduler.get_stream(0);
    auto *stream1 = scheduler.get_stream(1);
    auto check_next = [](scheduler_t::stream_t *stream,
                         scheduler_t::stream_status_t expect_status,
                         memref_tid_t expect_tid = INVALID_THREAD_ID,
                         trace_type_t expect_type = TRACE_TYPE_READ) {
        memref_t memref;
        scheduler_t::stream_status_t status = stream->next_record(memref);
        if (status != expect_status) {
            std::cerr << "Expected status " << expect_status << " != " << status << "\n";
            assert(false);
        }
        if (status == scheduler_t::STATUS_OK) {
            if (memref.marker.tid != expect_tid) {
                std::cerr << "Expected tid " << expect_tid << " != " << memref.marker.tid
                          << "\n";
                assert(false);
            }
            if (memref.marker.type != expect_type) {
                std::cerr << "Expected type " << expect_type
                          << " != " << memref.marker.type << "\n";
                assert(false);
            }
        }
    };
    // We expect the 1st of the start-at-0 TID_A to be deleted; so we should
    // start with TID_B (the 2nd of the start-at-0 TID_B).
    check_next(stream0, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_MARKER);
    check_next(stream0, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
    check_next(stream0, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
    check_next(stream0, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
    // We should have removed the 1st start-at-0  B and start with C
    // on cpu 1.
    check_next(stream1, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_MARKER);
    check_next(stream1, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_INSTR);
    check_next(stream1, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_INSTR);
    check_next(stream1, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_INSTR);
    check_next(stream1, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_INSTR);
    check_next(stream1, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_INSTR);
    check_next(stream1, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_INSTR);
    check_next(stream1, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_THREAD_EXIT);
    // Now cpu 0 should run A.
    check_next(stream0, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_MARKER);
    check_next(stream0, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_INSTR);
    check_next(stream0, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_INSTR);
    check_next(stream0, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_INSTR);
    // Cpu 0 now finishes with B.
    check_next(stream0, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
    check_next(stream0, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
    check_next(stream0, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
    check_next(stream0, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_THREAD_EXIT);
    check_next(stream0, scheduler_t::STATUS_IDLE);
    // Cpu 1 now finishes with A.
    check_next(stream1, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_INSTR);
    check_next(stream1, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_INSTR);
    check_next(stream1, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_INSTR);
    check_next(stream1, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_THREAD_EXIT);
    check_next(stream1, scheduler_t::STATUS_EOF);
    // Finalize.
    check_next(stream0, scheduler_t::STATUS_EOF);
#endif
}

static void
test_replay_as_traced_sort()
{
#ifdef HAS_ZIP
    // Test that outputs have the cpuids in sorted order.
    std::cerr << "\n----------------\nTesting replay as-traced sorting\n";

    static constexpr int NUM_INPUTS = 4;
    static constexpr int NUM_OUTPUTS = NUM_INPUTS; // Required to be equal.
    static constexpr int NUM_INSTRS = 2;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr addr_t PC_BASE = 1000;
    // Our unsorted cpuid order in the file.
    static const std::vector<int> CPUIDS = { 42, 7, 56, 3 };
    // Index into CPUIDS if sorted.
    static const std::vector<int> INDICES = { 3, 1, 0, 2 };
    static constexpr uint64_t TIMESTAMP_BASE = 100;

    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    for (int input_idx = 0; input_idx < NUM_INPUTS; input_idx++) {
        memref_tid_t tid = TID_BASE + input_idx;
        inputs[input_idx].push_back(test_util::make_thread(tid));
        inputs[input_idx].push_back(test_util::make_pid(1));
        // These timestamps do not line up with the schedule file but
        // that does not cause problems and leaving it this way
        // simplifies the testdata construction.
        inputs[input_idx].push_back(test_util::make_timestamp(TIMESTAMP_BASE));
        inputs[input_idx].push_back(
            test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, CPUIDS[input_idx]));
        for (int instr_idx = 0; instr_idx < NUM_INSTRS; ++instr_idx) {
            inputs[input_idx].push_back(test_util::make_instr(PC_BASE + instr_idx));
        }
        inputs[input_idx].push_back(test_util::make_exit(tid));
    }

    // Synthesize a cpu-schedule file with unsorted entries (see CPUIDS above).
    std::string cpu_fname = "tmp_test_cpu_i6721.zip";
    {
        zipfile_ostream_t outfile(cpu_fname);
        for (int i = 0; i < NUM_OUTPUTS; ++i) {
            std::vector<schedule_entry_t> sched;
            sched.emplace_back(TID_BASE + i, TIMESTAMP_BASE, CPUIDS[i], 0);
            std::ostringstream cpu_string;
            cpu_string << CPUIDS[i];
            std::string err = outfile.open_new_component(cpu_string.str());
            assert(err.empty());
            if (!outfile.write(reinterpret_cast<char *>(sched.data()),
                               sched.size() * sizeof(sched[0])))
                assert(false);
        }
    }

    // Replay the recorded schedule.
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = TID_BASE + i;
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(inputs[i])),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            tid);
        sched_inputs.emplace_back(std::move(readers));
    }
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_RECORDED_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/4);
    zipfile_istream_t infile(cpu_fname);
    sched_ops.replay_as_traced_istream = &infile;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    for (int i = 0; i < NUM_OUTPUTS; ++i) {
        auto *stream = scheduler.get_stream(i);
        memref_t memref;
        scheduler_t::stream_status_t status = stream->next_record(memref);
        if (status == scheduler_t::STATUS_OK) {
            assert(memref.marker.tid == TID_BASE + INDICES[i]);
            if (memref.marker.type == TRACE_TYPE_MARKER &&
                memref.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID) {
                assert(static_cast<int>(memref.marker.marker_value) ==
                       CPUIDS[INDICES[i]]);
            }
        } else
            assert(status == scheduler_t::STATUS_EOF);
    }
#endif
}

static void
test_replay_as_traced_from_file(const char *testdir)
{
#if (defined(X86_64) || defined(ARM_64)) && defined(HAS_ZIP)
    std::cerr << "\n----------------\nTesting replay as-traced from a file\n";
    std::string path = std::string(testdir) + "/drmemtrace.threadsig.x64.tracedir";
    std::string cpu_file =
        std::string(testdir) + "/drmemtrace.threadsig.x64.tracedir/cpu_schedule.bin.zip";
    static constexpr int NUM_OUTPUTS = 11; // Matches the actual trace's core footprint.
    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(path);
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_RECORDED_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/2);
    std::cerr << "Reading cpu file " << cpu_file << "\n";
    zipfile_istream_t infile(cpu_file);
    sched_ops.replay_as_traced_istream = &infile;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::vector<context_switch_t>> replay_sequence(NUM_OUTPUTS);
    std::vector<std::thread> threads;
    threads.reserve(NUM_OUTPUTS);
    for (int i = 0; i < NUM_OUTPUTS; ++i) {
        threads.emplace_back(std::thread(&simulate_core_and_record_schedule,
                                         scheduler.get_stream(i), std::ref(scheduler),
                                         std::ref(replay_sequence[i])));
    }
    for (std::thread &thread : threads)
        thread.join();
    std::ostringstream replay_string;
    for (int i = 0; i < NUM_OUTPUTS; ++i) {
        replay_string << "Core #" << i << ": ";
        for (const auto &cs : replay_sequence[i])
            replay_string << cs << " ";
        replay_string << "\n";
    }
    std::cerr << "As-traced replay:\n" << replay_string.str();
    assert(std::regex_search(replay_string.str(),
                             std::regex(R"DELIM(Core #0: 872902 => 872905.*
(.|\n)*
Core #10: 872901 => 872905.*
)DELIM")));
#endif
}

static void
test_times_of_interest()
{
#ifdef HAS_ZIP
    std::cerr << "\n----------------\nTesting times of interest\n";

    static constexpr int NUM_INPUTS = 3;
    static constexpr int NUM_OUTPUTS = 1;
    static constexpr int NUM_TIMESTAMPS = 3;
    static constexpr int NUM_INSTRS_PER_TIMESTAMP = 3;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr addr_t PC_BASE = 42;
    static constexpr int CPU0 = 6;
    static constexpr int CPU1 = 9;
    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    for (int i = 0; i < NUM_INPUTS; ++i) {
        memref_tid_t tid = TID_BASE + i;
        inputs[i].push_back(test_util::make_thread(tid));
        inputs[i].push_back(test_util::make_pid(1));
        for (int j = 0; j < NUM_TIMESTAMPS; ++j) {
            uint64_t timestamp = i == 2 ? (1 + 5 * (j + 1)) : (10 * (j + 1) + 10 * i);
            inputs[i].push_back(test_util::make_timestamp(timestamp));
            for (int k = 0; k < NUM_INSTRS_PER_TIMESTAMP; ++k) {
                inputs[i].push_back(test_util::make_instr(
                    PC_BASE + 1 /*1-based ranges*/ + j * NUM_INSTRS_PER_TIMESTAMP + k));
            }
        }
        inputs[i].push_back(test_util::make_exit(tid));
    }

    // Synthesize a cpu-schedule file.
    std::string cpu_fname = "tmp_test_times_of_interest.zip";
    {
        std::vector<schedule_entry_t> sched0;
        std::vector<schedule_entry_t> sched1;
        // We do not bother to interleave to make it easier to see the sequence
        // in this test.
        // Thread A.
        sched0.emplace_back(TID_BASE + 0, 10, CPU0, 0);
        sched0.emplace_back(TID_BASE + 0, 20, CPU0, 4);
        sched0.emplace_back(TID_BASE + 0, 30, CPU0, 7);
        // Thread B.
        sched0.emplace_back(TID_BASE + 1, 20, CPU0, 0);
        sched0.emplace_back(TID_BASE + 1, 30, CPU0, 4);
        sched0.emplace_back(TID_BASE + 1, 40, CPU0, 7);
        // Thread C.
        sched1.emplace_back(TID_BASE + 2, 6, CPU1, 0);
        sched1.emplace_back(TID_BASE + 2, 11, CPU1, 4);
        sched1.emplace_back(TID_BASE + 2, 16, CPU1, 7);
        std::ostringstream cpu0_string;
        cpu0_string << CPU0;
        std::ostringstream cpu1_string;
        cpu1_string << CPU1;
        zipfile_ostream_t outfile(cpu_fname);
        std::string err = outfile.open_new_component(cpu0_string.str());
        assert(err.empty());
        if (!outfile.write(reinterpret_cast<char *>(sched0.data()),
                           sched0.size() * sizeof(sched0[0])))
            assert(false);
        err = outfile.open_new_component(cpu1_string.str());
        assert(err.empty());
        if (!outfile.write(reinterpret_cast<char *>(sched1.data()),
                           sched1.size() * sizeof(sched1[0])))
            assert(false);
    }

    {
        // Test an erroneous range request with no gap.
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        std::vector<scheduler_t::input_reader_t> readers;
        for (int i = 0; i < NUM_INPUTS; i++) {
            memref_tid_t tid = TID_BASE + i;
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs[i])),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                tid);
        }
        sched_inputs.emplace_back(std::move(readers));
        // Pick times that have adjacent corresponding instructions: 30 and 32
        // have a time gap but no instruction gap.
        sched_inputs.back().times_of_interest = { { 25, 30 }, { 32, 33 } };
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        zipfile_istream_t infile(cpu_fname);
        sched_ops.replay_as_traced_istream = &infile;
        scheduler_t scheduler;
        assert(scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) ==
               scheduler_t::STATUS_ERROR_INVALID_PARAMETER);
    }
    {
        // Test a valid range request.
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        std::vector<scheduler_t::input_reader_t> readers;
        for (int i = 0; i < NUM_INPUTS; i++) {
            memref_tid_t tid = TID_BASE + i;
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs[i])),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                tid);
        }
        sched_inputs.emplace_back(std::move(readers));
        sched_inputs.back().times_of_interest = { { 25, 30 }, { 38, 39 } };
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        zipfile_istream_t infile(cpu_fname);
        sched_ops.replay_as_traced_istream = &infile;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS) {
            std::cerr << scheduler.get_error_string() << "\n";
            assert(false);
        }
        auto *stream0 = scheduler.get_stream(0);
        auto check_next = [](scheduler_t::stream_t *stream, memref_tid_t expect_tid,
                             trace_type_t expect_type, addr_t expect_addr = 0) {
            memref_t record;
            scheduler_t::stream_status_t status = stream->next_record(record);
            assert(status == scheduler_t::STATUS_OK);
            assert(record.instr.tid == expect_tid);
            if (record.instr.type != expect_type) {
                std::cerr << "Expected type " << expect_type
                          << " != " << record.instr.type << "\n";
                assert(false);
            }
            if (expect_addr != 0 && record.instr.addr != expect_addr) {
                std::cerr << "Expected addr " << expect_addr
                          << " != " << record.instr.addr << "\n";
                assert(false);
            }
        };
        // Range is 5 until the end.
        check_next(stream0, TID_BASE + 0, TRACE_TYPE_INSTR, PC_BASE + 5);
        check_next(stream0, TID_BASE + 0, TRACE_TYPE_INSTR, PC_BASE + 6);
        check_next(stream0, TID_BASE + 0, TRACE_TYPE_MARKER);
        check_next(stream0, TID_BASE + 0, TRACE_TYPE_INSTR, PC_BASE + 7);
        check_next(stream0, TID_BASE + 0, TRACE_TYPE_INSTR, PC_BASE + 8);
        check_next(stream0, TID_BASE + 0, TRACE_TYPE_INSTR, PC_BASE + 9);
        check_next(stream0, TID_BASE + 0, TRACE_TYPE_THREAD_EXIT);
        // Two ranges: 2-4 and 6-7.
        check_next(stream0, TID_BASE + 1, TRACE_TYPE_INSTR, PC_BASE + 2);
        check_next(stream0, TID_BASE + 1, TRACE_TYPE_INSTR, PC_BASE + 3);
        check_next(stream0, TID_BASE + 1, TRACE_TYPE_MARKER);
        check_next(stream0, TID_BASE + 1, TRACE_TYPE_INSTR, PC_BASE + 4);
        // Window id marker in between.
        check_next(stream0, TID_BASE + 1, TRACE_TYPE_MARKER);
        check_next(stream0, TID_BASE + 1, TRACE_TYPE_INSTR, PC_BASE + 6);
        check_next(stream0, TID_BASE + 1, TRACE_TYPE_MARKER);
        check_next(stream0, TID_BASE + 1, TRACE_TYPE_INSTR, PC_BASE + 7);
        check_next(stream0, TID_BASE + 1, TRACE_TYPE_THREAD_EXIT);
        memref_t record;
        assert(stream0->next_record(record) == scheduler_t::STATUS_EOF);
    }
#endif
}

static void
test_inactive()
{
#ifdef HAS_ZIP
    std::cerr << "\n----------------\nTesting inactive cores\n";
    static constexpr memref_tid_t TID_A = 42;
    static constexpr memref_tid_t TID_B = TID_A + 1;
    static constexpr int NUM_OUTPUTS = 2;
    std::vector<trace_entry_t> refs_A = {
        /* clang-format off */
        test_util::make_thread(TID_A),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_timestamp(10),
        test_util::make_instr(10),
        test_util::make_instr(30),
        test_util::make_instr(50),
        test_util::make_exit(TID_A),
        /* clang-format on */
    };
    std::vector<trace_entry_t> refs_B = {
        /* clang-format off */
        test_util::make_thread(TID_B),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_timestamp(20),
        test_util::make_instr(20),
        test_util::make_instr(40),
        test_util::make_instr(60),
        test_util::make_instr(80),
        test_util::make_exit(TID_B),
        /* clang-format on */
    };
    std::string record_fname = "tmp_test_replay_inactive.zip";
    {
        // Record.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_B)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_B);
        scheduler_t scheduler;
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/4);
        sched_ops.quantum_duration_instrs = 2;
        zipfile_ostream_t outfile(record_fname);
        sched_ops.schedule_record_ostream = &outfile;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        auto *stream0 = scheduler.get_stream(0);
        auto *stream1 = scheduler.get_stream(1);
        auto check_next = [](scheduler_t::stream_t *stream,
                             scheduler_t::stream_status_t expect_status,
                             memref_tid_t expect_tid = INVALID_THREAD_ID,
                             trace_type_t expect_type = TRACE_TYPE_READ) {
            memref_t memref;
            scheduler_t::stream_status_t status = stream->next_record(memref);
            assert(status == expect_status);
            if (status == scheduler_t::STATUS_OK) {
                if (memref.marker.tid != expect_tid) {
                    std::cerr << "Expected tid " << expect_tid
                              << " != " << memref.marker.tid << "\n";
                    assert(false);
                }
                if (memref.marker.type != expect_type) {
                    std::cerr << "Expected type " << expect_type
                              << " != " << memref.marker.type << "\n";
                    assert(false);
                }
            }
        };
        scheduler_t::stream_status_t status;
        // Unreading before reading should fail.
        status = stream0->unread_last_record();
        assert(status == scheduler_t::STATUS_INVALID);
        // Advance cpu0 to its 1st instr.
        check_next(stream0, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_MARKER);
        check_next(stream0, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_MARKER);
        check_next(stream0, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_INSTR);
        // Test unreading and re-reading.
        uint64_t ref_ord = stream0->get_record_ordinal();
        uint64_t instr_ord = stream0->get_instruction_ordinal();
        status = stream0->unread_last_record();
        assert(status == scheduler_t::STATUS_OK);
        assert(stream0->get_record_ordinal() == ref_ord - 1);
        assert(stream0->get_instruction_ordinal() == instr_ord - 1);
        // Speculation with queuing right after unread should fail.
        assert(stream0->start_speculation(300, true) == scheduler_t::STATUS_INVALID);
        check_next(stream0, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_INSTR);
        assert(stream0->get_record_ordinal() == ref_ord);
        assert(stream0->get_instruction_ordinal() == instr_ord);
        // Advance cpu1 to its 1st instr.
        check_next(stream1, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_MARKER);
        check_next(stream1, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_MARKER);
        check_next(stream1, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
        // Read one further than we want to process and then put it back.
        check_next(stream1, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
        status = stream1->unread_last_record();
        assert(status == scheduler_t::STATUS_OK);
        // Consecutive unread should fail.
        status = stream1->unread_last_record();
        assert(status == scheduler_t::STATUS_INVALID);
        // Make cpu1 inactive.
        status = stream1->set_active(false);
        assert(status == scheduler_t::STATUS_OK);
        check_next(stream1, scheduler_t::STATUS_IDLE);
        // Test making cpu1 inactive while it's already inactive.
        status = stream1->set_active(false);
        assert(status == scheduler_t::STATUS_OK);
        check_next(stream1, scheduler_t::STATUS_IDLE);
        // Advance cpu0 to its quantum end.
        check_next(stream0, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_INSTR);
        // Ensure cpu0 now picks up the input that was on cpu1.
        // This is also the record we un-read earlier.
        check_next(stream0, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
        // End of quantum.
        check_next(stream0, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_INSTR);
        // Make cpu1 active and then cpu0 inactive.
        status = stream1->set_active(true);
        assert(status == scheduler_t::STATUS_OK);
        status = stream0->set_active(false);
        assert(status == scheduler_t::STATUS_OK);
        check_next(stream0, scheduler_t::STATUS_IDLE);
        // Now cpu1 should finish things.
        check_next(stream1, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_THREAD_EXIT);
        check_next(stream1, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
        check_next(stream1, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
        check_next(stream1, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_THREAD_EXIT);
        check_next(stream1, scheduler_t::STATUS_EOF);
        if (scheduler.write_recorded_schedule() != scheduler_t::STATUS_SUCCESS)
            assert(false);
    }
    {
        replay_file_checker_t checker;
        zipfile_istream_t infile(record_fname);
        std::string res = checker.check(&infile);
        if (!res.empty())
            std::cerr << "replay file checker failed: " << res;
        assert(res.empty());
    }
    {
        // Replay.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_B)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_B);
        scheduler_t scheduler;
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_AS_PREVIOUSLY,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/4);
        zipfile_istream_t infile(record_fname);
        sched_ops.schedule_replay_istream = &infile;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_A);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == "..AAB.___");
        assert(sched_as_string[1] == "..B-ABB.");
    }
#endif // HAS_ZIP
}

static void
test_direct_switch()
{
    std::cerr << "\n----------------\nTesting direct switches\n";
    // This tests just direct switches with no unscheduled inputs or related
    // switch requests.
    // We have just 1 output to better control the order and avoid flakiness.
    static constexpr int NUM_OUTPUTS = 1;
    static constexpr int QUANTUM_DURATION = 100; // Never reached.
    static constexpr int BLOCK_LATENCY = 100;
    static constexpr int SWITCH_TIMEOUT = 2000;
    static constexpr double BLOCK_SCALE = 1. / (BLOCK_LATENCY);
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr memref_tid_t TID_A = TID_BASE + 0;
    static constexpr memref_tid_t TID_B = TID_BASE + 1;
    static constexpr memref_tid_t TID_C = TID_BASE + 2;
    std::vector<trace_entry_t> refs_A = {
        test_util::make_thread(TID_A),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        // A has the earliest timestamp and starts.
        test_util::make_timestamp(1001),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_instr(/*pc=*/101),
        test_util::make_instr(/*pc=*/102),
        test_util::make_timestamp(1002),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        // This test focuses on direct only with nothing "unscheduled";
        // thus, we always provide a timeout to avoid going unscheduled.
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_ARG_TIMEOUT, SWITCH_TIMEOUT),
        test_util::make_marker(TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH, TID_C),
        test_util::make_timestamp(4001),
        test_util::make_instr(/*pc=*/401),
        test_util::make_exit(TID_A),
    };
    std::vector<trace_entry_t> refs_B = {
        test_util::make_thread(TID_B),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        // B would go next by timestamp, so this is a good test of direct switches.
        test_util::make_timestamp(2001),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_instr(/*pc=*/201),
        test_util::make_instr(/*pc=*/202),
        test_util::make_instr(/*pc=*/203),
        test_util::make_instr(/*pc=*/204),
        test_util::make_exit(TID_B),
    };
    std::vector<trace_entry_t> refs_C = {
        test_util::make_thread(TID_C),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_timestamp(3001),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_instr(/*pc=*/301),
        test_util::make_instr(/*pc=*/302),
        test_util::make_timestamp(3002),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        // This test focuses on direct only with nothing "unscheduled";
        // thus, we always provide a timeout to avoid going unscheduled.
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_ARG_TIMEOUT, SWITCH_TIMEOUT),
        test_util::make_marker(TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH, TID_A),
        test_util::make_timestamp(5001),
        test_util::make_instr(/*pc=*/501),
        // Test a non-existent target: should be ignored, but not crash.
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        // This test focuses on direct only with nothing "unscheduled".
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_ARG_TIMEOUT, SWITCH_TIMEOUT),
        test_util::make_marker(TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH, TID_BASE + 3),
        test_util::make_exit(TID_C),
    };
    {
        // Test the defaults with direct switches enabled.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_B)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_B);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_C)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_C);
        // The string constructor writes "." for markers.
        // We expect A's first switch to be to C even though B has an earlier timestamp.
        // We expect C's direct switch to A to proceed immediately even though A still
        // has significant blocked time left.  But then after B is scheduled and finishes,
        // we still have to wait for C's block time so we see idle underscores:
        static const char *const CORE0_SCHED_STRING =
            "...AA..........CC.......A....BBBB._______________C....";
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        sched_ops.quantum_duration_us = QUANTUM_DURATION;
        // We use our mock's time==instruction count for a deterministic result.
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.time_units_per_us = 1.;
        sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
        sched_ops.block_time_multiplier = BLOCK_SCALE;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
        verify_scheduler_stats(scheduler.get_stream(0), /*switch_input_to_input=*/3,
                               /*switch_input_to_idle=*/1, /*switch_idle_to_input=*/1,
                               /*switch_nop=*/0, /*preempts=*/0, /*direct_attempts=*/3,
                               /*direct_successes=*/2, /*migrations=*/0);
    }
    {
        // Test disabling direct switches.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_B)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_B);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_C)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_C);
        // The string constructor writes "." for markers.
        // We expect A's first switch to be to B with an earlier timestamp.
        // We expect C's direct switch to A to not happen until A's blocked time ends.
        static const char *const CORE0_SCHED_STRING =
            "...AA..........BBBB....CC.......___________________C....___A.";
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        sched_ops.quantum_duration_us = QUANTUM_DURATION;
        // We use our mock's time==instruction count for a deterministic result.
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.time_units_per_us = 1.;
        sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
        sched_ops.block_time_multiplier = BLOCK_SCALE;
        sched_ops.honor_direct_switches = false;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
        verify_scheduler_stats(scheduler.get_stream(0), /*switch_input_to_input=*/2,
                               /*switch_input_to_idle=*/2, /*switch_idle_to_input=*/2,
                               /*switch_nop=*/0, /*preempts=*/0, /*direct_attempts=*/0,
                               /*direct_successes=*/0, /*migrations=*/0);
    }
}

static void
test_unscheduled_base()
{
    std::cerr << "\n----------------\nTesting unscheduled inputs\n";
    // We have just 1 output to better control the order and avoid flakiness.
    static constexpr int NUM_OUTPUTS = 1;
    static constexpr int QUANTUM_DURATION = 100; // Never reached.
    static constexpr int BLOCK_LATENCY = 100;
    static constexpr double BLOCK_SCALE = 1. / (BLOCK_LATENCY);
    static constexpr int SWITCH_TIMEOUT = 1000;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr memref_tid_t TID_A = TID_BASE + 0;
    static constexpr memref_tid_t TID_B = TID_BASE + 1;
    static constexpr memref_tid_t TID_C = TID_BASE + 2;
    std::vector<trace_entry_t> refs_A = {
        test_util::make_thread(TID_A),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        // A has the earliest timestamp and starts.
        test_util::make_timestamp(1001),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_instr(/*pc=*/101),
        test_util::make_instr(/*pc=*/102),
        test_util::make_timestamp(1002),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        // Test going unscheduled with no timeout.
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE, 0),
        test_util::make_timestamp(4202),
        // B makes us scheduled again.
        test_util::make_instr(/*pc=*/103),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        // Switch to B to test a direct switch to unscheduled.
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_ARG_TIMEOUT, SWITCH_TIMEOUT),
        test_util::make_marker(TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH, TID_B),
        test_util::make_timestamp(4402),
        test_util::make_instr(/*pc=*/401),
        test_util::make_exit(TID_A),
    };
    std::vector<trace_entry_t> refs_B = {
        test_util::make_thread(TID_B),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        // B runs next by timestamp.
        test_util::make_timestamp(2001),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_instr(/*pc=*/200),
        // B goes unscheduled with a timeout.
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_ARG_TIMEOUT, SWITCH_TIMEOUT),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE, 0),
        // C will run at this point.
        // Then, C blocks and our timeout lapses and we run again.
        test_util::make_timestamp(4001),
        test_util::make_instr(/*pc=*/201),
        // B tells C to not go unscheduled later.
        test_util::make_timestamp(4002),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_SCHEDULE, TID_C),
        test_util::make_timestamp(4004),
        test_util::make_instr(/*pc=*/202),
        // B makes A no longer unscheduled.
        test_util::make_timestamp(4006),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_SCHEDULE, TID_A),
        test_util::make_timestamp(4011),
        test_util::make_instr(/*pc=*/202),
        // B now goes unscheduled with no timeout.
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE, 0),
        // A switches to us.
        test_util::make_instr(/*pc=*/203),
        test_util::make_instr(/*pc=*/204),
        test_util::make_instr(/*pc=*/205),
        test_util::make_instr(/*pc=*/206),
        test_util::make_exit(TID_B),
    };
    std::vector<trace_entry_t> refs_C = {
        test_util::make_thread(TID_C),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        // C goes 3rd by timestamp.
        test_util::make_timestamp(3001),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_instr(/*pc=*/301),
        test_util::make_instr(/*pc=*/302),
        test_util::make_timestamp(3002),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        // C makes a long-latency blocking syscall, testing whether
        // A is still unscheduled.
        // We also test _SCHEDULE avoiding a future unschedule when C
        // unblocks.
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        test_util::make_timestamp(7002),
        test_util::make_instr(/*pc=*/501),
        // C asks to go unscheduled with no timeout, but a prior _SCHEDULE
        // means it just continues.
        test_util::make_timestamp(7004),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE, 0),
        test_util::make_timestamp(7008),
        test_util::make_instr(/*pc=*/502),
        test_util::make_exit(TID_C),
    };
    {
        // Test the defaults with direct switches enabled.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_B)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_B);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_C)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_C);
        // The string constructor writes "." for markers.
        // Matching the comments above, we expect A to go unscheduled;
        // Then B runs and goes unscheduled-with-timeout; C takes over and blocks.
        // We then have _=idle confirming A is unscheduled and B blocked.
        // B then runs and makes A schedulable before going unscheduled.
        // A then runs and switches back to B with a timeout.  B exits; A's timeout
        // has lapsed so it runs; finally we wait idle for C's long block to finish,
        // after which C runs and *does not unschedule* b/c of B's prior request.
        static const char *const CORE0_SCHED_STRING =
            "...AA.........B........CC.....________B......B......B....A......BBBB.______"
            "A._________________C......C.";

        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        sched_ops.quantum_duration_us = QUANTUM_DURATION;
        // We use our mock's time==instruction count for a deterministic result.
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.time_units_per_us = 1.;
        sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
        sched_ops.block_time_multiplier = BLOCK_SCALE;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
    }
    {
        // Test disabling direct switches.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_B)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_B);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_C)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_C);
        // The syscall latencies make this schedule not all that different: we just
        // finish B instead of switching to A toward the end.
        static const char *const CORE0_SCHED_STRING =
            "...AA.........B........CC.....__________________B......B......B....BBBB.____"
            "A......__A._______C......C.";

        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        sched_ops.quantum_duration_us = QUANTUM_DURATION;
        // We use our mock's time==instruction count for a deterministic result.
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.time_units_per_us = 1.;
        sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
        sched_ops.block_time_multiplier = BLOCK_SCALE;
        sched_ops.honor_direct_switches = false;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
    }
}

static void
test_unscheduled_fallback()
{
    std::cerr << "\n----------------\nTesting unscheduled hang workarounds\n";
    // We have just 1 output to better control the order and avoid flakiness.
    static constexpr int NUM_OUTPUTS = 1;
    static constexpr int QUANTUM_DURATION = 100; // Never reached.
    static constexpr int BLOCK_LATENCY = 100;
    static constexpr double BLOCK_SCALE = 1. / (BLOCK_LATENCY);
    static constexpr uint64_t BLOCK_TIME_MAX = 500;
    static constexpr int SWITCH_TIMEOUT = 2000;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr memref_tid_t TID_A = TID_BASE + 0;
    static constexpr memref_tid_t TID_B = TID_BASE + 1;
    static constexpr memref_tid_t TID_C = TID_BASE + 2;
    std::vector<trace_entry_t> refs_A = {
        test_util::make_thread(TID_A),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        // A has the earliest timestamp and starts.
        test_util::make_timestamp(1001),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_instr(/*pc=*/101),
        test_util::make_instr(/*pc=*/102),
        test_util::make_timestamp(1002),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        // Test going unscheduled with no timeout.
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE, 0),
        test_util::make_timestamp(4202),
        // B makes us scheduled again.
        test_util::make_instr(/*pc=*/102),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        // Switch to a missing thread to leave us unscheduled; B also went
        // unscheduled, leaving nothing scheduled, to test hang workarounds.
        test_util::make_marker(TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH, TID_BASE + 4),
        test_util::make_timestamp(4402),
        // We won't get here until the no-scheduled-input hang workaround.
        test_util::make_instr(/*pc=*/401),
        test_util::make_exit(TID_A),
    };
    std::vector<trace_entry_t> refs_B = {
        test_util::make_thread(TID_B),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        // B runs next by timestamp.
        test_util::make_timestamp(2001),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_instr(/*pc=*/200),
        // B goes unscheduled with a timeout.
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_ARG_TIMEOUT, SWITCH_TIMEOUT),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE, 0),
        // C will run at this point.
        // Then, C blocks and our timeout lapses and we run again.
        test_util::make_timestamp(4001),
        test_util::make_instr(/*pc=*/201),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        // B makes A no longer unscheduled.
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_ARG_TIMEOUT, SWITCH_TIMEOUT),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_SCHEDULE, TID_A),
        test_util::make_timestamp(4011),
        test_util::make_instr(/*pc=*/202),
        // B now goes unscheduled with no timeout.
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE, 0),
        // We won't get here until the hang workaround.
        test_util::make_instr(/*pc=*/203),
        test_util::make_instr(/*pc=*/204),
        test_util::make_instr(/*pc=*/205),
        test_util::make_instr(/*pc=*/206),
        test_util::make_exit(TID_B),
    };
    std::vector<trace_entry_t> refs_C = {
        test_util::make_thread(TID_C),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        // C goes 3rd by timestamp.
        test_util::make_timestamp(3001),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_instr(/*pc=*/301),
        test_util::make_instr(/*pc=*/302),
        test_util::make_timestamp(3002),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        // C makes a long-latency blocking syscall, testing whether
        // A is still unscheduled.
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        test_util::make_timestamp(7002),
        test_util::make_instr(/*pc=*/501),
        test_util::make_exit(TID_C),
    };
    {
        // Test with direct switches enabled and infinite timeouts.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_B)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_B);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_C)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_C);
        // This looks like the schedule in test_unscheduled() up until "..A.." when
        // we have an idle period equal to the rebalance_period from the start
        // (so BLOCK_TIME_MAX minus what was run).
        static const char *const CORE0_SCHED_STRING =
            "...AA.........B........CC.....__________________B......B....A.....__________"
            "_________C._________________________________________________________________"
            "____________________________________________________________________________"
            "____________________________________________________________________________"
            "____________________________________________________________________________"
            "____________________________________________________________________________"
            "____________________________________________________________________________"
            "___________BBBB.A.";

        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        sched_ops.quantum_duration_us = QUANTUM_DURATION;
        // We use our mock's time==instruction count for a deterministic result.
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.time_units_per_us = 1.;
        sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
        sched_ops.block_time_multiplier = BLOCK_SCALE;
        sched_ops.block_time_max_us = BLOCK_TIME_MAX;
        sched_ops.rebalance_period_us = BLOCK_TIME_MAX;
        sched_ops.honor_infinite_timeouts = true;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
    }
    {
        // Test disabling infinite timeouts.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_B)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_B);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_C)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_C);
        // Here we see much shorter idle time before A and B finish.
        static const char *const CORE0_SCHED_STRING =
            "...AA.........B........CC.....__A....._____A._________B......B...._____BBBB."
            "___________C.";

        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        sched_ops.quantum_duration_us = QUANTUM_DURATION;
        // We use our mock's time==instruction count for a deterministic result.
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.time_units_per_us = 1.;
        sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
        sched_ops.block_time_multiplier = BLOCK_SCALE;
        sched_ops.block_time_max_us = BLOCK_TIME_MAX;
        sched_ops.rebalance_period_us = BLOCK_TIME_MAX;
        sched_ops.honor_infinite_timeouts = false;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
    }
    {
        // Test disabling direct switches.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_B)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_B);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_C)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_C);
        // This result is identical to the one in test_unscheduled().
        static const char *const CORE0_SCHED_STRING =
            "...AA.........B........CC.....__________________B......B....BBBB._____A....."
            "__A._______C.";

        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        sched_ops.quantum_duration_us = QUANTUM_DURATION;
        // We use our mock's time==instruction count for a deterministic result.
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.time_units_per_us = 1.;
        sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
        sched_ops.block_time_multiplier = BLOCK_SCALE;
        sched_ops.block_time_max_us = BLOCK_TIME_MAX;
        sched_ops.rebalance_period_us = BLOCK_TIME_MAX;
        sched_ops.honor_direct_switches = false;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
    }
}

static void
test_unscheduled_initially()
{
    std::cerr << "\n----------------\nTesting initially-unscheduled threads\n";
    static constexpr int NUM_OUTPUTS = 1;
    static constexpr int BLOCK_LATENCY = 100;
    static constexpr double BLOCK_SCALE = 1. / (BLOCK_LATENCY);
    static constexpr uint64_t BLOCK_TIME_MAX = 500;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr memref_tid_t TID_A = TID_BASE + 0;
    static constexpr memref_tid_t TID_B = TID_BASE + 1;
    std::vector<trace_entry_t> refs_A = {
        test_util::make_thread(TID_A),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        // A has the earliest timestamp and would start.
        test_util::make_timestamp(1001),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        // A starts out unscheduled though.
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE, 0),
        test_util::make_timestamp(4202),
        // B makes us scheduled again.
        test_util::make_instr(/*pc=*/102),
        test_util::make_exit(TID_A),
    };
    std::vector<trace_entry_t> refs_B = {
        test_util::make_thread(TID_B),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        // B runs 2nd by timestamp.
        test_util::make_timestamp(3001),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_instr(/*pc=*/200),
        test_util::make_timestamp(3002),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        // B makes a long-latency blocking syscall, testing whether
        // A is really unscheduled.
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        test_util::make_timestamp(7002),
        test_util::make_instr(/*pc=*/201),
        // B makes A no longer unscheduled.
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_SCHEDULE, TID_A),
        test_util::make_timestamp(7021),
        test_util::make_instr(/*pc=*/202),
        test_util::make_exit(TID_B),
    };
    {
        // Test with infinite timeouts and direct switches enabled.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_B)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_B);
        // We have an idle period while B is blocked and A unscheduled.
        static const char *const CORE0_SCHED_STRING =
            "...B.....________________________________________B....B......A.";

        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.time_units_per_us = 1.;
        sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
        sched_ops.block_time_multiplier = BLOCK_SCALE;
        sched_ops.block_time_max_us = BLOCK_TIME_MAX;
        sched_ops.honor_infinite_timeouts = true;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
    }
    {
        // Test without infinite timeouts.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_B)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_B);
        // We have a medium idle period before A becomes schedulable.
        static const char *const CORE0_SCHED_STRING =
            "...B....._____.....A.__________________________________B....B.";

        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.time_units_per_us = 1.;
        sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
        sched_ops.block_time_multiplier = BLOCK_SCALE;
        sched_ops.block_time_max_us = BLOCK_TIME_MAX;
        sched_ops.honor_infinite_timeouts = false;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
    }
    {
        // Test disabling direct switches.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_B)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_B);
        // A runs first as it being unscheduled is ignored.
        static const char *const CORE0_SCHED_STRING =
            ".....A....B.....________________________________________B....B.";

        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        // We use our mock's time==instruction count for a deterministic result.
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.time_units_per_us = 1.;
        sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
        sched_ops.block_time_multiplier = BLOCK_SCALE;
        sched_ops.block_time_max_us = BLOCK_TIME_MAX;
        sched_ops.honor_direct_switches = false;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
    }
}

static void
test_unscheduled_initially_roi()
{
#ifdef HAS_ZIP
    std::cerr
        << "\n----------------\nTesting initially-unscheduled + time deps with ROI\n";
    static constexpr int NUM_OUTPUTS = 1;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr memref_tid_t TID_A = TID_BASE + 0;
    static constexpr memref_tid_t TID_B = TID_BASE + 1;
    std::vector<trace_entry_t> refs_A = {
        test_util::make_thread(TID_A),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_timestamp(1001),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        // A starts out unscheduled but we skip that.
        // (In a real trace some other thread would have to wake up A:
        // we omit that here to keep the test small.)
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE, 0),
        test_util::make_timestamp(4202),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_instr(/*pc=*/101),
        // We don't actually start until here.
        test_util::make_instr(/*pc=*/102),
        test_util::make_instr(/*pc=*/103),
        test_util::make_exit(TID_A),
    };
    std::vector<trace_entry_t> refs_B = {
        test_util::make_thread(TID_B),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_timestamp(3001),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_instr(/*pc=*/201),
        test_util::make_timestamp(4001),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_instr(/*pc=*/202),
        // B starts here, with a lower last timestamp than A.
        test_util::make_instr(/*pc=*/203),
        test_util::make_instr(/*pc=*/204),
        test_util::make_exit(TID_B),
    };
    // Instr counts are 1-based.
    std::vector<scheduler_t::range_t> regions_A;
    regions_A.emplace_back(2, 0);
    std::vector<scheduler_t::range_t> regions_B;
    regions_B.emplace_back(3, 0);
    // B should run first due to the lower timestamp at its ROI despite A's
    // start-of-trace timestamp being lower.
    static const char *const CORE0_SCHED_STRING = "..BB...AA.";

    std::string record_fname = "tmp_test_unsched_ROI.zip";
    {
        // Record.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_B)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_B);
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        sched_inputs.back().thread_modifiers.push_back(
            scheduler_t::input_thread_info_t(TID_A, regions_A));
        sched_inputs.back().thread_modifiers.push_back(
            scheduler_t::input_thread_info_t(TID_B, regions_B));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/4);
        zipfile_ostream_t outfile(record_fname);
        sched_ops.schedule_record_ostream = &outfile;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
        if (scheduler.write_recorded_schedule() != scheduler_t::STATUS_SUCCESS)
            assert(false);
    }
    {
        replay_file_checker_t checker;
        zipfile_istream_t infile(record_fname);
        std::string res = checker.check(&infile);
        if (!res.empty())
            std::cerr << "replay file checker failed: " << res;
        assert(res.empty());
    }
    {
        // Test replay as it has complexities with skip records.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_B)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_B);
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        // The regions are ignored on replay so we do not specify them.
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_AS_PREVIOUSLY,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/4);
        zipfile_istream_t infile(record_fname);
        sched_ops.schedule_replay_istream = &infile;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
    }
#endif
}

static void
test_unscheduled_initially_rebalance()
{
    // Tests i#7318 where on a rebalance attempt a too-large runqueue has nothing
    // but blocked inputs. That's easiest to make happen at the init-time
    // rebalance where we create a bunch of starts-unscheduled (but not infinite)
    // inputs.
    std::cerr << "\n----------------\nTesting initially-unscheduled init rebalance\n";
    static constexpr int NUM_OUTPUTS = 3;
    static constexpr int NUM_INPUTS = 5;
    static constexpr int BLOCK_LATENCY = 100;
    static constexpr double BLOCK_SCALE = 1. / (BLOCK_LATENCY);
    static constexpr uint64_t BLOCK_TIME_MAX = 500;
    static constexpr int MIGRATION_THRESHOLD = 0;
    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<trace_entry_t> refs[NUM_INPUTS];
    for (int i = 0; i < NUM_INPUTS; ++i) {
        if (i == 0) {
            // Just one input is runnable.
            refs[i] = {
                test_util::make_thread(TID_BASE + i),
                test_util::make_pid(1),
                test_util::make_version(TRACE_ENTRY_VERSION),
                // Runs last by timestamp.
                test_util::make_timestamp(3001),
                test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
                test_util::make_instr(/*pc=*/200),
                test_util::make_timestamp(3002),
                test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
                // Makes a long-latency blocking syscall, testing whether
                // the other threads are really unscheduled.
                test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
                test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
                test_util::make_timestamp(7002),
                test_util::make_instr(/*pc=*/201),
                test_util::make_exit(TID_BASE + i),
            };
        } else {
            // The rest start unscheduled.
            refs[i] = {
                test_util::make_thread(TID_BASE + i),
                test_util::make_pid(1),
                test_util::make_version(TRACE_ENTRY_VERSION),
                // These have the earliest timestamp and would start.
                test_util::make_timestamp(1001 + i),
                test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
                // They start out unscheduled though.  We don't set
                // honor_infinite_timeouts so this will eventually run.
                test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE, 0),
                test_util::make_timestamp(4202),
                test_util::make_instr(/*pc=*/102),
                test_util::make_exit(TID_BASE + i),
            };
        }
    }
    std::vector<scheduler_t::input_reader_t> readers;
    for (int i = 0; i < NUM_INPUTS; ++i) {
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs[i])),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_BASE + i);
    }
    // We need the initial runqueue assignment to be unbalanced.
    // We achieve that by using input bindings.
    // This relies on knowing the scheduler takes the 1st binding if there
    // are any if the bindings don't include all cores: so we can set to all-but-one
    // core and these will all pile up on output #0
    // prior to the init-time rebalance.  That makes output
    // #0 big enough for a rebalance attempt, which causes scheduler init to fail
    // without the i#7318 fix as it can only move one of those blocked inputs and
    // so it hits an IDLE status on a later move attempt.
    std::set<scheduler_t::output_ordinal_t> cores;
    cores.insert({ 0, 1 });
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    sched_inputs.back().thread_modifiers.emplace_back(cores);
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/3);
    sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
    sched_ops.time_units_per_us = 1.;
    sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
    sched_ops.block_time_multiplier = BLOCK_SCALE;
    sched_ops.block_time_max_us = BLOCK_TIME_MAX;
    sched_ops.honor_infinite_timeouts = false;
    sched_ops.migration_threshold_us = MIGRATION_THRESHOLD;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    // Our live thread A blocks and then the others become unblocked.
    // Because they were blocked, the init-time rebalance couldn't steal
    // any of them, and the duration here is too short for another rebalance,
    // so the other cores remain idle.
    static const char *const CORE0_SCHED_STRING =
        "...A.....__.....B......C......D......E.A.";
    static const char *const CORE1_SCHED_STRING =
        "_________________________________________";
    static const char *const CORE2_SCHED_STRING =
        "_________________________________________";
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    assert(sched_as_string[0] == CORE0_SCHED_STRING);
    assert(sched_as_string[1] == CORE1_SCHED_STRING);
    assert(sched_as_string[2] == CORE2_SCHED_STRING);
}

static void
test_unscheduled_small_timeout()
{
    // Test that a small timeout scaled to 0 does not turn into an infinite timeout.
    std::cerr << "\n----------------\nTesting unscheduled input with small timeout\n";
    static constexpr int NUM_OUTPUTS = 1;
    // 4*0.1 rounds to 0 (the scheduler's cast rounds any fraction down).
    static constexpr int UNSCHEDULE_TIMEOUT = 4;
    static constexpr double BLOCK_SCALE = 0.1;
    static constexpr memref_tid_t TID_A = 100;
    std::vector<trace_entry_t> refs_A = {
        test_util::make_thread(TID_A),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_timestamp(1001),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_instr(/*pc=*/101),
        test_util::make_timestamp(1002),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_ARG_TIMEOUT, UNSCHEDULE_TIMEOUT),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE, 0),
        test_util::make_timestamp(2002),
        test_util::make_instr(/*pc=*/102),
        test_util::make_exit(TID_A),
    };
    {
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        static const char *const CORE0_SCHED_STRING = "...A......._A.";

        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        // We use our mock's time==instruction count for a deterministic result.
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.time_units_per_us = 1.;
        sched_ops.block_time_multiplier = BLOCK_SCALE;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_A, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
    }
}

static void
test_unscheduled_no_alternative()
{
    // Test that an unscheduled 0-timeout input is not incorrectly executed if
    // there is nothing else to run (i#6959).
    std::cerr << "\n----------------\nTesting unscheduled no alternative (i#6959)\n";
    static constexpr int NUM_OUTPUTS = 1;
    static constexpr uint64_t REBALANCE_PERIOD_US = 50;
    static constexpr uint64_t BLOCK_TIME_MAX = 200;
    static constexpr memref_tid_t TID_A = 100;
    std::vector<trace_entry_t> refs_A = {
        test_util::make_thread(TID_A),
        test_util::make_pid(1),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_timestamp(1001),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_instr(/*pc=*/101),
        test_util::make_timestamp(1002),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        // No timeout means infinite (until the fallback kicks in).
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE, 0),
        test_util::make_timestamp(2002),
        test_util::make_instr(/*pc=*/102),
        test_util::make_exit(TID_A),
    };
    {
        // Test infinite timeouts.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        static const char *const CORE0_SCHED_STRING =
            "...A......__________________________________________________A.";

        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        // We use our mock's time==instruction count for a deterministic result.
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.time_units_per_us = 1.;
        sched_ops.rebalance_period_us = REBALANCE_PERIOD_US;
        sched_ops.block_time_max_us = BLOCK_TIME_MAX;
        sched_ops.honor_infinite_timeouts = true;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_A, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
    }
    {
        // Test finite timeouts.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_A)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_A);
        static const char *const CORE0_SCHED_STRING = "...A......____________________A.";

        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        // We use our mock's time==instruction count for a deterministic result.
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.time_units_per_us = 1.;
        sched_ops.rebalance_period_us = REBALANCE_PERIOD_US;
        sched_ops.block_time_max_us = BLOCK_TIME_MAX;
        sched_ops.honor_infinite_timeouts = false;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_A, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
    }
}

static void
test_unscheduled()
{
    test_unscheduled_base();
    test_unscheduled_fallback();
    test_unscheduled_initially();
    test_unscheduled_initially_roi();
    test_unscheduled_initially_rebalance();
    test_unscheduled_small_timeout();
    test_unscheduled_no_alternative();
}

static std::vector<std::string>
run_lockstep_simulation_for_kernel_seq(scheduler_t &scheduler, int num_outputs,
                                       memref_tid_t tid_base, int syscall_base,
                                       std::vector<std::vector<memref_t>> &refs,
                                       bool for_syscall_seq)
{
    // We have a custom version of run_lockstep_simulation here for more precise
    // testing of the markers and instructions and interfaces.
    // We record the entire sequence for a detailed check of some records, along with
    // a character representation for a higher-level view of the whole sequence.
    std::vector<scheduler_t::stream_t *> outputs(num_outputs, nullptr);
    std::vector<bool> eof(num_outputs, false);
    for (int i = 0; i < num_outputs; i++)
        outputs[i] = scheduler.get_stream(i);
    int num_eof = 0;
    refs.resize(num_outputs);
    std::vector<std::string> sched_as_string(num_outputs);
    std::vector<memref_tid_t> prev_tid(num_outputs, INVALID_THREAD_ID);
    std::vector<bool> in_switch(num_outputs, false);
    std::vector<bool> in_syscall(num_outputs, false);
    std::vector<uint64> prev_in_ord(num_outputs, 0);
    std::vector<uint64> prev_out_ord(num_outputs, 0);
    while (num_eof < num_outputs) {
        for (int i = 0; i < num_outputs; i++) {
            if (eof[i])
                continue;
            if (for_syscall_seq) {
                // Ensure that the stream returns the correct value. The marker value is
                // recorded in refs and will be checked separately.
                assert(TESTANY(OFFLINE_FILE_TYPE_KERNEL_SYSCALLS,
                               outputs[i]->get_filetype()));
            }
            memref_t memref;
            scheduler_t::stream_status_t status = outputs[i]->next_record(memref);
            if (status == scheduler_t::STATUS_EOF) {
                ++num_eof;
                eof[i] = true;
                continue;
            }
            if (status == scheduler_t::STATUS_IDLE) {
                sched_as_string[i] += '_';
                continue;
            }
            assert(status == scheduler_t::STATUS_OK);
            // Ensure stream API and the trace records are consistent.
            assert(outputs[i]->get_input_interface()->get_tid() == IDLE_THREAD_ID ||
                   outputs[i]->get_input_interface()->get_tid() ==
                       tid_from_memref_tid(memref.instr.tid));
            assert(outputs[i]->get_input_interface()->get_workload_id() == INVALID_PID ||
                   outputs[i]->get_input_interface()->get_workload_id() ==
                       workload_from_memref_tid(memref.instr.tid));
            refs[i].push_back(memref);
            if (tid_from_memref_tid(memref.instr.tid) != prev_tid[i]) {
                if (!sched_as_string[i].empty())
                    sched_as_string[i] += ',';
                sched_as_string[i] += 'A' +
                    static_cast<char>(tid_from_memref_tid(memref.instr.tid) - tid_base);
            }
            if (memref.marker.type == TRACE_TYPE_MARKER) {
                if (memref.marker.marker_type == TRACE_MARKER_TYPE_CONTEXT_SWITCH_START)
                    in_switch[i] = true;
                else if (memref.marker.marker_type ==
                         TRACE_MARKER_TYPE_SYSCALL_TRACE_START)
                    in_syscall[i] = true;
            }
            if (in_switch[i]) {
                // Test that switch code is marked synthetic.
                assert(outputs[i]->is_record_synthetic());
                // Test that it's marked as kernel, unless it's the end marker.
                assert(
                    outputs[i]->is_record_kernel() ||
                    (memref.marker.type == TRACE_TYPE_MARKER &&
                     memref.marker.marker_type == TRACE_MARKER_TYPE_CONTEXT_SWITCH_END));
                // Test that switch code doesn't count toward input ordinals, but
                // does toward output ordinals.
                assert(outputs[i]->get_input_interface()->get_record_ordinal() ==
                           prev_in_ord[i] ||
                       // Won't match if we just switched inputs.
                       (memref.marker.type == TRACE_TYPE_MARKER &&
                        memref.marker.marker_type ==
                            TRACE_MARKER_TYPE_CONTEXT_SWITCH_START));
                assert(outputs[i]->get_record_ordinal() > prev_out_ord[i]);
            } else if (in_syscall[i]) {
                bool is_trace_start = memref.marker.type == TRACE_TYPE_MARKER &&
                    memref.marker.marker_type == TRACE_MARKER_TYPE_SYSCALL_TRACE_START;
                bool is_trace_end = memref.marker.type == TRACE_TYPE_MARKER &&
                    memref.marker.marker_type == TRACE_MARKER_TYPE_SYSCALL_TRACE_END;
                // Test that syscall code is marked synthetic.
                assert(outputs[i]->is_record_synthetic());
                // Test that it's marked as kernel, unless it's the end marker.
                assert(outputs[i]->is_record_kernel() || is_trace_end);
                // Test that dynamically injected syscall code doesn't count toward
                // input ordinals, but does toward output ordinals.
                assert(outputs[i]->get_input_interface()->get_record_ordinal() ==
                           prev_in_ord[i] ||
                       // We readahead by one record to decide when to inject the
                       // syscall trace, so the input interface record ordinal will
                       // be advanced by one at trace start.
                       is_trace_start);
                assert(outputs[i]->get_record_ordinal() > prev_out_ord[i]);
            } else
                assert(!outputs[i]->is_record_synthetic());
            if (type_is_instr(memref.instr.type))
                sched_as_string[i] += 'i';
            else if (memref.marker.type == TRACE_TYPE_MARKER) {
                switch (memref.marker.marker_type) {
                case TRACE_MARKER_TYPE_VERSION: sched_as_string[i] += 'v'; break;
                case TRACE_MARKER_TYPE_FILETYPE: sched_as_string[i] += 'f'; break;
                case TRACE_MARKER_TYPE_TIMESTAMP: sched_as_string[i] += '0'; break;
                case TRACE_MARKER_TYPE_CONTEXT_SWITCH_END:
                    in_switch[i] = false;
                    ANNOTATE_FALLTHROUGH;
                case TRACE_MARKER_TYPE_CONTEXT_SWITCH_START:
                    if (memref.marker.marker_value == scheduler_t::SWITCH_PROCESS)
                        sched_as_string[i] += 'p';
                    else if (memref.marker.marker_value == scheduler_t::SWITCH_THREAD)
                        sched_as_string[i] += 't';
                    else
                        assert(false && "unknown context switch type");
                    break;
                case TRACE_MARKER_TYPE_FUNC_ID:
                case TRACE_MARKER_TYPE_FUNC_ARG:
                case TRACE_MARKER_TYPE_FUNC_RETVAL: sched_as_string[i] += 'F'; break;
                case TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL:
                case TRACE_MARKER_TYPE_SYSCALL_FAILED: sched_as_string[i] += 's'; break;
                case TRACE_MARKER_TYPE_SYSCALL: sched_as_string[i] += 'S'; break;
                case TRACE_MARKER_TYPE_SYSCALL_TRACE_END:
                    in_syscall[i] = false;
                    ANNOTATE_FALLTHROUGH;
                case TRACE_MARKER_TYPE_SYSCALL_TRACE_START:
                    sched_as_string[i] += '1' +
                        static_cast<char>(memref.marker.marker_value - syscall_base);
                    break;
                case TRACE_MARKER_TYPE_KERNEL_EVENT:
                case TRACE_MARKER_TYPE_KERNEL_XFER: sched_as_string[i] += 'k'; break;
                default: sched_as_string[i] += '?'; break;
                }
                // A context switch should happen only at the context_switch_start marker.
                // TODO i#7495: Add invariant checks that ensure this property for
                // core-sharded-on-disk traces. This would need moving the synthetic
                // tid-pid markers before the injected switch trace.
                if (memref.marker.marker_type == TRACE_MARKER_TYPE_CONTEXT_SWITCH_START) {
                    assert(prev_tid[i] != tid_from_memref_tid(memref.instr.tid));
                } else {
                    assert(for_syscall_seq || prev_tid[i] == INVALID_THREAD_ID ||
                           prev_tid[i] == tid_from_memref_tid(memref.instr.tid));
                }
            }
            prev_tid[i] = tid_from_memref_tid(memref.instr.tid);
            prev_in_ord[i] = outputs[i]->get_input_interface()->get_record_ordinal();
            prev_out_ord[i] = outputs[i]->get_record_ordinal();
        }
    }
    return sched_as_string;
}

static void
test_kernel_switch_sequences()
{
    std::cerr << "\n----------------\nTesting kernel switch sequences\n";
    static constexpr memref_tid_t TID_IN_SWITCHES = 1;
    static constexpr addr_t PROCESS_SWITCH_PC_START = 0xfeed101;
    static constexpr addr_t THREAD_SWITCH_PC_START = 0xcafe101;
    static constexpr uint64_t PROCESS_SWITCH_TIMESTAMP = 12345678;
    static constexpr uint64_t THREAD_SWITCH_TIMESTAMP = 87654321;
    std::vector<trace_entry_t> switch_sequence = {
        /* clang-format off */
        test_util::make_header(TRACE_ENTRY_VERSION),
        test_util::make_thread(TID_IN_SWITCHES),
        test_util::make_pid(TID_IN_SWITCHES),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_timestamp(PROCESS_SWITCH_TIMESTAMP),
        test_util::make_marker(
            TRACE_MARKER_TYPE_CONTEXT_SWITCH_START, scheduler_t::SWITCH_PROCESS),
        test_util::make_instr(PROCESS_SWITCH_PC_START),
        test_util::make_instr(PROCESS_SWITCH_PC_START + 1),
        test_util::make_marker(
            TRACE_MARKER_TYPE_CONTEXT_SWITCH_END, scheduler_t::SWITCH_PROCESS),
        test_util::make_exit(TID_IN_SWITCHES),
        test_util::make_footer(),
        // Test a complete trace after the first one, which is how we plan to store
        // these in an archive file.
        test_util::make_header(TRACE_ENTRY_VERSION),
        test_util::make_thread(TID_IN_SWITCHES),
        test_util::make_pid(TID_IN_SWITCHES),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_timestamp(THREAD_SWITCH_TIMESTAMP),
        test_util::make_marker(
            TRACE_MARKER_TYPE_CONTEXT_SWITCH_START, scheduler_t::SWITCH_THREAD),
        test_util::make_instr(THREAD_SWITCH_PC_START),
        test_util::make_instr(THREAD_SWITCH_PC_START+1),
        test_util::make_marker(
            TRACE_MARKER_TYPE_CONTEXT_SWITCH_END, scheduler_t::SWITCH_THREAD),
        test_util::make_exit(TID_IN_SWITCHES),
        test_util::make_footer(),
        /* clang-format on */
    };
    static constexpr int NUM_WORKLOADS = 3;
    static constexpr int NUM_INPUTS_PER_WORKLOAD = 3;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int NUM_INSTRS = 9;
    static constexpr int INSTR_QUANTUM = 3;
    static constexpr uint64_t TIMESTAMP = 44226688;
    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    std::vector<record_scheduler_t::input_workload_t> sched_record_inputs;

    for (int workload_idx = 0; workload_idx < NUM_WORKLOADS; workload_idx++) {
        std::vector<scheduler_t::input_reader_t> readers;
        std::vector<record_scheduler_t::input_reader_t> record_readers;
        for (int input_idx = 0; input_idx < NUM_INPUTS_PER_WORKLOAD; input_idx++) {
            std::vector<trace_entry_t> inputs;
            inputs.push_back(test_util::make_header(TRACE_ENTRY_VERSION));
            memref_tid_t tid =
                TID_BASE + workload_idx * NUM_INPUTS_PER_WORKLOAD + input_idx;
            inputs.push_back(test_util::make_thread(tid));
            inputs.push_back(test_util::make_pid(1));
            inputs.push_back(test_util::make_version(TRACE_ENTRY_VERSION));
            inputs.push_back(test_util::make_timestamp(TIMESTAMP));
            for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
                inputs.push_back(test_util::make_instr(42 + instr_idx * 4));
            }
            inputs.push_back(test_util::make_exit(tid));

            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs)),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                tid);
            record_readers.emplace_back(std::unique_ptr<test_util::mock_record_reader_t>(
                                            new test_util::mock_record_reader_t(inputs)),
                                        std::unique_ptr<test_util::mock_record_reader_t>(
                                            new test_util::mock_record_reader_t()),
                                        tid);
        }
        sched_inputs.emplace_back(std::move(readers));
        sched_record_inputs.emplace_back(std::move(record_readers));
    }
    {
        auto switch_reader = std::unique_ptr<test_util::mock_reader_t>(
            new test_util::mock_reader_t(switch_sequence));
        auto switch_reader_end =
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t());
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        sched_ops.quantum_duration_instrs = INSTR_QUANTUM;
        sched_ops.kernel_switch_reader = std::move(switch_reader);
        sched_ops.kernel_switch_reader_end = std::move(switch_reader_end);
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);

        std::vector<std::vector<memref_t>> refs;
        std::vector<std::string> sched_as_string = run_lockstep_simulation_for_kernel_seq(
            scheduler, NUM_OUTPUTS, TID_BASE, 0, refs,
            /*for_syscall_seq=*/false);
        // Check the high-level strings.
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] ==
               "Av0iii,Ctiitv0iii,Epiipv0iii,Gpiipv0iii,Itiitv0iii,Apiipiii,Ctiitiii,"
               "Epiipiii,Gpiipiii,Itiitiii,Apiipiii,Ctiitiii,Epiipiii,Gpiipiii,"
               "Itiitiii");
        assert(sched_as_string[1] ==
               "Bv0iii,Dpiipv0iii,Ftiitv0iii,Hpiipv0iii,Bpiipiii,Dpiipiii,Ftiitiii,"
               "Hpiipiii,Bpiipiii,Dpiipiii,Ftiitiii,Hpiipiii________________________");
        // Zoom in and check the first sequence record by record with value checks.
        int idx = 0;
        bool res = true;
        memref_tid_t workload1_tid1_final =
            (1ULL << MEMREF_ID_WORKLOAD_SHIFT) | (TID_BASE + 4);
        res = res &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_VERSION) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_INSTR) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_INSTR) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_INSTR) &&
            // Thread switch.
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_CONTEXT_SWITCH_START,
                      scheduler_t::SWITCH_THREAD) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_INSTR) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_INSTR) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_CONTEXT_SWITCH_END, scheduler_t::SWITCH_THREAD) &&
            // We now see the headers for this thread.
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_VERSION) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP) &&
            // The 3-instr quantum should not count the 2 switch instrs.
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_INSTR) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_INSTR) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_INSTR) &&
            // Process switch.
            check_ref(refs[0], idx, workload1_tid1_final, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_CONTEXT_SWITCH_START,
                      scheduler_t::SWITCH_PROCESS) &&
            check_ref(refs[0], idx, workload1_tid1_final, TRACE_TYPE_INSTR) &&
            check_ref(refs[0], idx, workload1_tid1_final, TRACE_TYPE_INSTR) &&
            check_ref(refs[0], idx, workload1_tid1_final, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_CONTEXT_SWITCH_END,
                      scheduler_t::SWITCH_PROCESS) &&
            // We now see the headers for this thread.
            check_ref(refs[0], idx, workload1_tid1_final, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_VERSION) &&
            check_ref(refs[0], idx, workload1_tid1_final, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP) &&
            // The 3-instr quantum should not count the 2 switch instrs.
            check_ref(refs[0], idx, workload1_tid1_final, TRACE_TYPE_INSTR) &&
            check_ref(refs[0], idx, workload1_tid1_final, TRACE_TYPE_INSTR) &&
            check_ref(refs[0], idx, workload1_tid1_final, TRACE_TYPE_INSTR);
        assert(res);
    }
    {
        auto switch_reader = std::unique_ptr<test_util::mock_record_reader_t>(
            new test_util::mock_record_reader_t(switch_sequence));
        auto switch_reader_end = std::unique_ptr<test_util::mock_record_reader_t>(
            new test_util::mock_record_reader_t());
        record_scheduler_t scheduler;

        record_scheduler_t::scheduler_options_t sched_ops(
            record_scheduler_t::MAP_TO_ANY_OUTPUT,
            record_scheduler_t::DEPENDENCY_TIMESTAMPS,
            record_scheduler_t::SCHEDULER_DEFAULTS,
            /*verbosity=*/3);
        sched_ops.quantum_duration_instrs = INSTR_QUANTUM;
        sched_ops.kernel_switch_reader = std::move(switch_reader);
        sched_ops.kernel_switch_reader_end = std::move(switch_reader_end);
        if (scheduler.init(sched_record_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            record_scheduler_t::STATUS_SUCCESS)
            assert(false);
        auto *stream0 = scheduler.get_stream(0);
        auto check_next = [](record_scheduler_t::stream_t *stream,
                             record_scheduler_t::stream_status_t expect_status,
                             trace_type_t expect_type = TRACE_TYPE_MARKER,
                             addr_t expect_addr = 0, addr_t expect_size = 0) {
            trace_entry_t record;
            record_scheduler_t::stream_status_t status = stream->next_record(record);
            assert(status == expect_status);
            if (status == record_scheduler_t::STATUS_OK) {
                if (record.type != expect_type) {
                    std::cerr << "Expected type " << expect_type << " != " << record.type
                              << "\n";
                    assert(false);
                }
                if (expect_size != 0 && record.size != expect_size) {
                    std::cerr << "Expected size " << expect_size << " != " << record.size
                              << "\n";
                    assert(false);
                }
                if (expect_addr != 0 && record.addr != expect_addr) {
                    std::cerr << "Expected addr " << expect_addr << " != " << record.addr
                              << "\n";
                    assert(false);
                }
            }
        };

        // cpu0 at TID_BASE.
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_HEADER);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD, TID_BASE);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_PID, 1);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER, 0,
                   TRACE_MARKER_TYPE_VERSION);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER, 0,
                   TRACE_MARKER_TYPE_TIMESTAMP);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
        assert(stream0->get_instruction_ordinal() == 3);
        assert(stream0->get_input_interface()->get_instruction_ordinal() == 3);
        // The synthetic TRACE_TYPE_THREAD and TRACE_TYPE_PID for the new
        // input before the injected context switch trace. This allows identifying
        // the injected context switch sequence records with the new input's
        // tid/pid, like what the stream APIs do.
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD,
                   TID_BASE + 2);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_PID, 1);
        // Injected context switch sequence.
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER, 1,
                   TRACE_MARKER_TYPE_CONTEXT_SWITCH_START);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER, 1,
                   TRACE_MARKER_TYPE_CONTEXT_SWITCH_END);

        // cpu0 at TID_BASE+2.
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_HEADER);
        // Original tid-pid entries from the input.
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD,
                   TID_BASE + 2);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_PID, 1);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER, 0,
                   TRACE_MARKER_TYPE_VERSION);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER, 0,
                   TRACE_MARKER_TYPE_TIMESTAMP);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
        assert(stream0->get_instruction_ordinal() == 8);
        assert(stream0->get_input_interface()->get_instruction_ordinal() == 3);
        // Synthetic tid-pid records.
        check_next(
            stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD,
            static_cast<addr_t>((1ULL << MEMREF_ID_WORKLOAD_SHIFT) | (TID_BASE + 4)));
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_PID,
                   static_cast<addr_t>((1ULL << MEMREF_ID_WORKLOAD_SHIFT) | 1));
        // Injected context switch sequence.
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER, 2,
                   TRACE_MARKER_TYPE_CONTEXT_SWITCH_START);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER, 2,
                   TRACE_MARKER_TYPE_CONTEXT_SWITCH_END);
        // cpu0 at TID_BASE+4.
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_HEADER);
        // Original tid-pid records from the input.
        check_next(
            stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD,
            static_cast<addr_t>((1ULL << MEMREF_ID_WORKLOAD_SHIFT) | (TID_BASE + 4)));
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_PID,
                   static_cast<addr_t>((1ULL << MEMREF_ID_WORKLOAD_SHIFT) | 1));
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER, 0,
                   TRACE_MARKER_TYPE_VERSION);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER, 0,
                   TRACE_MARKER_TYPE_TIMESTAMP);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
        assert(stream0->get_instruction_ordinal() == 13);
        assert(stream0->get_input_interface()->get_instruction_ordinal() == 3);

        // Synthetic tid-pid records.
        check_next(
            stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD,
            static_cast<addr_t>((2ULL << MEMREF_ID_WORKLOAD_SHIFT) | (TID_BASE + 6)));
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_PID,
                   static_cast<addr_t>((2ULL << MEMREF_ID_WORKLOAD_SHIFT) | 1));
        // Injected context switch sequence.
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER, 2,
                   TRACE_MARKER_TYPE_CONTEXT_SWITCH_START);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER, 2,
                   TRACE_MARKER_TYPE_CONTEXT_SWITCH_END);
        // cpu0 at TID_BASE+6.
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_HEADER);
        // Original tid-pid records from the input.
        check_next(
            stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD,
            static_cast<addr_t>((2ULL << MEMREF_ID_WORKLOAD_SHIFT) | (TID_BASE + 6)));
        check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_PID,
                   static_cast<addr_t>((2ULL << MEMREF_ID_WORKLOAD_SHIFT) | 1));
    }

    {
        // Test a bad input sequence.
        std::vector<trace_entry_t> bad_switch_sequence = {
            /* clang-format off */
        test_util::make_header(TRACE_ENTRY_VERSION),
        test_util::make_thread(TID_IN_SWITCHES),
        test_util::make_pid(TID_IN_SWITCHES),
        test_util::make_marker(
            TRACE_MARKER_TYPE_CONTEXT_SWITCH_START, scheduler_t::SWITCH_PROCESS),
        test_util::make_instr(PROCESS_SWITCH_PC_START),
        test_util::make_marker(
            TRACE_MARKER_TYPE_CONTEXT_SWITCH_END, scheduler_t::SWITCH_PROCESS),
        test_util::make_footer(),
        test_util::make_header(TRACE_ENTRY_VERSION),
        test_util::make_thread(TID_IN_SWITCHES),
        test_util::make_pid(TID_IN_SWITCHES),
        // Error: duplicate type.
        test_util::make_marker(
            TRACE_MARKER_TYPE_CONTEXT_SWITCH_START, scheduler_t::SWITCH_PROCESS),
        test_util::make_instr(PROCESS_SWITCH_PC_START),
        test_util::make_marker(
            TRACE_MARKER_TYPE_CONTEXT_SWITCH_END, scheduler_t::SWITCH_PROCESS),
        test_util::make_footer(),
            /* clang-format on */
        };
        auto bad_switch_reader = std::unique_ptr<test_util::mock_reader_t>(
            new test_util::mock_reader_t(bad_switch_sequence));
        auto bad_switch_reader_end =
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t());
        std::vector<scheduler_t::input_workload_t> test_sched_inputs;
        std::vector<scheduler_t::input_reader_t> readers;
        std::vector<trace_entry_t> inputs;
        inputs.push_back(test_util::make_header(TRACE_ENTRY_VERSION));
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(inputs)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_BASE);
        test_sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t test_sched_ops(
            scheduler_t::MAP_TO_ANY_OUTPUT, scheduler_t::DEPENDENCY_TIMESTAMPS,
            scheduler_t::SCHEDULER_DEFAULTS);
        test_sched_ops.kernel_switch_reader = std::move(bad_switch_reader);
        test_sched_ops.kernel_switch_reader_end = std::move(bad_switch_reader_end);
        scheduler_t test_scheduler;
        if (test_scheduler.init(test_sched_inputs, NUM_OUTPUTS,
                                std::move(test_sched_ops)) !=
            scheduler_t::STATUS_ERROR_INVALID_PARAMETER)
            assert(false);
    }
}

static void
test_kernel_syscall_sequences()
{
    std::cerr << "\n----------------\nTesting kernel syscall sequences\n";
    static constexpr memref_tid_t TID_IN_SYSCALLS = 1;
    static constexpr int SYSCALL_BASE = 42;
    static constexpr addr_t SYSCALL_PC_START = 0xfeed101;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr offline_file_type_t FILE_TYPE =
        offline_file_type_t::OFFLINE_FILE_TYPE_SYSCALL_NUMBERS;
    {
        std::vector<trace_entry_t> syscall_sequence = {
            /* clang-format off */
            test_util::make_header(TRACE_ENTRY_VERSION),
            test_util::make_thread(TID_IN_SYSCALLS),
            test_util::make_pid(TID_IN_SYSCALLS),
            test_util::make_version(TRACE_ENTRY_VERSION),
            test_util::make_timestamp(0),
            test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_TRACE_START, SYSCALL_BASE),
            test_util::make_instr(SYSCALL_PC_START),
            test_util::make_marker(TRACE_MARKER_TYPE_BRANCH_TARGET, 0),
            test_util::make_instr(SYSCALL_PC_START + 1, TRACE_TYPE_INSTR_INDIRECT_JUMP),
            test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_TRACE_END, SYSCALL_BASE),
            // XXX: Currently all syscall traces are concatenated. We may change
            // this to use an archive file instead.
            test_util::make_marker(
                TRACE_MARKER_TYPE_SYSCALL_TRACE_START, SYSCALL_BASE + 1),
            test_util::make_instr(SYSCALL_PC_START + 10),
            test_util::make_instr(SYSCALL_PC_START + 11),
            test_util::make_marker(TRACE_MARKER_TYPE_BRANCH_TARGET, 0),
            test_util::make_instr(SYSCALL_PC_START + 12, TRACE_TYPE_INSTR_INDIRECT_JUMP),
            test_util::make_marker(
                TRACE_MARKER_TYPE_SYSCALL_TRACE_END, SYSCALL_BASE + 1),
            test_util::make_exit(TID_IN_SYSCALLS),
            test_util::make_footer(),
            /* clang-format on */
        };
        auto syscall_reader = std::unique_ptr<test_util::mock_reader_t>(
            new test_util::mock_reader_t(syscall_sequence));
        auto syscall_reader_end =
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t());
        static constexpr int NUM_INPUTS = 3;
        static constexpr int NUM_INSTRS = 9;
        static constexpr int INSTR_QUANTUM = 3;
        static constexpr uint64_t TIMESTAMP = 44226688;
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        std::vector<scheduler_t::input_reader_t> readers;
        for (int input_idx = 0; input_idx < NUM_INPUTS; input_idx++) {
            std::vector<trace_entry_t> inputs;
            inputs.push_back(test_util::make_header(TRACE_ENTRY_VERSION));
            memref_tid_t tid = TID_BASE + input_idx;
            inputs.push_back(test_util::make_thread(tid));
            inputs.push_back(test_util::make_pid(1));
            inputs.push_back(test_util::make_version(TRACE_ENTRY_VERSION));
            inputs.push_back(
                // Just a non-zero filetype.
                test_util::make_marker(TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE));
            inputs.push_back(test_util::make_timestamp(TIMESTAMP));
            for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
                inputs.push_back(test_util::make_instr(
                    static_cast<addr_t>(42 * tid + instr_idx * 4), TRACE_TYPE_INSTR,
                    /*size=*/4));
                // Every other instr is a syscall.
                if (instr_idx % 2 == 0) {
                    // The markers after the syscall instr are supposed to be bracketed
                    // by timestamp markers.
                    bool add_post_timestamp = true;
                    inputs.push_back(test_util::make_timestamp(TIMESTAMP + instr_idx));
                    inputs.push_back(test_util::make_marker(
                        TRACE_MARKER_TYPE_SYSCALL, SYSCALL_BASE + (instr_idx / 2) % 2));
                    // Every other syscall is a blocking syscall.
                    if (instr_idx % 4 == 0) {
                        inputs.push_back(test_util::make_marker(
                            TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, /*value=*/0));
                    }
                    if (instr_idx == 0) {
                        // Assuming the first syscall was specified in -record_syscall,
                        // so we'll have additional markers.
                        inputs.push_back(test_util::make_marker(
                            TRACE_MARKER_TYPE_FUNC_ID,
                            static_cast<uintptr_t>(
                                func_trace_t::TRACE_FUNC_ID_SYSCALL_BASE) +
                                SYSCALL_BASE));
                        inputs.push_back(test_util::make_marker(
                            TRACE_MARKER_TYPE_FUNC_ARG, /*value=*/10));
                        if (input_idx == 0) {
                            // First syscall on first input was interrupted by a signal,
                            // so no post-syscall event.
                            inputs.push_back(test_util::make_marker(
                                TRACE_MARKER_TYPE_KERNEL_EVENT, /*value=*/1));
                            inputs.push_back(test_util::make_marker(
                                TRACE_MARKER_TYPE_KERNEL_XFER, /*value=*/1));
                            add_post_timestamp = false;
                        } else if (input_idx == 1) {
                            // First syscall on second input is a sigreturn that also
                            // adds a kernel_xfer marker.
                            inputs.push_back(test_util::make_marker(
                                TRACE_MARKER_TYPE_KERNEL_XFER, /*value=*/1));
                        } else {
                            inputs.push_back(test_util::make_marker(
                                TRACE_MARKER_TYPE_FUNC_ID,
                                static_cast<uintptr_t>(
                                    func_trace_t::TRACE_FUNC_ID_SYSCALL_BASE) +
                                    SYSCALL_BASE));
                            inputs.push_back(test_util::make_marker(
                                TRACE_MARKER_TYPE_FUNC_RETVAL, /*value=*/1));
                            inputs.push_back(test_util::make_marker(
                                TRACE_MARKER_TYPE_SYSCALL_FAILED, /*value=*/1));
                        }
                    }
                    if (add_post_timestamp) {
                        inputs.push_back(
                            test_util::make_timestamp(TIMESTAMP + instr_idx + 1));
                    }
                }
            }
            inputs.push_back(test_util::make_exit(tid));
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs)),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                tid);
        }
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_TIMESTAMPS,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        sched_ops.quantum_duration_instrs = INSTR_QUANTUM;
        sched_ops.kernel_syscall_reader = std::move(syscall_reader);
        sched_ops.kernel_syscall_reader_end = std::move(syscall_reader_end);
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::vector<memref_t>> refs;
        std::vector<std::string> sched_as_string = run_lockstep_simulation_for_kernel_seq(
            scheduler, NUM_OUTPUTS, TID_BASE, SYSCALL_BASE, refs,
            /*for_syscall_seq=*/true);
        // Check the high-level strings.
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        // The instrs in the injected syscall sequence count towards the #instr
        // quantum, but no context switch happens in the middle of the syscall seq.
        assert(sched_as_string[0] ==
               "Avf0i0SsFF1ii1kk,Cvf0i0SsFF1ii1FFs0,Aii0S2iii20,Cii0S2iii20,"
               "Aii0Ss1ii10,Cii0Ss1ii10,Aii0S2iii20,Cii0S2iii20,Aii0Ss1ii10,Cii0Ss1ii10");
        assert(sched_as_string[1] ==
               "Bvf0i0SsFF1ii1k0ii0S2iii20ii0Ss1ii10ii0S2iii20ii0Ss1ii10______________"
               "____________________________________________");
        // Zoom in and check the first few syscall sequences on the first output record
        // by record with value checks.
        int idx = 0;
        bool res = true;
        res = res &&
            // First thread.
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_VERSION) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_FILETYPE,
                      FILE_TYPE | OFFLINE_FILE_TYPE_KERNEL_SYSCALLS) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_INSTR) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_SYSCALL, SYSCALL_BASE) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_FUNC_ID,
                      static_cast<uintptr_t>(func_trace_t::TRACE_FUNC_ID_SYSCALL_BASE) +
                          SYSCALL_BASE) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_FUNC_ARG, 10) &&

            // Syscall_1 trace on first thread.
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_SYSCALL_TRACE_START, SYSCALL_BASE) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_INSTR) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_INSTR_INDIRECT_JUMP,
                      TRACE_MARKER_TYPE_RESERVED_END, 42 * TID_BASE + 1 * 4) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_SYSCALL_TRACE_END, SYSCALL_BASE) &&

            // Signal interruption on first thread.
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_KERNEL_EVENT, 1) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_KERNEL_XFER, 1) &&

            // Second thread.
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_VERSION) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_FILETYPE,
                      FILE_TYPE | OFFLINE_FILE_TYPE_KERNEL_SYSCALLS) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_INSTR) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_SYSCALL, SYSCALL_BASE) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_FUNC_ID,
                      static_cast<uintptr_t>(func_trace_t::TRACE_FUNC_ID_SYSCALL_BASE) +
                          SYSCALL_BASE) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_FUNC_ARG, 10) &&

            // Syscall_1 trace on second thread.
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_SYSCALL_TRACE_START, SYSCALL_BASE) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_INSTR) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_INSTR_INDIRECT_JUMP,
                      TRACE_MARKER_TYPE_RESERVED_END, 42 * (TID_BASE + 2) + 1 * 4) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_SYSCALL_TRACE_END, SYSCALL_BASE) &&

            // Post-syscall markers
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_FUNC_ID,
                      static_cast<uintptr_t>(func_trace_t::TRACE_FUNC_ID_SYSCALL_BASE) +
                          SYSCALL_BASE) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_FUNC_RETVAL, 1) &&
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_SYSCALL_FAILED, 1) &&

            // Post syscall timestamp.
            check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP) &&

            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_INSTR) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_INSTR) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_SYSCALL, SYSCALL_BASE + 1) &&
            // Syscall_2 trace on first thread.
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_SYSCALL_TRACE_START, SYSCALL_BASE + 1) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_INSTR) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_INSTR) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_INSTR_INDIRECT_JUMP,
                      TRACE_MARKER_TYPE_RESERVED_END, 42 * TID_BASE + 3 * 4) &&
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_SYSCALL_TRACE_END, SYSCALL_BASE + 1) &&

            // Post syscall timestamp.
            check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER,
                      TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP);
        assert(res);
    }
    {
        // Test a bad input sequence.
        std::vector<trace_entry_t> bad_syscall_sequence = {
            /* clang-format off */
        test_util::make_header(TRACE_ENTRY_VERSION),
        test_util::make_thread(TID_IN_SYSCALLS),
        test_util::make_pid(TID_IN_SYSCALLS),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_TRACE_START, SYSCALL_BASE),
        test_util::make_instr(SYSCALL_PC_START),
        test_util::make_instr(SYSCALL_PC_START + 1),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_TRACE_END, SYSCALL_BASE),
        // Error: duplicate trace for the same syscall.
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_TRACE_START, SYSCALL_BASE),
        test_util::make_instr(SYSCALL_PC_START),
        test_util::make_instr(SYSCALL_PC_START + 1),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_TRACE_END, SYSCALL_BASE),
        test_util::make_exit(TID_IN_SYSCALLS),
        test_util::make_footer(),
            /* clang-format on */
        };
        auto bad_syscall_reader = std::unique_ptr<test_util::mock_reader_t>(
            new test_util::mock_reader_t(bad_syscall_sequence));
        auto bad_syscall_reader_end =
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t());
        std::vector<scheduler_t::input_workload_t> test_sched_inputs;
        std::vector<scheduler_t::input_reader_t> readers;
        std::vector<trace_entry_t> inputs;
        inputs.push_back(test_util::make_header(TRACE_ENTRY_VERSION));
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(inputs)),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_BASE);
        test_sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t test_sched_ops(
            scheduler_t::MAP_TO_ANY_OUTPUT, scheduler_t::DEPENDENCY_TIMESTAMPS,
            scheduler_t::SCHEDULER_DEFAULTS);
        test_sched_ops.kernel_syscall_reader = std::move(bad_syscall_reader);
        test_sched_ops.kernel_syscall_reader_end = std::move(bad_syscall_reader_end);
        scheduler_t test_scheduler;
        if (test_scheduler.init(test_sched_inputs, NUM_OUTPUTS,
                                std::move(test_sched_ops)) !=
            scheduler_t::STATUS_ERROR_INVALID_PARAMETER)
            assert(false);
    }
}

void
test_random_schedule()
{
    std::cerr << "\n----------------\nTesting random scheduling\n";
    static constexpr int NUM_INPUTS = 7;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int NUM_INSTRS = 9;
    static constexpr int QUANTUM_DURATION = 3;
    static constexpr int ITERS = 9;
    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = TID_BASE + i;
        inputs[i].push_back(test_util::make_thread(tid));
        inputs[i].push_back(test_util::make_pid(1));
        inputs[i].push_back(test_util::make_version(TRACE_ENTRY_VERSION));
        inputs[i].push_back(test_util::make_timestamp(10)); // All the same time priority.
        for (int j = 0; j < NUM_INSTRS; j++) {
            inputs[i].push_back(test_util::make_instr(42 + j * 4));
        }
        inputs[i].push_back(test_util::make_exit(tid));
    }
    std::vector<std::set<std::string>> scheds_by_cpu(NUM_OUTPUTS);
    for (int iter = 0; iter < ITERS; ++iter) {
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        for (int i = 0; i < NUM_INPUTS; i++) {
            std::vector<scheduler_t::input_reader_t> readers;
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs[i])),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                TID_BASE + i);
            sched_inputs.emplace_back(std::move(readers));
        }
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        sched_ops.randomize_next_input = true;
        sched_ops.quantum_duration_instrs = QUANTUM_DURATION;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
            scheds_by_cpu[i].insert(sched_as_string[i]);
        }
    }
    // With non-determinism it's hard to have a precise test.
    // We assume most runs should be different: at least half of them (probably
    // more but let's not make this into a flaky test).
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        assert(scheds_by_cpu[i].size() >= ITERS / 2);
    }
}

void
check_next(record_scheduler_t::stream_t *stream,
           record_scheduler_t::stream_status_t expect_status,
           trace_type_t expect_type = TRACE_TYPE_MARKER, addr_t expect_addr = 0,
           addr_t expect_size = 0)
{
    trace_entry_t record;
    record_scheduler_t::stream_status_t status = stream->next_record(record);
    assert(status == expect_status);
    if (status == record_scheduler_t::STATUS_OK) {
        if (record.type != expect_type) {
            std::cerr << "Expected type " << expect_type << " != " << record.type << "\n";
            assert(false);
        }
        if (expect_size != 0 && record.size != expect_size) {
            std::cerr << "Expected size " << expect_size << " != " << record.size << "\n";
            assert(false);
        }
        if (expect_addr != 0 && record.addr != expect_addr) {
            std::cerr << "Expected addr " << expect_addr << " != " << record.addr << "\n";
            assert(false);
        }
    }
};

static void
test_record_scheduler()
{
    // Test record_scheduler_t switches, which operate differently:
    // they have to deal with encoding records preceding instructions,
    // and they have to insert tid,pid records.
    std::cerr << "\n----------------\nTesting record_scheduler_t\n";
    static constexpr memref_tid_t TID_A = 42;
    static constexpr memref_tid_t TID_B = TID_A + 1;
    static constexpr memref_tid_t PID_A = 142;
    static constexpr memref_tid_t PID_B = PID_A + 1;
    static constexpr int NUM_OUTPUTS = 1;
    static constexpr addr_t ENCODING_SIZE = 2;
    static constexpr addr_t ENCODING_IGNORE = 0xfeed;
    static constexpr uint64_t INITIAL_TIMESTAMP_A = 10;
    static constexpr uint64_t INITIAL_TIMESTAMP_B = 20;
    static constexpr uint64_t PRE_SYS_TIMESTAMP = 20;
    static constexpr uint64_t BLOCK_THRESHOLD = 500;
    std::vector<trace_entry_t> refs_A = {
        /* clang-format off */
        test_util::make_thread(TID_A),
        test_util::make_pid(PID_A),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_timestamp(INITIAL_TIMESTAMP_A),
        test_util::make_encoding(ENCODING_SIZE, ENCODING_IGNORE),
        test_util::make_instr(10),
        test_util::make_timestamp(PRE_SYS_TIMESTAMP),
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 42),
        test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        test_util::make_timestamp(PRE_SYS_TIMESTAMP + BLOCK_THRESHOLD),
        test_util::make_encoding(ENCODING_SIZE, ENCODING_IGNORE),
        test_util::make_instr(30),
        test_util::make_encoding(ENCODING_SIZE, ENCODING_IGNORE),
        test_util::make_instr(50),
        test_util::make_exit(TID_A),
        /* clang-format on */
    };
    std::vector<trace_entry_t> refs_B = {
        /* clang-format off */
        test_util::make_thread(TID_B),
        test_util::make_pid(PID_B),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_timestamp(INITIAL_TIMESTAMP_B),
        test_util::make_encoding(ENCODING_SIZE, ENCODING_IGNORE),
        test_util::make_instr(20),
        test_util::make_encoding(ENCODING_SIZE, ENCODING_IGNORE),
        test_util::make_instr(40),
        test_util::make_encoding(ENCODING_SIZE, ENCODING_IGNORE),
        // Test a target marker between the encoding and the instr.
        test_util::make_marker(TRACE_MARKER_TYPE_BRANCH_TARGET, 42),
        test_util::make_instr(60),
        // No encoding for repeated instr.
        test_util::make_instr(20),
        test_util::make_exit(TID_B),
        /* clang-format on */
    };
    std::vector<record_scheduler_t::input_reader_t> readers;
    readers.emplace_back(std::unique_ptr<test_util::mock_record_reader_t>(
                             new test_util::mock_record_reader_t(refs_A)),
                         std::unique_ptr<test_util::mock_record_reader_t>(
                             new test_util::mock_record_reader_t()),
                         TID_A);
    readers.emplace_back(std::unique_ptr<test_util::mock_record_reader_t>(
                             new test_util::mock_record_reader_t(refs_B)),
                         std::unique_ptr<test_util::mock_record_reader_t>(
                             new test_util::mock_record_reader_t()),
                         TID_B);
    record_scheduler_t scheduler;
    std::vector<record_scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    record_scheduler_t::scheduler_options_t sched_ops(
        record_scheduler_t::MAP_TO_ANY_OUTPUT, record_scheduler_t::DEPENDENCY_IGNORE,
        record_scheduler_t::SCHEDULER_DEFAULTS,
        /*verbosity=*/4);
    sched_ops.quantum_duration_instrs = 2;
    sched_ops.block_time_multiplier = 0.001; // Do not stay blocked.
    sched_ops.blocking_switch_threshold = BLOCK_THRESHOLD;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        record_scheduler_t::STATUS_SUCCESS)
        assert(false);
    auto *stream0 = scheduler.get_stream(0);
    // Advance cpu0 on TID_A to its 1st context switch.
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD, TID_A);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_PID, PID_A);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER);
    // Test ordinals.
    assert(stream0->get_instruction_ordinal() == 0);
    assert(stream0->get_input_interface()->get_instruction_ordinal() == 0);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_ENCODING);
    // The encoding should have incremented the ordinal. Note that the
    // record_reader_t and the corresponding scheduler both increment
    // these ordinals upon seeing the pre-instr encoding or branch_target marker
    // (if any).
    assert(stream0->get_instruction_ordinal() == 1);
    assert(stream0->get_input_interface()->get_instruction_ordinal() == 1);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
    // The instr should not have further incremented it.
    assert(stream0->get_instruction_ordinal() == 1);
    assert(stream0->get_input_interface()->get_instruction_ordinal() == 1);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER);
    // Ensure the context switch is *before* the encoding.
    // Advance cpu0 on TID_B to its 1st context switch.
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD, TID_B);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_PID, PID_B);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_ENCODING);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_ENCODING);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
    // Ensure the switch is *before* the encoding and target marker.
    assert(stream0->get_input_interface()->get_instruction_ordinal() == 2);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD, TID_A);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_PID, PID_A);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_ENCODING);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD, TID_B);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_PID, PID_B);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_ENCODING);
    assert(stream0->get_instruction_ordinal() == 5);
    assert(stream0->get_input_interface()->get_instruction_ordinal() == 3);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER);
    assert(stream0->get_instruction_ordinal() == 5);
    assert(stream0->get_input_interface()->get_instruction_ordinal() == 3);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
    // Should still be at the same count after the encoding, marker, and instr.
    assert(stream0->get_instruction_ordinal() == 5);
    assert(stream0->get_input_interface()->get_instruction_ordinal() == 3);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
    assert(stream0->get_instruction_ordinal() == 6);
    assert(stream0->get_input_interface()->get_instruction_ordinal() == 4);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD_EXIT);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD, TID_A);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_PID, PID_A);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_ENCODING);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD_EXIT);
    check_next(stream0, record_scheduler_t::STATUS_EOF);
}

static void
test_record_scheduler_i7574()
{
    // Demonstrates how the scheduler responds to traces with the i#7574 issue.
    // When there's an abandoned branch_target marker in the previous chunk, with
    // other markers before the corresponding encoding+instr in the next chunk, it
    // affects the scheduler in unexpected ways: the instr ordinals are erroneously
    // incremented for just the branch_target and then again at the encoding+instr, and
    // there may be a context switch that splits up the branch_target marker and its
    // corresponding instruction.
    // TODO i#7574: Workaround this issue in the scheduler and modify this test to
    // prove correct operation.
    std::cerr
        << "\n----------------\nTesting record_scheduler_t to show the i#7574 issue\n";
    static constexpr memref_tid_t TID_A = 42;
    static constexpr memref_tid_t TID_B = TID_A + 1;
    static constexpr memref_tid_t PID_A = 142;
    static constexpr memref_tid_t PID_B = PID_A + 1;
    static constexpr int NUM_OUTPUTS = 1;
    static constexpr addr_t ENCODING_SIZE = 2;
    static constexpr addr_t ENCODING_IGNORE = 0xfeed;
    static constexpr uint64_t INITIAL_TIMESTAMP_A = 10;
    static constexpr uint64_t INITIAL_TIMESTAMP_B = 20;
    std::vector<trace_entry_t> refs_A = {
        /* clang-format off */
        test_util::make_thread(TID_A),
        test_util::make_pid(PID_A),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_timestamp(INITIAL_TIMESTAMP_A),
        test_util::make_encoding(ENCODING_SIZE, ENCODING_IGNORE),
        test_util::make_instr(10),
        // Second instr, but the chunk end breaks up the
        // branch_target marker and the instr (i#7574).
        test_util::make_marker(TRACE_MARKER_TYPE_BRANCH_TARGET, 1),
        test_util::make_marker(TRACE_MARKER_TYPE_CHUNK_FOOTER, 1),
        test_util::make_marker(TRACE_MARKER_TYPE_RECORD_ORDINAL, 1),
        test_util::make_timestamp(INITIAL_TIMESTAMP_A + 1),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 1),
        test_util::make_encoding(ENCODING_SIZE, ENCODING_IGNORE),
        test_util::make_instr(30),
        // No encoding for repeated instr.
        test_util::make_instr(10),
        test_util::make_exit(TID_A),
        /* clang-format on */
    };
    std::vector<trace_entry_t> refs_B = {
        /* clang-format off */
        test_util::make_thread(TID_B),
        test_util::make_pid(PID_B),
        test_util::make_version(TRACE_ENTRY_VERSION),
        test_util::make_timestamp(INITIAL_TIMESTAMP_B),
        test_util::make_encoding(ENCODING_SIZE, ENCODING_IGNORE),
        test_util::make_instr(20),
        test_util::make_encoding(ENCODING_SIZE, ENCODING_IGNORE),
        test_util::make_instr(40),
        test_util::make_encoding(ENCODING_SIZE, ENCODING_IGNORE),
        test_util::make_instr(60),
        test_util::make_exit(TID_B),
        /* clang-format on */
    };
    std::vector<record_scheduler_t::input_reader_t> readers;
    readers.emplace_back(std::unique_ptr<test_util::mock_record_reader_t>(
                             new test_util::mock_record_reader_t(refs_A)),
                         std::unique_ptr<test_util::mock_record_reader_t>(
                             new test_util::mock_record_reader_t()),
                         TID_A);
    readers.emplace_back(std::unique_ptr<test_util::mock_record_reader_t>(
                             new test_util::mock_record_reader_t(refs_B)),
                         std::unique_ptr<test_util::mock_record_reader_t>(
                             new test_util::mock_record_reader_t()),
                         TID_B);
    record_scheduler_t scheduler;
    std::vector<record_scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    record_scheduler_t::scheduler_options_t sched_ops(
        record_scheduler_t::MAP_TO_ANY_OUTPUT, record_scheduler_t::DEPENDENCY_IGNORE,
        record_scheduler_t::SCHEDULER_DEFAULTS,
        /*verbosity=*/4);
    sched_ops.quantum_duration_instrs = 2;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        record_scheduler_t::STATUS_SUCCESS)
        assert(false);
    auto *stream0 = scheduler.get_stream(0);
    // Advance cpu0 on TID_A to its 1st context switch.
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD, TID_A);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_PID, PID_A);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER);
    // Test input/output instr ordinals.
    assert(stream0->get_instruction_ordinal() == 0);
    assert(stream0->get_input_interface()->get_instruction_ordinal() == 0);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_ENCODING);
    // The encoding should have incremented the input/output instr ordinals. Note
    // that the record_reader_t and the corresponding scheduler both increment
    // these ordinals upon seeing the pre-instr encoding or branch_target marker
    // (if any).
    assert(stream0->get_instruction_ordinal() == 1);
    assert(stream0->get_input_interface()->get_instruction_ordinal() == 1);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
    // The instr should not have further incremented it.
    assert(stream0->get_instruction_ordinal() == 1);
    assert(stream0->get_input_interface()->get_instruction_ordinal() == 1);

    // The branch_target marker should have incremented the input/output
    // instr ordinals.
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER, 0,
               TRACE_MARKER_TYPE_BRANCH_TARGET);
    assert(stream0->get_instruction_ordinal() == 2);
    assert(stream0->get_input_interface()->get_instruction_ordinal() == 2);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER, 0,
               TRACE_MARKER_TYPE_CHUNK_FOOTER);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER, 0,
               TRACE_MARKER_TYPE_RECORD_ORDINAL);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER, 0,
               TRACE_MARKER_TYPE_TIMESTAMP);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER, 0,
               TRACE_MARKER_TYPE_CPU_ID);

    // TODO i#7574: A context switch happens here because the input A has
    // seen all instrs for its quantum (the abandoned branch_target is erroneously
    // counted as one), and this is considered a safe spot for a context switch.
    // This needs to be worked around in the scheduler so that, in traces affected
    // by i#7574, the branch_target marker and the corresponding instr are not
    // split up.

    // Input B on core 0.
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD, TID_B);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_PID, PID_B);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_MARKER);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_ENCODING);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_ENCODING);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
    assert(stream0->get_instruction_ordinal() == 4);
    // Back to input A because input B has seen all instrs for its quantum.
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD, TID_A);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_PID, PID_A);

    // TODO i#7574: This instr is split from its branch_target marker. Would
    // increment the input/output instr ordinals again erroneously.
    // Note that at this point, the encoding entry has been read from the
    // input, but not returned by the scheduler yet; so only the input instr
    // ordinal is seen incremented.
    assert(stream0->get_instruction_ordinal() == 4);
    assert(stream0->get_input_interface()->get_instruction_ordinal() == 3);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_ENCODING);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
    assert(stream0->get_instruction_ordinal() == 5);
    assert(stream0->get_input_interface()->get_instruction_ordinal() == 3);

    // Remaining content from inputs A and B.
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD_EXIT);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD, TID_B);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_PID, PID_B);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_ENCODING);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_INSTR);
    check_next(stream0, record_scheduler_t::STATUS_OK, TRACE_TYPE_THREAD_EXIT);
    check_next(stream0, record_scheduler_t::STATUS_EOF);
}

static void
test_rebalancing()
{
    std::cerr << "\n----------------\nTesting rebalancing\n";
    // We want to get the cores into an unbalanced state.
    // The scheduler will start out with round-robin even assignment.
    // We use "unschedule" and "direct switch" operations to get all
    // inputs onto one core.
    static constexpr int NUM_OUTPUTS = 8;
    static constexpr int NUM_INPUTS_UNSCHED = 24;
    static constexpr int BLOCK_LATENCY = 100;
    static constexpr double BLOCK_SCALE = 1. / (BLOCK_LATENCY);
    static constexpr int QUANTUM_DURATION = 3 * NUM_OUTPUTS;
    static constexpr int NUM_INSTRS = QUANTUM_DURATION * 3;
    static constexpr int REBALANCE_PERIOD = NUM_OUTPUTS * 20 * NUM_INPUTS_UNSCHED;
    static constexpr int MIGRATION_THRESHOLD = QUANTUM_DURATION;
    // Keep unscheduled for longer.
    static constexpr uint64_t BLOCK_TIME_MAX = 250000;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr memref_tid_t TID_A = TID_BASE + 0;
    static constexpr memref_tid_t TID_B = TID_BASE + 1;
    static constexpr uint64_t TIMESTAMP_START_INSTRS = 9999;

    std::vector<trace_entry_t> refs_controller;
    refs_controller.push_back(test_util::make_thread(TID_A));
    refs_controller.push_back(test_util::make_pid(1));
    refs_controller.push_back(test_util::make_version(TRACE_ENTRY_VERSION));
    refs_controller.push_back(test_util::make_timestamp(1001));
    refs_controller.push_back(test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
    // Our controller switches to the first thread, who then switches to
    // the next, etc.
    refs_controller.push_back(test_util::make_instr(/*pc=*/101));
    refs_controller.push_back(test_util::make_instr(/*pc=*/102));
    refs_controller.push_back(test_util::make_timestamp(1101));
    refs_controller.push_back(test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
    refs_controller.push_back(test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999));
    refs_controller.push_back(
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_ARG_TIMEOUT, BLOCK_LATENCY));
    refs_controller.push_back(
        test_util::make_marker(TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH, TID_B));
    refs_controller.push_back(test_util::make_timestamp(1201));
    refs_controller.push_back(test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
    refs_controller.push_back(test_util::make_instr(/*pc=*/401));
    refs_controller.push_back(test_util::make_exit(TID_A));
    // Our unsched threads all start unscheduled.
    std::vector<std::vector<trace_entry_t>> refs_unsched(NUM_INPUTS_UNSCHED);
    for (int i = 0; i < NUM_INPUTS_UNSCHED; ++i) {
        refs_unsched[i].push_back(test_util::make_thread(TID_B + i));
        refs_unsched[i].push_back(test_util::make_pid(1));
        refs_unsched[i].push_back(test_util::make_version(TRACE_ENTRY_VERSION));
        refs_unsched[i].push_back(test_util::make_timestamp(2001));
        refs_unsched[i].push_back(test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
        // B starts unscheduled with no timeout.
        refs_unsched[i].push_back(test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999));
        refs_unsched[i].push_back(
            test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0));
        refs_unsched[i].push_back(
            test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE, 0));
        refs_unsched[i].push_back(test_util::make_timestamp(3001));
        refs_unsched[i].push_back(test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
        // Once scheduled, wake up the next thread.
        refs_unsched[i].push_back(test_util::make_timestamp(1101 + 100 * i));
        refs_unsched[i].push_back(test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
        refs_unsched[i].push_back(test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 999));
        refs_unsched[i].push_back(
            test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_ARG_TIMEOUT, BLOCK_LATENCY));
        refs_unsched[i].push_back(test_util::make_marker(
            TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH, TID_B + i + 1));
        // Give everyone the same timestamp so we alternate on preempts.
        refs_unsched[i].push_back(test_util::make_timestamp(TIMESTAMP_START_INSTRS));
        refs_unsched[i].push_back(test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
        // Now run a bunch of instrs so we'll reach our rebalancing period.
        for (int instrs = 0; instrs < NUM_INSTRS; ++instrs) {
            refs_unsched[i].push_back(test_util::make_instr(/*pc=*/200 + instrs));
        }
        refs_unsched[i].push_back(test_util::make_exit(TID_B + i));
    }
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(
            new test_util::mock_reader_t(refs_controller)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), TID_A);
    for (int i = 0; i < NUM_INPUTS_UNSCHED; ++i) {
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(refs_unsched[i])),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_B + i);
    }

    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/3);
    // We use our mock's time==instruction count for a deterministic result.
    sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
    sched_ops.time_units_per_us = 1.;
    sched_ops.quantum_duration_us = QUANTUM_DURATION;
    sched_ops.block_time_multiplier = BLOCK_SCALE;
    sched_ops.migration_threshold_us = MIGRATION_THRESHOLD;
    sched_ops.rebalance_period_us = REBALANCE_PERIOD;
    sched_ops.block_time_max_us = BLOCK_TIME_MAX;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
    // We should see a lot of migrations away from output 0: we should see the
    // per-output average per other output, minus the live input.
    assert(scheduler.get_stream(0)->get_schedule_statistic(
               memtrace_stream_t::SCHED_STAT_MIGRATIONS) >=
           (NUM_INPUTS_UNSCHED / NUM_OUTPUTS) * (NUM_OUTPUTS - 1) - 1);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        // Ensure we see multiple inputs on each output.
        std::unordered_set<char> inputs;
        for (char c : sched_as_string[i]) {
            if (std::isalpha(c))
                inputs.insert(c);
        }
        assert(inputs.size() >= (NUM_INPUTS_UNSCHED / NUM_OUTPUTS) - 1);
    }
}

static void
test_initial_migrate()
{
    std::cerr << "\n----------------\nTesting initial migrations\n";
    // We want to ensures migration thresholds are applied to never-executed inputs.
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr memref_tid_t TID_A = TID_BASE + 0;
    static constexpr memref_tid_t TID_B = TID_BASE + 1;
    static constexpr memref_tid_t TID_C = TID_BASE + 2;
    static constexpr uint64_t TIMESTAMP_START = 10;

    // We have 3 inputs and 2 outputs. We expect a round-robin initial assignment
    // to put A and C on output #0 and B on #1.
    // B will finish #1 and then try to steal C from A but should fail if initial
    // migrations have to wait for the threshold as though the input just ran
    // right before the trace started, which is how we treat them now.
    std::vector<trace_entry_t> refs_A = {
        /* clang-format off */
        test_util::make_thread(TID_A),
        test_util::make_pid(1),
        test_util::make_version(4),
        test_util::make_timestamp(TIMESTAMP_START),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_instr(10),
        test_util::make_instr(11),
        test_util::make_instr(12),
        test_util::make_instr(13),
        test_util::make_instr(14),
        test_util::make_instr(15),
        test_util::make_exit(TID_A),
        /* clang-format on */
    };
    std::vector<trace_entry_t> refs_B = {
        /* clang-format off */
        test_util::make_thread(TID_B),
        test_util::make_pid(1),
        test_util::make_version(4),
        test_util::make_timestamp(TIMESTAMP_START),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_instr(20),
        test_util::make_exit(TID_B),
        /* clang-format on */
    };
    std::vector<trace_entry_t> refs_C = {
        /* clang-format off */
        test_util::make_thread(TID_C),
        test_util::make_pid(1),
        test_util::make_version(4),
        test_util::make_timestamp(TIMESTAMP_START + 10),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        test_util::make_instr(30),
        test_util::make_instr(31),
        test_util::make_instr(32),
        test_util::make_exit(TID_C),
        /* clang-format on */
    };

    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(refs_A)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), TID_A);
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(refs_B)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), TID_B);
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(refs_C)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), TID_C);
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/3);
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
    // We should see zero migrations since output #1 failed to steal C from output #0.
    static const char *const CORE0_SCHED_STRING = "...AAAAAA....CCC.";
    static const char *const CORE1_SCHED_STRING = "...B.____________";
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        assert(scheduler.get_stream(i)->get_schedule_statistic(
                   memtrace_stream_t::SCHED_STAT_MIGRATIONS) == 0);
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    assert(sched_as_string[0] == CORE0_SCHED_STRING);
    assert(sched_as_string[1] == CORE1_SCHED_STRING);
}

static void
test_exit_early()
{
    std::cerr << "\n----------------\nTesting exiting early\n";
    static constexpr int NUM_INPUTS = 12;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int NUM_INSTRS = 9;
    static constexpr int QUANTUM_DURATION = 3;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr uint64_t TIMESTAMP = 101;
    static constexpr uint64_t BLOCK_LATENCY = 1500;
    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = TID_BASE + i;
        inputs[i].push_back(test_util::make_thread(tid));
        inputs[i].push_back(test_util::make_pid(1));
        inputs[i].push_back(test_util::make_version(TRACE_ENTRY_VERSION));
        inputs[i].push_back(
            test_util::make_timestamp(TIMESTAMP)); // All the same time priority.
        for (int j = 0; j < NUM_INSTRS; j++) {
            inputs[i].push_back(test_util::make_instr(42 + j * 4));
            // One input has a long blocking syscall toward the end.
            if (i == 0 && j == NUM_INSTRS - 2) {
                inputs[i].push_back(test_util::make_timestamp(TIMESTAMP));
                inputs[i].push_back(
                    test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL, 42));
                inputs[i].push_back(
                    test_util::make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0));
                inputs[i].push_back(test_util::make_timestamp(TIMESTAMP + BLOCK_LATENCY));
            }
        }
        inputs[i].push_back(test_util::make_exit(tid));
    }
    {
        // Run without any early exit.
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        for (int i = 0; i < NUM_INPUTS; i++) {
            std::vector<scheduler_t::input_reader_t> readers;
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs[i])),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                TID_BASE + i);
            sched_inputs.emplace_back(std::move(readers));
        }
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/2);
        // We use our mock's time==instruction count for a deterministic result.
        sched_ops.time_units_per_us = 1.;
        sched_ops.quantum_duration_instrs = QUANTUM_DURATION;
        sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
        sched_ops.exit_if_fraction_inputs_left = 0.;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        // We have a long idle wait just to execute A's final instruction.
        static const char *const CORE0_SCHED_STRING =
            "..AAA..CCC..EEE..GGG..III..KKKAAACCCEEEGGGIIIKKKAA....CCC.EEE.GGG.III.KKK.__"
            "_________________________________________________________________A.";
        static const char *const CORE1_SCHED_STRING =
            "..BBB..DDD..FFF..HHH..JJJ..LLLBBBDDDFFFHHHJJJLLLBBB.DDD.FFF.HHH.JJJ.LLL.____"
            "___________________________________________________________________";
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
        assert(sched_as_string[1] == CORE1_SCHED_STRING);
    }
    {
        // Run with any early exit.
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        for (int i = 0; i < NUM_INPUTS; i++) {
            std::vector<scheduler_t::input_reader_t> readers;
            readers.emplace_back(
                std::unique_ptr<test_util::mock_reader_t>(
                    new test_util::mock_reader_t(inputs[i])),
                std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
                TID_BASE + i);
            sched_inputs.emplace_back(std::move(readers));
        }
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/2);
        // We use our mock's time==instruction count for a deterministic result.
        sched_ops.time_units_per_us = 1.;
        sched_ops.quantum_duration_instrs = QUANTUM_DURATION;
        sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
        // NUM_INPUTS=11 * 0.1 = 1.1 so we'll exit with 1 input left.
        sched_ops.exit_if_fraction_inputs_left = 0.1;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE, /*send_time=*/true);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        // Now we exit after K and never execute the 9th A.
        static const char *const CORE0_SCHED_STRING =
            "..AAA..CCC..EEE..GGG..III..KKKAAACCCEEEGGGIIIKKKAA....CCC.EEE.GGG.III.KKK.";
        static const char *const CORE1_SCHED_STRING =
            "..BBB..DDD..FFF..HHH..JJJ..LLLBBBDDDFFFHHHJJJLLLBBB.DDD.FFF.HHH.JJJ.LLL.__";
        assert(sched_as_string[0] == CORE0_SCHED_STRING);
        assert(sched_as_string[1] == CORE1_SCHED_STRING);
    }
}

static void
test_dynamic_marker_updates()
{
    std::cerr << "\n----------------\nTesting marker and tid/pid updates\n";
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_IGNORE,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/2);
    static constexpr int NUM_INPUTS = 5;
    static constexpr int NUM_OUTPUTS = 3;
    // We need at least enough instrs to cover INSTRS_PER_US==time_units_per_us.
    static constexpr int TIMESTAMP_GAP_US = 10;
    const int NUM_INSTRS =
        static_cast<int>(sched_ops.time_units_per_us) * TIMESTAMP_GAP_US;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr memref_pid_t PID_BASE = 200;
    static constexpr uint64_t TIMESTAMP_BASE = 12340000;

    std::vector<trace_entry_t> inputs[NUM_INPUTS];

    std::minstd_rand rand_gen;
    rand_gen.seed(static_cast<int>(reinterpret_cast<int64_t>(&inputs[0])));

    for (int i = 0; i < NUM_INPUTS; i++) {
        // Each input is a separate workload with the same pid and tid.
        memref_tid_t tid = TID_BASE;
        inputs[i].push_back(test_util::make_thread(tid));
        inputs[i].push_back(test_util::make_pid(PID_BASE));
        inputs[i].push_back(test_util::make_version(TRACE_ENTRY_VERSION));
        // Add a randomly-increasing-value timestamp.
        uint64_t cur_timestamp = TIMESTAMP_BASE;
        cur_timestamp += rand_gen();
        inputs[i].push_back(test_util::make_timestamp(cur_timestamp));
        // Add a cpuid with a random value.
        inputs[i].push_back(test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, rand_gen()));
        for (int j = 0; j < NUM_INSTRS; j++) {
            inputs[i].push_back(test_util::make_instr(42 + j * 4));
            // Add a randomly-increasing-value timestamp.
            cur_timestamp += rand_gen();
            inputs[i].push_back(test_util::make_timestamp(cur_timestamp));
            // Add a cpuid with a random value.
            inputs[i].push_back(
                test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, rand_gen()));
        }
        inputs[i].push_back(test_util::make_exit(tid));
    }
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    for (int i = 0; i < NUM_INPUTS; i++) {
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(inputs[i])),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_BASE);
        sched_inputs.emplace_back(std::move(readers));
    }
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<scheduler_t::stream_t *> outputs(NUM_OUTPUTS, nullptr);
    std::vector<uintptr_t> first_timestamp(NUM_OUTPUTS, 0);
    std::vector<uintptr_t> last_timestamp(NUM_OUTPUTS, 0);
    std::vector<bool> eof(NUM_OUTPUTS, false);
    for (int i = 0; i < NUM_OUTPUTS; i++)
        outputs[i] = scheduler.get_stream(i);
    int num_eof = 0;
    while (num_eof < NUM_OUTPUTS) {
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            if (eof[i])
                continue;
            memref_t memref;
            scheduler_t::stream_status_t status = outputs[i]->next_record(memref);
            if (status == scheduler_t::STATUS_EOF) {
                ++num_eof;
                eof[i] = true;
                continue;
            }
            if (status == scheduler_t::STATUS_IDLE)
                continue;
            assert(status == scheduler_t::STATUS_OK);
            assert(
                memref.marker.tid ==
                ((outputs[i]->get_workload_id() << MEMREF_ID_WORKLOAD_SHIFT) | TID_BASE));
            assert(
                memref.marker.pid ==
                ((outputs[i]->get_workload_id() << MEMREF_ID_WORKLOAD_SHIFT) | PID_BASE));
            if (memref.marker.type != TRACE_TYPE_MARKER)
                continue;
            // Make sure the random values have some order now, satisfying invariants.
            if (memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP) {
                assert(memref.marker.marker_value >= last_timestamp[i]);
                last_timestamp[i] = memref.marker.marker_value;
                if (first_timestamp[i] == 0)
                    first_timestamp[i] = memref.marker.marker_value;
            } else if (memref.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID) {
                assert(memref.marker.marker_value ==
                       static_cast<uintptr_t>(outputs[i]->get_shard_index()));
            }
        }
    }
    // Ensure we didn't short-circuit or exit early.
    int64_t instrs_seen = 0;
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        instrs_seen += outputs[i]->get_instruction_ordinal();
        // Check that the timestamps increased enough.
        assert(last_timestamp[i] - first_timestamp[i] >= TIMESTAMP_GAP_US);
    }
    assert(instrs_seen == NUM_INPUTS * NUM_INSTRS);
}

static void
test_static_marker_updates()
{
    std::cerr << "\n----------------\nTesting static marker updates\n";
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_CONSISTENT_OUTPUT,
                                               scheduler_t::DEPENDENCY_IGNORE,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/2);
    static constexpr int NUM_INPUTS = 2;
    static constexpr int NUM_OUTPUTS = 2;
    const int NUM_INSTRS = 12;
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr memref_pid_t PID_BASE = 200;
    static constexpr uint64_t TIMESTAMP_BASE = 12340000;

    std::vector<trace_entry_t> inputs[NUM_INPUTS];

    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = TID_BASE;
        inputs[i].push_back(test_util::make_thread(tid));
        inputs[i].push_back(test_util::make_pid(PID_BASE));
        inputs[i].push_back(test_util::make_version(TRACE_ENTRY_VERSION));
        uint64_t cur_timestamp = TIMESTAMP_BASE + i;
        inputs[i].push_back(test_util::make_timestamp(cur_timestamp));
        inputs[i].push_back(test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 1));
        for (int j = 0; j < NUM_INSTRS; j++) {
            inputs[i].push_back(test_util::make_instr(42 + j * 4));
            // Include idle and wait markers, which should get transformed.
            // We have one idle and one wait for every instruction.
            inputs[i].push_back(test_util::make_marker(TRACE_MARKER_TYPE_CORE_IDLE, 0));
            inputs[i].push_back(test_util::make_marker(TRACE_MARKER_TYPE_CORE_WAIT, 0));
        }
        inputs[i].push_back(test_util::make_exit(tid));
    }
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    for (int i = 0; i < NUM_INPUTS; i++) {
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(inputs[i])),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_BASE);
        sched_inputs.emplace_back(std::move(readers));
    }
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<scheduler_t::stream_t *> outputs(NUM_OUTPUTS, nullptr);
    std::vector<bool> eof(NUM_OUTPUTS, false);
    for (int i = 0; i < NUM_OUTPUTS; i++)
        outputs[i] = scheduler.get_stream(i);
    int num_eof = 0, num_idle = 0, num_wait = 0, num_instr = 0;
    while (num_eof < NUM_OUTPUTS) {
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            if (eof[i])
                continue;
            memref_t memref;
            scheduler_t::stream_status_t status = outputs[i]->next_record(memref);
            if (status == scheduler_t::STATUS_EOF) {
                ++num_eof;
                eof[i] = true;
                continue;
            }
            if (status == scheduler_t::STATUS_IDLE) {
                ++num_idle;
                continue;
            }
            if (status == scheduler_t::STATUS_WAIT) {
                ++num_wait;
                continue;
            }
            assert(status == scheduler_t::STATUS_OK);
            // The idle and wait markers should have turned into statuses above.
            assert(memref.marker.type != TRACE_TYPE_MARKER ||
                   (memref.marker.marker_type != TRACE_MARKER_TYPE_CORE_IDLE &&
                    memref.marker.marker_type != TRACE_MARKER_TYPE_CORE_WAIT));
            if (type_is_instr(memref.instr.type))
                ++num_instr;
        }
    }
    assert(num_instr == NUM_INSTRS * NUM_INPUTS);
    // We should have one idle and one wait for every instruction.
    assert(num_instr == num_idle);
    assert(num_instr == num_wait);
}

static void
test_marker_updates()
{
    test_dynamic_marker_updates();
    test_static_marker_updates();
}

class test_options_t : public scheduler_fixed_tmpl_t<memref_t, reader_t> {
public:
    void
    check_options()
    {
        // Ensure scheduler_options_t.time_units_per_us ==
        // scheduler_impl_tmpl_t::INSTRS_PER_US.
        scheduler_t::scheduler_options_t default_options;
        assert(default_options.time_units_per_us == INSTRS_PER_US);
    }
};

static void
test_options_match()
{
    std::cerr << "\n----------------\nTesting option matching\n";
    test_options_t test;
    test.check_options();
}

// A mock noise generator that only generates TRACE_TYPE_READ records with
// address 0xdeadbeef and instruction fetches.
class mock_noise_generator_t : public noise_generator_t {
public:
    mock_noise_generator_t(noise_generator_info_t &info, const addr_t addr_to_generate)
        : noise_generator_t(info)
        , addr_to_generate_(addr_to_generate) {};

protected:
    trace_entry_t
    generate_trace_entry() override
    {
        trace_entry_t generated_entry;
        // We alternate between read and instruction fetch records.
        // We need to generate instructions to have the scheduler interleave noise
        // records with the rest of the input workloads. Instructions are what the
        // scheduler uses to estimate time quants to switch from one input workload
        // to another. Generating only read records does not advance the scheduler
        // time, which means all noise read records are scheduled altogether in the
        // same time quant, no matter how many.
        if (record_counter_ % 2) {
            generated_entry = { TRACE_TYPE_READ, 4, { addr_to_generate_ } };
        } else {
            generated_entry = { TRACE_TYPE_INSTR,
                                1,
                                { static_cast<addr_t>(record_counter_) } };
        }
        ++record_counter_;
        return generated_entry;
    }

private:
    addr_t addr_to_generate_ = 0x0;
    uint64_t record_counter_ = 0;
};

// A mock noise generator factory that creates mock_noise_generator_t.
class mock_noise_generator_factory_t
    : public noise_generator_factory_t<memref_t, reader_t> {
public:
    mock_noise_generator_factory_t(const addr_t addr_to_generate)
        : addr_to_generate_(addr_to_generate) {};

protected:
    std::unique_ptr<reader_t>
    create_noise_generator_begin(noise_generator_info_t &info) override
    {
        return std::unique_ptr<mock_noise_generator_t>(
            new mock_noise_generator_t(info, addr_to_generate_));
    };

    std::unique_ptr<reader_t>
    create_noise_generator_end() override
    {
        noise_generator_info_t info;
        info.num_records_to_generate = 0;
        return std::unique_ptr<mock_noise_generator_t>(
            new mock_noise_generator_t(info, 0));
    };

private:
    addr_t addr_to_generate_ = 0x0;
};

static void
test_noise_generator()
{
    std::cerr << "\n----------------\nTesting noise generator\n";
    static constexpr addr_t ADDR_TO_GENERATE = 0xdeadbeef;
    static constexpr uint64_t TIMESTAMP_BASE = 1;
    static constexpr memref_tid_t TID_BASE = 1;
    static constexpr int NUM_INPUTS = 2;
    static constexpr int NUM_OUTPUTS = 1;
    static constexpr int NUM_INSTRS = 1000;

    // Make some input workloads.
    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = TID_BASE + i;
        inputs[i].push_back(test_util::make_thread(tid));
        inputs[i].push_back(test_util::make_pid(1));
        // Add a timestamp after the PID as required by the scheduler.
        uint64_t cur_timestamp = TIMESTAMP_BASE + i * 10;
        inputs[i].push_back(test_util::make_timestamp(cur_timestamp));
        // Add instruction fetches.
        for (int j = 0; j < NUM_INSTRS; j++) {
            inputs[i].push_back(test_util::make_instr(42 + j * 4));
            inputs[i].push_back(test_util::make_memref(0xaaaaaaaa, TRACE_TYPE_READ));
        }
        inputs[i].push_back(test_util::make_exit(tid));
    }
    std::vector<scheduler_t::input_reader_t> readers;
    for (int i = 0; i < NUM_INPUTS; i++) {
        readers.emplace_back(
            std::unique_ptr<test_util::mock_reader_t>(
                new test_util::mock_reader_t(inputs[i])),
            std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()),
            TID_BASE + i);
    }

    // Create a noise generator.
    noise_generator_info_t noise_generator_info;
    noise_generator_info.pid = TID_BASE + NUM_INPUTS;
    noise_generator_info.tid = TID_BASE + NUM_INPUTS;
    noise_generator_info.num_records_to_generate = NUM_INSTRS;
    mock_noise_generator_factory_t noise_generator_factory(ADDR_TO_GENERATE);
    scheduler_t::input_reader_t noise_generator_reader =
        noise_generator_factory.create_noise_generator(noise_generator_info);
    // Check for errors.
    assert(noise_generator_factory.get_error_string().empty());
    // Add the noise generator to a separate input_reader_t vector like we do in an
    // analyzer.
    std::vector<scheduler_t::input_reader_t> noise_generator_readers;
    noise_generator_readers.emplace_back(std::move(noise_generator_reader));

    // Add input workloads and noise to the inputs to schedule.
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    // Input ordinal 0.
    sched_inputs.emplace_back(std::move(readers));
    // Input ordinal 1.
    sched_inputs.emplace_back(std::move(noise_generator_readers));

    // Create custom scheduler options.
    // MAP_TO_ANY_OUTPUT selects dynamic scheduling, which is what we currently support
    // for the noise generator.
    scheduler_t::scheduler_options_t sched_ops(
        scheduler_t::MAP_TO_ANY_OUTPUT, scheduler_t::DEPENDENCY_IGNORE,
        scheduler_t::SCHEDULER_DEFAULTS, /*verbose=*/4);
    // This is the default quantum_unit, but we specify it anyway in case it changes in
    // the future.
    sched_ops.quantum_unit = scheduler_t::QUANTUM_INSTRUCTIONS;
    // We shorten quantum_duration_instrs from the default since we only generate 1000
    // instructions. This is needed to have the scheduler interleave the input workloads
    // and the noise together in the same output. The default value is too large and
    // allows all records of each input to be scheduled together in a single time quant,
    // so there is no interleaving of records.
    sched_ops.quantum_duration_instrs = 5;

    // Initialize the scheduler.
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS) {
        assert(false);
    }

    memref_t memref;
    bool found_at_least_one_noise_generator_read = false;
    // We only have a single output.
    auto *stream = scheduler.get_stream(0);
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
        // Check that all read records generated by the noise generator have address
        // ADDR_TO_GENERATE.
        if (stream->get_input_workload_ordinal() == 1) {
            if (memref.data.type == TRACE_TYPE_READ) {
                assert(memref.data.addr == static_cast<addr_t>(ADDR_TO_GENERATE));
                found_at_least_one_noise_generator_read = true;
            }
        }
    }
    assert(found_at_least_one_noise_generator_read);
}

int
test_main(int argc, const char *argv[])
{
    // Takes in a path to the tests/ src dir.
    assert(argc == 2);
    // Avoid races with lazy drdecode init (b/279350357).
    dr_standalone_init();

    test_serial();
    test_parallel();
    test_param_checks();
    test_regions();
    test_only_threads();
    test_real_file_queries_and_filters(argv[1]);
    test_synthetic();
    test_synthetic_with_syscall_seq();
    test_synthetic_time_quanta();
    test_synthetic_with_timestamps();
    test_synthetic_with_priorities();
    test_synthetic_with_bindings();
    test_synthetic_with_syscalls();
    test_synthetic_multi_threaded(argv[1]);
    test_synthetic_with_output_limit();
    test_speculation();
    test_replay();
    test_replay_multi_threaded(argv[1]);
    test_replay_timestamps();
    test_replay_noeof();
    test_replay_skip();
    test_replay_limit();
    test_replay_as_traced_from_file(argv[1]);
    test_replay_as_traced();
    test_replay_as_traced_i6107_workaround();
    test_replay_as_traced_dup_start();
    test_replay_as_traced_sort();
    test_times_of_interest();
    test_inactive();
    test_direct_switch();
    test_unscheduled();
    test_kernel_switch_sequences();
    test_kernel_syscall_sequences();
    test_random_schedule();
    test_record_scheduler();
    test_record_scheduler_i7574();
    test_rebalancing();
    test_initial_migrate();
    test_exit_early();
    test_marker_updates();
    test_options_match();
    test_noise_generator();

    dr_standalone_exit();
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
