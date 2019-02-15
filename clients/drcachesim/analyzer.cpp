/* **********************************************************
 * Copyright (c) 2016-2019 Google, Inc.  All rights reserved.
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
#ifdef HAS_SNAPPY
#    include "reader/snappy_file_reader.h"
#endif
#include "common/utils.h"

#ifdef HAS_ZLIB
// Even if the file is uncompressed, zlib's gzip interface is faster than
// file_reader_t's fstream in our measurements, so we always use it when
// available.
typedef compressed_file_reader_t default_file_reader_t;
#else
typedef file_reader_t<std::ifstream *> default_file_reader_t;
#endif

analyzer_t::analyzer_t()
    : success(true)
    , num_tools(0)
    , tools(NULL)
    , parallel(true)
    , worker_count(0)
{
    /* Nothing else: child class needs to initialize. */
}

#ifdef HAS_SNAPPY
static bool
ends_with(const std::string &str, const std::string &with)
{
    size_t pos = str.rfind(with);
    if (pos == std::string::npos)
        return false;
    return (pos + with.size() == str.size());
}
#endif

static std::unique_ptr<reader_t>
get_reader(const std::string &path, int verbosity)
{
    std::unique_ptr<reader_t> reader;
#ifdef HAS_SNAPPY
    if (ends_with(path, ".sz"))
        reader.reset(new snappy_file_reader_t(path, verbosity));
#endif
    if (reader == nullptr)
        reader.reset(new default_file_reader_t(path, verbosity));
    return reader;
}

bool
analyzer_t::init_file_reader(const std::string &trace_path, int verbosity_in)
{
    verbosity = verbosity_in;
    if (trace_path.empty()) {
        ERRMSG("Trace file name is empty\n");
        return false;
    }
    for (int i = 0; i < num_tools; ++i) {
        if (parallel && !tools[i]->parallel_shard_supported()) {
            parallel = false;
            break;
        }
    }
    if (parallel && directory_iterator_t::is_directory(trace_path)) {
        directory_iterator_t end;
        directory_iterator_t iter(trace_path);
        if (!iter) {
            ERRMSG("Failed to list directory %s: %s", trace_path.c_str(),
                   iter.error_string().c_str());
            return false;
        }
        for (; iter != end; ++iter) {
            const std::string fname = *iter;
            if (fname == "." || fname == "..")
                continue;
            const std::string path = trace_path + DIRSEP + fname;
            thread_data.push_back(analyzer_shard_data_t(
                static_cast<int>(thread_data.size()), get_reader(path, verbosity), path));
            VPRINT(this, 2, "Opened reader for %s\n", path.c_str());
        }
        // Like raw2trace, we use a simple round-robin static work assigment.  This
        // could be improved later with dynamic work queue for better load balancing.
        if (worker_count <= 0)
            worker_count = std::thread::hardware_concurrency();
        worker_tasks.resize(worker_count);
        int worker = 0;
        for (size_t i = 0; i < thread_data.size(); ++i) {
            VPRINT(this, 2, "Worker %d assigned trace shard %zd\n", worker, i);
            worker_tasks[worker].push_back(&thread_data[i]);
            thread_data[i].worker = worker;
            worker = (worker + 1) % worker_count;
        }
    } else {
        parallel = false;
        serial_trace_iter = get_reader(trace_path, verbosity);
    }
    // It's ok if trace_end is a different type from serial_trace_iter, they
    // will still compare true if both at EOF.
    trace_end = std::unique_ptr<default_file_reader_t>(new default_file_reader_t());
    return true;
}

analyzer_t::analyzer_t(const std::string &trace_path, analysis_tool_t **tools_in,
                       int num_tools_in, int worker_count_in)
    : success(true)
    , num_tools(num_tools_in)
    , tools(tools_in)
    , parallel(true)
    , worker_count(worker_count_in)
{
    for (int i = 0; i < num_tools; ++i) {
        if (tools[i] == NULL || !*tools[i]) {
            success = false;
            error_string = "Tool is not successfully initialized";
            if (tools[i] != NULL)
                error_string += ": " + tools[i]->get_error_string();
            return;
        }
        const std::string error = tools[i]->initialize();
        if (!error.empty()) {
            success = false;
            error_string = "Tool failed to initialize: " + error;
            return;
        }
    }
    if (!init_file_reader(trace_path))
        success = false;
}

analyzer_t::analyzer_t(const std::string &trace_path)
    : success(true)
    , num_tools(0)
    , tools(NULL)
    // This external-iterator interface does not support parallel analysis.
    , parallel(false)
    , worker_count(0)
{
    if (!init_file_reader(trace_path))
        success = false;
}

analyzer_t::~analyzer_t()
{
    // Empty.
}

// Work around clang-format bug: no newline after return type for single-char operator.
// clang-format off
bool
analyzer_t::operator!()
// clang-format on
{
    return !success;
}

