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

/* Unit tests for the interval result generation API in analysis_tool_t. */

#include "analyzer.h"
#include "memref_gen.h"

#include <algorithm>
#include <iostream>
#include <vector>

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
        , first_timestamp_(0)
        , at_(0)
        , parallel_(parallel)
    {
    }
    scheduler_t::stream_status_t
    next_record(memref_t &record) override
    {
        if (at_ < refs_.size()) {
            record = refs_[at_++];
            if (record.marker.type == TRACE_TYPE_MARKER &&
                record.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP) {
                last_timestamps_[record.marker.tid] = record.marker.marker_value;
                if (first_timestamps_[record.marker.tid] == 0)
                    first_timestamps_[record.marker.tid] = record.marker.marker_value;
                last_timestamp_ = record.marker.marker_value;
                if (first_timestamp_ == 0)
                    first_timestamp_ = last_timestamp_;
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
        assert(at_ > 0);
        // All TIDs form a separate input stream.
        return refs_[at_ - 1].instr.tid;
    }
    uint64_t
    get_first_timestamp() const override
    {
        if (!parallel_)
            return first_timestamp_;
        assert(at_ > 0);
        auto it = first_timestamps_.find(refs_[at_ - 1].instr.tid);
        assert(it != first_timestamps_.end());
        return it->second;
    }
    uint64_t
    get_last_timestamp() const override
    {
        if (!parallel_)
            return last_timestamp_;
        assert(at_ > 0);
        auto it = last_timestamps_.find(refs_[at_ - 1].instr.tid);
        assert(it != last_timestamps_.end());
        return it->second;
    }

private:
    std::vector<memref_t> refs_;
    std::unordered_map<memref_tid_t, uint64_t> first_timestamps_;
    std::unordered_map<memref_tid_t, uint64_t> last_timestamps_;
    uint64_t first_timestamp_;
    uint64_t last_timestamp_;
    size_t at_;
    bool parallel_;
};

// Test analyzer_t that uses a test_stream_t instead of a stream provided
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
        verbosity_ = 2;
        worker_count_ = 1;
        test_stream_ =
            std::unique_ptr<scheduler_t::stream_t>(new test_stream_t(refs, parallel));
        worker_data_.push_back(analyzer_worker_data_t(0, test_stream_.get()));
    }

private:
    std::unique_ptr<scheduler_t::stream_t> test_stream_;
};

// Test analysis_tool_t that stores information about when the interval-end
// events (generate_shard_interval_result and generate_interval_result) were invoked.
class test_analysis_tool_t : public analysis_tool_t {
public:
    // Describes the point in trace when an interval ends.
    struct interval_end_point_t {
        memref_tid_t shard_id_;
        int seen_memrefs_;
        uint64_t interval_id_;

        bool
        operator==(const interval_end_point_t &rhs) const
        {
            return shard_id_ == rhs.shard_id_ && seen_memrefs_ == rhs.seen_memrefs_ &&
                interval_id_ == rhs.interval_id_;
        }
        bool
        operator<(const interval_end_point_t &rhs) const
        {
            if (shard_id_ != rhs.shard_id_)
                return shard_id_ < rhs.shard_id_;
            if (seen_memrefs_ != rhs.seen_memrefs_)
                return seen_memrefs_ < rhs.seen_memrefs_;
            return interval_id_ < rhs.interval_id_;
        }
    };

    // Describes the state recorded by test_analysis_tool_t at the end of each
    // interval.
    struct recorded_snapshot_t : public analysis_tool_t::interval_state_snapshot_t {
        recorded_snapshot_t(int shard_id, uint64_t interval_id,
                            uint64_t interval_end_timestamp,
                            std::vector<interval_end_point_t> component_intervals)
            : interval_state_snapshot_t(shard_id, interval_id, interval_end_timestamp)
            , component_intervals_(component_intervals)
        {
        }
        recorded_snapshot_t()
        {
        }

