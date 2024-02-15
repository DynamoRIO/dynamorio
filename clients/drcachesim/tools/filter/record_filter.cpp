/* **********************************************************
 * Copyright (c) 2022-2024 Google, Inc.  All rights reserved.
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
bool
is_any_instr_type(trace_type_t type)
{
    return type_is_instr(type) || type == TRACE_TYPE_INSTR_MAYBE_FETCH ||
        type == TRACE_TYPE_INSTR_NO_FETCH;
}
} // namespace

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
record_filter_t::get_writer(per_shard_t *per_shard, memtrace_stream_t *shard_stream)
{
    per_shard->output_path = output_dir_ + DIRSEP + shard_stream->get_stream_name();
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
    stream << TRACE_CHUNK_PREFIX << std::setfill('0') << std::setw(4)
           << shard->chunk_ordinal;
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

void *
record_filter_t::parallel_shard_init_stream(int shard_index, void *worker_data,
                                            memtrace_stream_t *shard_stream)
{
    auto per_shard = new per_shard_t;
    std::string error = get_writer(per_shard, shard_stream);
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
    for (auto &f : filters_) {
        per_shard->filter_shard_data.push_back(
            f->parallel_shard_init(shard_stream, stop_timestamp_ != 0));
        if (f->get_error_string() != "") {
            per_shard->error =
                "Failure in initializing filter function " + f->get_error_string();
            success_ = false;
        }
    }
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
        per_shard->chunk_ordinal <= 1 && per_shard->cur_chunk_instrs == 0) {
        per_shard->error = remove_output_file(per_shard);
        if (!per_shard->error.empty())
            res = false;
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
    if (!shard->writer->write((char *)&entry, sizeof(entry))) {
        shard->error = "Failed to write to output file " + shard->output_path;
        success_ = false;
        return false;
    }
    shard->cur_refs += shard->memref_counter.entry_memref_count(&entry);
    ++shard->output_entry_count;
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
            if (stop_timestamp_ != 0) {
                entry.addr |= OFFLINE_FILE_TYPE_BIMODAL_FILTERED_WARMUP;
            }
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
            break;
        case TRACE_MARKER_TYPE_PHYSICAL_ADDRESS:
        case TRACE_MARKER_TYPE_PHYSICAL_ADDRESS_NOT_AVAILABLE:
            if (!output && per_shard->archive_writer) {
                // These markers need to be repeated across chunks, yet even raw2trace
                // doesn't support this yet: so we bail on it here too.
                return "Removing physical address markers from archive output is not yet "
                       "supported";
            }
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
        per_shard->pc2encoding[entry.addr] = per_shard->last_encoding;
        // Disable the just-delayed encoding output below if this is
        // what used to be a new-chunk encoding.
        if (per_shard->cur_chunk_pcs.find(entry.addr) != per_shard->cur_chunk_pcs.end()) {
            VPRINT(this, 3, "clearing new-chunk last encoding @pc=0x%zx\n", entry.addr);
            per_shard->last_encoding.clear();
        }
    } else {
        // Insert the cached encoding if this is the first instance of this PC
        // (without an encoding) in this chunk, unless the user is removing all encodings.
        // XXX: What if there is a filter removing all encodings but only
        // to the stop point, so a partial remove that does not change
        // the filetype?  For now we do not support that, and we re-add
        // encodings at chunk boundaries regardless.
        if (TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, per_shard->filetype) &&
            per_shard->cur_chunk_pcs.find(entry.addr) == per_shard->cur_chunk_pcs.end()) {
            if (per_shard->pc2encoding.find(entry.addr) == per_shard->pc2encoding.end()) {
                return "Missing encoding for PC " + std::to_string(entry.addr) +
                    " at input entry " + std::to_string(per_shard->input_entry_count);
            }
            VPRINT(this, 3,
                   "output new-chunk encoding chunk=%" PRIu64 " ref=%" PRIu64 "\n",
                   per_shard->chunk_ordinal, per_shard->cur_refs);
            if (!write_trace_entries(per_shard, per_shard->pc2encoding[entry.addr])) {
                return "Failed to write";
            }
        }
    }
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
    } else {
        // Output if we have encodings that haven't yet been output.
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
    per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);
    ++per_shard->input_entry_count;
    trace_entry_t entry = input_entry;
    bool output = true;
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
                                                    per_shard->filter_shard_data[i])) {
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

bool
record_filter_t::print_results()
{
    uint64_t input_entry_count = 0;
    uint64_t output_entry_count = 0;
    for (const auto &shard : shard_map_) {
        input_entry_count += shard.second->input_entry_count;
        output_entry_count += shard.second->output_entry_count;
    }
    std::cerr << "Output " << output_entry_count << " entries from " << input_entry_count
              << " entries.\n";
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio
