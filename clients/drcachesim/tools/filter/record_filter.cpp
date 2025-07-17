/* **********************************************************
 * Copyright (c) 2022-2025 Google, Inc.  All rights reserved.
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

#include "record_filter.h"

#include <inttypes.h>
#include <stdint.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef HAS_ZLIB
#    include "common/gzip_ostream.h"
#endif
#ifdef HAS_ZIP
#    include "common/zipfile_ostream.h"
#endif
#include "memref.h"
#include "memtrace_stream.h"
#include "raw2trace_shared.h"
#include "trace_entry.h"
#include "utils.h"
#include "null_filter.h"
#include "cache_filter.h"
#include "trim_filter.h"
#include "type_filter.h"
#include "encodings2regdeps_filter.h"
#include "func_id_filter.h"
#include "modify_marker_value_filter.h"

#undef VPRINT
#ifdef DEBUG
#    define VPRINT(reader, level, ...)                            \
        do {                                                      \
            if ((reader)->verbosity_ >= (level)) {                \
                fprintf(stderr, "%s ", (reader)->output_prefix_); \
                fprintf(stderr, __VA_ARGS__);                     \
            }                                                     \
        } while (0)
// clang-format off
#    define UNUSED(x) /* nothing */
// clang-format on
#else
#    define VPRINT(reader, level, ...) /* nothing */
#    define UNUSED(x) ((void)(x))
#endif

