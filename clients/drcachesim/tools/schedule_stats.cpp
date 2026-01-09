/* **********************************************************
 * Copyright (c) 2017-2026 Google, Inc.  All rights reserved.
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

#include "schedule_stats.h"

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

const std::string schedule_stats_t::TOOL_NAME = "Schedule stats tool";

analysis_tool_t *
schedule_stats_tool_create(uint64_t print_every, unsigned int verbose)
{
    return new schedule_stats_t(print_every, verbose);
}

schedule_stats_t::schedule_stats_t(uint64_t print_every, unsigned int verbose)
    : knob_print_every_(print_every)
    , knob_verbose_(verbose)
{
    // Empty.
}

schedule_stats_t::~schedule_stats_t()
{
    for (auto &iter : shard_map_) {
        delete iter.second;
    }
}

std::string
schedule_stats_t::initialize_stream(memtrace_stream_t *serial_stream)
{
    serial_stream_ = serial_stream;
    return "";
}

std::string
schedule_stats_t::initialize_shard_type(shard_type_t shard_type)
{
    if (shard_type != SHARD_BY_CORE)
        return "Only core-sharded operation is supported";
    return "";
}

bool
schedule_stats_t::process_memref(const memref_t &memref)
{
    per_shard_t *per_shard;
    const auto &lookup = shard_map_.find(serial_stream_->get_output_cpuid());
    if (lookup == shard_map_.end()) {
        per_shard = new per_shard_t(this);
        per_shard->stream = serial_stream_;
        per_shard->core = serial_stream_->get_output_cpuid();
        per_shard->filetype = static_cast<intptr_t>(serial_stream_->get_filetype());
        per_shard->segment_start_microseconds = get_current_microseconds();
        shard_map_[per_shard->core] = per_shard;
    } else
        per_shard = lookup->second;
    if (!parallel_shard_memref(reinterpret_cast<void *>(per_shard), memref)) {
        error_string_ = per_shard->error;
        return false;
    }
    return true;
}

bool
schedule_stats_t::parallel_shard_supported()
{
    return true;
}

void *
schedule_stats_t::parallel_shard_init_stream(int shard_index, void *worker_data,
                                             memtrace_stream_t *stream)
{
    auto per_shard = new per_shard_t(this);
    std::lock_guard<std::mutex> guard(shard_map_mutex_);
    per_shard->stream = stream;
    per_shard->core = stream->get_output_cpuid();
    per_shard->filetype = static_cast<intptr_t>(stream->get_filetype());
    per_shard->segment_start_microseconds = get_current_microseconds();
    shard_map_[shard_index] = per_shard;
    return reinterpret_cast<void *>(per_shard);
}

bool
schedule_stats_t::parallel_shard_exit(void *shard_data)
{
    // Nothing (we read the shard data in print_results).
    per_shard_t *shard = reinterpret_cast<per_shard_t *>(shard_data);
    if (!update_state_time(shard, shard->cur_state))
        return false;
    return true;
}

void
schedule_stats_t::get_scheduler_stats(memtrace_stream_t *stream, counters_t &counters)
{
    counters.switches_input_to_input =
        static_cast<int64_t>(stream->get_schedule_statistic(
            memtrace_stream_t::SCHED_STAT_SWITCH_INPUT_TO_INPUT));
    counters.switches_input_to_idle = static_cast<int64_t>(stream->get_schedule_statistic(
        memtrace_stream_t::SCHED_STAT_SWITCH_INPUT_TO_IDLE));
    counters.switches_idle_to_input = static_cast<int64_t>(stream->get_schedule_statistic(
        memtrace_stream_t::SCHED_STAT_SWITCH_IDLE_TO_INPUT));
    counters.switches_nop = static_cast<int64_t>(
        stream->get_schedule_statistic(memtrace_stream_t::SCHED_STAT_SWITCH_NOP));
    counters.quantum_preempts = static_cast<int64_t>(
        stream->get_schedule_statistic(memtrace_stream_t::SCHED_STAT_QUANTUM_PREEMPTS));
    counters.migrations = static_cast<int64_t>(
        stream->get_schedule_statistic(memtrace_stream_t::SCHED_STAT_MIGRATIONS));
    counters.steals = static_cast<int64_t>(
        stream->get_schedule_statistic(memtrace_stream_t::SCHED_STAT_RUNQUEUE_STEALS));
    counters.rebalances = static_cast<int64_t>(stream->get_schedule_statistic(
        memtrace_stream_t::SCHED_STAT_RUNQUEUE_REBALANCES));
    counters.at_output_limit = static_cast<int64_t>(
        stream->get_schedule_statistic(memtrace_stream_t::SCHED_STAT_HIT_OUTPUT_LIMIT));
    counters.switch_sequence_injections =
        static_cast<int64_t>(stream->get_schedule_statistic(
            memtrace_stream_t::SCHED_STAT_KERNEL_SWITCH_SEQUENCE_INJECTIONS));
    counters.syscall_sequence_injections =
        static_cast<int64_t>(stream->get_schedule_statistic(
            memtrace_stream_t::SCHED_STAT_KERNEL_SYSCALL_SEQUENCE_INJECTIONS));
}

std::string
schedule_stats_t::parallel_shard_error(void *shard_data)
{
    per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);
    return per_shard->error;
}

uint64_t
schedule_stats_t::get_current_microseconds()
{
    return get_microsecond_timestamp();
}

bool
schedule_stats_t::update_state_time(per_shard_t *shard, state_t state)
{
    uint64_t cur = get_current_microseconds();
    assert(cur >= shard->segment_start_microseconds);
    assert(shard->segment_start_microseconds > 0);
    uint64_t delta = cur - shard->segment_start_microseconds;
    switch (state) {
    case STATE_CPU: shard->counters.cpu_microseconds += delta; break;
    case STATE_IDLE: shard->counters.idle_microseconds += delta; break;
    case STATE_WAIT: shard->counters.wait_microseconds += delta; break;
    default: return false;
    }
    shard->segment_start_microseconds = cur;
    return true;
}

// shard->prev_workload_id and shard->prev_tid are cleared when this is called,
// so we pass in the preserved values so there's no confusion.
void
schedule_stats_t::record_context_switch(per_shard_t *shard, int64_t prev_workload_id,
                                        int64_t prev_tid, int64_t workload_id,
                                        int64_t tid, int64_t input_id, int64_t letter_ord)
{
    bool add_to_counts =
        // Don't count switching from WAIT, or the initial entry on a core.
        !(prev_workload_id == INVALID_WORKLOAD_ID && prev_tid == INVALID_THREAD_ID) &&
        // Don't count both input-to-idle and idle-to-input: we count swap-outs
        // to match "perf".
        !(prev_workload_id == INVALID_WORKLOAD_ID && prev_tid == IDLE_THREAD_ID &&
          workload_id != INVALID_WORKLOAD_ID && tid != INVALID_THREAD_ID);
    uint64_t instr_delta = shard->counters.instrs - shard->switch_start_instrs;

    if (shard->thread_sequence.empty()) {
        std::lock_guard<std::mutex> lock(prev_core_mutex_);
        prev_core_[workload_tid_t(workload_id, tid)] = shard->core;
    } else {
        if (add_to_counts) {
            ++shard->counters.total_switches;
            schedule_record_t record;
            record.workload = prev_workload_id;
            record.tid = prev_tid;
            record.instructions = instr_delta;
            if (shard->saw_syscall || shard->saw_exit) {
                ++shard->counters.voluntary_switches;
                record.voluntary = true;
            }
            if (shard->direct_switch_target != INVALID_THREAD_ID &&
                shard->direct_switch_target == tid) {
                ++shard->counters.direct_switches;
                record.direct = true;
            }
            if (shard->last_syscall_number >= 0) {
                record.syscall_number = shard->last_syscall_number;
                if (shard->stream->get_version() <
                    TRACE_ENTRY_VERSION_FREQUENT_TIMESTAMPS) {
                    // Legacy versions do not have the timestamps to compute latencies.
                } else {
                    assert(shard->pre_syscall_timestamp > 0);
                    // We don't have the post-syscall timestamp for thread exit.
                    if (shard->post_syscall_timestamp > 0) {
                        histogram_interface_t *hist_ptr = find_or_add_histogram(
                            shard->counters.sysnum_switch_latency,
                            shard->last_syscall_number, kSysnumLatencyBinSize);
                        int64_t latency =
                            shard->post_syscall_timestamp - shard->pre_syscall_timestamp;
                        if (knob_verbose_ >= 3) {
                            std::cerr << "sysnum " << shard->last_syscall_number
                                      << " switch latency "
                                      << shard->post_syscall_timestamp << " - "
                                      << shard->pre_syscall_timestamp << " => " << latency
                                      << "\n";
                        }
                        hist_ptr->add(latency);
                        record.syscall_latency = latency;
                    }
                }
                shard->last_syscall_number = -1;
                shard->pre_syscall_timestamp = 0;
                shard->post_syscall_timestamp = 0;
            }
            shard->switch_record.push_back(record);
            shard->counters.instrs_per_switch->add(instr_delta);
            histogram_interface_t *hist_ptr = find_or_add_histogram(
                shard->counters.tid2instrs_per_switch,
                workload_tid_t(prev_workload_id, prev_tid), kSwitchBinSize);
            hist_ptr->add(instr_delta);
        }
        if (knob_verbose_ >= 2) {
            std::cerr << "Core #" << std::setw(2) << shard->core
                      << (add_to_counts ? "" : " (uncounted)") << " switch W"
                      << prev_workload_id << ".T" << prev_tid << " => W" << workload_id
                      << ".T" << tid << " after " << instr_delta << " instrs\n";
        }
        if (tid != INVALID_THREAD_ID && tid != IDLE_THREAD_ID) {
            // Tracking migrations requires a global lock but just once per context
            // switch seems to have negligible performance impact on parallel analysis.
            std::lock_guard<std::mutex> lock(prev_core_mutex_);
            workload_tid_t workload_tid(workload_id, tid);
            auto it = prev_core_.find(workload_tid);
            if (it != prev_core_.end() && it->second != shard->core) {
                ++shard->counters.observed_migrations;
            }
            prev_core_[workload_tid] = shard->core;
        }
        shard->switch_start_instrs = shard->counters.instrs;
    }
    // The idle and wait strings are handled by the caller.
    if (tid != INVALID_THREAD_ID && tid != IDLE_THREAD_ID) {
        // We convert to letters (works best for <=26 inputs but still gives an
        // idea of the behavior for more inputs).
        shard->thread_sequence +=
            THREAD_LETTER_INITIAL_START + static_cast<char>(letter_ord % 26);
        shard->cur_segment_instrs = 0;
    }
    if (knob_verbose_ >= 2) {
        memtrace_stream_t *istream = shard->stream->get_input_interface();
        std::ostringstream line;
        line << "Core #" << std::setw(2) << shard->core << " @" << std::setw(9)
             << shard->stream->get_record_ordinal() << " refs, " << std::setw(9)
             << shard->stream->get_instruction_ordinal() << " instrs: input "
             << std::setw(4) << input_id << " @" << std::setw(9)
             << (istream == nullptr ? 0 : istream->get_record_ordinal()) << " refs, "
             << std::setw(9)
             << (istream == nullptr ? 0 : istream->get_instruction_ordinal())
             << " instrs, time "
             << std::setw(16)
             // TODO i#5843: For time quanta, provide some way to get the
             // latest time and print that here instead of the timestamp?
             << (istream == nullptr ? 0 : istream->get_last_timestamp()) << " == thread "
             << tid << "\n";
        std::cerr << line.str();
    }
}

bool
schedule_stats_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    per_shard_t *shard = reinterpret_cast<per_shard_t *>(shard_data);
    int64_t input_id = shard->stream->get_input_id();
    assert(shard->stream->get_input_interface() != nullptr ||
           (memref.marker.type == TRACE_TYPE_MARKER &&
            memref.marker.marker_type == TRACE_MARKER_TYPE_CORE_IDLE));
    if (knob_verbose_ >= 4) {
        std::ostringstream line;
        memtrace_stream_t *input_stream = shard->stream->get_input_interface();
        line << "Core #" << std::setw(2) << shard->core << " @" << std::setw(9)
             << shard->stream->get_record_ordinal() << " refs, " << std::setw(9)
             << shard->stream->get_instruction_ordinal() << " instrs: input "
             << std::setw(4) << input_id << " @"
             << std::setw(9)
             // The interface is null when we see idle records.
             << (input_stream == nullptr ? -1 : input_stream->get_record_ordinal())
             << " refs, " << std::setw(9)
             << (input_stream == nullptr ? -1 : input_stream->get_instruction_ordinal())
             << " instrs: " << std::setw(16) << trace_type_names[memref.marker.type];
        if (type_is_instr(memref.instr.type))
            line << " pc=" << std::hex << memref.instr.addr << std::dec;
        else if (memref.marker.type == TRACE_TYPE_MARKER) {
            line << " " << memref.marker.marker_type
                 << " val=" << memref.marker.marker_value;
        }
        line << "\n";
        std::cerr << line.str();
    }
    state_t prev_state = shard->cur_state;
    int64_t tid = INVALID_THREAD_ID;
    int64_t workload_id = INVALID_WORKLOAD_ID;
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_CORE_WAIT)
        shard->cur_state = STATE_WAIT;
    else if (memref.marker.type == TRACE_TYPE_MARKER &&
             memref.marker.marker_type == TRACE_MARKER_TYPE_CORE_IDLE) {
        // When analyzing dynamically scheduled trace records, we expect
        // scheduler_tmpl_t::stream_status_t::STATUS_IDLE to be converted to
        // a TRACE_MARKER_TYPE_CORE_IDLE with tid set to IDLE_THREAD_ID.
        // When analyzing already-scheduled traces (aka core-sharded-on-disk
        // traces), the TRACE_MARKER_TYPE_CORE_IDLE on disk may not have a
        // preceding tid marker set to IDLE_THREAD_ID (therefore may be associated
        // with the tid of the prior input as far as the on-disk records are
        // concerned). But we have scheduler logic that converts such on-disk
        // TRACE_MARKER_TYPE_CORE_IDLE into the scheduler idle status (that is,
        // scheduler_tmpl_t::stream_status_t::STATUS_IDLE), which are converted
        // to a TRACE_MARKER_TYPE_CORE_IDLE with tid set to IDLE_THREAD_ID,
        // in the same manner as above.
        assert(memref.marker.tid == dynamorio::drmemtrace::IDLE_THREAD_ID);
        tid = dynamorio::drmemtrace::IDLE_THREAD_ID;
        shard->cur_state = STATE_IDLE;
    } else {
        shard->cur_state = STATE_CPU;
        tid = shard->stream->get_tid();
        workload_id = TESTANY(OFFLINE_FILE_TYPE_CORE_SHARDED, shard->filetype)
            ? workload_from_memref_tid(tid)
            : shard->stream->get_workload_id();
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_SYSCALL_TRACE_START)
        shard->in_syscall_trace = true;
    else if (memref.marker.type == TRACE_TYPE_MARKER &&
             memref.marker.marker_type == TRACE_MARKER_TYPE_SYSCALL_TRACE_END)
        shard->in_syscall_trace = false;
    if (shard->cur_state != prev_state) {
        if (!update_state_time(shard, prev_state))
            return false;
    }

    // We use <workload,tid> to detect switches (instead of input_id) to handle
    // core-sharded-on-disk.  However, we still prefer the input_id ordinal
    // for the letters.
    int64_t letter_ord =
        (TESTANY(OFFLINE_FILE_TYPE_CORE_SHARDED, shard->filetype) || input_id < 0)
        ? tid
        : input_id;
    if (workload_id != shard->prev_workload_id || tid != shard->prev_tid) {
        if (shard->in_syscall_trace) {
            shard->error =
                "Found unexpected switch in the middle of a kernel syscall trace.";
            return false;
        }
        record_context_switch(shard, shard->prev_workload_id, shard->prev_tid,
                              workload_id, tid, input_id, letter_ord);
    }
    shard->prev_workload_id = workload_id;
    shard->prev_tid = tid;

    if (shard->cur_state == STATE_WAIT) {
        ++shard->counters.waits;
        if (prev_state != STATE_WAIT) {
            shard->thread_sequence += WAIT_SYMBOL;
            shard->cur_segment_instrs = 0;
        } else {
            ++shard->cur_segment_instrs;
            if (shard->cur_segment_instrs == knob_print_every_) {
                shard->thread_sequence += WAIT_SYMBOL;
                shard->cur_segment_instrs = 0;
            }
        }
        return true;
    } else if (shard->cur_state == STATE_IDLE) {
        ++shard->counters.idles;
        if (prev_state != STATE_IDLE) {
            shard->thread_sequence += IDLE_SYMBOL;
            shard->cur_segment_instrs = 0;
        } else {
            ++shard->cur_segment_instrs;
            if (shard->cur_segment_instrs == knob_print_every_) {
                shard->thread_sequence += IDLE_SYMBOL;
                shard->cur_segment_instrs = 0;
            }
        }
        return true;
    }

    if (type_is_instr(memref.instr.type)) {
        ++shard->counters.instrs;
        ++shard->cur_segment_instrs;
        shard->counters.idle_micros_at_last_instr = shard->counters.idle_microseconds;
        if (shard->cur_segment_instrs == knob_print_every_) {
            shard->thread_sequence +=
                THREAD_LETTER_SUBSEQUENT_START + static_cast<char>(letter_ord % 26);
            shard->cur_segment_instrs = 0;
        }
        if (shard->last_syscall_number >= 0 && !shard->stream->is_record_kernel()) {
            // Since we clear last_syscall_number on detecting a context switch prior to
            // here, if it's still set and we've found a regular instruction, we just
            // completed a system call but did not switch.
            if (shard->stream->get_version() < TRACE_ENTRY_VERSION_FREQUENT_TIMESTAMPS) {
                // Legacy versions do not have the timestamps to compute latencies.
            } else {
                assert(shard->pre_syscall_timestamp > 0);
                assert(shard->post_syscall_timestamp > 0);
                histogram_interface_t *hist_ptr = find_or_add_histogram(
                    shard->counters.sysnum_noswitch_latency, shard->last_syscall_number,
                    kSysnumLatencyBinSize);
                int64_t latency =
                    shard->post_syscall_timestamp - shard->pre_syscall_timestamp;
                if (knob_verbose_ >= 3) {
                    std::cerr << "sysnum " << shard->last_syscall_number
                              << " noswitch latency " << latency << "\n";
                }
                hist_ptr->add(latency);
            }
            shard->last_syscall_number = -1;
            shard->pre_syscall_timestamp = 0;
            shard->post_syscall_timestamp = 0;
        }
        if (!shard->stream->is_record_kernel()) {
            // We wait until the next user-space instruction after the syscall trace
            // so that we've seen all syscall related trace entries (like the post-
            // syscall timestamp).
            shard->saw_syscall = false;
            shard->direct_switch_target = INVALID_THREAD_ID;
        }
        shard->saw_exit = false;
    }
    if (memref.instr.tid != INVALID_THREAD_ID)
        shard->counters.threads.insert(workload_tid_t(workload_id, memref.instr.tid));
    if (memref.marker.type == TRACE_TYPE_MARKER) {
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_SYSCALL) {
            ++shard->counters.syscalls;
            shard->saw_syscall = true;
            shard->last_syscall_number = static_cast<int>(memref.marker.marker_value);
        } else if (memref.marker.marker_type ==
                   TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL) {
            ++shard->counters.maybe_blocking_syscalls;
            shard->saw_syscall = true;
        } else if (memref.marker.marker_type == TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH) {
            ++shard->counters.direct_switch_requests;
            shard->direct_switch_target = memref.marker.marker_value;
        } else if (memref.marker.marker_type == TRACE_MARKER_TYPE_FILETYPE) {
            shard->filetype = static_cast<intptr_t>(memref.marker.marker_value);
        } else if (memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP) {
            if (shard->stream->is_record_kernel()) {
                shard->error = "Kernel traces are not expected to have timestamps.";
                return false;
            }
            if (shard->last_syscall_number < 0) {
                // We use get_input_interface() to get the original timestamp
                // instead of the scheduler-normalized one.
                // Unfortunately, we have no way to get the original for
                // core-sharded-on-disk traces, and the normalized ones are always
                // identical for pre vs post syscall due to not seeing any idle
                // records yet.  So the latencies are not useful for such traces;
                // but the syscall switch list is still useful.
                shard->pre_syscall_timestamp =
                    shard->stream->get_input_interface()->get_last_timestamp();
            } else if (shard->pre_syscall_timestamp > 0) {
                // We use get_input_interface() to get the original timestamp
                // instead of the scheduler-normalized one.
                shard->post_syscall_timestamp =
                    shard->stream->get_input_interface()->get_last_timestamp();
            }
        }
    } else if (memref.exit.type == TRACE_TYPE_THREAD_EXIT)
        shard->saw_exit = true;
    return true;
}

void
schedule_stats_t::print_percentage(double numerator, double denominator,
                                   const std::string &label)
{
    double fraction;
    if (denominator == 0) {
        if (numerator == 0)
            fraction = 0.;
        else
            fraction = 1.;
    } else
        fraction = numerator / denominator;
    std::cerr << std::setw(12) << std::setprecision(2) << 100 * fraction << label;
}

void
schedule_stats_t::print_counters(const counters_t &counters)
{
    std::cerr << std::setw(12) << counters.threads.size() << " threads";
    if (!counters.threads.empty()) {
        std::cerr << ": ";
        auto it = counters.threads.begin();
        while (it != counters.threads.end()) {
            std::cerr << "W" << it->workload_id << ".T" << tid_from_memref_tid(it->tid);
            ++it;
            if (it != counters.threads.end())
                std::cerr << ", ";
        }
    }
    std::cerr << "\n";
    std::cerr << std::setw(12) << counters.instrs << " instructions\n";
    std::cerr << std::setw(12) << counters.total_switches << " total context switches\n";
    double cspki = 0.;
    if (counters.instrs > 0)
        cspki = 1000 * counters.total_switches / static_cast<double>(counters.instrs);
    std::cerr << std::setw(12) << std::fixed << std::setprecision(7) << cspki
              << " CSPKI (context switches per 1000 instructions)\n";
    double ipcs = 0.;
    if (counters.total_switches > 0)
        ipcs = counters.instrs / static_cast<double>(counters.total_switches);
    std::cerr << std::setw(12) << std::fixed << std::setprecision(0) << ipcs
              << " instructions per context switch\n";
    std::cerr << std::setw(12) << std::fixed << std::setprecision(7)
              << counters.voluntary_switches << " voluntary context switches\n";
    std::cerr << std::setw(12) << counters.direct_switches
              << " direct context switches\n";
    std::cerr << std::setw(12) << counters.switch_sequence_injections
              << " context switch sequence injections\n";
    std::cerr << std::setw(12) << counters.syscall_sequence_injections
              << " system call sequence injections\n";
    print_percentage(static_cast<double>(counters.voluntary_switches),
                     static_cast<double>(counters.total_switches),
                     "% voluntary switches\n");
    print_percentage(static_cast<double>(counters.direct_switches),
                     static_cast<double>(counters.total_switches), "% direct switches\n");

    //  Statistics provided by scheduler.
    std::cerr << std::setw(12) << counters.switches_input_to_input
              << " switches input-to-input\n";
    std::cerr << std::setw(12) << counters.switches_input_to_idle
              << " switches input-to-idle\n";
    std::cerr << std::setw(12) << counters.switches_idle_to_input
              << " switches idle-to-input\n";
    std::cerr << std::setw(12) << counters.switches_nop << " switches nop-ed\n";
    std::cerr << std::setw(12) << counters.quantum_preempts << " quantum_preempts\n";
    std::cerr << std::setw(12) << counters.migrations << " migrations\n";
    std::cerr << std::setw(12) << counters.steals << " work steals\n";
    std::cerr << std::setw(12) << counters.rebalances << " rebalances\n";
    std::cerr << std::setw(12) << counters.at_output_limit << " output limits hit\n";

    std::cerr << std::setw(12) << counters.syscalls << " system calls\n";
    std::cerr << std::setw(12) << counters.maybe_blocking_syscalls
              << " maybe-blocking system calls\n";
    std::cerr << std::setw(12) << counters.direct_switch_requests
              << " direct switch requests\n";
    std::cerr << std::setw(12) << counters.observed_migrations
              << " observed migrations\n";
    std::cerr << std::setw(12) << counters.waits << " waits\n";
    std::cerr << std::setw(12) << counters.idles << " idles\n";
    print_percentage(static_cast<double>(counters.instrs),
                     static_cast<double>(counters.instrs + counters.idles),
                     "% cpu busy by record count\n");
    std::cerr << std::setw(12) << counters.cpu_microseconds << " cpu microseconds\n";
    std::cerr << std::setw(12) << counters.wait_microseconds << " wait microseconds\n";
    std::cerr << std::setw(12) << counters.idle_microseconds << " idle microseconds\n";
    std::cerr << std::setw(12) << counters.idle_micros_at_last_instr
              << " idle microseconds at last instr\n";
    print_percentage(
        static_cast<double>(counters.cpu_microseconds),
        static_cast<double>(counters.cpu_microseconds + counters.idle_microseconds),
        "% cpu busy by time\n");
    print_percentage(static_cast<double>(counters.cpu_microseconds),
                     static_cast<double>(counters.cpu_microseconds +
                                         counters.idle_micros_at_last_instr),
                     "% cpu busy by time, ignoring idle past last instr\n");
    std::cerr << "  Instructions per context switch histogram:\n";
    counters.instrs_per_switch->print();
    for (const auto &it : counters.tid2instrs_per_switch) {
        std::cerr << "  Thread W" << it.first.workload_id << ".T" << it.first.tid
                  << " instructions per context switch histogram:\n";
        it.second->print();
    }
    if (!counters.cores_per_thread->empty()) {
        std::cerr << "  Cores per thread:\n";
        counters.cores_per_thread->print();
    }
    std::cerr << "  Latencies for syscalls that incurred a switch:\n";
    for (const auto &it : counters.sysnum_switch_latency) {
        std::cerr << "    #" << it.first << "\n";
        it.second->print();
    }
    std::cerr << "  Latencies for syscalls that did not incur a switch:\n";
    for (const auto &it : counters.sysnum_noswitch_latency) {
        std::cerr << "    #" << it.first << "\n";
        it.second->print();
    }
}

void
schedule_stats_t::aggregate_results(counters_t &total)
{
    std::unordered_map<workload_tid_t, std::unordered_set<int64_t>, workload_tid_hash_t>
        cpu_footprint;
    for (const auto &shard : shard_map_) {
        // First update our per-shard data with per-shard stats from the scheduler.
        get_scheduler_stats(shard.second->stream, shard.second->counters);

        total += shard.second->counters;

        for (const workload_tid_t wtid : shard.second->counters.threads) {
            cpu_footprint[wtid].insert(shard.second->core);
        }

        // Sanity check against the scheduler's own stats, unless the trace
        // is pre-scheduled, or we're in core-serial mode where we don't have access
        // to the separate output streams, or we're in a unit test with a mock
        // stream and no stats.
        if (TESTANY(OFFLINE_FILE_TYPE_CORE_SHARDED, shard.second->filetype) ||
            serial_stream_ != nullptr ||
            shard.second->stream->get_schedule_statistic(
                memtrace_stream_t::SCHED_STAT_SWITCH_INPUT_TO_INPUT) < 0)
            continue;
        // We assume our counts fit in the get_schedule_statistic()'s double's 54-bit
        // mantissa and thus we can safely use "==".
        // Currently our switch count ignores idle-to-input.
        assert(shard.second->counters.total_switches ==
               shard.second->stream->get_schedule_statistic(
                   memtrace_stream_t::SCHED_STAT_SWITCH_INPUT_TO_INPUT) +
                   shard.second->stream->get_schedule_statistic(
                       memtrace_stream_t::SCHED_STAT_SWITCH_INPUT_TO_IDLE));
        assert(shard.second->counters.direct_switch_requests ==
               shard.second->stream->get_schedule_statistic(
                   memtrace_stream_t::SCHED_STAT_DIRECT_SWITCH_ATTEMPTS));
        assert(shard.second->counters.direct_switches ==
               shard.second->stream->get_schedule_statistic(
                   memtrace_stream_t::SCHED_STAT_DIRECT_SWITCH_SUCCESSES));
    }
    // Our observed_migrations are counted on the destination core, while
    // the scheduler reports migrations away from a source core: so we only
    // check the aggregate. For non-dynamic schedules the scheduler-reported
    // will be 0; for mock streams in tests it will be < 0; otherwise, the scheduler
    // may see more migrations due to inputs not yet executed moving among runqueues.
    assert(total.migrations <= 0. || total.migrations >= total.observed_migrations);

    // The += had no data to add as we do not update cores_per_thread incrementally
    // and instead derive it from counters_t.threads via cpu_footprint here.
    for (const auto &entry : cpu_footprint) {
        total.cores_per_thread->add(entry.second.size());
    }
}

bool
schedule_stats_t::print_results()
{
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << "Total counts:\n";
    counters_t total(this);
    aggregate_results(total);
    std::cerr << std::setw(12) << shard_map_.size() << " cores\n";
    print_counters(total);
    for (const auto &shard : shard_map_) {
        std::cerr << "Core #" << shard.second->core << " counts:\n";
        print_counters(shard.second->counters);
    }
    for (const auto &shard : shard_map_) {
        std::cerr << "Core #" << shard.second->core
                  << " schedule: " << shard.second->thread_sequence << "\n";
    }
    // For the switch-out list, limit entries at low verbosity to avoid spewing
    // 100K entries to the screen.
    size_t entries_per_shard = 4;
    if (knob_verbose_ >= 1) {
        entries_per_shard = 24;
        if (knob_verbose_ >= 2) {
            entries_per_shard = std::numeric_limits<size_t>::max();
        }
    }
    for (const auto &shard : shard_map_) {
        std::cerr << "Core #" << shard.second->core << " switch-outs:\n";
        size_t i;
        for (i = 0; i < std::min(entries_per_shard, shard.second->switch_record.size());
             ++i) {
            const schedule_record_t &record = shard.second->switch_record[i];
            std::cerr << std::setw(5) << i << " W" << record.workload << ".T" << std::left
                      << std::setw(6) << record.tid << std::right
                      << " instrs=" << std::setw(9) << record.instructions
                      << " sys#=" << std::setw(4) << record.syscall_number
                      << " latency=" << std::setw(7) << record.syscall_latency
                      << std::setw(6) << (record.voluntary ? "vol" : "invol")
                      << std::setw(7) << (record.direct ? "direct" : "") << "\n";
        }
        if (i < shard.second->switch_record.size()) {
            std::cerr << "    ... (increase -verbose to see more)\n";
        }
    }
    return true;
}

schedule_stats_t::counters_t
schedule_stats_t::get_total_counts()
{
    counters_t total(this);
    aggregate_results(total);
    return total;
}

bool
schedule_stats_t::get_switch_record(
    int core, std::vector<schedule_stats_t::schedule_record_t> &record)
{
    const auto &lookup = shard_map_.find(core);
    if (lookup == shard_map_.end())
        return false;
    record = lookup->second->switch_record;
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio
