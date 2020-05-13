/* **********************************************************
 * Copyright (c) 2016-2020 Google, Inc.  All rights reserved.
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

/* analyzer: represent a memory trace analysis tool that operates only
 * on a file.  We separate this from analyzer_multi, which can operate online
 * or on a raw trace file, to avoid needing to link in DR itself.
 */

#ifndef _ANALYZER_H_
#define _ANALYZER_H_ 1

/**
 * @file drmemtrace/analyzer.h
 * @brief DrMemtrace top-level trace analysis driver.
 */

#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include "analysis_tool.h"
#include "reader.h"

/**
 * An analyzer is the top-level driver of a set of trace analysis tools.
 * It supports two different modes of operation: either it iterates over the
 * trace and calls the process_memref() routine of each tool, or it exposes
 * an iteration interface to external control code.
 */
class analyzer_t {
public:
    /**
     * Usage: errors encountered during a constructor will set a flag that should
     * be queried via operator!().  If operator!() returns true, get_error_string()
     * can be used to try to obtain more information.
     */
    analyzer_t();
    virtual ~analyzer_t(); /**< Destructor. */
    /** Returns whether the analyzer was created successfully. */
    virtual bool operator!();
    /** Returns further information on an error in initializing the analyzer. */
    virtual std::string
    get_error_string();

    /**
     * We have two usage models: one where there are multiple tools and the
     * trace iteration is performed by analyzer_t and supports parallel trace
     * analysis, and another where a single tool controls the iteration.
     *
     * The default, simpler, multiple-tool-supporting model uses this constructor.
     * The analyzer will reference the tools array passed in during its lifetime:
     * it does not make a copy.
     * The user must free them afterward.
     * The analyzer calls the initialize() function on each tool before use.
     */
    analyzer_t(const std::string &trace_path, analysis_tool_t **tools, int num_tools,
               int worker_count = 0);
    /** Launches the analysis process. */
    virtual bool
    run();
    /** Presents the results of the analysis. */
    virtual bool
    print_stats();

    /**
     * The alternate usage model exposes the iterator to a single tool.
     * This model does not currently support parallel shard analysis.
     */
    analyzer_t(const std::string &trace_path);
    /**
     * As the iterator is more heavyweight than regular container iterators
     * we hold it internally and return a reference.  Copying will fail to compile
     * as reader_t is virtual, reminding the caller of begin() to use a reference.
     * This usage model supports only a single user of the iterator: the
     * multi-tool model above should be used if multiple tools are involved.
     */
    virtual reader_t &
    begin();
    virtual reader_t &
    end(); /** End iterator for the external-iterator usage model. */

protected:
    // Data for one trace shard.  Our concurrency model has each shard
    // analyzed by a single worker thread, eliminating the need for locks.
    struct analyzer_shard_data_t {
        analyzer_shard_data_t(int index, std::unique_ptr<reader_t> iter,
                              const std::string &trace_file)
            : index(index)
            , worker(0)
            , iter(std::move(iter))
            , trace_file(trace_file)
        {
        }
        analyzer_shard_data_t(analyzer_shard_data_t &&src)
        {
            index = src.index;
            worker = src.worker;
            iter = std::move(src.iter);
            trace_file = std::move(src.trace_file);
            error = std::move(src.error);
        }

        int index;
        int worker;
        std::unique_ptr<reader_t> iter;
        std::string trace_file;
        std::string error;

    private:
        analyzer_shard_data_t(const analyzer_shard_data_t &) = delete;
        analyzer_shard_data_t &
        operator=(const analyzer_shard_data_t &) = delete;
    };

    bool
    init_file_reader(const std::string &trace_path, int verbosity = 0);

    // This finalizes the trace_iter setup.  It can block and is meant to be
    // called at the top of run() or begin().
    bool
    start_reading();

    void
    process_tasks(std::vector<analyzer_shard_data_t *> *tasks);

    bool success_;
    std::string error_string_;
    std::vector<analyzer_shard_data_t> thread_data_;
    std::unique_ptr<reader_t> serial_trace_iter_;
    std::unique_ptr<reader_t> trace_end_;
    int num_tools_;
    analysis_tool_t **tools_;
    bool parallel_;
    int worker_count_;
    std::vector<std::vector<analyzer_shard_data_t *>> worker_tasks_;
    int verbosity_ = 0;
    const char *output_prefix_ = "[analyzer]";
};

#endif /* _ANALYZER_H_ */
