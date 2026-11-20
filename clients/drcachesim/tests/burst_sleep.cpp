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

// Enable asserts in release build testing too.
#undef NDEBUG

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include <atomic>
#include <iostream>
#include <thread>

#include "dr_api.h"
#include "drmemtrace/drmemtrace.h"
#include "drcovlib.h"
#include "analysis_tool.h"
#include "scheduler.h"
#include "tracer/raw2trace.h"
#include "tracer/raw2trace_directory.h"
#include "../../core/unix/include/syscall_target.h"

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

static pthread_cond_t condvar;
static bool child_ready;
static pthread_mutex_t lock;
static std::atomic<bool> child_should_exit;
static std::atomic<bool> saw_eintr;
static std::atomic<int> sleep_count;

static void
handler(int sig)
{
    // Nothing; just interrupt the sleep.
}

static void *
thread_routine(void *arg)
{
    signal(SIGUSR1, handler);

    pthread_mutex_lock(&lock);
    child_ready = true;
    pthread_cond_signal(&condvar);
    pthread_mutex_unlock(&lock);

    struct timespec sleeptime;
    sleeptime.tv_sec = 0;
    sleeptime.tv_nsec = 100000;
    struct timespec remaining;
    while (!child_should_exit.load(std::memory_order_acquire)) {
        sleep_count.fetch_add(1, std::memory_order_release);
        int res = nanosleep(&sleeptime, &remaining);
        if (res != 0) {
            assert(errno == EINTR);
            // Ensure the remaining time was deflated.
            assert(remaining.tv_sec <= sleeptime.tv_sec);
            saw_eintr.store(true, std::memory_order_release);
        }
    }
    return nullptr;
}

static double
do_some_work()
{
    // It is difficult to use a constant iter count in our work loop and still
    // produce a good sleep count across varying test machines: too few and we don't
    // have enough to see a scale effect; too many and the test takes too long on
    // slower machines.  We solve this by figuring out an iter count experimentally.
    // The first time we're called, we run until we see MIN_SLEEPS sleeps and 1 EINTR
    // in the other thread.  We record that iter count and use it next time.
    constexpr int MIN_SLEEPS = 50;
    static int computed_iters;

    pthread_t thread;
    void *retval;
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&condvar, NULL);

    pthread_mutex_lock(&lock);
    child_ready = false;
    pthread_mutex_unlock(&lock);
    child_should_exit.store(false, std::memory_order_release);
    saw_eintr.store(true /*false*/, std::memory_order_release);
    sleep_count.store(0, std::memory_order_release);
    child_should_exit.store(false, std::memory_order_release);

    int res = pthread_create(&thread, NULL, thread_routine, nullptr);
    assert(res == 0);
    // Wait for the child to start running.
    pthread_mutex_lock(&lock);
    while (!child_ready)
        pthread_cond_wait(&condvar, &lock);
    pthread_mutex_unlock(&lock);
    // Now take some time doing work so we can measure how many sleeps the child
    // accomplishes in this time period.
#ifdef DEBUG
    constexpr int EINTR_PERIOD = 30;
#else
    // Signal delivery is much faster, and we only want a few interruptions to test
    // that path: too many results in too many sleep syscalls in the count.
    constexpr int EINTR_PERIOD = 600;
#endif
    double val = static_cast<double>(MIN_SLEEPS);
    int i = 0;
    while (true) {
        if (computed_iters == 0) {
            if (sleep_count.load(std::memory_order_acquire) >= MIN_SLEEPS) {
                computed_iters = i;
                std::cerr << "iters for >= " << MIN_SLEEPS
                          << " sleeps: " << computed_iters << "\n";
                break;
            }
        } else if (i >= computed_iters &&
                   // We want to test the scaled (2nd run) EINTR path.
                   // We don't require on the 1st as that slows down
                   // debug test times (from 1.5s up to >10s if we require an
                   // EINTR: because there's so much other work being done it
                   // dwarfs the short app sleep) for no benefit.
                   // But in the inflated sleep run we hit EINTR pretty easily,
                   // so we're comfortable requiring it without worrying it
                   // will skew the results and make it seem there are more
                   // sleeps because we have more requirements (that would
                   // cause the test to move toward failure in any case and
                   // we would be more likely to notice).
                   saw_eintr.load(std::memory_order_acquire)) {
            break;
        }
        ++i;
        val += sin(val);
        // Test interrupting the thread's sleeps.
        if (!saw_eintr.load(std::memory_order_acquire) && i % EINTR_PERIOD == 0) {
            pthread_kill(thread, SIGUSR1);
        }
    }
    // Clean up.
    child_should_exit.store(true, std::memory_order_release);
    res = pthread_join(thread, &retval);
    assert(res == 0);
    pthread_cond_destroy(&condvar);
    pthread_mutex_destroy(&lock);
    return val;
}

/****************************************************************************
 * Trace processing code.
 */

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
    double res = do_some_work();
    assert(res > 0.);
    dr_app_stop_and_cleanup();
    assert(!dr_app_running_under_dynamorio());

    return post_process(out_subdir);
}

static int
count_sleeps(const std::string &dir)
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
    while (true) {
        memref_t memref;
        scheduler_t::stream_status_t status = stream->next_record(memref);
        if (status == scheduler_t::STATUS_EOF) {
            break;
        }
        assert(status == scheduler_t::STATUS_OK);
        if (memref.marker.type == TRACE_TYPE_MARKER &&
            memref.marker.marker_type == TRACE_MARKER_TYPE_SYSCALL &&
            (memref.marker.marker_value == SYS_nanosleep ||
             memref.marker.marker_value == SYS_clock_nanosleep)) {
            ++count;
        }
    }
    return count;
}

int
test_main(int argc, const char *argv[])
{
    // The first gather_trace call must be the default as computed_iters
    // is determined in the first call.
    std::string dir_default = gather_trace("", "default");
    std::string dir_scale = gather_trace("-scale_timeouts 20", "scale");

    void *dr_context = dr_standalone_init();

    int sleeps_default = count_sleeps(dir_default);
    int sleeps_scale = count_sleeps(dir_scale);
    std::cerr << "sleeps default=" << sleeps_default << " scale=" << sleeps_scale << "\n";
    // With a 10x scale, require at least a 2x difference (no higher, to allow
    // for variation on loaded test machines).
    assert(sleeps_default > 2 * sleeps_scale);

    dr_standalone_exit();
    std::cerr << "all done\n";
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
