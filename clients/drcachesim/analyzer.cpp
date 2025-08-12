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

#include "analyzer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include "memref.h"
#include "scheduler.h"
#include "analysis_tool.h"
#ifdef HAS_ZLIB
#    include "compressed_file_reader.h"
#else
#    include "file_reader.h"
#endif
#include "reader.h"
#include "record_file_reader.h"
#include "noise_generator.h"
#include "trace_entry.h"
#ifdef HAS_ZIP
#    include "reader/zipfile_file_reader.h"
#endif
#ifdef HAS_SNAPPY
#    include "reader/snappy_file_reader.h"
#endif
#include "common/utils.h"

namespace dynamorio {
namespace drmemtrace {

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
bool
analyzer_t::serial_mode_supported()
{
    return true;
}

template <>
bool
analyzer_t::record_has_tid(memref_t record, memref_tid_t &tid)
{
    // All memref_t records have tids (after PR #5739 changed the reader).
    tid = record.marker.tid;
    return true;
}

template <>
bool
analyzer_t::record_is_thread_final(memref_t record)
{
    return record.exit.type == TRACE_TYPE_THREAD_EXIT;
}

template <>
bool
analyzer_t::record_is_timestamp(const memref_t &record)
{
    return record.marker.type == TRACE_TYPE_MARKER &&
        record.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP;
}

template <>
bool
analyzer_t::record_is_instr(const memref_t &record)
{
    return type_is_instr(record.instr.type);
}

template <>
memref_t
analyzer_t::create_wait_marker()
{
    memref_t record = {}; // Zero the other fields.
    record.marker.type = TRACE_TYPE_MARKER;
    record.marker.marker_type = TRACE_MARKER_TYPE_CORE_WAIT;
    record.marker.tid = INVALID_THREAD_ID;
    return record;
}

template <>
memref_t
analyzer_t::create_idle_marker()
{
    memref_t record = {}; // Zero the other fields.
    record.marker.type = TRACE_TYPE_MARKER;
    record.marker.marker_type = TRACE_MARKER_TYPE_CORE_IDLE;
    record.marker.tid = INVALID_THREAD_ID;
    return record;
}

/******************************************************************************
 * Specializations for analyzer_tmpl_t<record_reader_t>, aka record_analyzer_t.
 */

template <>
bool
record_analyzer_t::serial_mode_supported()
{
    // TODO i#5727,i#5843: Once we move serial interleaving from file_reader_t into
    // the scheduler we can support serial mode for record files as we won't need
    // to implement interleaving inside record_file_reader_t.
    return false;
}

template <>
bool
record_analyzer_t::record_has_tid(trace_entry_t record, memref_tid_t &tid)
{
    if (record.type != TRACE_TYPE_THREAD)
        return false;
    tid = static_cast<memref_tid_t>(record.addr);
    return true;
}

template <>
bool
record_analyzer_t::record_is_thread_final(trace_entry_t record)
{
    return record.type == TRACE_TYPE_FOOTER;
}

template <>
bool
record_analyzer_t::record_is_timestamp(const trace_entry_t &record)
{
    return record.type == TRACE_TYPE_MARKER && record.size == TRACE_MARKER_TYPE_TIMESTAMP;
}

template <>
bool
record_analyzer_t::record_is_instr(const trace_entry_t &record)
{
    return type_is_instr(static_cast<trace_type_t>(record.type));
}

template <>
trace_entry_t
record_analyzer_t::create_wait_marker()
{
    trace_entry_t record;
    record.type = TRACE_TYPE_MARKER;
    record.size = TRACE_MARKER_TYPE_CORE_WAIT;
    record.addr = 0; // Marker value has no meaning so we zero it.
    return record;
}

template <>
trace_entry_t
record_analyzer_t::create_idle_marker()
{
    trace_entry_t record;
    record.type = TRACE_TYPE_MARKER;
    record.size = TRACE_MARKER_TYPE_CORE_IDLE;
    record.addr = 0; // Marker value has no meaning so we zero it.
    return record;
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
analyzer_tmpl_t<RecordType, ReaderType>::init_scheduler(
    const std::vector<std::string> &trace_paths,
    const std::set<memref_tid_t> &only_threads, const std::set<int> &only_shards,
    int output_limit, int verbosity, typename sched_type_t::scheduler_options_t options)
{
    verbosity_ = verbosity;
    if (trace_paths.empty()) {
        ERRMSG("Missing trace path(s)\n");
        return false;
    }
    std::vector<typename sched_type_t::range_t> regions;
    if (skip_instrs_ > 0) {
        // TODO i#5843: For serial mode with multiple inputs this is not doing the
        // right thing: this is skipping in every input stream, while the documented
        // behavior is supposed to be an output stream skip.  Once we have that
        // capability in the scheduler we should switch to that.
        regions.emplace_back(skip_instrs_ + 1, 0);
    }
    std::vector<typename sched_type_t::input_workload_t> workloads;
    for (const std::string &path : trace_paths) {
        if (path.empty()) {
            ERRMSG("Trace path is empty\n");
            return false;
        }
        workloads.emplace_back(path, regions);
        // As documented and checked in analyzer_multi.cpp, only_threads and only_shards
        // limits are not supported with -multi_indir.  That's already been checked, so
        // we do not perform additional checks here.
        workloads.back().only_threads = only_threads;
        workloads.back().only_shards = only_shards;
        workloads.back().output_limit = output_limit;
        if (regions.empty() && skip_to_timestamp_ > 0) {
            workloads.back().times_of_interest.emplace_back(skip_to_timestamp_, 0);
        }
    }
    return init_scheduler_common(workloads, std::move(options));
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::init_scheduler(
    std::unique_ptr<ReaderType> reader, std::unique_ptr<ReaderType> reader_end,
    int verbosity, typename sched_type_t::scheduler_options_t options)
{
    verbosity_ = verbosity;
    if (!reader || !reader_end) {
        ERRMSG("Readers are empty\n");
        return false;
    }
    std::vector<typename sched_type_t::input_reader_t> readers;
    // Use a sentinel for the tid so the scheduler will use the memref record tid.
    readers.emplace_back(std::move(reader), std::move(reader_end),
                         /*tid=*/INVALID_THREAD_ID);
    std::vector<typename sched_type_t::range_t> regions;
    if (skip_instrs_ > 0)
        regions.emplace_back(skip_instrs_ + 1, 0);
    std::vector<typename sched_type_t::input_workload_t> workloads;
    workloads.emplace_back(std::move(readers), regions);
    return init_scheduler_common(workloads, std::move(options));
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::init_scheduler_common(
    std::vector<typename sched_type_t::input_workload_t> &workloads,
    typename sched_type_t::scheduler_options_t options)
{
    // Add noise generator to input workloads.
    if (add_noise_generator_) {
        // TODO i#7216: here can be a good place to analyze the workloads in order to
        // tweak noise_generator_info_t parameters. For now we use noise_generator_info_t
        // default values.
        noise_generator_info_t noise_generator_info;
        // TODO i#7216: currently we only create a single-process, single-thread noise
        // generator. We plan to add more in the future (multi-process and/or
        // multi-thread noise generators).
        typename sched_type_t::input_reader_t noise_generator_reader =
            noise_generator_factory_.create_noise_generator(noise_generator_info);
        // Check for errors.
        error_string_ += noise_generator_factory_.get_error_string();
        if (!error_string_.empty()) {
            return false;
        }
        // input_workload_t needs a vector of input_reader_t, so we create a vector with
        // a single input_reader_t (the noise generator).
        std::vector<typename sched_type_t::input_reader_t> readers;
        readers.emplace_back(std::move(noise_generator_reader));
        // Add the noise generator to the scheduler's input workloads.
        workloads.emplace_back(std::move(readers));
    }

    for (int i = 0; i < num_tools_; ++i) {
        if (parallel_ && !tools_[i]->parallel_shard_supported()) {
            parallel_ = false;
            break;
        }
    }

    typename sched_type_t::scheduler_options_t sched_ops;
    int output_count = worker_count_;
    if (shard_type_ == SHARD_BY_CORE) {
        // Subclass must pass us options and set worker_count_ to # cores.
        if (worker_count_ <= 0) {
            error_string_ = "For -core_sharded, core count must be > 0";
            return false;
        }
        sched_ops = std::move(options);
        if (sched_ops.quantum_unit == sched_type_t::QUANTUM_TIME)
            sched_by_time_ = true;
        if (!parallel_) {
            // output_count remains the # of virtual cores, but we have just
            // one worker thread.  The scheduler multiplexes the output_count output
            // cores onto a single stream for us with this option:
            sched_ops.single_lockstep_output = true;
            worker_count_ = 1;
        }
    } else {
        if (parallel_) {
            sched_ops = sched_type_t::make_scheduler_parallel_options(verbosity_);
            if (worker_count_ <= 0)
                worker_count_ = std::thread::hardware_concurrency();
            output_count = worker_count_;
        } else {
            sched_ops = sched_type_t::make_scheduler_serial_options(verbosity_);
            worker_count_ = 1;
            output_count = 1;
        }
        // As noted in the init_scheduler_common() header comment, we preserve only
        // some select fields.
        sched_ops.replay_as_traced_istream = options.replay_as_traced_istream;
        sched_ops.read_inputs_in_init = options.read_inputs_in_init;
        sched_ops.kernel_syscall_trace_path = options.kernel_syscall_trace_path;
    }
    sched_mapping_ = options.mapping;
    if (scheduler_.init(workloads, output_count, std::move(sched_ops)) !=
        sched_type_t::STATUS_SUCCESS) {
        ERRMSG("Failed to initialize scheduler: %s\n",
               scheduler_.get_error_string().c_str());
        return false;
    }

    for (int i = 0; i < worker_count_; ++i) {
        worker_data_.push_back(analyzer_worker_data_t(i, scheduler_.get_stream(i)));
        if (options.read_inputs_in_init) {
            // The docs say we can query the filetype up front.
            uint64_t filetype = scheduler_.get_stream(i)->get_filetype();
            VPRINT(this, 2, "Worker %d filetype %" PRIx64 "\n", i, filetype);
            if (TESTANY(OFFLINE_FILE_TYPE_CORE_SHARDED, filetype)) {
                if (i == 0 && shard_type_ == SHARD_BY_CORE) {
                    // This is almost certainly user error.
                    // Better to exit than risk user confusion.
                    // XXX i#7045: Ideally this could be reported as an error by the
                    // scheduler, and also detected early in analyzer_multi to auto-fix
                    // (when no mode is specified: if the user specifies core-sharding
                    // there could be config differences and this should be an error),
                    // but neither is simple so today the user has to re-run.
                    error_string_ =
                        "Re-scheduling a core-sharded-on-disk trace is generally a "
                        "mistake; re-run with -no_core_sharded.\n";
                    return false;
                }
                shard_type_ = SHARD_BY_CORE;
            }
        }
    }

    return true;
}

template <typename RecordType, typename ReaderType>
analyzer_tmpl_t<RecordType, ReaderType>::analyzer_tmpl_t(
    const std::string &trace_path, analysis_tool_tmpl_t<RecordType> **tools,
    int num_tools, int worker_count, uint64_t skip_instrs, uint64_t interval_microseconds,
    uint64_t interval_instr_count, int verbosity)
    : success_(true)
    , num_tools_(num_tools)
    , tools_(tools)
    , parallel_(true)
    , worker_count_(worker_count)
    , skip_instrs_(skip_instrs)
    , interval_microseconds_(interval_microseconds)
    , interval_instr_count_(interval_instr_count)
    , verbosity_(verbosity)
{
    if (interval_microseconds_ > 0 && interval_instr_count_ > 0) {
        success_ = false;
        error_string_ = "Cannot enable both kinds of interval analysis";
        return;
    }
    // The scheduler will call reader_t::init() for each input file.  We assume
    // that won't block (analyzer_multi_t separates out IPC readers).
    typename sched_type_t::scheduler_options_t sched_ops;
    if (!init_scheduler({ trace_path }, {}, {}, /*output_limit=*/0, verbosity,
                        std::move(sched_ops))) {
        success_ = false;
        error_string_ = "Failed to create scheduler";
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
    }
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

template <typename RecordType, typename ReaderType>
uint64_t
analyzer_tmpl_t<RecordType, ReaderType>::get_current_microseconds()
{
    return get_microsecond_timestamp();
}

template <typename RecordType, typename ReaderType>
uint64_t
analyzer_tmpl_t<RecordType, ReaderType>::compute_timestamp_interval_id(
    uint64_t first_timestamp, uint64_t latest_timestamp)
{
    assert(first_timestamp <= latest_timestamp);
    assert(interval_microseconds_ > 0);
    // We keep the interval end timestamps independent of the first timestamp of the
    // trace. For the parallel mode, where we need to merge intervals from different
    // shards that were active during the same final whole-trace interval, having aligned
    // interval-end points makes it easier to merge. Note that interval ids are however
    // still dependent on the first timestamp since we want interval ids to start at a
    // small number >= 1.
    return latest_timestamp / interval_microseconds_ -
        first_timestamp / interval_microseconds_ + 1;
}

template <typename RecordType, typename ReaderType>
uint64_t
analyzer_tmpl_t<RecordType, ReaderType>::compute_instr_count_interval_id(
    uint64_t cur_instr_count)
{
    assert(interval_instr_count_ > 0);
    if (cur_instr_count == 0)
        return 1;
    // We want all memory access entries following an instr to stay in the same
    // interval as the instr, so we increment interval_id at instr entries. Also,
    // we want the last instr in each interval to have an ordinal that's a multiple
    // of interval_instr_count_.
    return (cur_instr_count - 1) / interval_instr_count_ + 1;
}

template <typename RecordType, typename ReaderType>
uint64_t
analyzer_tmpl_t<RecordType, ReaderType>::compute_interval_end_timestamp(
    uint64_t first_timestamp, uint64_t interval_id)
{
    assert(interval_microseconds_ > 0);
    assert(interval_id >= 1);
    uint64_t end_timestamp =
        (first_timestamp / interval_microseconds_ + interval_id) * interval_microseconds_;
    // Since the interval's end timestamp is exclusive, the end_timestamp would actually
    // fall under the next interval.
    assert(compute_timestamp_interval_id(first_timestamp, end_timestamp) ==
           interval_id + 1);
    return end_timestamp;
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::advance_interval_id(
    typename scheduler_tmpl_t<RecordType, ReaderType>::stream_t *stream,
    analyzer_shard_data_t *shard, uint64_t &prev_interval_index,
    uint64_t &prev_interval_init_instr_count, bool at_instr_record)
{
    uint64_t next_interval_index = 0;
    if (interval_microseconds_ > 0) {
        next_interval_index = compute_timestamp_interval_id(stream->get_first_timestamp(),
                                                            stream->get_last_timestamp());
    } else if (interval_instr_count_ > 0) {
        // The interval callbacks are invoked just prior to the process_memref or
        // parallel_shard_memref callback for the first instr of the new interval; This
        // keeps the instr's memory accesses in the same interval as the instr.
        next_interval_index =
            compute_instr_count_interval_id(stream->get_instruction_ordinal());
    } else {
        return false;
    }
    if (next_interval_index != shard->cur_interval_index) {
        assert(next_interval_index > shard->cur_interval_index);
        prev_interval_index = shard->cur_interval_index;
        prev_interval_init_instr_count = shard->cur_interval_init_instr_count;
        shard->cur_interval_index = next_interval_index;
        // If the next record to be presented to the tools is an instr record, we need to
        // adjust for the fact that the record has already been read from the stream.
        // Since we know that the next record is a part of the new interval and
        // cur_interval_init_instr_count is supposed to be the count just prior to the
        // new interval, we need to subtract one count for the instr.
        shard->cur_interval_init_instr_count =
            stream->get_instruction_ordinal() - (at_instr_record ? 1 : 0);
        return true;
    }
    return false;
}

template <typename RecordType, typename ReaderType>
void
analyzer_tmpl_t<RecordType, ReaderType>::process_serial(analyzer_worker_data_t &worker)
{
    std::vector<void *> user_worker_data(num_tools_);

    worker.shard_data[0].tool_data.resize(num_tools_);
    if (interval_microseconds_ != 0 || interval_instr_count_ != 0)
        worker.shard_data[0].cur_interval_index = 1;
    for (int i = 0; i < num_tools_; ++i) {
        worker.error = tools_[i]->initialize_stream(worker.stream);
        if (!worker.error.empty())
            return;
        worker.error = tools_[i]->initialize_shard_type(shard_type_);
        if (!worker.error.empty())
            return;
    }
    std::unordered_set<int> tool_exited;
    while (true) {
        RecordType record;
        // The current time is used for time quanta; for instr quanta, it's ignored and
        // we pass 0 and let the scheduler use instruction + idle counts.
        uint64_t cur_micros = sched_by_time_ ? get_current_microseconds() : 0;
        typename sched_type_t::stream_status_t status =
            worker.stream->next_record(record, cur_micros);
        if (status == sched_type_t::STATUS_WAIT) {
            record = create_wait_marker();
        } else if (status == sched_type_t::STATUS_IDLE) {
            assert(shard_type_ == SHARD_BY_CORE);
            record = create_idle_marker();
        } else if (status != sched_type_t::STATUS_OK) {
            if (status != sched_type_t::STATUS_EOF) {
                if (status == sched_type_t::STATUS_REGION_INVALID) {
                    worker.error =
                        "Too-far -skip_instrs for: " + worker.stream->get_stream_name();
                } else {
                    worker.error =
                        "Failed to read from trace: " + worker.stream->get_stream_name();
                }
            } else if (interval_microseconds_ != 0 || interval_instr_count_ != 0) {
                if (!process_interval(worker.shard_data[0].cur_interval_index,
                                      worker.shard_data[0].cur_interval_init_instr_count,
                                      &worker,
                                      /*parallel=*/false, /*at_instr_record=*/false) ||
                    !finalize_interval_snapshots(&worker, /*parallel=*/false))
                    return;
            }
            return;
        }
        // For zipfiles, we could jump chunk to chunk and use the record ordinal
        // marker, but this option is rarely used so we do a simple walk here.
        // Users should use skip_instrs for fast skipping.
        // We also do not present the prior timestamp when we get there.
        // Nor do we count anything the scheduler doesn't add to the ordinals:
        // dynamically injected synthetic records.
        if (skip_records_ > 0 &&
            skip_records_ >= worker.stream->get_output_record_ordinal())
            continue;
        uint64_t prev_interval_index;
        uint64_t prev_interval_init_instr_count;
        if ((record_is_timestamp(record) || record_is_instr(record)) &&
            advance_interval_id(worker.stream, &worker.shard_data[0], prev_interval_index,
                                prev_interval_init_instr_count,
                                record_is_instr(record)) &&
            !process_interval(prev_interval_index, prev_interval_init_instr_count,
                              &worker, /*parallel=*/false, record_is_instr(record))) {
            return;
        }
        for (int i = 0; i < num_tools_; ++i) {
            if (tool_exited.find(i) != tool_exited.end())
                continue;
            if (!tools_[i]->process_memref(record)) {
                worker.error = tools_[i]->get_error_string();
                if (worker.error.empty()) {
                    VPRINT(this, 1, "Worker %d tool %d exiting early on trace shard %s\n",
                           worker.index, i, worker.stream->get_stream_name().c_str());
                    tool_exited.insert(i);
                    if (static_cast<int>(tool_exited.size()) >= num_tools_) {
                        VPRINT(this, 1,
                               "Worker %d all tools exited early on trace shard %s\n",
                               worker.index, worker.stream->get_stream_name().c_str());
                        return;
                    }
                } else {
                    VPRINT(this, 1, "Worker %d hit memref error %s on trace shard %s\n",
                           worker.index, worker.error.c_str(),
                           worker.stream->get_stream_name().c_str());
                    return;
                }
            }
        }
        if (exit_after_records_ > 0 &&
            // We can't use get_record_ordinal() because it's the input
            // ordinal due to SCHEDULER_USE_INPUT_ORDINALS.  We do not want to
            // include skipped records here.
            worker.stream->get_output_record_ordinal() >=
                skip_records_ + exit_after_records_) {
            VPRINT(this, 1,
                   "Worker %d exiting after requested record count on shard %s\n",
                   worker.index, worker.stream->get_stream_name().c_str());
            return;
        }
    }
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::process_shard_exit(
    analyzer_worker_data_t *worker, int shard_index, bool do_process_final_interval)
{
    VPRINT(this, 1, "Worker %d finished trace shard %s\n", worker->index,
           worker->stream->get_stream_name().c_str());
    worker->shard_data[shard_index].exited = true;
    if (interval_microseconds_ != 0 || interval_instr_count_ != 0) {
        if (!do_process_final_interval) {
            ERRMSG("i#6793: Skipping process_interval for final interval of shard index "
                   "%d\n",
                   shard_index);
        } else if (!process_interval(
                       worker->shard_data[shard_index].cur_interval_index,
                       worker->shard_data[shard_index].cur_interval_init_instr_count,
                       worker,
                       /*parallel=*/true, /*at_instr_record=*/false, shard_index)) {
            return false;
        }
        if (!finalize_interval_snapshots(worker, /*parallel=*/true, shard_index)) {
            return false;
        }
    }
    for (int i = 0; i < num_tools_; ++i) {
        if (!tools_[i]->parallel_shard_exit(
                worker->shard_data[shard_index].tool_data[i].shard_data)) {
            worker->error = tools_[i]->parallel_shard_error(
                worker->shard_data[shard_index].tool_data[i].shard_data);
            VPRINT(this, 1, "Worker %d hit shard exit error %s on trace shard index %d\n",
                   worker->index, worker->error.c_str(), shard_index);
            return false;
        }
    }
    return true;
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::process_tasks_internal(
    analyzer_worker_data_t *worker)
{
    std::vector<void *> user_worker_data(num_tools_);

    for (int i = 0; i < num_tools_; ++i)
        user_worker_data[i] = tools_[i]->parallel_worker_init(worker->index);

    RecordType record;
    // The current time is used for time quanta; for instr quanta, it's ignored and
    // we pass 0.
    uint64_t cur_micros = sched_by_time_ ? get_current_microseconds() : 0;
    std::unordered_set<int> tool_exited;
    for (typename sched_type_t::stream_status_t status =
             worker->stream->next_record(record, cur_micros);
         status != sched_type_t::STATUS_EOF;
         status = worker->stream->next_record(record, cur_micros)) {
        if (sched_by_time_)
            cur_micros = get_current_microseconds();
        if (status == sched_type_t::STATUS_WAIT) {
            // We let tools know about waits so they can analyze the schedule.
            // We synthesize a record here.  If we wanted this to count toward output
            // stream ordinals we would need to add a scheduler API to inject it.
            record = create_wait_marker();
            if (parallel_) {
                // Don't spin on this artificial wait; retry later.
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } else if (status == sched_type_t::STATUS_IDLE) {
            assert(shard_type_ == SHARD_BY_CORE);
            // We let tools know about idle time so they can analyze cpu usage.
            // We synthesize a record here.  If we wanted this to count toward output
            // stream ordinals we would need to add a scheduler API to inject it.
            record = create_idle_marker();
        } else if (status != sched_type_t::STATUS_OK) {
            if (status == sched_type_t::STATUS_REGION_INVALID) {
                worker->error =
                    "Too-far -skip_instrs for: " + worker->stream->get_stream_name();
            } else {
                worker->error =
                    "Failed to read from trace: " + worker->stream->get_stream_name();
            }
            return false;
        }
        int shard_index = worker->stream->get_shard_index();
        if (worker->shard_data.find(shard_index) == worker->shard_data.end()) {
            VPRINT(this, 1, "Worker %d starting on trace shard %d stream is %p\n",
                   worker->index, shard_index, worker->stream);
            worker->shard_data[shard_index].tool_data.resize(num_tools_);
            if (interval_microseconds_ != 0 || interval_instr_count_ != 0)
                worker->shard_data[shard_index].cur_interval_index = 1;
            for (int i = 0; i < num_tools_; ++i) {
                worker->shard_data[shard_index].tool_data[i].shard_data =
                    tools_[i]->parallel_shard_init_stream(
                        shard_index, user_worker_data[i], worker->stream);
            }
            worker->shard_data[shard_index].shard_index = shard_index;
        }
        memref_tid_t tid;
        if (worker->shard_data[shard_index].shard_id == 0) {
            if (shard_type_ == SHARD_BY_CORE)
                worker->shard_data[shard_index].shard_id = worker->index;
            else if (record_has_tid(record, tid))
                worker->shard_data[shard_index].shard_id = tid;
        }
        // See comment in process_serial() on skip_records.
        // Parallel skipping is not well-supported: we skip in each worker, not each
        // shard, and even each shard (as -skip_instrs does today) may not be what the
        // user wants: XXX i#7230: Is there a better usage mode for parallel skipping?
        if (skip_records_ > 0 &&
            skip_records_ >= worker->stream->get_output_record_ordinal())
            continue;
        uint64_t prev_interval_index;
        uint64_t prev_interval_init_instr_count;
        if ((record_is_timestamp(record) || record_is_instr(record)) &&
            advance_interval_id(worker->stream, &worker->shard_data[shard_index],
                                prev_interval_index, prev_interval_init_instr_count,
                                record_is_instr(record)) &&
            !process_interval(prev_interval_index, prev_interval_init_instr_count, worker,
                              /*parallel=*/true, record_is_instr(record), shard_index)) {
            return false;
        }
        for (int i = 0; i < num_tools_; ++i) {
            if (tool_exited.find(i) != tool_exited.end())
                continue;
            if (!tools_[i]->parallel_shard_memref(
                    worker->shard_data[shard_index].tool_data[i].shard_data, record)) {
                worker->error = tools_[i]->parallel_shard_error(
                    worker->shard_data[shard_index].tool_data[i].shard_data);
                if (worker->error.empty()) {
                    VPRINT(this, 1, "Worker %d tool %d exiting early on trace shard %s\n",
                           worker->index, i, worker->stream->get_stream_name().c_str());
                    tool_exited.insert(i);
                    if (static_cast<int>(tool_exited.size()) >= num_tools_) {
                        VPRINT(this, 1,
                               "Worker %d all tools exited early on trace shard %s\n",
                               worker->index, worker->stream->get_stream_name().c_str());
                        return true;
                    }
                } else {
                    VPRINT(this, 1,
                           "Worker %d hit shard memref error %s on trace shard %s\n",
                           worker->index, worker->error.c_str(),
                           worker->stream->get_stream_name().c_str());
                    return false;
                }
            }
        }
        if (record_is_thread_final(record) && shard_type_ != SHARD_BY_CORE) {
            if (!process_shard_exit(worker, shard_index)) {
                return false;
            }
        }
        if (exit_after_records_ > 0 &&
            // We can't use get_record_ordinal() because it's the input
            // ordinal due to SCHEDULER_USE_INPUT_ORDINALS.  We do not want to
            // include skipped records here.
            worker->stream->get_output_record_ordinal() >=
                skip_records_ + exit_after_records_) {
            VPRINT(this, 1,
                   "Worker %d exiting after requested record count on shard %s\n",
                   worker->index, worker->stream->get_stream_name().c_str());
            return true;
        }
    }
    if (shard_type_ == SHARD_BY_CORE) {
        if (worker->shard_data.find(worker->index) != worker->shard_data.end()) {
            if (!process_shard_exit(worker, worker->index)) {
                return false;
            }
        }
    }
    // i#6444: Fallback for cases where there is a missing thread final record in
    // non-core-sharded traces, in which case we have not yet invoked
    // process_shard_exit.
    for (const auto &keyval : worker->shard_data) {
        if (!keyval.second.exited) {
            // i#6793: We skip processing the final interval for shards exited here
            // if the stream has already moved on and cannot provide the state for
            // the shard anymore.
            bool do_process_final_interval =
                keyval.second.shard_index == worker->stream->get_shard_index();
            if (!process_shard_exit(worker, keyval.second.shard_index,
                                    do_process_final_interval)) {
                return false;
            }
        }
    }
    for (int i = 0; i < num_tools_; ++i) {
        const std::string error = tools_[i]->parallel_worker_exit(user_worker_data[i]);
        if (!error.empty()) {
            worker->error = error;
            VPRINT(this, 1, "Worker %d hit worker exit error %s\n", worker->index,
                   error.c_str());
            return false;
        }
    }
    return true;
}

template <typename RecordType, typename ReaderType>
void
analyzer_tmpl_t<RecordType, ReaderType>::process_tasks(analyzer_worker_data_t *worker)
{
    if (!process_tasks_internal(worker)) {
        if (sched_mapping_ == sched_type_t::MAP_TO_ANY_OUTPUT) {
            // Avoid a hang in the scheduler if we leave our current input stranded.
            // XXX: Better to just do a global exit and not let the other threads
            // keep running?  That breaks the current model where errors are
            // propagated to the user to decide what to do.
            // We could perhaps add thread synch points to have other threads
            // exit earlier: but maybe some uses cases consider one shard error
            // to not affect others and not be fatal?
            if (worker->stream->set_active(false) != sched_type_t::STATUS_OK) {
                ERRMSG("Failed to set failing worker to inactive; may hang");
            }
        }
    }
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::combine_interval_snapshots(
    const std::vector<
        const typename analysis_tool_tmpl_t<RecordType>::interval_state_snapshot_t *>
        &latest_shard_snapshots,
    uint64_t interval_end_timestamp, int tool_idx,
    typename analysis_tool_tmpl_t<RecordType>::interval_state_snapshot_t *&result)
{
    result = tools_[tool_idx]->combine_interval_snapshots(latest_shard_snapshots,
                                                          interval_end_timestamp);
    if (result == nullptr) {
        error_string_ = "combine_interval_snapshots unexpectedly returned nullptr: " +
            tools_[tool_idx]->get_error_string();
        return false;
    }
    result->instr_count_delta_ = 0;
    result->instr_count_cumulative_ = 0;
    for (auto snapshot : latest_shard_snapshots) {
        if (snapshot == nullptr)
            continue;
        // As discussed in the doc for analysis_tool_t::combine_interval_snapshots,
        // we combine all shard's latest snapshots for cumulative metrics, whereas
        // we combine only the shards active in current interval for delta metrics.
        result->instr_count_cumulative_ += snapshot->instr_count_cumulative_;
        if (snapshot->interval_end_timestamp_ == interval_end_timestamp)
            result->instr_count_delta_ += snapshot->instr_count_delta_;
    }
    return true;
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::merge_shard_interval_results(
    // intervals[shard_idx] is a vector of interval_state_snapshot_t*
    // representing the interval snapshots for that shard.
    std::vector<std::vector<
        typename analysis_tool_tmpl_t<RecordType>::interval_state_snapshot_t *>>
        &intervals,
    // This function will write the resulting whole-trace intervals to
    // merged_intervals.
    std::vector<typename analysis_tool_tmpl_t<RecordType>::interval_state_snapshot_t *>
        &merged_intervals,
    int tool_idx)
{
    assert(!intervals.empty());
    assert(merged_intervals.empty());
    // Used to recompute the interval_id for the result whole trace intervals, which are
    // numbered by the earliest shard's timestamp.
    uint64_t earliest_ever_interval_end_timestamp = std::numeric_limits<uint64_t>::max();
    size_t shard_count = intervals.size();
    std::vector<size_t> at_idx(shard_count, 0);
    bool any_shard_has_results_left = true;
    std::vector<typename analysis_tool_tmpl_t<RecordType>::interval_state_snapshot_t *>
        last_snapshot_per_shard(shard_count, nullptr);
    while (any_shard_has_results_left) {
        // Look for the next whole trace interval across all shards, which will be the
        // one with the earliest interval-end timestamp.
        uint64_t earliest_interval_end_timestamp = std::numeric_limits<uint64_t>::max();
        for (size_t shard_idx = 0; shard_idx < shard_count; ++shard_idx) {
            if (at_idx[shard_idx] == intervals[shard_idx].size())
                continue;
            earliest_interval_end_timestamp = std::min(
                earliest_interval_end_timestamp,
                intervals[shard_idx][at_idx[shard_idx]]->interval_end_timestamp_);
        }
        // We're done if no shard has any interval left unprocessed.
        if (earliest_interval_end_timestamp == std::numeric_limits<uint64_t>::max()) {
            any_shard_has_results_left = false;
            continue;
        }
        assert(earliest_interval_end_timestamp % interval_microseconds_ == 0);
        if (earliest_ever_interval_end_timestamp ==
            std::numeric_limits<uint64_t>::max()) {
            earliest_ever_interval_end_timestamp = earliest_interval_end_timestamp;
        }
        // Update last_snapshot_per_shard for shards that were active during this
        // interval, which have a timestamp == earliest_interval_end_timestamp.
        for (size_t shard_idx = 0; shard_idx < shard_count; ++shard_idx) {
            if (at_idx[shard_idx] == intervals[shard_idx].size())
                continue;
            uint64_t cur_interval_end_timestamp =
                intervals[shard_idx][at_idx[shard_idx]]->interval_end_timestamp_;
            assert(cur_interval_end_timestamp >= earliest_interval_end_timestamp);
            if (cur_interval_end_timestamp > earliest_interval_end_timestamp)
                continue;
            // This shard was active during this interval. So, we update the current
            // shard's latest interval snapshot.
            if (last_snapshot_per_shard[shard_idx] != nullptr) {
                if (!tools_[tool_idx]->release_interval_snapshot(
                        last_snapshot_per_shard[shard_idx])) {
                    error_string_ = tools_[tool_idx]->get_error_string();
                    return false;
                }
            }
            last_snapshot_per_shard[shard_idx] = intervals[shard_idx][at_idx[shard_idx]];
            ++at_idx[shard_idx];
        }
        // Merge last_snapshot_per_shard to form the result of the current
        // whole-trace interval.
        std::vector<
            const typename analysis_tool_tmpl_t<RecordType>::interval_state_snapshot_t *>
            const_last_snapshot_per_shard;
        const_last_snapshot_per_shard.insert(const_last_snapshot_per_shard.end(),
                                             last_snapshot_per_shard.begin(),
                                             last_snapshot_per_shard.end());
        typename analysis_tool_tmpl_t<RecordType>::interval_state_snapshot_t
            *cur_merged_interval;
        if (!combine_interval_snapshots(const_last_snapshot_per_shard,
                                        earliest_interval_end_timestamp, tool_idx,
                                        cur_merged_interval))
            return false;
        // Add the merged interval to the result list of whole trace intervals.
        cur_merged_interval->shard_id_ = analysis_tool_tmpl_t<
            RecordType>::interval_state_snapshot_t::WHOLE_TRACE_SHARD_ID;
        cur_merged_interval->interval_end_timestamp_ = earliest_interval_end_timestamp;
        cur_merged_interval->interval_id_ = compute_timestamp_interval_id(
            earliest_ever_interval_end_timestamp, earliest_interval_end_timestamp);
        merged_intervals.push_back(cur_merged_interval);
    }
    for (auto snapshot : last_snapshot_per_shard) {
        if (snapshot != nullptr &&
            !tools_[tool_idx]->release_interval_snapshot(snapshot)) {
            error_string_ = tools_[tool_idx]->get_error_string();
            return false;
        }
    }
    return true;
}

template <typename RecordType, typename ReaderType>
void
analyzer_tmpl_t<RecordType, ReaderType>::populate_unmerged_shard_interval_results()
{
    for (auto &worker : worker_data_) {
        for (auto &shard_data : worker.shard_data) {
            assert(static_cast<int>(shard_data.second.tool_data.size()) == num_tools_);
            for (int tool_idx = 0; tool_idx < num_tools_; ++tool_idx) {
                key_tool_shard_t tool_shard_key = { tool_idx,
                                                    shard_data.second.shard_index };
                per_shard_interval_snapshots_[tool_shard_key] = std::move(
                    shard_data.second.tool_data[tool_idx].interval_snapshot_data);
            }
        }
    }
}

template <typename RecordType, typename ReaderType>
void
analyzer_tmpl_t<RecordType, ReaderType>::populate_serial_interval_results()
{
    assert(whole_trace_interval_snapshots_.empty());
    whole_trace_interval_snapshots_.resize(num_tools_);
    assert(worker_data_.size() == 1);
    assert(worker_data_[0].shard_data.size() == 1 &&
           worker_data_[0].shard_data.count(0) == 1);
    assert(static_cast<int>(worker_data_[0].shard_data[0].tool_data.size()) ==
           num_tools_);
    for (int tool_idx = 0; tool_idx < num_tools_; ++tool_idx) {
        whole_trace_interval_snapshots_[tool_idx] = std::move(
            worker_data_[0].shard_data[0].tool_data[tool_idx].interval_snapshot_data);
    }
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::collect_and_maybe_merge_shard_interval_results()
{
    assert(interval_microseconds_ != 0 || interval_instr_count_ != 0);
    if (!parallel_) {
        populate_serial_interval_results();
        return true;
    }
    if (interval_instr_count_ > 0) {
        // We do not merge interval state snapshots across shards. See comment by
        // per_shard_interval_snapshots for more details.
        populate_unmerged_shard_interval_results();
        return true;
    }
    // all_intervals[tool_idx][shard_idx] contains a vector of the
    // interval_state_snapshot_t* that were output by that tool for that shard.
    std::vector<std::vector<std::vector<
        typename analysis_tool_tmpl_t<RecordType>::interval_state_snapshot_t *>>>
        all_intervals(num_tools_);
    for (const auto &worker : worker_data_) {
        for (const auto &shard_data : worker.shard_data) {
            assert(static_cast<int>(shard_data.second.tool_data.size()) == num_tools_);
            for (int tool_idx = 0; tool_idx < num_tools_; ++tool_idx) {
                all_intervals[tool_idx].emplace_back(std::move(
                    shard_data.second.tool_data[tool_idx].interval_snapshot_data));
            }
        }
    }
    assert(whole_trace_interval_snapshots_.empty());
    whole_trace_interval_snapshots_.resize(num_tools_);
    for (int tool_idx = 0; tool_idx < num_tools_; ++tool_idx) {
        // We need to do this separately per tool because all tools may not
        // generate an interval_state_snapshot_t for the same intervals (even though
        // the framework notifies all tools of all intervals).
        if (!merge_shard_interval_results(all_intervals[tool_idx],
                                          whole_trace_interval_snapshots_[tool_idx],
                                          tool_idx)) {
            return false;
        }
    }
    return true;
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::run()
{
    // XXX i#3286: Add a %-completed progress message by looking at the file sizes.
    if (!parallel_) {
        process_serial(worker_data_[0]);
        if (!worker_data_[0].error.empty()) {
            error_string_ = worker_data_[0].error;
            return false;
        }
    } else {
        if (worker_count_ <= 0) {
            error_string_ = "Invalid worker count: must be > 0";
            return false;
        }
        for (int i = 0; i < num_tools_; ++i) {
            error_string_ = tools_[i]->initialize_stream(nullptr);
            if (!error_string_.empty())
                return false;
            error_string_ = tools_[i]->initialize_shard_type(shard_type_);
            if (!error_string_.empty())
                return false;
        }
        std::vector<std::thread> threads;
        VPRINT(this, 1, "Creating %d worker threads\n", worker_count_);
        threads.reserve(worker_count_);
        for (int i = 0; i < worker_count_; ++i) {
            threads.emplace_back(
                std::thread(&analyzer_tmpl_t::process_tasks, this, &worker_data_[i]));
        }
        for (std::thread &thread : threads)
            thread.join();
        for (auto &worker : worker_data_) {
            if (!worker.error.empty()) {
                error_string_ = worker.error;
                return false;
            }
        }
    }
    if (interval_microseconds_ != 0 || interval_instr_count_ != 0) {
        return collect_and_maybe_merge_shard_interval_results();
    }
    return true;
}

static void
print_output_separator()
{

    std::cerr << "\n=========================================================="
                 "=================\n";
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
            print_output_separator();
        }
    }
    // Now print interval results.
    // Should not have both whole-trace or per-shard interval snapshots.
    assert(whole_trace_interval_snapshots_.empty() ||
           per_shard_interval_snapshots_.empty());
    // We may have whole-trace intervals snapshots for instr count intervals in serial
    // mode, and for timestamp (microsecond) intervals in both serial and parallel mode.
    if (!whole_trace_interval_snapshots_.empty()) {
        // Separate non-interval and interval outputs.
        print_output_separator();
        std::cerr << "Printing whole-trace interval results:\n";
        for (int i = 0; i < num_tools_; ++i) {
            // whole_trace_interval_snapshots_[i] may be empty if the corresponding tool
            // did not produce any interval results.
            if (!whole_trace_interval_snapshots_[i].empty() &&
                !tools_[i]->print_interval_results(whole_trace_interval_snapshots_[i])) {
                error_string_ = tools_[i]->get_error_string();
                return false;
            }
            for (auto snapshot : whole_trace_interval_snapshots_[i]) {
                if (!tools_[i]->release_interval_snapshot(snapshot)) {
                    error_string_ = tools_[i]->get_error_string();
                    return false;
                }
            }
            if (i + 1 < num_tools_) {
                // Separate tool output.
                print_output_separator();
            }
        }
    } else if (!per_shard_interval_snapshots_.empty()) {
        // Separate non-interval and interval outputs.
        print_output_separator();
        std::cerr << "Printing unmerged per-shard interval results:\n";
        for (auto &interval_snapshots : per_shard_interval_snapshots_) {
            int tool_idx = interval_snapshots.first.tool_idx;
            if (!interval_snapshots.second.empty() &&
                !tools_[tool_idx]->print_interval_results(interval_snapshots.second)) {
                error_string_ = tools_[tool_idx]->get_error_string();
                return false;
            }
            for (auto snapshot : interval_snapshots.second) {
                if (!tools_[tool_idx]->release_interval_snapshot(snapshot)) {
                    error_string_ = tools_[tool_idx]->get_error_string();
                    return false;
                }
            }
            print_output_separator();
        }
    }
    return true;
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::finalize_interval_snapshots(
    analyzer_worker_data_t *worker, bool parallel, int shard_idx)
{
    assert(parallel ||
           shard_idx == 0); // Only parallel mode supports a non-zero shard_idx.
    for (int tool_idx = 0; tool_idx < num_tools_; ++tool_idx) {
        if (!worker->shard_data[shard_idx]
                 .tool_data[tool_idx]
                 .interval_snapshot_data.empty() &&
            !tools_[tool_idx]->finalize_interval_snapshots(worker->shard_data[shard_idx]
                                                               .tool_data[tool_idx]
                                                               .interval_snapshot_data)) {
            worker->error = tools_[tool_idx]->get_error_string();
            VPRINT(this, 1,
                   "Worker %d hit finalize_interval_snapshots error %s during %s "
                   "analysis in trace shard %s\n",
                   worker->index, worker->error.c_str(), parallel ? "parallel" : "serial",
                   worker->stream->get_stream_name().c_str());
            return false;
        }
    }
    return true;
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::process_interval(
    uint64_t interval_id, uint64_t interval_init_instr_count,
    analyzer_worker_data_t *worker, bool parallel, bool at_instr_record, int shard_idx)
{
    assert(parallel ||
           shard_idx == 0); // Only parallel mode supports a non-zero shard_idx.
    for (int tool_idx = 0; tool_idx < num_tools_; ++tool_idx) {
        typename analysis_tool_tmpl_t<RecordType>::interval_state_snapshot_t *snapshot;
        if (parallel) {
            snapshot = tools_[tool_idx]->generate_shard_interval_snapshot(
                worker->shard_data[shard_idx].tool_data[tool_idx].shard_data,
                interval_id);
        } else {
            snapshot = tools_[tool_idx]->generate_interval_snapshot(interval_id);
        }
        if (tools_[tool_idx]->get_error_string() != "") {
            worker->error = tools_[tool_idx]->get_error_string();
            VPRINT(this, 1,
                   "Worker %d hit process_interval error %s during %s analysis in trace "
                   "shard %s at "
                   "interval %" PRId64 "\n",
                   worker->index, worker->error.c_str(), parallel ? "parallel" : "serial",
                   worker->stream->get_stream_name().c_str(), interval_id);
            return false;
        }
        if (snapshot != nullptr) {
            snapshot->shard_id_ = parallel
                ? worker->shard_data[shard_idx].shard_id
                : analysis_tool_tmpl_t<
                      RecordType>::interval_state_snapshot_t::WHOLE_TRACE_SHARD_ID;
            snapshot->interval_id_ = interval_id;
            if (interval_microseconds_ > 0) {
                // For timestamp intervals, the interval_end_timestamp is the abstract
                // non-inclusive end timestamp for the interval_id. This is to make it
                // easier to line up the corresponding shard interval snapshots so that
                // we can merge them to form the whole-trace interval snapshots.
                snapshot->interval_end_timestamp_ = compute_interval_end_timestamp(
                    worker->stream->get_first_timestamp(), interval_id);
            } else {
                snapshot->interval_end_timestamp_ = worker->stream->get_last_timestamp();
            }
            // instr_count_cumulative for the interval snapshot is supposed to be
            // inclusive, so if the first record after the interval (that is, the record
            // we're at right now) is an instr, it must be subtracted.
            snapshot->instr_count_cumulative_ =
                worker->stream->get_instruction_ordinal() - (at_instr_record ? 1 : 0);
            snapshot->instr_count_delta_ =
                snapshot->instr_count_cumulative_ - interval_init_instr_count;
            worker->shard_data[shard_idx]
                .tool_data[tool_idx]
                .interval_snapshot_data.push_back(snapshot);
        }
    }
    return true;
}

template class analyzer_tmpl_t<memref_t, reader_t>;
template class analyzer_tmpl_t<trace_entry_t, dynamorio::drmemtrace::record_reader_t>;

} // namespace drmemtrace
} // namespace dynamorio
