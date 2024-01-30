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

#define NOMINMAX // Avoid windows.h messing up std::max.

#include "schedule_stats.h"

#ifdef WINDOWS
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#else
#    include <sys/time.h>
#endif

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
        per_shard = new per_shard_t;
        per_shard->stream = serial_stream_;
        per_shard->core = serial_stream_->get_output_cpuid();
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
    auto per_shard = new per_shard_t;
    std::lock_guard<std::mutex> guard(shard_map_mutex_);
    per_shard->stream = stream;
    per_shard->core = stream->get_output_cpuid();
    shard_map_[shard_index] = per_shard;
    return reinterpret_cast<void *>(per_shard);
}

bool
schedule_stats_t::parallel_shard_exit(void *shard_data)
{
    // Nothing (we read the shard data in print_results).
    per_shard_t *shard = reinterpret_cast<per_shard_t *>(shard_data);
    if (shard->prev_was_idle) {
        shard->counters.idle_microseconds +=
            get_current_microseconds() - shard->segment_start_microseconds;
    } else if (!shard->prev_was_wait) {
        shard->counters.cpu_microseconds +=
            get_current_microseconds() - shard->segment_start_microseconds;
    }
    return true;
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
#ifdef UNIX
    struct timeval time;
    if (gettimeofday(&time, nullptr) != 0)
        return 0;
    return time.tv_sec * 1000000 + time.tv_usec;
#else
    SYSTEMTIME sys_time;
    GetSystemTime(&sys_time);
    FILETIME file_time;
    if (!SystemTimeToFileTime(&sys_time, &file_time))
        return 0;
    return file_time.dwLowDateTime +
        (static_cast<uint64_t>(file_time.dwHighDateTime) << 32);
#endif
}

bool
schedule_stats_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    static constexpr char THREAD_LETTER_INITIAL_START = 'A';
    static constexpr char THREAD_LETTER_SUBSEQUENT_START = 'a';
    static constexpr char WAIT_SYMBOL = '-';
    static constexpr char IDLE_SYMBOL = '_';
    per_shard_t *shard = reinterpret_cast<per_shard_t *>(shard_data);
    if (knob_verbose_ >= 4) {
        std::ostringstream line;
        line << "Core #" << std::setw(2) << shard->core << " @" << std::setw(9)
             << shard->stream->get_record_ordinal() << " refs, " << std::setw(9)
             << shard->stream->get_instruction_ordinal() << " instrs: input "
             << std::setw(4) << shard->stream->get_input_id() << " @" << std::setw(9)
             << shard->stream->get_input_interface()->get_record_ordinal() << " refs, "
             << std::setw(9)
             << shard->stream->get_input_interface()->get_instruction_ordinal()
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
    // Cache and reset here to ensure we reset on early return paths.
    bool was_wait = shard->prev_was_wait;
    bool was_idle = shard->prev_was_idle;
    int64_t prev_input = shard->prev_input;
    shard->prev_was_wait = false;
    shard->prev_was_idle = false;
    shard->prev_input = -1;
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_CORE_WAIT) {
        ++shard->counters.waits;
        shard->prev_was_wait = true;
        if (!was_wait) {
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
    } else if (memref.marker.type == TRACE_TYPE_MARKER &&
               memref.marker.marker_type == TRACE_MARKER_TYPE_CORE_IDLE) {
        ++shard->counters.idles;
        shard->prev_was_idle = true;
        if (!was_idle) {
            shard->thread_sequence += IDLE_SYMBOL;
            shard->cur_segment_instrs = 0;
            if (!was_wait && shard->segment_start_microseconds > 0) {
                shard->counters.idle_microseconds +=
                    get_current_microseconds() - shard->segment_start_microseconds;
            }
            shard->segment_start_microseconds = get_current_microseconds();
        } else {
            ++shard->cur_segment_instrs;
            if (shard->cur_segment_instrs == knob_print_every_) {
                shard->thread_sequence += IDLE_SYMBOL;
                shard->cur_segment_instrs = 0;
            }
        }
        return true;
    }
    int64_t input = shard->stream->get_input_id();
    if (input != prev_input) {
        // We convert to letters which only works well for <=26 inputs.
        if (!shard->thread_sequence.empty()) {
            ++shard->counters.total_switches;
            if (shard->saw_syscall || shard->saw_exit)
                ++shard->counters.voluntary_switches;
            if (shard->direct_switch_target == memref.marker.tid)
                ++shard->counters.direct_switches;
        }
        shard->thread_sequence +=
            THREAD_LETTER_INITIAL_START + static_cast<char>(input % 26);
        shard->cur_segment_instrs = 0;
        if (!was_wait && !was_idle && shard->segment_start_microseconds > 0) {
            shard->counters.cpu_microseconds +=
                get_current_microseconds() - shard->segment_start_microseconds;
        }
        shard->segment_start_microseconds = get_current_microseconds();
        if (knob_verbose_ >= 2) {
            std::ostringstream line;
            line << "Core #" << std::setw(2) << shard->core << " @" << std::setw(9)
                 << shard->stream->get_record_ordinal() << " refs, " << std::setw(9)
                 << shard->stream->get_instruction_ordinal() << " instrs: input "
                 << std::setw(4) << input << " @" << std::setw(9)
                 << shard->stream->get_input_interface()->get_record_ordinal()
                 << " refs, " << std::setw(9)
                 << shard->stream->get_input_interface()->get_instruction_ordinal()
                 << " instrs, time "
                 << std::setw(16)
                 // TODO i#5843: For time quanta, provide some way to get the
                 // latest time and print that here instead of the the timestamp?
                 << shard->stream->get_input_interface()->get_last_timestamp()
                 << " == thread " << memref.instr.tid << "\n";
            std::cerr << line.str();
        }
    }
    shard->prev_input = input;
    if (type_is_instr(memref.instr.type)) {
        ++shard->counters.instrs;
        ++shard->cur_segment_instrs;
        shard->counters.idle_micros_at_last_instr = shard->counters.idle_microseconds;
        if (shard->cur_segment_instrs == knob_print_every_) {
            shard->thread_sequence +=
                THREAD_LETTER_SUBSEQUENT_START + static_cast<char>(input % 26);
            shard->cur_segment_instrs = 0;
        }
        shard->direct_switch_target = INVALID_THREAD_ID;
        shard->saw_syscall = false;
        shard->saw_exit = false;
    }
    if (memref.instr.tid != INVALID_THREAD_ID)
        shard->counters.threads.insert(memref.instr.tid);
    if (memref.marker.type == TRACE_TYPE_MARKER) {
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_SYSCALL) {
            ++shard->counters.syscalls;
            shard->saw_syscall = true;
        } else if (memref.marker.marker_type ==
                   TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL) {
            ++shard->counters.maybe_blocking_syscalls;
            shard->saw_syscall = true;
        } else if (memref.marker.marker_type == TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH) {
            ++shard->counters.direct_switch_requests;
            shard->direct_switch_target = memref.marker.marker_value;
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
    std::cerr << std::setw(12) << counters.threads.size() << " threads\n";
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
    print_percentage(static_cast<double>(counters.voluntary_switches),
                     static_cast<double>(counters.total_switches),
                     "% voluntary switches\n");
    print_percentage(static_cast<double>(counters.direct_switches),
                     static_cast<double>(counters.total_switches), "% direct switches\n");
    std::cerr << std::setw(12) << counters.syscalls << " system calls\n";
    std::cerr << std::setw(12) << counters.maybe_blocking_syscalls
              << " maybe-blocking system calls\n";
    std::cerr << std::setw(12) << counters.direct_switch_requests
              << " direct switch requests\n";
    std::cerr << std::setw(12) << counters.waits << " waits\n";
    std::cerr << std::setw(12) << counters.idles << " idles\n";
    print_percentage(static_cast<double>(counters.instrs),
                     static_cast<double>(counters.instrs + counters.idles),
                     "% cpu busy by record count\n");
    std::cerr << std::setw(12) << counters.cpu_microseconds << " cpu microseconds\n";
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
}

bool
schedule_stats_t::print_results()
{
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << "Total counts:\n";
    counters_t total;
    for (const auto &shard : shard_map_) {
        total += shard.second->counters;
    }
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
    return true;
}

schedule_stats_t::counters_t
schedule_stats_t::get_total_counts()
{
    counters_t total;
    for (const auto &shard : shard_map_) {
        total += shard.second->counters;
    }
    return total;
}

} // namespace drmemtrace
} // namespace dynamorio
