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
#undef NDEBUG
#include <assert.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#include "dr_api.h"
#include "scheduler.h"
#include "mock_reader.h"
#ifdef HAS_ZIP
#    include "zipfile_istream.h"
#    include "zipfile_ostream.h"
#endif

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
test_serial()
{
    std::cerr << "\n----------------\nTesting serial\n";
    static constexpr memref_tid_t TID_A = 42;
    static constexpr memref_tid_t TID_B = 99;
    std::vector<trace_entry_t> refs_A = {
        /* clang-format off */
        make_thread(TID_A),
        make_pid(1),
        // Include a header to test the scheduler queuing it.
        make_version(4),
        // Each timestamp is followed by an instr whose PC==time.
        make_timestamp(10),
        make_instr(10),
        make_timestamp(30),
        make_instr(30),
        make_timestamp(50),
        make_instr(50),
        make_exit(TID_A),
        /* clang-format on */
    };
    std::vector<trace_entry_t> refs_B = {
        /* clang-format off */
        make_thread(TID_B),
        make_pid(1),
        make_version(4),
        make_timestamp(20),
        make_instr(20),
        make_timestamp(40),
        make_instr(40),
        make_timestamp(60),
        make_instr(60),
        make_exit(TID_B),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(refs_A)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_A);
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(refs_B)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_B);
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
        make_thread(1),
        make_pid(1),
        make_instr(42),
        make_exit(1),
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
        readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs[i])),
                             std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
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
            assert(
                stream->get_record_ordinal() ==
                scheduler.get_input_stream_interface(stream->get_input_stream_ordinal())
                    ->get_record_ordinal());
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
test_param_checks()
{
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t()),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), 1);
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

// Tests regions without timestamps for a simple, direct test.
static void
test_regions_bare()
{
    std::cerr << "\n----------------\nTesting bare regions\n";
    std::vector<trace_entry_t> memrefs = {
        /* clang-format off */
        make_thread(1),
        make_pid(1),
        make_marker(TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
        make_instr(1),
        make_instr(2), // Region 1 is just this instr.
        make_instr(3),
        make_instr(4), // Region 2 is just this instr.
        make_instr(5), // Region 3 is just this instr.
        make_instr(6),
        make_instr(7),
        make_instr(8), // Region 4 starts here.
        make_instr(9), // Region 4 ends here.
        make_instr(10),
        make_exit(1),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(memrefs)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), 1);

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
        make_thread(1),
        make_pid(1),
        // This would not happen in a real trace, only in tests.  But it does
        // match a dynamic skip from the middle when an instruction has already
        // been read but not yet passed to the output stream.
        make_instr(1),
        make_instr(2), // The region skips the 1st instr.
        make_instr(3),
        make_instr(4),
        make_exit(1),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(memrefs)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), 1);

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
        make_thread(1),
        make_pid(1),
        make_marker(TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
        make_timestamp(10),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 1),
        make_instr(1),
        make_instr(2), // Region 1 is just this instr.
        make_instr(3),
        make_timestamp(20),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 2),
        make_timestamp(30),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 3),
        make_instr(4),
        make_timestamp(40),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 4),
        make_instr(5),
        make_instr(6), // Region 2 starts here.
        make_timestamp(50),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 5),
        make_instr(7), // Region 2 ends here.
        make_instr(8),
        make_exit(1),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(memrefs)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), 1);

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
        make_thread(1),
        make_pid(1),
        make_marker(TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
        make_timestamp(10),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 1),
        make_instr(1), // Region 1 starts at the start.
        make_instr(2),
        make_exit(1),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(memrefs)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), 1);

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
        make_thread(1),
        make_pid(1),
        make_marker(TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
        make_timestamp(10),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 1),
        make_instr(1),
        make_instr(2),
        make_exit(1),
        make_footer(),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(memrefs)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), 1);

    std::vector<scheduler_t::range_t> regions;
    // Start beyond the last instruction.
    regions.emplace_back(3, 0);

    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    sched_inputs[0].thread_modifiers.push_back(scheduler_t::input_thread_info_t(regions));
    if (scheduler.init(sched_inputs, 1,
                       scheduler_t::make_scheduler_serial_options(/*verbosity=*/4)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    auto *stream = scheduler.get_stream(0);
    memref_t memref;
    scheduler_t::stream_status_t status = stream->next_record(memref);
    assert(status == scheduler_t::STATUS_REGION_INVALID);
}

static void
test_regions()
{
    test_regions_timestamps();
    test_regions_bare();
    test_regions_bare_no_marker();
    test_regions_start();
    test_regions_too_far();
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
        make_thread(TID_A),
        make_pid(1),
        make_instr(50),
        make_exit(TID_A),
    };
    std::vector<trace_entry_t> refs_B = {
        make_thread(TID_B),
        make_pid(1),
        make_instr(60),
        make_exit(TID_B),
    };
    std::vector<trace_entry_t> refs_C = {
        make_thread(TID_C),
        make_pid(1),
        make_instr(60),
        make_exit(TID_C),
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(refs_A)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_A);
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(refs_B)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_B);
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(refs_C)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_C);

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
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
        assert(memref.instr.tid == TID_B);
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
    static constexpr memref_tid_t TID_2_A = 1257604;
    static constexpr memref_tid_t TID_2_B = 1257602;
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
               "drmemtrace.threadsig.1257604.1983.trace.zip" ||
           scheduler.get_input_stream_name(1) ==
               "drmemtrace.threadsig.1257602.1021.trace.zip");
    assert(scheduler.get_input_stream_name(2) ==
               "drmemtrace.threadsig.1257604.1983.trace.zip" ||
           scheduler.get_input_stream_name(2) ==
               "drmemtrace.threadsig.1257602.1021.trace.zip");
    // Ensure all tids were seen.
    assert(tids_seen.size() == 3);
    assert(tids_seen.find(TID_1_A) != tids_seen.end() &&
           tids_seen.find(TID_2_A) != tids_seen.end() &&
           tids_seen.find(TID_2_B) != tids_seen.end());
#endif
}

// Returns a string with one char per input.
// Assumes the input threads are all tid_base plus an offset < 26.
static std::vector<std::string>
run_lockstep_simulation(scheduler_t &scheduler, int num_outputs, memref_tid_t tid_base,
                        bool send_time = false, bool print_markers = true)
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
    static constexpr char THREAD_LETTER_START = 'A';
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
            if (type_is_instr(memref.instr.type)) {
                sched_as_string[i] +=
                    THREAD_LETTER_START + static_cast<char>(memref.instr.tid - tid_base);
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
test_synthetic()
{
    std::cerr << "\n----------------\nTesting synthetic\n";
    static constexpr int NUM_INPUTS = 7;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int NUM_INSTRS = 9;
    static constexpr int QUANTUM_DURATION = 3;
    // We do not want to block for very long.
    static constexpr double BLOCK_SCALE = 0.01;
    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = TID_BASE + i;
        inputs[i].push_back(make_thread(tid));
        inputs[i].push_back(make_pid(1));
        inputs[i].push_back(make_version(TRACE_ENTRY_VERSION));
        inputs[i].push_back(make_timestamp(10)); // All the same time priority.
        for (int j = 0; j < NUM_INSTRS; j++) {
            inputs[i].push_back(make_instr(42 + j * 4));
            // Test accumulation of usage across voluntary switches.
            if ((i == 0 || i == 1) && j == 1) {
                inputs[i].push_back(make_timestamp(20));
                inputs[i].push_back(make_marker(TRACE_MARKER_TYPE_SYSCALL, 42));
                inputs[i].push_back(
                    make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0));
                inputs[i].push_back(make_timestamp(120));
            }
        }
        inputs[i].push_back(make_exit(tid));
    }
    // Hardcoding here for the 2 outputs and 7 inputs.
    // We expect 3 letter sequences (our quantum) alternating every-other as each
    // core alternates. The dots are markers and thread exits.
    // A and B have a voluntary switch after their 1st 2 letters, but we expect
    // the usage to persist to their next scheduling which should only have
    // a single letter.
    static const char *const CORE0_SCHED_STRING =
        "..AA......CCC..EEE..GGGDDDFFFBBBCCC.EEE.AAA.GGG.";
    static const char *const CORE1_SCHED_STRING =
        "..BB......DDD..FFFABCCCEEEAAAGGGDDD.FFF.BBB.____";
    {
        // Test instruction quanta.
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        for (int i = 0; i < NUM_INPUTS; i++) {
            std::vector<scheduler_t::input_reader_t> readers;
            readers.emplace_back(
                std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs[i])),
                std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_BASE + i);
            sched_inputs.emplace_back(std::move(readers));
        }
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        sched_ops.quantum_duration = QUANTUM_DURATION;
        sched_ops.block_time_scale = BLOCK_SCALE;
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
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
                std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs[i])),
                std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_BASE + i);
            sched_inputs.emplace_back(std::move(readers));
        }
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.quantum_duration = QUANTUM_DURATION;
        sched_ops.block_time_scale = BLOCK_SCALE;
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
    static constexpr int PRE_BLOCK_TIME = 20;
    static constexpr int POST_BLOCK_TIME = 220;
    std::vector<trace_entry_t> refs[NUM_INPUTS];
    for (int i = 0; i < NUM_INPUTS; ++i) {
        refs[i].push_back(make_thread(TID_BASE + i));
        refs[i].push_back(make_pid(1));
        refs[i].push_back(make_version(TRACE_ENTRY_VERSION));
        refs[i].push_back(make_timestamp(10));
        refs[i].push_back(make_instr(10));
        refs[i].push_back(make_instr(30));
        if (i == 0) {
            refs[i].push_back(make_timestamp(PRE_BLOCK_TIME));
            refs[i].push_back(make_marker(TRACE_MARKER_TYPE_SYSCALL, 42));
            refs[i].push_back(make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0));
            refs[i].push_back(make_timestamp(POST_BLOCK_TIME));
        }
        refs[i].push_back(make_instr(50));
        refs[i].push_back(make_exit(TID_BASE + i));
    }
    std::string record_fname = "tmp_test_replay_time.zip";
    {
        // Record.
        std::vector<scheduler_t::input_reader_t> readers;
        for (int i = 0; i < NUM_INPUTS; ++i) {
            readers.emplace_back(
                std::unique_ptr<mock_reader_t>(new mock_reader_t(refs[i])),
                std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_BASE + i);
        }
        scheduler_t scheduler;
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/4);
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
        sched_ops.quantum_duration = 3;
        // Ensure it waits 10 steps.
        sched_ops.block_time_scale = 10. / (POST_BLOCK_TIME - PRE_BLOCK_TIME);
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
        check_next(cpu1, ++time, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_INSTR);
        check_next(cpu1, time, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_MARKER);
        check_next(cpu1, time, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_MARKER);
        check_next(cpu1, time, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_MARKER);
        check_next(cpu1, time, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_MARKER);
        // We just hit a blocking syscall in A so we swap to B.
        check_next(cpu1, ++time, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
        // This is another quantum end at 9 but no other input is available.
        check_next(cpu1, ++time, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
        check_next(cpu1, time, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_THREAD_EXIT);
        check_next(cpu1, ++time, scheduler_t::STATUS_IDLE);
        // Finish off C on cpu 0.
        check_next(cpu0, ++time, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_INSTR);
        check_next(cpu0, ++time, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_INSTR);
        check_next(cpu0, time, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_THREAD_EXIT);
        // Both cpus wait until A is unblocked.
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
    }
    {
        // Replay.
        std::vector<scheduler_t::input_reader_t> readers;
        for (int i = 0; i < NUM_INPUTS; ++i) {
            readers.emplace_back(
                std::unique_ptr<mock_reader_t>(new mock_reader_t(refs[i])),
                std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_BASE + i);
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
        assert(sched_as_string[1].substr(0, 12) == "..BA....BB._");
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
            inputs.push_back(make_thread(tid));
            inputs.push_back(make_pid(1));
            for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
                // Sprinkle timestamps every other instruction.
                if (instr_idx % 2 == 0) {
                    // We have different base timestamps per workload, and we have the
                    // later-ordered inputs in each with the earlier timestamps to
                    // better test scheduler ordering.
                    inputs.push_back(make_timestamp(
                        1000 * workload_idx +
                        100 * (NUM_INPUTS_PER_WORKLOAD - input_idx) + 10 * instr_idx));
                }
                inputs.push_back(make_instr(42 + instr_idx * 4));
            }
            inputs.push_back(make_exit(tid));
            readers.emplace_back(
                std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs)),
                std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
        }
        sched_inputs.emplace_back(std::move(readers));
    }
    // We have one input with lower timestamps than everyone, to
    // test that it never gets switched out.
    memref_tid_t tid = TID_BASE + NUM_WORKLOADS * NUM_INPUTS_PER_WORKLOAD;
    std::vector<trace_entry_t> inputs;
    inputs.push_back(make_thread(tid));
    inputs.push_back(make_pid(1));
    for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
        if (instr_idx % 2 == 0)
            inputs.push_back(make_timestamp(1 + instr_idx));
        inputs.push_back(make_instr(42 + instr_idx * 4));
    }
    inputs.push_back(make_exit(tid));
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
    sched_inputs.emplace_back(std::move(readers));

    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/3);
    sched_ops.quantum_duration = 3;
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
    // with {A,D,G}.  We should interleave within each group -- except once we reach J
    // we should completely finish it.
    assert(
        sched_as_string[0] ==
        ".CC.C.II.IC.CC.F.FF.I.II.FF.F..BB.B.HH.HE.EE.BB.B.HH.H..DD.DA.AA.G.GG.DD.D._");
    assert(sched_as_string[1] ==
           ".FF.F.JJ.JJ.JJ.JJ.J.CC.C.II.I..EE.EB.BB.H.HH.EE.E..AA.A.GG.GD.DD.AA.A.GG.G.");
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
            inputs.push_back(make_thread(tid));
            inputs.push_back(make_pid(1));
            for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
                // Sprinkle timestamps every other instruction.
                if (instr_idx % 2 == 0) {
                    // We have different base timestamps per workload, and we have the
                    // later-ordered inputs in each with the earlier timestamps to
                    // better test scheduler ordering.
                    inputs.push_back(make_timestamp(
                        1000 * workload_idx +
                        100 * (NUM_INPUTS_PER_WORKLOAD - input_idx) + 10 * instr_idx));
                }
                inputs.push_back(make_instr(42 + instr_idx * 4));
            }
            inputs.push_back(make_exit(tid));
            readers.emplace_back(
                std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs)),
                std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
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
    inputs.push_back(make_thread(tid));
    inputs.push_back(make_pid(1));
    for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
        if (instr_idx % 2 == 0)
            inputs.push_back(make_timestamp(1 + instr_idx));
        inputs.push_back(make_instr(42 + instr_idx * 4));
    }
    inputs.push_back(make_exit(tid));
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
    sched_inputs.emplace_back(std::move(readers));

    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/3);
    sched_ops.quantum_duration = 3;
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
    assert(
        sched_as_string[0] ==
        ".BB.B.HH.HE.EE.BB.B.HH.H..FF.F.JJ.JJ.JJ.JJ.J.CC.C.II.I..DD.DA.AA.G.GG.DD.D._");
    assert(sched_as_string[1] ==
           ".EE.EB.BB.H.HH.EE.E..CC.C.II.IC.CC.F.FF.I.II.FF.F..AA.A.GG.GD.DD.AA.A.GG.G.");
}

