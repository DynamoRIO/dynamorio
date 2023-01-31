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
        if (index_ < trace_.size() && type_is_instr(trace_[index_].instr.type))
            ++cur_instr_count_;
        return true;
    }
    trace_entry_t *
    read_next_entry() override
    {
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
    }
    bool
    read_next_thread_entry(size_t thread_index, OUT trace_entry_t *entry,
                           OUT bool *eof) override
    {
        return true;
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
        std::unique_ptr<reader_t> input =
            std::unique_ptr<mock_reader_t>(new mock_reader_t(inputs[i]));
        std::unique_ptr<reader_t> end =
            std::unique_ptr<mock_reader_t>(new mock_reader_t());
        sched_inputs.emplace_back(std::move(input), std::move(end), tid);
    }
    scheduler_t::scheduler_options_t sched_ops(scheduler_t::STREAM_BY_INPUT_SHARD,
                                               scheduler_t::SCHEDULE_RUN_TO_COMPLETION);
    scheduler_t scheduler(/*verbosity=*/4);
    if (scheduler.init(sched_inputs, NUM_OUTPUTS, sched_ops) !=
        scheduler_t::STATUS_SUCCESS)
        assert(false);
    std::unordered_map<memref_tid_t, int> tid2stream;
    int count = 0;
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        auto *stream = scheduler.get_stream(i);
        while (true) {
            memref_t memref;
            scheduler_t::stream_status_t status = stream->next_record(memref);
            if (status == scheduler_t::STATUS_EOF)
                break;
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
    std::unique_ptr<reader_t> input =
        std::unique_ptr<mock_reader_t>(new mock_reader_t(std::vector<memref_t>()));
    std::unique_ptr<reader_t> end = std::unique_ptr<mock_reader_t>(new mock_reader_t());
    std::vector<scheduler_t::range_t> regions;
    // Instr counts are 1-based so 0 is an invalid start.
    regions.emplace_back(0, 2);
    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(input), std::move(end), 1);
    sched_inputs[0].thread_modifiers.push_back(scheduler_t::input_thread_info_t(regions));
    scheduler_t::scheduler_options_t sched_ops(
        scheduler_t::STREAM_BY_SYNTHETIC_CPU,
        scheduler_t::SCHEDULE_INTERLEAVE_AS_RECORDED);
    assert(scheduler.init(sched_inputs, 1, sched_ops) ==
           scheduler_t::STATUS_ERROR_INVALID_PARAMETER);

    // Test stop > start.
    sched_inputs[0].thread_modifiers[0].regions_of_interest[0].start_instruction = 2;
    sched_inputs[0].thread_modifiers[0].regions_of_interest[0].stop_instruction = 1;
    assert(scheduler.init(sched_inputs, 1, sched_ops) ==
           scheduler_t::STATUS_ERROR_INVALID_PARAMETER);
}

static void
test_regions()
{
    std::vector<memref_t> memrefs = {
        make_instr(1, 1), make_instr(1, 2), // Region 1 is just this instr.
        make_instr(1, 3), make_instr(1, 4),
        make_instr(1, 5), make_instr(1, 6), // Region 2 starts here.
        make_instr(1, 7),                   // Region 2 ends here.
        make_instr(1, 8), make_exit(1),
    };
    std::unique_ptr<reader_t> input =
        std::unique_ptr<mock_reader_t>(new mock_reader_t(memrefs));
    std::unique_ptr<reader_t> end = std::unique_ptr<mock_reader_t>(new mock_reader_t());

    std::vector<scheduler_t::range_t> regions;
    // Instr counts are 1-based.
    regions.emplace_back(2, 2);
    regions.emplace_back(6, 7);

    scheduler_t scheduler(/*verbosity=*/4);
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(input), std::move(end), 1);
    sched_inputs[0].thread_modifiers.push_back(scheduler_t::input_thread_info_t(regions));
    scheduler_t::scheduler_options_t sched_ops(
        scheduler_t::STREAM_BY_SYNTHETIC_CPU,
        scheduler_t::SCHEDULE_INTERLEAVE_AS_RECORDED);
    if (scheduler.init(sched_inputs, 1, sched_ops) != scheduler_t::STATUS_SUCCESS)
        assert(false);
    int ordinal = 0;
    auto *stream = scheduler.get_stream(0);
    while (true) {
        memref_t memref;
        scheduler_t::stream_status_t status = stream->next_record(memref);
        if (status == scheduler_t::STATUS_EOF)
            break;
        assert(status == scheduler_t::STATUS_OK);
        switch (ordinal) {
        case 0:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr = 2);
            break;
        case 1:
            assert(memref.marker.type == TRACE_TYPE_MARKER);
            assert(memref.marker.marker_type == TRACE_MARKER_TYPE_WINDOW_ID);
            assert(memref.marker.marker_value == 1);
            break;
        case 2:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr = 6);
            break;
        case 3:
            assert(type_is_instr(memref.instr.type));
            assert(memref.instr.addr = 7);
            break;
        default: assert(ordinal == 4); assert(memref.exit.type == TRACE_TYPE_THREAD_EXIT);
        }
        ++ordinal;
    }
    assert(ordinal == 4);
}

} // namespace

int
main(int argc, const char *argv[])
{
    test_parallel();
    test_param_checks();
    test_regions();
    return 0;
}
