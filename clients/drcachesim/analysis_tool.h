/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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
#include "memtrace_stream.h"
#include "trace_entry.h"
#include <string>
#include <vector>

namespace dynamorio {
namespace drmemtrace {

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
 * parallel_shard_init_stream() will be invoked for each shard prior to invoking
 * parallel_shard_memref() for any entry in that shard; the data structure returned
 * from parallel_shard_init_stream() will be passed to parallel_shard_memref() for each
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
 * RecordType is the type of entry that is analyzed by the tool. Currently, we support
 * #memref_t and #trace_entry_t. analysis_tool_tmpl_t<#memref_t> is the primary type of
 * analysis tool and is used for most purposes. analysis_tool_tmpl_t≤#trace_entry_t> is
 * used in special cases where an offline trace needs to be observed exactly as stored on
 * disk, without hiding any internal entries.
 *
 * Analysis tools can subclass either of the specialized analysis_tool_tmpl_t<#memref_t>
 * or analysis_tool_tmpl_t<#trace_entry_t>, or analysis_tool_tmpl_t itself, as required.
 */
template <typename RecordType> class analysis_tool_tmpl_t {
public:
    /**
     * Errors encountered during the constructor will set the success flag, which should
     * be queried via operator!.  On an error, get_error_string() provides a descriptive
     * error message.
     */
    analysis_tool_tmpl_t()
        : success_(true) {};
    virtual ~analysis_tool_tmpl_t() {}; /**< Destructor. */
    /**
     * \deprecated The initialize_stream() function is called by the analyzer; this
     * function is only called if the default implementation of initialize_stream() is
     * left in place and it calls this version.  On an error, this returns an error
     * string.  On success, it returns "".
     */
    virtual std::string
    initialize()
    {
        return "";
    }
    /**
     * Tools are encouraged to perform any initialization that might fail here rather
     * than in the constructor.  The \p serial_stream interface allows tools to query
     * details of the underlying trace during serial operation; it is nullptr for
     * parallel operation (a per-shard version is passed to parallel_shard_init_stream()).
     * On an error, this returns an error string.  On success, it returns "".
     */
    virtual std::string
    initialize_stream(memtrace_stream_t *serial_stream)
    {
        return initialize();
    }
    /** Returns whether the tool was created successfully. */
    virtual bool
    operator!()
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
    process_memref(const RecordType &entry) = 0;
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
     * Struct that stores details of a tool's state snapshot at an interval. This is
     * useful for computing and combining interval results. Tools should inherit from
     * this struct to define their own state snapshot structs. Tools do not need to
     * supply any values to construct this base struct; they can simply use the
     * default constructor. The members of this base class will be set by the
     * framework automatically.
     */
    struct interval_state_snapshot_t {
        // This constructor is only for convenience in unit tests. The tool does not
        // need to provide these values, and can simply use the default constructor
        // below.
        interval_state_snapshot_t(int64_t shard_id, uint64_t interval_id,
                                  uint64_t interval_end_timestamp,
                                  uint64_t instr_count_cumulative,
                                  uint64_t instr_count_delta)
            : shard_id(shard_id)
            , interval_id(interval_id)
            , interval_end_timestamp(interval_end_timestamp)
            , instr_count_cumulative(instr_count_cumulative)
            , instr_count_delta(instr_count_delta)
        {
        }
        interval_state_snapshot_t()
        {
        }

        // The following fields are set automatically by the analyzer framework after
        // the tool returns the interval_state_snapshot_t* in the
        // generate_*interval_snapshot APIs. So they'll be available to the tool in
        // the combine_interval_snapshots and print_interval_results APIs.

        // Identifier for the shard to which this interval belongs. Currently, shards
        // map only to threads, so this is the thread id. Set to WHOLE_TRACE_SHARD_ID
        // for the whole trace interval snapshots.
        int64_t shard_id = 0;
        uint64_t interval_id = 0;
        // Stores the timestamp (exclusive) when the above interval ends. Note
        // that this is not the last timestamp actually seen in the trace interval,
        // but simply the abstract boundary of the interval. This will be aligned
        // to the specified -interval_microseconds.
        uint64_t interval_end_timestamp = 0;

        // Count of instructions: cumulative till this interval, and the incremental
        // delta in this interval vs the previous one. May be useful for tools to
        // compute PKI (per kilo instruction) metrics; obviates the need for each
        // tool to duplicate this.
        uint64_t instr_count_cumulative = 0;
        uint64_t instr_count_delta = 0;

        static constexpr int64_t WHOLE_TRACE_SHARD_ID = -1;

        virtual ~interval_state_snapshot_t() = default;
    };
    /**
     * Notifies the analysis tool that the given trace \p interval_id has ended so
     * that it can generate a snapshot of its internal state in a struct derived
     * from \p interval_state_snapshot_t, and return a pointer to it. The returned
     * pointer will be provided to the tool in later combine_interval_snapshots()
     * and print_interval_result() calls.
     *
     * \p interval_id is a positive ordinal of the trace interval that just ended.
     * Trace intervals have a length equal to the \p -interval_microseconds specified
     * to the framework. Trace intervals are measured using the value of the
     * #TRACE_MARKER_TYPE_TIMESTAMP markers. The provided \p interval_id
     * values will be monotonically increasing but may not be continuous,
     * i.e. the tool may not see some \p interval_id if the trace did not have
     * any activity in that interval.
     *
     * The returned \p interval_state_snapshot_t* will be passed to the
     * combine_interval_snapshots() API which is invoked by the framework to merge
     * multiple \p interval_state_snapshot_t from different shards in the parallel
     * mode of the analyzer.
     *
     * Finally, the print_interval_result() API is invoked with a list of
     * \p interval_state_snapshot_t* representing interval snapshots for the
     * whole trace.
     *
     * The tool must not de-allocate the state snapshot until
     * release_interval_snapshot() is invoked by the framework.
     *
     * An example use case of this API is to create a time series of some output
     * metric over the whole trace.
     */
    virtual interval_state_snapshot_t *
    generate_interval_snapshot(uint64_t interval_id)
    {
        return nullptr;
    }
    /**
     * Invoked by the framework to combine the shard-local \p interval_state_snapshot_t
     * objects pointed at by \p latest_shard_snapshots, to create the combined
     * \p interval_state_snapshot_t for the whole-trace interval that ends at
     * \p interval_end_timestamp. This is useful in the parallel mode of the analyzer,
     * where each trace shard is processed in parallel. In this mode, the framework
     * creates the \p interval_state_snapshot_t for each whole-trace interval, one by one.
     * For each interval, it invokes combine_interval_snapshots() with the latest seen
     * interval snapshots from each shard (in \p latest_shard_snapshots) as of the
     * interval ending at \p interval_end_timestamp. \p latest_shard_snapshots will
     * contain at least one non-null element, and at least one of them will have its \p
     * interval_state_snapshot_t::interval_end_timestamp set to \p interval_end_timestamp.
     * Tools must not return a nullptr. Tools must not modify the provided snapshots.
     *
     * \p interval_end_timestamp helps the tool to determine which of the provided
     * \p latest_shard_snapshots are relevant to it.
     * - for cumulative metrics (e.g. total instructions till an interval), the tool
     *   may want to combine latest interval snapshots from all shards to get the accurate
     *   cumulative count.
     * - for delta metrics (e.g. incremental instructions in an interval), the tool may
     *   want to combine interval snapshots from only the shards that were observed in the
     *   current interval (with \p latest_shard_snapshots [i]->interval_end_timestamp ==
     *   \p interval_end_timestamp)
     * - or if the tool mixes cumulative and delta metrics: some field-specific logic that
     *   combines the above two strategies.
     */
    virtual interval_state_snapshot_t *
    combine_interval_snapshots(
        const std::vector<const interval_state_snapshot_t *> latest_shard_snapshots,
        uint64_t interval_end_timestamp)
    {
        return nullptr;
    }
    /**
     * Prints the interval results for the given series of interval state snapshots in
     * \p interval_snapshots.
     *
     * This is currently invoked with the list of whole-trace interval snapshots (for
     * the parallel mode, these are the snapshots created by merging the shard-local
     * snapshots).
     *
     * The framework should be able to invoke this multiple times, possibly with a
     * different list of interval snapshots. So it should avoid free-ing memory or
     * changing global state. This is to keep open the possibility of the framework
     * printing interval results for each shard separately in future.
     */
    virtual bool
    print_interval_results(
        const std::vector<interval_state_snapshot_t *> &interval_snapshots)
    {
        return true;
    }
    /**
     * Notifies the tool that the \p interval_state_snapshot_t object pointed to
     * by \p interval_snapshot is no longer needed by the framework. The tool may
     * de-allocate it right away or later, as it needs. Returns whether it was
     * successful.
     */
    virtual bool
    release_interval_snapshot(interval_state_snapshot_t *interval_snapshot)
    {
        return true;
    }
    /**
     * Returns whether this tool supports analyzing trace shards concurrently, or
     * whether it needs to see a single thread-interleaved stream of traced
     * events.  This may be called prior to initialize().
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
     * here will be passed to the invocation of parallel_shard_init_stream() for each
     * shard upon which this worker operates.
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
     * \deprecated The parallel_shard_init_stream() is what is called by the analyzer;
     * this function is only called if the default implementation of
     * parallel_shard_init_stream() is left in place and it calls this version.
     */
    virtual void *
    parallel_shard_init(int shard_index, void *worker_data)
    {
        return nullptr;
    }
    /**
     * Invoked once for each trace shard prior to calling parallel_shard_memref() for
     * that shard, this allows a tool to create data local to a shard.  The \p
     * shard_index is a unique identifier allowing shard data to be stored into a global
     * table if desired (typically for aggregation use in print_results()).  The \p
     * worker_data is the return value of parallel_worker_init() for the worker thread
     * who will exclusively operate on this shard.  The \p shard_stream allows tools to
     * query details of the underlying trace shard during parallel operation; it is
     * valid only until parallel_shard_exit() is called.  The return value here will be
     * passed to each invocation of parallel_shard_memref() for that same shard.
     */
    virtual void *
    parallel_shard_init_stream(int shard_index, void *worker_data,
                               memtrace_stream_t *shard_stream)
    {
        return parallel_shard_init(shard_index, worker_data);
    }
    /**
     * Invoked once when all trace entries for a shard have been processed.  \p
     * shard_data is the value returned by parallel_shard_init_stream() for this shard.
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
     * shard_data parameter is the value returned by parallel_shard_init_stream() for this
     * shard.  Since each shard is operated upon in its entirety by the same worker
     * thread, no synchronization is needed.  The return value indicates whether this
     * function was successful. On failure, parallel_shard_error() returns a
     * descriptive message.
     */
    virtual bool
    parallel_shard_memref(void *shard_data, const RecordType &entry)
    {
        return false;
    }
    /** Returns a description of the last error for this shard. */
    virtual std::string
    parallel_shard_error(void *shard_data)
    {
        return "";
    }
    /**
     * Notifies the analysis tool that the given trace \p interval_id in the shard
     * represented by the given \p shard_data has ended, so that it can generate a
     * snapshot of its internal state in a struct derived from \p
     * interval_state_snapshot_t, and return a pointer to it. The returned pointer will
     * be provided to the tool in later combine_interval_snapshots() and
     * print_interval_result() calls.
     *
     * Note that the provided \p interval_id is local to the shard that is
     * represented by the given \p shard_data, and not the whole-trace interval. The
     * framework will automatically create an \p interval_state_snapshot_t for each
     * whole-trace interval by using combine_interval_snapshots() to combine one or more
     * shard-local \p interval_state_snapshot_t corresponding to that whole-trace
     * interval.
     *
     * \p interval_id is a positive ordinal of the trace interval that just ended.
     * Trace intervals have a length equal to the \p -interval_microseconds specified
     * to the framework. Trace intervals are measured using the value of the
     * #TRACE_MARKER_TYPE_TIMESTAMP markers. The provided \p interval_id
     * values will be monotonically increasing but may not be continuous,
     * i.e. the tool may not see some \p interval_id if the trace shard did not
     * have any activity in that interval.
     *
     * The returned \p interval_state_snapshot_t* will be passed to the
     * combine_interval_snapshot() API which is invoked by the framework to merge
     * multiple \p interval_state_snapshot_t from different shards in the parallel
     * mode of the analyzer.
     *
     * Finally, the print_interval_result() API is invoked with a list of
     * \p interval_state_snapshot_t* representing interval snapshots for the
     * whole trace. In the parallel mode of the analyzer, this list is computed by
     * combining the shard-local \p interval_state_snapshot_t using the tool's
     * combine_interval_snapshot() API.
     *
     * The tool must not de-allocate the state snapshot until
     * release_interval_snapshot() is invoked by the framework.
     *
     * An example use case of this API is to create a time series of some output
     * metric over the whole trace.
     */
    virtual interval_state_snapshot_t *
    generate_shard_interval_snapshot(void *shard_data, uint64_t interval_id)
    {
        return nullptr;
    }

protected:
    bool success_;
    std::string error_string_;
};

/** See #dynamorio::drmemtrace::analysis_tool_tmpl_t. */
typedef analysis_tool_tmpl_t<memref_t> analysis_tool_t;

/** See #dynamorio::drmemtrace::analysis_tool_tmpl_t. */
typedef analysis_tool_tmpl_t<trace_entry_t> record_analysis_tool_t;

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _ANALYSIS_TOOL_H_ */