namespace dynamorio {
namespace drmemtrace {

namespace {

template <typename T>
std::vector<T>
parse_string(const std::string &s, char sep = ',')
{
    size_t pos, at = 0;
    if (s.empty())
        return {};
    std::vector<T> vec;
    do {
        pos = s.find(sep, at);
        // base = 0 allows to handle both decimal and hex numbers.
        unsigned long long parsed_number =
            std::stoull(s.substr(at, pos), nullptr, /*base = */ 0);
        // XXX: parsed_number may be truncated if T is not large enough.
        // We could check that parsed_number is within the limits of T using
        // std::numeric_limits<>::min()/max(), but this returns 0 on T that are enums,
        // which we have when parsing trace_marker_type_t and trace_type_t values for
        // type_filter. In order to make numeric_limits work on enum, we need to add
        // std::underlying_type support to these enums.
        // We also need to consider what should happen when T is not large enough to
        // contain parsed_number. Should we skip that value? Output a warning? Output an
        // error and abort?
        vec.push_back(static_cast<T>(parsed_number));
        at = pos + 1;
    } while (pos != std::string::npos);
    return vec;
}

} // namespace

record_analysis_tool_t *
record_filter_tool_create(const std::string &output_dir, uint64_t stop_timestamp,
                          int cache_filter_size, const std::string &remove_trace_types,
                          const std::string &remove_marker_types,
                          uint64_t trim_before_timestamp, uint64_t trim_after_timestamp,
                          uint64_t trim_before_instr, uint64_t trim_after_instr,
                          bool encodings2regdeps, const std::string &keep_func_ids,
                          const std::string &modify_marker_value, unsigned int verbose)
{
    std::vector<
        std::unique_ptr<dynamorio::drmemtrace::record_filter_t::record_filter_func_t>>
        filter_funcs;
    if (cache_filter_size > 0) {
        filter_funcs.emplace_back(
            std::unique_ptr<dynamorio::drmemtrace::record_filter_t::record_filter_func_t>(
                // XXX: add more command-line options to allow the user to set these
                // parameters.
                new dynamorio::drmemtrace::cache_filter_t(
                    /*cache_associativity=*/1, /*cache_line_size=*/64, cache_filter_size,
                    /*filter_data=*/true, /*filter_instrs=*/false)));
    }
    if (!remove_trace_types.empty() || !remove_marker_types.empty()) {
        std::vector<trace_type_t> filter_trace_types =
            parse_string<trace_type_t>(remove_trace_types);
        std::vector<trace_marker_type_t> filter_marker_types =
            parse_string<trace_marker_type_t>(remove_marker_types);
        filter_funcs.emplace_back(
            std::unique_ptr<dynamorio::drmemtrace::record_filter_t::record_filter_func_t>(
                new dynamorio::drmemtrace::type_filter_t(filter_trace_types,
                                                         filter_marker_types)));
    }
    if (trim_before_timestamp > 0 || trim_after_timestamp > 0 || trim_before_instr > 0 ||
        trim_after_instr > 0) {
        filter_funcs.emplace_back(
            std::unique_ptr<dynamorio::drmemtrace::record_filter_t::record_filter_func_t>(
                new dynamorio::drmemtrace::trim_filter_t(
                    trim_before_timestamp, trim_after_timestamp, trim_before_instr,
                    trim_after_instr)));
    }
    if (encodings2regdeps) {
        filter_funcs.emplace_back(
            std::unique_ptr<dynamorio::drmemtrace::record_filter_t::record_filter_func_t>(
                new dynamorio::drmemtrace::encodings2regdeps_filter_t()));
    }
    if (!keep_func_ids.empty()) {
        std::vector<uint64_t> keep_func_ids_list = parse_string<uint64_t>(keep_func_ids);
        filter_funcs.emplace_back(
            std::unique_ptr<dynamorio::drmemtrace::record_filter_t::record_filter_func_t>(
                new dynamorio::drmemtrace::func_id_filter_t(keep_func_ids_list)));
    }
    if (!modify_marker_value.empty()) {
        std::vector<uint64_t> modify_marker_value_pairs_list =
            parse_string<uint64_t>(modify_marker_value);
        filter_funcs.emplace_back(
            std::unique_ptr<dynamorio::drmemtrace::record_filter_t::record_filter_func_t>(
                new dynamorio::drmemtrace::modify_marker_value_filter_t(
                    modify_marker_value_pairs_list)));
    }

    // TODO i#5675: Add other filters.

    return new dynamorio::drmemtrace::record_filter_t(output_dir, std::move(filter_funcs),
                                                      stop_timestamp, verbose);
}

record_filter_t::record_filter_t(
    const std::string &output_dir,
    std::vector<std::unique_ptr<record_filter_func_t>> filters, uint64_t stop_timestamp,
    unsigned int verbose)
    : output_dir_(output_dir)
    , filters_(std::move(filters))
    , stop_timestamp_(stop_timestamp)
    , verbosity_(verbose)
{
    UNUSED(verbosity_);
    UNUSED(output_prefix_);
}

record_filter_t::~record_filter_t()
{
    for (auto &iter : shard_map_) {
        delete iter.second;
    }
}

bool
record_filter_t::parallel_shard_supported()
{
    return true;
}

std::string
record_filter_t::initialize_shard_type(shard_type_t shard_type)
{
    shard_type_ = shard_type;
    return "";
}

std::string
record_filter_t::get_output_basename(memtrace_stream_t *shard_stream)
{
    if (shard_type_ == SHARD_BY_CORE) {
        // Use leading 0's for the core id to ensure lexicographic sort keeps
        // numeric core order for --only_shards.
        std::ostringstream name;
        name << output_dir_ << DIRSEP << "drmemtrace.core." << std::setfill('0')
             << std::setw(6) << shard_stream->get_shard_index() << ".trace";
        return name.str();
    } else {
        return output_dir_ + DIRSEP + shard_stream->get_stream_name();
    }
}

std::string
record_filter_t::initialize_shard_output(per_shard_t *per_shard,
                                         memtrace_stream_t *shard_stream)
{
    if (shard_type_ == SHARD_BY_CORE) {
        // Each output is a mix of inputs so we do not want to reuse the input
        // names with tids.
        // Since some shards may not have inputs, we need to synchronize determining
        // the file extension.
        // First, get our path without the extension, so we can add it later.
        per_shard->output_path = get_output_basename(shard_stream);
        std::string input_name = shard_stream->get_stream_name();
        // Now synchronize determining the extension.
        auto lock = std::unique_lock<std::mutex>(input_info_mutex_);
        if (!output_ext_.empty()) {
            VPRINT(this, 2,
                   "Shard #%d using pre-set ext=%s, ver=%" PRIu64 ", type=%" PRIu64 "\n",
                   shard_stream->get_shard_index(), output_ext_.c_str(), version_,
                   filetype_);
            per_shard->output_path += output_ext_;
            per_shard->filetype = static_cast<addr_t>(filetype_);
            lock.unlock();
        } else if (!input_name.empty()) {
            size_t last_dot = input_name.rfind('.');
            if (last_dot == std::string::npos)
                return "Failed to determine filename type from extension";
            output_ext_ = input_name.substr(last_dot);
            // Set the other key input data.
            version_ = shard_stream->get_version();
            filetype_ = add_to_filetype(shard_stream->get_filetype());
            if (version_ == 0) {
                // We give up support for version 0 to have an up-front error check
                // rather than having some output files with bad headers (i#6721).
                return "Version not available at shard init time";
            }
            VPRINT(this, 2,
                   "Shard #%d setting ext=%s, ver=%" PRIu64 ", type=%" PRIu64 "\n",
                   shard_stream->get_shard_index(), output_ext_.c_str(), version_,
                   filetype_);
            per_shard->output_path += output_ext_;
            per_shard->filetype = static_cast<addr_t>(filetype_);
            lock.unlock();
            input_info_cond_var_.notify_all();
        } else {
            // We have to wait for another shard with an input to set output_ext_.
            input_info_cond_var_.wait(lock, [this] { return !output_ext_.empty(); });
            VPRINT(this, 2,
                   "Shard #%d waited for ext=%s, ver=%" PRIu64 ", type=%" PRIu64 "\n",
                   shard_stream->get_shard_index(), output_ext_.c_str(), version_,
                   filetype_);
            per_shard->output_path += output_ext_;
            per_shard->filetype = static_cast<addr_t>(filetype_);
            lock.unlock();
        }
    } else {
        per_shard->output_path = get_output_basename(shard_stream);
    }
    return "";
}

std::string
record_filter_t::get_writer(per_shard_t *per_shard, memtrace_stream_t *shard_stream)
{
    if (per_shard->output_path.empty())
        return "Error: output_path is empty";
#ifdef HAS_ZLIB
    if (ends_with(per_shard->output_path, ".gz")) {
        VPRINT(this, 3, "Using the gzip writer for %s\n", per_shard->output_path.c_str());
        per_shard->file_writer =
            std::unique_ptr<std::ostream>(new gzip_ostream_t(per_shard->output_path));
        per_shard->writer = per_shard->file_writer.get();
        return "";
    }
#endif
#ifdef HAS_ZIP
    if (ends_with(per_shard->output_path, ".zip")) {
        VPRINT(this, 3, "Using the zip writer for %s\n", per_shard->output_path.c_str());
        per_shard->archive_writer = std::unique_ptr<archive_ostream_t>(
            new zipfile_ostream_t(per_shard->output_path));
        per_shard->writer = per_shard->archive_writer.get();
        return open_new_chunk(per_shard);
    }
#endif
    VPRINT(this, 3, "Using the default writer for %s\n", per_shard->output_path.c_str());
    per_shard->file_writer = std::unique_ptr<std::ostream>(
        new std::ofstream(per_shard->output_path, std::ofstream::binary));
    per_shard->writer = per_shard->file_writer.get();
    return "";
}

std::string
record_filter_t::remove_output_file(per_shard_t *per_shard)
{
    VPRINT(this, 1, "Removing zero-instruction file %s for tid %" PRId64 "\n",
           per_shard->output_path.c_str(), per_shard->tid);
    if (std::remove(per_shard->output_path.c_str()) != 0)
        return "Failed to remove zero-instruction file " + per_shard->output_path;
    return "";
}

std::string
record_filter_t::emit_marker(per_shard_t *shard, unsigned short marker_type,
                             uint64_t marker_value)
{
    trace_entry_t marker;
    marker.type = TRACE_TYPE_MARKER;
    marker.size = marker_type;
    marker.addr = static_cast<addr_t>(marker_value);
    if (!write_trace_entry(shard, marker))
        return "Failed to write marker";
    return "";
}

std::string
record_filter_t::open_new_chunk(per_shard_t *shard)
{
    VPRINT(this, 1, "Opening new chunk #%" PRIu64 "\n", shard->chunk_ordinal);
    std::string err;
    if (shard->chunk_ordinal > 0) {
        err =
            emit_marker(shard, TRACE_MARKER_TYPE_CHUNK_FOOTER, shard->chunk_ordinal - 1);
        if (!err.empty())
            return err;
    }

    std::ostringstream stream;
    stream << TRACE_CHUNK_PREFIX << std::setfill('0')
           << std::setw(TRACE_CHUNK_SUFFIX_WIDTH) << shard->chunk_ordinal;
    err = shard->archive_writer->open_new_component(stream.str());
    if (!err.empty())
        return err;

    if (shard->chunk_ordinal > 0) {
        // XXX i#6593: This sequence is currently duplicated with
        // raw2trace_t::emit_new_chunk_header().  Could we share it?
        err = emit_marker(shard, TRACE_MARKER_TYPE_RECORD_ORDINAL, shard->cur_refs);
        if (!err.empty())
            return err;
        err = emit_marker(shard, TRACE_MARKER_TYPE_TIMESTAMP, shard->last_timestamp);
        if (!err.empty())
            return err;
        err = emit_marker(shard, TRACE_MARKER_TYPE_CPU_ID, shard->last_cpu_id);
        if (!err.empty())
            return err;
        // We need to re-emit all encodings.
        shard->cur_chunk_pcs.clear();
    }

    ++shard->chunk_ordinal;
    shard->cur_chunk_instrs = 0;

    return "";
}

std::string
record_filter_t::initialize_stream(memtrace_stream_t *serial_stream)
{
    dcontext_.dcontext = dr_standalone_init();
    return "";
}

void *
record_filter_t::parallel_shard_init_stream(int shard_index, void *worker_data,
                                            memtrace_stream_t *shard_stream)
{
    auto per_shard = new per_shard_t;
    std::string error = initialize_shard_output(per_shard, shard_stream);
    if (!error.empty()) {
        per_shard->error = "Failure initializing output: " + error;
        success_ = false;
        return reinterpret_cast<void *>(per_shard);
    }
    error = get_writer(per_shard, shard_stream);
    if (!error.empty()) {
        per_shard->error = "Failure in opening writer: " + error;
        success_ = false;
        return reinterpret_cast<void *>(per_shard);
    }
    if (per_shard->writer == nullptr) {
        per_shard->error = "Could not open a writer for " + per_shard->output_path;
        success_ = false;
        return reinterpret_cast<void *>(per_shard);
    }
    per_shard->shard_stream = shard_stream;
    per_shard->enabled = true;
    per_shard->input_entry_count = 0;
    per_shard->output_entry_count = 0;
    per_shard->tid = shard_stream->get_tid();
    if (shard_type_ == SHARD_BY_CORE) {
        per_shard->memref_counter.set_core_sharded(true);
    }
    for (auto &f : filters_) {
        per_shard->filter_shard_data.push_back(
            f->parallel_shard_init(shard_stream, stop_timestamp_ != 0));
        if (f->get_error_string() != "") {
            per_shard->error =
                "Failure in initializing filter function " + f->get_error_string();
            success_ = false;
        }
    }
    per_shard->record_filter_info.last_encoding = &per_shard->last_encoding;
    per_shard->record_filter_info.dcontext = dcontext_.dcontext;
    std::lock_guard<std::mutex> guard(shard_map_mutex_);
    shard_map_[shard_index] = per_shard;
    return reinterpret_cast<void *>(per_shard);
}

bool
record_filter_t::parallel_shard_exit(void *shard_data)
{
    per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);
    bool res = true;
    for (int i = 0; i < static_cast<int>(filters_.size()); ++i) {
        if (!filters_[i]->parallel_shard_exit(per_shard->filter_shard_data[i]))
            res = false;
    }
    if (per_shard->last_written_record.type != TRACE_TYPE_FOOTER) {
        // When core-sharded some cores can end in TRACE_TYPE_IDLE.
        // i#6703: The scheduler should add this footer for us.
        trace_entry_t footer = {};
        footer.type = TRACE_TYPE_FOOTER;
        if (!write_trace_entry(per_shard, footer)) {
            per_shard->error = "Failed to write footer";
            return false;
        }
    }
    // Destroy the writer since we do not need it anymore. This also makes sure
    // that data is written out to the file; curiously, a simple flush doesn't
    // do it.
    per_shard->file_writer.reset(nullptr);
    per_shard->archive_writer.reset(nullptr);
    per_shard->writer = nullptr;
    // If the shard ended up with no instructions, delete it (otherwise the
    // invariant checker complains).
    VPRINT(this, 2, "shard %s chunk=%" PRIu64 " cur-instrs=%" PRIu64 "\n",
           per_shard->output_path.c_str(), per_shard->chunk_ordinal,
           per_shard->cur_chunk_instrs);
    if (!TESTANY(OFFLINE_FILE_TYPE_FILTERED | OFFLINE_FILE_TYPE_IFILTERED,
                 per_shard->filetype) &&
        // chunk_ordinal is 1 after the init-time call for archives; it
        // remains 0 for non-archives.
        per_shard->chunk_ordinal <= 1 && per_shard->cur_chunk_instrs == 0 &&
        // Leave a core-sharded completely-idle file.
        shard_type_ != SHARD_BY_CORE) {
        // Mark for removal.  We delay removal in case it involves global
        // operations that might race with other workers.
        per_shard->now_empty = true;
    }
    return res;
}

std::string
record_filter_t::parallel_shard_error(void *shard_data)
{
    per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);
    return per_shard->error;
}

bool
record_filter_t::write_trace_entry(per_shard_t *shard, const trace_entry_t &entry)
{
    if (shard->output_entry_count == 0 && entry.type != TRACE_TYPE_HEADER) {
        // When core-sharded with initially-idle cores we can start without a header.
        // XXX i#6703: The scheduler should insert these headers for us, as this
        // issue can affect other tools as well.
        // Our own stream's version + filetype are 0 so we use another shard's.
        std::lock_guard<std::mutex> guard(input_info_mutex_);
        std::vector<trace_entry_t> header;
        header.push_back({ TRACE_TYPE_HEADER, 0, { static_cast<addr_t>(version_) } });
        header.push_back({ TRACE_TYPE_MARKER,
                           TRACE_MARKER_TYPE_VERSION,
                           { static_cast<addr_t>(version_) } });
        header.push_back({ TRACE_TYPE_MARKER,
                           TRACE_MARKER_TYPE_FILETYPE,
                           { static_cast<addr_t>(filetype_) } });
        // file_reader_t::open_input_file demands tid+pid so we insert sentinel values.
        // We can't use INVALID_THREAD_ID as scheduler_t::open_reader() loops until
        // record_type_has_tid() which requires record.marker.tid != INVALID_THREAD_ID.
        header.push_back({ TRACE_TYPE_THREAD,
                           sizeof(thread_id_t),
                           { static_cast<addr_t>(IDLE_THREAD_ID) } });
        header.push_back({ TRACE_TYPE_PID,
                           sizeof(process_id_t),
                           { static_cast<addr_t>(INVALID_PID) } });
        // The scheduler itself demands a timestamp,cpuid pair.
        // We don't have a good value to use here though:
        // XXX i#6703: The scheduler should insert these for us.
        // As-is, these can cause confusion with -1 values, but this is our best
        // effort support until i#6703.
        header.push_back({ TRACE_TYPE_MARKER,
                           TRACE_MARKER_TYPE_TIMESTAMP,
                           { static_cast<addr_t>(-1) } });
        header.push_back(
            { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID, { static_cast<addr_t>(-1) } });
        if (!write_trace_entries(shard, header)) {
            shard->error += "Failed to write synthetic header";
            return false;
        }
    }
    if (!shard->writer->write((char *)&entry, sizeof(entry))) {
        shard->error = "Failed to write to output file " + shard->output_path;
        success_ = false;
        return false;
    }
    shard->cur_refs += shard->memref_counter.entry_memref_count(&entry);
    ++shard->output_entry_count;
    shard->last_written_record = entry;
    return true;
}

bool
record_filter_t::write_trace_entries(per_shard_t *shard,
                                     const std::vector<trace_entry_t> &entries)
{
    for (const trace_entry_t &entry : entries) {
        if (!write_trace_entry(shard, entry))
            return false;
    }
    return true;
}

std::string
record_filter_t::process_markers(per_shard_t *per_shard, trace_entry_t &entry,
                                 bool &output)
{
    if (entry.type == TRACE_TYPE_MARKER) {
        switch (entry.size) {
        case TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT:
            per_shard->chunk_size = entry.addr;
            break;
        case TRACE_MARKER_TYPE_FILETYPE:
            entry.addr = static_cast<addr_t>(add_to_filetype(entry.addr));
            per_shard->filetype = entry.addr;
            break;
        case TRACE_MARKER_TYPE_CHUNK_FOOTER:
            // We insert ourselves in open_new_chunk().
            output = false;
            break;
        case TRACE_MARKER_TYPE_RECORD_ORDINAL:
            // We insert ourselves in open_new_chunk().
            per_shard->input_count_at_ordinal = per_shard->input_entry_count;
            output = false;
            break;
        case TRACE_MARKER_TYPE_TIMESTAMP:
            if (output)
                per_shard->last_timestamp = entry.addr;
            // We insert our own start-of-chunk timestamp.
            if (per_shard->archive_writer &&
                per_shard->input_entry_count - per_shard->input_count_at_ordinal == 1)
                output = false;
            break;
        case TRACE_MARKER_TYPE_CPU_ID:
            if (output)
                per_shard->last_cpu_id = entry.addr;
            // We insert our own start-of-chunk cpuid.
            if (per_shard->archive_writer &&
                per_shard->input_entry_count - per_shard->input_count_at_ordinal == 2)
                output = false;
            if (output) {
                uint64_t instr_ord = per_shard->cur_chunk_instrs +
                    // For archives we increment chunk_ordinal up front.
                    (per_shard->archive_writer ? per_shard->chunk_ordinal - 1
                                               : per_shard->chunk_ordinal) *
                        per_shard->chunk_size;
                per_shard->sched_info.record_cpu_id(per_shard->tid, entry.addr,
                                                    per_shard->last_timestamp, instr_ord);
            }
            break;
        case TRACE_MARKER_TYPE_PHYSICAL_ADDRESS:
        case TRACE_MARKER_TYPE_PHYSICAL_ADDRESS_NOT_AVAILABLE:
            if (!output && per_shard->archive_writer) {
                // TODO i#6654: These markers need to be repeated across chunks.  Even
                // raw2trace doesn't support this yet: once we add it there we can add it
                // here or try to share code.
                return "Removing physical address markers from archive output is not yet "
                       "supported";
            }
            break;
        case TRACE_MARKER_TYPE_CORE_WAIT:
            // These are artificial timing records: do not output them, nor consider
            // them real input records.
            output = false;
            --per_shard->input_entry_count;
            break;
        }
    }
    return "";
}

std::string
record_filter_t::process_chunk_encodings(per_shard_t *per_shard, trace_entry_t &entry,
                                         bool output)
{
    if (!per_shard->archive_writer ||
        !is_any_instr_type(static_cast<trace_type_t>(entry.type)))
        return "";
    if (!per_shard->last_encoding.empty()) {
        if (per_shard->per_input == nullptr)
            return "Invalid input id for instruction";
        std::lock_guard<std::mutex> guard(per_shard->per_input->lock);
        per_shard->per_input->pc2encoding[entry.addr] = per_shard->last_encoding;
        // Disable the just-delayed encoding output in process_delayed_encodings() if
        // this is what used to be a new-chunk encoding but is no longer.
        if (per_shard->cur_chunk_pcs.find(entry.addr) != per_shard->cur_chunk_pcs.end()) {
            VPRINT(this, 3, "clearing new-chunk last encoding @pc=0x%zx\n", entry.addr);
            per_shard->last_encoding.clear();
        }
    } else if (output) {
        // Insert the cached encoding if this is the first instance of this PC
        // (without an encoding) in this chunk, unless the user is removing all encodings.
        // XXX: What if there is a filter removing all encodings but only
        // to the stop point, so a partial remove that does not change
        // the filetype?  For now we do not support that, and we re-add
        // encodings at chunk boundaries regardless. Note that filters that modify
        // encodings (even if they add or remove trace_entry_t records) do not incur in
        // this problem and we don't need support for partial removal of encodings in this
        // case. An example of such filters is encodings2regdeps_filter_t.
        if (TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, per_shard->filetype) &&
            per_shard->cur_chunk_pcs.find(entry.addr) == per_shard->cur_chunk_pcs.end()) {
            if (per_shard->per_input == nullptr)
                return "Invalid input id for instruction";
            std::lock_guard<std::mutex> guard(per_shard->per_input->lock);
            if (per_shard->per_input->pc2encoding.find(entry.addr) ==
                per_shard->per_input->pc2encoding.end()) {
                return "Missing encoding for PC " + std::to_string(entry.addr) +
                    " in shard " + per_shard->shard_stream->get_stream_name() +
                    " at input entry " + std::to_string(per_shard->input_entry_count);
            }
            VPRINT(this, 3,
                   "output new-chunk encoding chunk=%" PRIu64 " ref=%" PRIu64 "\n",
                   per_shard->chunk_ordinal, per_shard->cur_refs);
            // Sanity check that the encoding size is correct.
            const auto &enc = per_shard->per_input->pc2encoding[entry.addr];
            /* OFFLINE_FILE_TYPE_ARCH_REGDEPS traces have encodings with size != ifetch.
             * It's a design choice, not an error, hence we avoid this sanity check.
             */
            if (!TESTANY(OFFLINE_FILE_TYPE_ARCH_REGDEPS, per_shard->filetype)) {
                size_t enc_sz = 0;
                // Since all but the last entry are fixed-size we could avoid a loop
                // but the loop is easier to read and we have just 1 or 2 iters.
                for (const auto &record : enc)
                    enc_sz += record.size;
                if (enc_sz != entry.size) {
                    return "New-chunk encoding size " + std::to_string(enc_sz) +
                        " != instr size " + std::to_string(entry.size);
                }
            }
            if (!write_trace_entries(per_shard, enc)) {
                return "Failed to write";
            }
            // Avoid emitting the encoding twice.
            per_shard->delayed_encodings[entry.addr].clear();
        }
    }
    if (output)
        per_shard->cur_chunk_pcs.insert(entry.addr);
    return "";
}

std::string
record_filter_t::process_delayed_encodings(per_shard_t *per_shard, trace_entry_t &entry,
                                           bool output)
{
    if (!is_any_instr_type(static_cast<trace_type_t>(entry.type)))
        return "";
    if (!output) {
        if (!per_shard->last_encoding.empty()) {
            // Overwrite in case the encoding for this pc was already recorded.
            per_shard->delayed_encodings[entry.addr] =
                std::move(per_shard->last_encoding);
        }
    } else if (TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, per_shard->filetype)) {
        // Output if we have encodings that haven't yet been output, and
        // there is no filter removing all encodings (we don't support
        // partial encoding removal). Note that filters that modify encodings (even if
        // they add or remove trace_entry_t records) do not incur in this problem and we
        // don't need support for partial removal of encodings in this case. An example
        // of such filters is encodings2regdeps_filter_t.
        // We check prev_was_output to rule out filtered-out encodings
        // (we record all encodings for new-chunk insertion).
        if (!per_shard->last_encoding.empty() && per_shard->prev_was_output) {
            // This instruction is accompanied by a preceding encoding. Since
            // this instruction is not filtered out, output the encoding now.
            VPRINT(this, 3,
                   "output just-delayed encoding chunk=%" PRIu64 " ref=%" PRIu64
                   " pc=0x%zx\n",
                   per_shard->chunk_ordinal, per_shard->cur_refs, entry.addr);
            if (!write_trace_entries(per_shard, per_shard->last_encoding)) {
                return "Failed to write";
            }
            // Remove previously delayed encoding that doesn't need to be output
            // now that we have a more recent version for this instr.
            per_shard->delayed_encodings.erase(entry.addr);
        } else if (!per_shard->delayed_encodings[entry.addr].empty()) {
            // The previous instance of this instruction was filtered out and
            // its encoding was saved. Now that we have an instance of the same
            // instruction that is not filtered out, we need to output its
            // encoding.
            VPRINT(this, 3,
                   "output long-delayed encoding chunk=%" PRIu64 " ref=%" PRIu64
                   " pc=0x%zx\n",
                   per_shard->chunk_ordinal, per_shard->cur_refs, entry.addr);
            if (!write_trace_entries(per_shard,
                                     per_shard->delayed_encodings[entry.addr])) {
                return "Failed to write";
            }
            per_shard->delayed_encodings.erase(entry.addr);
        }
    }
    return "";
}

bool
record_filter_t::parallel_shard_memref(void *shard_data, const trace_entry_t &input_entry)
{
    if (!success_) {
        // Report an error that happened during shard init.
        return false;
    }
    per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);
    ++per_shard->input_entry_count;
    trace_entry_t entry = input_entry;
    bool output = true;
    // XXX i#7404: Once we have multi-workload inputs we'll want all our PC keys to become
    // pairs <get_workload_ordinal(), PC>.
    if (per_shard->shard_stream->get_workload_id() != per_shard->prev_workload_id &&
        per_shard->shard_stream->get_workload_id() >= 0 &&
        per_shard->prev_workload_id >= 0) {
        per_shard->error = "Multi-workload inputs not yet supported";
        return false;
    }
    int64_t input_id = per_shard->shard_stream->get_input_id();
    if (per_shard->prev_input_id != input_id) {
        VPRINT(this, 3,
               "shard %d switch from %" PRId64 " to %" PRId64 " (refs=%" PRIu64
               " instrs=%" PRIu64 ")\n",
               per_shard->shard_stream->get_shard_index(), per_shard->prev_input_id,
               input_id,
               per_shard->shard_stream->get_input_interface() == nullptr
                   ? 0
                   : per_shard->shard_stream->get_input_interface()->get_record_ordinal(),
               per_shard->shard_stream->get_input_interface() == nullptr
                   ? 0
                   : per_shard->shard_stream->get_input_interface()
                         ->get_instruction_ordinal());
        std::lock_guard<std::mutex> guard(input2info_mutex_);
        auto it = input2info_.find(input_id);
        if (it == input2info_.end()) {
            input2info_[input_id] = std::unique_ptr<per_input_t>(new per_input_t);
            it = input2info_.find(input_id);
        }
        // It would be nice to assert that this pointer is not in use in other shards
        // but that is too expensive.
        per_shard->per_input = it->second.get();
        // Not supposed to see a switch that splits an encoding from its instr.
        // That would cause recording an incorrect encoding into pc2encoding.
        if (!per_shard->last_encoding.empty()) {
            per_shard->error = "Input switch immediately after encoding not supported";
            return false;
        }
    }
    if (per_shard->enabled && stop_timestamp_ != 0 &&
        per_shard->shard_stream->get_last_timestamp() >= stop_timestamp_) {
        per_shard->enabled = false;
        trace_entry_t filter_boundary_entry = { TRACE_TYPE_MARKER,
                                                TRACE_MARKER_TYPE_FILTER_ENDPOINT,
                                                { 0 } };
        if (!write_trace_entry(per_shard, filter_boundary_entry)) {
            per_shard->error = "Failed to write";
            return false;
        }
    }
    if (per_shard->enabled) {
        for (int i = 0; i < static_cast<int>(filters_.size()); ++i) {
            if (!filters_[i]->parallel_shard_filter(entry,
                                                    per_shard->filter_shard_data[i],
                                                    per_shard->record_filter_info)) {
                output = false;
            }
            if (!filters_[i]->get_error_string().empty()) {
                per_shard->error = "Filter error: " + filters_[i]->get_error_string();
                return false;
            }
        }
    }

    if (per_shard->archive_writer) {
        // Wait until we reach the next instr or timestamp past the threshold to
        // insert the new chunk, to ensure we get all associated records with the
        // chunk-final instr.
        VPRINT(this, 4, "Cur chunk instr count: %" PRIu64 " vs threshold %" PRIu64 "\n",
               per_shard->cur_chunk_instrs, per_shard->chunk_size);
        if (per_shard->cur_chunk_instrs >= per_shard->chunk_size &&
            per_shard->chunk_size > 0 &&
            (is_any_instr_type(static_cast<trace_type_t>(entry.type)) ||
             (entry.type == TRACE_TYPE_MARKER &&
              entry.size == TRACE_MARKER_TYPE_TIMESTAMP) ||
             entry.type == TRACE_TYPE_THREAD_EXIT || entry.type == TRACE_TYPE_FOOTER)) {
            std::string error = open_new_chunk(per_shard);
            if (!error.empty()) {
                per_shard->error = error;
                return false;
            }
        }
    }

    per_shard->error = process_markers(per_shard, entry, output);
    if (!per_shard->error.empty())
        return false;

    per_shard->error = process_chunk_encodings(per_shard, entry, output);
    if (!per_shard->error.empty())
        return false;

    if (output && type_is_instr(static_cast<trace_type_t>(entry.type)) &&
        // Do not count PC-only i-filtered instrs.
        entry.size > 0)
        ++per_shard->cur_chunk_instrs;

    per_shard->error = process_delayed_encodings(per_shard, entry, output);
    if (!per_shard->error.empty())
        return false;

    per_shard->prev_was_output = output;

    if (entry.type == TRACE_TYPE_ENCODING) {
        // Delay output until we know whether its instr will be output.
        VPRINT(this, 4, "@%" PRIu64 " remembering last encoding %d %d 0x%zx\n",
               per_shard->input_entry_count, entry.type, entry.size, entry.addr);
        per_shard->last_encoding.push_back(entry);
        output = false;
    } else if (is_any_instr_type(static_cast<trace_type_t>(entry.type))) {
        per_shard->last_encoding.clear();
    }

    per_shard->prev_input_id = per_shard->shard_stream->get_input_id();
    per_shard->prev_workload_id = per_shard->shard_stream->get_workload_id();

    if (output) {
        // XXX i#5675: Currently we support writing to a single output file, but we may
        // want to write to multiple in the same run; e.g. splitting a trace. For now,
        // we can simply run the tool multiple times, but it can be made more efficient.
        if (!write_trace_entry(per_shard, entry)) {
            per_shard->error = "Failed to write";
            return false;
        }
    }

    return true;
}

