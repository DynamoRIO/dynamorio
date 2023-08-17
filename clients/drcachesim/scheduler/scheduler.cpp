/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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

#include "scheduler.h"

#include <stdint.h>

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <memory>
#include <mutex>
#include <ostream>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "memref.h"
#include "memtrace_stream.h"
#include "reader.h"
#include "record_file_reader.h"
#include "trace_entry.h"
#ifdef HAS_LZ4
#    include "lz4_file_reader.h"
#endif
#ifdef HAS_ZLIB
#    include "compressed_file_reader.h"
#endif
#ifdef HAS_ZIP
#    include "zipfile_file_reader.h"
#else
#    include "file_reader.h"
#endif
#ifdef HAS_SNAPPY
#    include "snappy_file_reader.h"
#endif
#include "directory_iterator.h"
#include "utils.h"
#ifdef UNIX
#    include <sys/time.h>
#else
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#endif

#undef VPRINT
#ifdef DEBUG
#    define VPRINT(obj, level, ...)                            \
        do {                                                   \
            if ((obj)->verbosity_ >= (level)) {                \
                fprintf(stderr, "%s ", (obj)->output_prefix_); \
                fprintf(stderr, __VA_ARGS__);                  \
            }                                                  \
        } while (0)
#    define VDO(obj, level, statement)        \
        do {                                  \
            if ((obj)->verbosity_ >= (level)) \
                statement                     \
        } while (0)
#else
#    define VPRINT(reader, level, ...)  /* Nothing. */
#    define VDO(obj, level, statementx) /* Nothing. */
#endif

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
 * Specializations for scheduler_tmpl_t<reader_t>, aka scheduler_t.
 */

template <>
std::unique_ptr<reader_t>
scheduler_tmpl_t<memref_t, reader_t>::get_default_reader()
{
    return std::unique_ptr<default_file_reader_t>(new default_file_reader_t());
}

