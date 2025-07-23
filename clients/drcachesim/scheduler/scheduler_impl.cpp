/* **********************************************************
 * Copyright (c) 2023-2025 Google, Inc.  All rights reserved.
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
#include "scheduler_impl.h"

#include <inttypes.h>
#include <stdint.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cinttypes>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "flexible_queue.h"
#include "memref.h"
#include "memtrace_stream.h"
#include "mutex_dbg_owned.h"
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

std::string
replay_file_checker_t::check(archive_istream_t *infile)
{
    // Ensure we don't have repeated idle records, which balloon the file size.
    scheduler_impl_t::schedule_record_t record;
    bool prev_was_idle = false;
    while (infile->read(reinterpret_cast<char *>(&record), sizeof(record))) {
        if (record.type == scheduler_impl_t::schedule_record_t::IDLE ||
            record.type == scheduler_impl_t::schedule_record_t::IDLE_BY_COUNT) {
            if (prev_was_idle)
                return "Error: consecutive idle records";
            prev_was_idle = true;
        } else
            prev_was_idle = false;
    }
    return "";
}

/****************************************************************
 * Specializations for scheduler_tmpl_impl_t<reader_t>, aka scheduler_impl_t.
 */

template <>
std::unique_ptr<reader_t>
scheduler_impl_tmpl_t<memref_t, reader_t>::get_default_reader()
{
    return std::unique_ptr<default_file_reader_t>(new default_file_reader_t());
}

template <>
std::unique_ptr<reader_t>
scheduler_impl_tmpl_t<memref_t, reader_t>::get_reader(const std::string &path,
                                                      int verbosity)
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
                fname == DRMEMTRACE_ENCODING_FILENAME || fname == DRMEMTRACE_V2P_FILENAME)
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
scheduler_impl_tmpl_t<memref_t, reader_t>::record_type_has_tid(memref_t record,
                                                               memref_tid_t &tid)
{
    if (record.marker.tid == INVALID_THREAD_ID)
        return false;
    tid = record.marker.tid;
    return true;
}

template <>
bool
scheduler_impl_tmpl_t<memref_t, reader_t>::record_type_has_pid(memref_t record,
                                                               memref_pid_t &pid)
{
    if (record.marker.pid == INVALID_PID)
        return false;
    pid = record.marker.pid;
    return true;
}

template <>
void
scheduler_impl_tmpl_t<memref_t, reader_t>::record_type_set_tid(memref_t &record,
                                                               memref_tid_t tid)
{
    record.marker.tid = tid;
}

template <>
void
scheduler_impl_tmpl_t<memref_t, reader_t>::record_type_set_pid(memref_t &record,
                                                               memref_pid_t pid)
{
    record.marker.pid = pid;
}

template <>
bool
scheduler_impl_tmpl_t<memref_t, reader_t>::record_type_is_instr(memref_t record,
                                                                addr_t *pc, size_t *size)
{
    if (type_is_instr(record.instr.type)) {
        if (pc != nullptr)
            *pc = record.instr.addr;
        if (size != nullptr)
            *size = record.instr.size;
        return true;
    }
    return false;
}

template <>
bool
scheduler_impl_tmpl_t<memref_t, reader_t>::record_type_is_indirect_branch_instr(
    memref_t &record, bool &has_indirect_branch_target, addr_t set_indirect_branch_target)
{
    has_indirect_branch_target = false;
    if (type_is_instr_branch(record.instr.type) &&
        !type_is_instr_direct_branch(record.instr.type)) {
        has_indirect_branch_target = true;
        // XXX: Zero may not be the perfect sentinel value as an app may have instrs that
        // jump to pc=0 (and later handle the fault). But current uses of
        // record_type_is_indirect_branch_instr use only non-zero values for
        // set_indirect_branch_target based on actual pcs seen in the trace.
        if (set_indirect_branch_target != 0) {
            record.instr.indirect_branch_target = set_indirect_branch_target;
        }
        return true;
    }
    return false;
}

template <>
bool
scheduler_impl_tmpl_t<memref_t, reader_t>::record_type_is_encoding(memref_t record)
{
    // There are no separate memref_t encoding records: encoding info is
    // inside instruction records.
    return false;
}

template <>
bool
scheduler_impl_tmpl_t<memref_t, reader_t>::record_type_is_instr_boundary(
    memref_t record, memref_t prev_record)
{
    return record_type_is_instr(record);
}

template <>
bool
scheduler_impl_tmpl_t<memref_t, reader_t>::record_type_is_thread_exit(memref_t record)
{
    return record.exit.type == TRACE_TYPE_THREAD_EXIT;
}

template <>
bool
scheduler_impl_tmpl_t<memref_t, reader_t>::record_type_is_marker(
    memref_t record, trace_marker_type_t &type, uintptr_t &value)
{
    if (record.marker.type != TRACE_TYPE_MARKER)
        return false;
    type = record.marker.marker_type;
    value = record.marker.marker_value;
    return true;
}

template <>
bool
scheduler_impl_tmpl_t<memref_t, reader_t>::record_type_is_non_marker_header(
    memref_t record)
{
    // Non-marker trace_entry_t headers turn into markers or are
    // hidden, so there are none in a memref_t stream.
    return false;
}

template <>
bool
scheduler_impl_tmpl_t<memref_t, reader_t>::record_type_set_marker_value(memref_t &record,
                                                                        uintptr_t value)
{
    if (record.marker.type != TRACE_TYPE_MARKER)
        return false;
    record.marker.marker_value = value;
    return true;
}

template <>
bool
scheduler_impl_tmpl_t<memref_t, reader_t>::record_type_is_timestamp(memref_t record,
                                                                    uintptr_t &value)
{
    if (record.marker.type != TRACE_TYPE_MARKER ||
        record.marker.marker_type != TRACE_MARKER_TYPE_TIMESTAMP)
        return false;
    value = record.marker.marker_value;
    return true;
}

template <>
bool
scheduler_impl_tmpl_t<memref_t, reader_t>::record_type_is_invalid(memref_t record)
{
    return record.instr.type == TRACE_TYPE_INVALID;
}

template <>
memref_t
scheduler_impl_tmpl_t<memref_t, reader_t>::create_region_separator_marker(
    memref_tid_t tid, uintptr_t value)
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
scheduler_impl_tmpl_t<memref_t, reader_t>::create_thread_exit(memref_tid_t tid)
{
    memref_t record = {};
    record.exit.type = TRACE_TYPE_THREAD_EXIT;
    // XXX i#5843: We have .pid as 0 for now; worth trying to fill it in?
    record.exit.tid = tid;
    return record;
}

template <>
memref_t
scheduler_impl_tmpl_t<memref_t, reader_t>::create_invalid_record()
{
    memref_t record = {};
    record.instr.type = TRACE_TYPE_INVALID;
    return record;
}

template <>
void
scheduler_impl_tmpl_t<memref_t, reader_t>::print_record(const memref_t &record)
{
    fprintf(stderr, "tid=%" PRId64 " type=%d", record.instr.tid, record.instr.type);
    if (type_is_instr(record.instr.type))
        fprintf(stderr, " pc=0x%zx size=%zu", record.instr.addr, record.instr.size);
    else if (record.marker.type == TRACE_TYPE_MARKER) {
        fprintf(stderr, " marker=%d val=%zu", record.marker.marker_type,
                record.marker.marker_value);
    }
    fprintf(stderr, "\n");
}

template <>
void
scheduler_impl_tmpl_t<memref_t, reader_t>::insert_switch_tid_pid(input_info_t &info)
{
    // We do nothing, as every record has a tid from the separate inputs.
}

template <>
template <>
typename scheduler_tmpl_t<memref_t, reader_t>::switch_type_t
scheduler_impl_tmpl_t<memref_t, reader_t>::invalid_kernel_sequence_key()
{
    return switch_type_t::SWITCH_INVALID;
}

template <>
template <>
int
scheduler_impl_tmpl_t<memref_t, reader_t>::invalid_kernel_sequence_key()
{
    // System numbers are small non-negative integers.
    return -1;
}

/******************************************************************************
 * Specializations for scheduler_impl_tmpl_t<record_reader_t>, aka
 * record_scheduler_impl_t.
 */

template <>
std::unique_ptr<dynamorio::drmemtrace::record_reader_t>
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::get_default_reader()
{
    return std::unique_ptr<default_record_file_reader_t>(
        new default_record_file_reader_t());
}

template <>
std::unique_ptr<dynamorio::drmemtrace::record_reader_t>
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::get_reader(const std::string &path,
                                                                  int verbosity)
{
    // TODO i#5675: Add support for other file formats.
    if (ends_with(path, ".sz"))
        return nullptr;
#ifdef HAS_ZIP
    if (ends_with(path, ".zip")) {
        return std::unique_ptr<dynamorio::drmemtrace::record_reader_t>(
            new zipfile_record_file_reader_t(path, verbosity));
    }
#endif
    return std::unique_ptr<dynamorio::drmemtrace::record_reader_t>(
        new default_record_file_reader_t(path, verbosity));
}

template <>
bool
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::record_type_has_tid(
    trace_entry_t record, memref_tid_t &tid)
{
    if (record.type != TRACE_TYPE_THREAD)
        return false;
    tid = static_cast<memref_tid_t>(record.addr);
    return true;
}

template <>
bool
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::record_type_has_pid(
    trace_entry_t record, memref_pid_t &pid)
{
    if (record.type != TRACE_TYPE_PID)
        return false;
    pid = static_cast<memref_pid_t>(record.addr);
    return true;
}

template <>
void
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::record_type_set_tid(
    trace_entry_t &record, memref_tid_t tid)
{
    if (record.type != TRACE_TYPE_THREAD)
        return;
    record.addr = static_cast<addr_t>(tid);
}

template <>
void
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::record_type_set_pid(
    trace_entry_t &record, memref_pid_t pid)
{
    if (record.type != TRACE_TYPE_PID)
        return;
    record.addr = static_cast<addr_t>(pid);
}

template <>
bool
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::record_type_is_instr(
    trace_entry_t record, addr_t *pc, size_t *size)
{
    if (type_is_instr(static_cast<trace_type_t>(record.type))) {
        if (pc != nullptr)
            *pc = record.addr;
        if (size != nullptr)
            *size = record.size;
        return true;
    }
    return false;
}

template <>
bool
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::
    record_type_is_indirect_branch_instr(trace_entry_t &record,
                                         bool &has_indirect_branch_target,
                                         addr_t set_indirect_branch_target_unused)
{
    has_indirect_branch_target = false;
    // Cannot set the provided indirect branch target here because
    // a prior trace_entry_t would have it.
    return type_is_instr_branch(static_cast<trace_type_t>(record.type)) &&
        !type_is_instr_direct_branch(static_cast<trace_type_t>(record.type));
}

template <>
bool
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::record_type_is_encoding(
    trace_entry_t record)
{
    return static_cast<trace_type_t>(record.type) == TRACE_TYPE_ENCODING;
}

template <>
typename scheduler_tmpl_t<trace_entry_t, record_reader_t>::stream_status_t
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::unread_last_record(
    typename sched_type_t::output_ordinal_t output, trace_entry_t &record,
    input_info_t *&input)
{
    // See the general unread_last_record() below: we don't support this as
    // we can't provide the prev-prev record for record_type_is_instr_boundary().
    return sched_type_t::STATUS_NOT_IMPLEMENTED;
}

