/* **********************************************************
 * Copyright (c) 2017-2022 Google, Inc.  All rights reserved.
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
    auto per_shard = new per_shard_t;
    std::lock_guard<std::mutex> guard(shard_map_mutex_);
    shard_map_[shard_index] = per_shard;
    return reinterpret_cast<void *>(per_shard);
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
    per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);
    return per_shard->error;
}

bool
basic_counts_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);
    counters_t *counters = &per_shard->counters[per_shard->counters.size() - 1];
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
            if (memref.marker.marker_type == TRACE_MARKER_TYPE_WINDOW_ID &&
                static_cast<intptr_t>(memref.marker.marker_value) !=
                    per_shard->last_window) {
                if (per_shard->last_window == -1 && memref.marker.marker_value != 0) {
                    // We assume that a single file with multiple windows always
                    // starts at 0, which is how we distinguish it from a split
                    // file starting at a high window number.  We check this below.
                    per_shard->last_window = memref.marker.marker_value;
                } else if (per_shard->last_window != -1 &&
                           per_shard->counters.size() !=
                               static_cast<size_t>(per_shard->last_window + 1)) {
                    per_shard->error = "Multi-window file must start at 0";
                    return false;
                } else {
                    per_shard->last_window = memref.marker.marker_value;
                    per_shard->counters.resize(per_shard->last_window + 1 /*0-based*/);
                    counters = &per_shard->counters[per_shard->counters.size() - 1];
                }
            }
            switch (memref.marker.marker_type) {
            case TRACE_MARKER_TYPE_FUNC_ID: ++counters->func_id_markers; break;
            case TRACE_MARKER_TYPE_FUNC_RETADDR: ++counters->func_retaddr_markers; break;
            case TRACE_MARKER_TYPE_FUNC_ARG: ++counters->func_arg_markers; break;
            case TRACE_MARKER_TYPE_FUNC_RETVAL: ++counters->func_retval_markers; break;
            case TRACE_MARKER_TYPE_PHYSICAL_ADDRESS: ++counters->phys_addr_markers; break;
            case TRACE_MARKER_TYPE_VIRTUAL_ADDRESS:
                // Counted implicitly as part of phys_addr_markers.
                break;
            case TRACE_MARKER_TYPE_PHYSICAL_ADDRESS_NOT_AVAILABLE:
                ++counters->phys_unavail_markers;
                break;
            default: ++counters->other_markers; break;
            }
        }
    } else if (memref.data.type == TRACE_TYPE_THREAD_EXIT) {
        per_shard->tid = memref.exit.tid;
    } else if (memref.data.type == TRACE_TYPE_INSTR_FLUSH) {
        counters->icache_flushes++;
    } else if (memref.data.type == TRACE_TYPE_DATA_FLUSH) {
        counters->dcache_flushes++;
    }
    return true;
}

bool
basic_counts_t::process_memref(const memref_t &memref)
{
    per_shard_t *per_shard;
    const auto &lookup = shard_map_.find(memref.data.tid);
    if (lookup == shard_map_.end()) {
        per_shard = new per_shard_t;
        shard_map_[memref.data.tid] = per_shard;
    } else
        per_shard = lookup->second;
    if (!parallel_shard_memref(reinterpret_cast<void *>(per_shard), memref)) {
        error_string_ = per_shard->error;
        return false;
    }
    return true;
}

bool
basic_counts_t::cmp_threads(const std::pair<memref_tid_t, per_shard_t *> &l,
                            const std::pair<memref_tid_t, per_shard_t *> &r)
{
    return (l.second->counters[0].instrs > r.second->counters[0].instrs);
}

void
basic_counts_t::print_counters(const counters_t &counters, int_least64_t num_threads,
                               const std::string &prefix)
{
    std::cerr << std::setw(12) << counters.instrs << prefix
              << " (fetched) instructions\n";
    std::cerr << std::setw(12) << counters.unique_pc_addrs.size() << prefix
              << " unique (fetched) instructions\n";
    std::cerr << std::setw(12) << counters.instrs_nofetch << prefix
              << " non-fetched instructions\n";
    std::cerr << std::setw(12) << counters.prefetches << prefix << " prefetches\n";
    std::cerr << std::setw(12) << counters.loads << prefix << " data loads\n";
    std::cerr << std::setw(12) << counters.stores << prefix << " data stores\n";
    std::cerr << std::setw(12) << counters.icache_flushes << prefix
              << " icache flushes\n";
    std::cerr << std::setw(12) << counters.dcache_flushes << prefix
              << " dcache flushes\n";
    if (num_threads > 0) {
        std::cerr << std::setw(12) << num_threads << prefix << " threads\n";
    }
    std::cerr << std::setw(12) << counters.sched_markers << prefix
              << " scheduling markers\n";
    std::cerr << std::setw(12) << counters.xfer_markers << prefix
              << " transfer markers\n";
    std::cerr << std::setw(12) << counters.func_id_markers << prefix
              << " function id markers\n";
    std::cerr << std::setw(12) << counters.func_retaddr_markers << prefix
              << " function return address markers\n";
    std::cerr << std::setw(12) << counters.func_arg_markers << prefix
              << " function argument markers\n";
    std::cerr << std::setw(12) << counters.func_retval_markers << prefix
              << " function return value markers\n";
    std::cerr << std::setw(12) << counters.phys_addr_markers << prefix
              << " physical address + virtual address marker pairs\n";
    std::cerr << std::setw(12) << counters.phys_unavail_markers << prefix
              << " physical address unavailable markers\n";
    std::cerr << std::setw(12) << counters.other_markers << prefix << " other markers\n";
}

bool
basic_counts_t::print_results()
{
    counters_t total;
    uintptr_t num_windows = 1;
    for (const auto &shard : shard_map_) {
        num_windows = std::max(num_windows, shard.second->counters.size());
    }
    for (const auto &shard : shard_map_) {
        for (const auto &ctr : shard.second->counters) {
            total += ctr;
        }
    }
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << "Total counts:\n";
    print_counters(total, shard_map_.size(), " total");

    if (num_windows > 1) {
        std::cerr << "Total windows: " << num_windows << "\n";
        for (uintptr_t i = 0; i < num_windows; ++i) {
            std::cerr << "Window #" << i << ":\n";
            for (const auto &shard : shard_map_) {
                if (shard.second->counters.size() > i) {
                    print_counters(shard.second->counters[i], 0, " window");
                }
            }
        }
    }

    // Print the threads sorted by instrs.
    std::vector<std::pair<memref_tid_t, per_shard_t *>> sorted(shard_map_.begin(),
                                                               shard_map_.end());
    std::sort(sorted.begin(), sorted.end(), cmp_threads);
    for (const auto &keyvals : sorted) {
        std::cerr << "Thread " << keyvals.second->tid << " counts:\n";
        print_counters(keyvals.second->counters[0], 0, "");
    }

    // TODO i#3599: also print thread-per-window stats.

    return true;
}
