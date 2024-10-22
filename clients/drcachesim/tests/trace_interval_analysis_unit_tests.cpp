/* **********************************************************
 * Copyright (c) 2023-2024 Google, Inc.  All rights reserved.
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
            if (tid2ordinal_.find(record.instr.tid) == tid2ordinal_.end()) {
                tid2ordinal_[record.instr.tid] = tid2ordinal_.size();
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
    scheduler_t::stream_status_t
    next_record(memref_t &record, uint64_t cur_time) override
    {
        return next_record(record);
    }
    std::string
    get_stream_name() const override
    {
        return "test_stream";
    }
    scheduler_t::input_ordinal_t
    get_input_stream_ordinal() const override
    {
        assert(at_ >= 0 && at_ < refs_.size());
        // Each TID forms a separate input stream.
        return tid2ordinal_.at(refs_[at_].instr.tid);
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
        if (it != instr_counts_.end()) {
            return it->second;
        }
        return 0;
    }
    int
    get_shard_index() const override
    {
        return get_input_stream_ordinal();
    }

private:
    std::unordered_map<memref_tid_t, scheduler_t::input_ordinal_t> tid2ordinal_;
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
                    int num_tools, bool parallel, uint64_t interval_microseconds,
                    uint64_t interval_instr_count)
        : analyzer_t()
    {
        num_tools_ = num_tools;
        tools_ = tools;
        parallel_ = parallel;
        interval_microseconds_ = interval_microseconds;
        interval_instr_count_ = interval_instr_count;
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
        saw_serial_generate_snapshot_ = true;
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
        // We generate a snapshot here, but we clear them all in
        // finalize_interval_snapshots to test that scenario.
        auto *snapshot = new interval_state_snapshot_t();
        return snapshot;
    }
    bool
    finalize_interval_snapshots(
        std::vector<interval_state_snapshot_t *> &interval_snapshots) override
    {
        if (saw_serial_generate_snapshot_) {
            error_string_ = "Did not expect finalize_interval_snapshots call in serial "
                            "mode which does not generate any snapshot.";
            return false;
        }
        for (auto snapshot : interval_snapshots) {
            delete snapshot;
        }
        // We clear the snapshots here so that there will be no
        // combine_interval_snapshots or print_interval_results calls.
        interval_snapshots.clear();
        return true;
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
    bool saw_serial_generate_snapshot_ = false;
};

#define SERIAL_TID 0

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
    recorded_snapshot_t(uint64_t shard_id, uint64_t interval_id,
                        uint64_t interval_end_timestamp, uint64_t instr_count_cumulative,
                        uint64_t instr_count_delta,
                        std::vector<interval_end_point_t> component_intervals)
        // Actual tools do not need to supply values to construct the base
        // interval_state_snapshot_t. This is only to make it easier to construct
        // the expected snapshot objects in this test.
        : interval_state_snapshot_t(shard_id, interval_id, interval_end_timestamp,
                                    instr_count_cumulative, instr_count_delta)
        , component_intervals(component_intervals)
        , tool_shard_id(shard_id)
    {
    }
    recorded_snapshot_t(uint64_t interval_id, uint64_t interval_end_timestamp,
                        uint64_t instr_count_cumulative, uint64_t instr_count_delta,
                        std::vector<interval_end_point_t> component_intervals)
        : recorded_snapshot_t(WHOLE_TRACE_SHARD_ID, interval_id, interval_end_timestamp,
                              instr_count_cumulative, instr_count_delta,
                              component_intervals)
    {
    }
    recorded_snapshot_t()
    {
    }

    bool
    operator==(const recorded_snapshot_t &rhs) const
    {
        return get_shard_id() == rhs.get_shard_id() &&
            tool_shard_id == rhs.tool_shard_id &&
            get_interval_id() == rhs.get_interval_id() &&
            get_interval_end_timestamp() == rhs.get_interval_end_timestamp() &&
            get_instr_count_cumulative() == rhs.get_instr_count_cumulative() &&
            get_instr_count_delta() == rhs.get_instr_count_delta() &&
            component_intervals == rhs.component_intervals;
    }
    void
    print() const
    {
        std::cerr << "(shard_id: " << get_shard_id()
                  << ", interval_id: " << get_interval_id()
                  << ", tool_shard_id: " << tool_shard_id
                  << ", end_timestamp: " << get_interval_end_timestamp()
                  << ", instr_count_cumulative: " << get_instr_count_cumulative()
                  << ", instr_count_delta: " << get_instr_count_delta()
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
    // Stores whether this snapshot was seen by finalize_interval_snapshots.
    bool saw_finalize_call = false;
};

// Test analysis_tool_t that records information about when the
// generate_shard_interval_snapshot and generate_interval_snapshot APIs were invoked.
class test_analysis_tool_t : public analysis_tool_t {
public:
    // Constructs an analysis_tool_t that expects the given interval state snapshots to be
    // produced.
    test_analysis_tool_t(
        const std::vector<std::vector<recorded_snapshot_t>> &expected_state_snapshots,
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
            { SERIAL_TID, seen_memrefs_, interval_id });
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
        parallel_mode_ = true;
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
    bool
    finalize_interval_snapshots(
        std::vector<interval_state_snapshot_t *> &interval_snapshots)
    {
        for (auto snapshot : interval_snapshots) {
            if (snapshot == nullptr) {
                error_string_ =
                    "Did not expect a nullptr snapshot in finalize_interval_snapshots";
                return false;
            }
            auto recorded_snapshot = dynamic_cast<recorded_snapshot_t *>(snapshot);
            if (recorded_snapshot->saw_finalize_call) {
                error_string_ = "interval_state_snapshot_t presented "
                                "to finalize_interval_snapshots multiple times";
                return false;
            }
            recorded_snapshot->saw_finalize_call = true;
        }
        return true;
    }
    analysis_tool_t::interval_state_snapshot_t *
    combine_interval_snapshots(
        const std::vector<const analysis_tool_t::interval_state_snapshot_t *>
            latest_shard_snapshots,
        uint64_t interval_end_timestamp) override
    {
        // If we expect multiple std::vector of interval snapshots (one for each shard),
        // it means we're not merging the snapshots across shards, so there should not
        // be any combine_interval_snapshot calls.
        if (expected_state_snapshots_.size() != 1) {
            error_string_ = "Did not expect any combine_interval_snapshots() calls";
            return nullptr;
        }
        if (!parallel_mode_) {
            error_string_ =
                "Did not expect any combine_interval_snapshots() calls in serial mode.";
            return nullptr;
        }
        recorded_snapshot_t *result = new recorded_snapshot_t();
        result->tool_shard_id =
            analysis_tool_t::interval_state_snapshot_t::WHOLE_TRACE_SHARD_ID;
        ++outstanding_snapshots_;
        for (auto snapshot : latest_shard_snapshots) {
            if (snapshot != nullptr &&
                (!combine_only_active_shards_ ||
                 snapshot->get_interval_end_timestamp() == interval_end_timestamp)) {
                auto recorded_snapshot =
                    dynamic_cast<const recorded_snapshot_t *const>(snapshot);
                if (recorded_snapshot->tool_shard_id !=
                    recorded_snapshot->get_shard_id()) {
                    FATAL_ERROR("shard_id stored by tool (%" PRIi64
                                ") and framework (%" PRIi64 ") mismatch",
                                recorded_snapshot->tool_shard_id,
                                recorded_snapshot->get_shard_id());
                    return nullptr;
                }
                if (!recorded_snapshot->saw_finalize_call) {
                    error_string_ =
                        "combine_interval_snapshots saw non-finalized snapshot";
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
        if (seen_print_interval_results_calls_ >= expected_state_snapshots_.size()) {
            error_string_ = "Saw more print_interval_results() calls than expected";
            return false;
        }
        std::vector<recorded_snapshot_t *> recorded_state_snapshots;
        for (const auto &p : snapshots) {
            recorded_state_snapshots.push_back(
                reinterpret_cast<recorded_snapshot_t *>(p));
        }
        if (!compare_results(
                recorded_state_snapshots,
                expected_state_snapshots_[seen_print_interval_results_calls_])) {
            error_string_ = "Unexpected state snapshots";
            std::cerr << "Expected:\n";
            for (const auto &snapshot :
                 expected_state_snapshots_[seen_print_interval_results_calls_])
                snapshot.print();
            std::cerr << "Found:\n";
            for (const auto &snapshot : recorded_state_snapshots)
                snapshot->print();
            return false;
        }
        ++seen_print_interval_results_calls_;
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
    int
    get_outstanding_print_interval_results_calls() const
    {
        return expected_state_snapshots_.size() - seen_print_interval_results_calls_;
    }

private:
    int seen_memrefs_;
    // We expect to see one print_interval_results call per shard (we do not merge
    // the shard interval snapshots for instr count intervals), or exactly one
    // print_interval_results call for the whole-trace (we merge shard interval
    // snapshots for timestamp intervals).
    std::vector<std::vector<recorded_snapshot_t>> expected_state_snapshots_;
    int outstanding_snapshots_;
    bool combine_only_active_shards_;
    int seen_print_interval_results_calls_ = 0;
    bool parallel_mode_ = false;

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
    constexpr uint64_t kNoIntervalInstrCount = 0;
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

    std::vector<std::vector<recorded_snapshot_t>> expected_state_snapshots;
    if (!parallel) {
        // Each whole trace interval is made up of only one snapshot, the
        // serial snapshot.
        expected_state_snapshots = { {
            // Format:
            // <interval_id, interval_end_timestamp, instr_count_cumulative,
            //  instr_count_delta, <tid, seen_memrefs, interval_id>>
            recorded_snapshot_t(1, 100, 1, 1, { { SERIAL_TID, 3, 1 } }),
            recorded_snapshot_t(2, 200, 3, 2, { { SERIAL_TID, 7, 2 } }),
            recorded_snapshot_t(3, 300, 6, 3, { { SERIAL_TID, 13, 3 } }),
            recorded_snapshot_t(5, 500, 7, 1, { { SERIAL_TID, 15, 5 } }),
            recorded_snapshot_t(6, 600, 7, 0, { { SERIAL_TID, 17, 6 } }),
            recorded_snapshot_t(7, 700, 8, 1, { { SERIAL_TID, 20, 7 } }),
        } };
    } else if (combine_only_active_shards) {
        // Each whole trace interval is made up of snapshots from each
        // shard that was active in that interval.
        expected_state_snapshots = {
            { // Format:
              // <interval_id, interval_end_timestamp, instr_count_cumulative,
              //  instr_count_delta, <tid, seen_memrefs, interval_id>>
              recorded_snapshot_t(1, 100, 1, 1, { { 51, 3, 1 } }),
              // Narration: The whole-trace interval_id=2 with interval_end_timestamp=200
              // is made up of the following two shard-local interval snapshots:
              // - from shard_id=51, the interval_id=2 that ends at the local_memref=5
              // - from shard_id=52, the interval_id=1 that ends at the local_memref=2
              recorded_snapshot_t(2, 200, 3, 2, { { 51, 5, 2 }, { 52, 2, 1 } }),
              recorded_snapshot_t(3, 300, 6, 3, { { 51, 7, 3 }, { 52, 6, 2 } }),
              recorded_snapshot_t(5, 500, 7, 1, { { 52, 8, 4 } }),
              recorded_snapshot_t(6, 600, 7, 0, { { 51, 9, 6 } }),
              recorded_snapshot_t(7, 700, 8, 1, { { 52, 11, 6 } }) }
        };
    } else {
        // Each whole trace interval is made up of last snapshots from all trace shards.
        expected_state_snapshots = {
            { { // Format:
                // <interval_id, interval_end_timestamp, instr_count_cumulative,
                //  instr_count_delta, <tid, seen_memrefs, interval_id>>
                recorded_snapshot_t(1, 100, 1, 1, { { 51, 3, 1 } }),
                recorded_snapshot_t(2, 200, 3, 2, { { 51, 5, 2 }, { 52, 2, 1 } }),
                recorded_snapshot_t(3, 300, 6, 3, { { 51, 7, 3 }, { 52, 6, 2 } }),
                recorded_snapshot_t(5, 500, 7, 1, { { 51, 7, 3 }, { 52, 8, 4 } }),
                recorded_snapshot_t(6, 600, 7, 0, { { 51, 9, 6 }, { 52, 8, 4 } }),
                recorded_snapshot_t(7, 700, 8, 1, { { 51, 9, 6 }, { 52, 11, 6 } }) } }
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
                                  kIntervalMicroseconds, kNoIntervalInstrCount);
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
    if (test_analysis_tool.get()->get_outstanding_print_interval_results_calls() != 0) {
        std::cerr
            << "Missing "
            << test_analysis_tool.get()->get_outstanding_print_interval_results_calls()
            << " print_interval_result() calls\n";
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

static bool
test_non_zero_interval_i6793_workaround(bool parallel,
                                        bool combine_only_active_shards = true)
{
    constexpr uint64_t kIntervalMicroseconds = 100;
    constexpr uint64_t kNoIntervalInstrCount = 0;
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
        // Missing thread exit for tid=51. Would cause the last interval
        // of this thread to not be processed and included in results.
        gen_marker(52, TRACE_MARKER_TYPE_TIMESTAMP, 610), // _ | 6 | 7
        gen_instr(52, 20016),                             // _ | 6 | 7
        // Missing thread exit for tid=52. Would not matter that it's missing
        // because the stream ends with tid=52 therefore can still provide
        // the state required for generating the interval snapshot here.
    };

    std::vector<std::vector<recorded_snapshot_t>> expected_state_snapshots;
    if (!parallel) {
        // Each whole trace interval is made up of only one snapshot, the
        // serial snapshot.
        // The missing exit for tid=51 does not affect the serial intervals.
        expected_state_snapshots = { {
            // Format:
            // <interval_id, interval_end_timestamp, instr_count_cumulative,
            //  instr_count_delta, <tid, seen_memrefs, interval_id>>
            recorded_snapshot_t(1, 100, 1, 1, { { SERIAL_TID, 3, 1 } }),
            recorded_snapshot_t(2, 200, 3, 2, { { SERIAL_TID, 7, 2 } }),
            recorded_snapshot_t(3, 300, 6, 3, { { SERIAL_TID, 13, 3 } }),
            recorded_snapshot_t(5, 500, 7, 1, { { SERIAL_TID, 15, 5 } }),
            recorded_snapshot_t(6, 600, 7, 0, { { SERIAL_TID, 16, 6 } }),
            recorded_snapshot_t(7, 700, 8, 1, { { SERIAL_TID, 18, 7 } }),
        } };
    } else if (combine_only_active_shards) {
        // Each whole trace interval is made up of snapshots from each
        // shard that was active in that interval.
        expected_state_snapshots = {
            { // Format:
              // <interval_id, interval_end_timestamp, instr_count_cumulative,
              //  instr_count_delta, <tid, seen_memrefs, interval_id>>
              recorded_snapshot_t(1, 100, 1, 1, { { 51, 3, 1 } }),
              // Narration: The whole-trace interval_id=2 with interval_end_timestamp=200
              // is made up of the following two shard-local interval snapshots:
              // - from shard_id=51, the interval_id=2 that ends at the local_memref=5
              // - from shard_id=52, the interval_id=1 that ends at the local_memref=2
              recorded_snapshot_t(2, 200, 3, 2, { { 51, 5, 2 }, { 52, 2, 1 } }),
              recorded_snapshot_t(3, 300, 6, 3, { { 51, 7, 3 }, { 52, 6, 2 } }),
              recorded_snapshot_t(5, 500, 7, 1, { { 52, 8, 4 } }),
              // No interval-6 including tid=51 because of its missing thread exit.
              // In such cases, instead of generating a likely faulty interval with
              // wrong interval_end_timestamp, instr_count_cumulative, and
              // instr_count_delta, we simply skip the final interval for that thread.
              recorded_snapshot_t(7, 700, 8, 1, { { 52, 10, 6 } }) }
        };
    } else {
        // Each whole trace interval is made up of last snapshots from all trace shards.
        expected_state_snapshots = {
            { { // Format:
                // <interval_id, interval_end_timestamp, instr_count_cumulative,
                //  instr_count_delta, <tid, seen_memrefs, interval_id>>
                recorded_snapshot_t(1, 100, 1, 1, { { 51, 3, 1 } }),
                recorded_snapshot_t(2, 200, 3, 2, { { 51, 5, 2 }, { 52, 2, 1 } }),
                recorded_snapshot_t(3, 300, 6, 3, { { 51, 7, 3 }, { 52, 6, 2 } }),
                recorded_snapshot_t(5, 500, 7, 1, { { 51, 7, 3 }, { 52, 8, 4 } }),
                // No interval-6 including tid=51 because of its missing thread exit.
                // So the interval merge logic did not observe any activity during the
                // interval-6.
                // The following whole-trace interval-7 constitutes of the interval-3
                // from tid=51, because the interval-6 from tid=51 was dropped because
                // of the missing thread exit.
                recorded_snapshot_t(7, 700, 8, 1, { { 51, 7, 3 }, { 52, 10, 6 } }) } }
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
                                  kIntervalMicroseconds, kNoIntervalInstrCount);
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
    if (test_analysis_tool.get()->get_outstanding_print_interval_results_calls() != 0) {
        std::cerr
            << "Missing "
            << test_analysis_tool.get()->get_outstanding_print_interval_results_calls()
            << " print_interval_result() calls\n";
        return false;
    }
    // One fewer generate snapshot call in parallel mode because of the missing thread
    // exit for tid=51.
    int expected_generate_call_count = parallel ? 7 : 6;
    if (dummy_analysis_tool.get()->get_generate_snapshot_count() !=
        expected_generate_call_count) {
        std::cerr << "Dummy analysis tool got "
                  << dummy_analysis_tool.get()->get_generate_snapshot_count()
                  << " interval API calls, but expected " << expected_generate_call_count
                  << "\n";
        return false;
    }
    fprintf(stderr,
            "test_non_zero_interval_i6793_workaround done for parallel=%d, "
            "combine_only_active_shards=%d\n",
            parallel, combine_only_active_shards);
    return true;
}

static bool
test_non_zero_instr_interval(bool parallel)
{
    constexpr uint64_t kNoIntervalMicroseconds = 0;
    constexpr uint64_t kIntervalInstrCount = 2;
    std::vector<memref_t> refs = {
        // Trace for a single worker which has two constituent shards. (scheduler_t
        // does not guarantee that workers will process shards one after the other.)
        // Expected active interval_id: tid_51_local | tid_52_local | whole_trace
        gen_marker(51, TRACE_MARKER_TYPE_TIMESTAMP, 40),  // 1 | _ | 1
        gen_instr(51, 10000),                             // 1 | _ | 1
        gen_data(51, true, 1234, 4),                      // 1 | _ | 1
        gen_marker(52, TRACE_MARKER_TYPE_TIMESTAMP, 151), // _ | 1 | 1
        gen_instr(52, 20000),                             // _ | 1 | 1
        gen_marker(51, TRACE_MARKER_TYPE_TIMESTAMP, 170), // 1 | _ | 1
        gen_instr(51, 10008),                             // 1 | _ | 2
        gen_marker(51, TRACE_MARKER_TYPE_TIMESTAMP, 201), // 1 | _ | 2
        gen_instr(51, 20004),                             // 2 | _ | 2
        gen_marker(52, TRACE_MARKER_TYPE_TIMESTAMP, 210), // _ | 1 | 2
        gen_instr(52, 20008),                             // _ | 1 | 3
        gen_marker(52, TRACE_MARKER_TYPE_TIMESTAMP, 270), // _ | 1 | 3
        gen_instr(52, 20008),                             // _ | 2 | 3
        gen_marker(52, TRACE_MARKER_TYPE_TIMESTAMP, 490), // _ | 2 | 3
        gen_instr(52, 20012),                             // _ | 2 | 4
        gen_marker(51, TRACE_MARKER_TYPE_TIMESTAMP, 590), // 2 | _ | 4
        gen_exit(51),                                     // 2 | _ | 4
        gen_marker(52, TRACE_MARKER_TYPE_TIMESTAMP, 610), // _ | 2 | 4
        gen_instr(52, 20016),                             // _ | 3 | 4
        gen_exit(52)                                      // _ | 3 | 4
    };

    std::vector<std::vector<recorded_snapshot_t>> expected_state_snapshots;
    if (!parallel) {
        // Each whole trace interval is made up of only one snapshot, the
        // serial snapshot.
        expected_state_snapshots = {
            { // Format:
              // <interval_id, interval_end_timestamp, instr_count_cumulative,
              //  instr_count_delta, <tid, seen_memrefs, interval_id>>
              recorded_snapshot_t(1, 170, 2, 2, { { SERIAL_TID, 6, 1 } }),
              recorded_snapshot_t(2, 210, 4, 2, { { SERIAL_TID, 10, 2 } }),
              recorded_snapshot_t(3, 490, 6, 2, { { SERIAL_TID, 14, 3 } }),
              recorded_snapshot_t(4, 610, 8, 2, { { SERIAL_TID, 20, 4 } }) }
        };
    } else {
        // For instr count intervals, we do not merge the shard intervals to form the
        // whole-trace intervals. Instead, there are multiple print_interval_result
        // calls, one for the interval snapshots of each shard. The shard_id is
        // included in the provided interval snapshots (see below).
        expected_state_snapshots = {
            // Format:
            // <shard_id, interval_id, interval_end_timestamp, instr_count_cumulative,
            //  instr_count_delta, <tid, seen_memrefs, interval_id>>
            { recorded_snapshot_t(51, 1, 201, 2, 2, { { 51, 6, 1 } }),
              recorded_snapshot_t(51, 2, 590, 3, 1, { { 51, 9, 2 } }) },
            { recorded_snapshot_t(52, 1, 270, 2, 2, { { 52, 5, 1 } }),
              recorded_snapshot_t(52, 2, 610, 4, 2, { { 52, 9, 2 } }),
              recorded_snapshot_t(52, 3, 610, 5, 1, { { 52, 11, 3 } }) },
        };
    }
    std::vector<analysis_tool_t *> tools;
    constexpr bool kNopCombineOnlyActiveShards = false;
    auto test_analysis_tool = std::unique_ptr<test_analysis_tool_t>(
        new test_analysis_tool_t(expected_state_snapshots, kNopCombineOnlyActiveShards));
    tools.push_back(test_analysis_tool.get());
    auto dummy_analysis_tool =
        std::unique_ptr<dummy_analysis_tool_t>(new dummy_analysis_tool_t());
    tools.push_back(dummy_analysis_tool.get());
    test_analyzer_t test_analyzer(refs, &tools[0], (int)tools.size(), parallel,
                                  kNoIntervalMicroseconds, kIntervalInstrCount);
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
    if (test_analysis_tool.get()->get_outstanding_print_interval_results_calls() != 0) {
        std::cerr
            << "Missing "
            << test_analysis_tool.get()->get_outstanding_print_interval_results_calls()
            << " print_interval_result() calls\n";
        return false;
    }
    int expected_generate_call_count = parallel ? 5 : 4;
    if (dummy_analysis_tool.get()->get_generate_snapshot_count() !=
        expected_generate_call_count) {
        std::cerr << "Dummy analysis tool got "
                  << dummy_analysis_tool.get()->get_generate_snapshot_count()
                  << " interval API calls, but expected " << expected_generate_call_count
                  << "\n";
        return false;
    }
    fprintf(stderr, "test_non_zero_instr_interval done for parallel=%d\n", parallel);
    return true;
}

static bool
test_non_zero_instr_interval_i6793_workaround(bool parallel)
{
    constexpr uint64_t kNoIntervalMicroseconds = 0;
    constexpr uint64_t kIntervalInstrCount = 2;
    std::vector<memref_t> refs = {
        // Trace for a single worker which has two constituent shards. (scheduler_t
        // does not guarantee that workers will process shards one after the other.)
        // Expected active interval_id: tid_51_local | tid_52_local | whole_trace
        gen_marker(51, TRACE_MARKER_TYPE_TIMESTAMP, 40),  // 1 | _ | 1
        gen_instr(51, 10000),                             // 1 | _ | 1
        gen_data(51, true, 1234, 4),                      // 1 | _ | 1
        gen_marker(52, TRACE_MARKER_TYPE_TIMESTAMP, 151), // _ | 1 | 1
        gen_instr(52, 20000),                             // _ | 1 | 1
        gen_marker(51, TRACE_MARKER_TYPE_TIMESTAMP, 170), // 1 | _ | 1
        gen_instr(51, 10008),                             // 1 | _ | 2
        gen_marker(51, TRACE_MARKER_TYPE_TIMESTAMP, 201), // 1 | _ | 2
        gen_instr(51, 20004),                             // 2 | _ | 2
        gen_marker(52, TRACE_MARKER_TYPE_TIMESTAMP, 210), // _ | 1 | 2
        gen_instr(52, 20008),                             // _ | 1 | 3
        gen_marker(52, TRACE_MARKER_TYPE_TIMESTAMP, 270), // _ | 1 | 3
        gen_instr(52, 20008),                             // _ | 2 | 3
        gen_marker(52, TRACE_MARKER_TYPE_TIMESTAMP, 490), // _ | 2 | 3
        gen_instr(52, 20012),                             // _ | 2 | 4
        gen_marker(51, TRACE_MARKER_TYPE_TIMESTAMP, 590), // 2 | _ | 4
        // Missing thread exit for tid=51. Would cause the last interval
        // of this thread to not be processed and included in results.
        gen_marker(52, TRACE_MARKER_TYPE_TIMESTAMP, 610), // _ | 2 | 4
        gen_instr(52, 20016),                             // _ | 3 | 4
        // Missing thread exit for tid=52. Would not matter that it's missing
        // because the stream ends with tid=52 therefore can still provide
        // the state required for generating the interval snapshot here.
    };

    std::vector<std::vector<recorded_snapshot_t>> expected_state_snapshots;
    if (!parallel) {
        // Each whole trace interval is made up of only one snapshot, the
        // serial snapshot.
        // The missing exit for tid=51 does not affect the serial intervals.
        expected_state_snapshots = {
            { // Format:
              // <interval_id, interval_end_timestamp, instr_count_cumulative,
              //  instr_count_delta, <tid, seen_memrefs, interval_id>>
              recorded_snapshot_t(1, 170, 2, 2, { { SERIAL_TID, 6, 1 } }),
              recorded_snapshot_t(2, 210, 4, 2, { { SERIAL_TID, 10, 2 } }),
              recorded_snapshot_t(3, 490, 6, 2, { { SERIAL_TID, 14, 3 } }),
              recorded_snapshot_t(4, 610, 8, 2, { { SERIAL_TID, 18, 4 } }) }
        };
    } else {
        // For instr count intervals, we do not merge the shard intervals to form the
        // whole-trace intervals. Instead, there are multiple print_interval_result
        // calls, one for the interval snapshots of each shard. The shard_id is
        // included in the provided interval snapshots (see below).
        expected_state_snapshots = {
            // Format:
            // <shard_id, interval_id, interval_end_timestamp, instr_count_cumulative,
            //  instr_count_delta, <tid, seen_memrefs, interval_id>>
            {
                recorded_snapshot_t(51, 1, 201, 2, 2, { { 51, 6, 1 } }),
                // We do not see any recorded snapshot for the second interval on tid=51
                // because tid=51 is missing a thread exit (a bug that affects some traces
                // prior to the i#6444 fix). In such cases, instead of generating a
                // likely faulty interval with wrong interval_end_timestamp,
                // instr_count_cumulative, and instr_count_delta, we simply skip the
                // final interval for that thread.
            },
            { recorded_snapshot_t(52, 1, 270, 2, 2, { { 52, 5, 1 } }),
              recorded_snapshot_t(52, 2, 610, 4, 2, { { 52, 9, 2 } }),
              // Even though a thread exit record is missing for tid=52, it still
              // generates a final interval, because tid=52 is the last thread in the
              // stream.
              recorded_snapshot_t(52, 3, 610, 5, 1, { { 52, 10, 3 } }) },
        };
    }
    std::vector<analysis_tool_t *> tools;
    constexpr bool kNopCombineOnlyActiveShards = false;
    auto test_analysis_tool = std::unique_ptr<test_analysis_tool_t>(
        new test_analysis_tool_t(expected_state_snapshots, kNopCombineOnlyActiveShards));
    tools.push_back(test_analysis_tool.get());
    auto dummy_analysis_tool =
        std::unique_ptr<dummy_analysis_tool_t>(new dummy_analysis_tool_t());
    tools.push_back(dummy_analysis_tool.get());
    test_analyzer_t test_analyzer(refs, &tools[0], (int)tools.size(), parallel,
                                  kNoIntervalMicroseconds, kIntervalInstrCount);
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
    if (test_analysis_tool.get()->get_outstanding_print_interval_results_calls() != 0) {
        std::cerr
            << "Missing "
            << test_analysis_tool.get()->get_outstanding_print_interval_results_calls()
            << " print_interval_result() calls\n";
        return false;
    }
    // One fewer generate call because of the missing thread exit on tid=51.
    int expected_generate_call_count = parallel ? 4 : 4;
    if (dummy_analysis_tool.get()->get_generate_snapshot_count() !=
        expected_generate_call_count) {
        std::cerr << "Dummy analysis tool got "
                  << dummy_analysis_tool.get()->get_generate_snapshot_count()
                  << " interval API calls, but expected " << expected_generate_call_count
                  << "\n";
        return false;
    }
    fprintf(stderr,
            "test_non_zero_instr_interval_i6793_workaround done for parallel=%d\n",
            parallel);
    return true;
}

int
test_main(int argc, const char *argv[])
{
    if (!test_non_zero_interval(false) || !test_non_zero_interval(true, true) ||
        !test_non_zero_interval(true, false) || !test_non_zero_instr_interval(false) ||
        !test_non_zero_instr_interval(true) ||
        !test_non_zero_interval_i6793_workaround(false) ||
        !test_non_zero_interval_i6793_workaround(true, true) ||
        !test_non_zero_interval_i6793_workaround(true, false) ||
        !test_non_zero_instr_interval_i6793_workaround(false) ||
        !test_non_zero_instr_interval_i6793_workaround(true))
        return 1;
    fprintf(stderr, "All done!\n");
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
