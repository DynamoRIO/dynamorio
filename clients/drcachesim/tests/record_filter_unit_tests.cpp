/* **********************************************************
 * Copyright (c) 2022-2024 Google, Inc.  All rights reserved.
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
#include "archive_ostream.h"
#include "dr_api.h"
#include "droption.h"
#include "tools/basic_counts.h"
#include "tools/filter/null_filter.h"
#include "tools/filter/cache_filter.h"
#include "tools/filter/record_filter.h"
#include "tools/filter/trim_filter.h"
#include "tools/filter/type_filter.h"
#include "tools/filter/encodings2regdeps_filter.h"
#include "trace_entry.h"
#include "zipfile_ostream.h"

#include <inttypes.h>
#include <fstream>
#include <set>
#include <vector>

namespace dynamorio {
namespace drmemtrace {

using record_filter_func_t =
    ::dynamorio::drmemtrace::record_filter_t::record_filter_func_t;
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
                         uint64_t last_timestamp, bool write_archive = false)
        : record_filter_t("", std::move(filters), last_timestamp,
                          /*verbose=*/0)
        , write_archive_(write_archive)
    {
    }
    std::vector<trace_entry_t>
    get_output_entries()
    {
        return output_;
    }

protected:
    bool
    write_trace_entry(dynamorio::drmemtrace::record_filter_t::per_shard_t *shard,
                      const trace_entry_t &entry) override
    {
        output_.push_back(entry);
        shard->cur_refs += shard->memref_counter.entry_memref_count(&entry);
        shard->last_written_record = entry;
        return true;
    }
    std::string
    get_writer(per_shard_t *per_shard, memtrace_stream_t *shard_stream) override
    {
        if (write_archive_) {
            per_shard->archive_writer =
                std::unique_ptr<archive_ostream_t>(new zipfile_ostream_t("/dev/null"));
            per_shard->writer = per_shard->archive_writer.get();
            return open_new_chunk(per_shard);
        }
        per_shard->file_writer =
            std::unique_ptr<std::ostream>(new std::ofstream("/dev/null"));
        per_shard->writer = per_shard->file_writer.get();
        return "";
    }
    std::string
    remove_output_file(per_shard_t *per_shard) override
    {
        output_.clear();
        return "";
    }

private:
    std::vector<trace_entry_t> output_;
    bool write_archive_ = false;
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
    int64_t
    get_input_id() const override
    {
        // Just one input for our tests.
        return 0;
    }

private:
    uint64_t last_timestamp_;
};

struct test_case_t {
    trace_entry_t entry;
    // Specifies whether the entry should be processed by the record_filter
    // as an input. Some entries are added only to show the expected output
    // and shouldn't be used as input to the record_filter.
    bool input;
    // Specifies whether the entry should be expected in the result of the
    // record filter. This is an array of size equal to the number of test
    // cases.
    std::vector<bool> output;
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

bool
process_entries_and_check_result(test_record_filter_t *record_filter,
                                 const std::vector<test_case_t> &entries, int index)
{
    record_filter->initialize_stream(nullptr);
    auto stream = std::unique_ptr<local_stream_t>(new local_stream_t());
    void *shard_data =
        record_filter->parallel_shard_init_stream(0, nullptr, stream.get());
    if (!*record_filter) {
        fprintf(stderr, "Filtering init failed: %s\n",
                record_filter->get_error_string().c_str());
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
            fprintf(stderr, "Filtering failed on entry %d: %s\n", i,
                    record_filter->parallel_shard_error(shard_data).c_str());
            return false;
        }
    }
    if (!record_filter->parallel_shard_exit(shard_data) || !*record_filter) {
        fprintf(stderr, "Filtering exit failed\n");
        return false;
    }
    if (!record_filter->print_results()) {
        fprintf(stderr, "Filtering results failed\n");
        return false;
    }

    std::vector<trace_entry_t> filtered = record_filter->get_output_entries();
    // Verbose output for easier debugging.
    fprintf(stderr, "Input:\n");
    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        if (!entries[i].input)
            continue;
        fprintf(stderr, "  %d: ", i);
        print_entry(entries[i].entry);
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "Output:\n");
    for (int i = 0; i < static_cast<int>(filtered.size()); ++i) {
        fprintf(stderr, "  %d: ", i);
        print_entry(filtered[i]);
        fprintf(stderr, "\n");
    }
    // Check filtered output entries.
    int j = 0;
    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        if (!entries[i].output[index])
            continue;
        if (j >= static_cast<int>(filtered.size())) {
            fprintf(stderr,
                    "Too few entries in filtered output (iter=%d). Expected: ", index);
            print_entry(entries[i].entry);
            fprintf(stderr, "\n");
            return false;
        }
        if (memcmp(&filtered[j], &entries[i].entry, sizeof(trace_entry_t)) != 0) {
            fprintf(stderr,
                    "Wrong filter result for iter=%d, at pos=%d. Expected: ", index, i);
            print_entry(entries[i].entry);
            fprintf(stderr, ", got: ");
            print_entry(filtered[j]);
            fprintf(stderr, "\n");
            return false;
        }
        ++j;
    }
    if (j < static_cast<int>(filtered.size())) {
        fprintf(stderr, "Got %d extra entries in filtered output (iter=%d). Next one: ",
                static_cast<int>(filtered.size()) - j, index);
        print_entry(filtered[j]);
        fprintf(stderr, "\n");
        return false;
    }
    return true;
}

static bool
test_encodings2regdeps_filter()
{
    constexpr addr_t PC = 0x7f6fdd3ec360;
    constexpr addr_t PC2 = 0x7f6fdd3eb1f7;
    // constexpr addr_t PC2 = 0x00007f6fdd3eb1f7;
    constexpr addr_t PC3 = 0x7f6fdd3eb21a;
    constexpr addr_t ENCODING_REAL_ISA = 0xe78948;
    constexpr addr_t ENCODING_REAL_ISA_2_PART1 = 0x841f0f66;
    constexpr addr_t ENCODING_REAL_ISA_2_PART2 = 0x0;
    constexpr addr_t ENCODING_REAL_ISA_3 = 0xab48f3;
    constexpr addr_t ENCODING_REGDEPS_ISA = 0x0006090600010011;
    constexpr addr_t ENCODING_REGDEPS_ISA_2 = 0x0000020400004010;
    constexpr addr_t ENCODING_REGDEPS_ISA_3_PART1 = 0x0209030600001042;
    constexpr addr_t ENCODING_REGDEPS_ISA_3_PART2 = 0x0000000000220903;
    std::vector<test_case_t> entries = {
        /* Trace shard header.
         */
        { { TRACE_TYPE_HEADER, 0, { 0x1 } }, true, { true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION, { 0x2 } }, true, { true } },
        /* File type, modified by record_filter encodings2regdeps to add
         * OFFLINE_FILE_TYPE_ARCH_REGDEPS.
         */
        { { TRACE_TYPE_MARKER,
            TRACE_MARKER_TYPE_FILETYPE,
            { OFFLINE_FILE_TYPE_ARCH_X86_64 | OFFLINE_FILE_TYPE_ENCODINGS |
              OFFLINE_FILE_TYPE_SYSCALL_NUMBERS | OFFLINE_FILE_TYPE_BLOCKING_SYSCALLS } },
          true,
          { false } },
        { { TRACE_TYPE_MARKER,
            TRACE_MARKER_TYPE_FILETYPE,
            { OFFLINE_FILE_TYPE_ARCH_REGDEPS | OFFLINE_FILE_TYPE_ENCODINGS |
              OFFLINE_FILE_TYPE_SYSCALL_NUMBERS | OFFLINE_FILE_TYPE_BLOCKING_SYSCALLS } },
          false,
          { true } },
        { { TRACE_TYPE_THREAD, 0, { 0x4 } }, true, { true } },
        { { TRACE_TYPE_PID, 0, { 0x5 } }, true, { true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, { 0x6 } },
          true,
          { true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT, { 0x3 } },
          true,
          { true } },

        /* Chunk 1.
         */
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 0x7 } }, true, { true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0x8 } }, true, { true } },
        /* Encoding, modified by the record_filter encodings2regdeps.
         * encoding real ISA size == encoding regdeps ISA size
         * (in terms of trace_entry_t).
         */
        { { TRACE_TYPE_ENCODING, 3, { ENCODING_REAL_ISA } }, true, { false } },
        { { TRACE_TYPE_ENCODING, 8, { ENCODING_REGDEPS_ISA } }, false, { true } },
        { { TRACE_TYPE_INSTR, 3, { PC } }, true, { true } },
        { { TRACE_TYPE_INSTR, 3, { PC } }, true, { true } },
        { { TRACE_TYPE_INSTR, 3, { PC } }, true, { true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER, { 0 } }, true, { true } },

        /* Chunk 2.
         */
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL, { 0xa } },
          true,
          { true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 0x7 } }, true, { true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0x8 } }, true, { true } },
        /* Duplicated encoding across chunk boundary.
         */
        { { TRACE_TYPE_ENCODING, 3, { ENCODING_REAL_ISA } }, true, { false } },
        { { TRACE_TYPE_ENCODING, 8, { ENCODING_REGDEPS_ISA } }, false, { true } },
        { { TRACE_TYPE_INSTR, 3, { PC } }, true, { true } },
        { { TRACE_TYPE_INSTR, 3, { PC } }, true, { true } },
        { { TRACE_TYPE_INSTR, 3, { PC } }, true, { true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER, { 1 } }, true, { true } },

        /* Chunk 3.
         */
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL, { 0xe } },
          true,
          { true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 0x7 } }, true, { true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0x8 } }, true, { true } },
        /* encoding real ISA size > encoding regdeps ISA size
         */
        { { TRACE_TYPE_ENCODING, 8, { ENCODING_REAL_ISA_2_PART1 } }, true, { false } },
        { { TRACE_TYPE_ENCODING, 1, { ENCODING_REAL_ISA_2_PART2 } }, true, { false } },
        { { TRACE_TYPE_ENCODING, 8, { ENCODING_REGDEPS_ISA_2 } }, false, { true } },
        { { TRACE_TYPE_INSTR, 9, { PC2 } }, true, { true } },
        { { TRACE_TYPE_INSTR, 9, { PC2 } }, true, { true } },
        { { TRACE_TYPE_INSTR, 9, { PC2 } }, true, { true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER, { 2 } }, true, { true } },

        /* Chunk 4.
         */
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL, { 0x12 } },
          true,
          { true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 0x7 } }, true, { true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0x8 } }, true, { true } },
        /* encoding real ISA size < encoding regdeps ISA size
         */
        { { TRACE_TYPE_ENCODING, 3, { ENCODING_REAL_ISA_3 } }, true, { false } },
        { { TRACE_TYPE_ENCODING, 8, { ENCODING_REGDEPS_ISA_3_PART1 } }, false, { true } },
        { { TRACE_TYPE_ENCODING, 4, { ENCODING_REGDEPS_ISA_3_PART2 } }, false, { true } },
        { { TRACE_TYPE_INSTR, 3, { PC3 } }, true, { true } },
        { { TRACE_TYPE_INSTR, 3, { PC3 } }, true, { true } },

        /* Trace shard footer.
         */
        { { TRACE_TYPE_FOOTER, 0, { 0x0 } }, true, { true } },
    };

    /* Construct encodings2regdeps_filter.
     */
    std::vector<std::unique_ptr<record_filter_func_t>> filters;
    auto encodings2regdeps_filter = std::unique_ptr<record_filter_func_t>(
        new dynamorio::drmemtrace::encodings2regdeps_filter_t());
    if (encodings2regdeps_filter->get_error_string() != "") {
        fprintf(stderr, "Couldn't construct a encodings2regdeps_filter %s",
                encodings2regdeps_filter->get_error_string().c_str());
        return false;
    }
    filters.push_back(std::move(encodings2regdeps_filter));

    /* Construct record_filter_t.
     */
    auto record_filter = std::unique_ptr<test_record_filter_t>(
        new test_record_filter_t(std::move(filters), 0, /*write_archive=*/true));

    /* Run the test.
     */
    if (!process_entries_and_check_result(record_filter.get(), entries, 0))
        return false;

    fprintf(stderr, "test_encodings2regdeps_filter passed\n");
    return true;
}

static bool
test_cache_and_type_filter()
{
    // We test two configurations:
    // 1. filter data address stream using a cache, and filter function markers
    //    and encoding entries, without any stop timestamp.
    // 2. filter data and instruction address stream using a cache, with a
    //    stop timestamp.
    std::vector<test_case_t> entries = {
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
        { { TRACE_TYPE_ENCODING, 4, { 0xf00d } }, true, { false, true } },
        { { TRACE_TYPE_INSTR, 4, { 0xaa00 } }, true, { true, true } },
        { { TRACE_TYPE_WRITE, 4, { 0xaa80 } }, true, { true, true } },

        // Unit header.
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 0x9 } },
          true,
          { true, true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0xa } },
          true,
          { true, true } },
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
        { { TRACE_TYPE_INSTR, 6, { 0xaa80 } }, true, { true, false } },
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
        { { TRACE_TYPE_INSTR, 6, { 0xab80 } }, true, { true, true } },
        // For the 2nd test: Delayed encodings from the previous instance
        // of the instruction at PC 0xaa80 that was filtered out.
        { { TRACE_TYPE_ENCODING, 4, { 0x8bad } }, false, { false, true } },
        { { TRACE_TYPE_ENCODING, 2, { 0xf00d } }, false, { false, true } },
        { { TRACE_TYPE_INSTR, 6, { 0xaa80 } }, true, { true, true } },
        // Trace shard footer.
        { { TRACE_TYPE_FOOTER, 0, { 0xa2 } }, true, { true, true } }
    };

    for (int k = 0; k < 2; ++k) {
        // Construct record_filter_func_ts.
        std::vector<std::unique_ptr<record_filter_func_t>> filters;
        auto cache_filter = std::unique_ptr<record_filter_func_t>(
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
            auto type_filter = std::unique_ptr<record_filter_func_t>(
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
        if (!process_entries_and_check_result(record_filter.get(), entries, k))
            return false;
    }
    fprintf(stderr, "test_cache_and_type_filter passed\n");
    return true;
}

static bool
test_chunk_update()
{
    {
        // Test that the ordinal marker is updated on removing records.
        // From Chunk 1 we remove 3 visible records (the _FUNC_ ones); the encodings
        // are also removed but are not visible in the record count.
        std::vector<test_case_t> entries = {
            // Header.
            { { TRACE_TYPE_HEADER, 0, { 0x1 } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION, { 0x2 } }, true, { true } },
            { { TRACE_TYPE_MARKER,
                TRACE_MARKER_TYPE_FILETYPE,
                { OFFLINE_FILE_TYPE_ENCODINGS } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE, { 0 } }, false, { true } },
            { { TRACE_TYPE_THREAD, 0, { 0x4 } }, true, { true } },
            { { TRACE_TYPE_PID, 0, { 0x5 } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, { 0x6 } },
              true,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT, { 0x2 } },
              true,
              { true } },
            // Chunk 1.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 0x7 } },
              true,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0x8 } }, true, { true } },
            { { TRACE_TYPE_ENCODING, 2, { 0xf00d } }, true, { false } },
            { { TRACE_TYPE_INSTR, 2, { 0x1234 } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ID, { 0 } }, true, { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_RETADDR, { 0 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ARG, { 0 } }, true, { false } },
            { { TRACE_TYPE_ENCODING, 2, { 0xf00d } }, true, { false } },
            { { TRACE_TYPE_INSTR, 2, { 0x1235 } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER, { 0 } },
              true,
              { true } },
            // Chunk 2.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL, { 12 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL, { 9 } },
              false,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 0x7 } },
              true,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0x8 } }, true, { true } },
            { { TRACE_TYPE_ENCODING, 2, { 0xf00d } }, true, { false } },
            { { TRACE_TYPE_INSTR, 2, { 0x1236 } }, true, { true } },
            { { TRACE_TYPE_FOOTER, 0, { 0xa2 } }, true, { true } },
        };
        std::vector<std::unique_ptr<record_filter_func_t>> filters;
        auto filter = std::unique_ptr<record_filter_func_t>(
            new dynamorio::drmemtrace::type_filter_t({ TRACE_TYPE_ENCODING },
                                                     { TRACE_MARKER_TYPE_FUNC_ID,
                                                       TRACE_MARKER_TYPE_FUNC_RETADDR,
                                                       TRACE_MARKER_TYPE_FUNC_ARG }));
        if (!filter->get_error_string().empty()) {
            fprintf(stderr, "Couldn't construct a type_filter %s",
                    filter->get_error_string().c_str());
            return false;
        }
        filters.push_back(std::move(filter));
        auto record_filter = std::unique_ptr<test_record_filter_t>(
            new test_record_filter_t(std::move(filters), 0, /*write_archive=*/true));
        if (!process_entries_and_check_result(record_filter.get(), entries, 0))
            return false;
    }
    {
        // Test that a filtered-out instr doesn't have new-chunk encodings added.
        class ordinal_filter_t : public record_filter_t::record_filter_func_t {
        public:
            ordinal_filter_t(std::set<int> ordinals)
                : ordinals_(ordinals)
            {
            }
            void *
            parallel_shard_init(memtrace_stream_t *shard_stream,
                                bool partial_trace_filter) override
            {
                return nullptr;
            }
            bool
            parallel_shard_filter(
                trace_entry_t &entry, void *shard_data,
                record_filter_t::record_filter_info_t &record_filter_info) override
            {
                bool res = true;
                if (type_is_instr(static_cast<trace_type_t>(entry.type))) {
                    if (ordinals_.find(cur_ord_) != ordinals_.end())
                        res = false;
                    ++cur_ord_;
                }
                return res;
            }
            bool
            parallel_shard_exit(void *shard_data) override
            {
                return true;
            }

        private:
            // Our test class supports only a single small shard.
            std::set<int> ordinals_;
            int cur_ord_ = 0;
        };
        constexpr addr_t PC_A = 0x1234;
        constexpr addr_t PC_B = 0x5678;
        constexpr addr_t ENCODING_A = 0x4321;
        constexpr addr_t ENCODING_B = 0x8765;
        // We have the following where "e" means encoding and | divides chunks and
        // x means we removed it:
        //   "eA A A | eB B eA" => "eA A x eB | x eA"
        // We check to ensure the 2nd B, now removed, has no encoding added.
        std::vector<test_case_t> entries = {
            // Header.
            { { TRACE_TYPE_HEADER, 0, { 0x1 } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION, { 0x2 } }, true, { true } },
            { { TRACE_TYPE_MARKER,
                TRACE_MARKER_TYPE_FILETYPE,
                { OFFLINE_FILE_TYPE_ENCODINGS } },
              true,
              { true } },
            { { TRACE_TYPE_THREAD, 0, { 0x4 } }, true, { true } },
            { { TRACE_TYPE_PID, 0, { 0x5 } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, { 0x6 } },
              true,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT, { 0x3 } },
              true,
              { true } },
            // Chunk 1.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 0x7 } },
              true,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0x8 } }, true, { true } },
            { { TRACE_TYPE_ENCODING, 2, { ENCODING_A } }, true, { true } },
            { { TRACE_TYPE_INSTR, 2, { PC_A } }, true, { true } },
            { { TRACE_TYPE_INSTR, 2, { PC_A } }, true, { true } },
            { { TRACE_TYPE_INSTR, 2, { PC_A } }, true, { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER, { 0 } },
              true,
              { false } },
            // Chunk 2.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL, { 10 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 0x8 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0x8 } }, true, { false } },
            { { TRACE_TYPE_ENCODING, 2, { ENCODING_B } }, true, { true } },
            { { TRACE_TYPE_INSTR, 2, { PC_B } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER, { 0 } },
              false,
              { true } },
            // New chunk 2.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL, { 10 } },
              false,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 0x8 } },
              false,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0x8 } }, false, { true } },
            // This is the heart of this test: there should be no inserted new-chunk
            // encoding for this filtered-out instruction.
            { { TRACE_TYPE_INSTR, 2, { PC_B } }, true, { false } },
            { { TRACE_TYPE_ENCODING, 2, { ENCODING_A } }, true, { true } },
            { { TRACE_TYPE_INSTR, 2, { PC_A } }, true, { true } },
            { { TRACE_TYPE_FOOTER, 0, { 0xa2 } }, true, { true } },
        };
        std::vector<std::unique_ptr<record_filter_func_t>> filters;
        auto filter =
            std::unique_ptr<record_filter_func_t>(new ordinal_filter_t({ 2, 4 }));
        if (!filter->get_error_string().empty()) {
            fprintf(stderr, "Couldn't construct a pc_filter %s",
                    filter->get_error_string().c_str());
            return false;
        }
        filters.push_back(std::move(filter));
        auto record_filter = std::unique_ptr<test_record_filter_t>(
            new test_record_filter_t(std::move(filters), 0, /*write_archive=*/true));
        if (!process_entries_and_check_result(record_filter.get(), entries, 0))
            return false;
    }
    fprintf(stderr, "test_chunk_update passed\n");
    return true;
}

static bool
test_trim_filter()
{
    constexpr addr_t TID = 5;
    constexpr addr_t PC_A = 0x1234;
    constexpr addr_t ENCODING_A = 0x4321;
    constexpr addr_t PC_B = 0x5678;
    constexpr addr_t ENCODING_B = 0x8765;
    {
        // Test invalid parameters.
        auto filter = std::unique_ptr<record_filter_func_t>(
            new dynamorio::drmemtrace::trim_filter_t(150, 149));
        if (filter->get_error_string().empty()) {
            fprintf(stderr, "Failed to return an error on invalid params");
            return false;
        }
        auto filter2 = std::unique_ptr<record_filter_func_t>(
            new dynamorio::drmemtrace::trim_filter_t(150, 150));
        if (filter2->get_error_string().empty()) {
            fprintf(stderr, "Failed to return an error on invalid params");
            return false;
        }
    }
    {
        // Test removing from mid-way in the 1st chunk to the very end.
        std::vector<test_case_t> entries = {
            // Header.
            { { TRACE_TYPE_HEADER, 0, { 0x1 } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION, { 0x2 } }, true, { true } },
            { { TRACE_TYPE_MARKER,
                TRACE_MARKER_TYPE_FILETYPE,
                { OFFLINE_FILE_TYPE_ENCODINGS } },
              true,
              { true } },
            { { TRACE_TYPE_THREAD, 0, { TID } }, true, { true } },
            { { TRACE_TYPE_PID, 0, { 0x5 } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, { 0x6 } },
              true,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT, { 2 } },
              true,
              { true } },
            // Chunk 1.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 100 } },
              true,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0 } }, true, { true } },
            { { TRACE_TYPE_ENCODING, 2, { ENCODING_A } }, true, { true } },
            { { TRACE_TYPE_INSTR, 2, { PC_A } }, true, { true } },
            // Removal starts here.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 200 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0 } }, true, { false } },
            { { TRACE_TYPE_ENCODING, 2, { ENCODING_B } }, true, { false } },
            { { TRACE_TYPE_INSTR, 2, { PC_B } }, true, { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER, { 0 } },
              true,
              { false } },
            // Chunk 2.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL, { 12 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 200 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0 } }, true, { false } },
            { { TRACE_TYPE_ENCODING, 2, { ENCODING_B } }, true, { false } },
            { { TRACE_TYPE_INSTR, 2, { PC_B } }, true, { false } },
            // These footer records should remain.
            { { TRACE_TYPE_THREAD_EXIT, 0, { TID } }, true, { true } },
            { { TRACE_TYPE_FOOTER, 0, { 0xa2 } }, true, { true } },
        };
        std::vector<std::unique_ptr<record_filter_func_t>> filters;
        auto filter = std::unique_ptr<record_filter_func_t>(
            new dynamorio::drmemtrace::trim_filter_t(50, 150));
        if (!filter->get_error_string().empty()) {
            fprintf(stderr, "Couldn't construct a trim_filter %s",
                    filter->get_error_string().c_str());
            return false;
        }
        filters.push_back(std::move(filter));
        auto record_filter = std::unique_ptr<test_record_filter_t>(
            new test_record_filter_t(std::move(filters), 0, /*write_archive=*/true));
        if (!process_entries_and_check_result(record_filter.get(), entries, 0))
            return false;
    }
    {
        // Test removing from the start to mid-way in the 1st chunk.
        // This requires repeating encodings in the new chunks.
        std::vector<test_case_t> entries = {
            // Header.
            { { TRACE_TYPE_HEADER, 0, { 0x1 } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION, { 0x2 } }, true, { true } },
            { { TRACE_TYPE_MARKER,
                TRACE_MARKER_TYPE_FILETYPE,
                { OFFLINE_FILE_TYPE_ENCODINGS } },
              true,
              { true } },
            { { TRACE_TYPE_THREAD, 0, { TID } }, true, { true } },
            { { TRACE_TYPE_PID, 0, { 0x5 } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, { 0x6 } },
              true,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT, { 2 } },
              true,
              { true } },
            // Original chunk 1.
            // Removal starts here.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 100 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0 } }, true, { false } },
            { { TRACE_TYPE_ENCODING, 2, { ENCODING_B } }, true, { false } },
            { { TRACE_TYPE_INSTR, 2, { PC_B } }, true, { false } },
            // Removal ends here.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 200 } },
              true,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0 } }, true, { true } },
            { { TRACE_TYPE_ENCODING, 2, { ENCODING_B } }, false, { true } },
            { { TRACE_TYPE_INSTR, 2, { PC_B } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER, { 0 } },
              true,
              { false } },
            // Original chunk 2.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL, { 12 } },
              true,
              { false } },
            // Dup timestamp;cpuid should be removed.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 200 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0 } }, true, { false } },
            // This encoding is not repeated b/c this is now in the same chunk as
            // the prior instance of this same instr.
            { { TRACE_TYPE_ENCODING, 2, { ENCODING_B } }, true, { false } },
            { { TRACE_TYPE_INSTR, 2, { PC_B } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER, { 0 } },
              false,
              { true } },
            // New chunk 2.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL, { 9 } },
              false,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 200 } },
              false,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0 } }, false, { true } },
            // This encoding is added since it is the first instance in a new chunk.
            { { TRACE_TYPE_ENCODING, 2, { ENCODING_B } }, false, { true } },
            { { TRACE_TYPE_INSTR, 2, { PC_B } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER, { 0 } },
              true,
              { false } },
            // Original chunk 3.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL, { 12 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 200 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0 } }, true, { false } },
            { { TRACE_TYPE_ENCODING, 2, { ENCODING_B } }, true, { false } },
            { { TRACE_TYPE_INSTR, 2, { PC_B } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER, { 1 } },
              false,
              { true } },
            // New chunk 3.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL, { 12 } },
              false,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 200 } },
              false,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0 } }, false, { true } },
            { { TRACE_TYPE_THREAD_EXIT, 0, { TID } }, true, { true } },
            { { TRACE_TYPE_FOOTER, 0, { 0xa2 } }, true, { true } },
        };
        std::vector<std::unique_ptr<record_filter_func_t>> filters;
        auto filter = std::unique_ptr<record_filter_func_t>(
            new dynamorio::drmemtrace::trim_filter_t(150, 600));
        if (!filter->get_error_string().empty()) {
            fprintf(stderr, "Couldn't construct a trim_filter %s",
                    filter->get_error_string().c_str());
            return false;
        }
        filters.push_back(std::move(filter));
        auto record_filter = std::unique_ptr<test_record_filter_t>(
            new test_record_filter_t(std::move(filters), 0, /*write_archive=*/true));
        if (!process_entries_and_check_result(record_filter.get(), entries, 0))
            return false;
    }
    {
        // Test removing a zero-instruction thread.
        std::vector<test_case_t> entries = {
            // Header.
            { { TRACE_TYPE_HEADER, 0, { 0x1 } }, true, { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION, { 0x2 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER,
                TRACE_MARKER_TYPE_FILETYPE,
                { OFFLINE_FILE_TYPE_ENCODINGS } },
              true,
              { false } },
            { { TRACE_TYPE_THREAD, 0, { TID } }, true, { false } },
            { { TRACE_TYPE_PID, 0, { 0x5 } }, true, { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, { 0x6 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT, { 2 } },
              true,
              { false } },
            // Original chunk 1.
            // Removal starts here.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 100 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0 } }, true, { false } },
            { { TRACE_TYPE_ENCODING, 2, { ENCODING_B } }, true, { false } },
            { { TRACE_TYPE_INSTR, 2, { PC_B } }, true, { false } },
            // Removal ends here.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 200 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0 } }, true, { false } },
            { { TRACE_TYPE_THREAD_EXIT, 0, { TID } }, true, { false } },
            { { TRACE_TYPE_FOOTER, 0, { 0xa2 } }, true, { false } },
        };
        std::vector<std::unique_ptr<record_filter_func_t>> filters;
        auto filter = std::unique_ptr<record_filter_func_t>(
            new dynamorio::drmemtrace::trim_filter_t(150, 600));
        if (!filter->get_error_string().empty()) {
            fprintf(stderr, "Couldn't construct a trim_filter %s",
                    filter->get_error_string().c_str());
            return false;
        }
        filters.push_back(std::move(filter));
        auto record_filter = std::unique_ptr<test_record_filter_t>(
            new test_record_filter_t(std::move(filters), 0, /*write_archive=*/true));
        if (!process_entries_and_check_result(record_filter.get(), entries, 0))
            return false;
    }
    {
        // Test removing from the start to mid-way in the 1st chunk while also
        // removing all encodings.
        std::vector<test_case_t> entries = {
            // Header.
            { { TRACE_TYPE_HEADER, 0, { 0x1 } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION, { 0x2 } }, true, { true } },
            { { TRACE_TYPE_MARKER,
                TRACE_MARKER_TYPE_FILETYPE,
                { OFFLINE_FILE_TYPE_ENCODINGS } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE, { 0 } }, false, { true } },
            { { TRACE_TYPE_THREAD, 0, { TID } }, true, { true } },
            { { TRACE_TYPE_PID, 0, { 0x5 } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, { 0x6 } },
              true,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT, { 2 } },
              true,
              { true } },
            // Original chunk 1.
            // Removal starts here.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 100 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0 } }, true, { false } },
            { { TRACE_TYPE_ENCODING, 2, { ENCODING_B } }, true, { false } },
            { { TRACE_TYPE_INSTR, 2, { PC_B } }, true, { false } },
            // Removal ends here.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 200 } },
              true,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0 } }, true, { true } },
            { { TRACE_TYPE_ENCODING, 2, { ENCODING_B } }, false, { false } },
            { { TRACE_TYPE_INSTR, 2, { PC_B } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER, { 0 } },
              true,
              { false } },
            // Original chunk 2.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL, { 12 } },
              true,
              { false } },
            // Dup timestamp;cpuid should be removed.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 200 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0 } }, true, { false } },
            { { TRACE_TYPE_ENCODING, 2, { ENCODING_B } }, true, { false } },
            { { TRACE_TYPE_INSTR, 2, { PC_B } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER, { 0 } },
              false,
              { true } },
            // New chunk 2.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL, { 9 } },
              false,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 200 } },
              false,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0 } }, false, { true } },
            // An encoding would be added here, but we want it removed.
            { { TRACE_TYPE_ENCODING, 2, { ENCODING_B } }, false, { false } },
            { { TRACE_TYPE_INSTR, 2, { PC_B } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER, { 0 } },
              true,
              { false } },
            // Original chunk 3.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL, { 12 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 200 } },
              true,
              { false } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0 } }, true, { false } },
            { { TRACE_TYPE_ENCODING, 2, { ENCODING_B } }, true, { false } },
            { { TRACE_TYPE_INSTR, 2, { PC_B } }, true, { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER, { 1 } },
              false,
              { true } },
            // New chunk 3.
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL, { 12 } },
              false,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 200 } },
              false,
              { true } },
            { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0 } }, false, { true } },
            { { TRACE_TYPE_THREAD_EXIT, 0, { TID } }, true, { true } },
            { { TRACE_TYPE_FOOTER, 0, { 0xa2 } }, true, { true } },
        };
        std::vector<std::unique_ptr<record_filter_func_t>> filters;
        auto filter = std::unique_ptr<record_filter_func_t>(
            new dynamorio::drmemtrace::trim_filter_t(150, 600));
        if (!filter->get_error_string().empty()) {
            fprintf(stderr, "Couldn't construct a trim_filter %s",
                    filter->get_error_string().c_str());
            return false;
        }
        filters.push_back(std::move(filter));
        auto type_filter = std::unique_ptr<record_filter_func_t>(
            new dynamorio::drmemtrace::type_filter_t({ TRACE_TYPE_ENCODING }, {}));
        if (type_filter->get_error_string() != "") {
            fprintf(stderr, "Couldn't construct a type_filter %s",
                    type_filter->get_error_string().c_str());
            return false;
        }
        filters.push_back(std::move(type_filter));
        auto record_filter = std::unique_ptr<test_record_filter_t>(
            new test_record_filter_t(std::move(filters), 0, /*write_archive=*/true));
        if (!process_entries_and_check_result(record_filter.get(), entries, 0))
            return false;
    }
    fprintf(stderr, "test_trim_filter passed\n");
    return true;
}

