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

/* analysis_tool: represent a memory trace analysis tool.
 */

#ifndef _ANALYSIS_TOOL_H_
#define _ANALYSIS_TOOL_H_ 1

/**
 * @file drmemtrace/analysis_tool.h
 * @brief DrMemtrace analysis tool base class.
 */

// To support installation of headers for analysis tools into a single
// separate directory we omit common/ here and rely on -I.
#include "memref.h"
#include <string>

/**
 * The base class for a tool that analyzes a trace.  A new tool should subclass this
 * class.
 *
 * Concurrent processing of traces is supported by logically splitting a trace into
 * "shards" which are each processed sequentially.  The default shard is a traced
 * application thread, but the tool interface can support other divisions.  For tools
 * that support concurrent processing of shards and do not need to see a single
 * thread-interleaved merged trace, the interface functions with the parallel_
 * prefix should be implemented, and parallel_shard_supported() should return true.
 * parallel_shard_init() will be invoked for each shard prior to invoking
 * parallel_shard_memref() for any entry in that shard; the data structure returned
 * from parallel_shard_init() will be passed to parallel_shard_memref() for each
 * trace entry for that shard.  The concurrency model used guarantees that all
 * entries from any one shard are processed by the same single worker thread, so no
 * synchronization is needed inside the parallel_ functions.  A single worker thread
 * invokes print_results() as well.
 *
 * For serial operation, process_memref(), operates on a trace entry in a single,
 * sorted, interleaved stream of trace entries.  In the default mode of operation,
 * the #analyzer_t class iterates over the trace and calls the process_memref()
 * function of each tool.  An alternative mode is supported which exposes the
 * iterator and allows a separate control infrastructure to be built.  This
 * alternative mode does not support parallel operation at this time.
 *
 * Both parallel and serial operation can be supported by a tool, typically by having
 * process_memref() create data on a newly seen traced thread and invoking
 * parallel_shard_memref() to do its work.
 *
 * For both parallel and serial operation, the function print_results() should be
 * overridden.  It is called just once after processing all trace data and it should
 * present the results of the analysis.  For parallel operation, any desired
 * aggregation across the whole trace should occur here as well, while shard-specific
 * results can be presented in parallel_shard_exit().
 *
 */
class analysis_tool_t {
public:
    /**
     * Errors encountered during the constructor will set the success flag, which should
     * be queried via operator!.  On an error, get_error_string() provides a descriptive
     * error message.
     */
    analysis_tool_t()
        : success_(true){};
    virtual ~analysis_tool_t(){}; /**< Destructor. */
    /**
     * Tools are encouraged to perform any initialization that might fail here rather
     * than in the constructor.  On an error, this returns an error string.  On success,
     * it returns "".
     */
    virtual std::string
    initialize()
    {
        return "";
    }
    /** Returns whether the tool was created successfully. */
    virtual bool operator!()
    {
        return !success_;
    }
    /** Returns a description of the last error. */
    virtual std::string
    get_error_string()
    {
        return error_string_;
    }
    /**
     * The heart of an analysis tool, this routine operates on a single trace entry and
     * takes whatever actions the tool needs to perform its analysis.
     * If it prints, it should leave the i/o state in a default format
     * (std::dec) to support multiple tools.
     * The return value indicates whether it was successful.
     * On failure, get_error_string() returns a descriptive message.
     */
    virtual bool
    process_memref(const memref_t &memref) = 0;
    /**
     * This routine reports the results of the trace analysis.
     * It should leave the i/o state in a default format (std::dec) to support
     * multiple tools.
     * The return value indicates whether it was successful.
     * On failure, get_error_string() returns a descriptive message.
     */
    virtual bool
    print_results() = 0;

    /**
     * Returns whether this tool supports analyzing trace shards concurrently, or
     * whether it needs to see a single thread-interleaved stream of traced
     * events.
     */
    virtual bool
    parallel_shard_supported()
    {
        return false;
    }
    /**
     * Invoked once for each worker thread prior to calling any shard routine from
     * that thread.  This allows a tool to create data local to a worker, such as a
     * cache of data global to the trace that crosses shards.  This data does not
     * need any synchronization as it will only be accessed by this worker thread.
     * The \p worker_index is a unique identifier for this worker.  The return value
     * here will be passed to the invocation of parallel_shard_init() for each shard
     * upon which this worker operates.
     */
    virtual void *
    parallel_worker_init(int worker_index)
    {
        return nullptr;
    }
    /**
     * Invoked once when a worker thread is finished processing all shard data.  The
     * \p worker_data is the return value of parallel_worker_init().  A non-empty
     * string indicates an error while an empty string indicates success.
     */
    virtual std::string
    parallel_worker_exit(void *worker_data)
    {
        return "";
    }
    /**
     * Invoked once for each trace shard prior to calling parallel_shard_memref() for
     * that shard, this allows a tool to create data local to a shard.  The \p
     * shard_index is a unique identifier allowing shard data to be stored into a
     * global table if desired (typically for aggregation use in print_results()).
     * The \p worker_data is the return value of parallel_worker_init() for the
     * worker thread who will exclusively operate on this shard.  The return value
     * here will be passed to each invocation of parallel_shard_memref() for that
     * same shard.
     */
    virtual void *
    parallel_shard_init(int shard_index, void *worker_data)
    {
        return nullptr;
    }
    /**
     * Invoked once when all trace entries for a shard have been processed.  \p
     * shard_data is the value returned by parallel_shard_init() for this shard.
     * This allows a tool to clean up its thread data, or to report thread analysis
     * results.  Most tools, however, prefer to aggregate data or at least sort data,
     * and perform nothing here, doing all cleanup in print_results() by storing the
     * thread data into a table.
     * Return whether exiting was successful. On failure, parallel_shard_error()
     * returns a descriptive message.
     */
    virtual bool
    parallel_shard_exit(void *shard_data)
    {
        return true;
    }
    /**
     * The heart of an analysis tool, this routine operates on a single trace entry
     * and takes whatever actions the tool needs to perform its analysis. The \p
     * shard_data parameter is the value returned by parallel_shard_init() for this
     * shard.  Since each shard is operated upon in its entirety by the same worker
     * thread, no synchronization is needed.  The return value indicates whether this
     * function was successful. On failure, parallel_shard_error() returns a
     * descriptive message.
     */
    virtual bool
    parallel_shard_memref(void *shard_data, const memref_t &memref)
    {
        return false;
    }
    /** Returns a description of the last error for this shard. */
    virtual std::string
    parallel_shard_error(void *shard_data)
    {
        return "";
    }

protected:
    bool success_;
    std::string error_string_;
};

#endif /* _ANALYSIS_TOOL_H_ */
