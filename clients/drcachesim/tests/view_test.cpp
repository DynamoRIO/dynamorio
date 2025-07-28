/* **********************************************************
 * Copyright (c) 2021-2025 Google, LLC  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, LLC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Unit tests for the view_t tool. */

#include "test_helpers.h"
#include <algorithm>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

#include "../tools/view.h"
#include "../common/memref.h"
#include "../tracer/raw2trace.h"
#include "../reader/file_reader.h"
#include "../scheduler/scheduler.h"
#include "decode_cache.h"
#include "memref_gen.h"
#include "trace_entry.h"
#include "analyzer.h"
#include "scheduler.h"
#include "mock_reader.h"

namespace dynamorio {
namespace drmemtrace {

#undef ASSERT
#define ASSERT(cond, msg, ...)        \
    do {                              \
        if (!(cond)) {                \
            std::cerr << msg << "\n"; \
            abort();                  \
        }                             \
    } while (0)

#define CHECK(cond, msg, ...)         \
    do {                              \
        if (!(cond)) {                \
            std::cerr << msg << "\n"; \
            return false;             \
        }                             \
    } while (0)

// These are for our mock serial reader and must be in the same namespace
// as file_reader_t's declaration.
template <> file_reader_t<std::vector<trace_entry_t>>::file_reader_t()
{
}

template <> file_reader_t<std::vector<trace_entry_t>>::~file_reader_t()
{
}

template <>
bool
file_reader_t<std::vector<trace_entry_t>>::open_single_file(const std::string &path)
{
    return true;
}

template <>
trace_entry_t *
file_reader_t<std::vector<trace_entry_t>>::read_next_entry()
{
    return nullptr;
}

class view_test_t : public view_t {
public:
    view_test_t(void *drcontext, instrlist_t &instrs)
        : view_t("", "", 0)
        , instrs_(&instrs)
    {
    }
    bool
    init_decode_cache() override
    {
        decode_cache_ = std::unique_ptr<decode_cache_t<disasm_info_t>>(
            new test_decode_cache_t<disasm_info_t>(dcontext_.dcontext, false, false,
                                                   instrs_));
        if (TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, filetype_)) {
            error_string_ =
                decode_cache_->init(static_cast<offline_file_type_t>(filetype_));
        } else if (!module_file_path_.empty()) {
            error_string_ =
                decode_cache_->init(static_cast<offline_file_type_t>(filetype_),
                                    module_file_path_, knob_alt_module_dir_);
        } else {
            decode_cache_.reset(nullptr);
        }
        return error_string_.empty();
    }

    void
    print_header() override
    {
        // Eliminate the header so we can count lines with one line per record.
    }

    bool
    print_results() override
    {
        if (!parallel_shard_exit(serial_stream_))
            return false;
        // Don't print a summary, so we can count one line per record.
        return true;
    }

private:
    instrlist_t *instrs_;
};

class view_nomod_test_t : public view_t {
public:
    view_nomod_test_t(void *drcontext, instrlist_t &instrs)
        : view_t("", "", 0)
    {
    }
};

std::string
run_test_helper(view_t &view, const std::vector<memref_t> &memrefs)
{
    class local_stream_t : public default_memtrace_stream_t {
    public:
        local_stream_t(view_t &view, const std::vector<memref_t> &memrefs)
            : view_(view)
            , memrefs_(memrefs)
        {
        }

        std::string
        run()
        {
            view_.initialize_stream(this);
            // Capture cerr.
            std::stringstream capture;
            std::streambuf *prior = std::cerr.rdbuf(capture.rdbuf());
            // Run the tool.
            for (const auto &memref : memrefs_) {
                ++ref_count_;
                if (type_is_instr(memref.instr.type))
                    ++instr_count_;
                if (!view_.process_memref(memref)) {
                    if (view_.get_error_string().empty())
                        break;
                    std::cout << "Hit error: " << view_.get_error_string() << "\n";
                }
            }
            // Return the result.
            std::string res = capture.str();
            std::cerr.rdbuf(prior);
            return res;
        }

        uint64_t
        get_record_ordinal() const override
        {
            return ref_count_;
        }
        uint64_t
        get_instruction_ordinal() const override
        {
            return instr_count_;
        }

    private:
        view_t &view_;
        const std::vector<memref_t> &memrefs_;
        uint64_t ref_count_ = 0;
        uint64_t instr_count_ = 0;
    };

    local_stream_t stream(view, memrefs);
    return stream.run();
}

class mock_analyzer_t : public analyzer_t {
public:
    mock_analyzer_t(std::vector<scheduler_t::input_workload_t> &sched_inputs,
                    analysis_tool_t **tools, int num_tools, int skip, int limit)
        : analyzer_t()
    {
        num_tools_ = num_tools;
        tools_ = tools;
        parallel_ = false;
        verbosity_ = 0;
        worker_count_ = 1;
        skip_records_ = skip;
        exit_after_records_ = limit;
        scheduler_t::scheduler_options_t sched_ops =
            scheduler_t::make_scheduler_serial_options(verbosity_);
        sched_mapping_ = sched_ops.mapping;
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

static std::string
run_with_analyzer(void *drcontext, instrlist_t &ilist,
                  const std::vector<trace_entry_t> &records, int skip_records = 0,
                  int num_records = 0, bool no_modules = false)
{
    memref_tid_t tid = 42;
    std::vector<scheduler_t::input_reader_t> readers;
    readers.emplace_back(
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t(records)),
        std::unique_ptr<test_util::mock_reader_t>(new test_util::mock_reader_t()), tid);
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(std::move(readers));
    std::vector<analysis_tool_t *> tools;
    auto test_tool = std::unique_ptr<view_test_t>(
        no_modules
            ? reinterpret_cast<view_test_t *>(new view_nomod_test_t(drcontext, ilist))
            : reinterpret_cast<view_test_t *>(new view_test_t(drcontext, ilist)));
    tools.push_back(test_tool.get());
    mock_analyzer_t analyzer(sched_inputs, &tools[0], (int)tools.size(), skip_records,
                             num_records);
    assert(!!analyzer);
    std::stringstream capture;
    std::streambuf *prior = std::cerr.rdbuf(capture.rdbuf());
    bool success = analyzer.run();
    assert(success);
    analyzer.print_stats();
    std::string res = capture.str();
    std::cerr.rdbuf(prior);
    return res;
}

static int
get_memref_count(const std::vector<trace_entry_t> &records)
{
    test_util::mock_reader_t reader = test_util::mock_reader_t(records);
    test_util::mock_reader_t reader_end = test_util::mock_reader_t();
    int memref_count = 0;
    for (reader.init(); reader != reader_end; ++reader) {
        ++memref_count;
    }
    return memref_count;
}

bool
test_no_limit_shared(void *drcontext, instrlist_t &ilist,
                     const std::vector<trace_entry_t> &records, bool no_modules)
{
    int memref_count = get_memref_count(records);
    std::string res = run_with_analyzer(drcontext, ilist, records, /*skip_records=*/0,
                                        /*num_records=*/0, no_modules);
    if (std::count(res.begin(), res.end(), '\n') != memref_count) {
        std::cerr << "Incorrect line count\n";
        return false;
    }
    std::stringstream ss(res);
    int prefix;
    ss >> prefix;
    if (prefix != 1) {
        std::cerr << "Expect 1-based line prefixes\n";
        return false;
    }
    return true;
}

bool
test_no_limit(void *drcontext, instrlist_t &ilist,
              const std::vector<trace_entry_t> &records)
{
    return test_no_limit_shared(drcontext, ilist, records, /*no_modules=*/false);
}

bool
test_no_modules(void *drcontext, instrlist_t &ilist,
                const std::vector<trace_entry_t> &records)
{
    return test_no_limit_shared(drcontext, ilist, records, /*no_modules=*/false);
}

bool
test_num_memrefs(void *drcontext, instrlist_t &ilist,
                 const std::vector<trace_entry_t> &records, int num_records)
{
    int memref_count = get_memref_count(records);
    ASSERT(num_records < memref_count, "need more records to limit");
    std::string res =
        run_with_analyzer(drcontext, ilist, records, /*skip_records=*/0, num_records);
    if (std::count(res.begin(), res.end(), '\n') != num_records) {
        std::cerr << "Incorrect num_memrefs count: expect " << num_records
                  << " but got \n"
                  << res << "\n";
        return false;
    }
    return true;
}

bool
test_skip_memrefs(void *drcontext, instrlist_t &ilist,
                  const std::vector<trace_entry_t> &records, int skip_records,
                  int num_records)
{
    // We do a simple check on the marker count.
    // XXX: To test precisely skipping the instrs and data we'll need to spend
    // more effort here, but the initial delayed markers are the corner cases.
    int all_count = 0, marker_count = 0;
    test_util::mock_reader_t reader = test_util::mock_reader_t(records);
    test_util::mock_reader_t reader_end = test_util::mock_reader_t();
    for (reader.init(); reader != reader_end; ++reader) {
        memref_t memref = *reader;
        if (all_count++ < skip_records)
            continue;
        if (all_count - skip_records > num_records)
            break;
        if (memref.marker.type == TRACE_TYPE_MARKER)
            ++marker_count;
    }
    ASSERT(num_records + skip_records <= get_memref_count(records),
           "need more memrefs to skip");
    std::string res =
        run_with_analyzer(drcontext, ilist, records, skip_records, num_records);
    if (std::count(res.begin(), res.end(), '\n') != num_records) {
        std::cerr << "Incorrect skipped_records count: expect " << num_records
                  << " but got \n"
                  << res << "\n";
        return false;
    }
    int found_markers = 0;
    size_t pos = 0;
    while (pos != std::string::npos) {
        pos = res.find("marker", pos);
        if (pos != std::string::npos) {
            ++found_markers;
            ++pos;
        }
    }
    if (found_markers != marker_count) {
        std::cerr << "Failed to skip proper number of markers\n";
        return false;
    }
    // Unfortunately this doesn't detect an error in the internal counter.
    // We rely on the marker count check for that.
    std::stringstream ss(res);
    int prefix;
    ss >> prefix;
    if (prefix != 1 + skip_records) {
        std::cerr << "Expect to start after skip count " << skip_records << " but found "
                  << prefix << "\n"
                  << res << "\n";
        return false;
    }
    return true;
}

bool
run_limit_tests(void *drcontext)
{
    bool res = true;

    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop1 = XINST_CREATE_nop(drcontext);
    instr_t *nop2 = XINST_CREATE_nop(drcontext);
    instr_t *jcc = XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ, opnd_create_instr(nop2));
    instrlist_append(ilist, nop1);
    instrlist_append(ilist, jcc);
    instrlist_append(ilist, nop2);
    size_t offs_nop1 = 0;
    size_t offs_jcc = offs_nop1 + instr_length(drcontext, nop1);
    size_t offs_nop2 = offs_jcc + instr_length(drcontext, jcc);

    const memref_tid_t t1 = 3;
    std::vector<trace_entry_t> records = {
        test_util::make_thread(t1),
        test_util::make_pid(/*pid=*/1),
        test_util::make_marker(TRACE_MARKER_TYPE_VERSION, 3),
        test_util::make_marker(TRACE_MARKER_TYPE_FILETYPE, 0),
        test_util::make_marker(TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
        test_util::make_marker(TRACE_MARKER_TYPE_TIMESTAMP, 1001),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 2),
        test_util::make_instr(offs_nop1),
        test_util::make_memref(0x42, TRACE_TYPE_READ, 4),
        test_util::make_marker(TRACE_MARKER_TYPE_TIMESTAMP, 1002),
        test_util::make_marker(TRACE_MARKER_TYPE_CPU_ID, 3),
        test_util::make_instr(offs_jcc, TRACE_TYPE_INSTR_UNTAKEN_JUMP),
        test_util::make_instr(offs_nop2, TRACE_TYPE_INSTR),
        test_util::make_memref(0x42, TRACE_TYPE_READ, 4),
    };
    int memref_count = get_memref_count(records);

    res = test_no_limit(drcontext, *ilist, records) && res;
    for (int i = 1; i < memref_count; ++i) {
        res = test_num_memrefs(drcontext, *ilist, records, i) && res;
    }
    constexpr int num_refs = 2;
    for (int i = 1; i < memref_count - num_refs; ++i) {
        res = test_skip_memrefs(drcontext, *ilist, records, i, num_refs) && res;
    }

