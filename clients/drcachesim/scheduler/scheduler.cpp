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
                int index = static_cast<input_ordinal_t>(inputs_.size());
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
    if (options_.mapping == MAP_AS_PREVIOUSLY) {
        if (options_.schedule_replay_istream == nullptr ||
            options_.schedule_record_ostream != nullptr ||
            options_.deps != DEPENDENCY_IGNORE)
            return STATUS_ERROR_INVALID_PARAMETER;
        sched_type_t::scheduler_status_t status = read_recorded_schedule();
        if (status != sched_type_t::STATUS_SUCCESS)
            return STATUS_ERROR_INVALID_PARAMETER;
    } else if (options_.schedule_replay_istream != nullptr)
        return STATUS_ERROR_INVALID_PARAMETER;
    if (options_.mapping == MAP_TO_CONSISTENT_OUTPUT) {
        // Assign the inputs up front to avoid locks once we're in parallel mode.
        // We use a simple round-robin static assignment for now.
        for (int i = 0; i < static_cast<input_ordinal_t>(inputs_.size()); ++i) {
            size_t index = i % output_count;
            if (outputs_[index].input_indices.empty())
                set_cur_input(static_cast<input_ordinal_t>(index), i);
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
            set_cur_input(0, 0);
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
        // TODO i#5843: Honor deps when scheduling on synthetic cores.
        if (options_.deps != DEPENDENCY_IGNORE)
            return STATUS_ERROR_NOT_IMPLEMENTED;
        // TODO i#5843: Implement time-based quanta.
        if (options_.quantum_unit != QUANTUM_INSTRUCTIONS)
            return STATUS_ERROR_NOT_IMPLEMENTED;
        // Assign initial inputs.
        // TODO i#5843: Once we support core bindings and priorities we'll want
        // to consider that here.
        for (int i = 0; i < static_cast<output_ordinal_t>(outputs_.size()); ++i) {
            if (i < static_cast<input_ordinal_t>(inputs_.size()))
                set_cur_input(i, i);
            else
                set_cur_input(i, INVALID_INPUT_ORDINAL);
        }
        for (int i = static_cast<output_ordinal_t>(outputs_.size());
             i < static_cast<input_ordinal_t>(inputs_.size()); ++i) {
            ready_.push(i);
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
    for (int i = 0; i < static_cast<output_ordinal_t>(outputs_.size()); ++i) {
        if (i < static_cast<input_ordinal_t>(inputs_.size()) &&
            !outputs_[i].record.empty()) {
            set_cur_input(i, outputs_[i].record[0].key.input);
        } else
            set_cur_input(i, INVALID_INPUT_ORDINAL);
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
                input.queue.push_back(record);
                ++(*input.reader);
            }
        }
        if (input.next_timestamp <= 0)
            return STATUS_ERROR_INVALID_PARAMETER;
        if (input.next_timestamp < min_time) {
            min_time = input.next_timestamp;
            // TODO i#5843: Support more than one output (already checked earlier).
            set_cur_input(0, static_cast<input_ordinal_t>(i));
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
    VPRINT(this, 3,
           "close_schedule_segment: input=%d start=%" PRId64 " stop=%" PRId64 "\n",
           input.index, outputs_[output].record.back().start_instruction, instr_ord);
    outputs_[output].record.back().stop_instruction = instr_ord;
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::set_cur_input(output_ordinal_t output,
                                                        input_ordinal_t input)
{
    assert(output >= 0 && output < static_cast<output_ordinal_t>(outputs_.size()));
    // 'input' might be INVALID_INPUT_ORDINAL.
    assert(input < static_cast<input_ordinal_t>(inputs_.size()));
    int prev_input = outputs_[output].cur_input;
    if (prev_input >= 0) {
        if (options_.mapping == MAP_TO_ANY_OUTPUT && prev_input != input)
            ready_.push(prev_input);
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
scheduler_tmpl_t<RecordType, ReaderType>::pick_next_input(output_ordinal_t output)
{
    bool need_lock =
        options_.mapping == MAP_TO_ANY_OUTPUT || options_.mapping == MAP_AS_PREVIOUSLY;
    auto scoped_lock = need_lock ? std::unique_lock<std::mutex>(sched_lock_)
                                 : std::unique_lock<std::mutex>();
    int prev_index = outputs_[output].cur_input;
    input_ordinal_t index = INVALID_INPUT_ORDINAL;
    while (true) {
        if (index < 0) {
            if (options_.mapping == MAP_AS_PREVIOUSLY) {
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
                        // When we skip our separator+timestamp markers are at the
                        // prior instr ord so do not wait for that.
                        outputs_[output].record[outputs_[output].record_index].type !=
                            schedule_record_t::SKIP) {
                        // Some other output stream has not advanced far enough, and we do
                        // not support multiple positions in one input stream: we wait.
                        // XXX i#5843: We may want to provide a kernel-mediated wait
                        // feature so a multi-threaded simulator doesn't have to do a
                        // spinning poll loop.
                        VPRINT(this, 3,
                               "next_record[%d]: waiting for input %d instr #%" PRId64
                               "\n",
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
                if (segment.type == schedule_record_t::SYNTHETIC_END) {
                    std::lock_guard<std::mutex> lock(*inputs_[index].lock);
                    // We're past the final region of interest and we need to insert
                    // a synthetic thread exit record.
                    inputs_[index].queue.push_back(
                        create_thread_exit(inputs_[index].tid));
                    inputs_[index].at_eof = true;
                    VPRINT(this, 2, "early end for input %d\n", index);
                    // We're done with this entry so move to and past it.
                    outputs_[output].record_index += 2;
                    return sched_type_t::STATUS_SKIPPED;
                } else if (segment.type == schedule_record_t::SKIP) {
                    std::lock_guard<std::mutex> lock(*inputs_[index].lock);
                    uint64_t cur_instr = inputs_[index].reader->get_instruction_ordinal();
                    VPRINT(this, 2,
                           "skipping from %" PRId64 " to %" PRId64
                           " instrs for schedule\n",
                           cur_instr, segment.stop_instruction);
                    auto status = skip_instructions(output, inputs_[index],
                                                    segment.stop_instruction - cur_instr -
                                                        1 /*exclusive*/);
                    // Increment the region to get window id markers with ordinals.
                    inputs_[index].cur_region++;
                    if (status != sched_type_t::STATUS_SKIPPED)
                        return sched_type_t::STATUS_INVALID;
                    // We're done with the skip so move to and past it.
                    outputs_[output].record_index += 2;
                    return sched_type_t::STATUS_SKIPPED;
                } else {
                    VPRINT(this, 2,
                           "next_record[%d]: advancing to input %d instr #%" PRId64 "\n",
                           output, index, segment.start_instruction);
                }
                ++outputs_[output].record_index;
            } else if (options_.mapping == MAP_TO_ANY_OUTPUT) {
                if (ready_.empty()) {
                    std::lock_guard<std::mutex> lock(*inputs_[prev_index].lock);
                    if (inputs_[prev_index].at_eof)
                        return sched_type_t::STATUS_EOF;
                    else
                        index = prev_index; // Go back to prior.
                } else {
                    // TODO i#5843: Add core binding and priority support.
                    index = ready_.front();
                    ready_.pop();
                }
            } else if (options_.deps == DEPENDENCY_TIMESTAMPS) {
                // TODO i#5843: This should require a lock for >1 outputs too.
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
            if (options_.schedule_record_ostream != nullptr)
                close_schedule_segment(output, inputs_[outputs_[output].cur_input]);
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
                                                      input_info_t *&input)
{
    if (outputs_[output].waiting) {
        sched_type_t::stream_status_t res = pick_next_input(output);
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
    if (outputs_[output].speculating) {
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
                sched_type_t::stream_status_t res = pick_next_input(output);
                if (res != sched_type_t::STATUS_OK)
                    return res;
                input = &inputs_[outputs_[output].cur_input];
                lock = std::unique_lock<std::mutex>(*input->lock);
                continue;
            } else {
                record = **input->reader;
            }
        }
        bool need_new_input = false;
        if (options_.mapping == MAP_AS_PREVIOUSLY) {
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
        } else if (options_.mapping == MAP_TO_ANY_OUTPUT &&
                   options_.quantum_unit == QUANTUM_INSTRUCTIONS &&
                   record_type_is_instr(record)) {
            // TODO i#5843: We also want to swap on blocking syscalls.
            ++input->instrs_in_quantum;
            if (input->instrs_in_quantum > options_.quantum_duration)
                need_new_input = true;
        }
        if (options_.deps == DEPENDENCY_TIMESTAMPS &&
            record_type_is_timestamp(record, input->next_timestamp))
            need_new_input = true;
        if (need_new_input) {
            int prev_input = outputs_[output].cur_input;
            lock.unlock();
            sched_type_t::stream_status_t res = pick_next_input(output);
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
    if (!outputs_[output].speculating && queue_current_record) {
        inputs_[outputs_[output].cur_input].queue.push_back(outputs_[output].last_record);
    }
    outputs_[output].speculating = true;
    outputs_[output].speculate_pc = start_address;
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::stop_speculation(output_ordinal_t output)
{
    if (!outputs_[output].speculating)
        return sched_type_t::STATUS_INVALID;
    outputs_[output].speculating = false;
    return sched_type_t::STATUS_OK;
}

template class scheduler_tmpl_t<memref_t, reader_t>;
template class scheduler_tmpl_t<trace_entry_t, dynamorio::drmemtrace::record_reader_t>;

} // namespace drmemtrace
} // namespace dynamorio
