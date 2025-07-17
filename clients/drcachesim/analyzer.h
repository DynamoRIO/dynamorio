/* **********************************************************
 * Copyright (c) 2016-2025 Google, Inc.  All rights reserved.
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

#include <stdint.h>

#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "analysis_tool.h"
#include "memref.h"
#include "noise_generator.h"
#include "reader.h"
#include "record_file_reader.h"
#include "scheduler.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

/**
 * An analyzer is the top-level driver of a set of trace analysis tools.
 * It supports two different modes of operation: either it iterates over the
 * trace and calls the process_memref() routine of each tool, or it exposes
 * an iteration interface to external control code.
 *
 * RecordType is the type of entry to be analyzed: #dynamorio::drmemtrace::memref_t or
 * #dynamorio::drmemtrace::trace_entry_t. ReaderType is the reader that allows reading
 * entries of type T: #dynamorio::drmemtrace::reader_t or
 * #dynamorio::drmemtrace::record_reader_t respectively.
 *
 * #dynamorio::drmemtrace::analyzer_tmpl_t<#dynamorio::drmemtrace::memref_t,
 * #dynamorio::drmemtrace::reader_t> is the primary type of analyzer, which is used for
 * most purposes. It uses tools of type
 * #dynamorio::drmemtrace::analysis_tool_tmpl_t<#dynamorio::drmemtrace::memref_t>. This
 * analyzer provides various features to support trace analysis, e.g. processing the
 * instruction encoding entries and making it available to the tool inside
 * #dynamorio::drmemtrace::memref_t.
 *
 * #dynamorio::drmemtrace::analyzer_tmpl_t<#dynamorio::drmemtrace::trace_entry_t,
 * #dynamorio::drmemtrace::record_reader_t> is used in special cases where an offline
 * trace needs to be observed exactly as stored on disk, without hiding any internal
 * entries. It uses tools of type
 * #dynamorio::drmemtrace::analysis_tool_tmpl_t<#dynamorio::drmemtrace::trace_entry_t>.
 *
 * TODO i#5727: When we convert #dynamorio::drmemtrace::reader_t into a template on
 * RecordType, we can remove the second template parameter to
 * #dynamorio::drmemtrace::analyzer_tmpl_t, and simply use reader_tmpl_t<RecordType>
 * instead.
 */
