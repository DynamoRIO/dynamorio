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

#include "analyzer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <memory>
#include <queue>
#include <string>
#include <thread>
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
analyzer_tmpl_t<RecordType, ReaderType>::init_scheduler(const std::string &trace_path,
                                                        memref_tid_t only_thread,
                                                        int verbosity)
{
    verbosity_ = verbosity;
    if (trace_path.empty()) {
        ERRMSG("Trace file name is empty\n");
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
    typename sched_type_t::input_workload_t workload(trace_path, regions);
    if (only_thread != INVALID_THREAD_ID) {
        workload.only_threads.insert(only_thread);
    }
    return init_scheduler_common(workload);
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::init_scheduler(
    std::unique_ptr<ReaderType> reader, std::unique_ptr<ReaderType> reader_end,
    int verbosity)
{
    verbosity_ = verbosity;
    if (!reader || !reader_end) {
        ERRMSG("Readers are empty\n");
        return false;
    }
    std::vector<typename sched_type_t::input_reader_t> readers;
    // With no modifiers or only_threads the tid doesn't matter.
    readers.emplace_back(std::move(reader), std::move(reader_end), /*tid=*/1);
    std::vector<typename sched_type_t::range_t> regions;
    if (skip_instrs_ > 0)
        regions.emplace_back(skip_instrs_ + 1, 0);
    typename sched_type_t::input_workload_t workload(std::move(readers), regions);
    return init_scheduler_common(workload);
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::init_scheduler_common(
    typename sched_type_t::input_workload_t &workload)
{
    for (int i = 0; i < num_tools_; ++i) {
        if (parallel_ && !tools_[i]->parallel_shard_supported()) {
            parallel_ = false;
            break;
        }
    }
    std::vector<typename sched_type_t::input_workload_t> sched_inputs(1);
    sched_inputs[0] = std::move(workload);
    typename sched_type_t::scheduler_options_t sched_ops;
    int output_count;
    if (parallel_) {
        sched_ops = sched_type_t::make_scheduler_parallel_options(verbosity_);
        if (worker_count_ <= 0)
            worker_count_ = std::thread::hardware_concurrency();
    } else {
        sched_ops = sched_type_t::make_scheduler_serial_options(verbosity_);
        worker_count_ = 1;
    }
    output_count = worker_count_;
    if (scheduler_.init(sched_inputs, output_count, sched_ops) !=
        sched_type_t::STATUS_SUCCESS) {
        ERRMSG("Failed to initialize scheduler: %s\n",
               scheduler_.get_error_string().c_str());
        return false;
    }

    for (int i = 0; i < worker_count_; ++i) {
        worker_data_.push_back(analyzer_worker_data_t(i, scheduler_.get_stream(i)));
    }

    return true;
}

template <typename RecordType, typename ReaderType>
analyzer_tmpl_t<RecordType, ReaderType>::analyzer_tmpl_t(
    const std::string &trace_path, analysis_tool_tmpl_t<RecordType> **tools,
    int num_tools, int worker_count, uint64_t skip_instrs, uint64_t interval_microseconds,
    int verbosity)
    : success_(true)
    , num_tools_(num_tools)
    , tools_(tools)
    , parallel_(true)
    , worker_count_(worker_count)
    , skip_instrs_(skip_instrs)
    , interval_microseconds_(interval_microseconds)
    , verbosity_(verbosity)
{
    // The scheduler will call reader_t::init() for each input file.  We assume
    // that won't block (analyzer_multi_t separates out IPC readers).
    if (!init_scheduler(trace_path, INVALID_THREAD_ID, verbosity)) {
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
analyzer_tmpl_t<RecordType, ReaderType>::compute_interval_id(uint64_t first_timestamp,
                                                             uint64_t latest_timestamp)
{
    assert(first_timestamp <= latest_timestamp);
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
analyzer_tmpl_t<RecordType, ReaderType>::compute_interval_end_timestamp(
    uint64_t first_timestamp, uint64_t interval_id)
{
    assert(interval_id >= 1);
    uint64_t end_timestamp =
        (first_timestamp / interval_microseconds_ + interval_id) * interval_microseconds_;
    // Since the interval's end timestamp is exclusive, the end_timestamp would actually
    // fall under the next interval.
    assert(compute_interval_id(first_timestamp, end_timestamp) == interval_id + 1);
    return end_timestamp;
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::advance_interval_id(
    typename scheduler_tmpl_t<RecordType, ReaderType>::stream_t *stream,
    analyzer_shard_data_t *shard, uint64_t &prev_interval_index,
    uint64_t &prev_interval_init_instr_count)
{
    if (interval_microseconds_ == 0) {
        return false;
    }
    uint64_t next_interval_index =
        compute_interval_id(stream->get_first_timestamp(), stream->get_last_timestamp());
    if (next_interval_index != shard->cur_interval_index) {
        assert(next_interval_index > shard->cur_interval_index);
        prev_interval_index = shard->cur_interval_index;
        prev_interval_init_instr_count = shard->cur_interval_init_instr_count;
        shard->cur_interval_index = next_interval_index;
        shard->cur_interval_init_instr_count = stream->get_instruction_ordinal();
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
    if (interval_microseconds_ != 0)
        worker.shard_data[0].cur_interval_index = 1;
    for (int i = 0; i < num_tools_; ++i) {
        worker.error = tools_[i]->initialize_stream(worker.stream);
        if (!worker.error.empty())
            return;
    }
    while (true) {
        RecordType record;
        typename sched_type_t::stream_status_t status =
            worker.stream->next_record(record);
        if (status != sched_type_t::STATUS_OK) {
            if (status != sched_type_t::STATUS_EOF) {
                if (status == sched_type_t::STATUS_REGION_INVALID) {
                    worker.error =
                        "Too-far -skip_instrs for: " + worker.stream->get_stream_name();
                } else {
                    worker.error =
                        "Failed to read from trace: " + worker.stream->get_stream_name();
                }
            } else if (interval_microseconds_ != 0) {
                process_interval(worker.shard_data[0].cur_interval_index,
                                 worker.shard_data[0].cur_interval_init_instr_count,
                                 &worker,
                                 /*parallel=*/false);
            }
            return;
        }
        uint64_t prev_interval_index;
        uint64_t prev_interval_init_instr_count;
        if (record_is_timestamp(record) &&
            advance_interval_id(worker.stream, &worker.shard_data[0], prev_interval_index,
                                prev_interval_init_instr_count) &&
            !process_interval(prev_interval_index, prev_interval_init_instr_count,
                              &worker, /*parallel=*/false)) {
            return;
        }
        for (int i = 0; i < num_tools_; ++i) {
            if (!tools_[i]->process_memref(record)) {
                worker.error = tools_[i]->get_error_string();
                VPRINT(this, 1, "Worker %d hit memref error %s on trace shard %s\n",
                       worker.index, worker.error.c_str(),
                       worker.stream->get_stream_name().c_str());
                return;
            }
        }
    }
}

template <typename RecordType, typename ReaderType>
void
analyzer_tmpl_t<RecordType, ReaderType>::process_tasks(analyzer_worker_data_t *worker)
{
    std::vector<void *> user_worker_data(num_tools_);

    for (int i = 0; i < num_tools_; ++i)
        user_worker_data[i] = tools_[i]->parallel_worker_init(worker->index);
    RecordType record;
    for (typename sched_type_t::stream_status_t status =
             worker->stream->next_record(record);
         status != sched_type_t::STATUS_EOF;
         status = worker->stream->next_record(record)) {
        if (status != sched_type_t::STATUS_OK) {
            if (status == sched_type_t::STATUS_REGION_INVALID) {
                worker->error =
                    "Too-far -skip_instrs for: " + worker->stream->get_stream_name();
            } else {
                worker->error =
                    "Failed to read from trace: " + worker->stream->get_stream_name();
            }
            return;
        }
        int shard_index = worker->stream->get_input_stream_ordinal();
        if (worker->shard_data.find(shard_index) == worker->shard_data.end()) {
            VPRINT(this, 1, "Worker %d starting on trace shard %d stream is %p\n",
                   worker->index, shard_index, worker->stream);
            worker->shard_data[shard_index].tool_data.resize(num_tools_);
            if (interval_microseconds_ != 0)
                worker->shard_data[shard_index].cur_interval_index = 1;
            for (int i = 0; i < num_tools_; ++i) {
                worker->shard_data[shard_index].tool_data[i].shard_data =
                    tools_[i]->parallel_shard_init_stream(
                        shard_index, user_worker_data[i], worker->stream);
            }
        }
        memref_tid_t tid;
        // Currently shards map only to threads, so the shard_id is the same as
        // the thread id.
        if (worker->shard_data[shard_index].shard_id == 0 &&
            record_has_tid(record, tid)) {
            worker->shard_data[shard_index].shard_id = tid;
        }
        uint64_t prev_interval_index;
        uint64_t prev_interval_init_instr_count;
        if (record_is_timestamp(record) &&
            advance_interval_id(worker->stream, &worker->shard_data[shard_index],
                                prev_interval_index, prev_interval_init_instr_count) &&
            !process_interval(prev_interval_index, prev_interval_init_instr_count, worker,
                              /*parallel=*/true, shard_index)) {
            return;
        }
        for (int i = 0; i < num_tools_; ++i) {
            if (!tools_[i]->parallel_shard_memref(
                    worker->shard_data[shard_index].tool_data[i].shard_data, record)) {
                worker->error = tools_[i]->parallel_shard_error(
                    worker->shard_data[shard_index].tool_data[i].shard_data);
                VPRINT(this, 1, "Worker %d hit shard memref error %s on trace shard %s\n",
                       worker->index, worker->error.c_str(),
                       worker->stream->get_stream_name().c_str());
                return;
            }
        }
        if (record_is_thread_final(record)) {
            VPRINT(this, 1, "Worker %d finished trace shard %s\n", worker->index,
                   worker->stream->get_stream_name().c_str());
            if (interval_microseconds_ != 0 &&
                !process_interval(
                    worker->shard_data[shard_index].cur_interval_index,
                    worker->shard_data[shard_index].cur_interval_init_instr_count, worker,
                    /*parallel=*/true, shard_index))
                return;
            for (int i = 0; i < num_tools_; ++i) {
                if (!tools_[i]->parallel_shard_exit(
                        worker->shard_data[shard_index].tool_data[i].shard_data)) {
                    worker->error = tools_[i]->parallel_shard_error(
                        worker->shard_data[shard_index].tool_data[i].shard_data);
                    VPRINT(this, 1,
                           "Worker %d hit shard exit error %s on trace shard %s\n",
                           worker->index, worker->error.c_str(),
                           worker->stream->get_stream_name().c_str());
                    return;
                }
            }
        }
    }
    for (int i = 0; i < num_tools_; ++i) {
        const std::string error = tools_[i]->parallel_worker_exit(user_worker_data[i]);
        if (!error.empty()) {
            worker->error = error;
            VPRINT(this, 1, "Worker %d hit worker exit error %s\n", worker->index,
                   error.c_str());
            return;
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
        error_string_ = "combine_interval_snapshots unexpectedly returned nullptr";
        return false;
    }
    result->instr_count_delta = 0;
    result->instr_count_cumulative = 0;
    for (auto snapshot : latest_shard_snapshots) {
        if (snapshot == nullptr)
            continue;
        // As discussed in the doc for analysis_tool_t::combine_interval_snapshots,
        // we combine all shard's latest snapshots for cumulative metrics, whereas
        // we combine only the shards active in current interval for delta metrics.
        result->instr_count_cumulative += snapshot->instr_count_cumulative;
        if (snapshot->interval_end_timestamp == interval_end_timestamp)
            result->instr_count_delta += snapshot->instr_count_delta;
    }
    return true;
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::merge_shard_interval_results(
    // intervals[shard_idx] is a queue of interval_state_snapshot_t*
    // representing the interval snapshots for that shard. This is a queue as we
    // process the intervals here in a FIFO manner. Using a queue also makes code
    // a bit simpler.
    std::vector<std::queue<
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
    bool any_shard_has_results_left = true;
    std::vector<typename analysis_tool_tmpl_t<RecordType>::interval_state_snapshot_t *>
        last_snapshot_per_shard(shard_count, nullptr);
    while (any_shard_has_results_left) {
        // Look for the next whole trace interval across all shards, which will be the
        // one with the earliest interval-end timestamp.
        uint64_t earliest_interval_end_timestamp = std::numeric_limits<uint64_t>::max();
        for (size_t shard_idx = 0; shard_idx < shard_count; ++shard_idx) {
            if (intervals[shard_idx].empty())
                continue;
            earliest_interval_end_timestamp =
                std::min(earliest_interval_end_timestamp,
                         intervals[shard_idx].front()->interval_end_timestamp);
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
            if (intervals[shard_idx].empty())
                continue;
            uint64_t cur_interval_end_timestamp =
                intervals[shard_idx].front()->interval_end_timestamp;
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
            last_snapshot_per_shard[shard_idx] = intervals[shard_idx].front();
            intervals[shard_idx].pop();
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
        cur_merged_interval->shard_id = analysis_tool_tmpl_t<
            RecordType>::interval_state_snapshot_t::WHOLE_TRACE_SHARD_ID;
        cur_merged_interval->interval_end_timestamp = earliest_interval_end_timestamp;
        cur_merged_interval->interval_id = compute_interval_id(
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
bool
analyzer_tmpl_t<RecordType, ReaderType>::collect_and_maybe_merge_shard_interval_results()
{
    // all_intervals[tool_idx][shard_idx] contains a queue of the
    // interval_state_snapshot_t* that were output by that tool for that shard.
    std::vector<std::vector<std::queue<
        typename analysis_tool_tmpl_t<RecordType>::interval_state_snapshot_t *>>>
        all_intervals(num_tools_);
    for (const auto &worker : worker_data_) {
        for (const auto &shard_data : worker.shard_data) {
            for (int tool_idx = 0; tool_idx < num_tools_; ++tool_idx) {
                all_intervals[tool_idx].emplace_back(std::move(
                    shard_data.second.tool_data[tool_idx].interval_snapshot_data));
            }
        }
    }
    assert(merged_interval_snapshots_.empty());
    merged_interval_snapshots_.resize(num_tools_);
    for (int tool_idx = 0; tool_idx < num_tools_; ++tool_idx) {
        // We need to do this separately per tool because all tools may not
        // generate an interval_state_snapshot_t for the same intervals (even though
        // the framework notifies all tools of all intervals).
        if (!merge_shard_interval_results(all_intervals[tool_idx],
                                          merged_interval_snapshots_[tool_idx],
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
    if (interval_microseconds_ != 0) {
        return collect_and_maybe_merge_shard_interval_results();
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
        if (interval_microseconds_ != 0 && !merged_interval_snapshots_.empty()) {
            // merged_interval_snapshots_ may be empty depending on the derived class's
            // implementation of collect_and_maybe_merge_shard_interval_results.
            if (!merged_interval_snapshots_[i].empty() &&
                !tools_[i]->print_interval_results(merged_interval_snapshots_[i])) {
                error_string_ = tools_[i]->get_error_string();
                return false;
            }
            for (auto snapshot : merged_interval_snapshots_[i]) {
                if (!tools_[i]->release_interval_snapshot(snapshot)) {
                    error_string_ = tools_[i]->get_error_string();
                    return false;
                }
            }
        }
        if (i + 1 < num_tools_) {
            // Separate tool output.
            std::cerr << "\n=========================================================="
                         "=================\n";
        }
    }
    return true;
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::process_interval(
    uint64_t interval_id, uint64_t interval_init_instr_count,
    analyzer_worker_data_t *worker, bool parallel, int shard_idx)
{
    assert(parallel || shard_idx == 0); // Default to zero for the serial mode.
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
            snapshot->shard_id = parallel
                ? worker->shard_data[shard_idx].shard_id
                : analysis_tool_tmpl_t<
                      RecordType>::interval_state_snapshot_t::WHOLE_TRACE_SHARD_ID;
            snapshot->interval_id = interval_id;
            snapshot->interval_end_timestamp = compute_interval_end_timestamp(
                worker->stream->get_first_timestamp(), interval_id);
            snapshot->instr_count_cumulative = worker->stream->get_instruction_ordinal();
            snapshot->instr_count_delta =
                snapshot->instr_count_cumulative - interval_init_instr_count;
            worker->shard_data[shard_idx].tool_data[tool_idx].interval_snapshot_data.push(
                snapshot);
        }
    }
    return true;
}

template class analyzer_tmpl_t<memref_t, reader_t>;
template class analyzer_tmpl_t<trace_entry_t, dynamorio::drmemtrace::record_reader_t>;

} // namespace drmemtrace
} // namespace dynamorio
