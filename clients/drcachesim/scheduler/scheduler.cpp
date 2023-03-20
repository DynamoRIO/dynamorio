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
#include "file_reader.h"
#ifdef HAS_ZLIB
#    include "compressed_file_reader.h"
#endif
#ifdef HAS_ZIP
#    include "zipfile_file_reader.h"
#endif
#ifdef HAS_SNAPPY
#    include "snappy_file_reader.h"
#endif
#include "directory_iterator.h"
#include "utils.h"

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
    input_info_t *input = nullptr;
    sched_type_t::stream_status_t res = scheduler_->next_record(ordinal_, record, input);
    if (res != sched_type_t::STATUS_OK)
        return res;

    // Update our memtrace_stream_t state.
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
    // A possible except is allowing warmup-phase-filtered traces to be mixed
    // with regular traces.
    trace_marker_type_t marker_type;
    uintptr_t marker_value;
    if (scheduler_->record_type_is_marker(record, marker_type, marker_value)) {
        switch (marker_type) {
        case TRACE_MARKER_TYPE_TIMESTAMP: last_timestamp_ = marker_value; break;
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
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::report_time(uint64_t cur_time)
{
    return sched_type_t::STATUS_NOT_IMPLEMENTED;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::start_speculation(
    addr_t start_address)
{
    return sched_type_t::STATUS_NOT_IMPLEMENTED;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::stop_speculation()
{
    return sched_type_t::STATUS_NOT_IMPLEMENTED;
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
    for (auto &workload : workload_inputs) {
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
                int index = static_cast<int>(inputs_.size());
                inputs_.emplace_back();
                input_info_t &input = inputs_.back();
                input.index = index;
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
                if (!input.binding.empty()) {
                    // TODO i#5843: Implement bindings.
                    return STATUS_ERROR_NOT_IMPLEMENTED;
                }
                input.priority = modifiers.priority;
                if (input.priority != 0) {
                    // TODO i#5843: Implement priorities.
                    return STATUS_ERROR_NOT_IMPLEMENTED;
                }
                for (const auto &range : modifiers.regions_of_interest) {
                    if (range.start_instruction == 0 ||
                        (range.stop_instruction < range.start_instruction &&
                         range.stop_instruction != 0))
                        return STATUS_ERROR_INVALID_PARAMETER;
                }
                input.regions_of_interest = modifiers.regions_of_interest;
            }
        }
    }
    outputs_.reserve(output_count);
    for (int i = 0; i < output_count; i++) {
        outputs_.emplace_back(this, i, verbosity_);
    }
    if (options_.mapping == MAP_TO_CONSISTENT_OUTPUT) {
        // Assign the inputs up front to avoid locks once we're in parallel mode.
        // We use a simple round-robin static assignment for now.
        for (int i = 0; i < static_cast<int>(inputs_.size()); i++) {
            size_t index = i % output_count;
            if (outputs_[index].input_indices.empty())
                outputs_[index].cur_input = i;
            outputs_[index].input_indices.push_back(i);
            VPRINT(this, 2, "Assigning input #%d to output #%zd\n", i, index);
        }
    } else if (options_.mapping == MAP_TO_RECORDED_OUTPUT) {
        // TODO i#5843,i#5694: Implement cpu-oriented parallel iteration.
        // Initially we only support analyzer_t's serial mode.
        if (options_.deps != DEPENDENCY_TIMESTAMPS)
            return STATUS_ERROR_NOT_IMPLEMENTED;
        // TODO i#5843: Support more than one output.
        if (output_count != 1)
            return STATUS_ERROR_NOT_IMPLEMENTED;
        if (inputs_.size() == 1) {
            outputs_[0].cur_input = 0;
        } else {
            // The old file_reader_t interleaving would output the top headers for every
            // thread first and then pick the oldest timestamp once it reached a
            // timestamp. We instead queue those headers so we can start directly with the
            // oldest timestamp's thread.
            sched_type_t::scheduler_status_t res = get_initial_timestamps();
            if (res != STATUS_SUCCESS)
                return res;
        }
    } else {
        // TODO i#5843: Implement scheduling onto synthetic cores.
        // For now we only support a single output stream with no deps.
        if (options_.deps != DEPENDENCY_IGNORE || output_count != 1)
            return STATUS_ERROR_NOT_IMPLEMENTED;
        outputs_[0].cur_input = 0;
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
    uint64_t min_time = 0xffffffffffffffff;
    for (size_t i = 0; i < inputs_.size(); i++) {
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
                input.queue.push_back(record);
                ++(*input.reader);
            }
        }
        if (input.next_timestamp <= 0)
            return STATUS_ERROR_INVALID_PARAMETER;
        if (input.next_timestamp < min_time) {
            min_time = input.next_timestamp;
            // TODO i#5843: Support more than one input (already checked earlier).
            outputs_[0].cur_input = static_cast<int>(i);
        }
    }
    if (outputs_[0].cur_input >= 0)
        return STATUS_SUCCESS;
    return STATUS_ERROR_INVALID_PARAMETER;
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
    int index = static_cast<int>(inputs_.size());
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
scheduler_tmpl_t<RecordType, ReaderType>::get_input_name(int output_ordinal)
{
    int index = outputs_[output_ordinal].cur_input;
    if (index < 0)
        return "";
    return inputs_[index].reader->get_stream_name();
}

template <typename RecordType, typename ReaderType>
int
scheduler_tmpl_t<RecordType, ReaderType>::get_input_ordinal(int output_ordinal)
{
    return outputs_[output_ordinal].cur_input;
}

template <typename RecordType, typename ReaderType>
bool
scheduler_tmpl_t<RecordType, ReaderType>::is_record_synthetic(int output_ordinal)
{
    int index = outputs_[output_ordinal].cur_input;
    if (index < 0)
        return false;
    return inputs_[index].reader->is_record_synthetic();
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::advance_region_of_interest(int output_ordinal,
                                                                     RecordType &record,
                                                                     input_info_t &input)
{
    uint64_t cur_instr = input.reader->get_instruction_ordinal();
    auto &cur_range = input.regions_of_interest[input.cur_region];
    if (cur_range.stop_instruction != 0 && cur_instr > cur_range.stop_instruction) {
        ++input.cur_region;
        VPRINT(this, 2, "at %" PRId64 " instrs: advancing to ROI #%d\n", cur_instr,
               input.cur_region);
        if (input.cur_region >= static_cast<int>(input.regions_of_interest.size())) {
            if (input.at_eof)
                return sched_type_t::STATUS_EOF;
            else {
                // We let the user know we're done.
                input.queue.push_back(create_thread_exit(input.tid));
                input.at_eof = true;
                return sched_type_t::STATUS_SKIPPED;
            }
        }
        cur_range = input.regions_of_interest[input.cur_region];
    }

    if (cur_instr >= cur_range.start_instruction)
        return sched_type_t::STATUS_OK;

    VPRINT(this, 2, "skipping from %" PRId64 " to %" PRId64 " instrs for ROI\n",
           cur_instr, cur_range.start_instruction);
    // We assume the queue contains no instrs (else our query of input.reader's
    // instr ordinal would include them and so be incorrect) and that we should
    // thus skip it all.
    while (!input.queue.empty())
        input.queue.pop_front();
    uint64_t input_start_ref = input.reader->get_record_ordinal();
    uint64_t input_start_instr = input.reader->get_instruction_ordinal();
    input.reader->skip_instructions(cur_range.start_instruction - cur_instr);
    if (*input.reader == *input.reader_end) {
        // Raise error because the input region is out of bounds.
        input.at_eof = true;
        return sched_type_t::STATUS_REGION_INVALID;
    }
    auto &stream = outputs_[output_ordinal].stream;

    if (stream.cur_ref_count_ == 0)
        ++stream.cur_ref_count_; // Account for reader_t::init()'s ++.
    // Subtract 1 as we'll be adding one in stream_t::next_record().
    uint64_t subtract = input.reader->is_record_synthetic() ? 0 : 1;
    VPRINT(this, 3, "post-skip: adding %" PRId64 " records to %" PRId64 "\n",
           input.reader->get_record_ordinal() - input_start_ref - subtract,
           stream.cur_ref_count_);
    stream.cur_ref_count_ +=
        input.reader->get_record_ordinal() - input_start_ref - subtract;
    // We don't need to subtract 1 for instrs: even though we could be adding one
    // in stream_t::next_record(), we're either inserting a window id and so we
    // will not do so, or this is a skip from the start and we'd only be off if
    // the first record were an instr: and even then our count started at 0 (see
    // just above where we correct the record count from 0).
    VPRINT(this, 3, "post-skip: adding %" PRId64 " instrs to %" PRId64 "\n",
           input.reader->get_instruction_ordinal() - input_start_instr,
           stream.cur_instr_count_);
    stream.cur_instr_count_ +=
        input.reader->get_instruction_ordinal() - input_start_instr;
    // If we skipped from the start we may not have seen the initial headers:
    // use the input's cached copies.
    if (stream.version_ == 0) {
        stream.version_ = input.reader->get_version();
        stream.last_timestamp_ = input.reader->get_last_timestamp();
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
        input.queue.push_back(
            create_region_separator_marker(input.tid, input.cur_region));
    }
    return sched_type_t::STATUS_SKIPPED;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::pick_next_input(int output_ordinal)
{
    if (options_.mapping == MAP_TO_ANY_OUTPUT) {
        // TODO i#5843: Implement synthetic scheduling with instr/time quanta.
        // These will require locks and central coordination.
        // For now we only support a single output.
        return sched_type_t::STATUS_EOF;
    }
    int index = outputs_[output_ordinal].cur_input;
    while (true) {
        if (index < 0) {
            if (options_.deps == DEPENDENCY_TIMESTAMPS) {
                uint64_t min_time = 0xffffffffffffffff;
                for (size_t i = 0; i < inputs_.size(); i++) {
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
                       output_ordinal, min_time, index);
            } else if (options_.mapping == MAP_TO_CONSISTENT_OUTPUT) {
                // We're done with the prior thread; take the next one that was
                // pre-allocated to this output (pre-allocated to avoid locks). Invariant:
                // the same output_ordinal will not be accessed by two different threads
                // simultaneously in this mode, allowing us to support a lock-free
                // parallel-friendly increment here.
                int indices_index = ++outputs_[output_ordinal].input_indices_index;
                if (indices_index >=
                    static_cast<int>(outputs_[output_ordinal].input_indices.size())) {
                    VPRINT(this, 2, "next_record[%d]: all at eof\n", output_ordinal);
                    return sched_type_t::STATUS_EOF;
                }
                index = outputs_[output_ordinal].input_indices[indices_index];
                VPRINT(this, 2,
                       "next_record[%d]: advancing to local index %d == input #%d\n",
                       output_ordinal, indices_index, index);
            } else
                return sched_type_t::STATUS_INVALID;
            // reader_t::at_eof_ is true until init() is called.
            if (inputs_[index].needs_init) {
                inputs_[index].reader->init();
                inputs_[index].needs_init = false;
            }
        }
        if (inputs_[index].at_eof ||
            *inputs_[index].reader == *inputs_[index].reader_end) {
            VPRINT(this, 2, "next_record[%d]: local index %d == input #%d at eof\n",
                   output_ordinal, outputs_[output_ordinal].input_indices_index, index);
            inputs_[index].at_eof = true;
            index = -1;
            // Loop and pick next thread.
            continue;
        }
        break;
    }
    outputs_[output_ordinal].cur_input = index;
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::next_record(int output_ordinal,
                                                      RecordType &record,
                                                      input_info_t *&input)
{
    if (outputs_[output_ordinal].cur_input < 0) {
        // This happens with more outputs than inputs.  For non-empty outputs we
        // require cur_input to be set to >=0 during init().
        return sched_type_t::STATUS_EOF;
    }
    input = &inputs_[outputs_[output_ordinal].cur_input];
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
            if (input->needs_advance && !input->at_eof)
                ++(*input->reader);
            else {
                input->needs_advance = true;
            }
            if (input->at_eof || *input->reader == *input->reader_end) {
                sched_type_t::stream_status_t res = pick_next_input(output_ordinal);
                if (res != sched_type_t::STATUS_OK)
                    return res;
                input = &inputs_[outputs_[output_ordinal].cur_input];
                continue;
            } else {
                record = **input->reader;
            }
        }
        if (options_.deps == DEPENDENCY_TIMESTAMPS &&
            record_type_is_timestamp(record, input->next_timestamp)) {
            int cur_input = outputs_[output_ordinal].cur_input;
            // Mark cur_input invalid to ensure pick_next_input() takes action.
            outputs_[output_ordinal].cur_input = -1;
            sched_type_t::stream_status_t res = pick_next_input(output_ordinal);
            if (res != sched_type_t::STATUS_OK)
                return res;
            if (outputs_[output_ordinal].cur_input != cur_input) {
                input->queue.push_back(record);
                input = &inputs_[outputs_[output_ordinal].cur_input];
                continue;
            }
        }
        if (input->needs_roi && !input->regions_of_interest.empty()) {
            sched_type_t::stream_status_t res =
                advance_region_of_interest(output_ordinal, record, *input);
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
    VPRINT(this, 4, "next_record[%d]: from %d: ", output_ordinal, input->index);
    VDO(this, 4, print_record(record););

    return sched_type_t::STATUS_OK;
}

template class scheduler_tmpl_t<memref_t, reader_t>;
template class scheduler_tmpl_t<trace_entry_t, dynamorio::drmemtrace::record_reader_t>;

} // namespace drmemtrace
} // namespace dynamorio