        bool
        operator==(const recorded_snapshot_t &rhs) const
        {
            return shard_id_ == rhs.shard_id_ && interval_id_ == rhs.interval_id_ &&
                interval_end_timestamp_ == rhs.interval_end_timestamp_ &&
                component_intervals_ == rhs.component_intervals_;
        }
        void
        print() const
        {
            std::cerr << "(shard: " << shard_id_ << ", interval_id: " << interval_id_
                      << ", interval_end_timestamp_: " << interval_end_timestamp_
                      << ", component_intervals: ";
            for (const auto &s : component_intervals_) {
                std::cerr << "(" << s.shard_id_ << "," << s.seen_memrefs_ << ","
                          << s.interval_id_ << "),";
            }
            std::cerr << ")\n";
        }

        // Stores the list of intervals that were combined to produce this snapshot.
        // In the serial case, this contains just a single value. In the parallel case,
        // this contains a list of size equal to the count of shard intervals that were
        // combined to create this snapshot.
        std::vector<interval_end_point_t> component_intervals_;
    };

    // Constructs an analysis_tool_t that expects the given state snapshots to be
    // produced.
    test_analysis_tool_t(std::vector<recorded_snapshot_t> expected_state_snapshots)
        : seen_memrefs_(0)
        , expected_state_snapshots_(expected_state_snapshots)
        , outstanding_snapshots_(0)
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
        ++outstanding_snapshots_;
        snapshot->component_intervals_.push_back(
            { /*tid=*/0, seen_memrefs_, interval_id });
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
        per_shard->magic_num_ = kMagicNum;
        per_shard->shard_id_ = kInvalidTid;
        per_shard->seen_memrefs_ = 0;
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
        ++shard->seen_memrefs_;
        if (shard->shard_id_ == kInvalidTid)
            shard->shard_id_ = memref.data.tid;
        else if (shard->shard_id_ != memref.data.tid) {
            FATAL_ERROR("Unexpected TID in memref");
        }
        return true;
    }
    analysis_tool_t::interval_state_snapshot_t *
    generate_shard_interval_snapshot(void *shard_data, uint64_t interval_id) override
    {
        per_shard_t *shard = reinterpret_cast<per_shard_t *>(shard_data);
        if (shard->magic_num_ != kMagicNum) {
            FATAL_ERROR("Invalid shard_data");
        }
        if (shard->shard_id_ == kInvalidTid)
            FATAL_ERROR("Expected TID to be found by now");
        auto *snapshot = new recorded_snapshot_t();
        ++outstanding_snapshots_;
        snapshot->component_intervals_.push_back(
            { shard->shard_id_, shard->seen_memrefs_, interval_id });
        return snapshot;
    }
    analysis_tool_t::interval_state_snapshot_t *
    combine_interval_snapshot(analysis_tool_t::interval_state_snapshot_t *one,
                              analysis_tool_t::interval_state_snapshot_t *two) override
    {
        recorded_snapshot_t *result = new recorded_snapshot_t();
        ++outstanding_snapshots_;
        recorded_snapshot_t *snapshot_one = dynamic_cast<recorded_snapshot_t *>(one);
        recorded_snapshot_t *snapshot_two = dynamic_cast<recorded_snapshot_t *>(two);
        result->component_intervals_ = std::move(snapshot_one->component_intervals_);
        result->component_intervals_.insert(result->component_intervals_.end(),
                                            snapshot_two->component_intervals_.begin(),
                                            snapshot_two->component_intervals_.end());
        return result;
    }
    bool
    compare_results(std::vector<recorded_snapshot_t *> &one,
                    std::vector<recorded_snapshot_t> &two)
    {
        if (one.size() != two.size())
            return false;
        for (int i = 0; i < one.size(); ++i) {
            std::sort(one[i]->component_intervals_.begin(),
                      one[i]->component_intervals_.end());
            std::sort(two[i].component_intervals_.begin(),
                      two[i].component_intervals_.end());
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
    get_outstanding_snapshot_count()
    {
        return outstanding_snapshots_;
    }

private:
    int seen_memrefs_;
    std::vector<recorded_snapshot_t> expected_state_snapshots_;
    int outstanding_snapshots_;

    // Data tracked per shard.
    struct per_shard_t {
        memref_tid_t shard_id_;
        uintptr_t magic_num_;
        int seen_memrefs_;
    };

    static constexpr uintptr_t kMagicNum = 0x8badf00d;
    static constexpr memref_tid_t kInvalidTid = -1;
};

static bool
test_non_zero_interval(bool parallel)
{
    constexpr uint64_t kIntervalMicroseconds = 100;
    std::vector<memref_t> refs = {
        // Trace for a single worker: expected intervals shard_id_1 - shard_id_2 - serial
        gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 40),  // 0 - _ - 0
        gen_instr(1, 100),                               // 0 - _ - 0
        gen_data(1, true, 1000, 4),                      // 0 - _ - 0
        gen_marker(2, TRACE_MARKER_TYPE_TIMESTAMP, 151), // 0 - 0 - 1
        gen_instr(2, 200),                               // 0 - 0 - 1
        gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 170), // 1 - 0 - 1
        gen_instr(1, 108),                               // 1 - 0 - 1
        gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 201), // 2 - 0 - 2
        gen_instr(1, 204),                               // 2 - 0 - 2
        gen_marker(2, TRACE_MARKER_TYPE_TIMESTAMP, 210), // 2 - 1 - 2
        gen_instr(2, 208),                               // 2 - 1 - 2
        gen_marker(2, TRACE_MARKER_TYPE_TIMESTAMP, 270), // 2 - 1 - 2
        gen_instr(2, 208),                               // 2 - 1 - 2
        gen_marker(2, TRACE_MARKER_TYPE_TIMESTAMP, 490), // 2 - 3 - 4
        gen_instr(2, 212),                               // 2 - 3 - 4
        gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 590), // 5 - 4 - 5
        gen_exit(1),                                     // 5 - 4 - 5
        gen_marker(2, TRACE_MARKER_TYPE_TIMESTAMP, 610), // _ - 5 - 6
        gen_instr(2, 216),                               // _ - 5 - 6
        gen_exit(2)                                      // _ - 5 - 6
    };
    // Format for interval_switch_point: <tid, seen_memrefs, interval_id>

    std::vector<test_analysis_tool_t::recorded_snapshot_t> serial_state_snapshots = {
        test_analysis_tool_t::recorded_snapshot_t(0, 0, 100, { { 0, 3, 0 } }),
        test_analysis_tool_t::recorded_snapshot_t(0, 1, 200, { { 0, 7, 1 } }),
        test_analysis_tool_t::recorded_snapshot_t(0, 2, 300, { { 0, 13, 2 } }),
        test_analysis_tool_t::recorded_snapshot_t(0, 4, 500, { { 0, 15, 4 } }),
        test_analysis_tool_t::recorded_snapshot_t(0, 5, 600, { { 0, 17, 5 } }),
        test_analysis_tool_t::recorded_snapshot_t(0, 6, 700, { { 0, 20, 6 } }),
    };
    std::vector<test_analysis_tool_t::recorded_snapshot_t> parallel_state_snapshots = {
        test_analysis_tool_t::recorded_snapshot_t(0, 0, 100, { { 1, 3, 0 } }),
        test_analysis_tool_t::recorded_snapshot_t(0, 1, 200,
                                                  { { 1, 5, 1 }, { 2, 2, 0 } }),
        test_analysis_tool_t::recorded_snapshot_t(0, 2, 300,
                                                  { { 1, 7, 2 }, { 2, 6, 1 } }),
        test_analysis_tool_t::recorded_snapshot_t(0, 4, 500, { { 2, 8, 3 } }),
        test_analysis_tool_t::recorded_snapshot_t(0, 5, 600, { { 1, 9, 5 } }),
        test_analysis_tool_t::recorded_snapshot_t(0, 6, 700, { { 2, 11, 5 } })
    };
    auto test_analysis_tool =
        std::unique_ptr<test_analysis_tool_t>(new test_analysis_tool_t(
            parallel ? parallel_state_snapshots : serial_state_snapshots));
    std::vector<analysis_tool_t *> tools;
    tools.push_back(test_analysis_tool.get());
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
    fprintf(stderr, "test_non_zero_interval done for parallel=%d\n", parallel);
    return true;
}

int
main(int argc, const char *argv[])
{
    if (!test_non_zero_interval(false) || !test_non_zero_interval(true))
        return 1;
    fprintf(stderr, "All done!\n");
    return 0;
}
