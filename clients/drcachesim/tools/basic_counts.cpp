/* **********************************************************
 * Copyright (c) 2017-2024 Google, Inc.  All rights reserved.
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

#define NOMINMAX // Avoid windows.h messing up std::max.

#include "basic_counts.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "analysis_tool.h"
#include "memref.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

const std::string basic_counts_t::TOOL_NAME = "Basic counts tool";
const char *const basic_counts_t::TOTAL_COUNT_PREFIX = " total";

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

std::string
basic_counts_t::initialize_stream(memtrace_stream_t *serial_stream)
{
    serial_stream_ = serial_stream;
    return "";
}

std::string
basic_counts_t::initialize_shard_type(shard_type_t shard_type)
{
    shard_type_ = shard_type;
    return "";
}

bool
basic_counts_t::parallel_shard_supported()
{
    return true;
}

void *
basic_counts_t::parallel_shard_init_stream(int shard_index, void *worker_data,
                                           memtrace_stream_t *stream)
{
    auto per_shard = new per_shard_t;
    std::lock_guard<std::mutex> guard(shard_map_mutex_);
    per_shard->stream = stream;
    per_shard->core = stream->get_output_cpuid();
    per_shard->tid = stream->get_tid();
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
    if (memref.instr.tid != INVALID_THREAD_ID &&
        memref.instr.tid != per_shard->last_tid_) {
        counters->unique_threads.insert(memref.instr.tid);
        per_shard->last_tid_ = memref.instr.tid;
    }
    if (type_is_instr(memref.instr.type)) {
        ++counters->instrs;
        if (per_shard->is_kernel) {
            ++counters->kernel_instrs;
        } else {
            ++counters->user_instrs;
        }
        counters->unique_pc_addrs.insert(memref.instr.addr);
        // The encoding entries aren't exposed at the memref_t level, but
        // we use encoding_is_new as a proxy.
        if (TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, per_shard->filetype_) &&
            memref.instr.encoding_is_new)
            ++counters->encodings;
    } else if (memref.instr.type == TRACE_TYPE_INSTR_NO_FETCH) {
        ++counters->instrs_nofetch;
        // The encoding entries aren't exposed at the memref_t level, but
        // we use encoding_is_new as a proxy.
        if (TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, per_shard->filetype_) &&
            memref.instr.encoding_is_new)
            ++counters->encodings;
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
        } else if (memref.marker.marker_type == TRACE_MARKER_TYPE_CORE_WAIT ||
                   memref.marker.marker_type == TRACE_MARKER_TYPE_CORE_IDLE) {
            // This is a synthetic record so do not increment any counts.
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
            case TRACE_MARKER_TYPE_SYSCALL: ++counters->syscall_number_markers; break;
            case TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL:
                ++counters->syscall_blocking_markers;
                break;
            case TRACE_MARKER_TYPE_SYSCALL_TRACE_START:
            case TRACE_MARKER_TYPE_CONTEXT_SWITCH_START:
                per_shard->is_kernel = true;
                break;
            case TRACE_MARKER_TYPE_SYSCALL_TRACE_END:
            case TRACE_MARKER_TYPE_CONTEXT_SWITCH_END:
                per_shard->is_kernel = false;
                break;
            case TRACE_MARKER_TYPE_FILETYPE:
                if (per_shard->filetype_ == -1) {
                    per_shard->filetype_ =
                        static_cast<intptr_t>(memref.marker.marker_value);
                } else if (per_shard->filetype_ !=
                           static_cast<intptr_t>(memref.marker.marker_value)) {
                    error_string_ = std::string("Filetype mismatch");
                    return false;
                }
                ANNOTATE_FALLTHROUGH;
            default: ++counters->other_markers; break;
            }
        }
    } else if (memref.data.type == TRACE_TYPE_THREAD_EXIT) {
        assert(shard_type_ != SHARD_BY_THREAD || per_shard->tid == memref.exit.tid);
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
    int shard_index = serial_stream_->get_shard_index();
    const auto &lookup = shard_map_.find(shard_index);
    if (lookup == shard_map_.end()) {
        per_shard = new per_shard_t;
        per_shard->stream = serial_stream_;
        per_shard->core = serial_stream_->get_output_cpuid();
        per_shard->tid = serial_stream_->get_tid();
        shard_map_[shard_index] = per_shard;
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
basic_counts_t::print_counters(const counters_t &counters, const std::string &prefix,
                               bool for_kernel_trace)
{
    std::cerr << std::setw(12) << counters.instrs << prefix
              << " (fetched) instructions\n";
    if (counters.is_tracking_unique_pc_addrs()) {
        std::cerr << std::setw(12) << counters.unique_pc_addrs.size() << prefix
                  << " unique (fetched) instructions\n";
    }
    std::cerr << std::setw(12) << counters.instrs_nofetch << prefix
              << " non-fetched instructions\n";
    if (for_kernel_trace) {
        std::cerr << std::setw(12) << counters.user_instrs << prefix
                  << " userspace instructions\n";
        std::cerr << std::setw(12) << counters.kernel_instrs << prefix
                  << " kernel instructions\n";
    }
    std::cerr << std::setw(12) << counters.prefetches << prefix << " prefetches\n";
    std::cerr << std::setw(12) << counters.loads << prefix << " data loads\n";
    std::cerr << std::setw(12) << counters.stores << prefix << " data stores\n";
    std::cerr << std::setw(12) << counters.icache_flushes << prefix
              << " icache flushes\n";
    std::cerr << std::setw(12) << counters.dcache_flushes << prefix
              << " dcache flushes\n";
    if (shard_type_ != SHARD_BY_THREAD || counters.unique_threads.size() > 1 ||
        prefix == TOTAL_COUNT_PREFIX) {
        std::cerr << std::setw(12) << counters.unique_threads.size() << prefix
                  << " threads\n";
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
    std::cerr << std::setw(12) << counters.syscall_number_markers << prefix
              << " system call number markers\n";
    std::cerr << std::setw(12) << counters.syscall_blocking_markers << prefix
              << " blocking system call markers\n";
    std::cerr << std::setw(12) << counters.other_markers << prefix << " other markers\n";
    std::cerr << std::setw(12) << counters.encodings << prefix << " encodings\n";
}

bool
basic_counts_t::print_results()
{
    counters_t total;
    uintptr_t num_windows = 1;
    for (const auto &shard : shard_map_) {
        num_windows = std::max(num_windows, shard.second->counters.size());
    }

    bool for_kernel_trace = false;
    for (const auto &shard : shard_map_) {
        for (const auto &ctr : shard.second->counters) {
            total += ctr;
        }
        if (!for_kernel_trace &&
            TESTANY(OFFLINE_FILE_TYPE_KERNEL_SYSCALLS |
                        OFFLINE_FILE_TYPE_KERNEL_SYSCALL_INSTR_ONLY,
                    shard.second->filetype_)) {
            for_kernel_trace = true;
        }
    }
    // Print kernel data if context switches were inserted.
    if (total.kernel_instrs > 0)
        for_kernel_trace = true;
    total.shard_count = shard_map_.size();
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << "Total counts:\n";
    print_counters(total, TOTAL_COUNT_PREFIX, for_kernel_trace);

    if (num_windows > 1) {
        std::cerr << "Total windows: " << num_windows << "\n";
        for (uintptr_t i = 0; i < num_windows; ++i) {
            std::cerr << "Window #" << i << ":\n";
            for (const auto &shard : shard_map_) {
                if (shard.second->counters.size() > i) {
                    print_counters(shard.second->counters[i], " window",
                                   for_kernel_trace);
                }
            }
        }
    }

    // Print the shards sorted by instrs.
    std::vector<std::pair<int, per_shard_t *>> sorted(shard_map_.begin(),
                                                      shard_map_.end());
    std::sort(sorted.begin(), sorted.end(), cmp_threads);
    for (const auto &keyvals : sorted) {
        if (shard_type_ == SHARD_BY_THREAD)
            std::cerr << "Thread " << keyvals.second->tid << " counts:\n";
        else
            std::cerr << "Core " << keyvals.second->core << " counts:\n";
        print_counters(keyvals.second->counters[0], "", for_kernel_trace);
    }

    // TODO i#3599: also print thread-per-window stats.

    return true;
}

basic_counts_t::counters_t
basic_counts_t::get_total_counts()
{
    counters_t total;
    for (const auto &shard : shard_map_) {
        for (const auto &ctr : shard.second->counters) {
            total += ctr;
        }
    }
    total.shard_count = shard_map_.size();
    return total;
}

analysis_tool_t::interval_state_snapshot_t *
basic_counts_t::generate_shard_interval_snapshot(void *shard_data, uint64_t interval_id)
{
    per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);
    count_snapshot_t *snapshot = new count_snapshot_t;
    // Tracking unique pc addresses for each snapshot takes up excessive space.
    snapshot->counters.stop_tracking_unique_pc_addrs();
    for (const auto &ctr : per_shard->counters) {
        snapshot->counters += ctr;
    }
    return snapshot;
}

analysis_tool_t::interval_state_snapshot_t *
basic_counts_t::generate_interval_snapshot(uint64_t interval_id)
{
    count_snapshot_t *snapshot = new count_snapshot_t;
    // Tracking unique pc addresses for each snapshot takes up excessive space.
    snapshot->counters.stop_tracking_unique_pc_addrs();
    for (const auto &shard : shard_map_) {
        for (const auto &ctr : shard.second->counters) {
            snapshot->counters += ctr;
        }
    }
    return snapshot;
}

analysis_tool_t::interval_state_snapshot_t *
basic_counts_t::combine_interval_snapshots(
    const std::vector<const analysis_tool_t::interval_state_snapshot_t *>
        latest_shard_snapshots,
    uint64_t interval_end_timestamp)
{
    count_snapshot_t *result = new count_snapshot_t;
    // The snapshots in latest_shard_snapshots do not track unique_pc_addrs, so
    // the combined result->counters also would not contain any element in the
    // unique_pc_addrs set. But we still need to explicitly disable
    // unique_pc_addrs tracking in result->counters using the following function
    // call. This is so that printing of unique_pc_addrs count is skipped as
    // intended during print_interval_results.
    result->counters.stop_tracking_unique_pc_addrs();
    result->counters.shard_count = 0;
    for (const auto snapshot : latest_shard_snapshots) {
        if (snapshot == nullptr)
            continue;
        result->counters +=
            dynamic_cast<const count_snapshot_t *const>(snapshot)->counters;
        ++result->counters.shard_count;
        assert(result->counters.unique_pc_addrs.empty());
    }
    return result;
}

bool
basic_counts_t::print_interval_results(
    const std::vector<interval_state_snapshot_t *> &interval_snapshots)
{
    std::cerr << "Counts per trace interval for ";
    if (!interval_snapshots.empty() &&
        interval_snapshots[0]->shard_id !=
            interval_state_snapshot_t::WHOLE_TRACE_SHARD_ID) {
        std::cerr << "TID " << interval_snapshots[0]->shard_id << ":\n";
    } else {
        std::cerr << "whole trace:\n";
    }
    counters_t last;
    for (const auto &snapshot_base : interval_snapshots) {
        auto *snapshot = dynamic_cast<count_snapshot_t *>(snapshot_base);
        std::cerr << "Interval #" << snapshot->interval_id << " ending at timestamp "
                  << snapshot->interval_end_timestamp << ":\n";
        counters_t diff = snapshot->counters;
        diff -= last;
        print_counters(diff, " interval delta");
        last = snapshot->counters;
        if (knob_verbose_ > 0) {
            if (snapshot->instr_count_cumulative !=
                static_cast<uint64_t>(snapshot->counters.instrs)) {
                std::stringstream err_stream;
                err_stream << "Cumulative instr count value provided by framework ("
                           << snapshot->instr_count_cumulative
                           << ") not equal to tool value (" << snapshot->counters.instrs
                           << ")\n";
                error_string_ = err_stream.str();
                return false;
            }
            if (snapshot->instr_count_delta != static_cast<uint64_t>(diff.instrs)) {
                std::stringstream err_stream;
                err_stream << "Delta instr count value provided by framework ("
                           << snapshot->instr_count_delta << ") not equal to tool value ("
                           << diff.instrs << ")\n";
                error_string_ = err_stream.str();
                return false;
            }
        }
    }
    return true;
}

bool
basic_counts_t::release_interval_snapshot(
    analysis_tool_t::interval_state_snapshot_t *snapshot)
{
    delete snapshot;
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio
