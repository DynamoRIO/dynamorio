/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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
#include "tools/filter/toggle_filter.h"
#include "tools/filter/record_filter.h"

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
    test_record_filter_t(const std::vector<record_filter_func_t *> &filters)
        : record_filter_t("", filters,
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
        return nullptr;
    }

private:
    std::vector<trace_entry_t> output;
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
    analysis_tool_t *basic_counts_tool = new basic_counts_t(/*verbose=*/0);
    std::vector<analysis_tool_t *> tools;
    tools.push_back(basic_counts_tool);
    analyzer_t analyzer(trace_dir, &tools[0], (int)tools.size());
    if (!analyzer) {
        FATAL_ERROR("failed to initialize analyzer: %s",
                    analyzer.get_error_string().c_str());
    }
    if (!analyzer.run()) {
        FATAL_ERROR("failed to run analyzer: %s", analyzer.get_error_string().c_str());
    }
    basic_counts_t::counters_t counts =
        ((basic_counts_t *)basic_counts_tool)->get_total_counts();
    delete basic_counts_tool;
    return counts;
}

static void
print_entry(trace_entry_t entry)
{
    fprintf(stderr, "%d:%d:%ld", entry.type, entry.size, entry.addr);
}

static bool
test_toggle_filter()
{
    struct expected_output {
        trace_entry_t entry;
        bool in_half[2];
    };

#define SPLIT_AT_INSTR_COUNT 5
    std::vector<struct expected_output> entries = {
        /* Trace shard header. */
        { { TRACE_TYPE_HEADER, 0, 1 }, { true, true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION, 2 }, { true, true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE, 3 }, { true, true } },
        { { TRACE_TYPE_THREAD, 0, 4 }, { true, true } },
        { { TRACE_TYPE_PID, 0, 5 }, { true, true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 6 }, { true, true } },
        /* Unit header. */
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, 7 }, { true, false } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, 8 }, { true, false } },
        { { TRACE_TYPE_INSTR, 4, 9 }, { true, false } },
        { { TRACE_TYPE_WRITE, 4, 10 }, { true, false } },
        { { TRACE_TYPE_INSTR, 4, 11 }, { true, false } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VIRTUAL_ADDRESS, 12 }, { true, true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_PHYSICAL_ADDRESS, 13 }, { true, true } },
        { { TRACE_TYPE_READ, 4, 14 }, { true, false } },
        { { TRACE_TYPE_INSTR, 4, 15 }, { true, false } },
        /* Unit header. */
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, 16 }, { true, false } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, 17 }, { true, false } },
        /* Unit header. */
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, 18 }, { true, true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, 19 }, { true, true } },
        { { TRACE_TYPE_INSTR, 4, 20 }, { true, false } },
        /* First half supposed to end here. See SPLIT_AT_INSTR_COUNT. */
        { { TRACE_TYPE_INSTR, 4, 21 }, { false, true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VIRTUAL_ADDRESS, 22 }, { false, true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_PHYSICAL_ADDRESS_NOT_AVAILABLE, 23 },
          { false, true } },
        { { TRACE_TYPE_READ, 4, 24 }, { false, true } },
        { { TRACE_TYPE_WRITE, 4, 25 }, { false, true } },
        /* Unit header. */
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, 26 }, { false, true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, 27 }, { false, true } },
        { { TRACE_TYPE_INSTR, 4, 28 }, { false, true } },
        { { TRACE_TYPE_READ, 4, 29 }, { false, true } },
        { { TRACE_TYPE_WRITE, 4, 30 }, { false, true } },
        /* Trace shard footer. */
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER, 31 }, { true, true } },
        { { TRACE_TYPE_FOOTER, 0, 32 }, { true, true } },
    };

    /* Check each half. */
    for (int k = 0; k < 2; ++k) {
        dynamorio::drmemtrace::record_filter_t::record_filter_func_t *toggle_filter =
            new dynamorio::drmemtrace::toggle_filter_t(SPLIT_AT_INSTR_COUNT, k == 0);
        test_record_filter_t *record_filter = new test_record_filter_t({ toggle_filter });
        void *shard_data = record_filter->parallel_shard_init_stream(0, nullptr, nullptr);
        for (int i = 0; i < (int)entries.size(); ++i) {
            if (!record_filter->parallel_shard_memref(shard_data, entries[i].entry)) {
                fprintf(stderr, "Filtering failed\n");
                return false;
            }
        }
        if (!record_filter->parallel_shard_exit(shard_data)) {
            fprintf(stderr, "Filtering exit failed\n");
            return false;
        }
        std::vector<trace_entry_t> half = record_filter->get_output_entries();
        delete record_filter;
        delete toggle_filter;
        int j = 0;
        for (int i = 0; i < (int)entries.size(); ++i) {
            if (entries[i].in_half[k]) {
                if (j >= (int)half.size()) {
                    fprintf(stderr, "Too few entries in filtered half %d. Expected: ", k);
                    print_entry(entries[i].entry);
                    fprintf(stderr, "\n");
                    return false;
                }
                // We do not verify encoding content for instructions.
                if (memcmp(&half[j], &entries[i].entry, sizeof(trace_entry_t)) != 0) {
                    fprintf(stderr, "Wrong filter result for half %d. Expected: ", k);
                    print_entry(entries[i].entry);
                    fprintf(stderr, ", got: ");
                    print_entry(half[j]);
                    fprintf(stderr, "\n");
                    return false;
                }
                ++j;
            }
        }
        if (j < (int)half.size()) {
            fprintf(stderr, "Got %d extra entries in filtered half %d. Next one: ",
                    (int)half.size() - j, k);
            print_entry(half[j]);
            fprintf(stderr, "\n");
            return false;
        }
    }
    fprintf(stderr, "test_toggle_filter passed\n");
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

    dynamorio::drmemtrace::record_filter_t::record_filter_func_t *null_filter =
        new dynamorio::drmemtrace::null_filter_t();
    std::vector<dynamorio::drmemtrace::record_filter_t::record_filter_func_t *>
        filter_funcs;
    filter_funcs.push_back(null_filter);
    record_analysis_tool_t *record_filter =
        new dynamorio::drmemtrace::record_filter_t(output_dir, filter_funcs,
                                                   /*verbosity=*/0);
    std::vector<record_analysis_tool_t *> tools;
    tools.push_back(record_filter);
    record_analyzer_t record_analyzer(op_trace_dir.get_value(), &tools[0],
                                      (int)tools.size());
    if (!record_analyzer) {
        FATAL_ERROR("Failed to initialize record filter: %s",
                    record_analyzer.get_error_string().c_str());
    }
    if (!record_analyzer.run()) {
        FATAL_ERROR("Failed to run record filter: %s",
                    record_analyzer.get_error_string().c_str());
    }
    delete record_filter;
    delete null_filter;

    basic_counts_t::counters_t c1 = get_basic_counts(op_trace_dir.get_value());
    basic_counts_t::counters_t c2 = get_basic_counts(output_dir);
    CHECK(c1.instrs != 0, "Bad input trace\n");
    CHECK(c1 == c2, "Null filter returned different counts\n");
    fprintf(stderr, "test_null_filter passed\n");
    return true;
}

int
main(int argc, const char *argv[])
{
    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_FRONTEND, argc, (const char **)argv,
                                       &parse_err, NULL) ||
        op_trace_dir.get_value().empty() || op_tmp_output_dir.get_value().empty()) {
        FATAL_ERROR("Usage error: %s\nUsage:\n%s", parse_err.c_str(),
                    droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
    }
    if (!test_toggle_filter() || !test_null_filter())
        return 1;
    fprintf(stderr, "All done!\n");
    return 0;
}
