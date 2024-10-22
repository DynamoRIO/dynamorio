/* **********************************************************
 * Copyright (c) 2017-2023 Google, Inc.  All rights reserved.
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
#include "raw2trace_directory.h"
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

std::string
opcode_mix_t::initialize()
{
    serial_shard_.worker = &serial_worker_;
    dcontext_.dcontext = dr_standalone_init();
    // The module_file_path is optional and unused for traces with
    // OFFLINE_FILE_TYPE_ENCODINGS.
    if (module_file_path_.empty())
        return "";
    // Legacy trace support where binaries are needed.
    // We do not support non-module code for such traces.
    std::string error = directory_.initialize_module_file(module_file_path_);
    if (!error.empty())
        return "Failed to initialize directory: " + error;
    module_mapper_ =
        module_mapper_t::create(directory_.modfile_bytes_, nullptr, nullptr, nullptr,
                                nullptr, knob_verbose_, knob_alt_module_dir_);
    module_mapper_->get_loaded_modules();
    error = module_mapper_->get_last_error();
    if (!error.empty())
        return "Failed to load binaries: " + error;
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
opcode_mix_t::parallel_worker_init(int worker_index)
{
    auto worker = new worker_data_t;
    return reinterpret_cast<void *>(worker);
}

std::string
opcode_mix_t::parallel_worker_exit(void *worker_data)
{
    worker_data_t *worker = reinterpret_cast<worker_data_t *>(worker_data);
    delete worker;
    return "";
}

void *
opcode_mix_t::parallel_shard_init(int shard_index, void *worker_data)
{
    worker_data_t *worker = reinterpret_cast<worker_data_t *>(worker_data);
    auto shard = new shard_data_t(worker);
    std::lock_guard<std::mutex> guard(shard_map_mutex_);
    shard_map_[shard_index] = shard;
    return reinterpret_cast<void *>(shard);
}

bool
opcode_mix_t::parallel_shard_exit(void *shard_data)
{
    // Nothing (we read the shard data in print_results).
    return true;
}

bool
opcode_mix_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_FILETYPE) {
        shard->filetype = static_cast<offline_file_type_t>(memref.marker.marker_value);
        /* We remove OFFLINE_FILE_TYPE_ARCH_REGDEPS from this check since DR_ISA_REGDEPS
         * is not a real ISA and can coexist with any real architecture.
         */
        if (TESTANY(OFFLINE_FILE_TYPE_ARCH_ALL & ~OFFLINE_FILE_TYPE_ARCH_REGDEPS,
                    memref.marker.marker_value) &&
            !TESTANY(build_target_arch_type(), memref.marker.marker_value)) {
            shard->error = std::string("Architecture mismatch: trace recorded on ") +
                trace_arch_string(static_cast<offline_file_type_t>(
                    memref.marker.marker_value)) +
                " but tool built for " + trace_arch_string(build_target_arch_type());
            return false;
        }
        /* If we are dealing with a regdeps trace, we need to set the dcontext ISA mode
         * to the correct synthetic ISA (i.e., DR_ISA_REGDEPS).
         */
        if (TESTANY(OFFLINE_FILE_TYPE_ARCH_REGDEPS, memref.marker.marker_value)) {
            /* Because isa_mode in dcontext is a global resource, we guard its access to
             * avoid data races (even though this is a benign data race, as all threads
             * are writing the same isa_mode value).
             */
            std::lock_guard<std::mutex> guard(dcontext_mutex_);
            dr_isa_mode_t isa_mode = dr_get_isa_mode(dcontext_.dcontext);
            if (isa_mode != DR_ISA_REGDEPS)
                dr_set_isa_mode(dcontext_.dcontext, DR_ISA_REGDEPS, nullptr);
        }
    } else if (memref.marker.type == TRACE_TYPE_MARKER &&
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
     * the trace, which contains the trace filetype. If we didn't encounter a
     * TRACE_MARKER_TYPE_FILETYPE we return an error and stop processing the trace.
     * We expected the value of TRACE_MARKER_TYPE_FILETYPE to contain at least one of the
     * enum values of offline_file_type_t (e.g., OFFLINE_FILE_TYPE_ARCH_), so
     * OFFLINE_FILE_TYPE_DEFAULT (== 0) should never be present alone and can be used as
     * uninitialized value of filetype for an error check.
     * XXX i#6812: we could allow traces that have some shards with no filetype, as long
     * as there is at least one shard with it, by caching the filetype from shards that
     * have it and using that one. We can do this using memtrace_stream_t::get_filetype(),
     * which requires updating opcode_mix to use parallel_shard_init_stream() rather than
     * the current (and deprecated) parallel_shard_init(). However, we should first decide
     * whether we want to allow traces that have some shards without a filetype.
     */
    if (shard->filetype == OFFLINE_FILE_TYPE_DEFAULT) {
        shard->error = "No file type found in this shard";
        return false;
    }

    ++shard->instr_count;

    app_pc decode_pc;
    const app_pc trace_pc = reinterpret_cast<app_pc>(memref.instr.addr);
    if (TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, shard->filetype)) {
        // The trace has instruction encodings inside it.
        decode_pc = const_cast<app_pc>(memref.instr.encoding);
        if (memref.instr.encoding_is_new) {
            // The code may have changed: invalidate the cache.
            shard->worker->opcode_data_cache.erase(trace_pc);
        }
    } else {
        // Legacy trace support where we need the binaries.
        if (!module_mapper_) {
            shard->error =
                "Module file path is missing and trace has no embedded encodings";
            return false;
        }
        if (trace_pc >= shard->last_trace_module_start &&
            static_cast<size_t>(trace_pc - shard->last_trace_module_start) <
                shard->last_trace_module_size) {
            decode_pc = shard->last_mapped_module_start +
                (trace_pc - shard->last_trace_module_start);
        } else {
            std::lock_guard<std::mutex> guard(mapper_mutex_);
            decode_pc = module_mapper_->find_mapped_trace_bounds(
                trace_pc, &shard->last_mapped_module_start,
                &shard->last_trace_module_size);
            if (!module_mapper_->get_last_error().empty()) {
                shard->last_trace_module_start = nullptr;
                shard->last_trace_module_size = 0;
                shard->error = "Failed to find mapped address for " +
                    to_hex_string(memref.instr.addr) + ": " +
                    module_mapper_->get_last_error();
                return false;
            }
            shard->last_trace_module_start =
                trace_pc - (decode_pc - shard->last_mapped_module_start);
        }
    }
    int opcode;
    uint category;
    auto cached_opcode_category = shard->worker->opcode_data_cache.find(trace_pc);
    if (cached_opcode_category != shard->worker->opcode_data_cache.end()) {
        opcode = cached_opcode_category->second.opcode;
        category = cached_opcode_category->second.category;
    } else {
        instr_t instr;
        instr_init(dcontext_.dcontext, &instr);
        app_pc next_pc =
            decode_from_copy(dcontext_.dcontext, decode_pc, trace_pc, &instr);
        if (next_pc == NULL || !instr_valid(&instr)) {
            instr_free(dcontext_.dcontext, &instr);
            shard->error =
                "Failed to decode instruction " + to_hex_string(memref.instr.addr);
            return false;
        }
        opcode = instr_get_opcode(&instr);
        category = instr_get_category(&instr);
        shard->worker->opcode_data_cache[trace_pc] = opcode_data_t(opcode, category);
        instr_free(dcontext_.dcontext, &instr);
    }
    ++shard->opcode_counts[opcode];
    ++shard->category_counts[category];
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
    shard_data_t total(0);
    if (shard_map_.empty()) {
        total = serial_shard_;
    } else {
        for (const auto &shard : shard_map_) {
            total.instr_count += shard.second->instr_count;
            for (const auto &keyvals : shard.second->opcode_counts) {
                total.opcode_counts[keyvals.first] += keyvals.second;
            }
            for (const auto &keyvals : shard.second->category_counts) {
                total.category_counts[keyvals.first] += keyvals.second;
            }
        }
    }
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << std::setw(15) << total.instr_count << " : total executed instructions\n";
    std::vector<std::pair<int, int64_t>> sorted(total.opcode_counts.begin(),
                                                total.opcode_counts.end());
    std::sort(sorted.begin(), sorted.end(), cmp_val);
    for (const auto &keyvals : sorted) {
        std::cerr << std::setw(15) << keyvals.second << " : " << std::setw(9)
                  << decode_opcode_name(keyvals.first) << "\n";
    }
    std::cerr << "\n";
    std::cerr << std::setw(15) << total.category_counts.size()
              << " : sets of categories\n";
    std::vector<std::pair<uint, int64_t>> sorted_category_counts(
        total.category_counts.begin(), total.category_counts.end());
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

} // namespace drmemtrace
} // namespace dynamorio
