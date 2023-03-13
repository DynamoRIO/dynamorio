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
#include <vector>

#include "scheduler.h"

using namespace dynamorio::drmemtrace;

namespace {

// A mock reader that iterates over a vector of records.
class mock_reader_t : public reader_t {
public:
    mock_reader_t() = default;
    explicit mock_reader_t(const std::vector<memref_t> &trace)
        : trace_(trace)
    {
    }
    bool
    init() override
    {
        process_input_entry();
        return true;
    }
    const memref_t &
    operator*() override
    {
        return trace_[index_];
    }
    bool
    operator==(const reader_t &rhs) const override
    {
        const auto &mock = reinterpret_cast<const mock_reader_t &>(rhs);
        return (index_ == trace_.size()) == (mock.index_ == mock.trace_.size());
    }
    bool
    operator!=(const reader_t &rhs) const override
    {
        const auto &mock = reinterpret_cast<const mock_reader_t &>(rhs);
        return (index_ == trace_.size()) != (mock.index_ == mock.trace_.size());
    }
    reader_t &
    operator++() override
    {
        ++index_;
        process_input_entry();
        return *this;
    }
    bool
    process_input_entry() override
    {
        ++cur_ref_count_;
        if (index_ < trace_.size() && type_is_instr(trace_[index_].instr.type))
            ++cur_instr_count_;
        return true;
    }
    trace_entry_t *
    read_next_entry() override
    {
        trace_entry_t *entry = read_queued_entry();
        if (entry != nullptr)
            return entry;
        // We need this to work just well enough for reader_t::skip_instructions
        // to identify instr entries.
        ++index_;
        if (index_ >= trace_.size())
            return nullptr;
        trace_entry_.type = trace_[index_].instr.type;
        trace_entry_.size = trace_[index_].instr.size;
        trace_entry_.addr = trace_[index_].instr.addr;
        return &trace_entry_;
    }
    void
    use_prev(trace_entry_t *prev) override
    {
        --index_;
        queue_.pop();
    }
    std::string
    get_stream_name() const override
    {
        return "";
    }

private:
    std::vector<memref_t> trace_;
    int index_ = 0;
    trace_entry_t trace_entry_;
};

static memref_t
make_instr(memref_tid_t tid, addr_t pc)
{
    memref_t memref;
    memref.instr.pid = 0;
    memref.instr.tid = tid;
    memref.instr.type = TRACE_TYPE_INSTR;
    memref.instr.addr = pc;
    memref.instr.size = 1;
    return memref;
}

static memref_t
make_exit(memref_tid_t tid)
{
    memref_t memref;
    memref.exit.type = TRACE_TYPE_THREAD_EXIT;
    memref.exit.tid = tid;
    return memref;
}

static memref_t
make_version(memref_tid_t tid, int version)
{
    memref_t memref;
    memref.marker.tid = tid;
    memref.marker.type = TRACE_TYPE_MARKER;
    memref.marker.marker_type = TRACE_MARKER_TYPE_VERSION;
    memref.marker.marker_value = version;
    return memref;
}

static memref_t
make_timestamp(memref_tid_t tid, uint64_t timestamp)
{
    memref_t memref;
    memref.marker.tid = tid;
    memref.marker.type = TRACE_TYPE_MARKER;
    memref.marker.marker_type = TRACE_MARKER_TYPE_TIMESTAMP;
    memref.marker.marker_value = timestamp;
    return memref;
}

static void
test_serial()
{
    static constexpr memref_tid_t TID_A = 42;
    static constexpr memref_tid_t TID_B = 99;
    std::vector<memref_t> refs_A = {
        /* clang-format off */
        // Include a header to test the scheduler queuing it.
        make_version(TID_A, 4),
        // Each timestamp is followed by an instr whose PC==time.
        make_timestamp(TID_A, 10),
        make_instr(TID_A, 10),
        make_timestamp(TID_A, 30),
        make_instr(TID_A, 30),
        make_timestamp(TID_A, 50),
        make_instr(TID_A, 50),
        make_exit(TID_A),
        /* clang-format on */
    };
    std::vector<memref_t> refs_B = {
        /* clang-format off */
        make_version(TID_B, 4),
        make_timestamp(TID_B, 20),
        make_instr(TID_B, 20),
        make_timestamp(TID_B, 40),
        make_instr(TID_B, 40),
        make_timestamp(TID_B, 60),
        make_instr(TID_B, 60),
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
    std::vector<memref_t> input_sequence = {
        make_instr(0, 1),
        make_instr(0, 2),
        make_exit(0),
    };
    static constexpr int NUM_INPUTS = 3;
    static constexpr int NUM_OUTPUTS = 2;
    std::vector<memref_t> inputs[NUM_INPUTS];
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = 100 + i;
        inputs[i] = input_sequence;
        for (auto &record : inputs[i])
            record.instr.tid = tid;
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
        }
    }
    assert(count == input_sequence.size() * NUM_INPUTS);
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
}

static void
test_regions()
{
    std::vector<memref_t> memrefs = {
        /* clang-format off */
        make_instr(1, 1),
        make_instr(1, 2), // Region 1 is just this instr.
        make_instr(1, 3),
        make_instr(1, 4),
        make_instr(1, 5),
        make_instr(1, 6), // Region 2 starts here.
        make_instr(1, 7), // Region 2 ends here.
        make_instr(1, 8),
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
    // TODO(i#5843: We don't have timestamps so we can't use the serial options.
    // We should make a new test with timestamps but currently that breaks the
    // existing scheduler region code (because the instr count is still low for
    // the inserted timestamp so it tries to skip again...): we separate fixing that
    // and adding a timestamp test as future work.
    if (scheduler.init(sched_inputs, 1,
                       scheduler_t::scheduler_options_t(
                           scheduler_t::MAP_TO_ANY_OUTPUT, scheduler_t::DEPENDENCY_IGNORE,
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
test_only_threads(const char *testdir)
{
    // Test with synthetic streams and readers.
    {
        static constexpr memref_tid_t TID_A = 42;
        static constexpr memref_tid_t TID_B = 99;
        static constexpr memref_tid_t TID_C = 7;
        std::vector<memref_t> refs_A = {
            make_instr(TID_A, 50),
            make_exit(TID_A),
        };
        std::vector<memref_t> refs_B = {
            make_instr(TID_B, 60),
            make_exit(TID_B),
        };
        std::vector<memref_t> refs_C = {
            make_instr(TID_B, 60),
            make_exit(TID_B),
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
    // Test with real files as that is a separate code path in the scheduler.
    {
        std::string trace1 =
            std::string(testdir) + "/drmemtrace.chase-snappy.x64.tracedir";
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
    }
}

} // namespace

int
main(int argc, const char *argv[])
{
    // Takes in a path to the tests/ src dir.
    assert(argc == 2);
    test_serial();
    test_parallel();
    test_param_checks();
    test_regions();
    test_only_threads(argv[1]);
    return 0;
}