template <>
bool
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::record_type_is_marker(
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
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::record_type_is_non_marker_header(
    trace_entry_t record)
{
    return record.type == TRACE_TYPE_HEADER || record.type == TRACE_TYPE_THREAD ||
        record.type == TRACE_TYPE_PID;
}

template <>
bool
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::record_type_is_instr_boundary(
    trace_entry_t record, trace_entry_t prev_record)
{
    // Don't advance past encodings or target markers and split them from their
    // associated instr.
    return (record_type_is_instr(record) ||
            record_reader_t::record_is_pre_instr(&record)) &&
        !record_reader_t::record_is_pre_instr(&prev_record);
}

template <>
bool
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::record_type_is_thread_exit(
    trace_entry_t record)
{
    return record.type == TRACE_TYPE_THREAD_EXIT;
}

template <>
bool
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::record_type_set_marker_value(
    trace_entry_t &record, uintptr_t value)
{
    if (record.type != TRACE_TYPE_MARKER)
        return false;
    record.addr = value;
    return true;
}

template <>
bool
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::record_type_is_timestamp(
    trace_entry_t record, uintptr_t &value)
{
    if (record.type != TRACE_TYPE_MARKER ||
        static_cast<trace_marker_type_t>(record.size) != TRACE_MARKER_TYPE_TIMESTAMP)
        return false;
    value = record.addr;
    return true;
}

template <>
bool
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::record_type_is_invalid(
    trace_entry_t record)
{
    return static_cast<trace_type_t>(record.type) == TRACE_TYPE_INVALID;
}

template <>
trace_entry_t
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::create_region_separator_marker(
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
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::create_thread_exit(
    memref_tid_t tid)
{
    trace_entry_t record;
    record.type = TRACE_TYPE_THREAD_EXIT;
    record.size = sizeof(tid);
    record.addr = static_cast<addr_t>(tid);
    return record;
}

template <>
trace_entry_t
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::create_invalid_record()
{
    trace_entry_t record;
    record.type = TRACE_TYPE_INVALID;
    record.size = 0;
    record.addr = 0;
    return record;
}

template <>
void
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::print_record(
    const trace_entry_t &record)
{
    fprintf(stderr, "type=%d size=%d addr=0x%zx\n", record.type, record.size,
            record.addr);
}

template <>
void
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::insert_switch_tid_pid(
    input_info_t &input)
{
    // We may not have the input's pid if read_inputs_in_init was set to false,
    // which happens today only in IPC readers which doesn't use this path.
    assert(input.pid != INVALID_PID);
    assert(input.tid != INVALID_THREAD_ID);

    // We need explicit tid,pid records so reader_t will see the new context.
    // We insert at the front, so we have reverse order.
    trace_entry_t pid;
    pid.type = TRACE_TYPE_PID;
    pid.size = 0;
    pid.addr = static_cast<addr_t>(input.pid);

    trace_entry_t tid;
    tid.type = TRACE_TYPE_THREAD;
    tid.size = 0;
    tid.addr = static_cast<addr_t>(input.tid);

    input.queue.push_front(pid);
    input.queue.push_front(tid);
}

template <>
template <>
typename scheduler_tmpl_t<trace_entry_t, record_reader_t>::switch_type_t
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::invalid_kernel_sequence_key()
{
    return switch_type_t::SWITCH_INVALID;
}

template <>
template <>
int
scheduler_impl_tmpl_t<trace_entry_t, record_reader_t>::invalid_kernel_sequence_key()
{
    // System numbers are small non-negative integers.
    return -1;
}

/***************************************************************************
 * Scheduler.
 */

template <typename RecordType, typename ReaderType>
void
scheduler_impl_tmpl_t<RecordType, ReaderType>::print_configuration()
{
    VPRINT(this, 1, "Scheduler configuration:\n");
    VPRINT(this, 1, "  %-25s : %zu\n", "Inputs", inputs_.size());
    VPRINT(this, 1, "  %-25s : %zu\n", "Outputs", outputs_.size());
    VPRINT(this, 1, "  %-25s : %d\n", "mapping", options_.mapping);
    VPRINT(this, 1, "  %-25s : %d\n", "deps", options_.deps);
    VPRINT(this, 1, "  %-25s : 0x%08x\n", "flags", options_.flags);
    VPRINT(this, 1, "  %-25s : %d\n", "quantum_unit", options_.quantum_unit);
    VPRINT(this, 1, "  %-25s : %" PRIu64 "\n", "quantum_duration",
           options_.quantum_duration);
    VPRINT(this, 1, "  %-25s : %d\n", "verbosity", options_.verbosity);
    VPRINT(this, 1, "  %-25s : %p\n", "schedule_record_ostream",
           options_.schedule_record_ostream);
    VPRINT(this, 1, "  %-25s : %p\n", "schedule_replay_istream",
           options_.schedule_replay_istream);
    VPRINT(this, 1, "  %-25s : %p\n", "replay_as_traced_istream",
           options_.replay_as_traced_istream);
    VPRINT(this, 1, "  %-25s : %" PRIu64 "\n", "syscall_switch_threshold",
           options_.syscall_switch_threshold);
    VPRINT(this, 1, "  %-25s : %" PRIu64 "\n", "blocking_switch_threshold",
           options_.blocking_switch_threshold);
    VPRINT(this, 1, "  %-25s : %f\n", "block_time_scale", options_.block_time_scale);
    VPRINT(this, 1, "  %-25s : %" PRIu64 "\n", "block_time_max", options_.block_time_max);
    VPRINT(this, 1, "  %-25s : %s\n", "kernel_switch_trace_path",
           options_.kernel_switch_trace_path.c_str());
    VPRINT(this, 1, "  %-25s : %p\n", "kernel_switch_reader",
           options_.kernel_switch_reader.get());
    VPRINT(this, 1, "  %-25s : %p\n", "kernel_switch_reader_end",
           options_.kernel_switch_reader_end.get());
    VPRINT(this, 1, "  %-25s : %d\n", "single_lockstep_output",
           options_.single_lockstep_output);
    VPRINT(this, 1, "  %-25s : %d\n", "randomize_next_input",
           options_.randomize_next_input);
    VPRINT(this, 1, "  %-25s : %d\n", "read_inputs_in_init",
           options_.read_inputs_in_init);
    VPRINT(this, 1, "  %-25s : %d\n", "honor_direct_switches",
           options_.honor_direct_switches);
    VPRINT(this, 1, "  %-25s : %f\n", "time_units_per_us", options_.time_units_per_us);
    VPRINT(this, 1, "  %-25s : %" PRIu64 "\n", "quantum_duration_us",
           options_.quantum_duration_us);
    VPRINT(this, 1, "  %-25s : %" PRIu64 "\n", "quantum_duration_instrs",
           options_.quantum_duration_instrs);
    VPRINT(this, 1, "  %-25s : %f\n", "block_time_multiplier",
           options_.block_time_multiplier);
    VPRINT(this, 1, "  %-25s : %" PRIu64 "\n", "block_time_max_us",
           options_.block_time_max_us);
    VPRINT(this, 1, "  %-25s : %" PRIu64 "\n", "migration_threshold_us",
           options_.migration_threshold_us);
    VPRINT(this, 1, "  %-25s : %" PRIu64 "\n", "rebalance_period_us",
           options_.rebalance_period_us);
    VPRINT(this, 1, "  %-25s : %d\n", "honor_infinite_timeouts",
           options_.honor_infinite_timeouts);
    VPRINT(this, 1, "  %-25s : %f\n", "exit_if_fraction_inputs_left",
           options_.exit_if_fraction_inputs_left);
    VPRINT(this, 1, "  %-25s : %s\n", "kernel_syscall_trace_path",
           options_.kernel_syscall_trace_path.c_str());
    VPRINT(this, 1, "  %-25s : %p\n", "kernel_syscall_reader",
           options_.kernel_syscall_reader.get());
    VPRINT(this, 1, "  %-25s : %p\n", "kernel_syscall_reader_end",
           options_.kernel_syscall_reader_end.get());
}

template <typename RecordType, typename ReaderType>
scheduler_impl_tmpl_t<RecordType, ReaderType>::~scheduler_impl_tmpl_t()
{
    for (unsigned int i = 0; i < outputs_.size(); ++i) {
        VPRINT(this, 1, "Stats for output #%d\n", i);
        VPRINT(this, 1, "  %-35s: %9" PRId64 "\n", "Switch input->input",
               outputs_[i].stats[memtrace_stream_t::SCHED_STAT_SWITCH_INPUT_TO_INPUT]);
        VPRINT(this, 1, "  %-35s: %9" PRId64 "\n", "Switch input->idle",
               outputs_[i].stats[memtrace_stream_t::SCHED_STAT_SWITCH_INPUT_TO_IDLE]);
        VPRINT(this, 1, "  %-35s: %9" PRId64 "\n", "Switch idle->input",
               outputs_[i].stats[memtrace_stream_t::SCHED_STAT_SWITCH_IDLE_TO_INPUT]);
        VPRINT(this, 1, "  %-35s: %9" PRId64 "\n", "Switch nop",
               outputs_[i].stats[memtrace_stream_t::SCHED_STAT_SWITCH_NOP]);
        VPRINT(this, 1, "  %-35s: %9" PRId64 "\n", "Quantum preempts",
               outputs_[i].stats[memtrace_stream_t::SCHED_STAT_QUANTUM_PREEMPTS]);
        VPRINT(this, 1, "  %-35s: %9" PRId64 "\n", "Direct switch attempts",
               outputs_[i].stats[memtrace_stream_t::SCHED_STAT_DIRECT_SWITCH_ATTEMPTS]);
        VPRINT(this, 1, "  %-35s: %9" PRId64 "\n", "Direct switch successes",
               outputs_[i].stats[memtrace_stream_t::SCHED_STAT_DIRECT_SWITCH_SUCCESSES]);
        VPRINT(this, 1, "  %-35s: %9" PRId64 "\n", "Migrations",
               outputs_[i].stats[memtrace_stream_t::SCHED_STAT_MIGRATIONS]);
        VPRINT(this, 1, "  %-35s: %9" PRId64 "\n", "Runqueue steals",
               outputs_[i].stats[memtrace_stream_t::SCHED_STAT_RUNQUEUE_STEALS]);
        VPRINT(this, 1, "  %-35s: %9" PRId64 "\n", "Runqueue rebalances",
               outputs_[i].stats[memtrace_stream_t::SCHED_STAT_RUNQUEUE_REBALANCES]);
        VPRINT(this, 1, "  %-35s: %9" PRId64 "\n", "Output limits hit",
               outputs_[i].stats[memtrace_stream_t::SCHED_STAT_HIT_OUTPUT_LIMIT]);
#ifndef NDEBUG
        VPRINT(this, 1, "  %-35s: %9" PRId64 "\n", "Runqueue lock acquired",
               outputs_[i].ready_queue.lock->get_count_acquired());
        VPRINT(this, 1, "  %-35s: %9" PRId64 "\n", "Runqueue lock contended",
               outputs_[i].ready_queue.lock->get_count_contended());
#endif
    }
}

template <typename RecordType, typename ReaderType>
bool
scheduler_impl_tmpl_t<RecordType, ReaderType>::check_valid_input_limits(
    const typename sched_type_t::input_workload_t &workload,
    input_reader_info_t &reader_info)
{
    if (!workload.only_shards.empty()) {
        for (input_ordinal_t ord : workload.only_shards) {
            if (ord < 0 || ord >= static_cast<input_ordinal_t>(reader_info.input_count)) {
                error_string_ = "only_shards entry " + std::to_string(ord) +
                    " out of bounds for a shard ordinal";
                return false;
            }
        }
    }
    if (!workload.only_threads.empty()) {
        for (memref_tid_t tid : workload.only_threads) {
            if (reader_info.unfiltered_tids.find(tid) ==
                reader_info.unfiltered_tids.end()) {
                error_string_ = "only_threads entry " + std::to_string(tid) +
                    " not found in workload inputs";
                return false;
            }
        }
    }
    return true;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::init(
    std::vector<typename scheduler_tmpl_t<RecordType, ReaderType>::input_workload_t>
        &workload_inputs,
    int output_count,
    typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_options_t options)
{
    options_ = std::move(options);
    verbosity_ = options_.verbosity;
    // workload_inputs is not const so we can std::move readers out of it.
    for (int workload_idx = 0; workload_idx < static_cast<int>(workload_inputs.size());
         ++workload_idx) {
        auto &workload = workload_inputs[workload_idx];
        if (workload.struct_size != sizeof(input_workload_t))
            return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
        if (!workload.only_threads.empty() && !workload.only_shards.empty())
            return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
        std::vector<input_ordinal_t> inputs_in_workload;
        input_reader_info_t reader_info;
        reader_info.only_threads = workload.only_threads;
        reader_info.only_shards = workload.only_shards;
        reader_info.first_input_ordinal = static_cast<input_ordinal_t>(inputs_.size());
        if (workload.path.empty()) {
            if (workload.readers.empty())
                return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
            reader_info.input_count = workload.readers.size();
            for (int i = 0; i < static_cast<int>(workload.readers.size()); ++i) {
                auto &reader = workload.readers[i];
                if (!reader.reader || !reader.end)
                    return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
                reader_info.unfiltered_tids.insert(reader.tid);
                if (!workload.only_threads.empty() &&
                    workload.only_threads.find(reader.tid) == workload.only_threads.end())
                    continue;
                if (!workload.only_shards.empty() &&
                    workload.only_shards.find(i) == workload.only_shards.end())
                    continue;
                int index = static_cast<input_ordinal_t>(inputs_.size());
                inputs_.emplace_back();
                input_info_t &input = inputs_.back();
                input.index = index;
                input.workload = workload_idx;
                inputs_in_workload.push_back(index);
                input.tid = reader.tid;
                input.reader = std::move(reader.reader);
                input.reader_end = std::move(reader.end);
                input.needs_init = true;
                reader_info.tid2input[input.tid] = input.index;
                tid2input_[workload_tid_t(workload_idx, input.tid)] = index;
            }
        } else {
            if (!workload.readers.empty())
                return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
            scheduler_status_t res = open_readers(workload.path, reader_info);
            if (res != sched_type_t::STATUS_SUCCESS)
                return res;
            for (const auto &it : reader_info.tid2input) {
                inputs_[it.second].workload = workload_idx;
                inputs_in_workload.push_back(it.second);
                tid2input_[workload_tid_t(workload_idx, it.first)] = it.second;
            }
        }
        int output_limit = 0;
        if (workload.struct_size > offsetof(workload_info_t, output_limit))
            output_limit = workload.output_limit;
        workloads_.emplace_back(output_limit, std::move(inputs_in_workload));
        if (!check_valid_input_limits(workload, reader_info))
            return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
        if (!workload.times_of_interest.empty()) {
            for (const auto &modifiers : workload.thread_modifiers) {
                if (!modifiers.regions_of_interest.empty()) {
                    // We do not support mixing with other ROI specifiers.
                    return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
                }
            }
            scheduler_status_t status =
                create_regions_from_times(reader_info.tid2input, workload);
            if (status != sched_type_t::STATUS_SUCCESS)
                return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
        }
        for (const auto &modifiers : workload.thread_modifiers) {
            // We can't actually use modifiers.struct_size to provide binary
            // backward compatibility due to C++ not supporting offsetof with
            // non-standard-layout types.  So we ignore struct_size and only
            // provide source compatibility.
            // Vector of ordinals into input for this workload.
            std::vector<int> which_workload_inputs;
            if (modifiers.tids.empty() && modifiers.shards.empty()) {
                // Apply to all inputs that have not already been modified.
                for (int i = 0; i < static_cast<int>(reader_info.input_count); ++i) {
                    if (!inputs_[reader_info.first_input_ordinal + i].has_modifier)
                        which_workload_inputs.push_back(i);
                }
            } else if (!modifiers.tids.empty()) {
                if (!modifiers.shards.empty()) {
                    error_string_ =
                        "Cannot set both tids and shards in input_thread_info_t";
                    return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
                }
                for (memref_tid_t tid : modifiers.tids) {
                    auto it = reader_info.tid2input.find(tid);
                    if (it == reader_info.tid2input.end()) {
                        error_string_ =
                            "Cannot find tid " + std::to_string(tid) + " for modifier";
                        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
                    }
                    which_workload_inputs.push_back(it->second -
                                                    reader_info.first_input_ordinal);
                }
            } else if (!modifiers.shards.empty()) {
                which_workload_inputs = modifiers.shards;
            }

            // We assume the overhead of copying the modifiers for every thread is
            // not high and the simplified code is worthwhile.
            for (int local_index : which_workload_inputs) {
                int index = local_index + reader_info.first_input_ordinal;
                input_info_t &input = inputs_[index];
                input.has_modifier = true;
                // Check for valid bindings.
                for (output_ordinal_t bind : modifiers.output_binding) {
                    if (bind < 0 || bind >= output_count) {
                        error_string_ =
                            "output_binding " + std::to_string(bind) + " out of bounds";
                        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
                    }
                }
                // It is common enough for every output to be passed (as part of general
                // code with a full set as a default value) that it is worth
                // detecting and ignoring in order to avoid hitting binding-handling
                // code and save time in initial placement and runqueue code.
                if (modifiers.output_binding.size() < static_cast<size_t>(output_count)) {
                    input.binding = modifiers.output_binding;
                }
                input.priority = modifiers.priority;
                for (size_t i = 0; i < modifiers.regions_of_interest.size(); ++i) {
                    const auto &range = modifiers.regions_of_interest[i];
                    VPRINT(this, 3, "ROI #%zu for input %d: [%" PRIu64 ", %" PRIu64 ")\n",
                           i, index, range.start_instruction, range.stop_instruction);
                    if (range.start_instruction == 0 ||
                        (range.stop_instruction < range.start_instruction &&
                         range.stop_instruction != 0)) {
                        error_string_ = "invalid start/stop range in regions of interest";
                        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
                    }
                    if (i == 0)
                        continue;
                    if (range.start_instruction <=
                        modifiers.regions_of_interest[i - 1].stop_instruction) {
                        error_string_ = "gap required between regions of interest";
                        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
                    }
                }
                input.regions_of_interest = modifiers.regions_of_interest;
            }
        }
    }

    // Legacy field support.
    scheduler_status_t res = legacy_field_support();
    if (res != sched_type_t::STATUS_SUCCESS)
        return res;

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
    if (options_.single_lockstep_output) {
        global_stream_ =
            std::unique_ptr<stream_t>(new stream_t(this, 0, verbosity_, output_count));
    }
    for (int i = 0; i < output_count; ++i) {
        outputs_.emplace_back(
            this, i,
            TESTANY(sched_type_t::SCHEDULER_SPECULATE_NOPS, options_.flags)
                ? spec_type_t::USE_NOPS
                // TODO i#5843: Add more flags for other options.
                : spec_type_t::LAST_FROM_TRACE,
            static_cast<int>(get_time_micros()), create_invalid_record(), verbosity_);
        if (options_.single_lockstep_output)
            outputs_.back().stream = global_stream_.get();
        if (options_.schedule_record_ostream != nullptr) {
            stream_status_t status = record_schedule_segment(
                i, schedule_record_t::VERSION, schedule_record_t::VERSION_CURRENT, 0, 0);
            if (status != sched_type_t::STATUS_OK) {
                error_string_ = "Failed to add version to recorded schedule";
                return sched_type_t::STATUS_ERROR_FILE_WRITE_FAILED;
            }
        }
    }

    VDO(this, 1, { print_configuration(); });

    live_input_count_.store(static_cast<int>(inputs_.size()), std::memory_order_release);

    res = read_switch_sequences();
    if (res != sched_type_t::STATUS_SUCCESS)
        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;

    res = read_syscall_sequences();
    if (res != sched_type_t::STATUS_SUCCESS)
        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;

    // Determine whether we need to read ahead in the inputs.  There are cases where we
    // do not want to do that as it would block forever if the inputs are not available
    // (e.g., online analysis IPC readers); it also complicates ordinals so we avoid it
    // if we can and enumerate all the cases that do need it.
    bool gather_timestamps = false;
    if (((options_.mapping == sched_type_t::MAP_AS_PREVIOUSLY ||
          options_.mapping == sched_type_t::MAP_TO_ANY_OUTPUT) &&
         options_.deps == sched_type_t::DEPENDENCY_TIMESTAMPS) ||
        (options_.mapping == sched_type_t::MAP_TO_RECORDED_OUTPUT &&
         options_.replay_as_traced_istream == nullptr && inputs_.size() > 1)) {
        gather_timestamps = true;
        if (!options_.read_inputs_in_init) {
            error_string_ = "Timestamp dependencies require read_inputs_in_init";
            return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
        }
    }
    // The filetype, if present, is before the first timestamp.  If we only need the
    // filetype we avoid going as far as the timestamp.
    bool gather_filetype = options_.read_inputs_in_init;
    if (gather_filetype || gather_timestamps) {
        res = this->get_initial_input_content(gather_timestamps);
        if (res != sched_type_t::STATUS_SUCCESS) {
            error_string_ = "Failed to read initial input contents for filetype";
            if (gather_timestamps)
                error_string_ += " and initial timestamps";
            return res;
        }
    }

    return set_initial_schedule();
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::legacy_field_support()
{
    if (options_.time_units_per_us == 0) {
        error_string_ = "time_units_per_us must be > 0";
        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
    }
    if (options_.quantum_duration > 0) {
        if (options_.struct_size > offsetof(scheduler_options_t, quantum_duration_us)) {
            error_string_ = "quantum_duration is deprecated; use quantum_duration_us and "
                            "time_units_per_us or quantum_duration_instrs";
            return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
        }
        if (options_.quantum_unit == sched_type_t::QUANTUM_INSTRUCTIONS) {
            options_.quantum_duration_instrs = options_.quantum_duration;
        } else {
            options_.quantum_duration_us =
                static_cast<uint64_t>(static_cast<double>(options_.quantum_duration) /
                                      options_.time_units_per_us);
            VPRINT(this, 2,
                   "Legacy support: setting quantum_duration_us to %" PRIu64 "\n",
                   options_.quantum_duration_us);
        }
    }
    if (options_.quantum_duration_us == 0) {
        error_string_ = "quantum_duration_us must be > 0";
        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
    }
    if (options_.block_time_scale > 0) {
        if (options_.struct_size > offsetof(scheduler_options_t, block_time_multiplier)) {
            error_string_ = "quantum_duration is deprecated; use block_time_multiplier "
                            "and time_units_per_us";
            return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
        }
        options_.block_time_multiplier =
            static_cast<double>(options_.block_time_scale) / options_.time_units_per_us;
        VPRINT(this, 2, "Legacy support: setting block_time_multiplier to %6.3f\n",
               options_.block_time_multiplier);
    }
    if (options_.block_time_multiplier == 0) {
        error_string_ = "block_time_multiplier must != 0";
        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
    }
    if (options_.block_time_max > 0) {
        if (options_.struct_size > offsetof(scheduler_options_t, block_time_max_us)) {
            error_string_ = "quantum_duration is deprecated; use block_time_max_us "
                            "and time_units_per_us";
            return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
        }
        options_.block_time_max_us = static_cast<uint64_t>(
            static_cast<double>(options_.block_time_max) / options_.time_units_per_us);
        VPRINT(this, 2, "Legacy support: setting block_time_max_us to %" PRIu64 "\n",
               options_.block_time_max_us);
    }
    if (options_.block_time_max_us == 0) {
        error_string_ = "block_time_max_us must be > 0";
        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
    }
    if (options_.exit_if_fraction_inputs_left < 0. ||
        options_.exit_if_fraction_inputs_left > 1.) {
        error_string_ = "exit_if_fraction_inputs_left must be 0..1";
        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
    }
    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
std::string
scheduler_impl_tmpl_t<RecordType, ReaderType>::recorded_schedule_component_name(
    output_ordinal_t output)
{
    static const char *const SCHED_CHUNK_PREFIX = "output.";
    std::ostringstream name;
    name << SCHED_CHUNK_PREFIX << std::setfill('0') << std::setw(4) << output;
    return name.str();
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::write_recorded_schedule()
{
    if (options_.schedule_record_ostream == nullptr)
        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
    for (int i = 0; i < static_cast<int>(outputs_.size()); ++i) {
        auto lock = acquire_scoped_output_lock_if_necessary(i);
        stream_status_t status =
            record_schedule_segment(i, schedule_record_t::FOOTER, 0, 0, 0);
        if (status != sched_type_t::STATUS_OK)
            return sched_type_t::STATUS_ERROR_FILE_WRITE_FAILED;
        std::string name = recorded_schedule_component_name(i);
        std::string err = options_.schedule_record_ostream->open_new_component(name);
        if (!err.empty()) {
            VPRINT(this, 1, "Failed to open component %s in record file: %s\n",
                   name.c_str(), err.c_str());
            return sched_type_t::STATUS_ERROR_FILE_WRITE_FAILED;
        }
        if (!options_.schedule_record_ostream->write(
                reinterpret_cast<char *>(outputs_[i].record.data()),
                outputs_[i].record.size() * sizeof(outputs_[i].record[0])))
            return sched_type_t::STATUS_ERROR_FILE_WRITE_FAILED;
    }
    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::create_regions_from_times(
    const std::unordered_map<memref_tid_t, int> &workload_tids,
    input_workload_t &workload)
{
    // First, read from the as-traced schedule file into data structures shared with
    // replay-as-traced.
    std::vector<std::vector<schedule_input_tracker_t>> input_sched(inputs_.size());
    // These are all unused.
    std::vector<std::set<uint64_t>> start2stop(inputs_.size());
    std::vector<std::vector<schedule_output_tracker_t>> all_sched;
    std::vector<output_ordinal_t> disk_ord2index;
    std::vector<uint64_t> disk_ord2cpuid;
    scheduler_status_t res = read_traced_schedule(input_sched, start2stop, all_sched,
                                                  disk_ord2index, disk_ord2cpuid);
    if (res != sched_type_t::STATUS_SUCCESS)
        return res;
    // Do not allow a replay mode to start later.
    options_.replay_as_traced_istream = nullptr;

    // Now create an interval tree of timestamps (with instr ordinals as payloads)
    // for each input. As our intervals do not overlap and have no gaps we need
    // no size, just the start address key.
    std::vector<std::map<uint64_t, uint64_t>> time_tree(inputs_.size());
    for (int input_idx = 0; input_idx < static_cast<input_ordinal_t>(inputs_.size());
         ++input_idx) {
        for (int sched_idx = 0;
             sched_idx < static_cast<int>(input_sched[input_idx].size()); ++sched_idx) {
            schedule_input_tracker_t &sched = input_sched[input_idx][sched_idx];
            VPRINT(this, 4, "as-read: input=%d start=%" PRId64 " time=%" PRId64 "\n",
                   input_idx, sched.start_instruction, sched.timestamp);
            time_tree[input_idx][sched.timestamp] = sched.start_instruction;
        }
    }

    // Finally, convert the requested time ranges into instr ordinal ranges.
    for (const auto &tid_it : workload_tids) {
        std::vector<range_t> instr_ranges;
        bool entire_tid = false;
        for (const auto &times : workload.times_of_interest) {
            uint64_t instr_start = 0, instr_end = 0;
            bool has_start = time_tree_lookup(time_tree[tid_it.second],
                                              times.start_timestamp, instr_start);
            bool has_end;
            if (times.stop_timestamp == 0)
                has_end = true;
            else {
                has_end = time_tree_lookup(time_tree[tid_it.second], times.stop_timestamp,
                                           instr_end);
            }
            if (has_start && has_end && instr_start == instr_end) {
                if (instr_start == 0 && instr_end == 0) {
                    entire_tid = true;
                } else {
                    ++instr_end;
                }
            }
            // If !has_start we'll include from 0.  The start timestamp will make it be
            // scheduled last but there will be no delay if no other thread is available.
            // If !has_end, instr_end will still be 0 which means the end of the trace.
            if (instr_start > 0 || instr_end > 0) {
                if (!instr_ranges.empty() &&
                    (instr_ranges.back().stop_instruction >= instr_start ||
                     instr_ranges.back().stop_instruction == 0)) {
                    error_string_ =
                        "times_of_interest are too close together: "
                        "corresponding instruction ordinals are overlapping or adjacent";
                    return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
                }
                instr_ranges.emplace_back(instr_start, instr_end);
                VPRINT(this, 2,
                       "tid %" PRIu64 " overlaps with times_of_interest [%" PRIu64
                       ", %" PRIu64 ") @ [%" PRIu64 ", %" PRIu64 ")\n",
                       tid_it.first, times.start_timestamp, times.stop_timestamp,
                       instr_start, instr_end);
            }
        }
        if (!entire_tid && instr_ranges.empty()) {
            // Exclude this thread completely.  We've already created its
            // inputs_ entry with cross-indices stored in other structures
            // so instead of trying to erase it we give it a max start point.
            VPRINT(this, 2,
                   "tid %" PRIu64 " has no overlap with any times_of_interest entry\n",
                   tid_it.first);
            instr_ranges.emplace_back(std::numeric_limits<uint64_t>::max(), 0);
        }
        if (entire_tid) {
            // No range is needed.
        } else {
            workload.thread_modifiers.emplace_back(instr_ranges);
            workload.thread_modifiers.back().tids.emplace_back(tid_it.first);
        }
    }
    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
bool
scheduler_impl_tmpl_t<RecordType, ReaderType>::time_tree_lookup(
    const std::map<uint64_t, uint64_t> &tree, uint64_t time, uint64_t &ordinal)
{
    auto it = tree.upper_bound(time);
    if (it == tree.begin() || it == tree.end()) {
        // We do not have a timestamp in the footer, so we assume any time
        // past the final known timestamp is too far and do not try to
        // fit into the final post-last-timestamp sequence.
        return false;
    }
    uint64_t upper_time = it->first;
    uint64_t upper_ord = it->second;
    it--;
    uint64_t lower_time = it->first;
    uint64_t lower_ord = it->second;
    double fraction = (time - lower_time) / static_cast<double>(upper_time - lower_time);
    double interpolate = lower_ord + fraction * (upper_ord - lower_ord);
    // We deliberately round down to ensure we include a system call that spans
    // the start time, so we'll get the right starting behavior for a thread that
    // should be blocked or unscheduled at this point in time (though the blocked
    // time might be too long as it starts before this target time).
    ordinal = static_cast<uint64_t>(interpolate);
    VPRINT(this, 3,
           "time2ordinal: time %" PRIu64 " => times [%" PRIu64 ", %" PRIu64
           ") ords [%" PRIu64 ", %" PRIu64 ") => interpolated %" PRIu64 "\n",
           time, lower_time, upper_time, lower_ord, upper_ord, ordinal);
    return true;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::read_traced_schedule(
    std::vector<std::vector<schedule_input_tracker_t>> &input_sched,
    std::vector<std::set<uint64_t>> &start2stop,
    std::vector<std::vector<schedule_output_tracker_t>> &all_sched,
    std::vector<output_ordinal_t> &disk_ord2index, std::vector<uint64_t> &disk_ord2cpuid)
{
    if (options_.replay_as_traced_istream == nullptr) {
        error_string_ = "Missing as-traced istream";
        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
    }

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
    // We initially number the outputs according to their order in the file, and then
    // sort by the stored cpuid below.
    // XXX i#6726: Should we support some direction from the user on this?  Simulation
    // may want to preserve the NUMA relationships and may need to set up its simulated
    // cores at init time, so it would prefer to partition by output stream identifier.
    // Maybe we could at least add the proposed memtrace_stream_t query for cpuid and
    // let it be called even before reading any records at all?
    output_ordinal_t cur_output = 0;
    uint64_t cur_cpu = std::numeric_limits<uint64_t>::max();
    while (options_.replay_as_traced_istream->read(reinterpret_cast<char *>(&entry),
                                                   sizeof(entry))) {
        if (entry.cpu != cur_cpu) {
            // This is a zipfile component boundary: one conmponent per cpu.
            if (cur_cpu != std::numeric_limits<uint64_t>::max()) {
                ++cur_output;
                if (options_.mapping == sched_type_t::MAP_TO_RECORDED_OUTPUT &&
                    !outputs_.empty() &&
                    cur_output >= static_cast<int>(outputs_.size())) {
                    error_string_ = "replay_as_traced_istream cpu count != output count";
                    return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
                }
            }
            cur_cpu = entry.cpu;
            disk_ord2cpuid.push_back(cur_cpu);
            disk_ord2index.push_back(cur_output);
        }
        input_ordinal_t input = tid2input[entry.thread];
        // The caller must fill in the stop ordinal in a second pass.
        uint64_t start = entry.start_instruction;
        uint64_t timestamp = entry.timestamp;
        // Some entries have no instructions (there is an entry for each timestamp, and
        // a signal can come in after a prior timestamp with no intervening instrs).
        if (all_sched.size() < static_cast<size_t>(cur_output + 1))
            all_sched.resize(cur_output + 1);
        if (!all_sched[cur_output].empty() &&
            input == all_sched[cur_output].back().input &&
            start == all_sched[cur_output].back().start_instruction) {
            VPRINT(this, 3,
                   "Output #%d: as-read segment #%zu has no instructions: skipping\n",
                   cur_output, all_sched[cur_output].size() - 1);
            continue;
        }
        all_sched[cur_output].emplace_back(true, input, start, timestamp);
        start2stop[input].insert(start);
        input_sched[input].emplace_back(cur_output, all_sched[cur_output].size() - 1,
                                        start, timestamp);
    }
    scheduler_status_t res =
        check_and_fix_modulo_problem_in_schedule(input_sched, start2stop, all_sched);
    if (res != sched_type_t::STATUS_SUCCESS)
        return res;
    return remove_zero_instruction_segments(input_sched, all_sched);
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::remove_zero_instruction_segments(
    std::vector<std::vector<schedule_input_tracker_t>> &input_sched,
    std::vector<std::vector<schedule_output_tracker_t>> &all_sched)

{
    // For a cpuid pair with no instructions in between, our
    // instruction-ordinal-based control points cannot model both sides.
    // For example:
    //    5   0:  1294139 <marker: page size 4096>
    //    6   0:  1294139 <marker: timestamp 13344214879969223>
    //    7   0:  1294139 <marker: tid 1294139 on core 2>
    //    8   0:  1294139 <marker: function==syscall #202>
    //    9   0:  1294139 <marker: function return value 0xffffffffffffff92>
    //   10   0:  1294139 <marker: system call failed: 110>
    //   11   0:  1294139 <marker: timestamp 13344214880209404>
    //   12   0:  1294139 <marker: tid 1294139 on core 2>
    //   13   1:  1294139 ifetch 3 byte(s) @ 0x0000563642cc5e75 8d 50 0b  lea...
    // That sequence has 2 different cpu_schedule file entries for that input
    // starting at instruction 0, which causes confusion when determining endpoints.
    // We just drop the older entry and keep the later one, which is the one bundled
    // with actual instructions.
    //
    // Should we not have instruction-based control points? The skip and
    // region-of-interest features were designed thinking about instructions, the more
    // natural unit for microarchitectural simulators.  It seemed like that was much more
    // usable for a user, and translated to other venues like PMU counts.  The scheduler
    // replay features were also designed that way.  But, that makes the infrastructure
    // messy as the underlying records are not built that way.  Xref i#6716 on an
    // instruction-based iterator.
    for (int input_idx = 0; input_idx < static_cast<input_ordinal_t>(inputs_.size());
         ++input_idx) {
        std::sort(
            input_sched[input_idx].begin(), input_sched[input_idx].end(),
            [](const schedule_input_tracker_t &l, const schedule_input_tracker_t &r) {
                return l.timestamp < r.timestamp;
            });
        uint64_t prev_start = 0;
        for (size_t i = 0; i < input_sched[input_idx].size(); ++i) {
            uint64_t start = input_sched[input_idx][i].start_instruction;
            assert(start >= prev_start);
            if (i > 0 && start == prev_start) {
                // Keep the newer one.
                VPRINT(this, 1, "Dropping same-input=%d same-start=%" PRIu64 " entry\n",
                       input_idx, start);
                all_sched[input_sched[input_idx][i - 1].output]
                         [static_cast<size_t>(
                              input_sched[input_idx][i - 1].output_array_idx)]
                             .valid = false;
                // If code after this used input_sched we would want to erase the
                // entry, but we have no further use so we leave it.
            }
            prev_start = start;
        }
    }
    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::check_and_fix_modulo_problem_in_schedule(
    std::vector<std::vector<schedule_input_tracker_t>> &input_sched,
    std::vector<std::set<uint64_t>> &start2stop,
    std::vector<std::vector<schedule_output_tracker_t>> &all_sched)

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
        std::sort(
            input_sched[input_idx].begin(), input_sched[input_idx].end(),
            [](const schedule_input_tracker_t &l, const schedule_input_tracker_t &r) {
                return l.timestamp < r.timestamp;
            });
        uint64_t prev_start = 0;
        uint64_t add_to_start = 0;
        bool in_order = true;
        for (schedule_input_tracker_t &sched : input_sched[input_idx]) {
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
                    return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
                }
            }
            // We could save space by not storing the early ones but we do need to
            // include all duplicates.
            if (timestamp2adjust[input_idx].find(sched.timestamp) !=
                timestamp2adjust[input_idx].end()) {
                error_string_ = "Same timestamps not supported for i#6107 workaround";
                return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
            }
            prev_start = sched.start_instruction;
            timestamp2adjust[input_idx][sched.timestamp] =
                sched.start_instruction + add_to_start;
            sched.start_instruction += add_to_start;
        }
    }
    if (!found_i6107)
        return sched_type_t::STATUS_SUCCESS;
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
            if (!segment.valid)
                continue;
            auto it = timestamp2adjust[segment.input].find(segment.timestamp);
            if (it == timestamp2adjust[segment.input].end()) {
                error_string_ = "Failed to find timestamp for i#6107 workaround";
                return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
            }
            assert(it->second >= segment.start_instruction);
            assert(it->second % DEFAULT_CHUNK_SIZE == segment.start_instruction);
            if (it->second != segment.start_instruction) {
                VPRINT(this, 2,
                       "Updating all_sched[%d][%d] input %d from %" PRId64 " to %" PRId64
                       "\n",
                       output_idx, sched_idx, segment.input, segment.start_instruction,
                       it->second);
            }
            segment.start_instruction = it->second;
        }
    }
    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::read_switch_sequences()
{
    return read_kernel_sequences(switch_sequence_, options_.kernel_switch_trace_path,
                                 std::move(options_.kernel_switch_reader),
                                 std::move(options_.kernel_switch_reader_end),
                                 TRACE_MARKER_TYPE_CONTEXT_SWITCH_START,
                                 TRACE_MARKER_TYPE_CONTEXT_SWITCH_END, "context switch");
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::read_syscall_sequences()
{

    return read_kernel_sequences(syscall_sequence_, options_.kernel_syscall_trace_path,
                                 std::move(options_.kernel_syscall_reader),
                                 std::move(options_.kernel_syscall_reader_end),
                                 TRACE_MARKER_TYPE_SYSCALL_TRACE_START,
                                 TRACE_MARKER_TYPE_SYSCALL_TRACE_END, "system call");
}

template <typename RecordType, typename ReaderType>
template <typename SequenceKey>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::read_kernel_sequences(
    std::unordered_map<SequenceKey, std::vector<RecordType>, custom_hash_t<SequenceKey>>
        &sequence,
    std::string trace_path, std::unique_ptr<ReaderType> reader,
    std::unique_ptr<ReaderType> reader_end, trace_marker_type_t start_marker,
    trace_marker_type_t end_marker, std::string sequence_type)
{
    if (!trace_path.empty()) {
        reader = get_reader(trace_path, verbosity_);
        if (!reader || !reader->init()) {
            error_string_ += "Failed to open file for kernel " + sequence_type +
                " sequences: " + trace_path;
            return sched_type_t::STATUS_ERROR_FILE_OPEN_FAILED;
        }
        reader_end = get_default_reader();
    } else if (!reader) {
        // No kernel data provided.
        return sched_type_t::STATUS_SUCCESS;
    } else {
        if (!reader_end) {
            error_string_ += "Provided kernel " + sequence_type + " reader but no end";
            return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
        }
        // We own calling init() as it can block.
        if (!reader->init()) {
            error_string_ += "Failed to init kernel " + sequence_type + " reader";
            return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
        }
    }
    // We assume these sequences are small and we can easily read them all into
    // memory and don't need to stream them on every use.
    // We read a single stream, even if underneath these are split into subfiles
    // in an archive.
    SequenceKey sequence_key = invalid_kernel_sequence_key<SequenceKey>();
    const SequenceKey INVALID_SEQ_KEY = invalid_kernel_sequence_key<SequenceKey>();
    bool in_sequence = false;
    while (*reader != *reader_end) {
        RecordType record = **reader;
        // Only remember the records between the markers.
        trace_marker_type_t marker_type = TRACE_MARKER_TYPE_RESERVED_END;
        uintptr_t marker_value = 0;
        bool is_marker = record_type_is_marker(record, marker_type, marker_value);
        if (is_marker && marker_type == start_marker) {
            if (in_sequence) {
                error_string_ += "Found another " + sequence_type +
                    " sequence start without prior ending";
                return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
            }
            sequence_key = static_cast<SequenceKey>(marker_value);
            in_sequence = true;
            if (sequence_key == INVALID_SEQ_KEY) {
                error_string_ +=
                    "Invalid " + sequence_type + " sequence found with default key";
                return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
            }
            if (!sequence[sequence_key].empty()) {
                error_string_ += "Duplicate " + sequence_type + " sequence found";
                return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
            }
        }
        if (in_sequence)
            sequence[sequence_key].push_back(record);
        if (is_marker && marker_type == end_marker) {
            if (!in_sequence) {
                error_string_ += "Found " + sequence_type +
                    " sequence end marker without start marker";
                return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
            }
            if (static_cast<SequenceKey>(marker_value) != sequence_key) {
                error_string_ += sequence_type + " marker values mismatched";
                return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
            }
            if (sequence[sequence_key].empty()) {
                error_string_ += sequence_type + " sequence empty";
                return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
            }
            VPRINT(this, 1, "Read %zu kernel %s records for key %d\n",
                   sequence[sequence_key].size(), sequence_type.c_str(), sequence_key);
            sequence_key = INVALID_SEQ_KEY;
            in_sequence = false;
        }
        ++(*reader);
    }
    assert(!in_sequence);
    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
bool
scheduler_impl_tmpl_t<RecordType, ReaderType>::process_next_initial_record(
    input_info_t &input, RecordType record, bool &found_filetype, bool &found_timestamp)
{
    // We want to identify threads that should start out unscheduled as
    // we attached in the middle of an _UNSCHEDULE system call.
    // That marker *before* any instruction indicates the initial
    // exit from such a syscall (the markers anywhere else are added on
    // entry to a syscall, after the syscall instruction fetch record).
    trace_marker_type_t marker_type;
    uintptr_t marker_value;
    if (record_type_is_invalid(record)) // Sentinel on first call.
        return true;                    // Keep reading.
    if (input.pid == INVALID_PID)
        record_type_has_pid(record, input.pid);
    // Though the tid must have been already set by other logic (the readahead in
    // open_reader, or the construction arg to input_workload_t), we still
    // check and set it for consistent treatment with pid.
    if (input.tid == INVALID_THREAD_ID)
        record_type_has_tid(record, input.tid);
    if (record_type_is_non_marker_header(record))
        return true; // Keep reading.
    if (!record_type_is_marker(record, marker_type, marker_value)) {
        VPRINT(this, 3, "Stopping initial readahead at non-marker\n");
        return false; // Stop reading.
    }
    uintptr_t timestamp;
    if (marker_type == TRACE_MARKER_TYPE_FILETYPE) {
        found_filetype = true;
        VPRINT(this, 2, "Input %d filetype %zu\n", input.index, marker_value);
    } else if (record_type_is_timestamp(record, timestamp)) {
        if (!found_timestamp) {
            // next_timestamp must be the first timestamp, even when we read ahead.
            input.next_timestamp = timestamp;
            found_timestamp = true;
        } else {
            // Stop at a 2nd timestamp to avoid interval count issues.
            VPRINT(this, 3, "Stopping initial readahead at 2nd timestamp\n");
            return false;
        }
    } else if (marker_type == TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE) {
        if (options_.honor_direct_switches &&
            options_.mapping != sched_type_t::MAP_AS_PREVIOUSLY) {
            VPRINT(this, 2, "Input %d starting unscheduled\n", input.index);
            input.unscheduled = true;
            if (!options_.honor_infinite_timeouts) {
                input.blocked_time = scale_blocked_time(options_.block_time_max_us);
                // Clamp at 1 since 0 means an infinite timeout for unscheduled=true.
                if (input.blocked_time == 0)
                    input.blocked_time = 1;
                // blocked_start_time will be set when we first pop this off a queue.
            }
            // Ignore this marker during regular processing.
            input.skip_next_unscheduled = true;
        }
        return false; // Stop reading.
    }
    return true; // Keep reading.
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::get_initial_input_content(
    bool gather_timestamps)
{
    // For every mode, read ahead until we see a filetype record so the user can
    // examine it prior to retrieving any records.
    VPRINT(this, 1, "Reading headers from inputs to find filetypes%s\n",
           gather_timestamps ? " and timestamps" : "");
    assert(options_.read_inputs_in_init);
    // Read ahead in each input until we find a timestamp record.
    // Queue up any skipped records to ensure we present them to the
    // output stream(s).
    for (size_t i = 0; i < inputs_.size(); ++i) {
        input_info_t &input = inputs_[i];
        std::lock_guard<mutex_dbg_owned> lock(*input.lock);

        // If the input jumps to the middle immediately, do that now so we'll have
        // the proper start timestamp.
        if (!input.regions_of_interest.empty() &&
            // The docs say for replay we allow the user to pass ROI but ignore it.
            // Maybe we should disallow it so we don't need checks like this?
            options_.mapping != sched_type_t::MAP_AS_PREVIOUSLY) {
            RecordType record = create_invalid_record();
            stream_status_t res =
                advance_region_of_interest(/*output=*/-1, record, input);
            if (res == sched_type_t::STATUS_SKIPPED) {
                input.next_timestamp =
                    static_cast<uintptr_t>(input.reader->get_last_timestamp());
                // We can skip the rest of the loop here (the filetype will be there
                // in the stream).
                continue;
            }
            if (res != sched_type_t::STATUS_OK) {
                VPRINT(this, 1, "Failed to advance initial ROI with status %d\n", res);
                return sched_type_t::STATUS_ERROR_RANGE_INVALID;
            }
        }

        bool found_filetype = false;
        bool found_timestamp = !gather_timestamps || input.next_timestamp > 0;
        if (process_next_initial_record(input, create_invalid_record(), found_filetype,
                                        found_timestamp)) {
            // First, check any queued records in the input.
            // XXX: Can we create a helper to iterate the queue and then the
            // reader, and avoid the duplicated loops here?  The challenge is
            // the non-consuming queue loop vs the consuming and queue-pushback
            // reader loop.
            for (const auto &record : input.queue) {
                if (!process_next_initial_record(input, record, found_filetype,
                                                 found_timestamp))
                    break;
            }
        }
        if (input.next_timestamp > 0)
            found_timestamp = true;
        if (process_next_initial_record(input, create_invalid_record(), found_filetype,
                                        found_timestamp)) {
            // If we didn't find our targets in the queue, request new records.
            if (input.needs_init) {
                input.reader->init();
                input.needs_init = false;
            }
            while (*input.reader != *input.reader_end) {
                RecordType record = **input.reader;
                if (record_type_is_instr(record)) {
                    ++input.instrs_pre_read;
                }
                trace_marker_type_t marker_type;
                uintptr_t marker_value;
                if (!process_next_initial_record(input, record, found_filetype,
                                                 found_timestamp))
                    break;
                // Don't go too far if only looking for filetype, to avoid reaching
                // the first instruction, which causes problems with ordinals when
                // there is no filetype as happens in legacy traces (and unit tests).
                // Just exit with a 0 filetype.
                if (!found_filetype &&
                    (record_type_is_timestamp(record, marker_value) ||
                     (record_type_is_marker(record, marker_type, marker_value) &&
                      marker_type == TRACE_MARKER_TYPE_PAGE_SIZE))) {
                    VPRINT(this, 2, "No filetype found: assuming unit test input.\n");
                    found_filetype = true;
                    if (!gather_timestamps)
                        break;
                }
                // If we see an instruction, there may be no timestamp (a malformed
                // synthetic trace in a test) or we may have to read thousands of records
                // to find it if it were somehow missing, which we do not want to do.  We
                // assume our queued records are few and do not include instructions when
                // we skip (see skip_instructions()).  Thus, we abort with an error.
                if (record_type_is_instr(record))
                    break;
                input.queue.push_back(record);
                ++(*input.reader);
            }
        }
        if (gather_timestamps && input.next_timestamp <= 0)
            return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
    }
    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::inject_pending_syscall_sequence(
    output_ordinal_t output, input_info_t *input, RecordType &record)
{
    assert(!input->in_syscall_injection);
    assert(input->to_inject_syscall != input_info_t::INJECT_NONE);
    if (!record_type_is_invalid(record)) {
        // May be invalid if we're at input eof, in which case we do not need to
        // save it.
        input->queue.push_front(record);
    }
    int syscall_num = input->to_inject_syscall;
    input->to_inject_syscall = input_info_t::INJECT_NONE;
    assert(syscall_sequence_.find(syscall_num) != syscall_sequence_.end());
    stream_status_t res = inject_kernel_sequence(syscall_sequence_[syscall_num], input);
    if (res != stream_status_t::STATUS_OK)
        return res;
    ++outputs_[output]
          .stats[memtrace_stream_t::SCHED_STAT_KERNEL_SYSCALL_SEQUENCE_INJECTIONS];
    VPRINT(this, 3, "Inserted %zu syscall records for syscall %d to %d.%d\n",
           syscall_sequence_[syscall_num].size(), syscall_num, input->workload,
           input->index);

    // Return the first injected record.
    assert(!input->queue.empty());
    record = input->queue.front();
    input->queue.pop_front();
    input->cur_from_queue = true;
    input->in_syscall_injection = true;
    return stream_status_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::maybe_inject_pending_syscall_sequence(
    output_ordinal_t output, input_info_t *input, RecordType &record)
{
    if (input->to_inject_syscall == input_info_t::INJECT_NONE)
        return stream_status_t::STATUS_OK;

    trace_marker_type_t marker_type;
    uintptr_t marker_value_unused;
    uintptr_t timestamp_unused;
    bool is_marker = record_type_is_marker(record, marker_type, marker_value_unused);
    bool is_injection_point = false;
    if (
        // For syscalls not specified in -record_syscall, which do not have
        // the func_id-func_retval markers.
        record_type_is_timestamp(record, timestamp_unused) ||
        // For syscalls that did not have a post-event because the trace ended.
        record_type_is_thread_exit(record) ||
        // For sigreturn, we want to inject before the kernel_xfer marker which
        // is after the syscall func_arg markers (if any) but before the
        // post-syscall timestamp marker.
        (is_marker && marker_type == TRACE_MARKER_TYPE_KERNEL_XFER) ||
        // For syscalls interrupted by a signal and did not have a post-syscall
        // event.
        (is_marker && marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT)) {
        is_injection_point = true;
    } else if (is_marker && marker_type == TRACE_MARKER_TYPE_FUNC_ID) {
        if (!input->saw_first_func_id_marker_after_syscall) {
            // XXX i#7482: If we allow recording zero args for syscalls in
            // -record_syscall, we would need to update this logic.
            input->saw_first_func_id_marker_after_syscall = true;
        } else {
            // For syscalls specified in -record_syscall, for which we inject
            // after the func_id-func_arg markers (if any) but before the
            // func_id-func_retval markers.
            is_injection_point = true;
        }
    }
    if (is_injection_point) {
        stream_status_t res = inject_pending_syscall_sequence(output, input, record);
        if (res != stream_status_t::STATUS_OK)
            return res;
        input->saw_first_func_id_marker_after_syscall = false;
    }
    return stream_status_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::inject_kernel_sequence(
    std::vector<RecordType> &sequence, input_info_t *input)
{
    // Inject kernel template code.  Since the injected records belong to this
    // input (the kernel is acting on behalf of this input) we insert them into the
    // input's queue, but ahead of any prior queued items.  This is why we walk in
    // reverse, for the push_front calls to the deque.  We update the tid of the
    // records here to match.  They are considered as is_record_synthetic() and do
    // not affect input stream ordinals.
    // XXX: These will appear before the top headers of a new thread which is slightly
    // odd to have regular records with the new tid before the top headers.
    assert(!sequence.empty());
    bool saw_any_instr = false;
    bool set_branch_target_marker = false;
    trace_marker_type_t marker_type;
    uintptr_t marker_value = 0;
    for (int i = static_cast<int>(sequence.size()) - 1; i >= 0; --i) {
        RecordType record = sequence[i];
        // TODO i#7495: Add invariant checks that ensure these are equal to the
        // context-switched-to thread when the switch sequence is injected into a
        // trace.
        record_type_set_tid(record, input->tid);
        record_type_set_pid(record, input->pid);
        if (record_type_is_instr(record)) {
            set_branch_target_marker = false;
            if (!saw_any_instr) {
                saw_any_instr = true;
                bool has_indirect_branch_target = false;
                // If the last to-be-injected instruction is an indirect branch, set its
                // indirect_branch_target field to the fallthrough pc of the last
                // returned instruction from this input (for syscall injection, it would
                // be the syscall for which we're injecting the trace). This is simpler
                // than trying to get the actual next instruction on this input for which
                // we would need to read-ahead.
                // TODO i#7496: The above strategy does not work for syscalls that
                // transfer control (like sigreturn) or for syscalls auto-restarted by a
                // signal.
                if (record_type_is_indirect_branch_instr(
                        record, has_indirect_branch_target, input->last_pc_fallthrough) &&
                    !has_indirect_branch_target) {
                    // trace_entry_t instr records do not hold the indirect branch target;
                    // instead a separate marker prior to the indirect branch instr holds
                    // it, which must be set separately.
                    set_branch_target_marker = true;
                }
            }
        } else if (set_branch_target_marker &&
                   record_type_is_marker(record, marker_type, marker_value) &&
                   marker_type == TRACE_MARKER_TYPE_BRANCH_TARGET) {
            record_type_set_marker_value(record, input->last_pc_fallthrough);
            set_branch_target_marker = false;
        }
        input->queue.push_front(record);
    }
    return stream_status_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::open_reader(
    const std::string &path, input_ordinal_t input_ordinal,
    input_reader_info_t &reader_info)
{
    if (path.empty() || directory_iterator_t::is_directory(path))
        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
    std::unique_ptr<ReaderType> reader = get_reader(path, verbosity_);
    if (!reader || !reader->init()) {
        // Include a suggestion to check the open file limit.
        // We could call getrlimit to see if it's a likely culprit; we could
        // try to call setrlimit ourselves but that doesn't feel right.
        error_string_ += "Failed to open " + path + " (was RLIMIT_NOFILE exceeded?)";
        return sched_type_t::STATUS_ERROR_FILE_OPEN_FAILED;
    }
    int index = static_cast<input_ordinal_t>(inputs_.size());
    inputs_.emplace_back();
    input_info_t &input = inputs_.back();
    input.index = index;
    // We need the tid up front.  Rather than assume it's still part of the filename,
    // we read the first record (we generalize to read until we find the first but we
    // expect it to be the first after PR #5739 changed the order file_reader_t passes
    // them to reader_t) to find it.
    // XXX: For core-sharded-on-disk traces, this tid is just the first one for
    // this core; it would be better to read the filetype and not match any tid
    // for such files?  Should we call get_initial_input_content() to do that?
    std::unique_ptr<ReaderType> reader_end = get_default_reader();
    memref_tid_t tid = INVALID_THREAD_ID;
    while (*reader != *reader_end) {
        RecordType record = **reader;
        if (record_type_has_tid(record, tid))
            break;
        input.queue.push_back(record);
        ++(*reader);
    }
    if (tid == INVALID_THREAD_ID) {
        error_string_ = "Failed to read " + path;
        return sched_type_t::STATUS_ERROR_FILE_READ_FAILED;
    }
    // For core-sharded inputs that start idle the tid might be IDLE_THREAD_ID.
    // That means the size of unfiltered_tids will not be the total input
    // size, which is why we have a separate input_count.
    reader_info.unfiltered_tids.insert(tid);
    ++reader_info.input_count;
    if (!reader_info.only_threads.empty() &&
        reader_info.only_threads.find(tid) == reader_info.only_threads.end()) {
        inputs_.pop_back();
        return sched_type_t::STATUS_SUCCESS;
    }
    if (!reader_info.only_shards.empty() &&
        reader_info.only_shards.find(input_ordinal) == reader_info.only_shards.end()) {
        inputs_.pop_back();
        return sched_type_t::STATUS_SUCCESS;
    }
    VPRINT(this, 1, "Opened reader for tid %" PRId64 " %s\n", tid, path.c_str());
    input.tid = tid;
    input.reader = std::move(reader);
    input.reader_end = std::move(reader_end);
    reader_info.tid2input[tid] = index;
    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::open_readers(
    const std::string &path, input_reader_info_t &reader_info)
{
    if (!directory_iterator_t::is_directory(path)) {
        return open_reader(path, 0, reader_info);
    }
    directory_iterator_t end;
    directory_iterator_t iter(path);
    if (!iter) {
        error_string_ = "Failed to list directory " + path + ": " + iter.error_string();
        return sched_type_t::STATUS_ERROR_FILE_OPEN_FAILED;
    }
    std::vector<std::string> files;
    for (; iter != end; ++iter) {
        const std::string fname = *iter;
        if (fname == "." || fname == ".." ||
            starts_with(fname, DRMEMTRACE_SERIAL_SCHEDULE_FILENAME) ||
            fname == DRMEMTRACE_CPU_SCHEDULE_FILENAME)
            continue;
        // Skip the auxiliary files.
        if (fname == DRMEMTRACE_MODULE_LIST_FILENAME ||
            fname == DRMEMTRACE_FUNCTION_LIST_FILENAME ||
            fname == DRMEMTRACE_ENCODING_FILENAME || fname == DRMEMTRACE_V2P_FILENAME)
            continue;
        const std::string file = path + DIRSEP + fname;
        files.push_back(file);
    }
    // Sort so we can have reliable shard ordinals for only_shards.
    // We assume leading 0's are used for important numbers embedded in the path,
    // so that a regular sort keeps numeric order.
    std::sort(files.begin(), files.end());
    for (int i = 0; i < static_cast<int>(files.size()); ++i) {
        scheduler_status_t res = open_reader(files[i], i, reader_info);
        if (res != sched_type_t::STATUS_SUCCESS)
            return res;
    }
    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
std::string
scheduler_impl_tmpl_t<RecordType, ReaderType>::get_input_name(output_ordinal_t output)
{
    int index = outputs_[output].cur_input;
    if (index < 0)
        return "";
    return inputs_[index].reader->get_stream_name();
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::input_ordinal_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::get_input_ordinal(output_ordinal_t output)
{
    return outputs_[output].cur_input;
}

template <typename RecordType, typename ReaderType>
int64_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::get_tid(output_ordinal_t output)
{
    int index = outputs_[output].cur_input;
    if (index < 0)
        return -1;
    if (inputs_[index].is_combined_stream() ||
        TESTANY(OFFLINE_FILE_TYPE_CORE_SHARDED, inputs_[index].reader->get_filetype()))
        return inputs_[index].last_record_tid;
    return inputs_[index].tid;
}

template <typename RecordType, typename ReaderType>
int
scheduler_impl_tmpl_t<RecordType, ReaderType>::get_shard_index(output_ordinal_t output)
{
    if (output < 0 || output >= static_cast<output_ordinal_t>(outputs_.size()))
        return -1;
    if (TESTANY(sched_type_t::SCHEDULER_USE_INPUT_ORDINALS |
                    sched_type_t::SCHEDULER_USE_SINGLE_INPUT_ORDINALS,
                options_.flags)) {
        if (inputs_.size() == 1 && inputs_[0].is_combined_stream()) {
            int index;
            memref_tid_t tid = get_tid(output);
            auto exists = tid2shard_.find(tid);
            if (exists == tid2shard_.end()) {
                index = static_cast<int>(tid2shard_.size());
                tid2shard_[tid] = index;
            } else
                index = exists->second;
            return index;
        }
        return get_input_ordinal(output);
    }
    return output;
}

template <typename RecordType, typename ReaderType>
int
scheduler_impl_tmpl_t<RecordType, ReaderType>::get_workload_ordinal(
    output_ordinal_t output)
{
    if (output < 0 || output >= static_cast<output_ordinal_t>(outputs_.size()))
        return -1;
    if (outputs_[output].cur_input < 0)
        return -1;
    return inputs_[outputs_[output].cur_input].workload;
}

template <typename RecordType, typename ReaderType>
bool
scheduler_impl_tmpl_t<RecordType, ReaderType>::is_record_synthetic(
    output_ordinal_t output)
{
    int index = outputs_[output].cur_input;
    if (index < 0)
        return false;
    if (outputs_[output].in_context_switch_code || outputs_[output].in_syscall_code)
        return true;
    return inputs_[index].reader->is_record_synthetic();
}

template <typename RecordType, typename ReaderType>
int64_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::get_output_cpuid(
    output_ordinal_t output) const
{
    if (options_.replay_as_traced_istream != nullptr)
        return outputs_[output].as_traced_cpuid;
    int index = outputs_[output].cur_input;
    if (index >= 0 &&
        TESTANY(OFFLINE_FILE_TYPE_CORE_SHARDED, inputs_[index].reader->get_filetype()))
        return outputs_[output].cur_input;
    return output;
}

template <typename RecordType, typename ReaderType>
memtrace_stream_t *
scheduler_impl_tmpl_t<RecordType, ReaderType>::get_input_stream(output_ordinal_t output)
{
    if (output < 0 || output >= static_cast<output_ordinal_t>(outputs_.size()))
        return nullptr;
    int index = outputs_[output].cur_input;
    if (index < 0)
        return nullptr;
    return inputs_[index].reader.get();
}

template <typename RecordType, typename ReaderType>
uint64_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::get_input_record_ordinal(
    output_ordinal_t output)
{
    if (output < 0 || output >= static_cast<output_ordinal_t>(outputs_.size()))
        return 0;
    int index = outputs_[output].cur_input;
    if (index < 0)
        return 0;
    uint64_t ord = inputs_[index].reader->get_record_ordinal();
    if (get_instr_ordinal(inputs_[index]) == 0) {
        // Account for get_initial_input_content() readahead for filetype/timestamp.
        // If this gets any more complex, the scheduler stream should track its
        // own counts for every input and just ignore the input stream's tracking.
        ord -= inputs_[index].queue.size() + (inputs_[index].cur_from_queue ? 1 : 0);
    }
    if (inputs_[index].in_syscall_injection) {
        // We readahead by one record when injecting syscalls.
        --ord;
    }
    return ord;
}

template <typename RecordType, typename ReaderType>
uint64_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::get_instr_ordinal(input_info_t &input)
{
    uint64_t reader_cur = input.reader->get_instruction_ordinal();
    assert(reader_cur >= static_cast<uint64_t>(input.instrs_pre_read));
    VPRINT(this, 5, "get_instr_ordinal: %" PRId64 " - %d\n", reader_cur,
           input.instrs_pre_read);
    return reader_cur - input.instrs_pre_read;
}

template <typename RecordType, typename ReaderType>
uint64_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::get_input_first_timestamp(
    output_ordinal_t output)
{
    if (output < 0 || output >= static_cast<output_ordinal_t>(outputs_.size()))
        return 0;
    int index = outputs_[output].cur_input;
    if (index < 0)
        return 0;
    uint64_t res = inputs_[index].reader->get_first_timestamp();
    if (get_instr_ordinal(inputs_[index]) == 0 &&
        (!inputs_[index].queue.empty() || inputs_[index].cur_from_queue)) {
        // Account for get_initial_input_content() readahead for filetype/timestamp.
        res = 0;
    }
    return res;
}

template <typename RecordType, typename ReaderType>
uint64_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::get_input_last_timestamp(
    output_ordinal_t output)
{
    if (output < 0 || output >= static_cast<output_ordinal_t>(outputs_.size()))
        return 0;
    int index = outputs_[output].cur_input;
    if (index < 0)
        return 0;
    uint64_t res = inputs_[index].reader->get_last_timestamp();
    if (get_instr_ordinal(inputs_[index]) == 0 &&
        (!inputs_[index].queue.empty() || inputs_[index].cur_from_queue)) {
        // Account for get_initial_input_content() readahead for filetype/timestamp.
        res = 0;
    }
    return res;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::advance_region_of_interest(
    output_ordinal_t output, RecordType &record, input_info_t &input)
{
    assert(input.lock->owned_by_cur_thread());
    // XXX i#7230: By using the provided ordinal, this should ignore synthetic records,
    // which we have documented in the option docs.  We should make a unit test
    // confirming and ensuring this matches -skip_records and invariant report ordinals.
    uint64_t cur_instr = get_instr_ordinal(input);
    uint64_t cur_reader_instr = input.reader->get_instruction_ordinal();
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
            if (input.at_eof) {
                // XXX: We're holding input.lock which is ok during eof_or_idle.
                return eof_or_idle(output, input.index);
            } else {
                // We let the user know we're done.
                if (options_.schedule_record_ostream != nullptr) {
                    stream_status_t status = close_schedule_segment(output, input);
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
                stream_status_t status = mark_input_eof(input);
                // For early EOF we still need our synthetic exit so do not return it yet.
                if (status != sched_type_t::STATUS_OK &&
                    status != sched_type_t::STATUS_EOF)
                    return status;
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
    // and making no progress (we're on the inserted timestamp + cpuid and our cur instr
    // count isn't yet the target).
    if (input.in_cur_region && cur_instr >= cur_range.start_instruction - 1)
        return sched_type_t::STATUS_OK;

    VPRINT(this, 2,
           "skipping from %" PRId64 " to %" PRIu64 " instrs (%" PRIu64
           " in reader) for ROI\n",
           cur_instr, cur_range.start_instruction,
           cur_range.start_instruction - cur_reader_instr - 1);
    if (options_.schedule_record_ostream != nullptr) {
        if (output >= 0) {
            record_schedule_skip(output, input.index, cur_instr,
                                 cur_range.start_instruction);
        } // Else, will be done in set_cur_input once assigned to an output.
    }
    if (cur_range.start_instruction < cur_reader_instr) {
        // We do not support skipping without skipping over the pre-read: we would
        // need to extract from the queue.
        return sched_type_t::STATUS_INVALID;
    }
    return skip_instructions(input, cur_range.start_instruction - cur_reader_instr - 1);
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::record_schedule_skip(
    output_ordinal_t output, input_ordinal_t input, uint64_t start_instruction,
    uint64_t stop_instruction)
{
    assert(inputs_[input].lock->owned_by_cur_thread());
    if (options_.schedule_record_ostream == nullptr)
        return sched_type_t::STATUS_INVALID;
    stream_status_t status;
    // Close any prior default record for this input.  If we switched inputs,
    // we'll already have closed the prior in set_cur_input().
    if (outputs_[output].record.back().type == schedule_record_t::DEFAULT &&
        outputs_[output].record.back().key.input == input) {
        status = close_schedule_segment(output, inputs_[input]);
        if (status != sched_type_t::STATUS_OK)
            return status;
    }
    if (outputs_[output].record.size() == 1) {
        // Replay doesn't handle starting out with a skip record: we need a
        // start=0,stop=0 dummy entry to get things rolling at the start of
        // an output's records, if we're the first record after the version.
        assert(outputs_[output].record.back().type == schedule_record_t::VERSION);
        status = record_schedule_segment(output, schedule_record_t::DEFAULT, input, 0, 0);
        if (status != sched_type_t::STATUS_OK)
            return status;
    }
    status = record_schedule_segment(output, schedule_record_t::SKIP, input,
                                     start_instruction, stop_instruction);
    if (status != sched_type_t::STATUS_OK)
        return status;
    status = record_schedule_segment(output, schedule_record_t::DEFAULT, input,
                                     stop_instruction);
    if (status != sched_type_t::STATUS_OK)
        return status;
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
void
scheduler_impl_tmpl_t<RecordType, ReaderType>::clear_input_queue(input_info_t &input)
{
    // We assume the queue contains no instrs other than the single candidate record we
    // ourselves read but did not pass to the user (else our query of input.reader's
    // instr ordinal would include them and so be incorrect) and that we should thus
    // skip it all when skipping ahead in the input stream.
    int i = 0;
    while (!input.queue.empty()) {
        assert(i == 0 ||
               (!record_type_is_instr(input.queue.front()) &&
                !record_type_is_encoding(input.queue.front())));
        ++i;
        input.queue.pop_front();
    }
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::skip_instructions(input_info_t &input,
                                                                 uint64_t skip_amount)
{
    assert(input.lock->owned_by_cur_thread());
    // reader_t::at_eof_ is true until init() is called.
    if (input.needs_init) {
        input.reader->init();
        input.needs_init = false;
    }
    // For a skip of 0 we still need to clear non-instrs from the queue, but
    // should not have an instr in there.
    assert(skip_amount > 0 || input.queue.empty() ||
           (!record_type_is_instr(input.queue.front()) &&
            !record_type_is_encoding(input.queue.front())));
    clear_input_queue(input);
    input.reader->skip_instructions(skip_amount);
    VPRINT(this, 3, "skip_instructions: input=%d amount=%" PRIu64 "\n", input.index,
           skip_amount);
    if (input.instrs_pre_read > 0) {
        // We do not support skipping without skipping over the pre-read: we would
        // need to extract from the queue.
        input.instrs_pre_read = 0;
    }
    if (*input.reader == *input.reader_end) {
        stream_status_t status = mark_input_eof(input);
        if (status != sched_type_t::STATUS_OK)
            return status;
        // Raise error because the input region is out of bounds, unless the max
        // was used which we ourselves use internally for times_of_interest.
        if (skip_amount >= std::numeric_limits<uint64_t>::max() - 2) {
            VPRINT(this, 2, "skip_instructions: input=%d skip to eof\n", input.index);
            return sched_type_t::STATUS_SKIPPED;
        } else {
            VPRINT(this, 2, "skip_instructions: input=%d skip out of bounds\n",
                   input.index);
            return sched_type_t::STATUS_REGION_INVALID;
        }
    }
    input.in_cur_region = true;

    // We've documented that an output stream's ordinals ignore skips in its input
    // streams, so we do not need to remember the input's ordinals pre-skip and increase
    // our output's ordinals commensurately post-skip.

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
uint64_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::get_time_micros()
{
    return get_microsecond_timestamp();
}

template <typename RecordType, typename ReaderType>
uint64_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::get_output_time(output_ordinal_t output)
{
    return outputs_[output].cur_time->load(std::memory_order_acquire);
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::record_schedule_segment(
    output_ordinal_t output, typename schedule_record_t::record_type_t type,
    input_ordinal_t input, uint64_t start_instruction, uint64_t stop_instruction)
{
    assert(type == schedule_record_t::VERSION || type == schedule_record_t::FOOTER ||
           // ::IDLE is a legacy type we should not see in new recordings.
           type == schedule_record_t::IDLE_BY_COUNT ||
           inputs_[input].lock->owned_by_cur_thread());
    // We always use the current wall-clock time, as the time stored in the prior
    // next_record() call can be out of order across outputs and lead to deadlocks.
    uint64_t timestamp = get_time_micros();
    if (type == schedule_record_t::IDLE_BY_COUNT &&
        outputs_[output].record.back().type == schedule_record_t::IDLE_BY_COUNT) {
        // Merge.  We don't need intermediate timestamps when idle, and consecutive
        // idle records quickly balloon the file.
        return sched_type_t::STATUS_OK;
    }
    if (type == schedule_record_t::IDLE_BY_COUNT) {
        // Start prior to this idle.
        outputs_[output].idle_start_count = outputs_[output].idle_count - 1;
        // That is what we'll record in the value union shared w/ start_instruction.
        assert(start_instruction == outputs_[output].idle_count - 1);
    }
    VPRINT(this, 3,
           "recording out=%d type=%d input=%d start=%" PRIu64 " stop=%" PRIu64
           " time=%" PRIu64 "\n",
           output, type, input, start_instruction, stop_instruction, timestamp);
    outputs_[output].record.emplace_back(type, input, start_instruction, stop_instruction,
                                         timestamp);
    // The stop is typically updated later in close_schedule_segment().
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::close_schedule_segment(
    output_ordinal_t output, input_info_t &input)
{
    assert(output >= 0 && output < static_cast<output_ordinal_t>(outputs_.size()));
    assert(!outputs_[output].record.empty());
    assert(outputs_[output].record.back().type == schedule_record_t::VERSION ||
           outputs_[output].record.back().type == schedule_record_t::FOOTER ||
           // ::IDLE is for legacy recordings, not new ones.
           outputs_[output].record.back().type == schedule_record_t::IDLE_BY_COUNT ||
           input.lock->owned_by_cur_thread());
    if (outputs_[output].record.back().type == schedule_record_t::SKIP) {
        // Skips already have a final stop value.
        return sched_type_t::STATUS_OK;
    }
    if (outputs_[output].record.back().type == schedule_record_t::IDLE_BY_COUNT) {
        uint64_t end_idle_count = outputs_[output].idle_count;
        assert(outputs_[output].idle_start_count >= 0);
        assert(end_idle_count >=
               static_cast<uint64_t>(outputs_[output].idle_start_count));
        outputs_[output].record.back().value.idle_duration =
            end_idle_count - outputs_[output].idle_start_count;
        VPRINT(this, 3,
               "close_schedule_segment[%d]: idle duration %" PRIu64 " = %" PRIu64
               " - %" PRIu64 "\n",
               output, outputs_[output].record.back().value.idle_duration, end_idle_count,
               outputs_[output].idle_start_count);
        return sched_type_t::STATUS_OK;
    }
    uint64_t instr_ord = get_instr_ordinal(input);
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
           "close_schedule_segment[%d]: input=%d type=%d start=%" PRIu64 " stop=%" PRIu64
           "\n",
           output, input.index, outputs_[output].record.back().type,
           outputs_[output].record.back().value.start_instruction, instr_ord);
    // Check for empty default entries, except the starter 0,0 ones.
    assert(outputs_[output].record.back().type != schedule_record_t::DEFAULT ||
           outputs_[output].record.back().value.start_instruction < instr_ord ||
           instr_ord == 0);
    outputs_[output].record.back().stop_instruction = instr_ord;
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
uint64_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::scale_blocked_time(
    uint64_t initial_time) const
{
    uint64_t scaled_us = static_cast<uint64_t>(static_cast<double>(initial_time) *
                                               options_.block_time_multiplier);
    if (scaled_us > options_.block_time_max_us) {
        // We have a max to avoid outlier latencies that are already a second or
        // more from scaling up to tens of minutes.  We assume a cap is representative
        // as the outliers likely were not part of key dependence chains.  Without a
        // cap the other threads all finish and the simulation waits for tens of
        // minutes further for a couple of outliers.
        scaled_us = options_.block_time_max_us;
    }
    return static_cast<uint64_t>(scaled_us * options_.time_units_per_us);
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::set_cur_input(
    output_ordinal_t output, input_ordinal_t input, bool caller_holds_cur_input_lock)
{
    // XXX i#5843: Merge tracking of current inputs with ready_queue.queue to better
    // manage the possible 3 states of each input (a live cur_input for an output stream,
    // in the ready_queue_, or at EOF) (4 states once we add i/o wait times).
    assert(output >= 0 && output < static_cast<output_ordinal_t>(outputs_.size()));
    // 'input' might be sched_type_t::INVALID_INPUT_ORDINAL.
    assert(input < static_cast<input_ordinal_t>(inputs_.size()));
    // The caller should never hold the input lock for MAP_TO_ANY_OUTPUT.
    assert(options_.mapping != sched_type_t::MAP_TO_ANY_OUTPUT ||
           !caller_holds_cur_input_lock);
    int prev_input = outputs_[output].cur_input;
    if (prev_input >= 0) {
        if (prev_input != input) {
            input_info_t &prev_info = inputs_[prev_input];
            {
                auto scoped_lock = caller_holds_cur_input_lock
                    ? std::unique_lock<mutex_dbg_owned>()
                    : std::unique_lock<mutex_dbg_owned>(*prev_info.lock);
                prev_info.cur_output = sched_type_t::INVALID_OUTPUT_ORDINAL;
                prev_info.last_run_time = get_output_time(output);
                if (options_.schedule_record_ostream != nullptr) {
                    stream_status_t status = close_schedule_segment(output, prev_info);
                    if (status != sched_type_t::STATUS_OK)
                        return status;
                }
            }
        }
    } else if (options_.schedule_record_ostream != nullptr &&
               (outputs_[output].record.back().type == schedule_record_t::IDLE ||
                outputs_[output].record.back().type ==
                    schedule_record_t::IDLE_BY_COUNT)) {
        input_info_t unused;
        stream_status_t status = close_schedule_segment(output, unused);
        if (status != sched_type_t::STATUS_OK)
            return status;
    }
    if (prev_input != input) {
        // Let subclasses act on the outgoing input.
        stream_status_t res =
            swap_out_input(output, prev_input, caller_holds_cur_input_lock);
        if (res != sched_type_t::STATUS_OK)
            return res;
    }
    if (outputs_[output].cur_input >= 0)
        outputs_[output].prev_input = outputs_[output].cur_input;
    outputs_[output].cur_input = input;
    if (prev_input == input)
        return sched_type_t::STATUS_OK;
    if (input < 0) {
        // Let subclasses act on the switch to idle.
        return swap_in_input(output, input);
    }

    int prev_workload = -1;
    if (outputs_[output].prev_input >= 0 && outputs_[output].prev_input != input) {
        // If the caller already holds the lock, do not re-acquire as that will hang.
        auto scoped_lock =
            (caller_holds_cur_input_lock && prev_input == outputs_[output].prev_input)
            ? std::unique_lock<mutex_dbg_owned>()
            : std::unique_lock<mutex_dbg_owned>(
                  *inputs_[outputs_[output].prev_input].lock);
        prev_workload = inputs_[outputs_[output].prev_input].workload;
    }

    std::lock_guard<mutex_dbg_owned> lock(*inputs_[input].lock);

    inputs_[input].cur_output = output;
    inputs_[input].containing_output = output;

    if (prev_input < 0 && outputs_[output].stream->version_ == 0) {
        // Set the version and filetype up front, to let the user query at init time
        // as documented.  Also set the other fields in case we did a skip for ROI.
        auto *stream = outputs_[output].stream;
        stream->version_ = inputs_[input].reader->get_version();
        stream->last_timestamp_ = inputs_[input].reader->get_last_timestamp();
        stream->first_timestamp_ = inputs_[input].reader->get_first_timestamp();
        stream->filetype_ = adjust_filetype(
            static_cast<offline_file_type_t>(inputs_[input].reader->get_filetype()));
        stream->cache_line_size_ = inputs_[input].reader->get_cache_line_size();
        stream->chunk_instr_count_ = inputs_[input].reader->get_chunk_instr_count();
        stream->page_size_ = inputs_[input].reader->get_page_size();
    }

    inputs_[input].prev_time_in_quantum =
        outputs_[output].cur_time->load(std::memory_order_acquire);

    if (options_.schedule_record_ostream != nullptr) {
        uint64_t instr_ord = get_instr_ordinal(inputs_[input]);
        VPRINT(this, 3, "set_cur_input: recording input=%d start=%" PRId64 "\n", input,
               instr_ord);
        if (!inputs_[input].regions_of_interest.empty() &&
            inputs_[input].cur_region == 0 && inputs_[input].in_cur_region &&
            (instr_ord == inputs_[input].regions_of_interest[0].start_instruction ||
             // The ord may be 1 less because we're still on the inserted timestamp.
             instr_ord + 1 == inputs_[input].regions_of_interest[0].start_instruction)) {
            // We skipped during init but didn't have an output for recording the skip:
            // record it now.
            record_schedule_skip(output, input, 0,
                                 inputs_[input].regions_of_interest[0].start_instruction);
        } else {
            stream_status_t status = record_schedule_segment(
                output, schedule_record_t::DEFAULT, input, instr_ord);
            if (status != sched_type_t::STATUS_OK)
                return status;
        }
    }

    // Let subclasses act on the incoming input.
    stream_status_t res = swap_in_input(output, input);
    if (res != sched_type_t::STATUS_OK)
        return res;

    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
bool
scheduler_impl_tmpl_t<RecordType, ReaderType>::need_output_lock()
{
    return options_.mapping == sched_type_t::MAP_TO_ANY_OUTPUT ||
        options_.mapping == sched_type_t::MAP_AS_PREVIOUSLY;
}

template <typename RecordType, typename ReaderType>
std::unique_lock<mutex_dbg_owned>
scheduler_impl_tmpl_t<RecordType, ReaderType>::acquire_scoped_output_lock_if_necessary(
    output_ordinal_t output)
{
    auto scoped_lock = need_output_lock()
        ? std::unique_lock<mutex_dbg_owned>(*outputs_[output].ready_queue.lock)
        : std::unique_lock<mutex_dbg_owned>();
    return scoped_lock;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::pick_next_input(output_ordinal_t output,
                                                               uint64_t blocked_time)
{
    stream_status_t res = sched_type_t::STATUS_OK;
    const input_ordinal_t prev_index = outputs_[output].cur_input;
    input_ordinal_t index = sched_type_t::INVALID_INPUT_ORDINAL;
    int iters = 0;
    while (true) {
        ++iters;
        if (index < 0) {
            res = pick_next_input_for_mode(output, blocked_time, prev_index, index);
            if (res == sched_type_t::STATUS_SKIPPED)
                break;
            if (res != sched_type_t::STATUS_OK)
                return res;
            // reader_t::at_eof_ is true until init() is called.
            std::lock_guard<mutex_dbg_owned> lock(*inputs_[index].lock);
            if (inputs_[index].needs_init) {
                inputs_[index].reader->init();
                inputs_[index].needs_init = false;
            }
        }
        std::lock_guard<mutex_dbg_owned> lock(*inputs_[index].lock);
        if (inputs_[index].at_eof ||
            *inputs_[index].reader == *inputs_[index].reader_end) {
            VPRINT(this, 2, "next_record[%d]: input #%d at eof\n", output, index);
            if (!inputs_[index].at_eof) {
                stream_status_t status = mark_input_eof(inputs_[index]);
                if (status != sched_type_t::STATUS_OK)
                    return status;
            }
            index = sched_type_t::INVALID_INPUT_ORDINAL;
            // Loop and pick next thread.
            continue;
        }
        break;
    }
    // We can't easily place these stats inside set_cur_input() as we call that to
    // temporarily give up our input.
    stream_status_t on_switch_res = on_context_switch(output, prev_index, index);
    if (on_switch_res != stream_status_t::STATUS_OK) {
        return on_switch_res;
    }
    set_cur_input(output, index);
    return res;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::on_context_switch(
    output_ordinal_t output, input_ordinal_t prev_input, input_ordinal_t new_input)
{
    if (prev_input == new_input) {
        ++outputs_[output].stats[memtrace_stream_t::SCHED_STAT_SWITCH_NOP];
        return stream_status_t::STATUS_OK;
    } else if (prev_input != sched_type_t::INVALID_INPUT_ORDINAL &&
               new_input != sched_type_t::INVALID_INPUT_ORDINAL) {
        ++outputs_[output].stats[memtrace_stream_t::SCHED_STAT_SWITCH_INPUT_TO_INPUT];
    } else if (new_input == sched_type_t::INVALID_INPUT_ORDINAL) {
        // XXX: For now, we do not inject a kernel context switch sequence on
        // input-to-idle transitions (note that we do so on idle-to-input though).
        // However, we may want to inject some other suitable sequence, but we're not
        // sure yet.
        ++outputs_[output].stats[memtrace_stream_t::SCHED_STAT_SWITCH_INPUT_TO_IDLE];
        return stream_status_t::STATUS_OK;
    } else {
        ++outputs_[output].stats[memtrace_stream_t::SCHED_STAT_SWITCH_IDLE_TO_INPUT];
        // Reset the flag so we'll try to steal if we go idle again.
        outputs_[output].tried_to_steal_on_idle = false;
    }

    // We want to insert the context switch records (which includes the new input's
    // tid and pid, and possibly the context switch sequence) on input-to-input and
    // idle-to-input cases. This is a better control point to do that than
    // set_cur_input. Here we get the stolen input events too, and we don't have
    // to filter out the init-time set_cur_input cases.

    bool injected_switch_trace = false;
    if (!switch_sequence_.empty()) {
        switch_type_t switch_type = sched_type_t::SWITCH_INVALID;
        if ( // XXX: idle-to-input transitions are assumed to be process switches
             // for now. But we may want to improve this heuristic.
            prev_input == sched_type_t::INVALID_INPUT_ORDINAL ||
            inputs_[prev_input].workload != inputs_[new_input].workload)
            switch_type = sched_type_t::SWITCH_PROCESS;
        else
            switch_type = sched_type_t::SWITCH_THREAD;
        if (switch_sequence_.find(switch_type) != switch_sequence_.end()) {
            stream_status_t res = inject_kernel_sequence(switch_sequence_[switch_type],
                                                         &inputs_[new_input]);
            if (res == stream_status_t::STATUS_OK) {
                injected_switch_trace = true;
                ++outputs_[output].stats
                      [memtrace_stream_t::SCHED_STAT_KERNEL_SWITCH_SEQUENCE_INJECTIONS];
                VPRINT(this, 3,
                       "Inserted %zu switch records for type %d from %d.%d to %d.%d\n",
                       switch_sequence_[switch_type].size(), switch_type,
                       prev_input != sched_type_t::INVALID_INPUT_ORDINAL
                           ? inputs_[prev_input].workload
                           : -1,
                       prev_input, inputs_[new_input].workload, new_input);
            } else if (res != stream_status_t::STATUS_EOF) {
                return res;
            }
        }
    }

    // We do not need synthetic tid-pid records if the original ones from the
    // input are coming up next (which happens when the input is scheduled
    // for the first time), unless we're also injecting a context switch trace,
    // in which case we need the synthetic tid-pid records prior to the injected
    // sequence (note that the tid-pid and switch records are injected LIFO in
    // the queue).
    if (injected_switch_trace ||
        inputs_[new_input].last_record_tid != INVALID_THREAD_ID) {
        insert_switch_tid_pid(inputs_[new_input]);
    }
    return stream_status_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
void
scheduler_impl_tmpl_t<RecordType, ReaderType>::update_syscall_state(
    RecordType record, output_ordinal_t output)
{
    if (outputs_[output].hit_syscall_code_end) {
        // We have to delay so the end marker is still in_syscall_code.
        outputs_[output].in_syscall_code = false;
        outputs_[output].hit_syscall_code_end = false;
    }

    trace_marker_type_t marker_type;
    uintptr_t marker_value = 0;
    if (!record_type_is_marker(record, marker_type, marker_value))
        return;
    switch (marker_type) {
    case TRACE_MARKER_TYPE_SYSCALL_TRACE_START:
        outputs_[output].in_syscall_code = true;
        break;
    case TRACE_MARKER_TYPE_SYSCALL_TRACE_END:
        // We have to delay until the next record.
        outputs_[output].hit_syscall_code_end = true;
        break;
    default: break;
    }
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::next_record(output_ordinal_t output,
                                                           RecordType &record,
                                                           input_info_t *&input,
                                                           uint64_t cur_time)
{
    record = create_invalid_record();
    // We do not enforce a globally increasing time to avoid the synchronization cost; we
    // do return an error on a time smaller than an input's current start time when we
    // check for quantum end.
    if (cur_time == 0) {
        // We add 1 to avoid an invalid value of 0.
        cur_time = 1 + outputs_[output].stream->get_output_instruction_ordinal() +
            outputs_[output].idle_count;
    }
    if (outputs_[output].initial_cur_time->load(std::memory_order_acquire) == 0) {
        outputs_[output].initial_cur_time->store(cur_time, std::memory_order_release);
    }
    // Invalid values for cur_time are checked below.
    outputs_[output].cur_time->store(cur_time, std::memory_order_release);
    if (!outputs_[output].active->load(std::memory_order_acquire)) {
        ++outputs_[output].idle_count;
        return sched_type_t::STATUS_IDLE;
    }
    if (outputs_[output].waiting) {
        if (options_.mapping == sched_type_t::MAP_AS_PREVIOUSLY &&
            outputs_[output].idle_start_count >= 0) {
            uint64_t duration = outputs_[output]
                                    .record[outputs_[output].record_index->load(
                                        std::memory_order_acquire)]
                                    .value.idle_duration;
            uint64_t now = outputs_[output].idle_count;
            if (now - outputs_[output].idle_start_count < duration) {
                VPRINT(this, 4,
                       "next_record[%d]: elapsed %" PRIu64 " < duration %" PRIu64 "\n",
                       output, now - outputs_[output].idle_start_count, duration);
                ++outputs_[output].idle_count;
                return sched_type_t::STATUS_IDLE;
            } else
                outputs_[output].idle_start_count = -1;
        }
        VPRINT(this, 5,
               "next_record[%d]: need new input (cur=waiting; idles=%" PRIu64 ")\n",
               output, outputs_[output].idle_count);
        stream_status_t res = pick_next_input(output, 0);
        if (res != sched_type_t::STATUS_OK && res != sched_type_t::STATUS_SKIPPED)
            return res;
        outputs_[output].waiting = false;
    }
    if (outputs_[output].cur_input < 0) {
        // This happens with more outputs than inputs.  For non-empty outputs we
        // require cur_input to be set to >=0 during init().
        stream_status_t status = eof_or_idle(output, outputs_[output].cur_input);
        assert(status != sched_type_t::STATUS_OK);
        if (status != sched_type_t::STATUS_STOLE)
            return status;
    }
    input = &inputs_[outputs_[output].cur_input];
    auto lock = std::unique_lock<mutex_dbg_owned>(*input->lock);
    // Since we do not ask for a start time, we have to check for the first record from
    // each input and set the time here.
    if (input->prev_time_in_quantum == 0)
        input->prev_time_in_quantum = cur_time;
    if (!outputs_[output].speculation_stack.empty()) {
        outputs_[output].prev_speculate_pc = outputs_[output].speculate_pc;
        error_string_ = outputs_[output].speculator.next_record(
            outputs_[output].speculate_pc, record);
        if (!error_string_.empty()) {
            VPRINT(this, 1, "next_record[%d]: speculation failed: %s\n", output,
                   error_string_.c_str());
            return sched_type_t::STATUS_INVALID;
        }
        // Leave the cur input where it is: the ordinals will remain unchanged.
        // Also avoid the context switch checks below as we cannot switch in the
        // middle of speculating (we also don't count speculated instructions toward
        // QUANTUM_INSTRUCTIONS).
        return sched_type_t::STATUS_OK;
    }
    while (true) {
        input->cur_from_queue = false;
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
            input->cur_from_queue = true;
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
            bool input_at_eof = input->at_eof || *input->reader == *input->reader_end;
            if (input_at_eof && input->to_inject_syscall != input_info_t::INJECT_NONE) {
                // The input's at eof but we have a syscall trace yet to be injected.
                stream_status_t res =
                    inject_pending_syscall_sequence(output, input, record);
                if (res != stream_status_t::STATUS_OK)
                    return res;
            } else if (input_at_eof) {
                if (!input->at_eof) {
                    stream_status_t status = mark_input_eof(*input);
                    if (status != sched_type_t::STATUS_OK)
                        return status;
                }
                lock.unlock();
                VPRINT(this, 5, "next_record[%d]: need new input (cur=%d eof)\n", output,
                       input->index);
                stream_status_t res = pick_next_input(output, 0);
                if (res != sched_type_t::STATUS_OK && res != sched_type_t::STATUS_SKIPPED)
                    return res;
                input = &inputs_[outputs_[output].cur_input];
                lock = std::unique_lock<mutex_dbg_owned>(*input->lock);
                if (res == sched_type_t::STATUS_SKIPPED) {
                    // Like for the ROI below, we need the queue or a de-ref.
                    input->needs_advance = false;
                }
                continue;
            } else {
                record = **input->reader;
            }
        }

        stream_status_t res =
            maybe_inject_pending_syscall_sequence(output, input, record);
        if (res != stream_status_t::STATUS_OK)
            return res;

        // Check whether all syscall injected records have been passed along
        // to the caller.
        trace_marker_type_t marker_type;
        uintptr_t marker_value_unused;
        if (input->in_syscall_injection &&
            record_type_is_marker(outputs_[output].last_record, marker_type,
                                  marker_value_unused) &&
            marker_type == TRACE_MARKER_TYPE_SYSCALL_TRACE_END) {
            input->in_syscall_injection = false;
        }
        VPRINT(this, 5,
               "next_record[%d]: candidate record from %d (@%" PRId64 "): ", output,
               input->index, get_instr_ordinal(*input));
        if (input->instrs_pre_read > 0 && record_type_is_instr(record))
            --input->instrs_pre_read;
        VDO(this, 5, print_record(record););

        // We want check_for_input_switch() to have the updated state, so we process
        // syscall trace related markers now.
        update_syscall_state(record, output);

        bool need_new_input = false;
        bool preempt = false;
        uint64_t blocked_time = 0;
        uint64_t prev_time_in_quantum = input->prev_time_in_quantum;
        res = check_for_input_switch(output, record, input, cur_time, need_new_input,
                                     preempt, blocked_time);
        if (res != sched_type_t::STATUS_OK && res != sched_type_t::STATUS_SKIPPED)
            return res;
        if (need_new_input) {
            int prev_input = outputs_[output].cur_input;
            VPRINT(this, 5, "next_record[%d]: need new input (cur=%d)\n", output,
                   prev_input);
            // We have to put the candidate record in the queue before we release
            // the lock since another output may grab this input.
            VPRINT(this, 5, "next_record[%d]: queuing candidate record\n", output);
            input->queue.push_back(record);
            lock.unlock();
            res = pick_next_input(output, blocked_time);
            if (res != sched_type_t::STATUS_OK && res != sched_type_t::STATUS_WAIT &&
                res != sched_type_t::STATUS_SKIPPED)
                return res;
            if (outputs_[output].cur_input != prev_input) {
                // TODO i#5843: Queueing here and in a few other places gets the stream
                // record and instruction ordinals off: we need to undo the ordinal
                // increases to avoid over-counting while queued and double-counting
                // when we resume.
                // In some cases we need to undo this on the output stream too.
                // So we should set suppress_ref_count_ in the input to get
                // is_record_synthetic() (and have our stream class check that
                // for instr count too) -- but what about output during speculation?
                // Decrement counts instead to undo?
                lock.lock();
                VPRINT(this, 5, "next_record_mid[%d]: switching from %d to %d\n", output,
                       prev_input, outputs_[output].cur_input);
                // We need to offset the {instrs,time_spent}_in_quantum values from
                // overshooting during dynamic scheduling, unless this is a preempt when
                // we've already reset to 0.
                if (!preempt && options_.mapping == sched_type_t::MAP_TO_ANY_OUTPUT) {
                    if (options_.quantum_unit == sched_type_t::QUANTUM_INSTRUCTIONS &&
                        record_type_is_instr_boundary(record,
                                                      outputs_[output].last_record)) {
                        assert(inputs_[prev_input].instrs_in_quantum > 0);
                        --inputs_[prev_input].instrs_in_quantum;
                    } else if (options_.quantum_unit == sched_type_t::QUANTUM_TIME) {
                        assert(inputs_[prev_input].time_spent_in_quantum >=
                               cur_time - prev_time_in_quantum);
                        inputs_[prev_input].time_spent_in_quantum -=
                            (cur_time - prev_time_in_quantum);
                    }
                }
                if (res == sched_type_t::STATUS_WAIT)
                    return res;
                input = &inputs_[outputs_[output].cur_input];
                lock = std::unique_lock<mutex_dbg_owned>(*input->lock);
                continue;
            } else {
                lock.lock();
                if (res != sched_type_t::STATUS_SKIPPED) {
                    // Get our candidate record back.
                    record = input->queue.back();
                    input->queue.pop_back();
                }
            }
            if (res == sched_type_t::STATUS_SKIPPED) {
                // Like for the ROI below, we need the queue or a de-ref.
                input->needs_advance = false;
                continue;
            }
        }
        if (input->needs_roi && options_.mapping != sched_type_t::MAP_AS_PREVIOUSLY &&
            !input->regions_of_interest.empty()) {
            input_ordinal_t prev_input = input->index;
            res = advance_region_of_interest(output, record, *input);
            if (res == sched_type_t::STATUS_SKIPPED) {
                // We need either the queue or to re-de-ref the reader so we loop,
                // but we do not want to come back here.
                input->needs_roi = false;
                input->needs_advance = false;
                continue;
            } else if (res == sched_type_t::STATUS_STOLE) {
                // We need to loop to get the new record.
                input = &inputs_[outputs_[output].cur_input];
                stream_status_t on_switch_res =
                    on_context_switch(output, prev_input, input->index);
                if (on_switch_res != stream_status_t::STATUS_OK) {
                    return on_switch_res;
                }
                lock.unlock();
                lock = std::unique_lock<mutex_dbg_owned>(*input->lock);
                lock.lock();
                continue;
            } else if (res != sched_type_t::STATUS_OK)
                return res;
        } else {
            input->needs_roi = true;
        }
        break;
    }
    update_next_record(output, record);
    VPRINT(this, 4, "next_record[%d]: from %d @%" PRId64 ": ", output, input->index,
           cur_time);
    VDO(this, 4, print_record(record););

    outputs_[output].last_record = record;
    record_type_has_tid(record, input->last_record_tid);
    record_type_has_pid(record, input->pid);
    return finalize_next_record(output, record, input);
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::finalize_next_record(
    output_ordinal_t output, const RecordType &record, input_info_t *input)
{
    trace_marker_type_t marker_type;
    uintptr_t marker_value;
    addr_t instr_pc;
    size_t instr_size;
    bool is_marker = record_type_is_marker(record, marker_type, marker_value);
    // Good to queue the injected records at this point, because we now surely will
    // be done with TRACE_MARKER_TYPE_SYSCALL.
    if (is_marker && marker_type == TRACE_MARKER_TYPE_SYSCALL &&
        syscall_sequence_.find(static_cast<int>(marker_value)) !=
            syscall_sequence_.end()) {
        assert(!input->in_syscall_injection);
        // The actual injection of the syscall trace happens later at the intended
        // point between the syscall function tracing markers.
        input->to_inject_syscall = static_cast<int>(marker_value);
        input->saw_first_func_id_marker_after_syscall = false;
    } else if (record_type_is_instr(record, &instr_pc, &instr_size)) {
        input->last_pc_fallthrough = instr_pc + instr_size;
    }
    if (is_marker) {
        // Turn idle+wait markers back into their respective status codes.
        if (marker_type == TRACE_MARKER_TYPE_CORE_IDLE)
            return stream_status_t::STATUS_IDLE;
        else if (marker_type == TRACE_MARKER_TYPE_CORE_WAIT)
            return stream_status_t::STATUS_WAIT;
    }
    return stream_status_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
void
scheduler_impl_tmpl_t<RecordType, ReaderType>::update_next_record(output_ordinal_t output,
                                                                  RecordType &record)
{
    // Initialize to zero to prevent uninit use errors.
    trace_marker_type_t marker_type = trace_marker_type_t::TRACE_MARKER_TYPE_KERNEL_EVENT;
    uintptr_t marker_value = 0;
    bool is_marker = record_type_is_marker(record, marker_type, marker_value);
    if (is_marker && marker_type == TRACE_MARKER_TYPE_FILETYPE) {
        record_type_set_marker_value(
            record, adjust_filetype(static_cast<offline_file_type_t>(marker_value)));
    }
    if (options_.mapping != sched_type_t::MAP_TO_ANY_OUTPUT &&
        options_.mapping != sched_type_t::MAP_AS_PREVIOUSLY)
        return; // Nothing to do.
    if (options_.replay_as_traced_istream != nullptr) {
        // Do not modify sched_type_t::MAP_TO_RECORDED_OUTPUT (turned into
        // sched_type_t::MAP_AS_PREVIOUSLY).
        return;
    }
    // We modify the tid and pid fields to ensure uniqueness across multiple workloads for
    // core-sharded-on-disk and with analyzers that look at the tid instead of using our
    // workload identifiers (and since the workload API is not there for
    // core-sharded-on-disk it may not be worth updating these analyzers).  To maintain
    // the original values, we write the workload ordinal into the top 32 bits.  We don't
    // support distinguishing for 32-bit-build record_filter.  We also ignore complexities
    // on Mac with its 64-bit tid type.
    int64_t workload = get_workload_ordinal(output);
    memref_tid_t cur_tid;
    if (record_type_has_tid(record, cur_tid) && workload > 0) {
        memref_tid_t new_tid = (workload << MEMREF_ID_WORKLOAD_SHIFT) | cur_tid;
        record_type_set_tid(record, new_tid);
    }
    memref_pid_t cur_pid;
    if (record_type_has_pid(record, cur_pid) && workload > 0) {
        memref_tid_t new_pid = (workload << MEMREF_ID_WORKLOAD_SHIFT) | cur_pid;
        record_type_set_pid(record, new_pid);
    }
    // For a dynamic schedule, the as-traced cpuids and timestamps no longer
    // apply and are just confusing (causing problems like interval analysis
    // failures), so we replace them.
    if (!is_marker)
        return; // Nothing to do.
    if (marker_type == TRACE_MARKER_TYPE_TIMESTAMP) {
        if (outputs_[output].base_timestamp == 0) {
            // Record the first input's first timestamp, as a base value.
#ifndef NDEBUG
            bool ok =
#endif
                record_type_is_timestamp(record, outputs_[output].base_timestamp);
            assert(ok);
            assert(outputs_[output].base_timestamp != 0);
            VPRINT(this, 2, "output %d base timestamp = %zu\n", output,
                   outputs_[output].base_timestamp);
        }
        uint64_t instr_ord = outputs_[output].stream->get_instruction_ordinal();
        uint64_t idle_count = outputs_[output].idle_count;
        uintptr_t new_time = static_cast<uintptr_t>(
            outputs_[output].base_timestamp + (instr_ord + idle_count) / INSTRS_PER_US);
        VPRINT(this, 4,
               "New time in output %d: %zu from base %zu and instrs %" PRIu64
               " idles %" PRIu64 "\n",
               output, new_time, outputs_[output].base_timestamp, instr_ord, idle_count);
#ifndef NDEBUG
        bool ok =
#endif
            record_type_set_marker_value(record, new_time);
        assert(ok);
    } else if (marker_type == TRACE_MARKER_TYPE_CPU_ID) {
#ifndef NDEBUG
        bool ok =
#endif
            record_type_set_marker_value(record, get_shard_index(output));
        assert(ok);
    }
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::unread_last_record(output_ordinal_t output,
                                                                  RecordType &record,
                                                                  input_info_t *&input)
{
    auto &outinfo = outputs_[output];
    if (record_type_is_invalid(outinfo.last_record))
        return sched_type_t::STATUS_INVALID;
    if (!outinfo.speculation_stack.empty())
        return sched_type_t::STATUS_INVALID;
    record = outinfo.last_record;
    input = &inputs_[outinfo.cur_input];
    std::lock_guard<mutex_dbg_owned> lock(*input->lock);
    VPRINT(this, 4, "next_record[%d]: unreading last record, from %d\n", output,
           input->index);
    input->queue.push_back(outinfo.last_record);
    // XXX: This should be record_type_is_instr_boundary() but we don't have the pre-prev
    // record.  For now we don't support unread_last_record() for record_reader_t,
    // enforced in a specialization of unread_last_record().
    if (options_.quantum_unit == sched_type_t::QUANTUM_INSTRUCTIONS &&
        record_type_is_instr(record))
        --input->instrs_in_quantum;
    outinfo.last_record = create_invalid_record();
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::start_speculation(
    output_ordinal_t output, addr_t start_address, bool queue_current_record)
{
    auto &outinfo = outputs_[output];
    if (outinfo.speculation_stack.empty()) {
        if (queue_current_record) {
            if (record_type_is_invalid(outinfo.last_record))
                return sched_type_t::STATUS_INVALID;
            inputs_[outinfo.cur_input].queue.push_back(outinfo.last_record);
        }
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
scheduler_impl_tmpl_t<RecordType, ReaderType>::stop_speculation(output_ordinal_t output)
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
scheduler_impl_tmpl_t<RecordType, ReaderType>::mark_input_eof(input_info_t &input)
{
    assert(input.lock->owned_by_cur_thread());
    if (input.at_eof)
        return sched_type_t::STATUS_OK;
    input.at_eof = true;
#ifndef NDEBUG
    int old_count =
#endif
        live_input_count_.fetch_add(-1, std::memory_order_release);
    assert(old_count > 0);
    int live_inputs = live_input_count_.load(std::memory_order_acquire);
    VPRINT(this, 2, "input %d at eof; %d live inputs left\n", input.index, live_inputs);
    if (options_.mapping == sched_type_t::MAP_TO_ANY_OUTPUT &&
        live_inputs <=
            static_cast<int>(inputs_.size() * options_.exit_if_fraction_inputs_left)) {
        VPRINT(this, 1, "exiting early at input %d with %d live inputs left\n",
               input.index, live_inputs);
        return sched_type_t::STATUS_EOF;
    }
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::eof_or_idle(output_ordinal_t output,
                                                           input_ordinal_t prev_input)
{
    stream_status_t res = eof_or_idle_for_mode(output, prev_input);
    // We should either get STATUS_IDLE (success, and we continue below) or
    // STATUS_EOF (success, and we exit this funcion) or some error (and we exit).
    // A return value of STATUS_OK is not allowed, as documented.
    assert(res != sched_type_t::STATUS_OK);
    if (res != sched_type_t::STATUS_IDLE)
        return res;
    // We rely on rebalancing to handle the case of every input being unscheduled.
    outputs_[output].waiting = true;
    if (prev_input != sched_type_t::INVALID_INPUT_ORDINAL)
        ++outputs_[output].stats[memtrace_stream_t::SCHED_STAT_SWITCH_INPUT_TO_IDLE];
    set_cur_input(output, sched_type_t::INVALID_INPUT_ORDINAL);
    ++outputs_[output].idle_count;
    return sched_type_t::STATUS_IDLE;
}

template <typename RecordType, typename ReaderType>
double
scheduler_impl_tmpl_t<RecordType, ReaderType>::get_statistic(
    output_ordinal_t output, memtrace_stream_t::schedule_statistic_t stat) const
{
    if (stat >= memtrace_stream_t::SCHED_STAT_TYPE_COUNT)
        return -1;
    return static_cast<double>(outputs_[output].stats[stat]);
}

template <typename RecordType, typename ReaderType>
offline_file_type_t
scheduler_impl_tmpl_t<RecordType, ReaderType>::adjust_filetype(
    offline_file_type_t orig_filetype) const
{
    uintptr_t filetype = static_cast<uintptr_t>(orig_filetype);
    if (!syscall_sequence_.empty()) {
        // If the read syscall_sequence_ does not have any trace for the
        // syscalls actually present in the trace, we may end up without any
        // syscall trace despite the following filetype bit set.
        filetype |= OFFLINE_FILE_TYPE_KERNEL_SYSCALLS;
    }
    return static_cast<offline_file_type_t>(filetype);
}

template class scheduler_impl_tmpl_t<memref_t, reader_t>;
template class scheduler_impl_tmpl_t<trace_entry_t,
                                     dynamorio::drmemtrace::record_reader_t>;

} // namespace drmemtrace
} // namespace dynamorio