// Tests I/O for the record_filter on a legacy trace.
// We also have the tool.record_filter test which tests on a freshly generated
// zipfile trace.
static bool
test_null_filter()
{
    std::string output_dir = op_tmp_output_dir.get_value() + DIRSEP + "null_filter";
    if (!local_create_dir(output_dir.c_str())) {
        FATAL_ERROR("Failed to create filtered trace output dir %s", output_dir.c_str());
    }
    auto null_filter =
        std::unique_ptr<record_filter_func_t>(new dynamorio::drmemtrace::null_filter_t());
    std::vector<std::unique_ptr<record_filter_func_t>> filter_funcs;
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

static bool
test_wait_filter()
{
    // Test that wait records (artificial timing during replay) are not preserved.
    constexpr addr_t TID = 5;
    constexpr addr_t PC_A = 0x1234;
    constexpr addr_t ENCODING_A = 0x4321;
    std::vector<test_case_t> entries = {
        { { TRACE_TYPE_HEADER, 0, { 0x1 } }, true, { true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION, { 0x2 } }, true, { true } },
        { { TRACE_TYPE_MARKER,
            TRACE_MARKER_TYPE_FILETYPE,
            { OFFLINE_FILE_TYPE_ENCODINGS } },
          true,
          { true } },
        { { TRACE_TYPE_THREAD, 0, { TID } }, true, { true } },
        { { TRACE_TYPE_PID, 0, { 0x5 } }, true, { true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, { 0x6 } },
          true,
          { true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 100 } }, true, { true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 0 } }, true, { true } },
        { { TRACE_TYPE_ENCODING, 2, { ENCODING_A } }, true, { true } },
        { { TRACE_TYPE_INSTR, 2, { PC_A } }, true, { true } },
        // Test wait and idle records.
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CORE_WAIT, { 0 } }, true, { false } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CORE_WAIT, { 0 } }, true, { false } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CORE_IDLE, { 0 } }, true, { true } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CORE_WAIT, { 0 } }, true, { false } },
        { { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CORE_IDLE, { 0 } }, true, { true } },
        { { TRACE_TYPE_THREAD_EXIT, 0, { TID } }, true, { true } },
        { { TRACE_TYPE_FOOTER, 0, { 0xa2 } }, true, { true } },
    };
    std::vector<std::unique_ptr<record_filter_func_t>> filters;
    auto record_filter = std::unique_ptr<test_record_filter_t>(
        new test_record_filter_t(std::move(filters), 0, /*write_archive=*/true));
    if (!process_entries_and_check_result(record_filter.get(), entries, 0))
        return false;
    fprintf(stderr, "test_wait_filter passed\n");
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
    if (!test_cache_and_type_filter() || !test_chunk_update() || !test_trim_filter() ||
        !test_null_filter() || !test_wait_filter() || !test_encodings2regdeps_filter())
        return 1;
    fprintf(stderr, "All done!\n");
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
