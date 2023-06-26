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

/* Unit tests for the trace interval analysis APIs in analysis_tool_t. */

#include "analyzer.h"
#include "memref_gen.h"

#include <algorithm>
#include <inttypes.h>
#include <iostream>
#include <vector>

namespace dynamorio {
namespace drmemtrace {

#define FATAL_ERROR(msg, ...)                               \
    do {                                                    \
        fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__); \
        fflush(stderr);                                     \
        exit(1);                                            \
    } while (0)

#define CHECK(cond, msg, ...)             \
    do {                                  \
        if (!(cond)) {                    \
            fprintf(stderr, "%s\n", msg); \
            return false;                 \
        }                                 \
    } while (0)

// Test scheduler_t::stream_t that simply returns the provided memref_t
// elements when next_record is invoked.
class test_stream_t : public scheduler_t::stream_t {
public:
    test_stream_t(const std::vector<memref_t> &refs, bool parallel)
        : stream_t(nullptr, 0, 0)
        , refs_(refs)
        , at_(-1)
        , parallel_(parallel)
        , instr_count_(0)
        , first_timestamp_(0)
    {
    }
    scheduler_t::stream_status_t
    next_record(memref_t &record) override
    {
        if (at_ + 1 < refs_.size()) {
            ++at_;
            record = refs_[at_];
            if (tid2ordinal.find(record.instr.tid) == tid2ordinal.end()) {
                tid2ordinal[record.instr.tid] = tid2ordinal.size();
            }
            if (record.marker.type == TRACE_TYPE_MARKER &&
                record.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP) {
                last_timestamps_[record.marker.tid] = record.marker.marker_value;
                if (first_timestamps_[record.marker.tid] == 0)
                    first_timestamps_[record.marker.tid] = record.marker.marker_value;
                last_timestamp_ = record.marker.marker_value;
                if (first_timestamp_ == 0)
                    first_timestamp_ = last_timestamp_;
            } else if (type_is_instr(record.instr.type)) {
                ++instr_count_;
                ++instr_counts_[record.instr.tid];
            }
            return scheduler_t::stream_status_t::STATUS_OK;
        }
        return scheduler_t::stream_status_t::STATUS_EOF;
    }
    std::string
    get_stream_name() const override
    {
        return "test_stream";
    }
    scheduler_t::input_ordinal_t
    get_input_stream_ordinal() override
    {
        assert(at_ >= 0 && at_ < refs_.size());
        // Each TID forms a separate input stream.
        return tid2ordinal[refs_[at_].instr.tid];
    }
    uint64_t
    get_first_timestamp() const override
    {
        if (!parallel_)
            return first_timestamp_;
        assert(at_ >= 0 && at_ < refs_.size());
        auto it = first_timestamps_.find(refs_[at_].instr.tid);
        assert(it != first_timestamps_.end());
        return it->second;
    }
    uint64_t
    get_last_timestamp() const override
    {
        if (!parallel_)
            return last_timestamp_;
        assert(at_ >= 0 && at_ < refs_.size());
        auto it = last_timestamps_.find(refs_[at_].instr.tid);
        assert(it != last_timestamps_.end());
        return it->second;
    }
    uint64_t
    get_instruction_ordinal() const override
    {
        if (!parallel_)
            return instr_count_;
        assert(at_ >= 0 && at_ < refs_.size());
        auto it = instr_counts_.find(refs_[at_].instr.tid);
        assert(it != instr_counts_.end());
        return it->second;
    }

private:
    std::unordered_map<memref_tid_t, scheduler_t::input_ordinal_t> tid2ordinal;
    std::vector<memref_t> refs_;
    int at_;
    bool parallel_;

    // Values tracked per thread.
    std::unordered_map<memref_tid_t, uint64_t> instr_counts_;
    std::unordered_map<memref_tid_t, uint64_t> first_timestamps_;
    std::unordered_map<memref_tid_t, uint64_t> last_timestamps_;

    // Values tracked for the whole trace.
    uint64_t instr_count_;
    uint64_t first_timestamp_;
    uint64_t last_timestamp_;
};

// Test analyzer_t that uses a test_stream_t instead of the stream provided
// by a scheduler_t.
class test_analyzer_t : public analyzer_t {
public:
    test_analyzer_t(const std::vector<memref_t> &refs, analysis_tool_t **tools,
                    int num_tools, bool parallel, uint64_t interval_microseconds)
        : analyzer_t()
    {
        num_tools_ = num_tools;
        tools_ = tools;
        parallel_ = parallel;
        interval_microseconds_ = interval_microseconds;
        verbosity_ = 1;
        worker_count_ = 1;
        test_stream_ =
            std::unique_ptr<scheduler_t::stream_t>(new test_stream_t(refs, parallel));
        worker_data_.push_back(analyzer_worker_data_t(0, test_stream_.get()));
    }

private:
    std::unique_ptr<scheduler_t::stream_t> test_stream_;
};

// Dummy analysis_tool_t that does not provide interval results. This helps verify
// the case when one of the tools run does not implement the interval related APIs.
class dummy_analysis_tool_t : public analysis_tool_t {
public:
    dummy_analysis_tool_t()
        : generate_snapshot_count_(0)
    {
    }
    bool
    process_memref(const memref_t &memref) override
    {
        return true;
    }
    analysis_tool_t::interval_state_snapshot_t *
    generate_interval_snapshot(uint64_t interval_id) override
    {
        ++generate_snapshot_count_;
        return nullptr;
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
    parallel_shard_init(int shard_index, void *worker_data) override
    {
        return nullptr;
    }
    bool
    parallel_shard_exit(void *shard_data) override
    {
        return true;
    }
    bool
    parallel_shard_memref(void *shard_data, const memref_t &memref) override
    {
        return true;
    }
    analysis_tool_t::interval_state_snapshot_t *
    generate_shard_interval_snapshot(void *shard_data, uint64_t interval_id) override
    {
        ++generate_snapshot_count_;
        return nullptr;
    }
    analysis_tool_t::interval_state_snapshot_t *
    combine_interval_snapshots(
        const std::vector<const analysis_tool_t::interval_state_snapshot_t *>
            latest_shard_snapshots,
        uint64_t interval_end_timestamp) override
    {
        FATAL_ERROR("Did not expect combine_interval_snapshots to be invoked");
        return nullptr;
    }
    bool
    print_interval_results(
        const std::vector<interval_state_snapshot_t *> &snapshots) override
    {
        FATAL_ERROR("Did not expect print_interval_results to be invoked");
        return true;
    }
    bool
    release_interval_snapshot(
        analysis_tool_t::interval_state_snapshot_t *snapshot) override
    {
        FATAL_ERROR("Did not expect release_interval_snapshot to be invoked");
        return true;
    }
    int
    get_generate_snapshot_count() const
    {
        return generate_snapshot_count_;
    }

private:
    int generate_snapshot_count_;
};

// Test analysis_tool_t that records information about when the
// generate_shard_interval_snapshot and generate_interval_snapshot APIs were invoked.
class test_analysis_tool_t : public analysis_tool_t {
public:
    // Describes the point in trace when an interval ends. This is same as the point
    // when the generate_*interval_snapshot API is invoked.
    struct interval_end_point_t {
        memref_tid_t tid;
        int seen_memrefs; // For parallel mode, this is the shard-local count.
        uint64_t interval_id;

        bool
        operator==(const interval_end_point_t &rhs) const
        {
            return tid == rhs.tid && seen_memrefs == rhs.seen_memrefs &&
                interval_id == rhs.interval_id;
        }
        bool
        operator<(const interval_end_point_t &rhs) const
        {
            if (tid != rhs.tid)
                return tid < rhs.tid;
            if (seen_memrefs != rhs.seen_memrefs)
                return seen_memrefs < rhs.seen_memrefs;
            return interval_id < rhs.interval_id;
        }
    };

