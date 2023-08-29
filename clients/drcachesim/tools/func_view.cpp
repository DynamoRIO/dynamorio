/* **********************************************************
 * Copyright (c) 2020-2023 Google, Inc.  All rights reserved.
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

/* This trace analyzer presents function call trace information, both
 * sequentially and in summary.  it optionally uses modules.log file
 * to qualify function names for offline traces.
 */

#include "func_view.h"

#include <assert.h>
#include <stdint.h>

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <mutex>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "analysis_tool.h"
#include "memref.h"
#include "raw2trace_directory.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

const std::string func_view_t::TOOL_NAME = "Function view tool";

analysis_tool_t *
func_view_tool_create(const std::string &funclist_file_path, bool full_trace,
                      unsigned int verbose)
{
    return new func_view_t(funclist_file_path, full_trace, verbose);
}

func_view_t::func_view_t(const std::string &funclist_file_path, bool full_trace,
                         unsigned int verbose)
    : knob_full_trace_(full_trace)
    , knob_verbose_(verbose)
    , funclist_file_path_(funclist_file_path)
{
}

std::string
func_view_t::initialize()
{
    std::vector<std::vector<std::string>> entries;
    raw2trace_directory_t directory_;
    std::string error =
        directory_.initialize_funclist_file(funclist_file_path_, &entries);
    if (!error.empty())
        return "Failed to read " + funclist_file_path_ + " " + error;
    for (const auto &entry : entries) {
        if (entry.size() < 4)
            return "Invalid funclist entry: has <4 fields.";
        int id = strtol(entry.front().c_str(), nullptr, 10);
        // If multiple syms have the same id, the args, noret, etc. come from
        // the first one.
        if (id2info_.find(id) != id2info_.end()) {
            id2info_[id].names.insert(entry.back());
            continue;
        }
        traced_info_t info;
        info.names.insert(entry.back());
        info.num_args = strtol(entry[1].c_str(), nullptr, 10);
        for (size_t i = 3; i < entry.size() - 1; ++i) {
            if (entry[i] == "noret")
                info.noret = true;
        }
        id2info_[id] = info;
    }
    return "";
}

func_view_t::~func_view_t()
{
    for (auto &iter : shard_map_) {
        delete iter.second;
    }
}

bool
func_view_t::parallel_shard_supported()
{
    return !knob_full_trace_;
}

void *
func_view_t::parallel_shard_init(int shard_index, void *worker_data)
{
    auto shard_data = new shard_data_t;
    std::lock_guard<std::mutex> guard(shard_map_mutex_);
    shard_map_[shard_index] = shard_data;
    return shard_data;
}

bool
func_view_t::parallel_shard_exit(void *shard_data)
{
    // Nothing (we read the shard data in print_results).
    return true;
}

std::string
func_view_t::parallel_shard_error(void *shard_data)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    return shard->error;
}

void
func_view_t::process_memref_for_markers(void *shard_data, const memref_t &memref)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    if (type_is_instr(memref.instr.type))
        shard->prev_pc = memref.instr.addr;
    if (memref.marker.type != TRACE_TYPE_MARKER)
        return;
    if (memref.marker.marker_type == TRACE_MARKER_TYPE_FUNC_ID) {
        shard->last_was_syscall = memref.marker.marker_value >=
            static_cast<int64_t>(func_trace_t::TRACE_FUNC_ID_SYSCALL_BASE);
    }
    if (shard->last_was_syscall)
        return;
    switch (memref.marker.marker_type) {
    case TRACE_MARKER_TYPE_FUNC_ID:
        if (shard->last_func_id != -1)
            shard->prev_noret = id2info_[shard->last_func_id].noret;
        shard->last_func_id = static_cast<int>(memref.marker.marker_value);
        break;
    case TRACE_MARKER_TYPE_FUNC_RETADDR:
        assert(shard->last_func_id != -1);
        ++shard->func_map[shard->last_func_id].num_calls;
        break;
    case TRACE_MARKER_TYPE_FUNC_RETVAL:
        assert(shard->last_func_id != -1);
        ++shard->func_map[shard->last_func_id].num_returns;
        break;
    default: break;
    }
}

bool
func_view_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    if (memref.data.type == TRACE_TYPE_THREAD_EXIT)
        shard->tid = memref.exit.tid;
    if (memref.marker.type != TRACE_TYPE_MARKER)
        return true;
    process_memref_for_markers(shard, memref);
    return true;
}

bool
func_view_t::process_memref(const memref_t &memref)
{
    shard_data_t *shard;
    const auto &lookup = shard_map_.find(memref.data.tid);
    if (lookup == shard_map_.end()) {
        shard = new shard_data_t;
        shard_map_[memref.data.tid] = shard;
    } else
        shard = lookup->second;
    process_memref_for_markers(shard, memref);
    if (!knob_full_trace_)
        return true;
    if (memref.data.type == TRACE_TYPE_THREAD_EXIT && shard->prev_was_arg) {
        if (shard->prev_noret)
            std::cerr << ")\n";
        else
            std::cerr << ") <no return>\n";
    }
    if (memref.marker.type != TRACE_TYPE_MARKER || shard->last_was_syscall)
        return true;
    switch (memref.marker.marker_type) {
    case TRACE_MARKER_TYPE_FUNC_RETADDR: {
        assert(shard->last_func_id != -1);
        const auto &info = id2info_[shard->last_func_id];
        bool was_nested = shard->nesting_level > 0;
        if (shard->prev_noret)
            --shard->nesting_level;
        std::string indent(shard->nesting_level * 4, ' ');
        // Print a "Tnnn" prefix so threads can be distinguished.
        std::cerr << ((was_nested && shard->prev_was_arg) ? "\n" : "") << "T" << std::dec
                  << std::left << std::setw(8) << memref.marker.tid
                  << std::right /*restore*/;
        std::cerr << indent << "0x" << std::hex << memref.marker.marker_value << " => "
                  << *id2info_[shard->last_func_id].names.begin() << "(";
        if (info.num_args == 0)
            std::cerr << ")";
        ++shard->nesting_level;
        shard->arg_idx = 0;
        if (info.num_args == 0)
            shard->prev_was_arg = true;
        break;
    }
    case TRACE_MARKER_TYPE_FUNC_ARG: {
        const auto &info = id2info_[shard->last_func_id];
        std::cerr << (shard->arg_idx > 0 ? ", " : "") << std::hex << "0x"
                  << memref.marker.marker_value;
        ++shard->arg_idx;
        shard->prev_was_arg = true;
        if (shard->arg_idx == info.num_args) {
            std::cerr << ")" << (info.noret ? "\n" : "");
            if (info.noret)
                shard->prev_was_arg = false;
        }
        break;
    }
    case TRACE_MARKER_TYPE_FUNC_RETVAL: {
        --shard->nesting_level;
        std::string indent;
        if (!shard->prev_was_arg) {
            std::cerr
                << "T" << std::dec << std::left << std::setw(8) << memref.marker.tid
                << std::right /*restore*/ << std::string(shard->nesting_level * 4, ' ');
        }
        std::cerr << (shard->prev_was_arg ? " =>" : "=>") << std::hex << " 0x"
                  << memref.marker.marker_value << "\n";
        shard->prev_was_arg = false;
        break;
    }
    default: break;
    }
    // Reset the i/o format for subsequent tool invocations.
    std::cerr << std::dec;
    return true;
}

bool
func_view_t::cmp_func_stats(const std::pair<int, func_stats_t> &l,
                            const std::pair<int, func_stats_t> &r)
{
    if (l.second.num_calls > r.second.num_calls)
        return true;
    if (l.second.num_calls < r.second.num_calls)
        return false;
    if (l.second.num_returns > r.second.num_returns)
        return true;
    if (l.second.num_returns < r.second.num_returns)
        return false;
    return l.first < r.first;
}

std::unordered_map<int, func_view_t::func_stats_t>
func_view_t::compute_totals()
{
    std::unordered_map<int, func_stats_t> func_totals;
    // Initialize all to 0 to include everything, even those we never saw.
    for (const auto &keyval : id2info_) {
        func_totals[keyval.first] = func_stats_t();
    }
    for (const auto &shard : shard_map_) {
        for (const auto &keyval : shard.second->func_map) {
            func_totals[keyval.first] += keyval.second;
        }
    }
    return func_totals;
}

bool
func_view_t::print_results()
{
    std::unordered_map<int, func_stats_t> func_totals = compute_totals();
    std::cerr << TOOL_NAME << " results:\n";
    if (func_totals.empty()) {
        std::cerr << "No functions founds.  Did you enable function tracing?:\n";
    }
    std::vector<std::pair<int, func_stats_t>> sorted(func_totals.size());
    std::partial_sort_copy(func_totals.begin(), func_totals.end(), sorted.begin(),
                           sorted.end(), cmp_func_stats);
    for (const auto &entry : sorted) {
        std::cerr << "Function id=" << entry.first << ": ";
        for (auto it = id2info_[entry.first].names.begin();
             it != id2info_[entry.first].names.end(); ++it) {
            std::cerr << *it;
            if (std::next(it) != id2info_[entry.first].names.end())
                std::cerr << ", ";
        }
        std::cerr << "\n";
        std::cerr << std::setw(9) << entry.second.num_calls << " calls\n";
        std::cerr << std::setw(9) << entry.second.num_returns << " returns\n";
    }
    // XXX: Should we print out a per-thread breakdown?
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio
