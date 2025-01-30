/* **********************************************************
 * Copyright (c) 2017-2025 Google, Inc.  All rights reserved.
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

/* This trace analyzer requires access to the modules.log file and the
 * libraries and binary from the traced execution in order to obtain further
 * information about each instruction than was stored in the trace.
 * It does not support online use, only offline.
 */

#include "opcode_mix.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "analysis_tool.h"
#include "dr_api.h"
#include "memref.h"
#include "raw2trace.h"
#include "raw2trace_shared.h"
#include "reader.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

const std::string opcode_mix_t::TOOL_NAME = "Opcode mix tool";

analysis_tool_t *
opcode_mix_tool_create(const std::string &module_file_path, unsigned int verbose,
                       const std::string &alt_module_dir)
{
    return new opcode_mix_t(module_file_path, verbose, alt_module_dir);
}

opcode_mix_t::opcode_mix_t(const std::string &module_file_path, unsigned int verbose,
                           const std::string &alt_module_dir)
    : module_file_path_(module_file_path)
    , knob_verbose_(verbose)
    , knob_alt_module_dir_(alt_module_dir)
{
}

bool
opcode_mix_t::init_decode_cache(shard_data_t *shard, void *dcontext,
                                offline_file_type_t filetype)
{
    shard->decode_cache =
        std::unique_ptr<decode_cache_t<opcode_data_t>>(new decode_cache_t<opcode_data_t>(
            dcontext,
            /*include_decoded_instr=*/true,
            /*persist_decoded_instrs=*/false, knob_verbose_));
    if (!TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, filetype)) {
        shard->error =
            shard->decode_cache->init(filetype, module_file_path_, knob_alt_module_dir_);
    } else {
        shard->error = shard->decode_cache->init(filetype);
    }
    return shard->error.empty();
}

std::string
opcode_mix_t::initialize_stream(dynamorio::drmemtrace::memtrace_stream_t *serial_stream)
{
    dcontext_.dcontext = dr_standalone_init();
    if (serial_stream != nullptr) {
        serial_shard_.stream = serial_stream;
    }
    return "";
}

opcode_mix_t::~opcode_mix_t()
{
    for (auto &iter : shard_map_) {
        delete iter.second;
    }
}

bool
opcode_mix_t::parallel_shard_supported()
{
    return true;
}

void *
opcode_mix_t::parallel_shard_init_stream(
    int shard_index, void *worker_data,
    dynamorio::drmemtrace::memtrace_stream_t *shard_stream)
{
    auto shard = new shard_data_t();
    shard->stream = shard_stream;
    std::lock_guard<std::mutex> guard(shard_map_mutex_);
    shard_map_[shard_index] = shard;
    return reinterpret_cast<void *>(shard);
}

bool
opcode_mix_t::parallel_shard_exit(void *shard_data)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    if (shard->decode_cache != nullptr)
        shard->decode_cache->clear_cache();
    // We still need the remaining shard data in print_results.
    return true;
}

bool
opcode_mix_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_VECTOR_LENGTH) {
#ifdef AARCH64
        const int new_vl_bits = memref.marker.marker_value * 8;
        if (dr_get_vector_length() != new_vl_bits) {
            dr_set_vector_length(new_vl_bits);
            // Changing the vector length can change the IR representation of some SVE
            // instructions but it will never change the opcode so we don't need to
            // flush the opcode cache.
        }
#endif
    }
    if (!type_is_instr(memref.instr.type) &&
        memref.data.type != TRACE_TYPE_INSTR_NO_FETCH) {
        return true;
    }

    /* At this point we start processing memref instructions, so we're past the header of
     * the trace, which contains the trace filetype. If -skip_instrs was used, we may not
     * have seen TRACE_MARKER_TYPE_FILETYPE itself but the memtrace_stream_t should be
     * able to provide it to us.
     */
    shard->filetype = static_cast<offline_file_type_t>(shard->stream->get_filetype());

    /* We expected the value of TRACE_MARKER_TYPE_FILETYPE to contain at least one of the
     * enum values of offline_file_type_t (e.g., OFFLINE_FILE_TYPE_ARCH_), so
     * OFFLINE_FILE_TYPE_DEFAULT (== 0) should never be present alone and can be used as
     * uninitialized value of filetype for an error check.
     * XXX i#6812: we could allow traces that have some shards with no filetype, as long
     * as there is at least one shard with it, by caching the filetype from shards that
     * have it and using that one. We can do this using memtrace_stream_t::get_filetype(),
     */
    if (shard->filetype == OFFLINE_FILE_TYPE_DEFAULT) {
        shard->error = "No file type found in this shard";
        return false;
    } else if (shard->decode_cache == nullptr &&
               !init_decode_cache(shard, dcontext_.dcontext, shard->filetype)) {
        return false;
    }

    ++shard->instr_count;
    opcode_data_t *opcode_data;
    shard->error = shard->decode_cache->add_decode_info(memref.instr, opcode_data);
    if (!shard->error.empty()) {
        return false;
    }
    // The opcode_data here will never be nullptr since we return
    // early if the prior add_decode_info returned an error.
    ++shard->opcode_counts[opcode_data->opcode_];
    ++shard->category_counts[opcode_data->category_];
    return true;
}

std::string
opcode_mix_t::parallel_shard_error(void *shard_data)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    return shard->error;
}

bool
opcode_mix_t::process_memref(const memref_t &memref)
{
    if (!parallel_shard_memref(reinterpret_cast<void *>(&serial_shard_), memref)) {
        error_string_ = serial_shard_.error;
        return false;
    }
    return true;
}

static bool
cmp_val(const std::pair<int, int64_t> &l, const std::pair<int, int64_t> &r)
{
    return (l.second > r.second) || (l.second == r.second && l.first < r.first);
}

std::string
opcode_mix_t::get_category_names(uint category)
{
    std::string category_name;
    if (category == DR_INSTR_CATEGORY_UNCATEGORIZED) {
        category_name += instr_get_category_name(DR_INSTR_CATEGORY_UNCATEGORIZED);
        return category_name;
    }

    const uint max_mask = 0x80000000;
    for (uint mask = 0x1; mask <= max_mask; mask <<= 1) {
        if (TESTANY(mask, category)) {
            if (category_name.length() > 0) {
                category_name += " ";
            }
            category_name +=
                instr_get_category_name(static_cast<dr_instr_category_t>(mask));
        }

        /*
         * Guard against 32 bit overflow.
         */
        if (mask == max_mask) {
            break;
        }
    }

    return category_name;
}

bool
opcode_mix_t::print_results()
{
    shard_data_t aggregated;
    shard_data_t *total = &aggregated;
    if (shard_map_.empty()) {
        // No default copy constructor for shard_data_t because of the
        // std::unique_ptr, so we resort to keeping a pointer to it.
        total = &serial_shard_;
    } else {
        for (const auto &shard : shard_map_) {
            aggregated.instr_count += shard.second->instr_count;
            for (const auto &keyvals : shard.second->opcode_counts) {
                aggregated.opcode_counts[keyvals.first] += keyvals.second;
            }
            for (const auto &keyvals : shard.second->category_counts) {
                aggregated.category_counts[keyvals.first] += keyvals.second;
            }
        }
    }
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << std::setw(15) << total->instr_count
              << " : total executed instructions\n";
    std::vector<std::pair<int, int64_t>> sorted(total->opcode_counts.begin(),
                                                total->opcode_counts.end());
    std::sort(sorted.begin(), sorted.end(), cmp_val);
    for (const auto &keyvals : sorted) {
        std::cerr << std::setw(15) << keyvals.second << " : " << std::setw(9)
                  << decode_opcode_name(keyvals.first) << "\n";
    }
    std::cerr << "\n";
    std::cerr << std::setw(15) << total->category_counts.size()
              << " : sets of categories\n";
    std::vector<std::pair<uint, int64_t>> sorted_category_counts(
        total->category_counts.begin(), total->category_counts.end());
    std::sort(sorted_category_counts.begin(), sorted_category_counts.end(), cmp_val);
    for (const auto &keyvals : sorted_category_counts) {
        std::cerr << std::setw(15) << keyvals.second << " : " << std::setw(9)
                  << get_category_names(keyvals.first) << "\n";
    }

    return true;
}

opcode_mix_t::interval_state_snapshot_t *
opcode_mix_t::generate_interval_snapshot(uint64_t interval_id)
{
    return generate_shard_interval_snapshot(&serial_shard_, interval_id);
}

opcode_mix_t::interval_state_snapshot_t *
opcode_mix_t::generate_shard_interval_snapshot(void *shard_data, uint64_t interval_id)
{
    assert(shard_data != nullptr);
    auto &shard = *reinterpret_cast<shard_data_t *>(shard_data);
    auto *snap = new snapshot_t;
    snap->opcode_counts_ = shard.opcode_counts;
    snap->category_counts_ = shard.category_counts;
    return snap;
}

