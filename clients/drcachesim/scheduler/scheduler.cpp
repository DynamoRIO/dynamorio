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
#include "reader/file_reader.h"
#ifdef HAS_ZLIB
#    include "reader/compressed_file_reader.h"
#endif
#ifdef HAS_ZIP
#    include "reader/zipfile_file_reader.h"
#endif
#ifdef HAS_SNAPPY
#    include "reader/snappy_file_reader.h"
#endif
#include "directory_iterator.h"
#include "common/utils.h"

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
memref_t
scheduler_tmpl_t<memref_t, reader_t>::create_region_separator_marker(memref_tid_t tid,
                                                                     uintptr_t value)
{
    memref_t record = {};
    record.marker.type = TRACE_TYPE_MARKER;
    record.marker.marker_type = TRACE_MARKER_TYPE_WINDOW_ID;
    record.marker.marker_value = value;
    record.marker.tid = tid;
    return record;
}

template <>
void
scheduler_tmpl_t<memref_t, reader_t>::print_record(const memref_t &record)
{
    fprintf(stderr, "tid=%" PRId64 " type=%d", record.instr.tid, record.instr.type);
    if (type_is_instr(record.instr.type))
        fprintf(stderr, " pc=0x%zx size=%zu", record.instr.addr, record.instr.size);
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
void
scheduler_tmpl_t<trace_entry_t, record_reader_t>::print_record(
    const trace_entry_t &record)
{
    fprintf(stderr, "type=%d\n", record.type);
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
        std::vector<memref_tid_t> workload_tids;
        if (workload.path.empty()) {
            if (!workload.reader || !workload.reader_end)
                return STATUS_ERROR_INVALID_PARAMETER;
            inputs_[workload.reader_tid].reader = std::move(workload.reader);
            inputs_[workload.reader_tid].reader_end = std::move(workload.reader_end);
            inputs_[workload.reader_tid].tid = workload.reader_tid;
            inputs_[workload.reader_tid].needs_init = true;
            workload_tids.push_back(workload.reader_tid);
        } else {
            if (!!workload.reader || !!workload.reader_end)
                return STATUS_ERROR_INVALID_PARAMETER;
            sched_type_t::scheduler_status_t res =
                open_readers(workload.path, workload_tids);
            if (res != STATUS_SUCCESS)
                return res;
        }
        for (const auto &modifiers : workload.thread_modifiers) {
            if (modifiers.struct_size != sizeof(input_thread_info_t))
                return STATUS_ERROR_INVALID_PARAMETER;
            const std::vector<memref_tid_t> *which_tids;
            if (modifiers.tids.empty())
                which_tids = &workload_tids;
            else
                which_tids = &modifiers.tids;
            // We assume the overhead of copying the modifiers for every thread is
            // not high and the simplified code is worthwhile.
            for (memref_tid_t tid : *which_tids) {
                if (inputs_.find(tid) == inputs_.end())
                    return STATUS_ERROR_INVALID_PARAMETER;
                inputs_[tid].tid = tid;
                inputs_[tid].binding = modifiers.output_binding;
                if (!inputs_[tid].binding.empty()) {
                    // TODO i#5843: Implement bindings.
                    return STATUS_ERROR_NOT_IMPLEMENTED;
                }
                inputs_[tid].priority = modifiers.priority;
                if (inputs_[tid].priority != 0) {
                    // TODO i#5843: Implement priorities.
                    return STATUS_ERROR_NOT_IMPLEMENTED;
                }
                for (const auto &range : modifiers.regions_of_interest) {
                    if (range.start_instruction == 0 ||
                        (range.stop_instruction < range.start_instruction &&
                         range.stop_instruction != 0))
                        return STATUS_ERROR_INVALID_PARAMETER;
                }
                inputs_[tid].regions_of_interest = modifiers.regions_of_interest;
            }
        }
    }
    outputs_.reserve(output_count);
    for (int i = 0; i < output_count; i++) {
        outputs_.emplace_back(this, i, verbosity_);
    }
    if (options_.consider_input_dependencies) {
        // TODO i#5843: Implement dependency analysis and use it to inform input stream
        // preemption.  Probably we would take offline analysis results as an input.
        return STATUS_ERROR_NOT_IMPLEMENTED;
    }
    if (options_.how_split == STREAM_BY_INPUT_SHARD) {
        // This mode is pure parallel with dependencies between threads ignored.
        if (options_.strategy != SCHEDULE_RUN_TO_COMPLETION)
            return STATUS_ERROR_INVALID_PARAMETER;
        // Assign the inputs up front to avoid locks once we're in parallel mode.
        // We use a simple round-robin static assignment for now.
        size_t count = 0;
        for (const auto &entry : inputs_) {
            size_t index = count % output_count;
            if (outputs_[index].tids_.empty())
                outputs_[index].cur_input_tid = entry.first;
            outputs_[index].tids_.push_back(entry.first);
            ++count;
        }
    } else if (options_.how_split == STREAM_BY_SYNTHETIC_CPU) {
        // TODO i#5843: Implement scheduling onto synthetic cores.
        // Initially we only support analyzer_t's serial mode.
        if (output_count != 1 || options_.strategy != SCHEDULE_INTERLEAVE_AS_RECORDED)
            return STATUS_ERROR_NOT_IMPLEMENTED;
    } else if (options_.how_split == STREAM_BY_RECORDED_CPU) {
        // TODO i#5843,i#5694: Implement cpu-oriented iteration.
        return STATUS_ERROR_NOT_IMPLEMENTED;
    } else
        return STATUS_ERROR_INVALID_PARAMETER;
    return STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_tmpl_t<RecordType, ReaderType>::open_reader(
    const std::string &path, std::vector<memref_tid_t> &workload_tids)
{
    if (path.empty() || directory_iterator_t::is_directory(path))
        return STATUS_ERROR_INVALID_PARAMETER;
    std::unique_ptr<ReaderType> reader = get_reader(path, verbosity_);
    if (!reader || !reader->init()) {
        error_string_ += "Failed to open " + path;
        return STATUS_ERROR_FILE_OPEN_FAILED;
    }
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
        inputs_[tid].queue.push(record);
        ++(*reader);
    }
    if (tid == INVALID_THREAD_ID) {
        error_string_ = "Failed to read " + path;
        return STATUS_ERROR_FILE_READ_FAILED;
    }
    VPRINT(this, 2, "Opened reader for tid %" PRId64 " %s\n", tid, path.c_str());
    inputs_[tid].reader = std::move(reader);
    inputs_[tid].reader_end = std::move(reader_end);
    workload_tids.push_back(tid);
    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_tmpl_t<RecordType, ReaderType>::open_readers(
    const std::string &path, std::vector<memref_tid_t> &workload_tids)
{
    if (options_.how_split == STREAM_BY_SYNTHETIC_CPU &&
        options_.strategy != SCHEDULE_INTERLEAVE_AS_RECORDED) {
        // TODO i#5843: Move the interleaving-by-timestamp code from
        // file_reader_t into this scheduler.  For now we leverage the
        // file_reader_t code and use a synthetic tid.
        std::unique_ptr<ReaderType> reader = get_reader(path, verbosity_);
        std::unique_ptr<ReaderType> reader_end = get_default_reader();
        if (!reader || !reader_end || !reader->init()) {
            error_string_ += "Failed to open " + path;
            return STATUS_ERROR_FILE_OPEN_FAILED;
        }
        memref_tid_t tid = 1;
        inputs_[tid].reader = std::move(reader);
        inputs_[tid].reader_end = std::move(reader_end);
        workload_tids.push_back(tid);
        return sched_type_t::STATUS_SUCCESS;
    }

    if (!directory_iterator_t::is_directory(path))
        return open_reader(path, workload_tids);
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
        const std::string file = path + DIRSEP + fname;
        sched_type_t::scheduler_status_t res = open_reader(file, workload_tids);
        if (res != sched_type_t::STATUS_SUCCESS)
            return res;
    }
    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
std::string
scheduler_tmpl_t<RecordType, ReaderType>::get_input_name(int output_ordinal)
{
    memref_tid_t tid = outputs_[output_ordinal].cur_input_tid;
    if (tid == 0)
        return "";
    return inputs_[tid].reader->get_stream_name();
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::advance_region_of_interest(int output_ordinal,
                                                                     input_info_t &input)
{
    uint64_t cur_instr = input.reader->get_instruction_ordinal();
    auto &cur_range = input.regions_of_interest[input.cur_region];
    if (cur_range.stop_instruction != 0 && cur_instr > cur_range.stop_instruction) {
        ++input.cur_region;
        VPRINT(this, 2, "at %" PRId64 " instrs: advancing to ROI #%d\n", cur_instr,
               input.cur_region);
        if (input.cur_region >= static_cast<int>(input.regions_of_interest.size())) {
            input.at_eof = true;
            return sched_type_t::STATUS_EOF;
        }
        cur_range = input.regions_of_interest[input.cur_region];
    }
    if (cur_instr < cur_range.start_instruction) {
        VPRINT(this, 2, "skipping from %" PRId64 " to %" PRId64 " instrs for ROI\n",
               cur_instr, cur_range.start_instruction);
        // We assume the queue contains no instrs (else our query of input.reader's
        // instr ordinal would include them and so be incorrect) and that we should
        // this skip it all.
        while (!input.queue.empty())
            input.queue.pop();
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
        // We don't need this for the instr count as we never skip and
        // start with an instr: always a timestamp.
        uint64_t subtract = input.reader->is_record_synthetic() ? 0 : 1;
        VPRINT(this, 3, "post-skip: adding %" PRId64 " records to %" PRId64 "\n",
               input.reader->get_record_ordinal() - input_start_ref - subtract,
               stream.cur_ref_count_);
        stream.cur_ref_count_ +=
            input.reader->get_record_ordinal() - input_start_ref - subtract;
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
        if (input.cur_region > 0) {
            // We let the user know we've skipped.
            input.queue.push(create_region_separator_marker(input.tid, input.cur_region));
        }
    }
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::next_record(int output_ordinal,
                                                      RecordType &record,
                                                      input_info_t *&input)
{
    if (!(options_.how_split == STREAM_BY_INPUT_SHARD &&
          options_.strategy == SCHEDULE_RUN_TO_COMPLETION) &&
        !(options_.how_split == STREAM_BY_SYNTHETIC_CPU &&
          options_.strategy == SCHEDULE_INTERLEAVE_AS_RECORDED)) {
        // TODO i#5843: Implement instr/time quanta.
        // TODO i#5843: Implement recorded-cpu scheduling.
        // These will require locks and central coordination.
        return sched_type_t::STATUS_NOT_IMPLEMENTED;
    }
    if (options_.how_split == STREAM_BY_INPUT_SHARD) {
        memref_tid_t tid = outputs_[output_ordinal].cur_input_tid;
        while (true) {
            if (tid == INVALID_THREAD_ID) {
                // We're just starting, or done with the prior thread; take the next one
                // that was pre-allocated to this output (pre-allocated to avoid locks).
                // Invariant: the same output_ordinal will not be accessed by two
                // different threads simultaneously in this mode, allowing us to support a
                // lock-free parallel-friendly increment here.
                ++outputs_[output_ordinal].cur_tids_index_;
                if (outputs_[output_ordinal].cur_tids_index_ >=
                    static_cast<int>(outputs_[output_ordinal].tids_.size())) {
                    VPRINT(this, 2, "next_record[%d]: all at eof\n", output_ordinal);
                    return sched_type_t::STATUS_EOF;
                }
                tid = outputs_[output_ordinal]
                          .tids_[outputs_[output_ordinal].cur_tids_index_];
                VPRINT(this, 2,
                       "next_record[%d]: advancing to index %d == tid %" PRId64 "\n",
                       output_ordinal, outputs_[output_ordinal].cur_tids_index_, tid);
            }
            if (inputs_[tid].at_eof || *inputs_[tid].reader == *inputs_[tid].reader_end) {
                VPRINT(this, 2, "next_record[%d]: index %d == tid %" PRId64 " at eof\n",
                       output_ordinal, outputs_[output_ordinal].cur_tids_index_, tid);
                tid = INVALID_THREAD_ID;
                inputs_[tid].at_eof = true;
                // Loop and pick next thread.
            } else if (!inputs_[tid].regions_of_interest.empty()) {
                sched_type_t::stream_status_t res =
                    advance_region_of_interest(output_ordinal, inputs_[tid]);
                if (res == sched_type_t::STATUS_EOF) {
                    VPRINT(this, 2,
                           "next_record[%d]: index %d == tid %" PRId64 " beyond ROI\n",
                           output_ordinal, outputs_[output_ordinal].cur_tids_index_, tid);
                    tid = INVALID_THREAD_ID;
                    // Loop and pick next thread.
                } else if (res != sched_type_t::STATUS_OK)
                    return res;
                else
                    break;
            } else
                break;
        }
        outputs_[output_ordinal].cur_input_tid = tid;
        input = &inputs_[tid];
    } else {
        // Currently file_reader_t is interleaving for us so we have just one input.
        // TODO i#5843: Move interleaving logic to here (and split it into a separate
        // function).
        if (inputs_.size() > 1)
            return sched_type_t::STATUS_NOT_IMPLEMENTED;
        for (auto &entry : inputs_) {
            input = &entry.second;
        }
    }
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
    if (!input->regions_of_interest.empty()) {
        sched_type_t::stream_status_t res =
            advance_region_of_interest(output_ordinal, *input);
        if (res != sched_type_t::STATUS_OK)
            return res;
    }
    if (!input->queue.empty()) {
        record = input->queue.front();
        input->queue.pop();
    } else {
        // We again have a flag check because reader_t::init() does an initial ++
        // and so we want to skip that on the first record but perform a ++ prior
        // to all subsequent records.  We do not want to ++ after reading as that
        // messes up memtrace_stream_t queries on ordinals while the user examines
        // the record.
        if (input->needs_advance)
            ++(*input->reader);
        else {
            input->needs_advance = true;
        }
        if (*input->reader == *input->reader_end)
            return sched_type_t::STATUS_EOF;
        record = **input->reader;
    }
    VPRINT(this, 4, "next_record[%d]: ", output_ordinal);
    VDO(this, 4, print_record(record););

    return sched_type_t::STATUS_OK;
}

template class scheduler_tmpl_t<memref_t, reader_t>;
template class scheduler_tmpl_t<trace_entry_t, dynamorio::drmemtrace::record_reader_t>;

} // namespace drmemtrace
} // namespace dynamorio
