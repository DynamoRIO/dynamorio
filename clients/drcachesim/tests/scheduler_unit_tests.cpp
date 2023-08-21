/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#include "dr_api.h"
#include "scheduler.h"
#ifdef HAS_ZIP
#    include "zipfile_istream.h"
#    include "zipfile_ostream.h"
#endif

namespace dynamorio {
namespace drmemtrace {

using ::dynamorio::drmemtrace::memtrace_stream_t;

// A mock reader that iterates over a vector of records.
class mock_reader_t : public reader_t {
public:
    mock_reader_t() = default;
    explicit mock_reader_t(const std::vector<trace_entry_t> &trace)
        : trace_(trace)
    {
        verbosity_ = 3;
    }
    bool
    init() override
    {
        at_eof_ = false;
        ++*this;
        return true;
    }
    trace_entry_t *
    read_next_entry() override
    {
        trace_entry_t *entry = read_queued_entry();
        if (entry != nullptr)
            return entry;
        ++index_;
        if (index_ >= static_cast<int>(trace_.size())) {
            at_eof_ = true;
            return nullptr;
        }
        return &trace_[index_];
    }
    std::string
    get_stream_name() const override
    {
        return "";
    }

private:
    std::vector<trace_entry_t> trace_;
    int index_ = -1;
};

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

static trace_entry_t
make_instr(addr_t pc, trace_type_t type = TRACE_TYPE_INSTR)
{
    trace_entry_t entry;
    entry.type = static_cast<unsigned short>(type);
    entry.size = 1;
    entry.addr = pc;
    return entry;
}

static trace_entry_t
make_exit(memref_tid_t tid)
{
    trace_entry_t entry;
    entry.type = TRACE_TYPE_THREAD_EXIT;
    entry.addr = static_cast<addr_t>(tid);
    return entry;
}

static trace_entry_t
make_footer()
{
    trace_entry_t entry;
    entry.type = TRACE_TYPE_FOOTER;
    return entry;
}

static trace_entry_t
make_version(int version)
{
    trace_entry_t entry;
    entry.type = TRACE_TYPE_MARKER;
    entry.size = TRACE_MARKER_TYPE_VERSION;
    entry.addr = version;
    return entry;
}

static trace_entry_t
make_thread(memref_tid_t tid)
{
    trace_entry_t entry;
    entry.type = TRACE_TYPE_THREAD;
    entry.addr = static_cast<addr_t>(tid);
    return entry;
}

static trace_entry_t
make_pid(memref_pid_t pid)
{
    trace_entry_t entry;
    entry.type = TRACE_TYPE_PID;
    entry.addr = static_cast<addr_t>(pid);
    return entry;
}

static trace_entry_t
make_timestamp(uint64_t timestamp)
{
    trace_entry_t entry;
    entry.type = TRACE_TYPE_MARKER;
    entry.size = TRACE_MARKER_TYPE_TIMESTAMP;
    entry.addr = static_cast<addr_t>(timestamp);
    return entry;
}

static trace_entry_t
make_marker(trace_marker_type_t type, uintptr_t value)
{
    trace_entry_t entry;
    entry.type = TRACE_TYPE_MARKER;
    entry.size = static_cast<unsigned short>(type);
    entry.addr = value;
    return entry;
}

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
                        bool send_time = false)
{
    // Walk the outputs in lockstep for crude but deterministic concurrency.
    std::vector<scheduler_t::stream_t *> outputs(num_outputs, nullptr);
    std::vector<bool> eof(num_outputs, false);
    for (int i = 0; i < num_outputs; i++)
        outputs[i] = scheduler.get_stream(i);
    int num_eof = 0;
    // Record the threads, one char each.
    std::vector<std::string> sched_as_string(num_outputs);
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
                status = outputs[i]->next_record(
                    memref, outputs[i]->get_instruction_ordinal() + 1);
            } else {
                status = outputs[i]->next_record(memref);
            }
            if (status == scheduler_t::STATUS_EOF) {
                ++num_eof;
                eof[i] = true;
                continue;
            }
            if (status == scheduler_t::STATUS_WAIT)
                continue;
            assert(status == scheduler_t::STATUS_OK);
            if (type_is_instr(memref.instr.type)) {
                sched_as_string[i] +=
                    'A' + static_cast<char>(memref.instr.tid - tid_base);
            }
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
    // Hardcoding here for the 2 outputs and 7 inputs.
    // We expect 3 letter sequences (our quantum) alternating every-other as each
    // core alternates; with an odd number the 2nd core finishes early.
    static const char *const CORE0_SCHED_STRING = "AAACCCEEEGGGBBBDDDFFFAAACCCEEEGGG";
    static const char *const CORE1_SCHED_STRING = "BBBDDDFFFAAACCCEEEGGGBBBDDDFFF";
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
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
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
        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
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
    std::vector<trace_entry_t> refs[NUM_INPUTS];
    for (int i = 0; i < NUM_INPUTS; ++i) {
        refs[i].push_back(make_thread(TID_BASE + i));
        refs[i].push_back(make_pid(1));
        refs[i].push_back(make_version(TRACE_ENTRY_VERSION));
        refs[i].push_back(make_timestamp(10));
        refs[i].push_back(make_instr(10));
        refs[i].push_back(make_instr(30));
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
        zipfile_ostream_t outfile(record_fname);
        sched_ops.schedule_record_ostream = &outfile;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        auto check_next = [](scheduler_t::stream_t *stream, uint64_t time,
                             scheduler_t::stream_status_t expect_status,
                             memref_tid_t expect_tid = INVALID_THREAD_ID,
                             trace_type_t expect_type = TRACE_TYPE_READ) {
            memref_t memref;
            scheduler_t::stream_status_t status = stream->next_record(memref, time);
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
        check_next(cpu1, ++time, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_INSTR);
        check_next(cpu1, time, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_THREAD_EXIT);
        check_next(cpu1, ++time, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
        // This is another quantum end at 9 but the queue is empty.
        check_next(cpu1, ++time, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
        // Finish off the inputs.
        check_next(cpu0, ++time, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_INSTR);
        check_next(cpu0, ++time, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_INSTR);
        check_next(cpu0, time, scheduler_t::STATUS_OK, TID_C, TRACE_TYPE_THREAD_EXIT);
        check_next(cpu0, time, scheduler_t::STATUS_EOF);
        check_next(cpu1, time, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_THREAD_EXIT);
        check_next(cpu1, time, scheduler_t::STATUS_EOF);
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
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_A);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == "ACCC");
        assert(sched_as_string[1] == "BAABB");
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
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
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
    assert(sched_as_string[0] == "CCCIIICCCFFFIIIFFFBBBHHHEEEBBBHHHDDDAAAGGGDDD");
    assert(sched_as_string[1] == "FFFJJJJJJJJJCCCIIIEEEBBBHHHEEEAAAGGGDDDAAAGGG");
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
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
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
    assert(sched_as_string[0] == "BBBHHHEEEBBBHHHFFFJJJJJJJJJCCCIIIDDDAAAGGGDDD");
    assert(sched_as_string[1] == "EEEBBBHHHEEECCCIIICCCFFFIIIFFFAAAGGGDDDAAAGGG");
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
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    // We have {A,B,C} on {2,4}, {D,E,F} on {0,1}, and {G,H,I} on {1,2,3}:
    assert(sched_as_string[0] == "DDDFFFDDDFFFDDDFFF");
    assert(sched_as_string[1] == "EEEHHHEEEIIIEEE");
    assert(sched_as_string[2] == "AAACCCGGGCCCHHHCCC");
    assert(sched_as_string[3] == "GGGIIIHHHGGGIII");
    assert(sched_as_string[4] == "BBBAAABBBAAABBB");
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
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    // We have {A,B,C} on {2,4}, {D,E,F} on {0,1}, and {G,H,I} on {1,2,3}:
    assert(sched_as_string[0] == "FFFFFFFFFEEEEEEEEE");
    assert(sched_as_string[1] == "IIIIIIIIIDDDDDDDDD");
    assert(sched_as_string[2] == "CCCCCCCCCAAAAAAAAA");
    assert(sched_as_string[3] == "HHHHHHHHHGGGGGGGGG");
    assert(sched_as_string[4] == "BBBBBBBBB");
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
                // Sprinkle timestamps every other instruction.  We use the
                // same formula as test_synthetic_with_priorities().
                if (instr_idx % 2 == 0) {
                    inputs.push_back(make_timestamp(
                        1000 * workload_idx +
                        100 * (NUM_INPUTS_PER_WORKLOAD - input_idx) + 10 * instr_idx));
                }
                inputs.push_back(make_instr(42 + instr_idx * 4));
                // Insert some blocking syscalls in the high-priority (see below)
                // middle threads.
                if (input_idx == 1 && instr_idx % (workload_idx + 1) == workload_idx) {
                    inputs.push_back(
                        make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0));
                }
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
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    // See the test_synthetic_with_priorities() test which has our base sequence.
    // But now B hits a syscall every instr, and E every other instr, so neither
    // reaches its 3-instr quantum.  (H's syscalls are every 3rd instr coinciding with its
    // quantum.)  Furthermore, B isn't finished by the time E and H are done and we see
    // the lower-priority C and F getting scheduled while B is in a wait state for its
    // blocking syscall.
    // Note that the 3rd B is not really on the two cores at the same time as there are
    // markers and other records that cause them to not actually line up.
    assert(sched_as_string[0] == "BHHHBHHHBHHHBEBIIIJJJJJJJJJCCCIIIDDDAAAGGGDDD");
    assert(sched_as_string[1] == "EEBEEBEEBEECCCFFFBCCCFFFIIIFFFAAAGGGDDDAAAGGG");
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
                // Sprinkle timestamps every other instruction.  We use the
                // same formula as test_synthetic_with_priorities().
                if (instr_idx % 2 == 0) {
                    inputs.push_back(make_timestamp(
                        1000 * workload_idx +
                        100 * (NUM_INPUTS_PER_WORKLOAD - input_idx) + 10 * instr_idx));
                }
                inputs.push_back(make_instr(42 + instr_idx * 4));
                // Insert some blocking syscalls.
                if (instr_idx % 3 == 1) {
                    inputs.push_back(
                        make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0));
                }
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
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    assert(sched_as_string[0] == "AAAAAAAAA");
    assert(sched_as_string[1] == "");
}

static bool
check_ref(std::vector<memref_t> &refs, int &idx, memref_tid_t expected_tid,
          trace_type_t expected_type,
          trace_marker_type_t expected_marker = TRACE_MARKER_TYPE_RESERVED_END)
{
    if (expected_tid != refs[idx].instr.tid || expected_type != refs[idx].instr.type) {
        std::cerr << "Record " << idx << " has tid " << refs[idx].instr.tid
                  << " and type " << refs[idx].instr.type << " != expected tid "
                  << expected_tid << " and expected type " << expected_type << "\n";
        return false;
    }
    if (expected_type == TRACE_TYPE_MARKER &&
        expected_marker != refs[idx].marker.marker_type) {
        std::cerr << "Record " << idx << " has marker type "
                  << refs[idx].marker.marker_type << " but expected " << expected_marker
                  << "\n";
        return false;
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
        make_marker(TRACE_MARKER_TYPE_SYSCALL, SYSNUM),
        make_marker(TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0),
        make_marker(TRACE_MARKER_TYPE_FUNC_ID, 100),
        make_marker(TRACE_MARKER_TYPE_FUNC_ARG, 42),
        make_timestamp(50),
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
    if (scheduler.init(sched_inputs, 1, sched_ops) != scheduler_t::STATUS_SUCCESS)
        assert(false);
    auto *stream = scheduler.get_stream(0);
    memref_t memref;
    std::vector<memref_t> refs;
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
        refs.push_back(memref);
    }
    std::vector<trace_entry_t> entries;
    int idx = 0;
    bool res = true;
    res = res &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_ref(refs, idx, TID_A, TRACE_TYPE_INSTR) &&
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
test_synthetic_with_syscalls()
{
    test_synthetic_with_syscalls_multiple();
    test_synthetic_with_syscalls_single();
    test_synthetic_with_syscalls_precise();
}

#if (defined(X86_64) || defined(ARM_64)) && defined(HAS_ZIP)
static void
simulate_core(scheduler_t::stream_t *stream)
{
    memref_t record;
    for (scheduler_t::stream_status_t status = stream->next_record(record);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(record)) {
        if (status == scheduler_t::STATUS_WAIT) {
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
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
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
    if (scheduler.init(sched_inputs, 1, sched_ops) != scheduler_t::STATUS_SUCCESS)
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
    static const char *const CORE0_SCHED_STRING = "AAACCCEEEGGGBBBDDDFFFAAACCCEEEGGG";
    static const char *const CORE1_SCHED_STRING = "BBBDDDFFFAAACCCEEEGGGBBBDDDFFF";

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
                                                   /*verbosity=*/2);
        sched_ops.quantum_duration = QUANTUM_INSTRS;

        zipfile_ostream_t outfile(record_fname);
        sched_ops.schedule_record_ostream = &outfile;

        scheduler_t scheduler;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
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
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
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
        if (status == scheduler_t::STATUS_WAIT) {
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
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
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
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
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
    static const char *const CORE0_SCHED_STRING = "AAACCC";
    static const char *const CORE1_SCHED_STRING = "BBBCCCCCCDDDAAABBBDDDAAABBBDDD";
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
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
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
        if (scheduler.init(sched_inputs, 1, sched_ops) != scheduler_t::STATUS_SUCCESS)
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
        if (scheduler.init(sched_inputs, 1, sched_ops) != scheduler_t::STATUS_SUCCESS)
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
    static const char *const CORE0_SCHED_STRING = "EEEAAACCCAAACCCBBBDDD";
    static const char *const CORE1_SCHED_STRING = "EEEBBBDDDBBBDDDAAACCC";
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
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
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
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
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
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
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
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
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
        // Advance cpu0 to its 1st instr.
        check_next(stream0, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_MARKER);
        check_next(stream0, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_MARKER);
        check_next(stream0, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_INSTR);
        // Advance cpu1 to its 1st instr.
        check_next(stream1, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_MARKER);
        check_next(stream1, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_MARKER);
        check_next(stream1, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
        // Make cpu1 inactive.
        status = stream1->set_active(false);
        assert(status == scheduler_t::STATUS_OK);
        check_next(stream1, scheduler_t::STATUS_WAIT);
        // Test making cpu1 inactive while it's already inactive.
        status = stream1->set_active(false);
        assert(status == scheduler_t::STATUS_OK);
        check_next(stream1, scheduler_t::STATUS_WAIT);
        // Advance cpu0 to its quantum end.
        check_next(stream0, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_INSTR);
        // Ensure cpu0 now picks up the input that was on cpu1.
        check_next(stream0, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
        check_next(stream0, scheduler_t::STATUS_OK, TID_B, TRACE_TYPE_INSTR);
        // End of quantum.
        check_next(stream0, scheduler_t::STATUS_OK, TID_A, TRACE_TYPE_INSTR);
        // Make cpu0 inactive and cpu1 active.
        status = stream0->set_active(false);
        assert(status == scheduler_t::STATUS_OK);
        check_next(stream0, scheduler_t::STATUS_WAIT);
        status = stream1->set_active(true);
        assert(status == scheduler_t::STATUS_OK);
        // Now cpu1 should finish things.
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
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::string> sched_as_string =
            run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_A);
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
        }
        assert(sched_as_string[0] == "AABBA");
        assert(sched_as_string[1] == "BB");
    }
#endif // HAS_ZIP
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
    test_replay_skip();
    test_replay_as_traced_from_file(argv[1]);
    test_replay_as_traced();
    test_replay_as_traced_i6107_workaround();
    test_inactive();

    dr_standalone_exit();
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