static void
test_synthetic_with_bindings()
{
    std::cerr << "\n----------------\nTesting synthetic with bindings\n";
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
            inputs.push_back(make_thread(tid));
            inputs.push_back(make_pid(1));
            for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
                // Include timestamps but keep each workload with the same time to
                // avoid complicating the test.
                if (instr_idx % 2 == 0) {
                    inputs.push_back(make_timestamp(10 * (instr_idx + 1)));
                }
                inputs.push_back(make_instr(42 + instr_idx * 4));
            }
            inputs.push_back(make_exit(tid));
            readers.emplace_back(
                std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs)),
                std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
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
    sched_ops.quantum_duration = 3;
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
    assert(sched_as_string[0] == ".DD.D.FF.FD.DD.F.FF.DD.D.FF.F._");
    assert(sched_as_string[1] == ".EE.E.HH.HE.EE.I.II.EE.E.______");
    assert(sched_as_string[2] == ".AA.A.CC.CG.GG.C.CC.HH.H.CC.C.");
    assert(sched_as_string[3] == ".GG.G.II.IH.HH.GG.G.II.I._____");
    assert(sched_as_string[4] == ".BB.BA.AA.B.BB.AA.A.BB.B._____");
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
            inputs.push_back(make_thread(tid));
            inputs.push_back(make_pid(1));
            for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
                // Use the same inverted timestamps as test_synthetic_with_timestamps()
                // to cover different code paths; in particular it has a case where
                // the last entry in the queue is the only one that fits on an output.
                if (instr_idx % 2 == 0) {
                    inputs.push_back(make_timestamp(
                        1000 * workload_idx +
                        100 * (NUM_INPUTS_PER_WORKLOAD - input_idx) + 10 * instr_idx));
                }
                inputs.push_back(make_instr(42 + instr_idx * 4));
            }
            inputs.push_back(make_exit(tid));
            readers.emplace_back(
                std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs)),
                std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
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
    sched_ops.quantum_duration = 3;
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
    assert(sched_as_string[0] == ".FF.FF.FF.FF.F..EE.EE.EE.EE.E._");
    assert(sched_as_string[1] == ".II.II.II.II.I..DD.DD.DD.DD.D._");
    assert(sched_as_string[2] == ".CC.CC.CC.CC.C..AA.AA.AA.AA.A._");
    assert(sched_as_string[3] == ".HH.HH.HH.HH.H..GG.GG.GG.GG.G.");
    assert(sched_as_string[4] == ".BB.BB.BB.BB.B._______________");
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
            inputs.push_back(make_thread(tid));
            inputs.push_back(make_pid(1));
            inputs.push_back(make_version(TRACE_ENTRY_VERSION));
            uint64_t stamp =
                10000 * workload_idx + 1000 * (NUM_INPUTS_PER_WORKLOAD - input_idx);
            for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
                // Sprinkle timestamps every other instruction.  We use a similar
                // priority scheme as test_synthetic_with_priorities() but we leave
                // room for blocking syscall timestamp gaps.
                if (instr_idx % 2 == 0 &&
                    (inputs.back().type != TRACE_TYPE_MARKER ||
                     inputs.back().size != TRACE_MARKER_TYPE_TIMESTAMP)) {
                    inputs.push_back(make_timestamp(stamp));
                }
                inputs.push_back(make_instr(42 + instr_idx * 4));
                // Insert some blocking syscalls in the high-priority (see below)
                // middle threads.
                if (input_idx == 1 && instr_idx % (workload_idx + 1) == workload_idx) {
                    inputs.push_back(make_timestamp(stamp + 10));
                    inputs.push_back(make_marker(TRACE_MARKER_TYPE_SYSCALL, 42));
                    inputs.push_back(
                        make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0));
                    // Blocked for 10 time units with our BLOCK_SCALE.
                    inputs.push_back(make_timestamp(stamp + 10 + 10 * BLOCK_LATENCY));
                } else {
                    // Insert meta records to keep the locksteps lined up.
                    inputs.push_back(make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
                    inputs.push_back(make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
                    inputs.push_back(make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
                    inputs.push_back(make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
                }
                stamp += 10;
            }
            inputs.push_back(make_exit(tid));
            readers.emplace_back(
                std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs)),
                std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
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
    inputs.push_back(make_thread(tid));
    inputs.push_back(make_pid(1));
    for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
        if (instr_idx % 2 == 0)
            inputs.push_back(make_timestamp(1 + instr_idx));
        inputs.push_back(make_instr(42 + instr_idx * 4));
    }
    inputs.push_back(make_exit(tid));
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
    sched_inputs.emplace_back(std::move(readers));

    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/3);
    sched_ops.quantum_duration = 3;
    // We use our mock's time==instruction count for a deterministic result.
    sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
    sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
    sched_ops.block_time_scale = BLOCK_SCALE;
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
           "BHHHFFFJJJJJJBEEHHHFFFBCCCEEIIIDDDBAAAGGGDDDB________B_______");
    assert(sched_as_string[1] == "EECCCIIICCCBEEJJJHHHIIIFFFEAAAGGGBDDDAAAGGG________B");
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
            inputs.push_back(make_thread(tid));
            inputs.push_back(make_pid(1));
            inputs.push_back(make_version(TRACE_ENTRY_VERSION));
            uint64_t stamp =
                10000 * workload_idx + 1000 * (NUM_INPUTS_PER_WORKLOAD - input_idx);
            for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
                // Sprinkle timestamps every other instruction.  We use a similar
                // priority scheme as test_synthetic_with_priorities() but we leave
                // room for blocking syscall timestamp gaps.
                if (instr_idx % 2 == 0 &&
                    (inputs.back().type != TRACE_TYPE_MARKER ||
                     inputs.back().size != TRACE_MARKER_TYPE_TIMESTAMP)) {
                    inputs.push_back(make_timestamp(stamp));
                }
                inputs.push_back(make_instr(42 + instr_idx * 4));
                // Insert some blocking syscalls.
                if (instr_idx % 3 == 1) {
                    inputs.push_back(make_timestamp(stamp + 10));
                    inputs.push_back(make_marker(TRACE_MARKER_TYPE_SYSCALL, 42));
                    inputs.push_back(
                        make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0));
                    // Blocked for 3 time units.
                    inputs.push_back(make_timestamp(stamp + 10 + 3 * BLOCK_LATENCY));
                } else {
                    // Insert meta records to keep the locksteps lined up.
                    inputs.push_back(make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
                    inputs.push_back(make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
                    inputs.push_back(make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
                    inputs.push_back(make_marker(TRACE_MARKER_TYPE_CPU_ID, 0));
                }
                stamp += 10;
            }
            inputs.push_back(make_exit(tid));
            readers.emplace_back(
                std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs)),
                std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
        }
        sched_inputs.emplace_back(std::move(readers));
    }
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/4);
    sched_ops.quantum_duration = 3;
    // We use our mock's time==instruction count for a deterministic result.
    sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
    sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
    sched_ops.block_time_scale = BLOCK_SCALE;
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
    assert(sched_as_string[0] == "..A....A....___________________________________A.....");
    assert(sched_as_string[1] == "____________A....A.....A....__A.....A....A...._______");
}