    // Ensure missing modules are fine.
    res = test_no_modules(drcontext, *ilist, records) && res;

    instrlist_clear_and_destroy(drcontext, ilist);
    return res;
}

/***************************************************************************
 * File reader mock.
 */

class mock_file_reader_t : public file_reader_t<std::vector<trace_entry_t>> {
public:
    mock_file_reader_t()
    {
        at_eof_ = true;
    }
    mock_file_reader_t(const std::vector<trace_entry_t> &entries)
    {
        input_file_ = entries;
        pos_ = 0;
    }
    trace_entry_t *
    read_next_entry() override
    {
        if (at_eof_)
            return nullptr;
        trace_entry_t *entry = read_queued_entry();
        if (entry != nullptr)
            return entry;
        if (pos_ >= input_file_.size()) {
            at_eof_ = true;
            return nullptr;
        }
        entry = &input_file_[pos_];
        ++pos_;
        return entry;
    }

private:
    size_t pos_;
};

std::string
run_serial_test_helper(view_t &view,
                       const std::vector<std::vector<trace_entry_t>> &entries,
                       const std::vector<memref_tid_t> &tids)

{
    class local_stream_t : public default_memtrace_stream_t {
    public:
        local_stream_t(view_t &view,
                       const std::vector<std::vector<trace_entry_t>> &entries,
                       const std::vector<memref_tid_t> &tids)
            : view_(view)
            , entries_(entries)
            , tids_(tids)
        {
        }

        std::string
        run()
        {
            view_.initialize_stream(this);
            // Capture cerr.
            std::stringstream capture;
            std::streambuf *prior = std::cerr.rdbuf(capture.rdbuf());
            // Run the tool.
            std::vector<scheduler_t::input_reader_t> readers;
            for (size_t i = 0; i < entries_.size(); i++) {
                readers.emplace_back(
                    std::unique_ptr<mock_file_reader_t>(
                        new mock_file_reader_t(entries_[i])),
                    std::unique_ptr<mock_file_reader_t>(new mock_file_reader_t()),
                    tids_[i]);
            }
            scheduler_t scheduler;
            std::vector<scheduler_t::input_workload_t> sched_inputs;
            sched_inputs.emplace_back(std::move(readers));
            if (scheduler.init(sched_inputs, 1,
                               scheduler_t::make_scheduler_serial_options()) !=
                scheduler_t::STATUS_SUCCESS)
                assert(false);
            auto *stream = scheduler.get_stream(0);
            memref_t memref;
            for (scheduler_t::stream_status_t status = stream->next_record(memref);
                 status != scheduler_t::STATUS_EOF;
                 status = stream->next_record(memref)) {
                assert(status == scheduler_t::STATUS_OK);
                ++ref_count_;
                if (type_is_instr(memref.instr.type))
                    ++instr_count_;
                if (!view_.process_memref(memref)) {
                    if (view_.get_error_string().empty())
                        break;
                    std::cout << "Hit error: " << view_.get_error_string() << "\n";
                }
            }
            // Return the result.
            std::string res = capture.str();
            std::cerr.rdbuf(prior);
            return res;
        }

        uint64_t
        get_record_ordinal() const override
        {
            return ref_count_;
        }
        uint64_t
        get_instruction_ordinal() const override
        {
            return instr_count_;
        }

    private:
        view_t &view_;
        const std::vector<std::vector<trace_entry_t>> &entries_;
        const std::vector<memref_tid_t> &tids_;
        uint64_t ref_count_ = 0;
        uint64_t instr_count_ = 0;
    };

    local_stream_t stream(view, entries, tids);
    return stream.run();
}

/***************************************************************************/

bool
run_single_thread_chunk_test(void *drcontext)
{
    const memref_tid_t t1 = 3;
    std::vector<memref_tid_t> tids = { t1 };
    std::vector<std::vector<trace_entry_t>> entries = { {
        { TRACE_TYPE_HEADER, 0, { 0x1 } },
        { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION, { 3 } },
        { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE, { 0 } },
        { TRACE_TYPE_THREAD, 0, { t1 } },
        { TRACE_TYPE_PID, 0, { t1 } },
        { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, { 64 } },
        { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT, { 2 } },
        { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 1002 } },
        { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 2 } },
        { TRACE_TYPE_INSTR, 4, { 42 } },
        { TRACE_TYPE_INSTR, 4, { 42 } },
        { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER, { 0 } },
        // We're testing that the reader hides this duplicate timestamp
        // at the start of a chunk.
        { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 1002 } },
        { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 3 } },
        { TRACE_TYPE_INSTR, 4, { 42 } },
    } };
    const char *expect = R"DELIM(           1           0:       W0.T3 <marker: version 3>
           2           0:       W0.T3 <marker: filetype 0x0>
           3           0:       W0.T3 <marker: cache line size 64>
           4           0:       W0.T3 <marker: chunk instruction count 2>
           5           0:       W0.T3 <marker: timestamp 1002>
           6           0:       W0.T3 <marker: W0.T3 on core 2>
           7           1:       W0.T3 ifetch       4 byte(s) @ 0x0000002a non-branch
           8           2:       W0.T3 ifetch       4 byte(s) @ 0x0000002a non-branch
           9           2:       W0.T3 <marker: chunk footer #0>
          10           3:       W0.T3 ifetch       4 byte(s) @ 0x0000002a non-branch
)DELIM";
    instrlist_t *ilist_unused = nullptr;
    view_nomod_test_t view(drcontext, *ilist_unused);
    std::string res = run_serial_test_helper(view, entries, tids);
    // Make 64-bit match our 32-bit expect string.
    res = std::regex_replace(res, std::regex("0x000000000000002a"), "0x0000002a");
    if (res != expect) {
        std::cerr << "Output mismatch: got |" << res << "| expected |" << expect << "|\n";
        return false;
    }
    return true;
}

bool
run_serial_chunk_test(void *drcontext)
{
    // We ensure headers are not omitted incorrectly, which they were
    // in the first implementation of the reader skipping dup headers:
    // i#5538#issuecomment-1407235283
    const memref_tid_t t1 = 3;
    const memref_tid_t t2 = 7;
    std::vector<memref_tid_t> tids = { t1, t2 };
    std::vector<std::vector<trace_entry_t>> entries = {
        {
            { TRACE_TYPE_HEADER, 0, { 0x1 } },
            { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION, { 3 } },
            { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE, { 0 } },
            { TRACE_TYPE_THREAD, 0, { t1 } },
            { TRACE_TYPE_PID, 0, { t1 } },
            { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, { 64 } },
            { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT, { 20 } },
            { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 1001 } },
            { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 2 } },
            { TRACE_TYPE_INSTR, 4, { 42 } },
            { TRACE_TYPE_INSTR, 4, { 42 } },
            { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 1003 } },
            { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 3 } },
            { TRACE_TYPE_INSTR, 4, { 42 } },
        },
        {
            { TRACE_TYPE_HEADER, 0, { 0x1 } },
            { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION, { 3 } },
            { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE, { 0 } },
            { TRACE_TYPE_THREAD, 0, { t2 } },
            { TRACE_TYPE_PID, 0, { t2 } },
            { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, { 64 } },
            { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT, { 2 } },
            { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 1002 } },
            { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 2 } },
            { TRACE_TYPE_INSTR, 4, { 42 } },
            { TRACE_TYPE_INSTR, 4, { 42 } },
            { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 1004 } },
            { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 3 } },
            { TRACE_TYPE_INSTR, 4, { 42 } },
        }
    };
    const char *expect =
        R"DELIM(           1           0:       W0.T3 <marker: version 3>
           2           0:       W0.T3 <marker: filetype 0x0>
           3           0:       W0.T3 <marker: cache line size 64>
           4           0:       W0.T3 <marker: chunk instruction count 20>
           5           0:       W0.T3 <marker: timestamp 1001>
           6           0:       W0.T3 <marker: W0.T3 on core 2>
           7           1:       W0.T3 ifetch       4 byte(s) @ 0x0000002a non-branch
           8           2:       W0.T3 ifetch       4 byte(s) @ 0x0000002a non-branch
------------------------------------------------------------
           9           2:       W0.T7 <marker: version 3>
          10           2:       W0.T7 <marker: filetype 0x0>
          11           2:       W0.T7 <marker: cache line size 64>
          12           2:       W0.T7 <marker: chunk instruction count 2>
          13           2:       W0.T7 <marker: timestamp 1002>
          14           2:       W0.T7 <marker: W0.T7 on core 2>
          15           3:       W0.T7 ifetch       4 byte(s) @ 0x0000002a non-branch
          16           4:       W0.T7 ifetch       4 byte(s) @ 0x0000002a non-branch
------------------------------------------------------------
          17           4:       W0.T3 <marker: timestamp 1003>
          18           4:       W0.T3 <marker: W0.T3 on core 3>
          19           5:       W0.T3 ifetch       4 byte(s) @ 0x0000002a non-branch
------------------------------------------------------------
          20           5:       W0.T7 <marker: timestamp 1004>
          21           5:       W0.T7 <marker: W0.T7 on core 3>
          22           6:       W0.T7 ifetch       4 byte(s) @ 0x0000002a non-branch
)DELIM";
    instrlist_t *ilist_unused = nullptr;
    view_nomod_test_t view(drcontext, *ilist_unused);
    std::string res = run_serial_test_helper(view, entries, tids);
    // Make 64-bit match our 32-bit expect string.
    res = std::regex_replace(res, std::regex("0x000000000000002a"), "0x0000002a");
    if (res != expect) {
        std::cerr << "Output mismatch: got |" << res << "| expected |" << expect << "|\n";
        return false;
    }
    return true;
}

bool
run_chunk_tests(void *drcontext)
{
    return run_single_thread_chunk_test(drcontext) && run_serial_chunk_test(drcontext);
}

/* Test view tool on a OFFLINE_FILE_TYPE_ARCH_REGDEPS trace.
 * The trace hardcoded in entries is X64, so we only test on X64 architectures here.
 */
bool
run_regdeps_test(void *drcontext)
{
#ifdef X64
    const memref_tid_t t1 = 3;
    std::vector<memref_tid_t> tids = { t1 };
    constexpr addr_t PC_mov = 0x00007f6fdd3ec360;
    constexpr addr_t PC_vpmovmskb = 0x00007f6fdc76cb35;
    constexpr addr_t PC_vmovdqu = 0x00007f6fdc76cb2d;
    constexpr addr_t PC_lock_cmpxchg = 0x00007f86ef03d107;
    constexpr addr_t PC_branch = 0x00005605ec276c4d;
    constexpr addr_t ENCODING_REGDEPS_ISA_mov = 0x0006090600010011;
    constexpr addr_t ENCODING_REGDEPS_ISA_vpmovmskb = 0x004e032100004011;
    constexpr addr_t ENCODING_REGDEPS_ISA_vmovdqu = 0x0009492100004811;
    constexpr addr_t ENCODING_REGDEPS_ISA_lock_cmpxchg_1 = 0x0402020400001931;
    constexpr addr_t ENCODING_REGDEPS_ISA_lock_cmpxchg_2 = 0x00000026;
    constexpr addr_t ENCODING_REGDEPS_ISA_branch = 0x00002200;
    std::vector<std::vector<trace_entry_t>> entries = { {
        { TRACE_TYPE_HEADER, 0, { 0x1 } },
        { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION, { 3 } },
        { TRACE_TYPE_MARKER,
          TRACE_MARKER_TYPE_FILETYPE,
          { OFFLINE_FILE_TYPE_ARCH_REGDEPS | OFFLINE_FILE_TYPE_ENCODINGS |
            OFFLINE_FILE_TYPE_SYSCALL_NUMBERS | OFFLINE_FILE_TYPE_BLOCKING_SYSCALLS } },
        { TRACE_TYPE_THREAD, 0, { t1 } },
        { TRACE_TYPE_PID, 0, { t1 } },
        { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, { 64 } },
        { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT, { 5 } },
        { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 1002 } },
        { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { 2 } },
        { TRACE_TYPE_ENCODING, 8, { ENCODING_REGDEPS_ISA_mov } },
        { TRACE_TYPE_INSTR, 3, { PC_mov } },
        { TRACE_TYPE_ENCODING, 8, { ENCODING_REGDEPS_ISA_vpmovmskb } },
        { TRACE_TYPE_INSTR, 4, { PC_vpmovmskb } },
        { TRACE_TYPE_ENCODING, 8, { ENCODING_REGDEPS_ISA_vmovdqu } },
        { TRACE_TYPE_INSTR, 4, { PC_vmovdqu } },
        { TRACE_TYPE_ENCODING, 8, { ENCODING_REGDEPS_ISA_lock_cmpxchg_1 } },
        { TRACE_TYPE_ENCODING, 4, { ENCODING_REGDEPS_ISA_lock_cmpxchg_2 } },
        { TRACE_TYPE_INSTR, 10, { PC_lock_cmpxchg } },
        { TRACE_TYPE_ENCODING, 4, { ENCODING_REGDEPS_ISA_branch } },
        { TRACE_TYPE_INSTR_TAKEN_JUMP, 2, { PC_branch } },
        { TRACE_TYPE_FOOTER, 0, { 0 } },
    } };

    /* clang-format off */
    std::string expect =
        std::string(R"DELIM(           1           0:       W0.T3 <marker: version 3>
           2           0:       W0.T3 <marker: filetype 0x20e00>
           3           0:       W0.T3 <marker: cache line size 64>
           4           0:       W0.T3 <marker: chunk instruction count 5>
           5           0:       W0.T3 <marker: timestamp 1002>
           6           0:       W0.T3 <marker: W0.T3 on core 2>
           7           1:       W0.T3 ifetch       3 byte(s) @ 0x00007f6fdd3ec360 00010011 00060906 move [8byte]       %rv4 -> %rv7
           8           2:       W0.T3 ifetch       4 byte(s) @ 0x00007f6fdc76cb35 00004011 004e0321 simd [32byte]       %rv76 -> %rv1
           9           3:       W0.T3 ifetch       4 byte(s) @ 0x00007f6fdc76cb2d 00004811 00094921 load simd [32byte]       %rv7 -> %rv71
          10           4:       W0.T3 ifetch      10 byte(s) @ 0x00007f86ef03d107 00001931 04020204 load store [4byte]       %rv0 %rv2 %rv36 -> %rv0
          10           4:       W0.T3                                             00000026
          11           5:       W0.T3 ifetch       2 byte(s) @ 0x00005605ec276c4d 00002200          branch  (taken)
)DELIM");
    /* clang-format on */

    instrlist_t *ilist_unused = nullptr;
    view_nomod_test_t view(drcontext, *ilist_unused);
    std::string res = run_serial_test_helper(view, entries, tids);
    if (res != expect) {
        std::cerr << "Output mismatch: got |" << res << "| expected |" << expect << "|\n";
        return false;
    }
#endif
    return true;
}

int
test_main(int argc, const char *argv[])
{
    void *drcontext = dr_standalone_init();
    if (run_limit_tests(drcontext) && run_chunk_tests(drcontext) &&
        run_regdeps_test(drcontext)) {
        std::cerr << "view_test passed\n";
        return 0;
    }
    std::cerr << "view_test FAILED\n";
    exit(1);
}

} // namespace drmemtrace
} // namespace dynamorio
