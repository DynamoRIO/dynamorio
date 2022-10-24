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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <fstream>
#include <thread>
#include "directory_iterator.h"
#include "reader/trace_entry_file_reader.h"
#include "trace_entry.h"
#ifdef HAS_ZLIB
#    include "reader/compressed_file_reader.h"
#    include "common/gzip_ostream.h"
#endif
#include "trace_filter.h"

#ifdef DEBUG
#    define VPRINT(reader, level, ...)                            \
        do {                                                      \
            if ((reader)->verbosity_ >= (level)) {                \
                fprintf(stderr, "%s ", (reader)->output_prefix_); \
                fprintf(stderr, __VA_ARGS__);                     \
            }                                                     \
        } while (0)
#    define UNUSED(x) ((void)(x))
#else
#    define VPRINT(reader, level, ...) /* nothing */
#    define UNUSED                     /* nothing */
#endif

typedef trace_entry_file_reader_t<std::ifstream> default_trace_entry_file_reader_t;

trace_filter_t::trace_filter_t(const std::string &trace_dir,
                               const std::string &output_dir, int worker_count,
                               int verbosity)
    : verbosity_(verbosity)
    , worker_count_(worker_count)
    , trace_dir_(trace_dir)
    , output_dir_(output_dir)
{
    if (!init_file_reader_writer(trace_dir, output_dir, verbosity))
        error_string_ = "Could not initialize trace reader and writer";
    UNUSED(verbosity_);
    UNUSED(output_prefix_);
}

#if defined(HAS_ZLIB)
static bool
ends_with(const std::string &str, const std::string &with)
{
    size_t pos = str.rfind(with);
    if (pos == std::string::npos)
        return false;
    return (pos + with.size() == str.size());
}
#endif

std::unique_ptr<trace_entry_reader_t>
trace_filter_t::get_reader(const std::string &path, int verbosity)
{
#ifdef HAS_ZLIB
    if (ends_with(path, trace_filter_t::gzip_suffix)) {
        VPRINT(this, 3, "Using the gzip reader\n");
        input_file_format = FILE_FORMAT_GZIP;
        return std::unique_ptr<trace_entry_reader_t>(
            new compressed_trace_entry_file_reader_t(path, verbosity));
    }
#endif
    VPRINT(this, 3, "Using the default reader\n");
    input_file_format = FILE_FORMAT_UNKNOWN;
    return std::unique_ptr<trace_entry_reader_t>(
        new default_trace_entry_file_reader_t(path, verbosity));
}

std::unique_ptr<std::ostream>
trace_filter_t::get_writer(const std::string &path, int verbosity)
{
    std::unique_ptr<std::ostream> ofile;
#ifdef HAS_ZLIB
    if (input_file_format == FILE_FORMAT_GZIP) {
        VPRINT(this, 3, "Using the gzip writer\n");
        return std::unique_ptr<std::ostream>(new gzip_ostream_t(path));
    }
#endif
    assert(input_file_format == FILE_FORMAT_UNKNOWN);
    VPRINT(this, 3, "Using the default writer\n");
    return std::unique_ptr<std::ostream>(new std::ofstream(path, std::ofstream::binary));
}

bool
trace_filter_t::init_file_reader_writer(const std::string &trace_dir,
                                        const std::string &output_dir, int verbosity)
{
    if (trace_dir.empty() || output_dir.empty()) {
        ERRMSG("Trace dir or output dir name is empty\n");
        return false;
    }
    if (!directory_iterator_t::is_directory(trace_dir) ||
        !directory_iterator_t::is_directory(output_dir)) {
        ERRMSG("Trace dir or output dir is not a directory\n");
        return false;
    }
    directory_iterator_t end;
    directory_iterator_t iter(trace_dir);
    if (!iter) {
        ERRMSG("Failed to list directory %s: %s", trace_dir.c_str(),
               iter.error_string().c_str());
        return false;
    }
    for (; iter != end; ++iter) {
        const std::string fname = *iter;
        if (fname == "." || fname == "..")
            continue;
        const std::string trace_path = trace_dir + DIRSEP + fname;
        const std::string output_path = output_dir + DIRSEP + fname;
        std::unique_ptr<trace_entry_reader_t> reader = get_reader(trace_path, verbosity);
        if (!reader) {
            error_string_ = "Could not get a reader for " + trace_path;
            return false;
        }

        std::unique_ptr<std::ostream> writer = get_writer(output_path, verbosity);
        if (!writer) {
            error_string_ = "Could not get a writer for " + output_path;
            return false;
        }
        VPRINT(this, 2, "Opened output file %s\n", output_path.c_str());

        thread_data_.push_back(shard_data_t(static_cast<int>(thread_data_.size()),
                                            std::move(reader), std::move(writer),
                                            trace_path, output_path));
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
    trace_end_ = std::unique_ptr<default_trace_entry_file_reader_t>(
        new default_trace_entry_file_reader_t());
    return true;
}

void
trace_filter_t::process_tasks(std::vector<shard_data_t *> *tasks)
{
    if (tasks->empty()) {
        VPRINT(this, 1, "Worker has no tasks\n");
        return;
    }
    VPRINT(this, 1, "Worker %d assigned %zd task(s)\n", (*tasks)[0]->worker,
           tasks->size());
    for (shard_data_t *tdata : *tasks) {
        VPRINT(this, 1, "Worker %d starting on trace shard %d\n", tdata->worker,
               tdata->index);
        if (!tdata->iter->init()) {
            tdata->error = "Failed to read from trace" + tdata->trace_file;
            return;
        }
        for (; *tdata->iter != *trace_end_; ++(*tdata->iter)) {
            const trace_entry_t &entry = **tdata->iter;
            // TODO i#5675: Filter entries here.
            if (!tdata->writer->write((char *)&entry, sizeof(entry))) {
                tdata->error = "Failed to write to output file " + tdata->output_file;
                return;
            }
        }
        VPRINT(this, 1, "Worker %d finished trace shard %d\n", tdata->worker,
               tdata->index);
    }
}

bool
trace_filter_t::run()
{
    if (error_string_ != "")
        return false;
    // XXX i#3286: Add a %-completed progress message by looking at the file sizes.
    if (worker_count_ <= 0) {
        error_string_ = "Invalid worker count: must be > 0";
        return false;
    }
    std::vector<std::thread> threads;
    VPRINT(this, 1, "Creating %d worker threads\n", worker_count_);
    threads.reserve(worker_count_);
    for (int i = 0; i < worker_count_; ++i) {
        threads.emplace_back(
            std::thread(&trace_filter_t::process_tasks, this, &worker_tasks_[i]));
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
