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
#endif

#include "droption.h"
#include "dr_frontend.h"
#include "scheduler.h"
#include "zipfile_istream.h"
#include "zipfile_ostream.h"

using namespace dynamorio::drmemtrace;

#define FATAL_ERROR(msg, ...)                               \
    do {                                                    \
        fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__); \
        fflush(stderr);                                     \
        exit(1);                                            \
    } while (0)

namespace {

droption_t<std::string>
    op_trace_dir(DROPTION_SCOPE_FRONTEND, "trace_dir", "",
                 "[Required] Trace input directory",
                 "Specifies the directory containing the trace files to be analyzed.");

droption_t<int> op_verbose(DROPTION_SCOPE_ALL, "verbose", 0, 0, 64, "Verbosity level",
                           "Verbosity level for notifications.");

droption_t<int> op_num_cores(DROPTION_SCOPE_ALL, "num_cores", 4, 0, 8192,
                             "Number of cores", "Number of cores");

droption_t<int64_t> op_sched_quantum(DROPTION_SCOPE_ALL, "sched_quantum", 1 * 1000 * 1000,
                                     "Scheduling quantum in instructions",
                                     "Scheduling quantum in instructions");

droption_t<std::string> op_record_file(DROPTION_SCOPE_FRONTEND, "record_file", "",
                                       "Path for storing record of schedule",
                                       "Path for storing record of schedule.");

droption_t<std::string> op_replay_file(DROPTION_SCOPE_FRONTEND, "replay_file", "",
                                       "Path with stored schedule for replay",
                                       "Path with stored schedule for replay.");

void
simulate_core(int ordinal, scheduler_t::stream_t *stream, const scheduler_t &scheduler,
              std::vector<memref_tid_t> &thread_sequence)
{
    memref_t record;
    memref_tid_t prev_tid = INVALID_THREAD_ID;
    for (scheduler_t::stream_status_t status = stream->next_record(record);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(record)) {
        if (status == scheduler_t::STATUS_WAIT) {
            std::this_thread::yield();
            continue;
        }
        if (status != scheduler_t::STATUS_OK)
            FATAL_ERROR("scheduler failed to advance: %d", status);
        if (thread_sequence.empty())
            thread_sequence.push_back(record.instr.tid);
        if (record.instr.tid != prev_tid) {
            thread_sequence.push_back(record.instr.tid);
            if (op_verbose.get_value() > 1) {
                std::ostringstream line;
                line
                    << "Core #" << ordinal << " @" << stream->get_record_ordinal()
                    << " refs, " << stream->get_instruction_ordinal() << " instrs: input "
                    << stream->get_input_stream_ordinal() << " @"
                    << scheduler
                           .get_input_stream_interface(stream->get_input_stream_ordinal())
                           ->get_record_ordinal()
                    << " refs, "
                    << scheduler
                           .get_input_stream_interface(stream->get_input_stream_ordinal())
                           ->get_instruction_ordinal()
                    << " instrs == thread " << record.instr.tid << "\n";
                std::cerr << line.str();
            }
            prev_tid = record.instr.tid;
        }
    }
}

} // namespace

int
_tmain(int argc, const TCHAR *targv[])
{
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
        scheduler_t::MAP_TO_ANY_OUTPUT, scheduler_t::DEPENDENCY_IGNORE,
        scheduler_t::SCHEDULER_DEFAULTS, op_verbose.get_value());
    sched_ops.quantum_duration = op_sched_quantum.get_value();
    std::unique_ptr<zipfile_ostream_t> record_zip;
    std::unique_ptr<zipfile_istream_t> replay_zip;
    if (!op_record_file.get_value().empty()) {
        record_zip.reset(new zipfile_ostream_t(op_record_file.get_value()));
        sched_ops.schedule_record_ostream = record_zip.get();
    }
    if (!op_replay_file.get_value().empty()) {
        replay_zip.reset(new zipfile_istream_t(op_replay_file.get_value()));
        sched_ops.schedule_replay_istream = replay_zip.get();
        sched_ops.mapping = scheduler_t::MAP_AS_PREVIOUSLY;
    }
    if (scheduler.init(sched_inputs, op_num_cores.get_value(), sched_ops) !=
        scheduler_t::STATUS_SUCCESS) {
        FATAL_ERROR("failed to initialize scheduler: %s",
                    scheduler.get_error_string().c_str());
    }

    std::vector<std::thread> threads;
    std::vector<std::vector<memref_tid_t>> schedules(op_num_cores.get_value());
    std::cerr << "Creating " << op_num_cores.get_value() << " simulator threads\n";
    threads.reserve(op_num_cores.get_value());
    for (int i = 0; i < op_num_cores.get_value(); ++i) {
        threads.emplace_back(std::thread(&simulate_core, i, scheduler.get_stream(i),
                                         std::ref(scheduler), std::ref(schedules[i])));
    }
    for (std::thread &thread : threads)
        thread.join();

    for (int i = 0; i < op_num_cores.get_value(); ++i) {
        std::cerr << "Core #" << i << ": ";
        for (memref_tid_t tid : schedules[i])
            std::cerr << tid << " ";
        std::cerr << "\n";
    }

    if (!op_record_file.get_value().empty()) {
        if (scheduler.write_recorded_schedule() != scheduler_t::STATUS_SUCCESS) {
            FATAL_ERROR("Failed to write schedule to %s",
                        op_record_file.get_value().c_str());
        }
    }

    return 0;
}
