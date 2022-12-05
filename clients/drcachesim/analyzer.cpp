/* **********************************************************
 * Copyright (c) 2016-2022 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <iostream>
#include <thread>
#include "analysis_tool.h"
#include "analyzer.h"
#include "reader/file_reader.h"
#ifdef HAS_ZLIB
#    include "reader/compressed_file_reader.h"
#endif
#ifdef HAS_ZIP
#    include "reader/zipfile_file_reader.h"
#endif
#ifdef HAS_SNAPPY
#    include "reader/snappy_file_reader.h"
#endif
#include "common/utils.h"
#include "memtrace_stream.h"

#ifdef HAS_ZLIB
// Even if the file is uncompressed, zlib's gzip interface is faster than
// file_reader_t's fstream in our measurements, so we always use it when
// available.
typedef compressed_file_reader_t default_file_reader_t;
typedef compressed_record_file_reader_t default_record_file_reader_t;
#else
typedef file_reader_t<std::ifstream *> default_file_reader_t;
typedef dynamorio::drmemtrace::record_file_reader_t<std::ifstream>
    default_record_file_reader_t;
#endif

/****************************************************************
 * Specializations for analyzer_tmpl_t<reader_t>, aka analyzer_t.
 */

template <>
std::unique_ptr<reader_t>
analyzer_t::get_default_reader()
{
    return std::unique_ptr<default_file_reader_t>(new default_file_reader_t());
}

template <>
std::unique_ptr<reader_t>
analyzer_t::get_reader(const std::string &path, int verbosity)
{
#if defined(HAS_SNAPPY) || defined(HAS_ZIP)
#    ifdef HAS_SNAPPY
    if (ends_with(path, ".sz"))
        return std::unique_ptr<reader_t>(new snappy_file_reader_t(path, verbosity));
#    endif
#    ifdef HAS_ZIP
    if (ends_with(path, ".zip"))
        return std::unique_ptr<reader_t>(new zipfile_file_reader_t(path, verbosity));
#    endif
    // If path is a directory, and any file in it ends in .sz, return a snappy reader.
    if (directory_iterator_t::is_directory(path)) {
        directory_iterator_t end;
        directory_iterator_t iter(path);
        if (!iter) {
            ERRMSG("Failed to list directory %s: %s", path.c_str(),
                   iter.error_string().c_str());
            return nullptr;
        }
        for (; iter != end; ++iter) {
            const std::string fname = *iter;
            if (fname == "." || fname == ".." ||
                starts_with(fname, DRMEMTRACE_SERIAL_SCHEDULE_FILENAME) ||
                fname == DRMEMTRACE_CPU_SCHEDULE_FILENAME)
                continue;
#    ifdef HAS_SNAPPY
            if (ends_with(*iter, ".sz")) {
                return std::unique_ptr<reader_t>(
                    new snappy_file_reader_t(path, verbosity));
            }
#    endif
#    ifdef HAS_ZIP
            if (ends_with(*iter, ".zip")) {
                return std::unique_ptr<reader_t>(
                    new zipfile_file_reader_t(path, verbosity));
            }
#    endif
        }
    }
#endif
    // No snappy/zlib support, or didn't find a .sz/.zip file.
    return std::unique_ptr<reader_t>(new default_file_reader_t(path, verbosity));
}

template <>
bool
analyzer_t::serial_mode_supported()
{
    return true;
}

/******************************************************************************
 * Specializations for analyzer_tmpl_t<record_reader_t>, aka record_analyzer_t.
 */

template <>
std::unique_ptr<dynamorio::drmemtrace::record_reader_t>
record_analyzer_t::get_default_reader()
{
    return std::unique_ptr<default_record_file_reader_t>(
        new default_record_file_reader_t());
}

template <>
std::unique_ptr<dynamorio::drmemtrace::record_reader_t>
record_analyzer_t::get_reader(const std::string &path, int verbosity)
{
    // TODO i#5675: Add support for other file formats, particularly
    // .zip files.
    return std::unique_ptr<dynamorio::drmemtrace::record_reader_t>(
        new default_record_file_reader_t(path, verbosity));
}

template <>
bool
record_analyzer_t::serial_mode_supported()
{
    // TODO i#5727: Add support in record_file_reader_t to interleave
    // multiple traces and create a single trace stream.
    return false;
}

/********************************************************************
 * Other analyzer_tmpl_t routines that do not need to be specialized.
 */

template <typename RecordType, typename ReaderType>
analyzer_tmpl_t<RecordType, ReaderType>::analyzer_tmpl_t()
    : success_(true)
    , num_tools_(0)
    , tools_(NULL)
    , parallel_(true)
    , worker_count_(0)
{
    /* Nothing else: child class needs to initialize. */
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::init_file_reader(const std::string &trace_path,
                                                          int verbosity)
{
    verbosity_ = verbosity;
    if (trace_path.empty()) {
        ERRMSG("Trace file name is empty\n");
        return false;
    }
    for (int i = 0; i < num_tools_; ++i) {
        if (parallel_ && !tools_[i]->parallel_shard_supported()) {
            parallel_ = false;
            break;
        }
    }
    if (parallel_ && directory_iterator_t::is_directory(trace_path)) {
        directory_iterator_t end;
        directory_iterator_t iter(trace_path);
        if (!iter) {
            ERRMSG("Failed to list directory %s: %s", trace_path.c_str(),
                   iter.error_string().c_str());
            return false;
        }
        for (; iter != end; ++iter) {
            const std::string fname = *iter;
            if (fname == "." || fname == ".." ||
                starts_with(fname, DRMEMTRACE_SERIAL_SCHEDULE_FILENAME) ||
                fname == DRMEMTRACE_CPU_SCHEDULE_FILENAME)
                continue;
            const std::string path = trace_path + DIRSEP + fname;
            std::unique_ptr<ReaderType> reader = get_reader(path, verbosity);
            if (!reader) {
                return false;
            }
            thread_data_.push_back(analyzer_shard_data_t(
                static_cast<int>(thread_data_.size()), std::move(reader), path));
            VPRINT(this, 2, "Opened reader for %s\n", path.c_str());
        }
        // Like raw2trace, we use a simple round-robin static work assigment.  This
        // could be improved later with dynamic work queue for better load balancing.
        if (worker_count_ <= 0)
            worker_count_ = std::thread::hardware_concurrency();
        worker_tasks_.resize(worker_count_);
        int worker = 0;
        for (size_t i = 0; i < thread_data_.size(); ++i) {
            VPRINT(this, 2, "Worker %d assigned trace shard %zd\n", worker, i);
            worker_tasks_[worker].push_back(&thread_data_[i]);
            thread_data_[i].worker = worker;
            worker = (worker + 1) % worker_count_;
        }
    } else {
        parallel_ = false;
        serial_trace_iter_ = get_reader(trace_path, verbosity);
        if (!serial_trace_iter_) {
            return false;
        }
        VPRINT(this, 2, "Opened serial reader for %s\n", trace_path.c_str());
    }
    // It's ok if trace_end_ is a different type from serial_trace_iter_, they
    // will still compare true if both at EOF.
    trace_end_ = get_default_reader();
    return true;
}

template <typename RecordType, typename ReaderType>
analyzer_tmpl_t<RecordType, ReaderType>::analyzer_tmpl_t(
    const std::string &trace_path, analysis_tool_tmpl_t<RecordType> **tools,
    int num_tools, int worker_count, uint64_t skip_instrs)
    : success_(true)
    , num_tools_(num_tools)
    , tools_(tools)
    , parallel_(true)
    , worker_count_(worker_count)
    , skip_instrs_(skip_instrs)
{
    if (!init_file_reader(trace_path)) {
        success_ = false;
        error_string_ = "Failed to create reader";
        return;
    }
    for (int i = 0; i < num_tools; ++i) {
        if (tools_[i] == NULL || !*tools_[i]) {
            success_ = false;
            error_string_ = "Tool is not successfully initialized";
            if (tools_[i] != NULL)
                error_string_ += ": " + tools_[i]->get_error_string();
            return;
        }
        const std::string error = tools_[i]->initialize_stream(serial_trace_iter_.get());
        if (!error.empty()) {
            success_ = false;
            error_string_ = "Tool failed to initialize: " + error;
            return;
        }
    }
}

template <typename RecordType, typename ReaderType>
analyzer_tmpl_t<RecordType, ReaderType>::analyzer_tmpl_t(const std::string &trace_path)
    : success_(true)
    , num_tools_(0)
    , tools_(NULL)
    // This external-iterator interface does not support parallel analysis.
    , parallel_(false)
    , worker_count_(0)
{
    if (!init_file_reader(trace_path))
        success_ = false;
}

// Work around clang-format bug: no newline after return type for single-char operator.
// clang-format off
template <typename RecordType, typename ReaderType>
// clang-format on
analyzer_tmpl_t<RecordType, ReaderType>::~analyzer_tmpl_t()
{
    // Empty.
}

template <typename RecordType, typename ReaderType>
// Work around clang-format bug: no newline after return type for single-char operator.
// clang-format off
bool
analyzer_tmpl_t<RecordType,ReaderType>::operator!()
// clang-format on
{
    return !success_;
}

template <typename RecordType, typename ReaderType>
std::string
analyzer_tmpl_t<RecordType, ReaderType>::get_error_string()
{
    return error_string_;
}

// Used only for serial iteration.
template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::start_reading()
{
    if (!serial_mode_supported()) {
        ERRMSG("Serial mode not supported by this analyzer\n");
        return false;
    }
    if (!serial_trace_iter_->init()) {
        ERRMSG("Failed to read from trace\n");
        return false;
    }
    return true;
}

template <typename RecordType, typename ReaderType>
void
analyzer_tmpl_t<RecordType, ReaderType>::process_tasks(
    std::vector<analyzer_shard_data_t *> *tasks)
{
    if (tasks->empty()) {
        VPRINT(this, 1, "Worker has no tasks\n");
        return;
    }
    VPRINT(this, 1, "Worker %d assigned %zd task(s)\n", (*tasks)[0]->worker,
           tasks->size());
    std::vector<void *> worker_data(num_tools_);
    for (int i = 0; i < num_tools_; ++i)
        worker_data[i] = tools_[i]->parallel_worker_init((*tasks)[0]->worker);
    for (analyzer_shard_data_t *tdata : *tasks) {
        VPRINT(this, 1, "Worker %d starting on trace shard %d\n", tdata->worker,
               tdata->index);
        if (!tdata->iter->init()) {
            tdata->error = "Failed to read from trace: " + tdata->trace_file;
            return;
        }
        std::vector<void *> shard_data(num_tools_);
        for (int i = 0; i < num_tools_; ++i) {
            shard_data[i] = tools_[i]->parallel_shard_init_stream(
                tdata->index, worker_data[i], tdata->iter.get());
        }
        VPRINT(this, 1, "shard_data[0] is %p\n", shard_data[0]);
        if (skip_instrs_ > 0) {
            // We skip in each thread.
            // TODO i#5538: Add top-level header data to memtrace_stream_t for
            // access by tools, since we're skipping it here.  We considered
            // not skipping until we see the 1st timestamp but the stream access
            // approach has other benefits and seems cleaner.
            (*tdata->iter) = (*tdata->iter).skip_instructions(skip_instrs_);
        }
        for (; *tdata->iter != *trace_end_; ++(*tdata->iter)) {
            const RecordType &entry = **tdata->iter;
            for (int i = 0; i < num_tools_; ++i) {
                if (!tools_[i]->parallel_shard_memref(shard_data[i], entry)) {
                    tdata->error = tools_[i]->parallel_shard_error(shard_data[i]);
                    VPRINT(this, 1,
                           "Worker %d hit shard memref error %s on trace shard %d\n",
                           tdata->worker, tdata->error.c_str(), tdata->index);
                    return;
                }
            }
        }
        VPRINT(this, 1, "Worker %d finished trace shard %d\n", tdata->worker,
               tdata->index);
        for (int i = 0; i < num_tools_; ++i) {
            if (!tools_[i]->parallel_shard_exit(shard_data[i])) {
                tdata->error = tools_[i]->parallel_shard_error(shard_data[i]);
                VPRINT(this, 1, "Worker %d hit shard exit error %s on trace shard %d\n",
                       tdata->worker, tdata->error.c_str(), tdata->index);
                return;
            }
        }
    }
    for (int i = 0; i < num_tools_; ++i) {
        const std::string error = tools_[i]->parallel_worker_exit(worker_data[i]);
        if (!error.empty()) {
            (*tasks)[0]->error = error;
            VPRINT(this, 1, "Worker %d hit worker exit error %s\n", (*tasks)[0]->worker,
                   error.c_str());
            return;
        }
    }
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::run()
{
    // XXX i#3286: Add a %-completed progress message by looking at the file sizes.
    if (!parallel_) {
        if (!start_reading())
            return false;
        if (skip_instrs_ > 0) {
            // TODO i#5538: Add top-level header data to memtrace_stream_t; see above.
            (*serial_trace_iter_) = (*serial_trace_iter_).skip_instructions(skip_instrs_);
        }
        for (; *serial_trace_iter_ != *trace_end_; ++(*serial_trace_iter_)) {
            const RecordType entry = **serial_trace_iter_;
            for (int i = 0; i < num_tools_; ++i) {
                // We short-circuit and exit on an error to avoid confusion over
                // the results and avoid wasted continued work.
                if (!tools_[i]->process_memref(entry)) {
                    error_string_ = tools_[i]->get_error_string();
                    return false;
                }
            }
        }
        return true;
    }
    if (worker_count_ <= 0) {
        error_string_ = "Invalid worker count: must be > 0";
        return false;
    }
    std::vector<std::thread> threads;
    VPRINT(this, 1, "Creating %d worker threads\n", worker_count_);
    threads.reserve(worker_count_);
    for (int i = 0; i < worker_count_; ++i) {
        threads.emplace_back(
            std::thread(&analyzer_tmpl_t::process_tasks, this, &worker_tasks_[i]));
    }
    for (std::thread &thread : threads)
        thread.join();
    for (auto &tdata : thread_data_) {
        if (!tdata.error.empty()) {
            error_string_ = tdata.error;
            return false;
        }
    }
    return true;
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::print_stats()
{
    for (int i = 0; i < num_tools_; ++i) {
        // Each tool should reset i/o state, but we reset the format here just in case.
        std::cerr << std::dec;
        if (!tools_[i]->print_results()) {
            error_string_ = tools_[i]->get_error_string();
            return false;
        }
        if (i + 1 < num_tools_) {
            // Separate tool output.
            std::cerr << "\n=========================================================="
                         "=================\n";
        }
    }
    return true;
}

// XXX i#3287: Figure out how to support parallel operation with this external
// iterator interface.
template <typename RecordType, typename ReaderType>
ReaderType &
analyzer_tmpl_t<RecordType, ReaderType>::begin()
{
    if (!start_reading())
        return *trace_end_;
    return *serial_trace_iter_;
}

template <typename RecordType, typename ReaderType>
ReaderType &
analyzer_tmpl_t<RecordType, ReaderType>::end()
{
    return *trace_end_;
}

template class analyzer_tmpl_t<memref_t, reader_t>;
template class analyzer_tmpl_t<trace_entry_t, dynamorio::drmemtrace::record_reader_t>;