static bool
check_ref(std::vector<memref_t> &refs, int &idx, memref_tid_t expected_tid,
          trace_type_t expected_type,
          trace_marker_type_t expected_marker = TRACE_MARKER_TYPE_RESERVED_END,
          uintptr_t expected_marker_value = 0)
{
    if (expected_tid != refs[idx].instr.tid || expected_type != refs[idx].instr.type) {
        std::cerr << "Record " << idx << " has tid " << refs[idx].instr.tid
                  << " and type " << refs[idx].instr.type << " != expected tid "
                  << expected_tid << " and expected type " << expected_type << "\n";
        return false;
    }
    if (expected_type == TRACE_TYPE_MARKER) {
        if (expected_marker != refs[idx].marker.marker_type) {
            std::cerr << "Record " << idx << " has marker type "
                      << refs[idx].marker.marker_type << " but expected "
                      << expected_marker << "\n";
            return false;
        }
        if (expected_marker_value != 0 &&
            expected_marker_value != refs[idx].marker.marker_value) {
            std::cerr << "Record " << idx << " has marker value "
                      << refs[idx].marker.marker_value << " but expected "
                      << expected_marker_value << "\n";
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
    std::vector<trace_entry_t> refs_A = {
        /* clang-format off */
        make_thread(TID_A),
        make_pid(1),
        make_version(TRACE_ENTRY_VERSION),
        make_timestamp(20),
        make_instr(10),
        make_timestamp(120),
        make_marker(TRACE_MARKER_TYPE_SYSCALL, SYSNUM),
        make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        make_marker(TRACE_MARKER_TYPE_FUNC_ID, 100),
        make_marker(TRACE_MARKER_TYPE_FUNC_ARG, 42),
        make_timestamp(250),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 1),
        make_marker(TRACE_MARKER_TYPE_FUNC_ID, 100),
        make_marker(TRACE_MARKER_TYPE_FUNC_RETVAL, 0),
        make_instr(12),
        make_exit(TID_A),
        /* clang-format on */
    };
    std::vector<trace_entry_t> refs_B = {
        /* clang-format off */
        make_thread(TID_B),
        make_pid(1),
        make_version(TRACE_ENTRY_VERSION),
        make_timestamp(120),
        make_instr(20),
        make_instr(21),
        make_exit(TID_B),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(refs_A)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_A);
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(refs_B)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_B);
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/4);
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
        make_thread(TID_A),
        make_pid(1),
        make_version(TRACE_ENTRY_VERSION),
        make_timestamp(20),
        make_instr(10),
        // Test 0 latency.
        make_timestamp(120),
        make_marker(TRACE_MARKER_TYPE_SYSCALL, SYSNUM),
        make_timestamp(120),
        make_instr(10),
        // Test large but too-short latency.
        make_timestamp(200),
        make_marker(TRACE_MARKER_TYPE_SYSCALL, SYSNUM),
        make_timestamp(699),
        make_instr(10),
        // Test just large enough latency, with func markers in between.
        make_timestamp(1000),
        make_marker(TRACE_MARKER_TYPE_SYSCALL, SYSNUM),
        make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        make_marker(TRACE_MARKER_TYPE_FUNC_ID, 100),
        make_marker(TRACE_MARKER_TYPE_FUNC_ARG, 42),
        make_timestamp(1000 + BLOCK_LATENCY),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 1),
        make_marker(TRACE_MARKER_TYPE_FUNC_ID, 100),
        make_marker(TRACE_MARKER_TYPE_FUNC_RETVAL, 0),
        make_instr(12),
        make_exit(TID_A),
        /* clang-format on */
    };
    std::vector<trace_entry_t> refs_B = {
        /* clang-format off */
        make_thread(TID_B),
        make_pid(1),
        make_version(TRACE_ENTRY_VERSION),
        make_timestamp(2000),
        make_instr(20),
        make_instr(21),
        make_exit(TID_B),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(refs_A)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_A);
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(refs_B)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_B);
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/4);
    // We use a mock time for a deterministic result.
    sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
    sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
    sched_ops.block_time_scale = BLOCK_SCALE;
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
        inputs.push_back(make_thread(tid));
        inputs.push_back(make_pid(1));
        inputs.push_back(make_version(TRACE_ENTRY_VERSION));
        uint64_t stamp = 10000 * NUM_INPUTS;
        inputs.push_back(make_timestamp(stamp));
        for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
            inputs.push_back(make_instr(42 + instr_idx * 4));
            if (instr_idx == 1) {
                // Insert a blocking syscall in one input.
                if (input_idx == 0) {
                    inputs.push_back(make_timestamp(stamp + 10));
                    inputs.push_back(make_marker(TRACE_MARKER_TYPE_SYSCALL, 42));
                    inputs.push_back(
                        make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0));
                    // Blocked for BLOCK_UNITS time units with BLOCK_SCALE, but
                    // after each queue rejection it should go to the back of
                    // the queue and all the other inputs should be selected
                    // before another retry.
                    inputs.push_back(
                        make_timestamp(stamp + 10 + BLOCK_UNITS * BLOCK_LATENCY));
                } else {
                    // Insert a timestamp to match the blocked input so the inputs
                    // are all at equal priority in the queue.
                    inputs.push_back(
                        make_timestamp(stamp + 10 + BLOCK_UNITS * BLOCK_LATENCY));
                }
            }
        }
        inputs.push_back(make_exit(tid));
        readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs)),
                             std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
    }
    sched_inputs.emplace_back(std::move(readers));
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/3);
    sched_ops.quantum_duration = 3;
    // We use a mock time for a deterministic result.
    sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
    sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
    sched_ops.block_time_scale = BLOCK_SCALE;
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
    sched_ops.quantum_duration = QUANTUM_DURATION;
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
test_speculation()
{
    std::cerr << "\n----------------\nTesting speculation\n";
    std::vector<trace_entry_t> memrefs = {
        /* clang-format off */
        make_thread(1),
        make_pid(1),
        make_marker(TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
        make_timestamp(10),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 1),
        // Conditional branch.
        make_instr(1, TRACE_TYPE_INSTR_CONDITIONAL_JUMP),
        // It fell through in the trace.
        make_instr(2),
        // Another conditional branch.
        make_instr(3, TRACE_TYPE_INSTR_CONDITIONAL_JUMP),
        // It fell through in the trace.
        make_instr(4),
        make_instr(5),
        make_exit(1),
        /* clang-format on */
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(memrefs)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), 1);

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
            stream->start_speculation(100, true);
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
            stream->stop_speculation();
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
            stream->start_speculation(200, false);
            break;
        case 10:
            // We should now see nops from the speculator.
            assert(type_is_instr(memref.instr.type));
            assert(memref_is_nop_instr(memref));
            assert(memref.instr.addr == 200);
            // Test a nested start_speculation().
            stream->start_speculation(300, false);
            // Ensure unread_last_record() fails during nested speculation.
            assert(stream->unread_last_record() == scheduler_t::STATUS_INVALID);
            break;
        case 11:
            assert(type_is_instr(memref.instr.type));
            assert(memref_is_nop_instr(memref));
            assert(memref.instr.addr == 300);
            stream->stop_speculation();
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
            stream->start_speculation(400, true);
            break;
        case 13:
            assert(type_is_instr(memref.instr.type));
            assert(memref_is_nop_instr(memref));
            assert(memref.instr.addr == 400);
            stream->stop_speculation();
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
            stream->stop_speculation();
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
    // We expect 3 letter sequences (our quantum) alternating every-other as each
    // core alternates; with an odd number the 2nd core finishes early.
    static const char *const CORE0_SCHED_STRING = "AAACCCEEEGGGBBBDDDFFFAAA.CCC.EEE.GGG.";
    static const char *const CORE1_SCHED_STRING = "BBBDDDFFFAAACCCEEEGGGBBB.DDD.FFF.____";

    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = TID_BASE + i;
        inputs[i].push_back(make_thread(tid));
        inputs[i].push_back(make_pid(1));
        for (int j = 0; j < NUM_INSTRS; j++)
            inputs[i].push_back(make_instr(42 + j * 4));
        inputs[i].push_back(make_exit(tid));
    }
    std::string record_fname = "tmp_test_replay_record.zip";

    // Record.
    {
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        for (int i = 0; i < NUM_INPUTS; i++) {
            memref_tid_t tid = TID_BASE + i;
            std::vector<scheduler_t::input_reader_t> readers;
            readers.emplace_back(
                std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs[i])),
                std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
            sched_inputs.emplace_back(std::move(readers));
        }
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/3);
        sched_ops.quantum_duration = QUANTUM_INSTRS;

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
    // Now replay the schedule several times to ensure repeatability.
    for (int outer = 0; outer < 5; ++outer) {
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        for (int i = 0; i < NUM_INPUTS; i++) {
            memref_tid_t tid = TID_BASE + i;
            std::vector<scheduler_t::input_reader_t> readers;
            readers.emplace_back(
                std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs[i])),
                std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
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
        sched_ops.quantum_duration = QUANTUM_DURATION;
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
// We subclass scheduler_t to access its record struct and functions.
class test_scheduler_t : public scheduler_t {
public:
    void
    write_test_schedule(std::string record_fname)
    {
        // This is hardcoded for 4 inputs and 2 outputs and a 3-instruction
        // scheduling quantum.
        // The 1st output's consumer was very slow and only managed 2 segments.
        scheduler_t scheduler;
        std::vector<schedule_record_t> sched0;
        sched0.emplace_back(scheduler_t::schedule_record_t::VERSION, 0, 0, 0, 0);
        sched0.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 0, 0, 4, 11);
        // There is a huge time gap here.
        sched0.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 2, 7,
                            0xffffffffffffffffUL, 91);
        sched0.emplace_back(scheduler_t::schedule_record_t::FOOTER, 0, 0, 0, 0);
        std::vector<schedule_record_t> sched1;
        sched1.emplace_back(scheduler_t::schedule_record_t::VERSION, 0, 0, 0, 0);
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 1, 0, 4, 10);
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 2, 0, 4, 20);
        // Input 2 advances early so core 0 is no longer waiting on it but only
        // the timestamp.
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 2, 4, 7, 60);
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 3, 0, 4, 30);
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 0, 4, 7, 40);
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 1, 4, 7, 50);
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 3, 4, 7, 70);
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 0, 7,
                            0xffffffffffffffffUL, 80);
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 1, 7,
                            0xffffffffffffffffUL, 90);
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 3, 7,
                            0xffffffffffffffffUL, 110);
        sched1.emplace_back(scheduler_t::schedule_record_t::FOOTER, 0, 0, 0, 0);
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
        inputs[i].push_back(make_thread(tid));
        inputs[i].push_back(make_pid(1));
        // We need a timestamp so the scheduler will find one for initial
        // input processing.  We do not try to duplicate the timestamp
        // sequences in the stored file and just use a dummy timestamp here.
        inputs[i].push_back(make_timestamp(10 + i));
        for (int j = 0; j < NUM_INSTRS; j++)
            inputs[i].push_back(make_instr(42 + j * 4));
        inputs[i].push_back(make_exit(tid));
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
        readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs[i])),
                             std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
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
// We subclass scheduler_t to access its record struct and functions.
class test_noeof_scheduler_t : public scheduler_t {
public:
    void
    write_test_schedule(std::string record_fname)
    {
        // We duplicate test_scheduler_t but we have one input ending early before
        // eof.
        scheduler_t scheduler;
        std::vector<schedule_record_t> sched0;
        sched0.emplace_back(scheduler_t::schedule_record_t::VERSION, 0, 0, 0, 0);
        sched0.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 0, 0, 4, 11);
        // There is a huge time gap here.
        // Max numeric value means continue until EOF.
        sched0.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 2, 7,
                            0xffffffffffffffffUL, 91);
        sched0.emplace_back(scheduler_t::schedule_record_t::FOOTER, 0, 0, 0, 0);
        std::vector<schedule_record_t> sched1;
        sched1.emplace_back(scheduler_t::schedule_record_t::VERSION, 0, 0, 0, 0);
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 1, 0, 4, 10);
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 2, 0, 4, 20);
        // Input 2 advances early so core 0 is no longer waiting on it but only
        // the timestamp.
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 2, 4, 7, 60);
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 3, 0, 4, 30);
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 0, 4, 7, 40);
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 1, 4, 7, 50);
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 3, 4, 7, 70);
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 0, 7,
                            0xffffffffffffffffUL, 80);
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 1, 7,
                            0xffffffffffffffffUL, 90);
        // Input 3 never reaches EOF (end is exclusive: so it stops at 8 with the
        // real end at 9).
        sched1.emplace_back(scheduler_t::schedule_record_t::DEFAULT, 3, 7, 9, 110);
        sched1.emplace_back(scheduler_t::schedule_record_t::FOOTER, 0, 0, 0, 0);
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
        inputs[i].push_back(make_thread(tid));
        inputs[i].push_back(make_pid(1));
        // We need a timestamp so the scheduler will find one for initial
        // input processing.  We do not try to duplicate the timestamp
        // sequences in the stored file and just use a dummy timestamp here.
        inputs[i].push_back(make_timestamp(10 + i));
        for (int j = 0; j < NUM_INSTRS; j++)
            inputs[i].push_back(make_instr(42 + j * 4));
        inputs[i].push_back(make_exit(tid));
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
        readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs[i])),
                             std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
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
        make_thread(1),
        make_pid(1),
        make_marker(TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
        make_timestamp(10),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 1),
        make_instr(1),
        make_instr(2), // Region 1 is just this instr.
        make_instr(3),
        make_timestamp(20),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 2),
        make_timestamp(30),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 3),
        make_instr(4),
        make_timestamp(40),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 4),
        make_instr(5),
        make_instr(6), // Region 2 starts here.
        make_timestamp(50),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 5),
        make_instr(7), // Region 2 ends here.
        make_instr(8),
        make_exit(1),
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
        readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(memrefs)),
                             std::unique_ptr<mock_reader_t>(new mock_reader_t()), 1);
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
        // Replay.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(memrefs)),
                             std::unique_ptr<mock_reader_t>(new mock_reader_t()), 1);
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
#endif
}