template <>
std::unique_ptr<reader_t>
scheduler_tmpl_t<memref_t, reader_t>::get_reader(const std::string &path, int verbosity)
{
#if defined(HAS_SNAPPY) || defined(HAS_ZIP) || defined(HAS_LZ4)
#    ifdef HAS_LZ4
    if (ends_with(path, ".lz4")) {
        return std::unique_ptr<reader_t>(new lz4_file_reader_t(path, verbosity));
    }
#    endif
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
            error_string_ =
                "Failed to list directory " + path + ": " + iter.error_string() + ". ";
            return nullptr;
        }
        for (; iter != end; ++iter) {
            const std::string fname = *iter;
            if (fname == "." || fname == ".." ||
                starts_with(fname, DRMEMTRACE_SERIAL_SCHEDULE_FILENAME) ||
                fname == DRMEMTRACE_CPU_SCHEDULE_FILENAME)
                continue;
            // Skip the auxiliary files.
            if (fname == DRMEMTRACE_MODULE_LIST_FILENAME ||
                fname == DRMEMTRACE_FUNCTION_LIST_FILENAME ||
                fname == DRMEMTRACE_ENCODING_FILENAME)
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
#    ifdef HAS_LZ4
            if (ends_with(path, ".lz4")) {
                return std::unique_ptr<reader_t>(new lz4_file_reader_t(path, verbosity));
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
scheduler_tmpl_t<memref_t, reader_t>::record_type_has_tid(memref_t record,
                                                          memref_tid_t &tid)
{
    if (record.marker.tid == INVALID_THREAD_ID)
        return false;
    tid = record.marker.tid;
    return true;
}

template <>
bool
scheduler_tmpl_t<memref_t, reader_t>::record_type_is_instr(memref_t record)
{
    return type_is_instr(record.instr.type);
}

template <>
bool
scheduler_tmpl_t<memref_t, reader_t>::record_type_is_marker(memref_t record,
                                                            trace_marker_type_t &type,
                                                            uintptr_t &value)
{
    if (record.marker.type != TRACE_TYPE_MARKER)
        return false;
    type = record.marker.marker_type;
    value = record.marker.marker_value;
    return true;
}

template <>
bool
scheduler_tmpl_t<memref_t, reader_t>::record_type_is_timestamp(memref_t record,
                                                               uintptr_t &value)
{
    if (record.marker.type != TRACE_TYPE_MARKER ||
        record.marker.marker_type != TRACE_MARKER_TYPE_TIMESTAMP)
        return false;
    value = record.marker.marker_value;
    return true;
}

template <>
memref_t
scheduler_tmpl_t<memref_t, reader_t>::create_region_separator_marker(memref_tid_t tid,
                                                                     uintptr_t value)
{
    memref_t record = {};
    record.marker.type = TRACE_TYPE_MARKER;
    record.marker.marker_type = TRACE_MARKER_TYPE_WINDOW_ID;
    record.marker.marker_value = value;
    // XXX i#5843: We have .pid as 0 for now; worth trying to fill it in?
    record.marker.tid = tid;
    return record;
}

template <>
memref_t
scheduler_tmpl_t<memref_t, reader_t>::create_thread_exit(memref_tid_t tid)
{
    memref_t record = {};
    record.exit.type = TRACE_TYPE_THREAD_EXIT;
    // XXX i#5843: We have .pid as 0 for now; worth trying to fill it in?
    record.exit.tid = tid;
    return record;
}

template <>
void
scheduler_tmpl_t<memref_t, reader_t>::print_record(const memref_t &record)
{
    fprintf(stderr, "tid=%" PRId64 " type=%d", record.instr.tid, record.instr.type);
    if (type_is_instr(record.instr.type))
        fprintf(stderr, " pc=0x%zx size=%zu", record.instr.addr, record.instr.size);
    else if (record.marker.type == TRACE_TYPE_MARKER) {
        fprintf(stderr, " marker=0x%d val=%zu", record.marker.marker_type,
                record.marker.marker_value);
    }
    fprintf(stderr, "\n");
}

/******************************************************************************
 * Specializations for scheduler_tmpl_t<record_reader_t>, aka record_scheduler_t.
 */

template <>
std::unique_ptr<dynamorio::drmemtrace::record_reader_t>
scheduler_tmpl_t<trace_entry_t, record_reader_t>::get_default_reader()
{
    return std::unique_ptr<default_record_file_reader_t>(
        new default_record_file_reader_t());
}

template <>
std::unique_ptr<dynamorio::drmemtrace::record_reader_t>
scheduler_tmpl_t<trace_entry_t, record_reader_t>::get_reader(const std::string &path,
                                                             int verbosity)
{
    // TODO i#5675: Add support for other file formats, particularly
    // .zip files.
    if (ends_with(path, ".sz") || ends_with(path, ".zip"))
        return nullptr;
    return std::unique_ptr<dynamorio::drmemtrace::record_reader_t>(
        new default_record_file_reader_t(path, verbosity));
}

template <>
bool
scheduler_tmpl_t<trace_entry_t, record_reader_t>::record_type_has_tid(
    trace_entry_t record, memref_tid_t &tid)
{
    if (record.type != TRACE_TYPE_THREAD)
        return false;
    tid = static_cast<memref_tid_t>(record.addr);
    return true;
}

template <>
bool
scheduler_tmpl_t<trace_entry_t, record_reader_t>::record_type_is_instr(
    trace_entry_t record)
{
    return type_is_instr(static_cast<trace_type_t>(record.type));
}

template <>
bool
scheduler_tmpl_t<trace_entry_t, record_reader_t>::record_type_is_marker(
    trace_entry_t record, trace_marker_type_t &type, uintptr_t &value)
{
    if (record.type != TRACE_TYPE_MARKER)
        return false;
    type = static_cast<trace_marker_type_t>(record.size);
    value = record.addr;
    return true;
}

template <>
bool
scheduler_tmpl_t<trace_entry_t, record_reader_t>::record_type_is_timestamp(
    trace_entry_t record, uintptr_t &value)
{
    if (record.type != TRACE_TYPE_MARKER ||
        static_cast<trace_marker_type_t>(record.size) != TRACE_MARKER_TYPE_TIMESTAMP)
        return false;
    value = record.addr;
    return true;
}

template <>
trace_entry_t
scheduler_tmpl_t<trace_entry_t, record_reader_t>::create_region_separator_marker(
    memref_tid_t tid, uintptr_t value)
{
    // We ignore the tid.
    trace_entry_t record;
    record.type = TRACE_TYPE_MARKER;
    record.size = TRACE_MARKER_TYPE_WINDOW_ID;
    record.addr = value;
    return record;
}

template <>
trace_entry_t
scheduler_tmpl_t<trace_entry_t, record_reader_t>::create_thread_exit(memref_tid_t tid)
{
    trace_entry_t record;
    record.type = TRACE_TYPE_THREAD_EXIT;
    record.size = sizeof(tid);
    record.addr = static_cast<addr_t>(tid);
    return record;
}

template <>
void
scheduler_tmpl_t<trace_entry_t, record_reader_t>::print_record(
    const trace_entry_t &record)
{
    fprintf(stderr, "type=%d size=%d addr=0x%zx\n", record.type, record.size,
            record.addr);
}

/***************************************************************************
 * Scheduled stream.
 */

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::next_record(RecordType &record)
{
    return next_record(record, 0);
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::next_record(RecordType &record,
                                                                uint64_t cur_time)
{
    input_info_t *input = nullptr;
    sched_type_t::stream_status_t res =
        scheduler_->next_record(ordinal_, record, input, cur_time);
    if (res != sched_type_t::STATUS_OK)
        return res;

    // Update our memtrace_stream_t state.
    std::lock_guard<std::mutex> guard(*input->lock);
    if (!input->reader->is_record_synthetic())
        ++cur_ref_count_;
    if (scheduler_->record_type_is_instr(record))
        ++cur_instr_count_;
    VPRINT(scheduler_, 4,
           "stream record#=%" PRId64 ", instr#=%" PRId64 " (cur input %" PRId64
           " record#=%" PRId64 ", instr#=%" PRId64 ")\n",
           cur_ref_count_, cur_instr_count_, input->tid,
           input->reader->get_record_ordinal(), input->reader->get_instruction_ordinal());

    // Update our header state.
    // If we skipped over these, advance_region_of_interest() sets them.
    // TODO i#5843: Check that all inputs have the same top-level headers here.
    // A possible exception is allowing warmup-phase-filtered traces to be mixed
    // with regular traces.
    trace_marker_type_t marker_type;
    uintptr_t marker_value;
    if (scheduler_->record_type_is_marker(record, marker_type, marker_value)) {
        switch (marker_type) {
        case TRACE_MARKER_TYPE_TIMESTAMP:
            last_timestamp_ = marker_value;
            if (first_timestamp_ == 0)
                first_timestamp_ = last_timestamp_;
            break;

        case TRACE_MARKER_TYPE_VERSION: version_ = marker_value; break;
        case TRACE_MARKER_TYPE_FILETYPE: filetype_ = marker_value; break;
        case TRACE_MARKER_TYPE_CACHE_LINE_SIZE: cache_line_size_ = marker_value; break;
        case TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT:
            chunk_instr_count_ = marker_value;
            break;
        case TRACE_MARKER_TYPE_PAGE_SIZE: page_size_ = marker_value; break;
        default: // No action needed.
            break;
        }
    }
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::start_speculation(
    addr_t start_address, bool queue_current_record)
{
    return scheduler_->start_speculation(ordinal_, start_address, queue_current_record);
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::stop_speculation()
{
    return scheduler_->stop_speculation(ordinal_);
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::set_active(bool active)
{
    return scheduler_->set_output_active(ordinal_, active);
}

/***************************************************************************
 * Scheduler.
 */

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_tmpl_t<RecordType, ReaderType>::init(
    std::vector<input_workload_t> &workload_inputs, int output_count,
    scheduler_options_t options)
{
    options_ = options;
    verbosity_ = options_.verbosity;
    // workload_inputs is not const so we can std::move readers out of it.
    std::unordered_map<int, std::vector<int>> workload2inputs(workload_inputs.size());
    for (int workload_idx = 0; workload_idx < static_cast<int>(workload_inputs.size());
         ++workload_idx) {
        auto &workload = workload_inputs[workload_idx];
        if (workload.struct_size != sizeof(input_workload_t))
            return STATUS_ERROR_INVALID_PARAMETER;
        std::unordered_map<memref_tid_t, int> workload_tids;
        if (workload.path.empty()) {
            if (workload.readers.empty())
                return STATUS_ERROR_INVALID_PARAMETER;
            for (auto &reader : workload.readers) {
                if (!reader.reader || !reader.end)
                    return STATUS_ERROR_INVALID_PARAMETER;
                if (!workload.only_threads.empty() &&
                    workload.only_threads.find(reader.tid) == workload.only_threads.end())
                    continue;
                int index = static_cast<input_ordinal_t>(inputs_.size());
                inputs_.emplace_back();
                input_info_t &input = inputs_.back();
                input.index = index;
                input.workload = workload_idx;
                workload2inputs[workload_idx].push_back(index);
                input.tid = reader.tid;
                input.reader = std::move(reader.reader);
                input.reader_end = std::move(reader.end);
                input.needs_init = true;
                workload_tids[input.tid] = input.index;
            }
        } else {
            if (!workload.readers.empty())
                return STATUS_ERROR_INVALID_PARAMETER;
            sched_type_t::scheduler_status_t res =
                open_readers(workload.path, workload.only_threads, workload_tids);
            if (res != STATUS_SUCCESS)
                return res;
            for (const auto &it : workload_tids) {
                inputs_[it.second].workload = workload_idx;
                workload2inputs[workload_idx].push_back(it.second);
            }
        }
        for (const auto &modifiers : workload.thread_modifiers) {
            if (modifiers.struct_size != sizeof(input_thread_info_t))
                return STATUS_ERROR_INVALID_PARAMETER;
            const std::vector<memref_tid_t> *which_tids;
            std::vector<memref_tid_t> workload_tid_vector;
            if (modifiers.tids.empty()) {
                // Apply to all tids that have not already been modified.
                for (const auto entry : workload_tids) {
                    if (!inputs_[entry.second].has_modifier)
                        workload_tid_vector.push_back(entry.first);
                }
                which_tids = &workload_tid_vector;
            } else
                which_tids = &modifiers.tids;
            // We assume the overhead of copying the modifiers for every thread is
            // not high and the simplified code is worthwhile.
            for (memref_tid_t tid : *which_tids) {
                if (workload_tids.find(tid) == workload_tids.end())
                    return STATUS_ERROR_INVALID_PARAMETER;
                int index = workload_tids[tid];
                input_info_t &input = inputs_[index];
                input.has_modifier = true;
                input.binding = modifiers.output_binding;
                input.priority = modifiers.priority;
                for (size_t i = 0; i < modifiers.regions_of_interest.size(); ++i) {
                    const auto &range = modifiers.regions_of_interest[i];
                    if (range.start_instruction == 0 ||
                        (range.stop_instruction < range.start_instruction &&
                         range.stop_instruction != 0))
                        return STATUS_ERROR_INVALID_PARAMETER;
                    if (i == 0)
                        continue;
                    if (range.start_instruction <=
                        modifiers.regions_of_interest[i - 1].stop_instruction)
                        return STATUS_ERROR_INVALID_PARAMETER;
                }
                input.regions_of_interest = modifiers.regions_of_interest;
            }
        }
    }

    if (TESTANY(sched_type_t::SCHEDULER_USE_SINGLE_INPUT_ORDINALS, options_.flags) &&
        inputs_.size() == 1 && output_count == 1) {
        options_.flags = static_cast<scheduler_flags_t>(
            static_cast<int>(options_.flags) |
            static_cast<int>(sched_type_t::SCHEDULER_USE_INPUT_ORDINALS));
    }

    // TODO i#5843: Once the speculator supports more options, change the
    // default.  For now we hardcode nops as the only supported option.
    options_.flags = static_cast<scheduler_flags_t>(
        static_cast<int>(options_.flags) |
        static_cast<int>(sched_type_t::SCHEDULER_SPECULATE_NOPS));

    outputs_.reserve(output_count);
    for (int i = 0; i < output_count; ++i) {
        outputs_.emplace_back(this, i,
                              TESTANY(SCHEDULER_SPECULATE_NOPS, options_.flags)
                                  ? spec_type_t::USE_NOPS
                                  // TODO i#5843: Add more flags for other options.
                                  : spec_type_t::LAST_FROM_TRACE,
                              verbosity_);
        if (options_.schedule_record_ostream != nullptr) {
            sched_type_t::stream_status_t status = record_schedule_segment(
                i, schedule_record_t::VERSION, schedule_record_t::VERSION_CURRENT, 0, 0);
            if (status != sched_type_t::STATUS_OK) {
                error_string_ = "Failed to add version to recorded schedule";
                return STATUS_ERROR_FILE_WRITE_FAILED;
            }
        }
    }
    return set_initial_schedule(workload2inputs);
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_tmpl_t<RecordType, ReaderType>::set_initial_schedule(
    std::unordered_map<int, std::vector<int>> &workload2inputs)
{
    if (options_.mapping == MAP_AS_PREVIOUSLY) {
        if (options_.schedule_replay_istream == nullptr ||
            options_.schedule_record_ostream != nullptr)
            return STATUS_ERROR_INVALID_PARAMETER;
        sched_type_t::scheduler_status_t status = read_recorded_schedule();
        if (status != sched_type_t::STATUS_SUCCESS)
            return STATUS_ERROR_INVALID_PARAMETER;
        if (options_.deps == DEPENDENCY_TIMESTAMPS) {
            // Match the ordinals from the original run by pre-reading the timestamps.
            sched_type_t::scheduler_status_t res = get_initial_timestamps();
            if (res != STATUS_SUCCESS)
                return res;
        }
    } else if (options_.schedule_replay_istream != nullptr) {
        return STATUS_ERROR_INVALID_PARAMETER;
    } else if (options_.mapping == MAP_TO_CONSISTENT_OUTPUT) {
        // Assign the inputs up front to avoid locks once we're in parallel mode.
        // We use a simple round-robin static assignment for now.
        for (int i = 0; i < static_cast<input_ordinal_t>(inputs_.size()); ++i) {
            size_t index = i % outputs_.size();
            if (outputs_[index].input_indices.empty())
                set_cur_input(static_cast<input_ordinal_t>(index), i);
            outputs_[index].input_indices.push_back(i);
            VPRINT(this, 2, "Assigning input #%d to output #%zd\n", i, index);
        }
    } else if (options_.mapping == MAP_TO_RECORDED_OUTPUT) {
        if (options_.replay_as_traced_istream != nullptr) {
            // Even for just one output we honor a request to replay the schedule
            // (although it should match the analyzer serial mode so there's no big
            // benefit to reading the schedule file.  The analyzer serial mode or other
            // special cases of one output don't set the replay_as_traced_istream
            // field.)
            sched_type_t::scheduler_status_t status = read_traced_schedule();
            if (status != sched_type_t::STATUS_SUCCESS)
                return STATUS_ERROR_INVALID_PARAMETER;
            // Now leverage the regular replay code.
            options_.mapping = MAP_AS_PREVIOUSLY;
        } else if (outputs_.size() > 1) {
            return STATUS_ERROR_INVALID_PARAMETER;
        } else if (inputs_.size() == 1) {
            set_cur_input(0, 0);
        } else {
            // The old file_reader_t interleaving would output the top headers for every
            // thread first and then pick the oldest timestamp once it reached a
            // timestamp. We instead queue those headers so we can start directly with the
            // oldest timestamp's thread.
            sched_type_t::scheduler_status_t res = get_initial_timestamps();
            if (res != STATUS_SUCCESS)
                return res;
            uint64_t min_time = std::numeric_limits<uint64_t>::max();
            input_ordinal_t min_input = -1;
            for (int i = 0; i < static_cast<input_ordinal_t>(inputs_.size()); ++i) {
                if (inputs_[i].next_timestamp < min_time) {
                    min_time = inputs_[i].next_timestamp;
                    min_input = i;
                }
            }
            if (min_input < 0)
                return STATUS_ERROR_INVALID_PARAMETER;
            set_cur_input(0, static_cast<input_ordinal_t>(min_input));
        }
    } else {
        // Assign initial inputs.
        if (options_.deps == DEPENDENCY_TIMESTAMPS) {
            sched_type_t::scheduler_status_t res = get_initial_timestamps();
            if (res != STATUS_SUCCESS)
                return res;
            // Compute the min timestamp (==base_timestamp) per workload and sort
            // all inputs by relative time from the base.
            for (int workload_idx = 0;
                 workload_idx < static_cast<int>(workload2inputs.size());
                 ++workload_idx) {
                uint64_t min_time = std::numeric_limits<uint64_t>::max();
                input_ordinal_t min_input = -1;
                for (int input_idx : workload2inputs[workload_idx]) {
                    if (inputs_[input_idx].next_timestamp < min_time) {
                        min_time = inputs_[input_idx].next_timestamp;
                        min_input = input_idx;
                    }
                }
                if (min_input < 0)
                    return STATUS_ERROR_INVALID_PARAMETER;
                for (int input_idx : workload2inputs[workload_idx]) {
                    VPRINT(this, 4,
                           "workload %d: setting input %d base_timestamp to %" PRIu64
                           " vs next_timestamp %zu\n",
                           workload_idx, input_idx, min_time,
                           inputs_[input_idx].next_timestamp);
                    inputs_[input_idx].base_timestamp = min_time;
                    inputs_[input_idx].order_by_timestamp = true;
                }
            }
            // Pick the starting inputs by sorting by relative time from each workload's
            // base_timestamp, which our queue does for us.  We want the rest of the
            // inputs in the queue in any case so it is simplest to insert all and
            // remove the first N rather than sorting the first N separately.
            for (int i = 0; i < static_cast<input_ordinal_t>(inputs_.size()); ++i) {
                add_to_ready_queue(&inputs_[i]);
            }
            for (int i = 0; i < static_cast<output_ordinal_t>(outputs_.size()); ++i) {
                if (i < static_cast<input_ordinal_t>(inputs_.size())) {
                    input_info_t *queue_next = pop_from_ready_queue(i);
                    if (queue_next == nullptr)
                        set_cur_input(i, INVALID_INPUT_ORDINAL);
                    else
                        set_cur_input(i, queue_next->index);
                } else
                    set_cur_input(i, INVALID_INPUT_ORDINAL);
            }
        } else {
            // Just take the 1st N inputs (even if all from the same workload).
            for (int i = 0; i < static_cast<output_ordinal_t>(outputs_.size()); ++i) {
                if (i < static_cast<input_ordinal_t>(inputs_.size()))
                    set_cur_input(i, i);
                else
                    set_cur_input(i, INVALID_INPUT_ORDINAL);
            }
            for (int i = static_cast<output_ordinal_t>(outputs_.size());
                 i < static_cast<input_ordinal_t>(inputs_.size()); ++i) {
                add_to_ready_queue(&inputs_[i]);
            }
        }
    }
    return STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
std::string
scheduler_tmpl_t<RecordType, ReaderType>::recorded_schedule_component_name(
    output_ordinal_t output)
{
    static const char *const SCHED_CHUNK_PREFIX = "output.";
    std::ostringstream name;
    name << SCHED_CHUNK_PREFIX << std::setfill('0') << std::setw(4) << output;
    return name.str();
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_tmpl_t<RecordType, ReaderType>::write_recorded_schedule()
{
    if (options_.schedule_record_ostream == nullptr)
        return STATUS_ERROR_INVALID_PARAMETER;
    std::lock_guard<std::mutex> guard(sched_lock_);
    for (int i = 0; i < static_cast<int>(outputs_.size()); ++i) {
        sched_type_t::stream_status_t status =
            record_schedule_segment(i, schedule_record_t::FOOTER, 0, 0, 0);
        if (status != sched_type_t::STATUS_OK)
            return STATUS_ERROR_FILE_WRITE_FAILED;
        std::string name = recorded_schedule_component_name(i);
        std::string err = options_.schedule_record_ostream->open_new_component(name);
        if (!err.empty()) {
            VPRINT(this, 1, "Failed to open component %s in record file: %s\n",
                   name.c_str(), err.c_str());
            return STATUS_ERROR_FILE_WRITE_FAILED;
        }
        if (!options_.schedule_record_ostream->write(
                reinterpret_cast<char *>(outputs_[i].record.data()),
                outputs_[i].record.size() * sizeof(outputs_[i].record[0])))
            return STATUS_ERROR_FILE_WRITE_FAILED;
    }
    return STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_tmpl_t<RecordType, ReaderType>::read_recorded_schedule()
{
    if (options_.schedule_replay_istream == nullptr)
        return STATUS_ERROR_INVALID_PARAMETER;

    schedule_record_t record;
    // We assume we can easily fit the whole context switch sequence in memory.
    // If that turns out not to be the case for very long traces, we deliberately
    // used an archive format so we could do parallel incremental reads.
    // (Conversely, if we want to commit to storing in memory, we could use a
    // non-archive format and store the output ordinal in the version record.)
    for (int i = 0; i < static_cast<int>(outputs_.size()); ++i) {
        std::string err = options_.schedule_replay_istream->open_component(
            recorded_schedule_component_name(i));
        if (!err.empty()) {
            error_string_ = "Failed to open schedule_replay_istream component " +
                recorded_schedule_component_name(i) + ": " + err;
            return STATUS_ERROR_INVALID_PARAMETER;
        }
        // XXX: This could be made more efficient if we stored the record count
        // in the version field's stop_instruction field or something so we can
        // size the vector up front.  As this only happens once we do not bother
        // and live with a few vector resizes.
        bool saw_footer = false;
        while (options_.schedule_replay_istream->read(reinterpret_cast<char *>(&record),
                                                      sizeof(record))) {
            if (record.type == schedule_record_t::VERSION) {
                if (record.key.version != schedule_record_t::VERSION_CURRENT)
                    return STATUS_ERROR_INVALID_PARAMETER;
            } else if (record.type == schedule_record_t::FOOTER) {
                saw_footer = true;
                break;
            } else
                outputs_[i].record.push_back(record);
        }
        if (!saw_footer) {
            error_string_ = "Record file missing footer";
            return STATUS_ERROR_INVALID_PARAMETER;
        }
        VPRINT(this, 1, "Read %zu recorded records for output #%d\n",
               outputs_[i].record.size(), i);
    }
    // See if there was more data in the file (we do this after reading to not
    // mis-report i/o or path errors as this error).
    std::string err = options_.schedule_replay_istream->open_component(
        recorded_schedule_component_name(static_cast<output_ordinal_t>(outputs_.size())));
    if (err.empty()) {
        error_string_ = "Not enough output streams for recorded file";
        return STATUS_ERROR_INVALID_PARAMETER;
    }
    for (int i = 0; i < static_cast<output_ordinal_t>(outputs_.size()); ++i) {
        if (!outputs_[i].record.empty()) {
            set_cur_input(i, outputs_[i].record[0].key.input);
        } else
            set_cur_input(i, INVALID_INPUT_ORDINAL);
    }
    return STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_tmpl_t<RecordType, ReaderType>::read_traced_schedule()
{
    if (options_.replay_as_traced_istream == nullptr)
        return STATUS_ERROR_INVALID_PARAMETER;

    schedule_entry_t entry(0, 0, 0, 0);
    // See comment in read_recorded_schedule() on our assumption that we can
    // easily fit the whole context switch sequence in memory.  This cpu_schedule
    // file has an entry per timestamp, though, even for consecutive ones on the same
    // core, so it uses more memory.
    // We do not have a subfile listing feature in archive_istream_t, but we can
    // read sequentially as each record has a cpu field.
    // This schedule_entry_t format doesn't have the stop instruction ordinal (as it was
    // designed for skip targets only), so we take two passes to get that information.
    // If we do find memory is an issue we could add a stop field to schedule_entry_t
    // and collapse as we go, saving memory.
    // We also need to translate the thread and cpu id values into 0-based ordinals.
    std::unordered_map<memref_tid_t, input_ordinal_t> tid2input;
    for (int i = 0; i < static_cast<input_ordinal_t>(inputs_.size()); ++i) {
        tid2input[inputs_[i].tid] = i;
    }
    std::vector<std::set<uint64_t>> start2stop(inputs_.size());
    // We number the outputs according to their order in the file.
    // XXX i#5843: Should we support some direction from the user on this?  Simulation
    // may want to preserve the NUMA relationships and may need to set up its simulated
    // cores at init time, so it would prefer to partition by output stream identifier.
    // Maybe we could at least add the proposed memtrace_stream_t query for cpuid and
    // let it be called even before reading any records at all?
    output_ordinal_t cur_output = 0;
    uint64_t cur_cpu = std::numeric_limits<uint64_t>::max();
    // We also want to collapse same-cpu consecutive records so we start with
    // a temporary local vector.
    std::vector<std::vector<schedule_record_t>> all_sched(outputs_.size());
    // Work around i#6107 by tracking counts sorted by timestamp for each input.
    std::vector<std::vector<schedule_record_t>> input_sched(inputs_.size());
    while (options_.replay_as_traced_istream->read(reinterpret_cast<char *>(&entry),
                                                   sizeof(entry))) {
        if (entry.cpu != cur_cpu) {
            if (cur_cpu != std::numeric_limits<uint64_t>::max()) {
                ++cur_output;
                if (cur_output >= static_cast<int>(outputs_.size())) {
                    error_string_ = "replay_as_traced_istream cpu count != output count";
                    return STATUS_ERROR_INVALID_PARAMETER;
                }
            }
            cur_cpu = entry.cpu;
        }
        input_ordinal_t input = tid2input[entry.thread];
        // We'll fill in the stop ordinal in our second pass below.
        uint64_t start = entry.start_instruction;
        uint64_t timestamp = entry.timestamp;
        // Some entries have no instructions (there is an entry for each timestamp, and
        // a signal can come in after a prior timestamp with no intervening instrs).
        if (!all_sched[cur_output].empty() &&
            input == all_sched[cur_output].back().key.input &&
            start == all_sched[cur_output].back().start_instruction) {
            VPRINT(this, 3,
                   "Output #%d: as-read segment #%zu has no instructions: skipping\n",
                   cur_output, all_sched[cur_output].size() - 1);
            continue;
        }
        all_sched[cur_output].emplace_back(schedule_record_t::DEFAULT, input, start, 0,
                                           timestamp);
        start2stop[input].insert(start);
        input_sched[input].emplace_back(schedule_record_t::DEFAULT, input, start, 0,
                                        timestamp);
    }
    sched_type_t::scheduler_status_t res =
        check_and_fix_modulo_problem_in_schedule(input_sched, start2stop, all_sched);
    if (res != sched_type_t::STATUS_SUCCESS)
        return res;
    for (int output_idx = 0; output_idx < static_cast<output_ordinal_t>(outputs_.size());
         ++output_idx) {
        VPRINT(this, 1, "Read %zu as-traced records for output #%d\n",
               all_sched[output_idx].size(), output_idx);
        // Update the stop_instruction field and collapse consecutive entries while
        // inserting into the final location.
        int start_consec = -1;
        for (int sched_idx = 0;
             sched_idx < static_cast<int>(all_sched[output_idx].size()); ++sched_idx) {
            auto &segment = all_sched[output_idx][sched_idx];
            auto find = start2stop[segment.key.input].find(segment.start_instruction);
            ++find;
            if (find == start2stop[segment.key.input].end())
                segment.stop_instruction = std::numeric_limits<uint64_t>::max();
            else
                segment.stop_instruction = *find;
            VPRINT(this, 4,
                   "as-read segment #%d: input=%d start=%" PRId64 " stop=%" PRId64
                   " time=%" PRId64 "\n",
                   sched_idx, segment.key.input, segment.start_instruction,
                   segment.stop_instruction, segment.timestamp);
            if (sched_idx + 1 < static_cast<int>(all_sched[output_idx].size()) &&
                segment.key.input == all_sched[output_idx][sched_idx + 1].key.input &&
                segment.stop_instruction >
                    all_sched[output_idx][sched_idx + 1].start_instruction) {
                // A second sanity check.
                error_string_ = "Invalid decreasing start field in schedule file";
                return STATUS_ERROR_INVALID_PARAMETER;
            } else if (sched_idx + 1 < static_cast<int>(all_sched[output_idx].size()) &&
                       segment.key.input ==
                           all_sched[output_idx][sched_idx + 1].key.input &&
                       segment.stop_instruction ==
                           all_sched[output_idx][sched_idx + 1].start_instruction) {
                // Collapse into next.
                if (start_consec == -1)
                    start_consec = sched_idx;
            } else {
                schedule_record_t &toadd = start_consec >= 0
                    ? all_sched[output_idx][start_consec]
                    : all_sched[output_idx][sched_idx];
                outputs_[output_idx].record.emplace_back(
                    static_cast<typename schedule_record_t::record_type_t>(toadd.type),
                    +toadd.key.input, +toadd.start_instruction,
                    +all_sched[output_idx][sched_idx].stop_instruction, +toadd.timestamp);
                start_consec = -1;
                VDO(this, 3, {
                    auto &added = outputs_[output_idx].record.back();
                    VPRINT(this, 3,
                           "segment #%zu: input=%d start=%" PRId64 " stop=%" PRId64
                           " time=%" PRId64 "\n",
                           outputs_[output_idx].record.size() - 1, added.key.input,
                           added.start_instruction, added.stop_instruction,
                           added.timestamp);
                });
            }
        }
        VPRINT(this, 1, "Collapsed duplicates for %zu as-traced records for output #%d\n",
               outputs_[output_idx].record.size(), output_idx);
        if (!outputs_[output_idx].record.empty()) {
            if (outputs_[output_idx].record[0].start_instruction != 0) {
                VPRINT(this, 1, "Initial input for output #%d is: wait state\n",
                       output_idx);
                set_cur_input(output_idx, INVALID_INPUT_ORDINAL);
                outputs_[output_idx].waiting = true;
                outputs_[output_idx].record_index = -1;
            } else {
                VPRINT(this, 1, "Initial input for output #%d is %d\n", output_idx,
                       outputs_[output_idx].record[0].key.input);
                set_cur_input(output_idx, outputs_[output_idx].record[0].key.input);
            }
        } else
            set_cur_input(output_idx, INVALID_INPUT_ORDINAL);
    }
    return STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_tmpl_t<RecordType, ReaderType>::check_and_fix_modulo_problem_in_schedule(
    std::vector<std::vector<schedule_record_t>> &input_sched,
    std::vector<std::set<uint64_t>> &start2stop,
    std::vector<std::vector<schedule_record_t>> &all_sched)

{
    // Work around i#6107 where the counts in the file are incorrectly modulo the chunk
    // size.  Unfortunately we need to construct input_sched and sort it for each input
    // in order to even detect this issue; we could bump the trace version to let us
    // know it's not present if these steps become overhead concerns.

    // We store the actual instruction count for each timestamp, for each input, keyed
    // by timestamp so we can look it up when iterating over the per-cpu schedule.  We
    // do not support consecutive identical timestamps in one input for this workaround.
    std::vector<std::unordered_map<uint64_t, uint64_t>> timestamp2adjust(inputs_.size());

    // We haven't read into the trace far enough to find the actual chunk size, so for
    // this workaround we only support what was the default in raw2trace up to this
    // point, 10M.
    static constexpr uint64_t DEFAULT_CHUNK_SIZE = 10 * 1000 * 1000;

    // For each input, sort and walk the schedule and look for decreasing counts.
    // Construct timestamp2adjust so we can fix the other data structures if necessary.
    bool found_i6107 = false;
    for (int input_idx = 0; input_idx < static_cast<input_ordinal_t>(inputs_.size());
         ++input_idx) {
        std::sort(input_sched[input_idx].begin(), input_sched[input_idx].end(),
                  [](const schedule_record_t &l, const schedule_record_t &r) {
                      return l.timestamp < r.timestamp;
                  });
        uint64_t prev_start = 0;
        uint64_t add_to_start = 0;
        bool in_order = true;
        for (const schedule_record_t &sched : input_sched[input_idx]) {
            if (sched.start_instruction < prev_start) {
                // If within 50% of the end of the chunk we assume it's i#6107.
                if (prev_start * 2 > DEFAULT_CHUNK_SIZE) {
                    add_to_start += DEFAULT_CHUNK_SIZE;
                    if (in_order) {
                        VPRINT(this, 2, "Working around i#6107 for input #%d\n",
                               input_idx);
                        in_order = false;
                        found_i6107 = true;
                    }
                } else {
                    error_string_ = "Invalid decreasing start field in schedule file";
                    return STATUS_ERROR_INVALID_PARAMETER;
                }
            }
            // We could save space by not storing the early ones but we do need to
            // include all duplicates.
            if (timestamp2adjust[input_idx].find(sched.timestamp) !=
                timestamp2adjust[input_idx].end()) {
                error_string_ = "Same timestamps not supported for i#6107 workaround";
                return STATUS_ERROR_INVALID_PARAMETER;
            }
            prev_start = sched.start_instruction;
            timestamp2adjust[input_idx][sched.timestamp] =
                sched.start_instruction + add_to_start;
        }
    }
    if (!found_i6107)
        return STATUS_SUCCESS;
    // Rebuild start2stop.
    for (int input_idx = 0; input_idx < static_cast<input_ordinal_t>(inputs_.size());
         ++input_idx) {
        start2stop[input_idx].clear();
        for (auto &keyval : timestamp2adjust[input_idx]) {
            start2stop[input_idx].insert(keyval.second);
        }
    }
    // Update all_sched.
    for (int output_idx = 0; output_idx < static_cast<output_ordinal_t>(outputs_.size());
         ++output_idx) {
        for (int sched_idx = 0;
             sched_idx < static_cast<int>(all_sched[output_idx].size()); ++sched_idx) {
            auto &segment = all_sched[output_idx][sched_idx];
            auto it = timestamp2adjust[segment.key.input].find(segment.timestamp);
            if (it == timestamp2adjust[segment.key.input].end()) {
                error_string_ = "Failed to find timestamp for i#6107 workaround";
                return STATUS_ERROR_INVALID_PARAMETER;
            }
            assert(it->second >= segment.start_instruction);
            assert(it->second % DEFAULT_CHUNK_SIZE == segment.start_instruction);
            if (it->second != segment.start_instruction) {
                VPRINT(this, 2,
                       "Updating all_sched[%d][%d] input %d from %" PRId64 " to %" PRId64
                       "\n",
                       output_idx, sched_idx, segment.key.input,
                       segment.start_instruction, it->second);
            }
            segment.start_instruction = it->second;
        }
    }
    return STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_tmpl_t<RecordType, ReaderType>::get_initial_timestamps()
{
    // Read ahead in each input until we find a timestamp record.
    // Queue up any skipped records to ensure we present them to the
    // output stream(s).
    for (size_t i = 0; i < inputs_.size(); ++i) {
        input_info_t &input = inputs_[i];
        if (input.next_timestamp <= 0) {
            for (const auto &record : input.queue) {
                if (record_type_is_timestamp(record, input.next_timestamp))
                    break;
            }
        }
        if (input.next_timestamp <= 0) {
            if (input.needs_init) {
                input.reader->init();
                input.needs_init = false;
            }
            while (input.reader != input.reader_end) {
                RecordType record = **input.reader;
                if (record_type_is_timestamp(record, input.next_timestamp))
                    break;
                // If we see an instruction, there may be no timestamp (a malformed
                // synthetic trace in a test) or we may have to read thousands of records
                // to find it if it were somehow missing, which we do not want to do.  We
                // assume our queued records are few and do not include instructions when
                // we skip (see skip_instrutions()).  Thus, we abort with an error.
                if (record_type_is_instr(record))
                    break;
                input.queue.push_back(record);
                ++(*input.reader);
            }
        }
        if (input.next_timestamp <= 0)
            return STATUS_ERROR_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_tmpl_t<RecordType, ReaderType>::open_reader(
    const std::string &path, const std::set<memref_tid_t> &only_threads,
    std::unordered_map<memref_tid_t, int> &workload_tids)
{
    if (path.empty() || directory_iterator_t::is_directory(path))
        return STATUS_ERROR_INVALID_PARAMETER;
    std::unique_ptr<ReaderType> reader = get_reader(path, verbosity_);
    if (!reader || !reader->init()) {
        error_string_ += "Failed to open " + path;
        return STATUS_ERROR_FILE_OPEN_FAILED;
    }
    int index = static_cast<input_ordinal_t>(inputs_.size());
    inputs_.emplace_back();
    input_info_t &input = inputs_.back();
    input.index = index;
    // We need the tid up front.  Rather than assume it's still part of the filename, we
    // read the first record (we generalize to read until we find the first but we
    // expect it to be the first after PR #5739 changed the order file_reader_t passes
    // them to reader_t) to find it.
    std::unique_ptr<ReaderType> reader_end = get_default_reader();
    memref_tid_t tid = INVALID_THREAD_ID;
    while (reader != reader_end) {
        RecordType record = **reader;
        if (record_type_has_tid(record, tid))
            break;
        input.queue.push_back(record);
        ++(*reader);
    }
    if (tid == INVALID_THREAD_ID) {
        error_string_ = "Failed to read " + path;
        return STATUS_ERROR_FILE_READ_FAILED;
    }
    if (!only_threads.empty() && only_threads.find(tid) == only_threads.end()) {
        inputs_.pop_back();
        return sched_type_t::STATUS_SUCCESS;
    }
    VPRINT(this, 1, "Opened reader for tid %" PRId64 " %s\n", tid, path.c_str());
    input.tid = tid;
    input.reader = std::move(reader);
    input.reader_end = std::move(reader_end);
    workload_tids[tid] = index;
    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_tmpl_t<RecordType, ReaderType>::open_readers(
    const std::string &path, const std::set<memref_tid_t> &only_threads,
    std::unordered_map<memref_tid_t, int> &workload_tids)
{
    if (!directory_iterator_t::is_directory(path))
        return open_reader(path, only_threads, workload_tids);
    directory_iterator_t end;
    directory_iterator_t iter(path);
    if (!iter) {
        error_string_ = "Failed to list directory " + path + ": " + iter.error_string();
        return sched_type_t::STATUS_ERROR_FILE_OPEN_FAILED;
    }
    for (; iter != end; ++iter) {
        const std::string fname = *iter;
        if (fname == "." || fname == ".." ||
            starts_with(fname, DRMEMTRACE_SERIAL_SCHEDULE_FILENAME) ||
            fname == DRMEMTRACE_CPU_SCHEDULE_FILENAME)
            continue;
        // Skip the auxiliary files.
        if (fname == DRMEMTRACE_MODULE_LIST_FILENAME ||
            fname == DRMEMTRACE_FUNCTION_LIST_FILENAME ||
            fname == DRMEMTRACE_ENCODING_FILENAME)
            continue;
        const std::string file = path + DIRSEP + fname;
        sched_type_t::scheduler_status_t res =
            open_reader(file, only_threads, workload_tids);
        if (res != sched_type_t::STATUS_SUCCESS)
            return res;
    }
    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
std::string
scheduler_tmpl_t<RecordType, ReaderType>::get_input_name(output_ordinal_t output)
{
    int index = outputs_[output].cur_input;
    if (index < 0)
        return "";
    return inputs_[index].reader->get_stream_name();
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::input_ordinal_t
scheduler_tmpl_t<RecordType, ReaderType>::get_input_ordinal(output_ordinal_t output)
{
    return outputs_[output].cur_input;
}

template <typename RecordType, typename ReaderType>
int
scheduler_tmpl_t<RecordType, ReaderType>::get_workload_ordinal(output_ordinal_t output)
{
    if (output < 0 || output >= static_cast<output_ordinal_t>(outputs_.size()))
        return -1;
    if (outputs_[output].cur_input < 0)
        return -1;
    return inputs_[outputs_[output].cur_input].workload;
}

template <typename RecordType, typename ReaderType>
bool
scheduler_tmpl_t<RecordType, ReaderType>::is_record_synthetic(output_ordinal_t output)
{
    int index = outputs_[output].cur_input;
    if (index < 0)
        return false;
    return inputs_[index].reader->is_record_synthetic();
}

template <typename RecordType, typename ReaderType>
memtrace_stream_t *
scheduler_tmpl_t<RecordType, ReaderType>::get_input_stream(output_ordinal_t output)
{
    if (output < 0 || output >= static_cast<output_ordinal_t>(outputs_.size()))
        return nullptr;
    int index = outputs_[output].cur_input;
    if (index < 0)
        return nullptr;
    return inputs_[index].reader.get();
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::advance_region_of_interest(
    output_ordinal_t output, RecordType &record, input_info_t &input)
{
    uint64_t cur_instr = input.reader->get_instruction_ordinal();
    assert(input.cur_region >= 0 &&
           input.cur_region < static_cast<int>(input.regions_of_interest.size()));
    auto &cur_range = input.regions_of_interest[input.cur_region];
    // Look for the end of the current range.
    if (input.in_cur_region && cur_range.stop_instruction != 0 &&
        cur_instr > cur_range.stop_instruction) {
        ++input.cur_region;
        input.in_cur_region = false;
        VPRINT(this, 2, "at %" PRId64 " instrs: advancing to ROI #%d\n", cur_instr,
               input.cur_region);
        if (input.cur_region >= static_cast<int>(input.regions_of_interest.size())) {
            if (input.at_eof)
                return sched_type_t::STATUS_EOF;
            else {
                // We let the user know we're done.
                if (options_.schedule_record_ostream != nullptr) {
                    sched_type_t::stream_status_t status =
                        close_schedule_segment(output, input);
                    if (status != sched_type_t::STATUS_OK)
                        return status;
                    // Indicate we need a synthetic thread exit on replay.
                    status =
                        record_schedule_segment(output, schedule_record_t::SYNTHETIC_END,
                                                input.index, cur_instr, 0);
                    if (status != sched_type_t::STATUS_OK)
                        return status;
                }
                input.queue.push_back(create_thread_exit(input.tid));
                input.at_eof = true;
                return sched_type_t::STATUS_SKIPPED;
            }
        }
        cur_range = input.regions_of_interest[input.cur_region];
    }

    if (!input.in_cur_region && cur_instr >= cur_range.start_instruction) {
        // We're already there (back-to-back regions).
        input.in_cur_region = true;
        // Even though there's no gap we let the user know we're on a new region.
        if (input.cur_region > 0) {
            VPRINT(this, 3, "skip_instructions input=%d: inserting separator marker\n",
                   input.index);
            input.queue.push_back(record);
            record = create_region_separator_marker(input.tid, input.cur_region);
        }
        return sched_type_t::STATUS_OK;
    }
    // If we're within one and already skipped, just exit to avoid re-requesting a skip
    // and making no progress (we're on the inserted timetamp + cpuid and our cur instr
    // count isn't yet the target).
    if (input.in_cur_region && cur_instr >= cur_range.start_instruction - 1)
        return sched_type_t::STATUS_OK;

    VPRINT(this, 2, "skipping from %" PRId64 " to %" PRId64 " instrs for ROI\n",
           cur_instr, cur_range.start_instruction);
    if (options_.schedule_record_ostream != nullptr) {
        sched_type_t::stream_status_t status = close_schedule_segment(output, input);
        if (status != sched_type_t::STATUS_OK)
            return status;
        status = record_schedule_segment(output, schedule_record_t::SKIP, input.index,
                                         cur_instr, cur_range.start_instruction);
        if (status != sched_type_t::STATUS_OK)
            return status;
        status = record_schedule_segment(output, schedule_record_t::DEFAULT, input.index,
                                         cur_range.start_instruction);
        if (status != sched_type_t::STATUS_OK)
            return status;
    }
    return skip_instructions(output, input, cur_range.start_instruction - cur_instr - 1);
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::skip_instructions(output_ordinal_t output,
                                                            input_info_t &input,
                                                            uint64_t skip_amount)
{
    // We assume the queue contains no instrs (else our query of input.reader's
    // instr ordinal would include them and so be incorrect) and that we should
    // thus skip it all.
    while (!input.queue.empty()) {
        assert(!record_type_is_instr(input.queue.front()));
        input.queue.pop_front();
    }
    input.reader->skip_instructions(skip_amount);
    if (*input.reader == *input.reader_end) {
        // Raise error because the input region is out of bounds.
        input.at_eof = true;
        return sched_type_t::STATUS_REGION_INVALID;
    }
    input.in_cur_region = true;
    auto &stream = outputs_[output].stream;

    // We've documented that an output stream's ordinals ignore skips in its input
    // streams, so we do not need to remember the input's ordinals pre-skip and increase
    // our output's ordinals commensurately post-skip.

    // If we skipped from the start we may not have seen the initial headers:
    // use the input's cached copies.
    if (stream.version_ == 0) {
        stream.version_ = input.reader->get_version();
        stream.last_timestamp_ = input.reader->get_last_timestamp();
        stream.first_timestamp_ = input.reader->get_first_timestamp();
        stream.filetype_ = input.reader->get_filetype();
        stream.cache_line_size_ = input.reader->get_cache_line_size();
        stream.chunk_instr_count_ = input.reader->get_chunk_instr_count();
        stream.page_size_ = input.reader->get_page_size();
    }
    // We let the user know we've skipped.  There's no discontinuity for the
    // first one so we do not insert a marker there (if we do want to insert one,
    // we need to update the view tool to handle a window marker as the very
    // first entry).
    if (input.cur_region > 0) {
        VPRINT(this, 3, "skip_instructions input=%d: inserting separator marker\n",
               input.index);
        input.queue.push_back(
            create_region_separator_marker(input.tid, input.cur_region));
    }
    return sched_type_t::STATUS_SKIPPED;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::record_schedule_segment(
    output_ordinal_t output, typename schedule_record_t::record_type_t type,
    input_ordinal_t input, uint64_t start_instruction, uint64_t stop_instruction)
{
    uint64_t timestamp;
    // XXX i#5843: Should we just use dr_get_microseconds() and avoid split-OS support
    // inside here?  We will be pulling in drdecode at least for identifying blocking
    // syscalls so maybe full DR isn't much more since we're often linked with raw2trace
    // which already needs it.  If we do we can remove the headers for this code too.
#ifdef UNIX
    struct timeval time;
    if (gettimeofday(&time, nullptr) != 0)
        return sched_type_t::STATUS_RECORD_FAILED;
    timestamp = time.tv_sec * 1000000 + time.tv_usec;
#else
    SYSTEMTIME sys_time;
    GetSystemTime(&sys_time);
    FILETIME file_time;
    if (!SystemTimeToFileTime(&sys_time, &file_time))
        return sched_type_t::STATUS_RECORD_FAILED;
    timestamp =
        file_time.dwLowDateTime + (static_cast<uint64_t>(file_time.dwHighDateTime) << 32);
#endif
    outputs_[output].record.emplace_back(type, input, start_instruction, stop_instruction,
                                         timestamp);
    // The stop is typically updated later in close_schedule_segment().
    if (type == schedule_record_t::DEFAULT) {
        inputs_[input].recorded_in_schedule = true;
    }
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::close_schedule_segment(output_ordinal_t output,
                                                                 input_info_t &input)
{
    assert(output >= 0 && output < static_cast<output_ordinal_t>(outputs_.size()));
    assert(!outputs_[output].record.empty());
    if (outputs_[output].record.back().type == schedule_record_t::SKIP) {
        // Skips already have a final stop value.
        return sched_type_t::STATUS_OK;
    }
    uint64_t instr_ord = input.reader->get_instruction_ordinal();
    if (input.at_eof || *input.reader == *input.reader_end) {
        // The end is exclusive, so use the max int value.
        instr_ord = std::numeric_limits<uint64_t>::max();
    }
    if (input.switching_pre_instruction) {
        input.switching_pre_instruction = false;
        // We aren't switching after reading a new instruction that we do not pass
        // to the consumer, so to have an exclusive stop instr ordinal we need +1.
        VPRINT(
            this, 3,
            "set_cur_input: +1 to instr_ord for not-yet-processed instr for input=%d\n",
            input.index);
        ++instr_ord;
    }
    VPRINT(this, 3,
           "close_schedule_segment: input=%d start=%" PRId64 " stop=%" PRId64 "\n",
           input.index, outputs_[output].record.back().start_instruction, instr_ord);
    outputs_[output].record.back().stop_instruction = instr_ord;
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
bool
scheduler_tmpl_t<RecordType, ReaderType>::ready_queue_empty()
{
    return ready_priority_.empty();
}

template <typename RecordType, typename ReaderType>
void
scheduler_tmpl_t<RecordType, ReaderType>::add_to_ready_queue(input_info_t *input)
{
    VPRINT(
        this, 4,
        "add_to_ready_queue (pre-size %zu): input %d priority %d timestamp delta %" PRIu64
        "\n",
        ready_priority_.size(), input->index, input->priority,
        input->reader->get_last_timestamp() - input->base_timestamp);
    input->queue_counter = ++ready_counter_;
    ready_priority_.push(input);
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::input_info_t *
scheduler_tmpl_t<RecordType, ReaderType>::pop_from_ready_queue(
    output_ordinal_t for_output)
{
    std::set<input_info_t *> skipped;
    input_info_t *res = nullptr;
    do {
        res = ready_priority_.top();
        ready_priority_.pop();
        if (res->binding.empty() || res->binding.find(for_output) != res->binding.end())
            break;
        // We keep searching for a suitable input.
        skipped.insert(res);
        res = nullptr;
    } while (!ready_priority_.empty());
    // Re-add the ones we skipped, but without changing their counters so we preserve
    // the prior FIFO order.
    for (input_info_t *save : skipped)
        ready_priority_.push(save);
    if (res != nullptr) {
        VPRINT(this, 4,
               "pop_from_ready_queue[%d] (post-size %zu): input %d priority %d timestamp "
               "delta %" PRIu64 "\n",
               for_output, ready_priority_.size(), res->index, res->priority,
               res->reader->get_last_timestamp() - res->base_timestamp);
    }
    return res;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::set_cur_input(output_ordinal_t output,
                                                        input_ordinal_t input)
{
    // XXX i#5843: Merge tracking of current inputs with ready_queue_ to better manage
    // the possible 3 states of each input (a live cur_input for an output stream, in
    // the ready_queue_, or at EOF).
    assert(output >= 0 && output < static_cast<output_ordinal_t>(outputs_.size()));
    // 'input' might be INVALID_INPUT_ORDINAL.
    assert(input < static_cast<input_ordinal_t>(inputs_.size()));
    int prev_input = outputs_[output].cur_input;
    if (prev_input >= 0) {
        if (options_.mapping == MAP_TO_ANY_OUTPUT && prev_input != input)
            add_to_ready_queue(&inputs_[prev_input]);
        if (prev_input != input && options_.schedule_record_ostream != nullptr) {
            input_info_t &prev_info = inputs_[prev_input];
            std::lock_guard<std::mutex> lock(*prev_info.lock);
            sched_type_t::stream_status_t status =
                close_schedule_segment(output, prev_info);
            if (status != sched_type_t::STATUS_OK)
                return status;
        }
    }
    outputs_[output].cur_input = input;
    if (input < 0)
        return STATUS_OK;
    if (prev_input == input)
        return STATUS_OK;
    std::lock_guard<std::mutex> lock(*inputs_[input].lock);
    inputs_[input].instrs_in_quantum = 0;
    inputs_[input].start_time_in_quantum = outputs_[output].cur_time;
    if (options_.schedule_record_ostream != nullptr) {
        uint64_t instr_ord = inputs_[input].reader->get_instruction_ordinal();
        if (!inputs_[input].recorded_in_schedule && instr_ord == 1) {
            // Due to differing reader->init() vs initial set_cur_input() orderings
            // we can have an initial value of 1 for non-initial input streams
            // with few markers; we reset to 0 for such cases.
            VPRINT(this, 3,
                   "set_cur_input: adjusting instr_ord from 1 to 0 for input=%d\n",
                   input);
            instr_ord = 0;
        }
        VPRINT(this, 3, "set_cur_input: recording input=%d start=%" PRId64 "\n", input,
               instr_ord);
        sched_type_t::stream_status_t status =
            record_schedule_segment(output, schedule_record_t::DEFAULT, input, instr_ord);
        if (status != sched_type_t::STATUS_OK)
            return status;
    }
    return STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::pick_next_input_as_previously(
    output_ordinal_t output, input_ordinal_t &index)
{
    if (outputs_[output].record_index + 1 >=
        static_cast<int>(outputs_[output].record.size()))
        return sched_type_t::STATUS_EOF;
    const schedule_record_t &segment =
        outputs_[output].record[outputs_[output].record_index + 1];
    index = segment.key.input;
    {
        std::lock_guard<std::mutex> lock(*inputs_[index].lock);
        if (inputs_[index].reader->get_instruction_ordinal() >
            segment.start_instruction) {
            VPRINT(this, 1,
                   "WARNING: next_record[%d]: input %d wants instr #%" PRId64
                   " but it is already at #%" PRId64 "\n",
                   output, index, segment.start_instruction,
                   inputs_[index].reader->get_instruction_ordinal());
        }
        if (inputs_[index].reader->get_instruction_ordinal() <
                segment.start_instruction &&
            // The output may have begun in the wait state.
            (outputs_[output].record_index == -1 ||
             // When we skip our separator+timestamp markers are at the
             // prior instr ord so do not wait for that.
             outputs_[output].record[outputs_[output].record_index].type !=
                 schedule_record_t::SKIP)) {
            // Some other output stream has not advanced far enough, and we do
            // not support multiple positions in one input stream: we wait.
            // XXX i#5843: We may want to provide a kernel-mediated wait
            // feature so a multi-threaded simulator doesn't have to do a
            // spinning poll loop.
            VPRINT(this, 3, "next_record[%d]: waiting for input %d instr #%" PRId64 "\n",
                   output, index, segment.start_instruction);
            // Give up this input and go into a wait state.
            // We'll come back here on the next next_record() call.
            set_cur_input(output, INVALID_INPUT_ORDINAL);
            outputs_[output].waiting = true;
            return sched_type_t::STATUS_WAIT;
        }
    }
    // Also wait if this segment is ahead of the next-up segment on another
    // output.  We only have a timestamp per context switch so we can't
    // enforce finer-grained timing replay.
    if (options_.deps == DEPENDENCY_TIMESTAMPS) {
        for (int i = 0; i < static_cast<output_ordinal_t>(outputs_.size()); ++i) {
            if (i != output &&
                outputs_[i].record_index + 1 <
                    static_cast<int>(outputs_[i].record.size()) &&
                segment.timestamp >
                    outputs_[i].record[outputs_[i].record_index + 1].timestamp) {
                VPRINT(this, 3,
                       "next_record[%d]: waiting because timestamp %" PRIu64
                       " is ahead of output %d\n",
                       output, segment.timestamp, i);
                // Give up this input and go into a wait state.
                // We'll come back here on the next next_record() call.
                set_cur_input(output, INVALID_INPUT_ORDINAL);
                outputs_[output].waiting = true;
                return sched_type_t::STATUS_WAIT;
            }
        }
    }
    if (segment.type == schedule_record_t::SYNTHETIC_END) {
        std::lock_guard<std::mutex> lock(*inputs_[index].lock);
        // We're past the final region of interest and we need to insert
        // a synthetic thread exit record.
        inputs_[index].queue.push_back(create_thread_exit(inputs_[index].tid));
        inputs_[index].at_eof = true;
        VPRINT(this, 2, "early end for input %d\n", index);
        // We're done with this entry so move to and past it.
        outputs_[output].record_index += 2;
        return sched_type_t::STATUS_SKIPPED;
    } else if (segment.type == schedule_record_t::SKIP) {
        std::lock_guard<std::mutex> lock(*inputs_[index].lock);
        uint64_t cur_instr = inputs_[index].reader->get_instruction_ordinal();
        VPRINT(this, 2, "skipping from %" PRId64 " to %" PRId64 " instrs for schedule\n",
               cur_instr, segment.stop_instruction);
        auto status =
            skip_instructions(output, inputs_[index],
                              segment.stop_instruction - cur_instr - 1 /*exclusive*/);
        // Increment the region to get window id markers with ordinals.
        inputs_[index].cur_region++;
        if (status != sched_type_t::STATUS_SKIPPED)
            return sched_type_t::STATUS_INVALID;
        // We're done with the skip so move to and past it.
        outputs_[output].record_index += 2;
        return sched_type_t::STATUS_SKIPPED;
    } else {
        VPRINT(this, 2, "next_record[%d]: advancing to input %d instr #%" PRId64 "\n",
               output, index, segment.start_instruction);
    }
    ++outputs_[output].record_index;
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::pick_next_input(output_ordinal_t output,
                                                          bool in_wait_state)
{
    bool need_lock =
        options_.mapping == MAP_TO_ANY_OUTPUT || options_.mapping == MAP_AS_PREVIOUSLY;
    auto scoped_lock = need_lock ? std::unique_lock<std::mutex>(sched_lock_)
                                 : std::unique_lock<std::mutex>();
    input_ordinal_t prev_index = outputs_[output].cur_input;
    input_ordinal_t index = INVALID_INPUT_ORDINAL;
    while (true) {
        if (index < 0) {
            if (options_.mapping == MAP_AS_PREVIOUSLY) {
                sched_type_t::stream_status_t res =
                    pick_next_input_as_previously(output, index);
                if (res != sched_type_t::STATUS_OK)
                    return res;
            } else if (options_.mapping == MAP_TO_ANY_OUTPUT) {
                if (ready_queue_empty()) {
                    if (prev_index == INVALID_INPUT_ORDINAL)
                        return sched_type_t::STATUS_EOF;
                    std::lock_guard<std::mutex> lock(*inputs_[prev_index].lock);
                    if (inputs_[prev_index].at_eof)
                        return sched_type_t::STATUS_EOF;
                    else
                        index = prev_index; // Go back to prior.
                } else {
                    if (!in_wait_state) {
                        // Give up the input before we go to the queue so we can add
                        // ourselves to the queue.  If we're the highest priority we
                        // shouldn't switch.  The queue preserves FIFO for same-priority
                        // cases so we will switch if someone of equal priority is
                        // waiting.
                        set_cur_input(output, INVALID_INPUT_ORDINAL);
                    }
                    input_info_t *queue_next = pop_from_ready_queue(output);
                    if (queue_next == nullptr)
                        return sched_type_t::STATUS_EOF;
                    index = queue_next->index;
                }
            } else if (options_.deps == DEPENDENCY_TIMESTAMPS) {
                uint64_t min_time = std::numeric_limits<uint64_t>::max();
                for (size_t i = 0; i < inputs_.size(); ++i) {
                    std::lock_guard<std::mutex> lock(*inputs_[i].lock);
                    if (!inputs_[i].at_eof && inputs_[i].next_timestamp > 0 &&
                        inputs_[i].next_timestamp < min_time) {
                        min_time = inputs_[i].next_timestamp;
                        index = static_cast<int>(i);
                    }
                }
                if (index < 0)
                    return sched_type_t::STATUS_EOF;
                VPRINT(this, 2,
                       "next_record[%d]: advancing to timestamp %" PRIu64
                       " == input #%d\n",
                       output, min_time, index);
            } else if (options_.mapping == MAP_TO_CONSISTENT_OUTPUT) {
                // We're done with the prior thread; take the next one that was
                // pre-allocated to this output (pre-allocated to avoid locks). Invariant:
                // the same output will not be accessed by two different threads
                // simultaneously in this mode, allowing us to support a lock-free
                // parallel-friendly increment here.
                int indices_index = ++outputs_[output].input_indices_index;
                if (indices_index >=
                    static_cast<int>(outputs_[output].input_indices.size())) {
                    VPRINT(this, 2, "next_record[%d]: all at eof\n", output);
                    return sched_type_t::STATUS_EOF;
                }
                index = outputs_[output].input_indices[indices_index];
                VPRINT(this, 2,
                       "next_record[%d]: advancing to local index %d == input #%d\n",
                       output, indices_index, index);
            } else
                return sched_type_t::STATUS_INVALID;
            // reader_t::at_eof_ is true until init() is called.
            std::lock_guard<std::mutex> lock(*inputs_[index].lock);
            if (inputs_[index].needs_init) {
                inputs_[index].reader->init();
                inputs_[index].needs_init = false;
            }
        }
        std::lock_guard<std::mutex> lock(*inputs_[index].lock);
        if (inputs_[index].at_eof ||
            *inputs_[index].reader == *inputs_[index].reader_end) {
            VPRINT(this, 2, "next_record[%d]: local index %d == input #%d at eof\n",
                   output, outputs_[output].input_indices_index, index);
            if (options_.schedule_record_ostream != nullptr &&
                prev_index != INVALID_INPUT_ORDINAL)
                close_schedule_segment(output, inputs_[prev_index]);
            inputs_[index].at_eof = true;
            index = INVALID_INPUT_ORDINAL;
            // Loop and pick next thread.
            continue;
        }
        break;
    }
    set_cur_input(output, index);
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::next_record(output_ordinal_t output,
                                                      RecordType &record,
                                                      input_info_t *&input,
                                                      uint64_t cur_time)
{
    // We do not enforce a globally increasing time to avoid the synchronization cost; we
    // do return an error on a time smaller than an input's current start time when we
    // check for quantum end.
    outputs_[output].cur_time = cur_time;
    if (!outputs_[output].active)
        return sched_type_t::STATUS_WAIT;
    if (outputs_[output].waiting) {
        sched_type_t::stream_status_t res = pick_next_input(output, true);
        if (res != sched_type_t::STATUS_OK)
            return res;
        outputs_[output].waiting = false;
    }
    if (outputs_[output].cur_input < 0) {
        // This happens with more outputs than inputs.  For non-empty outputs we
        // require cur_input to be set to >=0 during init().
        return sched_type_t::STATUS_EOF;
    }
    input = &inputs_[outputs_[output].cur_input];
    auto lock = std::unique_lock<std::mutex>(*input->lock);
    // Since we do not ask for a start time, we have to check for the first record from
    // each input and set the time here.
    if (input->start_time_in_quantum == 0)
        input->start_time_in_quantum = cur_time;
    if (!outputs_[output].speculation_stack.empty()) {
        outputs_[output].prev_speculate_pc = outputs_[output].speculate_pc;
        error_string_ = outputs_[output].speculator.next_record(
            outputs_[output].speculate_pc, record);
        if (!error_string_.empty())
            return sched_type_t::STATUS_INVALID;
        // Leave the cur input where it is: the ordinals will remain unchanged.
        // Also avoid the context switch checks below as we cannot switch in the
        // middle of speculating (we also don't count speculated instructions toward
        // QUANTUM_INSTRUCTIONS).
        return sched_type_t::STATUS_OK;
    }
    while (true) {
        if (input->needs_init) {
            // We pay the cost of this conditional to support ipc_reader_t::init() which
            // blocks and must be called right before reading its first record.
            // The user can't call init() when it accesses the output streams because
            // it std::moved the reader to us; we can't call it between our own init()
            // and here as we have no control point in between, and our init() is too
            // early as the user may have other work after that.
            input->reader->init();
            input->needs_init = false;
        }
        if (!input->queue.empty()) {
            record = input->queue.front();
            input->queue.pop_front();
        } else {
            // We again have a flag check because reader_t::init() does an initial ++
            // and so we want to skip that on the first record but perform a ++ prior
            // to all subsequent records.  We do not want to ++ after reading as that
            // messes up memtrace_stream_t queries on ordinals while the user examines
            // the record.
            if (input->needs_advance && !input->at_eof) {
                ++(*input->reader);
            } else {
                input->needs_advance = true;
            }
            if (input->at_eof || *input->reader == *input->reader_end) {
                lock.unlock();
                sched_type_t::stream_status_t res = pick_next_input(output, false);
                if (res != sched_type_t::STATUS_OK)
                    return res;
                input = &inputs_[outputs_[output].cur_input];
                lock = std::unique_lock<std::mutex>(*input->lock);
                continue;
            } else {
                record = **input->reader;
            }
        }
        VPRINT(this, 5, "next_record[%d]: candidate record from %d: ", output,
               input->index);
        VDO(this, 5, print_record(record););
        bool need_new_input = false;
        bool in_wait_state = false;
        if (options_.mapping == MAP_AS_PREVIOUSLY) {
            assert(outputs_[output].record_index >= 0);
            if (outputs_[output].record_index >=
                static_cast<int>(outputs_[output].record.size())) {
                // We're on the last record.
            } else if (outputs_[output].record[outputs_[output].record_index].type ==
                       schedule_record_t::SKIP) {
                need_new_input = true;
            } else {
                uint64_t stop = outputs_[output]
                                    .record[outputs_[output].record_index]
                                    .stop_instruction;
                // The stop is exclusive.  0 does mean to do nothing (easiest
                // to have an empty record to share the next-entry for a start skip
                // or other cases).
                if (input->reader->get_instruction_ordinal() >= stop) {
                    need_new_input = true;
                }
            }
        } else if (options_.mapping == MAP_TO_ANY_OUTPUT) {
            trace_marker_type_t marker_type;
            uintptr_t marker_value;
            if (input->processing_blocking_syscall) {
                // Wait until we're past all the markers associated with the syscall.
                // XXX: We may prefer to stop before the return value marker for futex,
                // or a kernel xfer marker, but our recorded format is on instr
                // boundaries so we live with those being before the switch.
                if (record_type_is_instr(record)) {
                    // Assume it will block and we should switch to a different input.
                    need_new_input = true;
                    in_wait_state = true;
                    input->processing_blocking_syscall = false;
                    VPRINT(this, 3, "next_record[%d]: hit blocking syscall in input %d\n",
                           output, input->index);
                }
            } else if (record_type_is_marker(record, marker_type, marker_value) &&
                       marker_type == TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL) {
                input->processing_blocking_syscall = true;
            } else if (options_.quantum_unit == QUANTUM_INSTRUCTIONS &&
                       record_type_is_instr(record)) {
                ++input->instrs_in_quantum;
                if (input->instrs_in_quantum > options_.quantum_duration) {
                    // We again prefer to switch to another input even if the current
                    // input has the oldest timestamp, prioritizing context switches
                    // over timestamp ordering.
                    need_new_input = true;
                }
            } else if (options_.quantum_unit == QUANTUM_TIME) {
                // The above if-else cases are all either for non-instrs or
                // QUANTUM_INSTRUCTIONS, except the blocking syscall next instr which is
                // already switching: so an else{} works here.
                if (cur_time == 0 || cur_time < input->start_time_in_quantum) {
                    VPRINT(this, 1,
                           "next_record[%d]: invalid time %" PRIu64 " vs start %" PRIu64
                           "\n",
                           output, cur_time, input->start_time_in_quantum);
                    return sched_type_t::STATUS_INVALID;
                }
                if (cur_time - input->start_time_in_quantum >=
                        options_.quantum_duration &&
                    // We only switch on instruction boundaries.  We could possibly switch
                    // in between (e.g., scatter/gather long sequence of reads/writes) by
                    // setting input->switching_pre_instruction.
                    record_type_is_instr(record)) {
                    VPRINT(this, 4,
                           "next_record[%d]: hit end of time quantum after %" PRIu64
                           " (%" PRIu64 " - %" PRIu64 ")\n",
                           output, cur_time - input->start_time_in_quantum, cur_time,
                           input->start_time_in_quantum);
                    need_new_input = true;
                }
            }
        }
        if (options_.deps == DEPENDENCY_TIMESTAMPS &&
            options_.mapping != MAP_AS_PREVIOUSLY &&
            // For MAP_TO_ANY_OUTPUT with timestamps: enforcing asked-for context switch
            // rates is more important that honoring precise trace-buffer-based
            // timestamp inter-input dependencies so we do not end a quantum early due
            // purely to timestamps.
            options_.mapping != MAP_TO_ANY_OUTPUT &&
            record_type_is_timestamp(record, input->next_timestamp))
            need_new_input = true;
        if (need_new_input) {
            int prev_input = outputs_[output].cur_input;
            lock.unlock();
            sched_type_t::stream_status_t res = pick_next_input(output, in_wait_state);
            if (res != sched_type_t::STATUS_OK && res != sched_type_t::STATUS_WAIT &&
                res != sched_type_t::STATUS_SKIPPED)
                return res;
            if (outputs_[output].cur_input != prev_input) {
                // TODO i#5843: Queueing here and in a few other places gets the
                // ordinals off: we need to undo the ordinal increases to avoid
                // over-counting while queued and double-counting when we resume.
                // In some cases we need to undo this on the output stream too.
                // So we should set suppress_ref_count_ in the input to get
                // is_record_synthetic() (and have our stream class check that
                // for instr count too) -- but what about output during speculation?
                // Decrement counts instead to undo?
                lock.lock();
                VPRINT(this, 5, "next_record_mid[%d]: from %d: queueing ", output,
                       input->index);
                VDO(this, 5, print_record(record););
                input->queue.push_back(record);
                if (res == sched_type_t::STATUS_WAIT)
                    return res;
                input = &inputs_[outputs_[output].cur_input];
                lock = std::unique_lock<std::mutex>(*input->lock);
                continue;
            } else
                lock.lock();
            if (res == sched_type_t::STATUS_SKIPPED) {
                // Like for the ROI below, we need the queue or a de-ref.
                input->needs_advance = false;
                continue;
            }
        }
        if (input->needs_roi && options_.mapping != MAP_AS_PREVIOUSLY &&
            !input->regions_of_interest.empty()) {
            sched_type_t::stream_status_t res =
                advance_region_of_interest(output, record, *input);
            if (res == sched_type_t::STATUS_SKIPPED) {
                // We need either the queue or to re-de-ref the reader so we loop,
                // but we do not want to come back here.
                input->needs_roi = false;
                input->needs_advance = false;
                continue;
            } else if (res != sched_type_t::STATUS_OK)
                return res;
        } else {
            input->needs_roi = true;
        }
        break;
    }
    VPRINT(this, 4, "next_record[%d]: from %d: ", output, input->index);
    VDO(this, 4, print_record(record););

    outputs_[output].last_record = record;
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::start_speculation(output_ordinal_t output,
                                                            addr_t start_address,
                                                            bool queue_current_record)
{
    auto &outinfo = outputs_[output];
    if (outinfo.speculation_stack.empty()) {
        if (queue_current_record)
            inputs_[outinfo.cur_input].queue.push_back(outinfo.last_record);
        // The store address for the outer layer is not used since we have the
        // actual trace storing our resumption context, so we store a sentinel.
        static constexpr addr_t SPECULATION_OUTER_ADDRESS = 0;
        outinfo.speculation_stack.push(SPECULATION_OUTER_ADDRESS);
    } else {
        if (queue_current_record) {
            // XXX i#5843: We'll re-call the speculator so we're assuming a repeatable
            // response with the same instruction returned.  We should probably save the
            // precise record either here or in the speculator.
            outinfo.speculation_stack.push(outinfo.prev_speculate_pc);
        } else
            outinfo.speculation_stack.push(outinfo.speculate_pc);
    }
    // Set the prev in case another start is called before reading a record.
    outinfo.prev_speculate_pc = outinfo.speculate_pc;
    outinfo.speculate_pc = start_address;
    VPRINT(this, 2, "start_speculation layer=%zu pc=0x%zx\n",
           outinfo.speculation_stack.size(), start_address);
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::stop_speculation(output_ordinal_t output)
{
    auto &outinfo = outputs_[output];
    if (outinfo.speculation_stack.empty())
        return sched_type_t::STATUS_INVALID;
    if (outinfo.speculation_stack.size() > 1) {
        // speculate_pc is only used when exiting inner layers.
        outinfo.speculate_pc = outinfo.speculation_stack.top();
    }
    VPRINT(this, 2, "stop_speculation layer=%zu (resume=0x%zx)\n",
           outinfo.speculation_stack.size(), outinfo.speculate_pc);
    outinfo.speculation_stack.pop();
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::set_output_active(output_ordinal_t output,
                                                            bool active)
{
    if (options_.mapping != MAP_TO_ANY_OUTPUT)
        return sched_type_t::STATUS_INVALID;
    if (outputs_[output].active == active)
        return sched_type_t::STATUS_OK;
    outputs_[output].active = active;
    VPRINT(this, 2, "Output stream %d is now %s\n", output,
           active ? "active" : "inactive");
    std::lock_guard<std::mutex> guard(sched_lock_);
    if (!active) {
        // Make the now-inactive output's input available for other cores.
        // This will reset its quantum too.
        // We aren't switching on a just-read instruction not passed to the consumer.
        inputs_[outputs_[output].cur_input].switching_pre_instruction = true;
        set_cur_input(output, INVALID_INPUT_ORDINAL);
    } else {
        outputs_[output].waiting = true;
    }
    return sched_type_t::STATUS_OK;
}

template class scheduler_tmpl_t<memref_t, reader_t>;
template class scheduler_tmpl_t<trace_entry_t, dynamorio::drmemtrace::record_reader_t>;

} // namespace drmemtrace
} // namespace dynamorio
