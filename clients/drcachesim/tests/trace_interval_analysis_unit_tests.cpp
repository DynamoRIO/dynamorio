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
    test_stream_t(const std::vector<memref_t> &refs,  bool parallel)
        : stream_t(nullptr, 0, 0)
        , refs_(refs)
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
        test_stream_ = std::unique_ptr<scheduler_t::stream_t>(
            new test_stream_t(refs, parallel));
        worker_data_.push_back(analyzer_worker_data_t(0, test_stream_.get()));
    }

private:
    std::unique_ptr<scheduler_t::stream_t> test_stream_;
};

// Test analysis_tool_t that stores information about when the interval-end
// events (generate_shard_interval_result and generate_interval_result) were invoked.
class test_analysis_tool_t : public analysis_tool_t {
public:
    test_analysis_tool_t()
        : seen_memrefs_(0)
    {
    }
    bool
    process_memref(const memref_t &memref) override
    {
        ++seen_memrefs_;
        return true;
    }
    bool
    generate_interval_result(uint64_t interval_id) override
    {
        serial_interval_ends_.push_back(std::make_pair(interval_id, seen_memrefs_));
        return true;
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
        return true;
    }
    bool
    parallel_shard_memref(void *shard_data, const memref_t &memref) override
    {
        per_shard_t *shard = reinterpret_cast<per_shard_t *>(shard_data);
        ++shard->seen_memrefs;
        if (shard->tid == kInvalidTid)
            shard->tid = memref.data.tid;
        else if (shard->tid != memref.data.tid)
            FATAL_ERROR("Unexpected TID in memref %d %d", shard->tid, memref.data.tid);
        return true;
    }
    bool
    generate_shard_interval_result(void *shard_data, uint64_t interval_id) override
    {
        per_shard_t *shard = reinterpret_cast<per_shard_t *>(shard_data);
        if (shard->magic_num != kMagicNum) {
            FATAL_ERROR("Invalid shard_data");
        }
        if (shard->tid == kInvalidTid)
            FATAL_ERROR("Expected TID to be found by now");
        parallel_interval_ends_[shard->tid].push_back(
            std::make_pair(interval_id, shard->seen_memrefs));
        return true;
    }

    std::vector<std::pair<uint64_t, int>> serial_interval_ends_;
    std::unordered_map<memref_tid_t, std::vector<std::pair<uint64_t, int>>>
        parallel_interval_ends_;

private:
    int seen_memrefs_;

    struct per_shard_t {
        memref_tid_t tid;
        uintptr_t magic_num;
        int seen_memrefs;
    };

    static constexpr uintptr_t kMagicNum = 0x8badf00d;
    static constexpr memref_tid_t kInvalidTid = -1;
};

static bool
test_non_zero_interval(bool parallel)
{
    constexpr uint64_t kIntervalMicroseconds = 100;
    std::vector<memref_t> refs = {
        gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 1), gen_instr(1, 100),
        gen_data(1, true, 1000, 4),

        gen_marker(2, TRACE_MARKER_TYPE_TIMESTAMP, 51), gen_instr(2, 200),

        gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 70), gen_instr(1, 108),

        // 0th serial interval ends.
        // 0th shard interval ends for tid=1.
        gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 101), gen_instr(1, 204),

        gen_marker(2, TRACE_MARKER_TYPE_TIMESTAMP, 110), gen_instr(2, 208),

        // 1th shard interval ends for tid=2.
        gen_marker(2, TRACE_MARKER_TYPE_TIMESTAMP, 170), gen_instr(2, 208),

        // 1st serial interval ends.
        // 1st shard interval ends for tid=2
        gen_marker(2, TRACE_MARKER_TYPE_TIMESTAMP, 390), gen_instr(2, 208),

        // 3rd serial interval ends.
        // 3rd shard interval ends for tid=2.
        gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 490), gen_exit(1), gen_exit(2)
        // 4th interval ends for serial and all shards.
    };

    auto test_analysis_tool =
        std::unique_ptr<test_analysis_tool_t>(new test_analysis_tool_t());
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
    if (parallel) {
        std::unordered_map<memref_tid_t, std::vector<std::pair<uint64_t, int>>>
            expected_interval_ends;
        expected_interval_ends[1] = { std::make_pair(0, 5), std::make_pair(1, 7),
                                      std::make_pair(4, 9) };
        expected_interval_ends[2] = { std::make_pair(0, 4), std::make_pair(1, 6),
                                      std::make_pair(3, 9) };
        CHECK(
            test_analysis_tool->serial_interval_ends_.empty(),
            "The serial API generate_interval_result should not be invoked for parallel "
            "analysis");
        CHECK(test_analysis_tool->parallel_interval_ends_ == expected_interval_ends,
              "generate_shard_interval_result invoked at unexpected times.");
    } else {
        std::vector<std::pair<uint64_t, int>> expected_interval_ends = {
            // Pair of <interval_id, seen_memrefs_when_interval_ended>.
            std::make_pair(0, 7), std::make_pair(1, 13), std::make_pair(3, 15),
            std::make_pair(4, 18)
        };
        CHECK(test_analysis_tool->parallel_interval_ends_.empty(),
              "The parallel API generate_shard_interval_result should not be invoked for "
              "serial analysis");
        CHECK(test_analysis_tool->serial_interval_ends_ == expected_interval_ends,
              "generate_interval_result invoked at unexpected times.");
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
