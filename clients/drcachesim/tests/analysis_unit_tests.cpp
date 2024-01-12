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

#include <assert.h>

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

#include "analyzer.h"
#include "mock_reader.h"
#include "scheduler.h"
#ifdef HAS_ZIP
#    include "zipfile_istream.h"
#    include "zipfile_ostream.h"
#endif

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
            sched_ops = std::move(*sched_ops_in);
            // XXX: We could refactor init_scheduler_common() to share a couple of
            // these lines.
            if (sched_ops.quantum_unit == sched_type_t::QUANTUM_TIME)
                sched_by_time_ = true;
        } else if (parallel)
            sched_ops = scheduler_t::make_scheduler_parallel_options(verbosity_);
        else
            sched_ops = scheduler_t::make_scheduler_serial_options(verbosity_);
        if (scheduler_.init(sched_inputs, worker_count_, std::move(sched_ops)) !=
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
            if (memref.marker.type == TRACE_TYPE_MARKER &&
                (memref.marker.marker_type == TRACE_MARKER_TYPE_CORE_WAIT ||
                 memref.marker.marker_type == TRACE_MARKER_TYPE_CORE_IDLE))
                return true;
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

bool
test_wait_records()
{
#ifdef HAS_ZIP
    std::cerr << "\n----------------\nTesting wait records\n";

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

    // Synthesize a cpu-schedule file with some waits in it, if run in lockstep.
    // In pure lockstep it looks like this with a - for a wait and . for a
    // non-instruction record, to help understand the file entries below:
    //   core0: "EEE-AAA-CCCAAACCCBBB.DDD."
    //   core1: "---EEE.BBBDDDBBBDDDAAA.CCC."
    std::string cpu_fname = "tmp_test_wait_records.zip";
    {
        // Instr counts are 1-based, but the first lists 0 (really starts at 1).
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
                                               /*verbosity=*/1);
    zipfile_istream_t infile(cpu_fname);
    sched_ops.replay_as_traced_istream = &infile;

    class test_tool_t : public analysis_tool_t {
    public:
        // Caller must pre-allocate the vector with a slot per output stream.
        test_tool_t(std::vector<std::string> *schedule_strings)
            : schedule_strings_(schedule_strings)
        {
        }
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
            (*schedule_strings_)[shard->index] = shard->schedule;
            delete shard;
            return true;
        }
        bool
        parallel_shard_memref(void *shard_data, const memref_t &memref) override
        {
            per_shard_t *shard = reinterpret_cast<per_shard_t *>(shard_data);
            // We run in *rough* lockstep to avoid a flaky test: we just need to
            // avoid the 2nd output making it through several initial records
            // before the 1st output runs and sees a STATUS_WAIT.
            static constexpr int MAX_WAITS = 100000;
            int waits;
            while (global_records_ < 3 * shard->records / 2) {
                std::this_thread::yield();
                // Avoid a hang.  It shouldn't happen with these inputs though.
                if (++waits > MAX_WAITS)
                    break;
            }
            ++shard->records;
            ++global_records_;
            if (memref.marker.type == TRACE_TYPE_MARKER &&
                memref.marker.marker_type == TRACE_MARKER_TYPE_CORE_WAIT) {
                shard->schedule += '-';
                return true;
            }
            int64_t input = shard->stream->get_input_id();
            shard->schedule += 'A' + static_cast<char>(input % 26);
            return true;
        }

    private:
        struct per_shard_t {
            int index;
            memtrace_stream_t *stream;
            std::string schedule;
            int64_t records = 0;
        };
        std::atomic<int64_t> global_records_ { 0 };
        std::vector<std::string> *schedule_strings_;
    };

    std::vector<std::string> schedule_strings(NUM_OUTPUTS);
    std::vector<analysis_tool_t *> tools;
    auto test_tool = std::unique_ptr<test_tool_t>(new test_tool_t(&schedule_strings));
    tools.push_back(test_tool.get());
    mock_analyzer_t analyzer(sched_inputs, &tools[0], (int)tools.size(),
                             /*parallel=*/true, NUM_OUTPUTS, &sched_ops);
    assert(!!analyzer);
    bool res = analyzer.run();
    assert(res);
    for (const auto &sched : schedule_strings) {
        std::cerr << "Schedule: " << sched << "\n";
    }
    // Due to non-determinism we can't put too many restrictions here so we
    // just ensure we saw at least one wait at the start.
    assert(schedule_strings[1][0] == '-');
#endif
    return true;
}

int
test_main(int argc, const char *argv[])
{
    if (!test_queries() || !test_wait_records())
        return 1;
    std::cerr << "All done!\n";
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