std::string
analyzer_t::get_error_string()
{
    return error_string;
}

// Used only for serial iteration.
bool
analyzer_t::start_reading()
{
    if (!serial_trace_iter->init()) {
        ERRMSG("Failed to read from trace\n");
        return false;
    }
    return true;
}

void
analyzer_t::process_tasks(std::vector<analyzer_shard_data_t *> *tasks)
{
    if (tasks->empty()) {
        VPRINT(this, 1, "Worker has no tasks\n");
        return;
    }
    VPRINT(this, 1, "Worker %d assigned %zd task(s)\n", (*tasks)[0]->worker,
           tasks->size());
    std::vector<void *> worker_data(num_tools);
    for (int i = 0; i < num_tools; ++i)
        worker_data[i] = tools[i]->parallel_worker_init((*tasks)[0]->worker);
    for (analyzer_shard_data_t *tdata : *tasks) {
        VPRINT(this, 1, "Worker %d starting on trace shard %d\n", tdata->worker,
               tdata->index);
        if (!tdata->iter->init()) {
            tdata->error = "Failed to read from trace" + tdata->trace_file;
            return;
        }
        std::vector<void *> shard_data(num_tools);
        for (int i = 0; i < num_tools; ++i)
            shard_data[i] = tools[i]->parallel_shard_init(tdata->index, worker_data[i]);
        VPRINT(this, 1, "shard_data[0] is %p\n", shard_data[0]);
        for (; *tdata->iter != *trace_end; ++(*tdata->iter)) {
            for (int i = 0; i < num_tools; ++i) {
                const memref_t &memref = **tdata->iter;
                if (!tools[i]->parallel_shard_memref(shard_data[i], memref)) {
                    tdata->error = tools[i]->parallel_shard_error(shard_data[i]);
                    VPRINT(this, 1,
                           "Worker %d hit shard memref error %s on trace shard %d\n",
                           tdata->worker, tdata->error.c_str(), tdata->index);
                    return;
                }
            }
        }
        VPRINT(this, 1, "Worker %d finished trace shard %d\n", tdata->worker,
               tdata->index);
        for (int i = 0; i < num_tools; ++i) {
            if (!tools[i]->parallel_shard_exit(shard_data[i])) {
                tdata->error = tools[i]->parallel_shard_error(shard_data[i]);
                VPRINT(this, 1, "Worker %d hit shard exit error %s on trace shard %d\n",
                       tdata->worker, tdata->error.c_str(), tdata->index);
                return;
            }
        }
    }
    for (int i = 0; i < num_tools; ++i) {
        const std::string error = tools[i]->parallel_worker_exit(worker_data[i]);
        if (!error.empty()) {
            (*tasks)[0]->error = error;
            VPRINT(this, 1, "Worker %d hit worker exit error %s\n", (*tasks)[0]->worker,
                   error.c_str());
            return;
        }
    }
}

bool
analyzer_t::run()
{
    // XXX i#3286: Add a %-completed progress message by looking at the file sizes.
    if (!parallel) {
        if (!start_reading())
            return false;
        for (; *serial_trace_iter != *trace_end; ++(*serial_trace_iter)) {
            for (int i = 0; i < num_tools; ++i) {
                memref_t memref = **serial_trace_iter;
                // We short-circuit and exit on an error to avoid confusion over
                // the results and avoid wasted continued work.
                if (!tools[i]->process_memref(memref)) {
                    error_string = tools[i]->get_error_string();
                    return false;
                }
            }
        }
        return true;
    }
    if (worker_count <= 0) {
        error_string = "Invalid worker count: must be > 0";
        return false;
    }
    std::vector<std::thread> threads;
    VPRINT(this, 1, "Creating %d worker threads\n", worker_count);
    threads.reserve(worker_count);
    for (int i = 0; i < worker_count; ++i) {
        threads.emplace_back(
            std::thread(&analyzer_t::process_tasks, this, &worker_tasks[i]));
    }
    for (std::thread &thread : threads)
        thread.join();
    for (auto &tdata : thread_data) {
        if (!tdata.error.empty()) {
            error_string = tdata.error;
            return false;
        }
    }
    return true;
}

bool
analyzer_t::print_stats()
{
    for (int i = 0; i < num_tools; ++i) {
        if (!tools[i]->print_results()) {
            error_string = tools[i]->get_error_string();
            return false;
        }
        if (i + 1 < num_tools) {
            // Separate tool output.
            std::cerr << "\n=========================================================="
                         "=================\n";
        }
    }
    return true;
}

// XXX i#3287: Figure out how to support parallel operation with this external
// iterator interface.
reader_t &
analyzer_t::begin()
{
    if (!start_reading())
        return *trace_end;
    return *serial_trace_iter;
}

reader_t &
analyzer_t::end()
{
    return *trace_end;
}
