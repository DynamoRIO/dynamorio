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
#if (defined(X86_64) || defined(ARM_64)) && defined(HAS_ZIP) && defined(HAS_SNAPPY)
    std::string trace1 = std::string(testdir) + "/drmemtrace.chase-snappy.x64.tracedir";
    // This trace has 2 threads: we pick the smallest one.
    static constexpr memref_tid_t TID_1_A = 23699;
    std::string trace2 = std::string(testdir) + "/drmemtrace.threadsig.x64.tracedir";
    // This trace has many threads: we pick 2 of the smallest.
    static constexpr memref_tid_t TID_2_A = 3018559;
    static constexpr memref_tid_t TID_2_B = 3018555;
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
               "drmemtrace.threadsig.3018559.5319.trace.zip" ||
           scheduler.get_input_stream_name(1) ==
               "drmemtrace.threadsig.3018555.0775.trace.zip");
    assert(scheduler.get_input_stream_name(2) ==
               "drmemtrace.threadsig.3018559.5319.trace.zip" ||
           scheduler.get_input_stream_name(2) ==
               "drmemtrace.threadsig.3018555.0775.trace.zip");
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
run_lockstep_simulation(scheduler_t &scheduler, int num_outputs, memref_tid_t tid_base)
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
            scheduler_t::stream_status_t status = outputs[i]->next_record(memref);
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
    std::vector<std::string> sched_as_string =
        run_lockstep_simulation(scheduler, NUM_OUTPUTS, TID_BASE);
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        std::cerr << "cpu #" << i << " schedule: " << sched_as_string[i] << "\n";
    }
    // Hardcoding here for the 2 outputs and 7 inputs.
    // We expect 3 letter sequences (our quantum) alternating every-other as each
    // core alternates; with an odd number the 2nd core finishes early.
    assert(sched_as_string[0] == "AAACCCEEEGGGBBBDDDFFFAAACCCEEEGGG");
    assert(sched_as_string[1] == "BBBDDDFFFAAACCCEEEGGGBBBDDDFFF");
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
                                                   /*verbosity=*/3);
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
    }
#endif // HAS_ZIP
}