template <typename RecordType, typename ReaderType> class analyzer_tmpl_t {
public:
    /**
     * Usage: errors encountered during a constructor will set a flag that should
     * be queried via operator!().  If operator!() returns true, get_error_string()
     * can be used to try to obtain more information.
     */
    analyzer_tmpl_t();
    virtual ~analyzer_tmpl_t(); /**< Destructor. */
    /** Returns whether the analyzer was created successfully. */
    virtual bool
    operator!();
    /** Returns further information on an error in initializing the analyzer. */
    virtual std::string
    get_error_string();

    /**
     * The analyzer usage model supports multiple tools, with the trace iteration
     * performed by analyzer_t.  It supports parallel trace analysis.  The analyzer will
     * reference the tools array passed in during its lifetime: it does not make a copy.
     * The user must free them afterward.  The analyzer calls the initialize_stream()
     * function on each tool before use.
     */
    analyzer_tmpl_t(const std::string &trace_path,
                    analysis_tool_tmpl_t<RecordType> **tools, int num_tools,
                    int worker_count = 0, uint64_t skip_instrs = 0,
                    uint64_t interval_microseconds = 0, uint64_t interval_instr_count = 0,
                    int verbosity = 0);
    /** Launches the analysis process. */
    virtual bool
    run();
    /** Presents the results of the analysis. */
    virtual bool
    print_stats();

protected:
    typedef scheduler_tmpl_t<RecordType, ReaderType> sched_type_t;

    // Tool data for one shard.
    struct analyzer_tool_shard_data_t {
        analyzer_tool_shard_data_t()
            : shard_data(nullptr)
        {
        }
        // Provide a move constructor for use with std::vector.
        analyzer_tool_shard_data_t(analyzer_tool_shard_data_t &&src)
        {
            shard_data = src.shard_data;
            interval_snapshot_data = std::move(src.interval_snapshot_data);
        }

        void *shard_data;
        // Stores the interval state snapshots generated by this tool for this shard
        // in the same order as they are generated.
        std::vector<
            typename analysis_tool_tmpl_t<RecordType>::interval_state_snapshot_t *>
            interval_snapshot_data;

    private:
        // Delete copy constructor and assignment operator to avoid overhead of
        // copying, e.g. by std::vector.
        analyzer_tool_shard_data_t(const analyzer_tool_shard_data_t &) = delete;
        analyzer_tool_shard_data_t &
        operator=(const analyzer_tool_shard_data_t &) = delete;
    };

    // Data for one trace shard.
    struct analyzer_shard_data_t {
        analyzer_shard_data_t()
            : cur_interval_index(0)
            , cur_interval_init_instr_count(0)
            , shard_id(0)
        {
        }

        uint64_t cur_interval_index;
        // Cumulative instr count as it was just before the start of the current
        // interval.
        uint64_t cur_interval_init_instr_count;
        // Identifier for the shard (thread or core id).
        int64_t shard_id;
        // Ordinal for the shard.
        int shard_index = 0;
        std::vector<analyzer_tool_shard_data_t> tool_data;
        bool exited = false;

    private:
        // Delete copy constructor and assignment operator to avoid overhead of
        // copying. We can define a move constructor in future if needed.
        analyzer_shard_data_t(const analyzer_shard_data_t &) = delete;
        analyzer_shard_data_t &
        operator=(const analyzer_shard_data_t &) = delete;
    };

    // Data for one worker thread.  Our concurrency model has each input shard
    // analyzed by a single worker thread, eliminating the need for locks.
    struct analyzer_worker_data_t {
        analyzer_worker_data_t(int index, typename sched_type_t::stream_t *stream)
            : index(index)
            , stream(stream)
        {
        }
        // Provide a move constructor for use with std::vector.
        analyzer_worker_data_t(analyzer_worker_data_t &&src)
        {
            index = src.index;
            stream = src.stream;
            shard_data = std::move(src.shard_data);
            error = std::move(src.error);
        }

        int index;
        typename scheduler_tmpl_t<RecordType, ReaderType>::stream_t *stream;
        std::string error;
        std::unordered_map<int, analyzer_shard_data_t> shard_data;

    private:
        // Delete copy constructor and assignment operator to avoid overhead of
        // copying, e.g. by std::vector.
        analyzer_worker_data_t(const analyzer_worker_data_t &) = delete;
        analyzer_worker_data_t &
        operator=(const analyzer_worker_data_t &) = delete;
    };

    // See comment on init_scheduler_common() for some noteworthy details.
    bool
    init_scheduler(const std::vector<std::string> &trace_paths,
                   // To include all threads/shards, use empty sets.
                   const std::set<memref_tid_t> &only_threads,
                   const std::set<int> &only_shards, int output_limit, int verbosity,
                   typename sched_type_t::scheduler_options_t options);

    // See comment on init_scheduler_common() for some noteworthy details.
    bool
    init_scheduler(std::unique_ptr<ReaderType> reader,
                   std::unique_ptr<ReaderType> reader_end, int verbosity,
                   typename sched_type_t::scheduler_options_t options);

    // For core-sharded, worker_count_ must be set prior to calling this; for parallel
    // mode if it is not set it will be set to the underlying core count.
    // For core-sharded, all of "options" is used; otherwise, the
    // read_inputs_in_init, replay_as_traced_istream, and kernel_syscall_trace_path
    // fields are preserved.
    bool
    init_scheduler_common(std::vector<typename sched_type_t::input_workload_t> &workloads,
                          typename sched_type_t::scheduler_options_t options);

    // Used for std::thread so we need an rvalue (so no &worker).
    void
    process_tasks(analyzer_worker_data_t *worker);

    void
    process_serial(analyzer_worker_data_t &worker);

    // Helper for process_tasks().
    bool
    process_tasks_internal(analyzer_worker_data_t *worker);

    // Helper for process_tasks() which calls parallel_shard_exit() in each tool.
    // Returns false if there was an error and the caller should return early.
    bool
    process_shard_exit(analyzer_worker_data_t *worker, int shard_index,
                       bool do_process_final_interval = true);

    bool
    record_has_tid(RecordType record, memref_tid_t &tid);

    bool
    record_is_thread_final(RecordType record);

    bool
    record_is_timestamp(const RecordType &record);

    bool
    record_is_instr(const RecordType &record);

    RecordType
    create_wait_marker();

    RecordType
    create_idle_marker();

    // Invoked after all interval state snapshots have been generated for the given
    // shard_idx and before any merging or printing of interval snapshots. This
    // invokes the finalize_interval_snapshots API for all tools that returned some
    // non-null interval snapshot.
    bool
    finalize_interval_snapshots(analyzer_worker_data_t *worker, bool parallel,
                                int shard_idx = 0);

    // Invoked when the given interval finishes during serial or parallel
    // analysis of the trace. For parallel analysis, the shard_id
    // parameter should be set to the shard_id for which the interval
    // finished. For serial analysis, it should remain the default value.
    bool
    process_interval(uint64_t interval_id, uint64_t interval_init_instr_count,
                     analyzer_worker_data_t *worker, bool parallel, bool at_instr_record,
                     int shard_idx = 0);

    // Compute interval id for the given latest_timestamp, assuming the trace (or
    // trace shard) starts at the given first_timestamp. This is relevant when
    // timestamp intervals are enabled using interval_microseconds_.
    uint64_t
    compute_timestamp_interval_id(uint64_t first_timestamp, uint64_t latest_timestamp);

    // Compute interval id at the given instr count. This is relevant when instr count
    // intervals are enabled using interval_instr_count_.
    uint64_t
    compute_instr_count_interval_id(uint64_t cur_instr_count);

    // Compute the interval end timestamp (non-inclusive) for the given interval_id,
    // assuming the trace (or trace shard) starts at the given first_timestamp.
    uint64_t
    compute_interval_end_timestamp(uint64_t first_timestamp, uint64_t interval_id);

    // Possibly advances the current interval id stored in the worker data, based
    // on the most recent seen timestamp in the trace stream. Returns whether the
    // current interval id was updated, and if so also sets the previous interval index
    // in prev_interval_index.
    // at_instr_record indicates that the next record that will be presented to
    // the analysis tools is an instr record.
    bool
    advance_interval_id(
        typename scheduler_tmpl_t<RecordType, ReaderType>::stream_t *stream,
        analyzer_shard_data_t *shard, uint64_t &prev_interval_index,
        uint64_t &prev_interval_init_instr_count, bool at_instr_record);

    // Collects interval results for all shards from the workers, and then optional
    // merges the shard-local intervals to form the whole-trace interval results using
    // merge_shard_interval_results(). Derived classes may override this to change
    // whether and how shard-local intervals are merged.
    virtual bool
    collect_and_maybe_merge_shard_interval_results();

    // Computes and stores the interval results in whole_trace_interval_snapshots_. For
    // serial analysis where we already have only a single shard, this involves
    // simply copying interval_state_snapshot_t* from the input. For parallel
    // analysis, this involves merging results from multiple shards for intervals
    // that map to the same final whole-trace interval.
    bool
    merge_shard_interval_results(
        std::vector<std::vector<
            typename analysis_tool_tmpl_t<RecordType>::interval_state_snapshot_t *>>
            &intervals,
        std::vector<typename analysis_tool_tmpl_t<RecordType>::interval_state_snapshot_t
                        *> &merged_intervals,
        int tool_idx);

    // Populates the per_shard_interval_snapshots_ field based on the interval snapshots
    // stored in worker_data_.
    void
    populate_unmerged_shard_interval_results();

    // Populates the whole_trace_interval_snapshots_ field based on the interval snapshots
    // stored in the only entry of worker_data_.
    void
    populate_serial_interval_results();

    // Combines all interval snapshots in the given vector to create the interval
    // snapshot for the whole-trace interval ending at interval_end_timestamp and
    // stores it in 'result'. These snapshots are for the tool at tool_idx. Returns
    // whether it was successful. This routine also combines the instr count fields
    // in analysis_tool_tmpl_t<RecordType>::interval_state_snapshot_t.
    bool
    combine_interval_snapshots(
        const std::vector<
            const typename analysis_tool_tmpl_t<RecordType>::interval_state_snapshot_t *>
            &latest_shard_snapshots,
        uint64_t interval_end_timestamp, int tool_idx,
        typename analysis_tool_tmpl_t<RecordType>::interval_state_snapshot_t *&result);

    uint64_t
    get_current_microseconds();

    bool success_;
    scheduler_tmpl_t<RecordType, ReaderType> scheduler_;
    std::string error_string_;
    // For serial mode we have just one entry here but it is this controller
    // thread who operates it, not a separately-created worker.
    std::vector<analyzer_worker_data_t> worker_data_;
    int num_tools_;
    analysis_tool_tmpl_t<RecordType> **tools_;
    // Stores the interval state snapshots, merged across shards. These are
    // produced when timestamp intervals are enabled using interval_microseconds_.
    //
    // whole_trace_interval_snapshots_[tool_idx] is a vector of the interval snapshots
    // (in order of the intervals) for that tool. For the parallel mode, these
    // interval state snapshots are produced after merging corresponding shard
    // interval snapshots using merge_shard_interval_results.
    std::vector<std::vector<
        typename analysis_tool_tmpl_t<RecordType>::interval_state_snapshot_t *>>
        whole_trace_interval_snapshots_;

    // Key that combines tool and shard idx for use with an std::unordered_map.
    struct key_tool_shard_t {
        int tool_idx;
        int shard_idx;
        bool
        operator==(const key_tool_shard_t &rhs) const
        {
            return tool_idx == rhs.tool_idx && shard_idx == rhs.shard_idx;
        }
    };
    struct key_tool_shard_hash_t {
        std::size_t
        operator()(const key_tool_shard_t &t) const
        {
            return std::hash<int>()(t.tool_idx ^ t.shard_idx);
        }
    };

    // Stores the interval state snapshots, unmerged across shards. These are
    // produced when instr count intervals are enabled using interval_instr_count_.
    //
    // per_shard_interval_snapshots_[(tool_idx, shard_idx)] is a vector
    // of the interval snapshots for that tool and shard. Note that the snapshots for
    // each shard are separate; they are not merged across shards.
    //
    // TODO i#6643: Figure out a useful way to merge instr count intervals across shards.
    // One way is to merge the shard interval snapshots that correspond to the same
    // [interval_instr_count_ * interval_id, interval_instr_count_ * (interval_id + 1))
    // shard-local instrs. But it is not clear whether this is useful.
    // Another way is to merge the shard interval snapshots that correspond to the same
    // [interval_instr_count_ * interval_id, interval_instr_count_ * (interval_id + 1))
    // whole-trace instrs. But that is much harder to compute. We'd need some way to
    // identify the whole-trace interval boundaries in each shard's stream (since we
    // process each shard separately); this would likely need a pre-processing pass.
    std::unordered_map<key_tool_shard_t,
                       std::vector<typename analysis_tool_tmpl_t<
                           RecordType>::interval_state_snapshot_t *>,
                       key_tool_shard_hash_t>
        per_shard_interval_snapshots_;

    bool parallel_;
    int worker_count_;
    const char *output_prefix_ = "[analyzer]";
    uint64_t skip_instrs_ = 0;
    uint64_t skip_records_ = 0;
    uint64_t skip_to_timestamp_ = 0;
    uint64_t exit_after_records_ = 0;
    uint64_t interval_microseconds_ = 0;
    uint64_t interval_instr_count_ = 0;
    int verbosity_ = 0;
    shard_type_t shard_type_ = SHARD_BY_THREAD;
    bool sched_by_time_ = false;
    typename sched_type_t::mapping_t sched_mapping_ = sched_type_t::MAP_TO_ANY_OUTPUT;

    // Factory to create noise generators that can then be added to the scheduler's
    // input workloads.
    noise_generator_factory_t<RecordType, ReaderType> noise_generator_factory_;
    bool add_noise_generator_ = false;

private:
    bool
    serial_mode_supported();
};

/** See #dynamorio::drmemtrace::analyzer_tmpl_t. */
typedef analyzer_tmpl_t<memref_t, reader_t> analyzer_t;

/** See #dynamorio::drmemtrace::analyzer_tmpl_t. */
typedef analyzer_tmpl_t<trace_entry_t, dynamorio::drmemtrace::record_reader_t>
    record_analyzer_t;

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _ANALYZER_H_ */