static void
test_replay_limit()
{
#ifdef HAS_ZIP
    std::cerr << "\n----------------\nTesting replay of ROI-limited inputs\n";

    std::vector<trace_entry_t> input_sequence;
    input_sequence.push_back(make_thread(/*tid=*/1));
    input_sequence.push_back(make_pid(/*pid=*/1));
    input_sequence.push_back(make_marker(TRACE_MARKER_TYPE_PAGE_SIZE, 4096));
    input_sequence.push_back(make_timestamp(10));
    input_sequence.push_back(make_marker(TRACE_MARKER_TYPE_CPU_ID, 1));
    static constexpr int NUM_INSTRS = 1000;
    for (int i = 0; i < NUM_INSTRS; ++i) {
        input_sequence.push_back(make_instr(/*pc=*/i));
    }
    input_sequence.push_back(make_exit(/*tid=*/1));

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
                std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs[i])),
                std::unique_ptr<mock_reader_t>(new mock_reader_t()), BASE_TID + i);
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
                std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs[i])),
                std::unique_ptr<mock_reader_t>(new mock_reader_t()), BASE_TID + i);
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
                std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs[i])),
                std::unique_ptr<mock_reader_t>(new mock_reader_t()), BASE_TID + i);
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
        sched_ops.quantum_duration = NUM_INSTRS / 10;
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
        assert(switches > 4);
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
        inputs[i].push_back(make_thread(tid));
        inputs[i].push_back(make_pid(1));
        // The last input will be earlier than all others. It will execute
        // 3 instrs on each core. This is to test the case when an output
        // begins in the wait state.
        for (int j = 0; j < (i == NUM_INPUTS - 1 ? 6 : NUM_INSTRS); j++)
            inputs[i].push_back(make_instr(42 + j * 4));
        inputs[i].push_back(make_exit(tid));
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
        readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs[i])),
                             std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
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
        inputs[input_idx].push_back(make_thread(tid));
        inputs[input_idx].push_back(make_pid(1));
        for (int step_idx = 0; step_idx <= CHUNK_NUM_INSTRS / SCHED_STEP_INSTRS;
             ++step_idx) {
            inputs[input_idx].push_back(make_timestamp(101 + step_idx));
            for (int instr_idx = 0; instr_idx < SCHED_STEP_INSTRS; ++instr_idx) {
                inputs[input_idx].push_back(make_instr(42 + instr_idx));
            }
        }
        inputs[input_idx].push_back(make_exit(tid));
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
            std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs[input_idx])),
            std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
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
test_replay_as_traced_from_file(const char *testdir)
{
#if (defined(X86_64) || defined(ARM_64)) && defined(HAS_ZIP)
    std::cerr << "\n----------------\nTesting replay as-traced from a file\n";
    std::string path = std::string(testdir) + "/drmemtrace.threadsig.x64.tracedir";
    std::string cpu_file =
        std::string(testdir) + "/drmemtrace.threadsig.x64.tracedir/cpu_schedule.bin.zip";
    // This checked-in trace has 8 threads on 7 cores.  It doesn't have
    // much thread migration but our synthetic test above covers that.
    static const char *const SCHED_STRING =
        "Core #0: 1257598 \nCore #1: 1257603 \nCore #2: 1257601 \n"
        "Core #3: 1257599 => 1257604 @ <366987,87875,13331862029895453> "
        "(<366986,87875,13331862029895453> => <1,0,0>) \n"
        "Core #4: 1257600 \nCore #5: 1257596 \nCore #6: 1257602 \n";
    static constexpr int NUM_OUTPUTS = 7; // Matches the actual trace's core footprint.
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
    std::cerr << "As-traced from file:\n"
              << SCHED_STRING << "Versus replay:\n"
              << replay_string.str();
    assert(replay_string.str() == SCHED_STRING);
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
        make_thread(TID_A),
        make_pid(1),
        make_version(TRACE_ENTRY_VERSION),
        make_timestamp(10),
        make_instr(10),
        make_instr(30),
        make_instr(50),
        make_exit(TID_A),
        /* clang-format on */
    };
    std::vector<trace_entry_t> refs_B = {
        /* clang-format off */
        make_thread(TID_B),
        make_pid(1),
        make_version(TRACE_ENTRY_VERSION),
        make_timestamp(20),
        make_instr(20),
        make_instr(40),
        make_instr(60),
        make_instr(80),
        make_exit(TID_B),
        /* clang-format on */
    };
    std::string record_fname = "tmp_test_replay_inactive.zip";
    {
        // Record.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(refs_A)),
                             std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_A);
        readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(refs_B)),
                             std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_B);
        scheduler_t scheduler;
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(std::move(readers));
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/4);
        sched_ops.quantum_duration = 2;
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
        // Make cpu0 inactive and cpu1 active.
        status = stream0->set_active(false);
        assert(status == scheduler_t::STATUS_OK);
        check_next(stream0, scheduler_t::STATUS_IDLE);
        status = stream1->set_active(true);
        assert(status == scheduler_t::STATUS_OK);
        // Now cpu1 should finish things.
        check_next(stream1, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
        check_next(stream1, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
        check_next(stream1, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_THREAD_EXIT);
        check_next(stream1, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_THREAD_EXIT);
        check_next(stream1, scheduler_t::STATUS_EOF);
        if (scheduler.write_recorded_schedule() != scheduler_t::STATUS_SUCCESS)
            assert(false);
    }
    {
        // Replay.
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(refs_A)),
                             std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_A);
        readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(refs_B)),
                             std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_B);
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
        assert(sched_as_string[0] == "..AABA.__");
        assert(sched_as_string[1] == "..B--BB.");
    }
#endif // HAS_ZIP
}

static void
test_direct_switch()
{
    std::cerr << "\n----------------\nTesting direct switches\n";
    // We have just 1 output to better control the order and avoid flakiness.
    static constexpr int NUM_OUTPUTS = 1;
    static constexpr int QUANTUM_DURATION = 100; // Never reached.
    static constexpr int BLOCK_LATENCY = 100;
    static constexpr double BLOCK_SCALE = 1. / (BLOCK_LATENCY);
    static constexpr memref_tid_t TID_BASE = 100;
    static constexpr memref_tid_t TID_A = TID_BASE + 0;
    static constexpr memref_tid_t TID_B = TID_BASE + 1;
    static constexpr memref_tid_t TID_C = TID_BASE + 2;
    std::vector<trace_entry_t> refs_A = {
        make_thread(TID_A),
        make_pid(1),
        make_version(TRACE_ENTRY_VERSION),
        // A has the earliest timestamp and starts.
        make_timestamp(1001),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        make_instr(/*pc=*/101),
        make_instr(/*pc=*/102),
        make_timestamp(1002),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        make_marker(TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH, TID_C),
        make_timestamp(4001),
        make_instr(/*pc=*/401),
        make_exit(TID_A),
    };
    std::vector<trace_entry_t> refs_B = {
        make_thread(TID_B),
        make_pid(1),
        make_version(TRACE_ENTRY_VERSION),
        // B would go next by timestamp, so this is a good test of direct switches.
        make_timestamp(2001),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        make_instr(/*pc=*/201),
        make_instr(/*pc=*/202),
        make_instr(/*pc=*/203),
        make_instr(/*pc=*/204),
        make_exit(TID_B),
    };
    std::vector<trace_entry_t> refs_C = {
        make_thread(TID_C),
        make_pid(1),
        make_version(TRACE_ENTRY_VERSION),
        make_timestamp(3001),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        make_instr(/*pc=*/301),
        make_instr(/*pc=*/302),
        make_timestamp(3002),
        make_marker(TRACE_MARKER_TYPE_CPU_ID, 0),
        make_marker(TRACE_MARKER_TYPE_SYSCALL, 999),
        make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        make_marker(TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH, TID_A),
        make_timestamp(5001),
        make_instr(/*pc=*/501),
        // Test a non-existent target: should be ignored, but not crash.
        make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        make_marker(TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH, TID_BASE + 3),
        make_exit(TID_C),
    };
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(refs_A)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_A);
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(refs_B)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_B);
    readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(refs_C)),
                         std::unique_ptr<mock_reader_t>(new mock_reader_t()), TID_C);
    // The string constructor writes "." for markers.
    // We expect A's first switch to be to C even though B has an earlier timestamp.
    // We expect C's direct switch to A to proceed immediately even though A still
    // has significant blocked time left.  But then after B is scheduled and finishes,
    // we still have to wait for C's block time so we see idle underscores:
    static const char *const CORE0_SCHED_STRING =
        "...AA.........CC......A....BBBB.______________C...";

    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/3);
    sched_ops.quantum_duration = QUANTUM_DURATION;
    // We use our mock's time==instruction count for a deterministic result.
    sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
    sched_ops.blocking_switch_threshold = BLOCK_LATENCY;
    sched_ops.block_time_scale = BLOCK_SCALE;
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
        make_header(TRACE_ENTRY_VERSION),
        make_thread(TID_IN_SWITCHES),
        make_pid(TID_IN_SWITCHES),
        make_version(TRACE_ENTRY_VERSION),
        make_marker(TRACE_MARKER_TYPE_CONTEXT_SWITCH_START, scheduler_t::SWITCH_PROCESS),
        make_timestamp(PROCESS_SWITCH_TIMESTAMP),
        make_instr(PROCESS_SWITCH_PC_START),
        make_instr(PROCESS_SWITCH_PC_START + 1),
        make_marker(TRACE_MARKER_TYPE_CONTEXT_SWITCH_END, scheduler_t::SWITCH_PROCESS),
        make_exit(TID_IN_SWITCHES),
        make_footer(),
        // Test a complete trace after the first one, which is how we plan to store
        // these in an archive file.
        make_header(TRACE_ENTRY_VERSION),
        make_thread(TID_IN_SWITCHES),
        make_pid(TID_IN_SWITCHES),
        make_version(TRACE_ENTRY_VERSION),
        make_marker(TRACE_MARKER_TYPE_CONTEXT_SWITCH_START, scheduler_t::SWITCH_THREAD),
        make_timestamp(THREAD_SWITCH_TIMESTAMP),
        make_instr(THREAD_SWITCH_PC_START),
        make_instr(THREAD_SWITCH_PC_START+1),
        make_marker(TRACE_MARKER_TYPE_CONTEXT_SWITCH_END, scheduler_t::SWITCH_THREAD),
        make_exit(TID_IN_SWITCHES),
        make_footer(),
        /* clang-format on */
    };
    auto switch_reader =
        std::unique_ptr<mock_reader_t>(new mock_reader_t(switch_sequence));
    auto switch_reader_end = std::unique_ptr<mock_reader_t>(new mock_reader_t());
    static constexpr int NUM_WORKLOADS = 3;
    static constexpr int NUM_INPUTS_PER_WORKLOAD = 3;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int NUM_INSTRS = 9;
    static constexpr int INSTR_QUANTUM = 3;
    static constexpr uint64_t TIMESTAMP = 44226688;
    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    for (int workload_idx = 0; workload_idx < NUM_WORKLOADS; workload_idx++) {
        std::vector<scheduler_t::input_reader_t> readers;
        for (int input_idx = 0; input_idx < NUM_INPUTS_PER_WORKLOAD; input_idx++) {
            std::vector<trace_entry_t> inputs;
            inputs.push_back(make_header(TRACE_ENTRY_VERSION));
            memref_tid_t tid =
                TID_BASE + workload_idx * NUM_INPUTS_PER_WORKLOAD + input_idx;
            inputs.push_back(make_thread(tid));
            inputs.push_back(make_pid(1));
            inputs.push_back(make_version(TRACE_ENTRY_VERSION));
            inputs.push_back(make_timestamp(TIMESTAMP));
            for (int instr_idx = 0; instr_idx < NUM_INSTRS; instr_idx++) {
                inputs.push_back(make_instr(42 + instr_idx * 4));
            }
            inputs.push_back(make_exit(tid));
            readers.emplace_back(
                std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs)),
                std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
        }
        sched_inputs.emplace_back(std::move(readers));
    }
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_TIMESTAMPS,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/4);
    sched_ops.quantum_duration = INSTR_QUANTUM;
    sched_ops.kernel_switch_reader = std::move(switch_reader);
    sched_ops.kernel_switch_reader_end = std::move(switch_reader_end);
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);

    // We have a custom version of run_lockstep_simulation here for more precise
    // testing of the markers and instructions and interfaces.
    // We record the entire sequence for a detailed check of some records, along with
    // a character representation for a higher-level view of the whole sequence.
    std::vector<scheduler_t::stream_t *> outputs(NUM_OUTPUTS, nullptr);
    std::vector<bool> eof(NUM_OUTPUTS, false);
    for (int i = 0; i < NUM_OUTPUTS; i++)
        outputs[i] = scheduler.get_stream(i);
    int num_eof = 0;
    std::vector<std::vector<memref_t>> refs(NUM_OUTPUTS);
    std::vector<std::string> sched_as_string(NUM_OUTPUTS);
    std::vector<memref_tid_t> prev_tid(NUM_OUTPUTS, INVALID_THREAD_ID);
    std::vector<bool> in_switch(NUM_OUTPUTS, false);
    std::vector<uint64> prev_in_ord(NUM_OUTPUTS, 0);
    std::vector<uint64> prev_out_ord(NUM_OUTPUTS, 0);
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
                sched_as_string[i] += '_';
                continue;
            }
            assert(status == scheduler_t::STATUS_OK);
            refs[i].push_back(memref);
            if (memref.instr.tid != prev_tid[i]) {
                if (!sched_as_string[i].empty())
                    sched_as_string[i] += ',';
                sched_as_string[i] +=
                    'A' + static_cast<char>(memref.instr.tid - TID_BASE);
            }
            if (memref.marker.type == TRACE_TYPE_MARKER &&
                memref.marker.marker_type == TRACE_MARKER_TYPE_CONTEXT_SWITCH_START)
                in_switch[i] = true;
            if (in_switch[i]) {
                // Test that switch code is marked synthetic.
                assert(outputs[i]->is_record_synthetic());
                // Test that switch code doesn't count toward input ordinals, but
                // does toward output ordinals.
                assert(outputs[i]->get_input_interface()->get_record_ordinal() ==
                           prev_in_ord[i] ||
                       // Won't match if we just switched inputs.
                       (memref.marker.type == TRACE_TYPE_MARKER &&
                        memref.marker.marker_type ==
                            TRACE_MARKER_TYPE_CONTEXT_SWITCH_START));
                assert(outputs[i]->get_record_ordinal() > prev_out_ord[i]);
            } else
                assert(!outputs[i]->is_record_synthetic());
            if (type_is_instr(memref.instr.type))
                sched_as_string[i] += 'i';
            else if (memref.marker.type == TRACE_TYPE_MARKER) {
                switch (memref.marker.marker_type) {
                case TRACE_MARKER_TYPE_VERSION: sched_as_string[i] += 'v'; break;
                case TRACE_MARKER_TYPE_TIMESTAMP: sched_as_string[i] += '0'; break;
                case TRACE_MARKER_TYPE_CONTEXT_SWITCH_END:
                    in_switch[i] = false;
                    // Fall-through.
                case TRACE_MARKER_TYPE_CONTEXT_SWITCH_START:
                    if (memref.marker.marker_value == scheduler_t::SWITCH_PROCESS)
                        sched_as_string[i] += 'p';
                    else if (memref.marker.marker_value == scheduler_t::SWITCH_THREAD)
                        sched_as_string[i] += 't';
                    else
                        assert(false && "unknown context switch type");
                    break;
                default: sched_as_string[i] += '?'; break;
                }
            }
            prev_tid[i] = memref.instr.tid;
            prev_in_ord[i] = outputs[i]->get_input_interface()->get_record_ordinal();
            prev_out_ord[i] = outputs[i]->get_record_ordinal();
        }
    }
    // Check the high-level strings.
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    assert(sched_as_string[0] ==
           "Av0iii,Ct0iitv0iii,Ep0iipv0iii,Gp0iipv0iii,It0iitv0iii,Cp0iipiii,Ep0iipiii,"
           "Gp0iipiii,Ap0iipiii,Bt0iitiii,Dp0iipiii,Ft0iitiii,Hp0iipiii______");
    assert(sched_as_string[1] ==
           "Bv0iii,Dp0iipv0iii,Ft0iitv0iii,Hp0iipv0iii,Ap0iipiii,Bt0iitiii,Dp0iipiii,"
           "Ft0iitiii,Hp0iipiii,It0iitiii,Cp0iipiii,Ep0iipiii,Gp0iipiii,It0iitiii");

    // Zoom in and check the first sequence record by record with value checks.
    int idx = 0;
    bool res = true;
    res = res &&
        check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP,
                  TIMESTAMP) &&
        check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_INSTR) &&
        check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_INSTR) &&
        check_ref(refs[0], idx, TID_BASE, TRACE_TYPE_INSTR) &&
        // Thread switch.
        check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                  TRACE_MARKER_TYPE_CONTEXT_SWITCH_START, scheduler_t::SWITCH_THREAD) &&
        check_ref(refs[0], idx, TID_BASE + 2, TRACE_TYPE_MARKER,
                  TRACE_MARKER_TYPE_TIMESTAMP, THREAD_SWITCH_TIMESTAMP) &&
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
        check_ref(refs[0], idx, TID_BASE + 4, TRACE_TYPE_MARKER,
                  TRACE_MARKER_TYPE_CONTEXT_SWITCH_START, scheduler_t::SWITCH_PROCESS) &&
        check_ref(refs[0], idx, TID_BASE + 4, TRACE_TYPE_MARKER,
                  TRACE_MARKER_TYPE_TIMESTAMP, PROCESS_SWITCH_TIMESTAMP) &&
        check_ref(refs[0], idx, TID_BASE + 4, TRACE_TYPE_INSTR) &&
        check_ref(refs[0], idx, TID_BASE + 4, TRACE_TYPE_INSTR) &&
        check_ref(refs[0], idx, TID_BASE + 4, TRACE_TYPE_MARKER,
                  TRACE_MARKER_TYPE_CONTEXT_SWITCH_END, scheduler_t::SWITCH_PROCESS) &&
        // We now see the headers for this thread.
        check_ref(refs[0], idx, TID_BASE + 4, TRACE_TYPE_MARKER,
                  TRACE_MARKER_TYPE_VERSION) &&
        check_ref(refs[0], idx, TID_BASE + 4, TRACE_TYPE_MARKER,
                  TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP) &&
        // The 3-instr quantum should not count the 2 switch instrs.
        check_ref(refs[0], idx, TID_BASE + 4, TRACE_TYPE_INSTR) &&
        check_ref(refs[0], idx, TID_BASE + 4, TRACE_TYPE_INSTR) &&
        check_ref(refs[0], idx, TID_BASE + 4, TRACE_TYPE_INSTR);

    {
        // Test a bad input sequence.
        std::vector<trace_entry_t> bad_switch_sequence = {
            /* clang-format off */
        make_header(TRACE_ENTRY_VERSION),
        make_thread(TID_IN_SWITCHES),
        make_pid(TID_IN_SWITCHES),
        make_marker(TRACE_MARKER_TYPE_CONTEXT_SWITCH_START, scheduler_t::SWITCH_PROCESS),
        make_instr(PROCESS_SWITCH_PC_START),
        make_marker(TRACE_MARKER_TYPE_CONTEXT_SWITCH_END, scheduler_t::SWITCH_PROCESS),
        make_footer(),
        make_header(TRACE_ENTRY_VERSION),
        make_thread(TID_IN_SWITCHES),
        make_pid(TID_IN_SWITCHES),
        // Error: duplicate type.
        make_marker(TRACE_MARKER_TYPE_CONTEXT_SWITCH_START, scheduler_t::SWITCH_PROCESS),
        make_instr(PROCESS_SWITCH_PC_START),
        make_marker(TRACE_MARKER_TYPE_CONTEXT_SWITCH_END, scheduler_t::SWITCH_PROCESS),
        make_footer(),
            /* clang-format on */
        };
        auto bad_switch_reader =
            std::unique_ptr<mock_reader_t>(new mock_reader_t(bad_switch_sequence));
        auto bad_switch_reader_end = std::unique_ptr<mock_reader_t>(new mock_reader_t());
        std::vector<scheduler_t::input_workload_t> test_sched_inputs;
        std::vector<scheduler_t::input_reader_t> readers;
        std::vector<trace_entry_t> inputs;
        inputs.push_back(make_header(TRACE_ENTRY_VERSION));
        readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs)),
                             std::unique_ptr<mock_reader_t>(new mock_reader_t()),
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
    test_synthetic_time_quanta();
    test_synthetic_with_timestamps();
    test_synthetic_with_priorities();
    test_synthetic_with_bindings();
    test_synthetic_with_bindings_weighted();
    test_synthetic_with_syscalls();
    test_synthetic_multi_threaded(argv[1]);
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
    test_inactive();
    test_direct_switch();
    test_kernel_switch_sequences();

    dr_standalone_exit();
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
