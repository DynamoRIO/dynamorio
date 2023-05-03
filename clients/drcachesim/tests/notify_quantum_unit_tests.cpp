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

/* Unit tests for the quantum notification API in analysis_tool_t. */

#include "analyzer.h"
#include "dr_api.h"
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
    test_stream_t(std::vector<memref_t> refs)
        : stream_t(nullptr, 0, 0)
        , refs_(refs)
        , at_(0)
    {
    }
    scheduler_t::stream_status_t
    next_record(memref_t &record) override
    {
        if (at_ < refs_.size()) {
            record = refs_[at_++];
            if (record.marker.type == TRACE_TYPE_MARKER &&
                record.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP) {
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
        return 0;
    }

private:
    std::vector<memref_t> refs_;
    size_t at_;
};

// Test analyzer_t that uses a test_stream_t instead of a stream provided
// by a scheduler_t.
class test_analyzer_t : public analyzer_t {
public:
    test_analyzer_t(const std::vector<memref_t> &refs, analysis_tool_t **tools,
                    int num_tools, bool parallel, uint64_t quantum_microseconds)
        : analyzer_t()
    {
        num_tools_ = num_tools;
        tools_ = tools;
        parallel_ = parallel;
        quantum_microseconds_ = quantum_microseconds;
        verbosity_ = 2;
        worker_count_ = 1;
        test_stream = std::unique_ptr<scheduler_t::stream_t>(new test_stream_t(refs));
        worker_data_.push_back(analyzer_worker_data_t(0, test_stream.get()));
    }

private:
    std::unique_ptr<scheduler_t::stream_t> test_stream;
};

// Test analysis_tool_t that stores information about when the quantum-end
// events (parallel_shard_quantum_end and notify_quantum_end) were invoked.
class test_analysis_tool_t : public analysis_tool_t {
public:
    test_analysis_tool_t()
        : seen_memrefs_(0)
        , seen_parallel_memrefs_(0)
    {
    }
    bool
    process_memref(const memref_t &memref) override
    {
        ++seen_memrefs_;
        return true;
    }
    bool
    notify_quantum_end(uint64_t quantum_id) override
    {
        serial_quantum_ends.push_back(std::make_pair(quantum_id, seen_memrefs_));
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
        return reinterpret_cast<void *>(0x8badf00d);
    }
    bool
    parallel_shard_exit(void *shard_data) override
    {
        return true;
    }
    bool
    parallel_shard_memref(void *shard_data, const memref_t &memref) override
    {
        ++seen_parallel_memrefs_;
        return true;
    }
    bool
    parallel_shard_quantum_end(void *shard_data, uint64_t quantum_id) override
    {
        if (shard_data != reinterpret_cast<void *>(0x8badf00d)) {
            fprintf(stderr, "Invalid shard_data\n");
            return false;
        }
        parallel_quantum_ends.push_back(
            std::make_pair(quantum_id, seen_parallel_memrefs_));
        return true;
    }

    std::vector<std::pair<uint64_t, int>> serial_quantum_ends;
    std::vector<std::pair<uint64_t, int>> parallel_quantum_ends;

private:
    int seen_memrefs_;
    int seen_parallel_memrefs_;
};

static bool
test_non_zero_quantum(bool parallel)
{
    constexpr int kQuantumMicroseconds = 100;
    std::vector<memref_t> refs = {
        gen_instr(1, 1), gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 1), gen_instr(1, 2),
        gen_data(1, true, 100, 4), gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 50),
        gen_instr(1, 3),
        // 0th quantum ends here.
        gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 101), gen_instr(1, 4),
        // 1st quantum ends here.
        gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 500),
        // 4th quantum ends here.
        gen_exit(1)
    };

    auto test_analysis_tool =
        std::unique_ptr<test_analysis_tool_t>(new test_analysis_tool_t());
    std::vector<analysis_tool_t *> tools;
    tools.push_back(test_analysis_tool.get());
    test_analyzer_t test_analyzer(refs, &tools[0], tools.size(), parallel,
                                  kQuantumMicroseconds);

    if (!test_analyzer) {
        FATAL_ERROR("failed to initialize test analyzer: %s",
                    test_analyzer.get_error_string().c_str());
    }
    if (!test_analyzer.run()) {
        FATAL_ERROR("failed to run test_analyzer: %s",
                    test_analyzer.get_error_string().c_str());
    }
    std::vector<std::pair<uint64_t, int>> expected_quantum_ends = {
        std::make_pair(0, 6), std::make_pair(1, 8), std::make_pair(4, 10)
    };
    if (parallel) {
        CHECK(test_analysis_tool->serial_quantum_ends.size() == 0,
              "The serial API notify_quantum_end should not be invoked for parallel "
              "analysis");
        CHECK(test_analysis_tool->parallel_quantum_ends == expected_quantum_ends,
              "parallel_shard_quantum_end invoked at unexpected times.");
    } else {
        CHECK(test_analysis_tool->parallel_quantum_ends.size() == 0,
              "The parallel API parallel_shard_quantum_end should not be invoked for "
              "serial analysis");
        CHECK(test_analysis_tool->serial_quantum_ends == expected_quantum_ends,
              "notify_quantum_end invoked at unexpected times.");
    }
    fprintf(stderr, "test_non_zero_quantum done");
    return true;
}

int
main(int argc, const char *argv[])
{
    if (!test_non_zero_quantum(false) || !test_non_zero_quantum(false))
        return 1;
    fprintf(stderr, "All done!\n");
    return 0;
}