bool
opcode_mix_t::finalize_interval_snapshots(
    std::vector<interval_state_snapshot_t *> &interval_snapshots)
{
    // Loop through snapshots in reverse order, subtracting the *earlier*
    // snapshot's cumulative values from this snapshot's cumulative values, to get
    // deltas.  The first snapshot needs no updates, obviously.
    for (int i = static_cast<int>(interval_snapshots.size()) - 1; i > 0; --i) {
        auto &this_snap = *reinterpret_cast<snapshot_t *>(interval_snapshots[i]);
        auto &prior_snap = *reinterpret_cast<snapshot_t *>(interval_snapshots[i - 1]);
        for (auto &opc_count : this_snap.opcode_counts_) {
            opc_count.second -= prior_snap.opcode_counts_[opc_count.first];
        }
        for (auto &cat_count : this_snap.category_counts_) {
            cat_count.second -= prior_snap.category_counts_[cat_count.first];
        }
    }
    return true;
}

opcode_mix_t::interval_state_snapshot_t *
opcode_mix_t::combine_interval_snapshots(
    const std::vector<const interval_state_snapshot_t *> latest_shard_snapshots,
    uint64_t interval_end_timestamp)
{
    snapshot_t *super_snap = new snapshot_t;
    for (const interval_state_snapshot_t *base_snap : latest_shard_snapshots) {
        const auto *snap = reinterpret_cast<const snapshot_t *>(base_snap);
        // Skip nullptrs and snapshots from different intervals.
        if (snap == nullptr ||
            snap->get_interval_end_timestamp() != interval_end_timestamp) {
            continue;
        }
        for (const auto opc_count : snap->opcode_counts_) {
            super_snap->opcode_counts_[opc_count.first] += opc_count.second;
        }
        for (const auto cat_count : snap->category_counts_) {
            super_snap->category_counts_[cat_count.first] += cat_count.second;
        }
    }
    return super_snap;
}

bool
opcode_mix_t::print_interval_results(
    const std::vector<interval_state_snapshot_t *> &interval_snapshots)
{
    // Number of opcodes and categories to print per interval.
    constexpr int PRINT_TOP_N = 3;
    std::cerr << "There were " << interval_snapshots.size() << " intervals created.\n";
    for (auto *base_snap : interval_snapshots) {
        const auto *snap = reinterpret_cast<const snapshot_t *>(base_snap);
        std::cerr << "ID:" << snap->get_interval_id() << " ending at instruction "
                  << snap->get_instr_count_cumulative() << " has "
                  << snap->opcode_counts_.size() << " opcodes"
                  << " and " << snap->category_counts_.size() << " categories.\n";
        std::vector<std::pair<int, int64_t>> sorted(snap->opcode_counts_.begin(),
                                                    snap->opcode_counts_.end());
        std::sort(sorted.begin(), sorted.end(), cmp_val);
        for (int i = 0; i < PRINT_TOP_N && i < static_cast<int>(sorted.size()); ++i) {
            std::cerr << "   [" << i + 1 << "]"
                      << " Opcode: " << decode_opcode_name(sorted[i].first) << " ("
                      << sorted[i].first << ")"
                      << " Count=" << sorted[i].second << " PKI="
                      << sorted[i].second * 1000.0 / snap->get_instr_count_delta()
                      << "\n";
        }
        std::vector<std::pair<uint, int64_t>> sorted_cats(snap->category_counts_.begin(),
                                                          snap->category_counts_.end());
        std::sort(sorted_cats.begin(), sorted_cats.end(), cmp_val);
        for (int i = 0; i < PRINT_TOP_N && i < static_cast<int>(sorted_cats.size());
             ++i) {
            std::cerr << "   [" << i + 1 << "]"
                      << " Category=" << get_category_names(sorted_cats[i].first)
                      << " Count=" << sorted_cats[i].second << " PKI="
                      << sorted_cats[i].second * 1000.0 / snap->get_instr_count_delta()
                      << "\n";
        }
    }
    return true;
}

bool
opcode_mix_t::release_interval_snapshot(interval_state_snapshot_t *interval_snapshot)
{
    delete interval_snapshot;
    return true;
}

std::string
opcode_mix_t::opcode_data_t::set_decode_info_derived(
    void *dcontext, const dynamorio::drmemtrace::_memref_instr_t &memref_instr,
    instr_t *instr, app_pc decode_pc)
{
    opcode_ = instr_get_opcode(instr);
    category_ = instr_get_category(instr);
    return "";
}

} // namespace drmemtrace
} // namespace dynamorio
