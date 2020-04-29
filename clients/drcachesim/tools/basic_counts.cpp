/* **********************************************************
 * Copyright (c) 2017-2019 Google, Inc.  All rights reserved.
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

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>

#include "basic_counts.h"
#include "../common/utils.h"

const std::string basic_counts_t::TOOL_NAME = "Basic counts tool";

analysis_tool_t *
basic_counts_tool_create(unsigned int verbose)
{
    return new basic_counts_t(verbose);
}

basic_counts_t::basic_counts_t(unsigned int verbose)
    : knob_verbose_(verbose)
{
    // Empty.
}

basic_counts_t::~basic_counts_t()
{
    for (auto &iter : shard_map_) {
        delete iter.second;
    }
}

bool
basic_counts_t::parallel_shard_supported()
{
    return true;
}

void *
basic_counts_t::parallel_shard_init(int shard_index, void *worker_data)
{
    auto counters = new counters_t;
    std::lock_guard<std::mutex> guard(shard_map_mutex_);
    shard_map_[shard_index] = counters;
    return reinterpret_cast<void *>(counters);
}

bool
basic_counts_t::parallel_shard_exit(void *shard_data)
{
    // Nothing (we read the shard data in print_results).
    return true;
}

std::string
basic_counts_t::parallel_shard_error(void *shard_data)
{
    counters_t *counters = reinterpret_cast<counters_t *>(shard_data);
    return counters->error;
}

bool
basic_counts_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    counters_t *counters = reinterpret_cast<counters_t *>(shard_data);
    if (type_is_instr(memref.instr.type)) {
        ++counters->instrs;
        counters->unique_pc_addrs.insert(memref.instr.addr);
    } else if (memref.data.type == TRACE_TYPE_INSTR_NO_FETCH) {
        ++counters->instrs_nofetch;
    } else if (type_is_prefetch(memref.data.type)) {
        ++counters->prefetches;
    } else if (memref.data.type == TRACE_TYPE_READ) {
        ++counters->loads;
    } else if (memref.data.type == TRACE_TYPE_WRITE) {
        ++counters->stores;
    } else if (memref.marker.type == TRACE_TYPE_MARKER) {
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP ||
            memref.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID) {
            ++counters->sched_markers;
        } else if (memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT ||
                   memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_XFER) {
            ++counters->xfer_markers;
        } else {
            switch (memref.marker.marker_type) {
            case TRACE_MARKER_TYPE_FUNC_ID: ++counters->func_id_markers; break;
            case TRACE_MARKER_TYPE_FUNC_RETADDR: ++counters->func_retaddr_markers; break;
            case TRACE_MARKER_TYPE_FUNC_ARG: ++counters->func_arg_markers; break;
            case TRACE_MARKER_TYPE_FUNC_RETVAL: ++counters->func_retval_markers; break;
            default: ++counters->other_markers; break;
            }
        }
    } else if (memref.data.type == TRACE_TYPE_THREAD_EXIT) {
        counters->tid = memref.exit.tid;
    }
    return true;
}

bool
basic_counts_t::process_memref(const memref_t &memref)
{
    counters_t *counters;
    const auto &lookup = shard_map_.find(memref.data.tid);
    if (lookup == shard_map_.end()) {
        counters = new counters_t;
        shard_map_[memref.data.tid] = counters;
    } else
        counters = lookup->second;
    if (!parallel_shard_memref(reinterpret_cast<void *>(counters), memref)) {
        error_string_ = counters->error;
        return false;
    }
    return true;
}

bool
basic_counts_t::cmp_counters(const std::pair<memref_tid_t, counters_t *> &l,
                             const std::pair<memref_tid_t, counters_t *> &r)
{
    return (l.second->instrs > r.second->instrs);
}

bool
basic_counts_t::print_results()
{
    counters_t total;
    for (const auto &shard : shard_map_) {
        total += *shard.second;
    }
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << "Total counts:\n";
    std::cerr << std::setw(12) << total.instrs << " total (fetched) instructions\n";
    std::cerr << std::setw(12) << total.unique_pc_addrs.size()
              << " total unique (fetched) instructions\n";
    std::cerr << std::setw(12) << total.instrs_nofetch
              << " total non-fetched instructions\n";
    std::cerr << std::setw(12) << total.prefetches << " total prefetches\n";
    std::cerr << std::setw(12) << total.loads << " total data loads\n";
    std::cerr << std::setw(12) << total.stores << " total data stores\n";
    std::cerr << std::setw(12) << shard_map_.size() << " total threads\n";
    std::cerr << std::setw(12) << total.sched_markers << " total scheduling markers\n";
    std::cerr << std::setw(12) << total.xfer_markers << " total transfer markers\n";
    std::cerr << std::setw(12) << total.func_id_markers << " total function id markers\n";
    std::cerr << std::setw(12) << total.func_retaddr_markers
              << " total function return address markers\n";
    std::cerr << std::setw(12) << total.func_arg_markers
              << " total function argument markers\n";
    std::cerr << std::setw(12) << total.func_retval_markers
              << " total function return value markers\n";
    std::cerr << std::setw(12) << total.other_markers << " total other markers\n";

    // Print the threads sorted by instrs.
    std::vector<std::pair<memref_tid_t, counters_t *>> sorted(shard_map_.begin(),
                                                              shard_map_.end());
    std::sort(sorted.begin(), sorted.end(), cmp_counters);
    for (const auto &keyvals : sorted) {
        std::cerr << "Thread " << keyvals.second->tid << " counts:\n";
        std::cerr << std::setw(12) << keyvals.second->instrs
                  << " (fetched) instructions\n";
        std::cerr << std::setw(12) << keyvals.second->unique_pc_addrs.size()
                  << " unique (fetched) instructions\n";
        std::cerr << std::setw(12) << keyvals.second->instrs_nofetch
                  << " non-fetched instructions\n";
        std::cerr << std::setw(12) << keyvals.second->prefetches << " prefetches\n";
        std::cerr << std::setw(12) << keyvals.second->loads << " data loads\n";
        std::cerr << std::setw(12) << keyvals.second->stores << " data stores\n";
        std::cerr << std::setw(12) << keyvals.second->sched_markers
                  << " scheduling markers\n";
        std::cerr << std::setw(12) << keyvals.second->xfer_markers
                  << " transfer markers\n";
        std::cerr << std::setw(12) << keyvals.second->func_id_markers
                  << " function id markers\n";
        std::cerr << std::setw(12) << keyvals.second->func_retaddr_markers
                  << " function return address markers\n";
        std::cerr << std::setw(12) << keyvals.second->func_arg_markers
                  << " function argument markers\n";
        std::cerr << std::setw(12) << keyvals.second->func_retval_markers
                  << " function return value markers\n";
        std::cerr << std::setw(12) << keyvals.second->other_markers << " other markers\n";
    }
    return true;
}
