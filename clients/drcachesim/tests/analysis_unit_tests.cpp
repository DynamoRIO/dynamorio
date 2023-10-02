/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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

/* Unit tests for trace analysis APIs. */

#include "analyzer.h"
#include "mock_reader.h"
#include "scheduler.h"

#include <assert.h>

#include <iostream>
#include <vector>

namespace dynamorio {
namespace drmemtrace {

// An analyzer that takes in any number of scheduler inputs, plus optional direct
// scheduler options for SHARD_BY_CORE.
class mock_analyzer_t : public analyzer_t {
public:
    mock_analyzer_t(std::vector<scheduler_t::input_workload_t> &sched_inputs,
                    analysis_tool_t **tools, int num_tools, bool parallel,
                    int worker_count, scheduler_t::scheduler_options_t *sched_ops_in)
        : analyzer_t()
    {
        num_tools_ = num_tools;
        tools_ = tools;
        parallel_ = parallel;
        verbosity_ = 1;
        worker_count_ = worker_count;
        scheduler_t::scheduler_options_t sched_ops;
        if (sched_ops_in != nullptr) {
            shard_type_ = SHARD_BY_CORE;
            sched_ops = *sched_ops_in;
            // XXX: We could refactor init_scheduler_common() to share a couple of
            // these lines.
            if (sched_ops.quantum_unit == sched_type_t::QUANTUM_TIME)
                sched_by_time_ = true;
        } else if (parallel)
            sched_ops = scheduler_t::make_scheduler_parallel_options(verbosity_);
        else
            sched_ops = scheduler_t::make_scheduler_serial_options(verbosity_);
        if (scheduler_.init(sched_inputs, worker_count_, sched_ops) !=
            sched_type_t::STATUS_SUCCESS) {
            assert(false);
            success_ = false;
        }
        for (int i = 0; i < worker_count_; ++i) {
            worker_data_.push_back(analyzer_worker_data_t(i, scheduler_.get_stream(i)));
        }
    }
};

bool
test_queries()
{
    std::cerr << "\n----------------\nTesting queries\n";
    std::vector<trace_entry_t> input_sequence = {
        make_thread(/*tid=*/1),
        make_pid(/*pid=*/1),
        make_instr(/*pc=*/42),
        make_exit(/*tid=*/1),
    };
    static constexpr int NUM_INPUTS = 3;
    static constexpr int NUM_OUTPUTS = 2;
    static constexpr int BASE_TID = 100;
    std::vector<trace_entry_t> inputs[NUM_INPUTS];
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    for (int i = 0; i < NUM_INPUTS; i++) {
        memref_tid_t tid = BASE_TID + i;
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
    scheduler_t::scheduler_options_t sched_ops(
        scheduler_t::MAP_TO_ANY_OUTPUT, scheduler_t::DEPENDENCY_IGNORE,
        scheduler_t::SCHEDULER_DEFAULTS, /*verbosity=*/3);

    class test_tool_t : public analysis_tool_t {
    public:
        bool
        process_memref(const memref_t &memref) override
        {
            assert(false); // Only expect parallel mode.
            return false;
        }
        bool
        print_results() override
        {
            return true;
        }
        bool
        parallel_shard_supported() override
        {
            return true;
        }
        void *
        parallel_shard_init_stream(int shard_index, void *worker_data,
                                   memtrace_stream_t *stream) override
        {
            auto per_shard = new per_shard_t;
            per_shard->index = shard_index;
            per_shard->stream = stream;
            return reinterpret_cast<void *>(per_shard);
        }
        bool
        parallel_shard_exit(void *shard_data) override
        {
            per_shard_t *shard = reinterpret_cast<per_shard_t *>(shard_data);
            delete shard;
            return true;
        }
        bool
        parallel_shard_memref(void *shard_data, const memref_t &memref) override
        {
            per_shard_t *shard = reinterpret_cast<per_shard_t *>(shard_data);
            // These are our testing goals: these queries.
            // We have one thread for each of our NUM_INPUTS workloads.
            assert(shard->stream->get_output_cpuid() == shard->index);
            // We have just one thread per workload, so they're the same.
            assert(shard->stream->get_workload_id() == memref.instr.tid - BASE_TID);
            assert(shard->stream->get_input_id() == memref.instr.tid - BASE_TID);
            return true;
        }

    private:
        struct per_shard_t {
            int index;
            memtrace_stream_t *stream;
        };
    };

    std::vector<analysis_tool_t *> tools;
    auto test_tool = std::unique_ptr<test_tool_t>(new test_tool_t);
    tools.push_back(test_tool.get());
    mock_analyzer_t analyzer(sched_inputs, &tools[0], (int)tools.size(),
                             /*parallel=*/true, NUM_OUTPUTS, &sched_ops);
    assert(!!analyzer);
    bool res = analyzer.run();
    assert(res);
    return true;
}

int
test_main(int argc, const char *argv[])
{
    if (!test_queries())
        return 1;
    std::cerr << "All done!\n";
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