#if (defined(X86_64) || defined(ARM_64)) && defined(HAS_ZIP)
static void
simulate_core_and_record_schedule(scheduler_t::stream_t *stream,
                                  std::vector<memref_tid_t> &thread_sequence)
{
    memref_t record;
    memref_tid_t prev_tid = INVALID_THREAD_ID;
    for (scheduler_t::stream_status_t status = stream->next_record(record);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(record)) {
        if (status == scheduler_t::STATUS_WAIT) {
            std::this_thread::yield();
            continue;
        }
        assert(status == scheduler_t::STATUS_OK);
        if (thread_sequence.empty())
            thread_sequence.push_back(record.instr.tid);
        else if (record.instr.tid != prev_tid)
            thread_sequence.push_back(record.instr.tid);
        prev_tid = record.instr.tid;
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
    std::vector<std::vector<memref_tid_t>> thread_sequence(NUM_OUTPUTS);
    {
        // Record.
        scheduler_t scheduler;
        std::vector<scheduler_t::input_workload_t> sched_inputs;
        sched_inputs.emplace_back(path);
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_ANY_OUTPUT,
                                                   scheduler_t::DEPENDENCY_IGNORE,
                                                   scheduler_t::SCHEDULER_DEFAULTS,
                                                   /*verbosity=*/2);
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
                                             scheduler.get_stream(i),
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
                                                   /*verbosity=*/2);
        zipfile_istream_t infile(record_fname);
        sched_ops.schedule_replay_istream = &infile;
        if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
            scheduler_t::STATUS_SUCCESS)
            assert(false);
        std::vector<std::vector<memref_tid_t>> replay_sequence(NUM_OUTPUTS);
        std::vector<std::thread> threads;
        threads.reserve(NUM_OUTPUTS);
        for (int i = 0; i < NUM_OUTPUTS; ++i) {
            threads.emplace_back(std::thread(&simulate_core_and_record_schedule,
                                             scheduler.get_stream(i),
                                             std::ref(replay_sequence[i])));
        }
        for (std::thread &thread : threads)
            thread.join();
        std::cerr << "Recorded:\n";
        for (int i = 0; i < NUM_OUTPUTS; ++i) {
            std::cerr << "Core #" << i << ": ";
            for (memref_tid_t tid : thread_sequence[i])
                std::cerr << tid << " ";
            std::cerr << "\n";
        }
        std::cerr << "Replayed:\n";
        for (int i = 0; i < NUM_OUTPUTS; ++i) {
            std::cerr << "Core #" << i << ": ";
            for (memref_tid_t tid : replay_sequence[i])
                std::cerr << tid << " ";
            std::cerr << "\n";
        }
        // TODO i#5438: Save the record and instruction ordinals at each context switch
        // and compare them, once fencepost errors with queueing are solved.
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
        scheduler_t::scheduler_options_t sched_ops(scheduler_t::MAP_TO_RECORDED_OUTPUT,
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

    static constexpr int NUM_INPUTS = 4;
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
        for (int j = 0; j < NUM_INSTRS; j++)
            inputs[i].push_back(make_instr(42 + j * 4));
        inputs[i].push_back(make_exit(tid));
    }

    // Synthesize a cpu-schedule file.
    std::string cpu_fname = "tmp_test_cpu_as_traced.zip";
    static const char *const CORE0_SCHED_STRING = "AAACCCAAACCCBBBDDD";
    static const char *const CORE1_SCHED_STRING = "BBBDDDBBBDDDAAACCC";
    {
        std::vector<schedule_entry_t> sched0;
        sched0.emplace_back(TID_BASE, 101, CPU0, 0);
        sched0.emplace_back(TID_BASE + 2, 103, CPU0, 0);
        sched0.emplace_back(TID_BASE, 105, CPU0, 4);
        sched0.emplace_back(TID_BASE + 2, 107, CPU0, 4);
        sched0.emplace_back(TID_BASE + 1, 109, CPU0, 7);
        sched0.emplace_back(TID_BASE + 3, 111, CPU0, 7);
        std::vector<schedule_entry_t> sched1;
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
                                               /*verbosity=*/4);
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
test_replay_as_traced_from_file(const char *testdir)
{
#if (defined(X86_64) || defined(ARM_64)) && defined(HAS_ZIP)
    std::cerr << "\n----------------\nTesting replay as-traced from a file\n";
    std::string path = std::string(testdir) + "/drmemtrace.threadsig.x64.tracedir";
    std::string cpu_file =
        std::string(testdir) + "/drmemtrace.threadsig.x64.tracedir/cpu_schedule.bin.zip";
    // This checked-in trace has 8 threads on 9 cores, with one core starting out in a
    // wait state and many entries to merge in the cpu_schedule file.  It doesn't have
    // much thread migration but our synthetic test above covers that.
    static const char *const SCHED_STRING =
        "Core #0: 3018553 \nCore #1: 3018556 \nCore #2: 3018557 \nCore #3: 3018558 "
        "\nCore #4: 3018520 \nCore #5: 3018520 \nCore #6: 3018554 \nCore #7: 3018555 "
        "\nCore #8: 3018559 \n";
    static constexpr int NUM_OUTPUTS = 9; // Matches the actual trace's core footprint.
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
    std::vector<std::vector<memref_tid_t>> replay_sequence(NUM_OUTPUTS);
    std::vector<std::thread> threads;
    threads.reserve(NUM_OUTPUTS);
    for (int i = 0; i < NUM_OUTPUTS; ++i) {
        threads.emplace_back(std::thread(&simulate_core_and_record_schedule,
                                         scheduler.get_stream(i),
                                         std::ref(replay_sequence[i])));
    }
    for (std::thread &thread : threads)
        thread.join();
    std::ostringstream replay_string;
    for (int i = 0; i < NUM_OUTPUTS; ++i) {
        replay_string << "Core #" << i << ": ";
        for (memref_tid_t tid : replay_sequence[i])
            replay_string << tid << " ";
        replay_string << "\n";
    }
    std::cerr << "As-traced from file:\n"
              << SCHED_STRING << "Versus replay:\n"
              << replay_string.str();
    assert(replay_string.str() == SCHED_STRING);
#endif
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
    test_replay();
    test_replay_multi_threaded(argv[1]);
    test_replay_timestamps();
    test_replay_skip();
    test_replay_as_traced_from_file(argv[1]);
    test_replay_as_traced();

    dr_standalone_exit();
    return 0;
}