    // Describes the state recorded by test_analysis_tool_t at the end of each
    // interval.
    struct recorded_snapshot_t : public analysis_tool_t::interval_state_snapshot_t {
        recorded_snapshot_t(uint64_t interval_id, uint64_t interval_end_timestamp,
                            uint64_t instr_count_cumulative, uint64_t instr_count_delta,
                            std::vector<interval_end_point_t> component_intervals)
            // Actual tools do not need to supply values to construct the base
            // interval_state_snapshot_t. This is only to make it easier to construct
            // the expected snapshot objects in this test.
            // Since the final verification happens only for the whole trace intervals,
            // we simply use WHOLE_TRACE_SHARD_ID here and for tool_shard_id below.
            : interval_state_snapshot_t(WHOLE_TRACE_SHARD_ID, interval_id,
                                        interval_end_timestamp, instr_count_cumulative,
                                        instr_count_delta)
            , component_intervals(component_intervals)
            , tool_shard_id(WHOLE_TRACE_SHARD_ID)
        {
        }
        recorded_snapshot_t()
        {
        }

        bool
        operator==(const recorded_snapshot_t &rhs) const
        {
            return shard_id == rhs.shard_id && tool_shard_id == rhs.tool_shard_id &&
                interval_id == rhs.interval_id &&
                interval_end_timestamp == rhs.interval_end_timestamp &&
                instr_count_cumulative == rhs.instr_count_cumulative &&
                instr_count_delta == rhs.instr_count_delta &&
                component_intervals == rhs.component_intervals;
        }
        void
        print() const
        {
            std::cerr << "(shard_id: " << shard_id << ", interval_id: " << interval_id
                      << ", tool_shard_id: " << tool_shard_id
                      << ", end_timestamp: " << interval_end_timestamp
                      << ", instr_count_cumulative: " << instr_count_cumulative
                      << ", instr_count_delta: " << instr_count_delta
                      << ", component_intervals: ";
            for (const auto &s : component_intervals) {
                std::cerr << "(tid:" << s.tid << ", seen_memrefs:" << s.seen_memrefs
                          << ", interval_id:" << s.interval_id << "),";
            }
            std::cerr << ")\n";
        }

        // Stores the list of intervals that were combined to produce this snapshot.
        // In the serial case, this contains just a single value. In the parallel case,
        // this contains a list of size equal to the count of shard interval snapshots
        // that were combined to create this snapshot.
        std::vector<interval_end_point_t> component_intervals;
        // Stores the shard_id recorded by the test tool. Compared with the shard_id
        // stored by the framework in the base struct.
        int64_t tool_shard_id;
    };

