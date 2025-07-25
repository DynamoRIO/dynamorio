/* **********************************************************
 * Copyright (c) 2025 Google, Inc.  All rights reserved.
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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
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
#include <cstdint>
#include <iostream>
#include <thread>

// This is set globally in CMake for other tests so easier to undef here.
#undef DR_REG_ENUM_COMPATIBILITY

#include "configure.h"
#include "dr_api.h"
#include "drx.h"
#include "tools.h"

#ifndef LINUX
#    error Only Linux supported for this test.
#endif

namespace dynamorio {
namespace drmemtrace {

#define VERBOSE 1
#if VERBOSE
#    define VPRINT(...) print(__VA_ARGS__)
#else
#    define VPRINT(...) /* nothing */
#endif

#ifndef X64
typedef struct _timespec64_t {
    int64 tv_sec;
    int64 tv_nsec;
} timespec64_t;
#endif

static pthread_cond_t condvar;
static bool child_ready;
static pthread_mutex_t lock;
static std::atomic<bool> child_should_exit;

bool
my_setenv(const char *var, const char *value)
{
    return setenv(var, value, 1 /*override*/) == 0;
}

static void
handler(int sig)
{
    // Nothing; just interrupt the sleep.
}

static void *
thread_routine(void *arg)
{
    bool clock_version = static_cast<bool>(reinterpret_cast<size_t>(arg));
    int64_t sleep_count = 0;

    signal(SIGUSR1, handler);

    pthread_mutex_lock(&lock);
    child_ready = true;
    pthread_cond_signal(&condvar);
    pthread_mutex_unlock(&lock);

    constexpr int SLEEP_NSEC = 100000;
    struct timespec sleeptime;
    sleeptime.tv_sec = 0;
    sleeptime.tv_nsec = SLEEP_NSEC;
    struct timespec remaining;
#ifndef X64
    timespec64_t sleeptime64;
    sleeptime64.tv_sec = 0;
    sleeptime64.tv_nsec = SLEEP_NSEC;
    timespec64_t remaining64;
#endif
    int64_t eintr_count = 0;
    while (!child_should_exit.load(std::memory_order_acquire)) {
        ++sleep_count;
        int res;
        if (clock_version) {
            // This is what modern libc nanosleep() calls.
#ifdef X64
            res = syscall(SYS_clock_nanosleep, CLOCK_REALTIME, 0, &sleeptime, &remaining);
#else
            res = syscall(SYS_clock_nanosleep_time64, CLOCK_REALTIME, 0, &sleeptime64,
                          &remaining64);
#endif
        } else {
            res = syscall(SYS_nanosleep, &sleeptime, &remaining);
        }
        if (res != 0) {
            assert(errno == EINTR);
            // Ensure the remaining time was deflated.
#ifndef X64
            if (clock_version)
                assert(remaining64.tv_sec <= sleeptime64.tv_sec);
            else
#endif
                assert(remaining.tv_sec <= sleeptime.tv_sec);
            ++eintr_count;
        }
    }
    assert(eintr_count > 0);
    std::cerr << "eintrs=" << eintr_count << "\n";
    return reinterpret_cast<void *>(sleep_count);
}

static int64_t
do_some_work(bool clock_version)
{
    pthread_t thread;
    void *retval;
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&condvar, NULL);

    pthread_mutex_lock(&lock);
    child_ready = false;
    pthread_mutex_unlock(&lock);
    child_should_exit.store(false, std::memory_order_release);

    int res = pthread_create(&thread, NULL, thread_routine,
                             reinterpret_cast<void *>(clock_version));
    assert(res == 0);
    // Wait for the child to start running.
    pthread_mutex_lock(&lock);
    while (!child_ready)
        pthread_cond_wait(&condvar, &lock);
    pthread_mutex_unlock(&lock);
    // Now take some time doing work so we can measure how many sleeps the child
    // accomplishes in this time period.
    constexpr int ITERS = 10000000;
    double val = static_cast<double>(ITERS);
    for (int i = 0; i < ITERS; ++i) {
        val += sin(val);
        // Test interrupting the thread's sleeps.
        if (i % (ITERS / 20) == 0 ||
            // Add a use of val so it's not optimized away.
            val == 0.) {
            pthread_kill(thread, SIGUSR1);
        }
    }
    // Clean up.
    child_should_exit.store(true, std::memory_order_release);
    res = pthread_join(thread, &retval);
    assert(res == 0);
    pthread_cond_destroy(&condvar);
    pthread_mutex_destroy(&lock);
    return reinterpret_cast<int64_t>(retval);
}

static void
event_exit(void)
{
    bool ok = drx_unregister_time_scaling();
    assert(ok);
    drx_exit();
    dr_fprintf(STDERR, "client done\n");
}

static int64_t
test_sleep(bool clock_version, int scale)
{
    std::string dr_ops("-stderr_mask 0xc -client_lib ';;" + std::to_string(scale) + "'");
    if (!my_setenv("DYNAMORIO_OPTIONS", dr_ops.c_str()))
        std::cerr << "failed to set env var!\n";
    dr_app_setup_and_start();
    int64_t count = do_some_work(clock_version);
    dr_app_stop_and_cleanup();
    return count;
}

static void
test_sleep_scale()
{
    int64_t sleeps_default = test_sleep(/*clock_version=*/true, 1);
    constexpr int SCALE = 100;
    int64_t sleeps_scaled = test_sleep(/*clock_version=*/true, SCALE);
    std::cerr << "sleeps default=" << sleeps_default << " clock scaled=" << sleeps_scaled
              << "\n";
    // Ensure the scaling ends up within an order of magnitude.
    assert(sleeps_default > SCALE / 10 * sleeps_scaled);
    // Test SYS_nanosleep.
    sleeps_scaled = test_sleep(/*clock_version=*/false, SCALE);
    std::cerr << "sleeps default=" << sleeps_default
              << " noclock scaled=" << sleeps_scaled << "\n";
    assert(sleeps_default > SCALE / 10 * sleeps_scaled);
}

} // namespace drmemtrace
} // namespace dynamorio

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    uint timeout_scale = 1;
    if (argc >= 2)
        timeout_scale = atoi(argv[1]);
    dr_fprintf(STDERR, "in dr_client_main scale=%u\n", timeout_scale);

    dr_register_exit_event(dynamorio::drmemtrace::event_exit);
    bool ok = drx_init();
    assert(ok);

    drx_time_scale_t scale = {
        sizeof(scale),
    };
    scale.timer_scale = 1;
    scale.timeout_scale = timeout_scale;
    ok = drx_register_time_scaling(&scale);
    assert(ok);
}

int
main(int argc, char *argv[])
{
    dynamorio::drmemtrace::test_sleep_scale();
    print("app done\n");
    return 0;
}
