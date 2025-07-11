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
#include <signal.h>
#include <math.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#include "configure.h"
#include "dr_api.h"
#include "drx.h"
#include "tools.h"

#define VERBOSE 1
#if VERBOSE
#    define VPRINT(...) print(__VA_ARGS__)
#else
#    define VPRINT(...) /* nothing */
#endif

enum {
    TIMER_ITIMER_REAL,
    TIMER_ITIMER_VIRTUAL,
    TIMER_ITIMER_PROF,
    TIMER_POSIX_REAL,
    TIMER_POSIX_CPU,
    TIMER_TYPE_COUNT,
};

static const int ITIMER_TYPES[] = { ITIMER_REAL, ITIMER_VIRTUAL, ITIMER_PROF };
static const int SIGNAL_TYPES[] = { SIGALRM, SIGVTALRM, SIGPROF };

const int INTERVAL_MICROSEC = 10000;
const int INTERVAL_NANOSEC = 10000000;

static const uint SCALE = 10;
/* Ideally we'd see a 10x difference but we have to allow for wide variations
 * in test machine load to avoid flakiness.
 */
static const uint MIN_PASSING_SCALE = 2;
/* This test is single-threaded so we do not use atomics. */
static volatile int count[TIMER_TYPE_COUNT];
static timer_t posix_real_id;
static timer_t posix_cpu_id;

static void
signal_handler(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (sig == SIGALRM)
        ++count[TIMER_ITIMER_REAL];
    else if (sig == SIGVTALRM)
        ++count[TIMER_ITIMER_VIRTUAL];
    else if (sig == SIGPROF)
        ++count[TIMER_ITIMER_PROF];
    else if (sig == SIGUSR1)
        ++count[TIMER_POSIX_REAL];
    else if (sig == SIGUSR2)
        ++count[TIMER_POSIX_CPU];
    else
        assert(0);
}

static void
do_some_work(void)
{
    // Occupy time so we get SIGVTALRM and SIGPROF.
    static const int ITERS = 10000000;
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
}

static void
create_posix_timers(void)
{
    /* Create two POSIX timers to stress different varieties. */
    struct sigevent se = {};
    se.sigev_notify = SIGEV_THREAD_ID;
#ifndef sigev_notify_thread_id
#    define sigev_notify_thread_id _sigev_un._tid
#endif
    se.sigev_notify_thread_id = syscall(SYS_gettid);
    intercept_signal(SIGUSR1, signal_handler, false);
    se.sigev_signo = SIGUSR1;
    int res = timer_create(CLOCK_REALTIME, &se, &posix_real_id);
    assert(res == 0);

    se.sigev_notify = SIGEV_SIGNAL;
    intercept_signal(SIGUSR2, signal_handler, false);
    se.sigev_signo = SIGUSR2;
    res = timer_create(CLOCK_PROCESS_CPUTIME_ID, &se, &posix_cpu_id);
    assert(res == 0);
}

static void
enable_timers(void)
{
    /* Test every type of itimer and some POSIX timers. */
    for (int i = 0; i < TIMER_TYPE_COUNT; ++i)
        count[i] = 0;
    int res;
    struct itimerval val;
    val.it_interval.tv_sec = 0;
    val.it_interval.tv_usec = INTERVAL_MICROSEC;
    val.it_value.tv_sec = 0;
    val.it_value.tv_usec = INTERVAL_MICROSEC;
    for (int i = 0; i < sizeof(ITIMER_TYPES) / sizeof(ITIMER_TYPES[0]); ++i) {
        intercept_signal(SIGNAL_TYPES[i], signal_handler, false);
        bool called_in_asm = false;
#ifdef X86_64
        if (i == 0) {
            /* We check one with asm to ensure param registers are restored;
             * just one to ensure any libc wrapping is also covered in the others.
             */
            called_in_asm = true;
            struct itimerval *postsys_val = NULL, *val_ptr = &val;
            __asm__ __volatile__("mov %[itype], %%rdi\n\t"
                                 "mov %[val_ptr], %%rsi\n\t"
                                 "mov $0, %%edx\n\t"
                                 "mov %[sysnum], %%eax\n\t"
                                 "syscall\n\t"
                                 "mov %%eax, %[result]\n\t"
                                 "mov %%rsi, %[param_val]\n\t"
                                 : [result] "=m"(res), [param_val] "=m"(postsys_val)
                                 : [itype] "m"(ITIMER_TYPES[i]), [val_ptr] "m"(val_ptr),
                                   [sysnum] "i"(SYS_setitimer)
                                 : "rdi", "rsi", "rdx", "memory");
            assert(res == 0);
            assert(postsys_val == &val);
        }
#endif
        if (!called_in_asm) {
            res = setitimer(ITIMER_TYPES[i], &val, NULL);
            assert(res == 0);
        }
    }
    struct itimerspec spec;
    spec.it_interval.tv_sec = 0;
    spec.it_interval.tv_nsec = INTERVAL_NANOSEC;
    spec.it_value.tv_sec = 0;
    spec.it_value.tv_nsec = INTERVAL_NANOSEC;
    res = timer_settime(posix_real_id, 0, &spec, NULL);
    assert(res == 0);
    res = timer_settime(posix_cpu_id, 0, &spec, NULL);
    assert(res == 0);
}

static void
disable_timers(void)
{
    int res;
    struct itimerval val = {};
    for (int i = 0; i < sizeof(ITIMER_TYPES) / sizeof(ITIMER_TYPES[0]); ++i) {
        /* Ensure the value is what we set. */
        struct itimerval read_val = {};
        res = getitimer(ITIMER_TYPES[i], &read_val);
        assert(res == 0);
        assert(read_val.it_interval.tv_sec == 0);
        assert(read_val.it_interval.tv_usec == INTERVAL_MICROSEC);
        /* Now disable. */
        res = setitimer(ITIMER_TYPES[i], &val, NULL);
        assert(res == 0);
    }
    /* Ensure the values are what we set. */
    struct itimerspec read_spec = {};
    res = timer_gettime(posix_real_id, &read_spec);
    assert(res == 0);
    assert(read_spec.it_interval.tv_sec == 0);
    assert(read_spec.it_interval.tv_nsec == INTERVAL_NANOSEC);
    res = timer_gettime(posix_cpu_id, &read_spec);
    assert(res == 0);
    assert(read_spec.it_interval.tv_sec == 0);
    assert(read_spec.it_interval.tv_nsec == INTERVAL_NANOSEC);
    /* Now disable. */
    struct itimerspec spec = {};
    res = timer_settime(posix_real_id, 0, &spec, NULL);
    assert(res == 0);
    res = timer_settime(posix_cpu_id, 0, &spec, NULL);
    assert(res == 0);
}

static void
event_exit(void)
{
    bool ok = drx_unregister_time_scaling();
    assert(ok);
    drx_exit();
    dr_fprintf(STDERR, "client done\n");
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    uint timer_scale = 1;
    if (argc >= 2)
        timer_scale = atoi(argv[1]);
    dr_fprintf(STDERR, "in dr_client_main scale=%u\n", timer_scale);

    dr_register_exit_event(event_exit);
    bool ok = drx_init();
    assert(ok);

    drx_time_scale_t scale = {
        sizeof(scale),
    };
    scale.timer_scale = timer_scale;
    scale.timeout_scale = 1;
    ok = drx_register_time_scaling(&scale);
    assert(ok);
}

int
main(int argc, char *argv[])
{
    /* Setup. */
    create_posix_timers();

    /* First, get the expected counts under DR with no scaling. */
    VPRINT("\nGetting original timer counts\n");
    /* TODO i#7542: Once we resolve the no-client issue, change this reference
     * run to be plain DR with no client. For now we use a client with a scale
     * of 1 to avoid the i#7542 signal issue.
     */
    if (!my_setenv("DYNAMORIO_OPTIONS", "-stderr_mask 0xc -client_lib ';;1'"))
        print("failed to set env var!\n");
    int orig_count[TIMER_TYPE_COUNT];
    dr_app_setup_and_start();
    enable_timers();
    /* We can't just sleep as that will only trigger ITIMER_REAL: the others
     * require actual cpu time.
     */
    do_some_work();
    disable_timers();
    dr_app_stop_and_cleanup();
    for (int i = 0; i < TIMER_TYPE_COUNT; ++i)
        orig_count[i] = count[i];

    /* Test scaling pre-existing timers. */
    VPRINT("\nTesting pre-existing timers with scale %u\n", SCALE);
    char buf[64];
    dr_snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "-stderr_mask 0xc -client_lib ';;%u'",
                SCALE);
    NULL_TERMINATE_BUFFER(buf);
    if (!my_setenv("DYNAMORIO_OPTIONS", buf))
        print("failed to set env var!\n");
    enable_timers();
    dr_app_setup_and_start();
    do_some_work();
    disable_timers();
    dr_app_stop_and_cleanup();
    for (int i = 0; i < TIMER_TYPE_COUNT; ++i) {
        print("Counter #%d: orig=%d scaled=%d\n", i, orig_count[i], count[i]);
        assert(count[i] * MIN_PASSING_SCALE < orig_count[i]);
    }

    /* Test scaling timers added after attach. */
    VPRINT("\nTesting later-added timers with scale %u\n", SCALE);
    dr_app_setup_and_start();
    enable_timers();
    do_some_work();
    disable_timers();
    dr_app_stop_and_cleanup();
    for (int i = 0; i < TIMER_TYPE_COUNT; ++i) {
        print("Counter #%d: orig=%d scaled=%d\n", i, orig_count[i], count[i]);
        assert(count[i] * MIN_PASSING_SCALE < orig_count[i]);
    }

    /* Test scaling timers that start before and end after DR. */
    VPRINT("\nTesting spanning timers with scale %u\n", SCALE);
    enable_timers();
    dr_app_setup_and_start();
    do_some_work();
    dr_app_stop_and_cleanup();
    disable_timers();
    for (int i = 0; i < TIMER_TYPE_COUNT; ++i) {
        print("Counter #%d: orig=%d scaled=%d\n", i, orig_count[i], count[i]);
        assert(count[i] * MIN_PASSING_SCALE < orig_count[i]);
    }

    print("app done\n");
    return 0;
}
