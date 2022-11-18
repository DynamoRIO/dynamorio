/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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

#include <iostream>
#include <fstream>
#include <memory>
#include <vector>

#ifdef HAS_ZLIB
#    include "common/gzip_ostream.h"
#endif
#include "record_filter.h"

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

record_filter_t::record_filter_t(const std::string &output_dir,
                                 const std::vector<record_filter_func_t *> &filters,
                                 uint64_t stop_timestamp, unsigned int verbose)
    : output_dir_(output_dir)
    , filters_(filters)
    , stop_timestamp_(stop_timestamp)
    , verbosity_(verbose)
    , input_entry_count_(0)
    , output_entry_count_(0)
{
    UNUSED(verbosity_);
    UNUSED(output_prefix_);
}

record_filter_t::~record_filter_t()
{
}

bool
record_filter_t::parallel_shard_supported()
{
    return true;
}

std::unique_ptr<std::ostream>
record_filter_t::get_writer(per_shard_t *per_shard, memtrace_stream_t *shard_stream)
{
    per_shard->output_path = output_dir_ + DIRSEP + shard_stream->get_stream_name();
#ifdef HAS_ZLIB
    if (ends_with(per_shard->output_path, ".gz")) {
        VPRINT(this, 3, "Using the gzip writer for %s\n", per_shard->output_path.c_str());
        return std::unique_ptr<std::ostream>(new gzip_ostream_t(per_shard->output_path));
    }
#endif
    VPRINT(this, 3, "Using the default writer for %s\n", per_shard->output_path.c_str());
    return std::unique_ptr<std::ostream>(
        new std::ofstream(per_shard->output_path, std::ofstream::binary));
}

void *
record_filter_t::parallel_shard_init_stream(int shard_index, void *worker_data,
                                            memtrace_stream_t *shard_stream)
{
    auto per_shard = new per_shard_t;
    per_shard->writer = get_writer(per_shard, shard_stream);
    per_shard->shard_stream = shard_stream;
    per_shard->enabled = true;
    if (!per_shard->writer) {
        per_shard->error = "Could not open a writer for " + per_shard->output_path;
        success_ = false;
    }
    for (record_filter_func_t *f : filters_) {
        per_shard->filter_shard_data.push_back(f->parallel_shard_init(shard_stream));
        if (f->get_error_string() != "") {
            per_shard->error =
                "Failure in initializing filter function " + f->get_error_string();
            success_ = false;
        }
    }
    return reinterpret_cast<void *>(per_shard);
}

bool
record_filter_t::parallel_shard_exit(void *shard_data)
{
    per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);
    input_entry_count_ += per_shard->input_entry_count;
    output_entry_count_ += per_shard->output_entry_count;
    bool res = true;
    for (int i = 0; i < static_cast<int>(filters_.size()); ++i) {
        if (!filters_[i]->parallel_shard_exit(per_shard->filter_shard_data[i]))
            res = false;
    }
    delete per_shard;
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
    ++shard->output_entry_count;
    return true;
}

bool
record_filter_t::parallel_shard_memref(void *shard_data, const trace_entry_t &entry)
{
    per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);
    ++per_shard->input_entry_count;
    bool output = true;
    if (per_shard->enabled && stop_timestamp_ != 0 &&
        per_shard->shard_stream->get_last_timestamp() >= stop_timestamp_) {
        per_shard->enabled = false;
        trace_entry_t filter_boundary_entry = { TRACE_TYPE_MARKER,
                                                TRACE_MARKER_TYPE_FILTER_BOUNDARY,
                                                { 0 } };
        if (!write_trace_entry(per_shard, filter_boundary_entry))
            return false;
    }
    if (per_shard->enabled) {
        for (int i = 0; i < static_cast<int>(filters_.size()); ++i) {
            if (!filters_[i]->parallel_shard_filter(entry,
                                                    per_shard->filter_shard_data[i]))
                output = false;
        }
    }

    // Optimize space by outputting the unit header only if we are outputting something
    // from that unit.
    if (entry.type == TRACE_TYPE_MARKER) {
        switch (entry.size) {
        case TRACE_MARKER_TYPE_TIMESTAMP:
            // No need to remember the previous unit's header anymore. We're in the
            // next unit now.
            // XXX: it may happen that we never output a unit header due to this
            // optimization. We should ensure that we output it at least once. We
            // skip handling this corner case for now.
            per_shard->last_filtered_unit_header.clear();
            ANNOTATE_FALLTHROUGH;
        case TRACE_MARKER_TYPE_WINDOW_ID:
        case TRACE_MARKER_TYPE_CPU_ID:
            if (output)
                per_shard->last_filtered_unit_header.push_back(entry);
            output = false;
        }
    }
    // Since we're outputting something from this unit, output its unit header.
    if (output && !per_shard->last_filtered_unit_header.empty()) {
        for (trace_entry_t &unit_header_entry : per_shard->last_filtered_unit_header) {
            if (!write_trace_entry(per_shard, unit_header_entry))
                return false;
        }
        per_shard->last_filtered_unit_header.clear();
    }

    // XXX i#5675: Currently we support writing to a single output file, but we may
    // want to write to multiple in the same run; e.g. splitting a trace. For now,
    // we can simply run the tool multiple times, but it can be made more efficient.
    if (output && !write_trace_entry(per_shard, entry))
        return false;
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
    std::cerr << "Output " << output_entry_count_ << " entries from "
              << input_entry_count_ << " entries.\n";
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio
