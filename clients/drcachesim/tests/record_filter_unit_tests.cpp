/* **********************************************************
 * Copyright (c) 2022-2023 Google, Inc.  All rights reserved.
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

/* Unit tests for the record_filter analyzer. */

#include "analyzer.h"
#include "dr_api.h"
#include "droption.h"
#include "tools/basic_counts.h"
#include "tools/filter/null_filter.h"
#include "tools/filter/cache_filter.h"
#include "tools/filter/record_filter.h"
#include "tools/filter/type_filter.h"

#include <inttypes.h>
#include <fstream>
#include <vector>

namespace dynamorio {
namespace drmemtrace {

using ::dynamorio::droption::droption_parser_t;
using ::dynamorio::droption::DROPTION_SCOPE_ALL;
using ::dynamorio::droption::DROPTION_SCOPE_FRONTEND;
using ::dynamorio::droption::droption_t;

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

static droption_t<std::string>
    op_trace_dir(DROPTION_SCOPE_FRONTEND, "trace_dir", "",
                 "[Required] Trace input directory",
                 "Specifies the directory containing the trace files to be filtered.");

static droption_t<std::string> op_tmp_output_dir(
    DROPTION_SCOPE_FRONTEND, "tmp_output_dir", "",
    "[Required] Output directory for the filtered trace",
    "Specifies the directory where the filtered trace will be written.");

class test_record_filter_t : public dynamorio::drmemtrace::record_filter_t {
public:
    test_record_filter_t(std::vector<std::unique_ptr<record_filter_func_t>> filters,
                         uint64_t last_timestamp)
        : record_filter_t("", std::move(filters), last_timestamp,
                          /*verbose=*/0)
    {
    }
    std::vector<trace_entry_t>
    get_output_entries()
    {
        return output;
    }

protected:
    bool
    write_trace_entry(dynamorio::drmemtrace::record_filter_t::per_shard_t *shard,
                      const trace_entry_t &entry) override
    {
        output.push_back(entry);
        return true;
    }
    std::unique_ptr<std::ostream>
    get_writer(per_shard_t *per_shard, memtrace_stream_t *shard_stream) override
    {
        return std::unique_ptr<std::ostream>(new std::ofstream("/dev/null"));
    }

private:
    std::vector<trace_entry_t> output;
};

class local_stream_t : public default_memtrace_stream_t {
public:
    uint64_t
    get_last_timestamp() const override
    {
        return last_timestamp_;
    }
    void
    set_last_timestamp(uint64_t last_timestamp)
    {
        last_timestamp_ = last_timestamp;
    }

private:
    uint64_t last_timestamp_;
};

static bool
local_create_dir(const char *dir)
{
    if (dr_directory_exists(dir))
        return true;
    return dr_create_dir(dir);
}

basic_counts_t::counters_t
get_basic_counts(const std::string &trace_dir)
{
    auto basic_counts_tool =
        std::unique_ptr<basic_counts_t>(new basic_counts_t(/*verbose=*/0));
    std::vector<analysis_tool_t *> tools;
    tools.push_back(basic_counts_tool.get());
    analyzer_t analyzer(trace_dir, &tools[0], static_cast<int>(tools.size()));
    if (!analyzer) {
        FATAL_ERROR("failed to initialize analyzer: %s",
                    analyzer.get_error_string().c_str());
    }
    if (!analyzer.run()) {
        FATAL_ERROR("failed to run analyzer: %s", analyzer.get_error_string().c_str());
    }
    basic_counts_t::counters_t counts = basic_counts_tool->get_total_counts();
    return counts;
}

static void
print_entry(trace_entry_t entry)
{
    fprintf(stderr, "%s:%d:%" PRIxPTR, trace_type_names[entry.type], entry.size,
            entry.addr);
}

static bool
test_cache_and_type_filter()
{
    struct test_case {
        trace_entry_t entry;
        // Specifies whether the entry should be processed by the record_filter
        // as an input. Some entries are added only to show the expected output
        // and shouldn't be used as input to the record_filter.
        bool input;
        // Specifies whether the entry should be expected in the result of the
        // record filter. This is an array of size equal to the number of test
        // cases.
        bool output[2];
    };

    // We test two configurations:
    // 1. filter data address stream using a cache, and filter function markers
    //    and encoding entries, without any stop timestamp.
    // 2. filter data and instruction address stream using a cache, with a
    //    stop timestamp.
    std::vector<struct test_case> entries = {
        // Trace shard header.
        { { TRACE_TYPE_HEADER, 0, { 0x1 } }, true, { true, true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION, { 0x2 } },
          true,
          { true, true } },
        // File type, also modified by the record_filter based on the filtering
        // done by the filter functions.
        { { TRACE_TYPE_MARKER,
            TRACE_MARKER_TYPE_FILETYPE,
            { OFFLINE_FILE_TYPE_NO_OPTIMIZATIONS | OFFLINE_FILE_TYPE_ENCODINGS } },
          true,
          { false, false } },
        { { TRACE_TYPE_MARKER,
            TRACE_MARKER_TYPE_FILETYPE,
            { OFFLINE_FILE_TYPE_NO_OPTIMIZATIONS | OFFLINE_FILE_TYPE_DFILTERED } },
          false,
          { true, false } },
        { { TRACE_TYPE_MARKER,
            TRACE_MARKER_TYPE_FILETYPE,
            { OFFLINE_FILE_TYPE_NO_OPTIMIZATIONS | OFFLINE_FILE_TYPE_ENCODINGS |
              OFFLINE_FILE_TYPE_DFILTERED | OFFLINE_FILE_TYPE_IFILTERED |
              OFFLINE_FILE_TYPE_BIMODAL_FILTERED_WARMUP } },
          false,
          { false, true } },
        { { TRACE_TYPE_THREAD, 0, { 0x4 } }, true, { true, true } },
        { { TRACE_TYPE_PID, 0, { 0x5 } }, true, { true, true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, { 0x6 } },
          true,
          { true, true } },

        // Unit header.
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 0x7 } },
          true,
          { true, true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0x8 } },
          true,
          { true, true } },
        { { TRACE_TYPE_INSTR, 4, { 0xaa00 } }, true, { true, true } },
        { { TRACE_TYPE_WRITE, 4, { 0xaa80 } }, true, { true, true } },

        // Unit header. For the 1st test, this is skipped, since no entry
        // is output from this unit.
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 0x9 } },
          true,
          { false, true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0xa } },
          true,
          { false, true } },
        // Filtered out by cache_filter.
        { { TRACE_TYPE_WRITE, 4, { 0xaa90 } }, true, { false, false } },
        // For the 1st test: filtered out by type_filter.
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ID, { 0xb } },
          true,
          { false, true } },

        // Unit header.
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 0xc } },
          true,
          { true, true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0xd } },
          true,
          { true, true } },
        // For the 1st test: All function markers are filtered out
        // by type filter.
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ID, { 0xe } },
          true,
          { false, true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ARG, { 0xf } },
          true,
          { false, true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_RETADDR, { 0xa0 } },
          true,
          { false, true } },
        // For the 1st test, these encoding entries are filtered out by
        // the type_filter.
        // For the 2nd test, these encoding entries are delayed since the
        // following instruction at PC 0xaa80 is filtered out by the
        // cache_filter.
        { { TRACE_TYPE_ENCODING, 4, { 0x8bad } }, true, { false, false } },
        { { TRACE_TYPE_ENCODING, 2, { 0xf00d } }, true, { false, false } },
        { { TRACE_TYPE_INSTR, 4, { 0xaa80 } }, true, { true, false } },
        // Filtered out by the cache_filter.
        { { TRACE_TYPE_READ, 4, { 0xaaa0 } }, true, { false, false } },

        // Filter endpoint marker. Only added in the 2nd test where
        // we specify a stop_timestamp.
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILTER_ENDPOINT, { 0 } },
          false,
          { false, true } },

        // Unit header.
        // For the 2nd test: Since this timestamp is greater than the
        // last_timestamp set below, all later entries will be output
        // regardless of the configured filter.
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 0xabcdef } },
          true,
          { true, true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0xa0 } },
          true,
          { true, true } },
        // For the 1st test: Filtered out by type_filter.
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ID, { 0xa1 } },
          true,
          { false, true } },
        // For the 1st test: encoding entries are filtered out by type_filter.
        { { TRACE_TYPE_ENCODING, 4, { 0xdead } }, true, { false, true } },
        { { TRACE_TYPE_ENCODING, 2, { 0xbeef } }, true, { false, true } },
        { { TRACE_TYPE_INSTR, 4, { 0xab80 } }, true, { true, true } },
        // For the 2nd test: Delayed encodings from the previous instance
        // of the instruction at PC 0xaa80 that was filtered out.
        { { TRACE_TYPE_ENCODING, 4, { 0x8bad } }, false, { false, true } },
        { { TRACE_TYPE_ENCODING, 2, { 0xf00d } }, false, { false, true } },
        { { TRACE_TYPE_INSTR, 4, { 0xaa80 } }, true, { true, true } },
        // Trace shard footer.
        { { TRACE_TYPE_FOOTER, 0, { 0xa2 } }, true, { true, true } }
    };

    for (int k = 0; k < 2; ++k) {
        auto stream = std::unique_ptr<local_stream_t>(new local_stream_t());
        // Construct record_filter_func_ts.
        std::vector<
            std::unique_ptr<dynamorio::drmemtrace::record_filter_t::record_filter_func_t>>
            filters;
        auto cache_filter =
            std::unique_ptr<dynamorio::drmemtrace::record_filter_t::record_filter_func_t>(
                new dynamorio::drmemtrace::cache_filter_t(
                    /*cache_associativity=*/1, /*cache_line_size=*/64, /*cache_size=*/128,
                    /*filter_data=*/true, /*filter_instrs=*/k == 1));
        if (cache_filter->get_error_string() != "") {
            fprintf(stderr, "Couldn't construct a cache_filter %s",
                    cache_filter->get_error_string().c_str());
            return false;
        }
        filters.push_back(std::move(cache_filter));

        if (k == 0) {
            auto type_filter = std::unique_ptr<
                dynamorio::drmemtrace::record_filter_t::record_filter_func_t>(
                new dynamorio::drmemtrace::type_filter_t({ TRACE_TYPE_ENCODING },
                                                         { TRACE_MARKER_TYPE_FUNC_ID,
                                                           TRACE_MARKER_TYPE_FUNC_RETADDR,
                                                           TRACE_MARKER_TYPE_FUNC_ARG }));
            if (type_filter->get_error_string() != "") {
                fprintf(stderr, "Couldn't construct a type_filter %s",
                        type_filter->get_error_string().c_str());
                return false;
            }
            filters.push_back(std::move(type_filter));
        }

        // Construct record_filter_t.
        uint64_t stop_timestamp = k == 0 ? 0 : 0xabcdee;
        auto record_filter = std::unique_ptr<test_record_filter_t>(
            new test_record_filter_t(std::move(filters), stop_timestamp));
        void *shard_data =
            record_filter->parallel_shard_init_stream(0, nullptr, stream.get());
        if (!*record_filter) {
            fprintf(stderr, "Filtering init failed\n");
            return false;
        }

        // Process each trace entry.
        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
            // We need to emulate the stream for the tool.
            if (entries[i].entry.type == TRACE_TYPE_MARKER &&
                entries[i].entry.size == TRACE_MARKER_TYPE_TIMESTAMP)
                stream->set_last_timestamp(entries[i].entry.addr);
            if (entries[i].input &&
                !record_filter->parallel_shard_memref(shard_data, entries[i].entry)) {
                fprintf(stderr, "Filtering failed\n");
                return false;
            }
        }
        if (!record_filter->parallel_shard_exit(shard_data) || !*record_filter) {
            fprintf(stderr, "Filtering exit failed\n");
            return false;
        }

        // Check filtered output entries.
        std::vector<trace_entry_t> filtered = record_filter->get_output_entries();
        int j = 0;
        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
            if (!entries[i].output[k])
                continue;
            if (j >= static_cast<int>(filtered.size())) {
                fprintf(stderr,
                        "Too few entries in filtered output (iter=%d). Expected: ", k);
                print_entry(entries[i].entry);
                fprintf(stderr, "\n");
                return false;
            }
            if (memcmp(&filtered[j], &entries[i].entry, sizeof(trace_entry_t)) != 0) {
                fprintf(stderr,
                        "Wrong filter result for iter=%d, at pos=%d. Expected: ", k, i);
                print_entry(entries[i].entry);
                fprintf(stderr, ", got: ");
                print_entry(filtered[j]);
                fprintf(stderr, "\n");
                return false;
            }
            ++j;
        }
        if (j < static_cast<int>(filtered.size())) {
            fprintf(stderr,
                    "Got %d extra entries in filtered output (iter=%d). Next one: ",
                    static_cast<int>(filtered.size()) - j, k);
            print_entry(filtered[j]);
            fprintf(stderr, "\n");
            return false;
        }
    }
    fprintf(stderr, "test_cache_and_type_filter passed\n");
    return true;
}

// Tests I/O for the record_filter.
static bool
test_null_filter()
{
    std::string output_dir = op_tmp_output_dir.get_value() + DIRSEP + "null_filter";
    if (!local_create_dir(output_dir.c_str())) {
        FATAL_ERROR("Failed to create filtered trace output dir %s", output_dir.c_str());
    }
    auto null_filter =
        std::unique_ptr<dynamorio::drmemtrace::record_filter_t::record_filter_func_t>(
            new dynamorio::drmemtrace::null_filter_t());
    std::vector<
        std::unique_ptr<dynamorio::drmemtrace::record_filter_t::record_filter_func_t>>
        filter_funcs;
    filter_funcs.push_back(std::move(null_filter));
    // We use a very small stop_timestamp for the record filter. This is to verify that
    // we emit the TRACE_MARKER_TYPE_FILTER_ENDPOINT marker for each thread even if it
    // starts after the given stop_timestamp. Since the stop_timestamp is so small, all
    // other entries are expected to stay.
    static constexpr uint64_t stop_timestamp_us = 1;
    auto record_filter = std::unique_ptr<dynamorio::drmemtrace::record_filter_t>(
        new dynamorio::drmemtrace::record_filter_t(output_dir, std::move(filter_funcs),
                                                   stop_timestamp_us,
                                                   /*verbosity=*/0));
    std::vector<record_analysis_tool_t *> tools;
    tools.push_back(record_filter.get());
    record_analyzer_t record_analyzer(op_trace_dir.get_value(), &tools[0],
                                      static_cast<int>(tools.size()));
    if (!record_analyzer) {
        FATAL_ERROR("Failed to initialize record filter: %s",
                    record_analyzer.get_error_string().c_str());
    }
    if (!record_analyzer.run()) {
        FATAL_ERROR("Failed to run record filter: %s",
                    record_analyzer.get_error_string().c_str());
    }

    basic_counts_t::counters_t c1 = get_basic_counts(op_trace_dir.get_value());
    // We expect one extra marker (TRACE_MARKER_TYPE_FILTER_ENDPOINT) for each thread.
    c1.other_markers += c1.shard_count;
    basic_counts_t::counters_t c2 = get_basic_counts(output_dir);
    CHECK(c1.instrs != 0, "Bad input trace\n");
    CHECK(c1 == c2, "Null filter returned different counts\n");
    fprintf(stderr, "test_null_filter passed\n");
    return true;
}

int
test_main(int argc, const char *argv[])
{
    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_FRONTEND, argc, (const char **)argv,
                                       &parse_err, NULL) ||
        op_trace_dir.get_value().empty() || op_tmp_output_dir.get_value().empty()) {
        FATAL_ERROR("Usage error: %s\nUsage:\n%s", parse_err.c_str(),
                    droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
    }
    if (!test_cache_and_type_filter() || !test_null_filter())
        return 1;
    // TODO i#5675: Add test using a freshly generated trace (during the test) when
    // zip support is added.
    fprintf(stderr, "All done!\n");
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