    // Constructs an analysis_tool_t that expects the given interval state snapshots to be
    // produced.
    test_analysis_tool_t(const std::vector<recorded_snapshot_t> &expected_state_snapshots,
                         bool combine_only_active_shards)
        : seen_memrefs_(0)
        , expected_state_snapshots_(expected_state_snapshots)
        , outstanding_snapshots_(0)
        , combine_only_active_shards_(combine_only_active_shards)
    {
    }
    bool
    process_memref(const memref_t &memref) override
    {
        ++seen_memrefs_;
        return true;
    }
    analysis_tool_t::interval_state_snapshot_t *
    generate_interval_snapshot(uint64_t interval_id) override
    {
        auto *snapshot = new recorded_snapshot_t();
        snapshot->tool_shard_id =
            analysis_tool_t::interval_state_snapshot_t::WHOLE_TRACE_SHARD_ID;
        snapshot->component_intervals.push_back(
            { /*tid=*/0, seen_memrefs_, interval_id });
        ++outstanding_snapshots_;
        return snapshot;
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
    parallel_shard_init(int shard_index, void *worker_data) override
    {
        auto per_shard = new per_shard_t;
        per_shard->magic_num = kMagicNum;
        per_shard->tid = kInvalidTid;
        per_shard->seen_memrefs = 0;
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
        ++shard->seen_memrefs;
        if (shard->tid == kInvalidTid)
            shard->tid = memref.data.tid;
        else if (shard->tid != memref.data.tid) {
            FATAL_ERROR("Unexpected TID in memref");
        }
        return true;
    }
    analysis_tool_t::interval_state_snapshot_t *
    generate_shard_interval_snapshot(void *shard_data, uint64_t interval_id) override
    {
        per_shard_t *shard = reinterpret_cast<per_shard_t *>(shard_data);
        if (shard->magic_num != kMagicNum) {
            FATAL_ERROR("Invalid shard_data");
        }
        if (shard->tid == kInvalidTid)
            FATAL_ERROR("Expected TID to be found by now");
        auto *snapshot = new recorded_snapshot_t();
        snapshot->tool_shard_id = shard->tid;
        snapshot->component_intervals.push_back(
            { shard->tid, shard->seen_memrefs, interval_id });
        ++outstanding_snapshots_;
        return snapshot;
    }
    analysis_tool_t::interval_state_snapshot_t *
    combine_interval_snapshots(
        const std::vector<const analysis_tool_t::interval_state_snapshot_t *>
            latest_shard_snapshots,
        uint64_t interval_end_timestamp) override
    {
        recorded_snapshot_t *result = new recorded_snapshot_t();
        result->tool_shard_id =
            analysis_tool_t::interval_state_snapshot_t::WHOLE_TRACE_SHARD_ID;
        ++outstanding_snapshots_;
        for (auto snapshot : latest_shard_snapshots) {
            if (snapshot != nullptr &&
                (!combine_only_active_shards_ ||
                 snapshot->interval_end_timestamp == interval_end_timestamp)) {
                auto recorded_snapshot =
                    dynamic_cast<const recorded_snapshot_t *const>(snapshot);
                if (recorded_snapshot->tool_shard_id != recorded_snapshot->shard_id) {
                    FATAL_ERROR("shard_id stored by tool (%" PRIi64
                                ") and framework (%" PRIi64 ") mismatch",
                                recorded_snapshot->tool_shard_id,
                                recorded_snapshot->shard_id);
                    return nullptr;
                }
                result->component_intervals.insert(
                    result->component_intervals.end(),
                    recorded_snapshot->component_intervals.begin(),
                    recorded_snapshot->component_intervals.end());
            }
        }
        return result;
    }
    bool
    compare_results(std::vector<recorded_snapshot_t *> &one,
                    std::vector<recorded_snapshot_t> &two)
    {
        if (one.size() != two.size())
            return false;
        for (int i = 0; i < one.size(); ++i) {
            std::sort(one[i]->component_intervals.begin(),
                      one[i]->component_intervals.end());
            std::sort(two[i].component_intervals.begin(),
                      two[i].component_intervals.end());
            if (!(*one[i] == two[i]))
                return false;
        }
        return true;
    }
    bool
    print_interval_results(
        const std::vector<interval_state_snapshot_t *> &snapshots) override
    {
        std::vector<recorded_snapshot_t *> recorded_state_snapshots;
        for (const auto &p : snapshots) {
            recorded_state_snapshots.push_back(
                reinterpret_cast<recorded_snapshot_t *>(p));
        }
        if (!compare_results(recorded_state_snapshots, expected_state_snapshots_)) {
            error_string_ = "Unexpected state snapshots";
            std::cerr << "Expected:\n";
            for (const auto &snapshot : expected_state_snapshots_)
                snapshot.print();
            std::cerr << "Found:\n";
            for (const auto &snapshot : recorded_state_snapshots)
                snapshot->print();
            return false;
        }
        return true;
    }
    bool
    release_interval_snapshot(
        analysis_tool_t::interval_state_snapshot_t *snapshot) override
    {
        delete snapshot;
        --outstanding_snapshots_;
        return true;
    }
    int
    get_outstanding_snapshot_count() const
    {
        return outstanding_snapshots_;
    }

private:
    int seen_memrefs_;
    std::vector<recorded_snapshot_t> expected_state_snapshots_;
    int outstanding_snapshots_;
    bool combine_only_active_shards_;

    // Data tracked per shard.
    struct per_shard_t {
        memref_tid_t tid;
        uintptr_t magic_num;
        int seen_memrefs;
    };

    static constexpr uintptr_t kMagicNum = 0x8badf00d;
    static constexpr memref_tid_t kInvalidTid = -1;
};

static bool
test_non_zero_interval(bool parallel, bool combine_only_active_shards = true)
{
    constexpr uint64_t kIntervalMicroseconds = 100;
    std::vector<memref_t> refs = {
        // Trace for a single worker which has two constituent shards. (scheduler_t
        // does not guarantee that workers will process shards one after the other.)
        // Expected active interval_id: tid_51_local | tid_52_local | whole_trace
        gen_marker(51, TRACE_MARKER_TYPE_TIMESTAMP, 40),  // 1 | _ | 1
        gen_instr(51, 10000),                             // 1 | _ | 1
        gen_data(51, true, 1234, 4),                      // 1 | _ | 1
        gen_marker(52, TRACE_MARKER_TYPE_TIMESTAMP, 151), // _ | 1 | 2
        gen_instr(52, 20000),                             // _ | 1 | 2
        gen_marker(51, TRACE_MARKER_TYPE_TIMESTAMP, 170), // 2 | _ | 2
        gen_instr(51, 10008),                             // 2 | _ | 2
        gen_marker(51, TRACE_MARKER_TYPE_TIMESTAMP, 201), // 3 | _ | 3
        gen_instr(51, 20004),                             // 3 | _ | 3
        gen_marker(52, TRACE_MARKER_TYPE_TIMESTAMP, 210), // _ | 2 | 3
        gen_instr(52, 20008),                             // _ | 2 | 3
        gen_marker(52, TRACE_MARKER_TYPE_TIMESTAMP, 270), // _ | 2 | 3
        gen_instr(52, 20008),                             // _ | 2 | 3
        gen_marker(52, TRACE_MARKER_TYPE_TIMESTAMP, 490), // _ | 4 | 5
        gen_instr(52, 20012),                             // _ | 4 | 5
        gen_marker(51, TRACE_MARKER_TYPE_TIMESTAMP, 590), // 6 | _ | 6
        gen_exit(51),                                     // 6 | _ | 6
        gen_marker(52, TRACE_MARKER_TYPE_TIMESTAMP, 610), // _ | 6 | 7
        gen_instr(52, 20016),                             // _ | 6 | 7
        gen_exit(52)                                      // _ | 6 | 7
    };

    std::vector<test_analysis_tool_t::recorded_snapshot_t> expected_state_snapshots;
    if (!parallel) {
        // Each whole trace interval is made up of only one snapshot, the
        // serial snapshot.
        expected_state_snapshots = {
            // Format for interval_end_point_t: <tid, seen_memrefs, interval_id>
            test_analysis_tool_t::recorded_snapshot_t(1, 100, 1, 1, { { 0, 3, 1 } }),
            test_analysis_tool_t::recorded_snapshot_t(2, 200, 3, 2, { { 0, 7, 2 } }),
            test_analysis_tool_t::recorded_snapshot_t(3, 300, 6, 3, { { 0, 13, 3 } }),
            test_analysis_tool_t::recorded_snapshot_t(5, 500, 7, 1, { { 0, 15, 5 } }),
            test_analysis_tool_t::recorded_snapshot_t(6, 600, 7, 0, { { 0, 17, 6 } }),
            test_analysis_tool_t::recorded_snapshot_t(7, 700, 8, 1, { { 0, 20, 7 } }),
        };
    } else if (combine_only_active_shards) {
        // Each whole trace interval is made up of snapshots from each
        // shard that was active in that interval.
        expected_state_snapshots = {
            // Format for interval_end_point_t: <tid, seen_memrefs, interval_id>
            test_analysis_tool_t::recorded_snapshot_t(1, 100, 1, 1, { { 51, 3, 1 } }),
            // Narration: The whole-trace interval_id=1 with interval_end_timestamp=200
            // is made up of the following two shard-local interval snapshots:
            // - from shard_id=51, the interval_id=1 that ends at the local_memref=5
            // - from shard_id=52, the interval_id=0 that ends at the local_memref=2
            test_analysis_tool_t::recorded_snapshot_t(2, 200, 3, 2,
                                                      { { 51, 5, 2 }, { 52, 2, 1 } }),
            test_analysis_tool_t::recorded_snapshot_t(3, 300, 6, 3,
                                                      { { 51, 7, 3 }, { 52, 6, 2 } }),
            test_analysis_tool_t::recorded_snapshot_t(5, 500, 7, 1, { { 52, 8, 4 } }),
            test_analysis_tool_t::recorded_snapshot_t(6, 600, 7, 0, { { 51, 9, 6 } }),
            test_analysis_tool_t::recorded_snapshot_t(7, 700, 8, 1, { { 52, 11, 6 } })
        };
    } else {
        // Each whole trace interval is made up of last snapshots from all trace shards.
        expected_state_snapshots = {
            // Format for interval_end_point_t: <tid, seen_memrefs, interval_id>
            test_analysis_tool_t::recorded_snapshot_t(1, 100, 1, 1, { { 51, 3, 1 } }),
            test_analysis_tool_t::recorded_snapshot_t(2, 200, 3, 2,
                                                      { { 51, 5, 2 }, { 52, 2, 1 } }),
            test_analysis_tool_t::recorded_snapshot_t(3, 300, 6, 3,
                                                      { { 51, 7, 3 }, { 52, 6, 2 } }),
            test_analysis_tool_t::recorded_snapshot_t(5, 500, 7, 1,
                                                      { { 51, 7, 3 }, { 52, 8, 4 } }),
            test_analysis_tool_t::recorded_snapshot_t(6, 600, 7, 0,
                                                      { { 51, 9, 6 }, { 52, 8, 4 } }),
            test_analysis_tool_t::recorded_snapshot_t(7, 700, 8, 1,
                                                      { { 51, 9, 6 }, { 52, 11, 6 } })
        };
    }
    std::vector<analysis_tool_t *> tools;
    auto test_analysis_tool = std::unique_ptr<test_analysis_tool_t>(
        new test_analysis_tool_t(expected_state_snapshots, combine_only_active_shards));
    tools.push_back(test_analysis_tool.get());
    auto dummy_analysis_tool =
        std::unique_ptr<dummy_analysis_tool_t>(new dummy_analysis_tool_t());
    tools.push_back(dummy_analysis_tool.get());
    test_analyzer_t test_analyzer(refs, &tools[0], (int)tools.size(), parallel,
                                  kIntervalMicroseconds);
    if (!test_analyzer) {
        FATAL_ERROR("failed to initialize test analyzer: %s",
                    test_analyzer.get_error_string().c_str());
    }
    if (!test_analyzer.run()) {
        FATAL_ERROR("failed to run test_analyzer: %s",
                    test_analyzer.get_error_string().c_str());
    }
    if (!test_analyzer.print_stats()) {
        FATAL_ERROR("failed to print stats: %s",
                    test_analyzer.get_error_string().c_str());
    }
    if (test_analysis_tool.get()->get_outstanding_snapshot_count() != 0) {
        std::cerr << "Failed to release all outstanding snapshots: "
                  << test_analysis_tool.get()->get_outstanding_snapshot_count()
                  << " left\n";
        return false;
    }
    int expected_generate_call_count = parallel ? 8 : 6;
    if (dummy_analysis_tool.get()->get_generate_snapshot_count() !=
        expected_generate_call_count) {
        std::cerr << "Dummy analysis tool got "
                  << dummy_analysis_tool.get()->get_generate_snapshot_count()
                  << " interval API calls, but expected " << expected_generate_call_count
                  << "\n";
        return false;
    }
    fprintf(
        stderr,
        "test_non_zero_interval done for parallel=%d, combine_only_active_shards=%d\n",
        parallel, combine_only_active_shards);
    return true;
}

int
test_main(int argc, const char *argv[])
{
    if (!test_non_zero_interval(false) || !test_non_zero_interval(true, true) ||
        !test_non_zero_interval(true, false))
        return 1;
    fprintf(stderr, "All done!\n");
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
