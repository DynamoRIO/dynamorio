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

#undef NDEBUG
#include <assert.h>
#include <iostream>
#include <thread>
#include <vector>

#include "dr_api.h"
#include "scheduler.h"

using namespace dynamorio::drmemtrace;

namespace {

// A mock reader that iterates over a vector of records.
class mock_reader_t : public reader_t {
public:
    mock_reader_t() = default;
    explicit mock_reader_t(const std::vector<trace_entry_t> &trace)
        : trace_(trace)
    {
        verbosity_ = 4;
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
        make_instr(4),
        make_instr(5),
        make_instr(6), // Region 2 starts here.
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
            assert(memref.instr.addr == 6);
            break;
        case 3:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 7);
            break;
        default: assert(ordinal == 4); assert(memref.exit.type == TRACE_TYPE_THREAD_EXIT);
        }
        ++ordinal;
    }
    assert(ordinal == 5);
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
            break;
        default: assert(ordinal == 5); assert(memref.exit.type == TRACE_TYPE_THREAD_EXIT);
        }
        ++ordinal;
    }
    assert(ordinal == 6);
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
#if (defined(X86_64) || defined(ARM_64)) && defined(HAS_ZLIB) && defined(HAS_SNAPPY)
    std::string trace1 = std::string(testdir) + "/drmemtrace.chase-snappy.x64.tracedir";
    // This trace has 2 threads: we pick the smallest one.
    static constexpr memref_tid_t TID_1_A = 23699;
    std::string trace2 = std::string(testdir) + "/drmemtrace.threadsig.x64.tracedir";
    // This trace has many threads: we pick 2 of the smallest.
    static constexpr memref_tid_t TID_2_A = 10507;
    static constexpr memref_tid_t TID_2_B = 10508;
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
    int max_input_index = 0;
    std::set<memref_tid_t> tids_seen;
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
        assert(memref.instr.tid == TID_1_A || memref.instr.tid == TID_2_A ||
               memref.instr.tid == TID_2_B);
        tids_seen.insert(memref.instr.tid);
        if (stream->get_input_stream_ordinal() > max_input_index)
            max_input_index = stream->get_input_stream_ordinal();
    }
    // Ensure 3 input streams and test input queries.
    assert(max_input_index == 2);
    assert(scheduler.get_input_stream_count() == 3);
    assert(scheduler.get_input_stream_name(0) ==
           "chase.20190225.185346.23699.memtrace.sz");
    // These could be in any order (dir listing determines that).
    assert(scheduler.get_input_stream_name(1) ==
               "drmemtrace.threadsig.10507.6178.trace.gz" ||
           scheduler.get_input_stream_name(1) ==
               "drmemtrace.threadsig.10508.1635.trace.gz");
    assert(scheduler.get_input_stream_name(2) ==
               "drmemtrace.threadsig.10507.6178.trace.gz" ||
           scheduler.get_input_stream_name(2) ==
               "drmemtrace.threadsig.10508.1635.trace.gz");
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
    static constexpr memref_tid_t TID_BASE = 100;
    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = TID_BASE + i;
        inputs[i].push_back(make_thread(tid));
        inputs[i].push_back(make_pid(1));
        for (int j = 0; j < NUM_INSTRS; j++)
            inputs[i].push_back(make_instr(42 + j * 4));
        inputs[i].push_back(make_exit(tid));
        std::vector<scheduler_t::input_reader_t> readers;
        readers.emplace_back(std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs[i])),
                             std::unique_ptr<mock_reader_t>(new mock_reader_t()), tid);
        sched_inputs.emplace_back(std::move(readers));
    }
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_IGNORE,
                                               scheduler_t::SCHEDULER_DEFAULTS,
                                               /*verbosity=*/4);
    sched_ops.quantum_duration = 3;
    scheduler_t scheduler;
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    // Walk the outputs in lockstep for crude but deterministic concurrency.
    std::vector<scheduler_t::stream_t *> outputs(NUM_OUTPUTS, nullptr);
    std::vector<bool> eof(NUM_OUTPUTS, false);
    for (int i = 0; i < NUM_OUTPUTS; i++)
        outputs[i] = scheduler.get_stream(i);
    int num_eof = 0;
    // Record the threads, one char each.
    std::vector<std::string> sched_as_string(NUM_OUTPUTS);
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
            assert(status == scheduler_t::STATUS_OK);
            if (type_is_instr(memref.instr.type)) {
                sched_as_string[i] +=
                    'A' + static_cast<char>(memref.instr.tid - TID_BASE);
            }
        }
    }
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    // Hardcoding here for the 2 outputs and 7 inputs.
    // We expect 3 letter sequences (our quantum) alternating every-other as each
    // core alternates; with an odd number the 2nd core finishes early.
    assert(sched_as_string[0] == "AAACCCEEEGGGBBBDDDFFFAAACCCEEEGGG");
    assert(sched_as_string[1] == "BBBDDDFFFAAACCCEEEGGGBBBDDDFFF");
}

#if (defined(X86_64) || defined(ARM_64)) && defined(HAS_ZLIB)
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
#if (defined(X86_64) || defined(ARM_64)) && defined(HAS_ZLIB)
    std::string path = std::string(testdir) + "/drmemtrace.threadsig.x64.tracedir";
    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(path);
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                               scheduler_t::DEPENDENCY_IGNORE,
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
            // Back to the trace, but skipping what we already read.
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr == 5);
            break;
        default:
            assert(ordinal == 13);
            assert(memref.exit.type == TRACE_TYPE_THREAD_EXIT);
        }
        ++ordinal;
    }
    assert(ordinal == 14);
}

} // namespace

int
main(int argc, const char *argv[])
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
    test_synthetic_multi_threaded(argv[1]);
    test_speculation();

    dr_standalone_exit();
    return 0;
}