bool
record_filter_t::process_memref(const trace_entry_t &memref)
{
    // XXX i#5675: Serial analysis is not yet supported. Each shard is processed
    // independently of the others. A cache filter may want to use a global cache.
    return false;
}

std::string
record_filter_t::open_serial_schedule_file()
{
    if (serial_schedule_ostream_ != nullptr)
        return "Already opened";
    if (output_dir_.empty())
        return "No output directory specified";
    std::string path = output_dir_ + DIRSEP + DRMEMTRACE_SERIAL_SCHEDULE_FILENAME;
#ifdef HAS_ZLIB
    path += ".gz";
    serial_schedule_file_ = std::unique_ptr<std::ostream>(new gzip_ostream_t(path));
#else
    serial_schedule_file_ =
        std::unique_ptr<std::ostream>(new std::ofstream(path, std::ofstream::binary));
#endif
    if (!serial_schedule_file_)
        return "Failed to open serial schedule file " + path;
    serial_schedule_ostream_ = serial_schedule_file_.get();
    return "";
}

std::string
record_filter_t::open_cpu_schedule_file()
{
    if (cpu_schedule_ostream_ != nullptr)
        return "Already opened";
    if (output_dir_.empty())
        return "No output directory specified";
    std::string path = output_dir_ + DIRSEP + DRMEMTRACE_CPU_SCHEDULE_FILENAME;
#ifdef HAS_ZIP
    cpu_schedule_file_ = std::unique_ptr<archive_ostream_t>(new zipfile_ostream_t(path));
    if (!cpu_schedule_file_)
        return "Failed to open cpu schedule file " + path;
    cpu_schedule_ostream_ = cpu_schedule_file_.get();
    return "";
#else
    return "Zipfile support is required for cpu schedule files";
#endif
}

std::string
record_filter_t::write_schedule_files()
{

    schedule_file_t sched;
    std::string err;
    err = open_serial_schedule_file();
    if (!err.empty())
        return err;
    err = open_cpu_schedule_file();
    if (!err.empty()) {
#ifdef HAS_ZIP
        return err;
#else
        if (starts_with(err, "Zipfile support")) {
            // Just skip the cpu file.
        } else {
            return err;
        }
#endif
    }
    for (const auto &shard : shard_map_) {
        err = sched.merge_shard_data(shard.second->sched_info);
        if (!err.empty())
            return err;
    }
    if (serial_schedule_ostream_ == nullptr)
        return "Serial file not opened";
    err = sched.write_serial_file(serial_schedule_ostream_);
    if (!err.empty())
        return err;
    // Make the cpu file optional for !HAS_ZIP, but don't wrap this inside
    // HAS_ZIP as some subclasses have non-minizip zip support and don't have
    // that define.
    if (cpu_schedule_ostream_ != nullptr) {
        err = sched.write_cpu_file(cpu_schedule_ostream_);
        if (!err.empty())
            return err;
    }
    return "";
}

bool
record_filter_t::print_results()
{
    bool res = true;
    uint64_t input_entry_count = 0;
    uint64_t output_entry_count = 0;
    for (const auto &shard : shard_map_) {
        input_entry_count += shard.second->input_entry_count;
        if (shard.second->now_empty) {
            error_string_ = remove_output_file(shard.second);
            if (!error_string_.empty())
                res = false;
        } else
            output_entry_count += shard.second->output_entry_count;
    }
    std::cerr << "Output " << output_entry_count << " entries from " << input_entry_count
              << " entries.\n";
    if (output_dir_.empty()) {
        std::cerr << "Not writing schedule files: no output directory was specified.\n";
        return res;
    }
    error_string_ = write_schedule_files();
    if (!error_string_.empty())
        res = false;
    return res;
}

} // namespace drmemtrace
} // namespace dynamorio
