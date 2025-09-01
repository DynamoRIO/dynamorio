/* **********************************************************
 * Copyright (c) 2019-2025 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

// Tests -time_syscall_scale during tracing.

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

#include <atomic>
#include <iostream>
#include <thread>

// This is set globally in CMake for other tests so easier to undef here.
#undef DR_REG_ENUM_COMPATIBILITY

#include "configure.h"
#include "dr_api.h"
#include "drmemtrace/drmemtrace.h"
#include "drcovlib.h"
#include "analysis_tool.h"
#include "scheduler.h"
#include "test_helpers.h"
#include "tracer/raw2trace.h"
#include "tracer/raw2trace_directory.h"

#undef ALIGN_FORWARD // Conflicts with drcachesim utils.h.
#include "tools.h"

#ifndef LINUX
#    error Only Linux supported for this test.
#endif

namespace dynamorio {
namespace drmemtrace {

bool
my_setenv(const char *var, const char *value)
{
    return setenv(var, value, 1 /*override*/) == 0;
}

/****************************************************************************
 * Code that gets traced.
 */

static timer_t posix_timer_id;

// We test both an itimer and a POSIX timer.
static void
signal_handler_posix(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    assert(sig == SIGUSR1);
}

static void
signal_handler_itimer(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    assert(sig == SIGPROF);
}

static void
create_posix_timer(void)
{
    // SIGEV_THREAD_ID is our target timer as we've seen real-world applications
    // use it.
    struct sigevent se = {};
    se.sigev_notify = SIGEV_THREAD_ID;
#ifndef sigev_notify_thread_id
#    define sigev_notify_thread_id _sigev_un._tid
#endif
    se.sigev_notify_thread_id = syscall(SYS_gettid);
    intercept_signal(SIGUSR1, signal_handler_posix, false);
    se.sigev_signo = SIGUSR1;
    int res = timer_create(CLOCK_PROCESS_CPUTIME_ID, &se, &posix_timer_id);
    assert(res == 0);
}

static void
enable_timers(void)
{
    int res;
    struct itimerval val;
    val.it_interval.tv_sec = 0;
    val.it_interval.tv_usec = 10000;
    val.it_value.tv_sec = 0;
    val.it_value.tv_usec = 10000;
    intercept_signal(SIGPROF, signal_handler_itimer, false);
    res = setitimer(ITIMER_PROF, &val, NULL);
    assert(res == 0);
    struct itimerspec spec;
    spec.it_interval.tv_sec = 0;
    spec.it_interval.tv_nsec = 10000000;
    spec.it_value.tv_sec = 0;
    spec.it_value.tv_nsec = 10000000;
    res = timer_settime(posix_timer_id, 0, &spec, NULL);
    assert(res == 0);
}

static void
disable_timers(void)
{
    int res;
    struct itimerval val = {};
    res = setitimer(ITIMER_PROF, &val, NULL);
    assert(res == 0);
    struct itimerspec spec = {};
    res = timer_settime(posix_timer_id, 0, &spec, NULL);
    assert(res == 0);
}

static void
do_some_work(void)
{
    enable_timers();
    // Do real work to trigger CPU time-based timers.
    static const int ITERS = 5000;
    double val = ITERS / 33.;
    double **vals = (double **)calloc(ITERS, sizeof(double *));
    for (int i = 0; i < ITERS; ++i) {
        vals[i] = (double *)malloc(sizeof(double));
        *vals[i] = sin(val);
        val += *vals[i];
        vals[i] = (double *)realloc(vals[i], 2 * sizeof(double));
    }
    for (int i = 0; i < ITERS; ++i)
        free(vals[i]);
    free(vals);
    disable_timers();
}

/****************************************************************************
 * Trace processing code.
 */

// XXX: We could try to share common elements of these drmemtrace burst
// tests to share code like this across them.
static std::string
post_process(const std::string &out_subdir)
{
    const char *raw_dir;
    drmemtrace_status_t mem_res = drmemtrace_get_output_path(&raw_dir);
    assert(mem_res == DRMEMTRACE_SUCCESS);
    std::string outdir = std::string(raw_dir) + DIRSEP + out_subdir;
    void *dr_context = dr_standalone_init();
    /* Now write a final trace to a location that the drcachesim -indir step
     * run by the outer test harness will find (TRACE_FILENAME).
     * Use a new scope to free raw2trace_directory_t before dr_standalone_exit().
     * We could alternatively make a scope exit template helper.
     */
    {
        raw2trace_directory_t dir;
        if (!dr_create_dir(outdir.c_str())) {
            std::cerr << "Failed to create output dir";
            assert(false);
        }
        std::string dir_err = dir.initialize(raw_dir, outdir);
        assert(dir_err.empty());
        raw2trace_t raw2trace(dir.modfile_bytes_, dir.in_files_, dir.out_files_,
                              dir.out_archives_, dir.encoding_file_,
                              dir.serial_schedule_file_, dir.cpu_schedule_file_,
                              dr_context, 0);
        std::string error = raw2trace.do_conversion();
        if (!error.empty()) {
            std::cerr << "raw2trace failed: " << error << "\n";
            assert(false);
        }
    }
    dr_standalone_exit();
    return outdir;
}

static void
event_sample(void *drcontext, dr_mcontext_t *mcontext)
{
    // Do nothing.
}

extern "C" {
/* This dr_client_main should be called instead of the one in tracer.cpp. */
DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drmemtrace_client_main(id, argc, argv);
    // Test itimer multiplexing interacting with scaling.
    bool ok = dr_set_itimer(ITIMER_VIRTUAL, 10, event_sample);
    assert(ok);
}
}

static std::string
gather_trace(const std::string &tracer_ops, const std::string &out_subdir)
{
    std::string dr_ops("-stderr_mask 0xc -client_lib ';;-offline " + tracer_ops + "'");
    if (!my_setenv("DYNAMORIO_OPTIONS", dr_ops.c_str()))
        std::cerr << "failed to set env var!\n";
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());
    dr_app_start();
    assert(dr_app_running_under_dynamorio());
    do_some_work();
    dr_app_stop_and_cleanup();
    assert(!dr_app_running_under_dynamorio());

    return post_process(out_subdir);
}

static int
count_signals(const std::string &dir)
{
    int count = 0;
    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(dir);
    if (scheduler.init(sched_inputs, 1, scheduler_t::make_scheduler_serial_options()) !=
        scheduler_t::STATUS_SUCCESS) {
        std::cerr << "Failed to initialize scheduler " << scheduler.get_error_string()
                  << "\n";
    }
    auto *stream = scheduler.get_stream(0);
    int signals = 0;
    while (true) {
        memref_t memref;
        scheduler_t::stream_status_t status = stream->next_record(memref);
        if (status == scheduler_t::STATUS_EOF) {
            break;
        }
        assert(status == scheduler_t::STATUS_OK);
        if (memref.marker.type == TRACE_TYPE_MARKER &&
            memref.marker.marker_type == TRACE_MARKER_TYPE_SIGNAL_NUMBER)
            ++count;
    }
    return count;
}

int
test_main(int argc, const char *argv[])
{
    create_posix_timer();

    std::cerr << "gathering no-scaling trace\n";
    std::string dir_default = gather_trace("", "default");
    std::cerr << "gathering scaled-timer trace\n";
    std::string dir_scale = gather_trace("-scale_timers 10", "scale");
    std::cerr << "processing results\n";

    void *dr_context = dr_standalone_init();

    int signals_default = count_signals(dir_default);
    int signals_scale = count_signals(dir_scale);
    std::cerr << "signals default=" << signals_default << " scale=" << signals_scale
              << "\n";
    // We scaled by 10, but machine load can cause a wide range of actual results.
    // We thus only require a 2x difference to avoid flakiness.
    assert(signals_default > 2 * signals_scale);

    dr_standalone_exit();
    std::cerr << "all done\n";
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
