/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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

/* Standalone scheduler launcher and "simulator" for file traces. */

#include <iostream>
#include <thread>

#ifdef WINDOWS
#    define UNICODE
#    define _UNICODE
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#else
#    include <sys/time.h>
#endif

#include "droption.h"
#include "dr_frontend.h"
#include "scheduler.h"
#include "trace_entry.h"
#include "test_helpers.h"
#ifdef HAS_ZIP
#    include "zipfile_istream.h"
#    include "zipfile_ostream.h"
#endif

using ::dynamorio::drmemtrace::disable_popups;
using ::dynamorio::drmemtrace::memref_t;
using ::dynamorio::drmemtrace::scheduler_t;
using ::dynamorio::drmemtrace::TRACE_TYPE_MARKER;
using ::dynamorio::drmemtrace::trace_type_names;
#ifdef HAS_ZIP
using ::dynamorio::drmemtrace::zipfile_istream_t;
using ::dynamorio::drmemtrace::zipfile_ostream_t;
#endif
using ::dynamorio::droption::droption_parser_t;
using ::dynamorio::droption::DROPTION_SCOPE_ALL;
using ::dynamorio::droption::DROPTION_SCOPE_FRONTEND;
using ::dynamorio::droption::droption_t;

namespace {

#define FATAL_ERROR(msg, ...)                               \
    do {                                                    \
        fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__); \
        fflush(stderr);                                     \
        exit(1);                                            \
    } while (0)

droption_t<std::string>
    op_trace_dir(DROPTION_SCOPE_FRONTEND, "trace_dir", "",
                 "[Required] Trace input directory",
                 "Specifies the directory containing the trace files to be analyzed.");

droption_t<int> op_verbose(DROPTION_SCOPE_ALL, "verbose", 1, 0, 64, "Verbosity level",
                           "Verbosity level for notifications.");

droption_t<int> op_num_cores(DROPTION_SCOPE_ALL, "num_cores", 4, 0, 8192,
                             "Number of cores", "Number of cores");

droption_t<int64_t> op_sched_quantum(DROPTION_SCOPE_ALL, "sched_quantum", 1 * 1000 * 1000,
                                     "Scheduling quantum",
                                     "Scheduling quantum: in instructions by default; in "
                                     "miroseconds if -sched_time is set.");

droption_t<bool> op_sched_time(DROPTION_SCOPE_ALL, "sched_time", false,
                               "Whether to use time for the scheduling quantum",
                               "Whether to use time for the scheduling quantum");

droption_t<bool> op_honor_stamps(DROPTION_SCOPE_ALL, "honor_stamps", true,
                                 "Whether to honor recorded timestamps for ordering",
                                 "Whether to honor recorded timestamps for ordering");

droption_t<double> op_block_time_scale(DROPTION_SCOPE_ALL, "block_time_scale", 1.,
                                       "Input block time scale factor",
                                       "A higher value here results in blocking syscalls "
                                       "keeping inputs unscheduled for longer.");

#ifdef HAS_ZIP
droption_t<std::string> op_record_file(DROPTION_SCOPE_FRONTEND, "record_file", "",
                                       "Path for storing record of schedule",
                                       "Path for storing record of schedule.");

droption_t<std::string> op_replay_file(DROPTION_SCOPE_FRONTEND, "replay_file", "",
                                       "Path with stored schedule for replay",
                                       "Path with stored schedule for replay.");
droption_t<std::string>
    op_cpu_schedule_file(DROPTION_SCOPE_FRONTEND, "cpu_schedule_file", "",
                         "Path with stored as-traced schedule for replay",
                         "Path with stored as-traced schedule for replay.");
#endif

droption_t<uint64_t> op_print_every(DROPTION_SCOPE_ALL, "print_every", 5000,
                                    "A letter is printed every N instrs",
                                    "A letter is printed every N instrs");

uint64_t
get_current_microseconds()
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

// Processes the stream of records scheduled on the "ordinal"-th virtual core with
// output stream "steram" and scheduler "scheduler".
// Returns in "thread_sequence" a representation of which inputs ran and for
// how long on the core:
// - The letter 'A' plus the input ordinal % 26 represents that input.  A letter
//   is printed on each context switch and additionally after each -print_every
//   instructions.
// - A comma represents a context switch to a new input.
// - A '-' represents a wait where no input was ready or a scheduler-encorced
//   dependence is not yet met.
void
simulate_core(int ordinal, scheduler_t::stream_t *stream, const scheduler_t &scheduler,
              std::string &thread_sequence)
{
    // XXX: Could we share some code with the schedule_stats analysis tool?
    // Some features are now duplicated in both.
    static constexpr char THREAD_LETTER_INITIAL_START = 'A';
    static constexpr char THREAD_LETTER_SUBSEQUENT_START = 'a';
    static constexpr char WAIT_SYMBOL = '-';
    static constexpr char IDLE_SYMBOL = '_';
    memref_t record;
    uint64_t micros = op_sched_time.get_value() ? get_current_microseconds() : 0;
    uint64_t cur_segment_instrs = 0;
    bool prev_was_wait = false, prev_was_idle = false;
    // Measure cpu usage by counting each next_record() as one cycle.
    uint64_t cycles_total = 0, cycles_busy = 0;
    // Thread ids can be duplicated, so use the input ordinals to distinguish.
    scheduler_t::input_ordinal_t prev_input = scheduler_t::INVALID_INPUT_ORDINAL;
    for (scheduler_t::stream_status_t status = stream->next_record(record, micros);
         status != scheduler_t::STATUS_EOF;
         status = stream->next_record(record, micros)) {
        if (op_sched_time.get_value())
            micros = get_current_microseconds();
        ++cycles_total;
        // Cache and reset here to ensure we reset on early return paths.
        bool was_wait = prev_was_wait;
        bool was_idle = prev_was_idle;
        prev_was_wait = false;
        prev_was_idle = false;
        if (status == scheduler_t::STATUS_WAIT) {
            if (!was_wait || cur_segment_instrs == op_print_every.get_value())
                thread_sequence += WAIT_SYMBOL;
            ++cur_segment_instrs;
            if (cur_segment_instrs == op_print_every.get_value())
                cur_segment_instrs = 0;
            prev_was_wait = true;
            std::this_thread::yield();
            continue;
        } else if (status == scheduler_t::STATUS_IDLE) {
            if (!was_idle || cur_segment_instrs == op_print_every.get_value())
                thread_sequence += IDLE_SYMBOL;
            ++cur_segment_instrs;
            if (cur_segment_instrs == op_print_every.get_value())
                cur_segment_instrs = 0;
            prev_was_idle = true;
            std::this_thread::yield();
            continue;
        } else if (status != scheduler_t::STATUS_OK)
            FATAL_ERROR("scheduler failed to advance: %d", status);
        ++cycles_busy;
        if (op_verbose.get_value() >= 4) {
            std::ostringstream line;
            line << "Core #" << std::setw(2) << ordinal << " @" << std::setw(9)
                 << stream->get_record_ordinal() << " refs, " << std::setw(9)
                 << stream->get_instruction_ordinal() << " instrs: input " << std::setw(4)
                 << stream->get_input_stream_ordinal() << " @" << std::setw(9)
                 << scheduler
                        .get_input_stream_interface(stream->get_input_stream_ordinal())
                        ->get_record_ordinal()
                 << " refs, " << std::setw(9)
                 << scheduler
                        .get_input_stream_interface(stream->get_input_stream_ordinal())
                        ->get_instruction_ordinal()
                 << " instrs: " << std::setw(16) << trace_type_names[record.marker.type];
            if (type_is_instr(record.instr.type))
                line << " pc=" << std::hex << record.instr.addr << std::dec;
            else if (record.marker.type == TRACE_TYPE_MARKER) {
                line << " " << record.marker.marker_type
                     << " val=" << record.marker.marker_value;
            }
            line << "\n";
            std::cerr << line.str();
        }
        scheduler_t::input_ordinal_t input = stream->get_input_stream_ordinal();
        if (input != prev_input) {
            // We convert to letters which only works well for <=26 inputs.
            thread_sequence +=
                THREAD_LETTER_INITIAL_START + static_cast<char>(input % 26);
            cur_segment_instrs = 0;
            if (op_verbose.get_value() >= 2) {
                std::ostringstream line;
                line
                    << "Core #" << std::setw(2) << ordinal << " @" << std::setw(9)
                    << stream->get_record_ordinal() << " refs, " << std::setw(9)
                    << stream->get_instruction_ordinal() << " instrs: input "
                    << std::setw(4) << input << " @" << std::setw(9)
                    << scheduler
                           .get_input_stream_interface(stream->get_input_stream_ordinal())
                           ->get_record_ordinal()
                    << " refs, " << std::setw(9)
                    << scheduler
                           .get_input_stream_interface(stream->get_input_stream_ordinal())
                           ->get_instruction_ordinal()
                    << " instrs, time " << std::setw(16)
                    << (op_sched_time.get_value()
                            ? micros
                            : scheduler
                                  .get_input_stream_interface(
                                      stream->get_input_stream_ordinal())
                                  ->get_last_timestamp())
                    << " == thread " << record.instr.tid << "\n";
                std::cerr << line.str();
            }
            prev_input = input;
        }
        if (type_is_instr(record.instr.type)) {
            ++cur_segment_instrs;
            if (cur_segment_instrs == op_print_every.get_value()) {
                thread_sequence +=
                    THREAD_LETTER_SUBSEQUENT_START + static_cast<char>(input % 26);
                cur_segment_instrs = 0;
            }
        }
#ifdef HAS_ZIP
        else if (record.marker.type == dynamorio::drmemtrace::TRACE_TYPE_MARKER) {
            if (record.marker.marker_type ==
                dynamorio::drmemtrace::TRACE_MARKER_TYPE_CPU_ID) {
                if (!op_cpu_schedule_file.get_value().empty()) {
                    int cpu = (int)record.marker.marker_value;
                    int output_cpuid = stream->get_output_cpuid();
                    if (cpu != output_cpuid) {
                        FATAL_ERROR("CPU marker %d on core #%d differs from output "
                                    "stream CPU ID %d\n",
                                    cpu, ordinal, output_cpuid);
                    }
                }
            }
        }
#endif
    }
    float usage = 0;
    if (cycles_total > 0)
        usage = 100.f * cycles_busy / static_cast<float>(cycles_total);
    std::ostringstream line;
    line << "Core #" << std::setw(2) << ordinal << " usage: " << std::setw(9) << usage
         << "%\n";
    std::cerr << line.str();
}

} // namespace

int
_tmain(int argc, const TCHAR *targv[])
{
    disable_popups();

    // Convert to UTF-8 if necessary
    char **argv;
    drfront_status_t sc = drfront_convert_args(targv, &argv, argc);
    if (sc != DRFRONT_SUCCESS)
        FATAL_ERROR("Failed to process args: %d", sc);

    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_FRONTEND, argc, (const char **)argv,
                                       &parse_err, NULL) ||
        op_trace_dir.get_value().empty()) {
        FATAL_ERROR("Usage error: %s\nUsage:\n%s", parse_err.c_str(),
                    droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
    }

    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(op_trace_dir.get_value());
    scheduler_t::scheduler_options_t sched_ops(
        scheduler_t::MAP_TO_ANY_OUTPUT,
        op_honor_stamps.get_value() ? scheduler_t::DEPENDENCY_TIMESTAMPS
                                    : scheduler_t::DEPENDENCY_IGNORE,
        scheduler_t::SCHEDULER_DEFAULTS, op_verbose.get_value());
    sched_ops.quantum_duration = op_sched_quantum.get_value();
    if (op_sched_time.get_value())
        sched_ops.quantum_unit = scheduler_t::QUANTUM_TIME;
    sched_ops.block_time_scale = op_block_time_scale.get_value();
#ifdef HAS_ZIP
    std::unique_ptr<zipfile_ostream_t> record_zip;
    std::unique_ptr<zipfile_istream_t> replay_zip;
    std::unique_ptr<zipfile_istream_t> cpu_schedule_zip;
    if (!op_record_file.get_value().empty()) {
        record_zip.reset(new zipfile_ostream_t(op_record_file.get_value()));
        sched_ops.schedule_record_ostream = record_zip.get();
    } else if (!op_replay_file.get_value().empty()) {
        replay_zip.reset(new zipfile_istream_t(op_replay_file.get_value()));
        sched_ops.schedule_replay_istream = replay_zip.get();
        sched_ops.mapping = scheduler_t::MAP_AS_PREVIOUSLY;
        sched_ops.deps = scheduler_t::DEPENDENCY_TIMESTAMPS;
    } else if (!op_cpu_schedule_file.get_value().empty()) {
        cpu_schedule_zip.reset(new zipfile_istream_t(op_cpu_schedule_file.get_value()));
        sched_ops.mapping = scheduler_t::MAP_TO_RECORDED_OUTPUT;
        sched_ops.deps = scheduler_t::DEPENDENCY_TIMESTAMPS;
        sched_ops.replay_as_traced_istream = cpu_schedule_zip.get();
    }
#endif
    if (scheduler.init(sched_inputs, op_num_cores.get_value(), std::move(sched_ops)) !=
        scheduler_t::STATUS_SUCCESS) {
        FATAL_ERROR("failed to initialize scheduler: %s",
                    scheduler.get_error_string().c_str());
    }

    std::vector<std::thread> threads;
    std::vector<std::string> schedules(op_num_cores.get_value());
    std::cerr << "Creating " << op_num_cores.get_value() << " simulator threads\n";
    threads.reserve(op_num_cores.get_value());
    for (int i = 0; i < op_num_cores.get_value(); ++i) {
        threads.emplace_back(std::thread(&simulate_core, i, scheduler.get_stream(i),
                                         std::ref(scheduler), std::ref(schedules[i])));
    }
    for (std::thread &thread : threads)
        thread.join();

    for (int i = 0; i < op_num_cores.get_value(); ++i) {
        std::cerr << "Core #" << i << ": " << schedules[i] << "\n";
    }

#ifdef HAS_ZIP
    if (!op_record_file.get_value().empty()) {
        if (scheduler.write_recorded_schedule() != scheduler_t::STATUS_SUCCESS) {
            FATAL_ERROR("Failed to write schedule to %s",
                        op_record_file.get_value().c_str());
        }
    }
#endif

    return 0;
}
