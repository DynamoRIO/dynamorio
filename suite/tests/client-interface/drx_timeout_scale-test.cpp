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
#include <linux/futex.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
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

static void *
thread_routine(void *arg)
{
    int64_t futex_count = 0;

    pthread_mutex_lock(&lock);
    child_ready = true;
    pthread_cond_signal(&condvar);
    pthread_mutex_unlock(&lock);

    // We perform a futex that will always time out as our value never changes.
    constexpr int FUTEX_VAL = 0xabcd;
    int32_t futex_var = FUTEX_VAL;
    constexpr int FUTEX_NSEC = 100000;
    struct timespec timeout_default;
    timeout_default.tv_sec = 0;
    timeout_default.tv_nsec = FUTEX_NSEC;
    struct timespec timeout_zero;
    timeout_zero.tv_sec = 0;
    timeout_zero.tv_nsec = 0;
    while (!child_should_exit.load(std::memory_order_acquire)) {
        // Test a zero timeout.
        struct timespec *timeout = &timeout_default;
        if (futex_count == 0)
            timeout = &timeout_zero;

        // Test a relative timeout.
        int res = syscall(SYS_futex, &futex_var, FUTEX_WAIT, FUTEX_VAL, timeout,
                          /*uaddr2=*/nullptr, /*val3=*/0);
        assert(res != 0 && errno == ETIMEDOUT);

        // Test an absolute timeout.
        struct timespec cur_time;
        // Test both realtime and monotonic clocks.
        bool realtime = futex_count % 2 == 0;
        res = clock_gettime(realtime ? CLOCK_REALTIME : CLOCK_MONOTONIC, &cur_time);
        assert(res == 0);
        constexpr int MAX_TV_NSEC = 1000000000ULL;
        int64_t cur_nanos =
            static_cast<int64_t>(cur_time.tv_sec) * MAX_TV_NSEC + cur_time.tv_nsec;
        int64_t target_nanos = cur_nanos + FUTEX_NSEC;
        struct timespec timeout_abs;
        timeout_abs.tv_sec = target_nanos / MAX_TV_NSEC;
        timeout_abs.tv_nsec = target_nanos % MAX_TV_NSEC;
        res = syscall(SYS_futex, &futex_var,
                      FUTEX_WAIT_BITSET | (realtime ? FUTEX_CLOCK_REALTIME : 0),
                      FUTEX_VAL, &timeout_abs,
                      /*uaddr2=*/nullptr, FUTEX_BITSET_MATCH_ANY);
        assert(res != 0 && errno == ETIMEDOUT);

        ++futex_count;
    }
    return reinterpret_cast<void *>(futex_count);
}

static int64_t
do_some_work()
{
    pthread_t thread;
    void *retval;
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&condvar, NULL);

    pthread_mutex_lock(&lock);
    child_ready = false;
    pthread_mutex_unlock(&lock);
    child_should_exit.store(false, std::memory_order_release);

    int res = pthread_create(&thread, NULL, thread_routine, NULL);
    assert(res == 0);
    // Wait for the child to start running.
    pthread_mutex_lock(&lock);
    while (!child_ready)
        pthread_cond_wait(&condvar, &lock);
    pthread_mutex_unlock(&lock);
    // Now take some time doing work so we can measure how many futexes the child
    // accomplishes in this time period.
    constexpr int ITERS = 10000000;
    double val = static_cast<double>(ITERS);
    for (int i = 0; i < ITERS; ++i) {
        val += sin(val);
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
    drx_time_scale_stat_t *stats;
    bool ok = drx_get_time_scaling_stats(&stats);
    assert(ok);
    for (int i = 0; i < DRX_SCALE_STAT_TYPES; ++i) {
        dr_fprintf(STDERR,
                   "type %d: attempt " SZFMT " fail " SZFMT " nop " SZFMT
                   " was-zero " SZFMT "\n",
                   i, stats[i].count_attempted, stats[i].count_failed, stats[i].count_nop,
                   stats[i].count_zero_to_nonzero);
    }
    assert(stats[DRX_SCALE_FUTEX].count_attempted > 0);
    assert(stats[DRX_SCALE_FUTEX].count_attempted >=
           stats[DRX_SCALE_FUTEX].count_failed + stats[DRX_SCALE_FUTEX].count_nop);
    assert(stats[DRX_SCALE_FUTEX].count_failed == 0);
    // Either scale was 1 and everything is a nop, or if scaling then our 0-duration
    // futex should have become non-0.
    assert(stats[DRX_SCALE_FUTEX].count_nop == stats[DRX_SCALE_FUTEX].count_attempted ||
           stats[DRX_SCALE_FUTEX].count_nop == 0);
    assert(stats[DRX_SCALE_FUTEX].count_zero_to_nonzero > 0);

    ok = drx_unregister_time_scaling();
    assert(ok);
    drx_exit();
    dr_fprintf(STDERR, "client done\n");
}

static int64_t
test_futex(int scale)
{
    std::string dr_ops("-stderr_mask 0xc -client_lib ';;" + std::to_string(scale) + "'");
    if (!my_setenv("DYNAMORIO_OPTIONS", dr_ops.c_str()))
        std::cerr << "failed to set env var!\n";
    dr_app_setup_and_start();
    int64_t count = do_some_work();
    dr_app_stop_and_cleanup();
    return count;
}

static void
test_futex_scale()
{
    int64_t futexes_default = test_futex(/*scale=*/1);
    constexpr int SCALE = 100;
    int64_t futexes_scaled = test_futex(SCALE);
    std::cerr << "futexes default=" << futexes_default << " scaled=" << futexes_scaled
              << "\n";
    // Ensure the scaling ends up within an order of magnitude.
    assert(futexes_default > SCALE / 10 * futexes_scaled);
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
    dynamorio::drmemtrace::test_futex_scale();
    print("app done\n");
    return 0;
}
