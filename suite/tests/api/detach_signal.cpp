/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

/* XXX: We undef this b/c it's easier than getting rid of from CMake with the
 * global cflags config where all the other tests want this set.
 */
#undef DR_REG_ENUM_COMPATIBILITY

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <atomic>
#include "configure.h"
#include "dr_api.h"
#include "tools.h"
#include "thread.h"
#include "condvar.h"

#define VERBOSE 0
#define NUM_THREADS 10

#if VERBOSE
#    define VPRINT(...) print(__VA_ARGS__)
#else
#    define VPRINT(...) /* nothing */
#endif

/* SIGSTKSZ*2 results in a fatal error from DR on fitting the copied frame. */
#define ALT_STACK_SIZE (SIGSTKSZ * 4)

#ifdef MACOS
#    define DR_SUSPEND_SIGNAL SIGFPE /* DR's takeover signal. */
#else
#    define DR_SUSPEND_SIGNAL SIGILL /* DR's takeover signal. */
#endif

static volatile bool sideline_exit = false;
static void *sideline_continue;
static void *sideline_ready[NUM_THREADS];

static thread_local SIGJMP_BUF mark;
static std::atomic<int> count;

static sigset_t handler_mask;

static void
handle_signal(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    /* Ensure the mask within the handler is correct. */
    sigset_t actual_mask = {
        0, /* Set padding to 0 so we can use memcmp */
    };
    int res = sigprocmask(SIG_BLOCK, NULL, &actual_mask);
    assert(res == 0);
    sigset_t expect_mask1;
    memcpy(&expect_mask1, &handler_mask, sizeof(expect_mask1));
    sigaddset(&expect_mask1, signal);
    sigaddset(&expect_mask1, SIGUSR1);
    sigaddset(&expect_mask1, SIGURG);
    /* We also have init-time signal tests with a different mask. */
    sigset_t expect_mask2;
    memcpy(&expect_mask2, &handler_mask, sizeof(expect_mask2));
    sigaddset(&expect_mask2, signal);
    sigaddset(&expect_mask2, DR_SUSPEND_SIGNAL);
    assert(memcmp(&expect_mask1, &actual_mask, sizeof(expect_mask1)) == 0 ||
           memcmp(&expect_mask2, &actual_mask, sizeof(expect_mask2)) == 0);

    count++;
    SIGLONGJMP(mark, count);
}

THREAD_FUNC_RETURN_TYPE
sideline_spinner(void *arg)
{
    unsigned int idx = (unsigned int)(uintptr_t)arg;
    if (dr_app_running_under_dynamorio())
        print("ERROR: thread %d should NOT be under DynamoRIO\n", idx);

    if (idx == 0) {
        /* Delay attach to help test i#4640 where a signal arrives in a native thread
         * during DR takeover.
         */
        sigset_t delay_attach_mask;
        sigemptyset(&delay_attach_mask);
        sigaddset(&delay_attach_mask, DR_SUSPEND_SIGNAL);
        int res = sigprocmask(SIG_SETMASK, &delay_attach_mask, NULL);
        assert(res == 0);
    }

    VPRINT("%d signaling sideline_ready\n", idx);
    signal_cond_var(sideline_ready[idx]);

    if (idx == 0) {
        /* Spend some time generating signals while DR_SUSPEND_SIGNAL is blocked to try
         * and generate some after DR starts takeover and puts its own handler in place,
         * but before it can take us over.
         */
        for (int i = 0; i < 10000; i++) {
            if (SIGSETJMP(mark) == 0) {
                *(int *)arg = 42; /* SIGSEGV */
            }
            if (SIGSETJMP(mark) == 0) {
                pthread_kill(pthread_self(), SIGURG);
            }
        }
        sigset_t clear_mask;
        sigemptyset(&clear_mask);
        int res = sigprocmask(SIG_SETMASK, &clear_mask, NULL);
        assert(res == 0);
    }

    VPRINT("%d waiting for continue\n", idx);
    wait_cond_var(sideline_continue);
    if (!dr_app_running_under_dynamorio())
        print("ERROR: thread %d should be under DynamoRIO\n", idx);
    VPRINT("%d signaling sideline_ready\n", idx);
    signal_cond_var(sideline_ready[idx]);

    stack_t sigstack;
    sigstack.ss_sp = (char *)malloc(ALT_STACK_SIZE);
    sigstack.ss_size = ALT_STACK_SIZE;
    sigstack.ss_flags = 0;
    int res = sigaltstack(&sigstack, NULL);
    assert(res == 0);

    /* Block some signals to test mask preservation. */
    sigset_t mask = {
        0, /* Set padding to 0 so we can use memcmp */
    };
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGURG);
    res = sigprocmask(SIG_SETMASK, &mask, NULL);
    assert(res == 0);

    /* Now sit in a signal-generating loop. */
    while (!sideline_exit) {
        /* We generate 4 different signals to test different types. */
        if (SIGSETJMP(mark) == 0) {
            *(int *)arg = 42; /* SIGSEGV */
        }
        if (SIGSETJMP(mark) == 0) {
            pthread_kill(pthread_self(), SIGBUS);
        }
        if (SIGSETJMP(mark) == 0) {
            pthread_kill(pthread_self(), SIGURG);
        }
        if (SIGSETJMP(mark) == 0) {
            pthread_kill(pthread_self(), SIGALRM);
        }
        sigset_t check_mask = {
            0, /* Set padding to 0 so we can use memcmp */
        };
        res = sigprocmask(SIG_BLOCK, NULL, &check_mask);
        assert(res == 0 && memcmp(&mask, &check_mask, sizeof(mask)) == 0);
    }

    stack_t check_stack;
    res = sigaltstack(NULL, &check_stack);
    assert(res == 0 && check_stack.ss_sp == sigstack.ss_sp &&
           check_stack.ss_size == sigstack.ss_size &&
           check_stack.ss_flags == sigstack.ss_flags);
    sigstack.ss_flags = SS_DISABLE;
    res = sigaltstack(&sigstack, NULL);
    assert(res == 0);
    free(sigstack.ss_sp);

    return THREAD_FUNC_RETURN_ZERO;
}

static void
intercept_signal_with_mask(int sig, handler_3_t handler, bool sigstack, sigset_t *mask)
{
    int res;
    struct sigaction act;
    act.sa_sigaction = (void (*)(int, siginfo_t *, void *))handler;
    memcpy(&act.sa_mask, mask, sizeof(act.sa_mask));
    act.sa_flags = SA_SIGINFO;
    if (sigstack)
        act.sa_flags |= SA_ONSTACK;
    res = sigaction(sig, &act, NULL);
    ASSERT_NOERR(res);
}

int
main(void)
{
    int i;
    thread_t thread[NUM_THREADS]; /* On Linux, the tid. */

    sigset_t prior_mask;
    sigemptyset(&handler_mask);
    sigaddset(&handler_mask, DR_SUSPEND_SIGNAL);
    int res = sigprocmask(SIG_SETMASK, &handler_mask, &prior_mask);
    assert(res == 0);
    res = sigprocmask(SIG_SETMASK, &prior_mask, NULL);
    assert(res == 0);

    /* We request an alt stack for some signals but not all to test both types. */
    intercept_signal_with_mask(SIGSEGV, (handler_3_t)&handle_signal, true, &handler_mask);
    intercept_signal_with_mask(SIGBUS, (handler_3_t)&handle_signal, false, &handler_mask);
    intercept_signal_with_mask(SIGURG, (handler_3_t)&handle_signal, true, &handler_mask);
    intercept_signal_with_mask(SIGALRM, (handler_3_t)&handle_signal, false,
                               &handler_mask);

    sideline_continue = create_cond_var();
    for (i = 0; i < NUM_THREADS; i++) {
        sideline_ready[i] = create_cond_var();
        thread[i] = create_thread(sideline_spinner, (void *)(uintptr_t)i);
    }

    /* Initialize DR. */
    dr_app_setup();

    /* Wait for all the threads to be scheduled. */
    VPRINT("waiting for ready\n");
    for (i = 0; i < NUM_THREADS; i++) {
        wait_cond_var(sideline_ready[i]);
        reset_cond_var(sideline_ready[i]);
    }
    /* Now get each thread to start its signal loop. */
    dr_app_start();
    VPRINT("signaling continue\n");
    signal_cond_var(sideline_continue);
    VPRINT("waiting for ready\n");
    for (i = 0; i < NUM_THREADS; i++) {
        wait_cond_var(sideline_ready[i]);
        reset_cond_var(sideline_ready[i]);
    }
    reset_cond_var(sideline_continue);

    /* Detach */
    int pre_count = count.load(std::memory_order_acquire);
    print("signal count pre-detach: %d\n", pre_count);
    print("detaching\n");
    /* We use the _with_stats variant to catch register errors such as i#4457. */
    dr_stats_t stats = { sizeof(dr_stats_t) };
    dr_app_stop_and_cleanup_with_stats(&stats);
    int post_count = count.load(std::memory_order_acquire);
    assert(post_count > pre_count);
    print("signal count post-detach: %d\n", count.load(std::memory_order_acquire));
    assert(stats.basic_block_count > 0);
    print("native signals delivered: %ld\n", stats.num_native_signals);
    assert(stats.num_native_signals > 0);

    sideline_exit = true;
    for (i = 0; i < NUM_THREADS; i++) {
        join_thread(thread[i]);
    }

    destroy_cond_var(sideline_continue);
    for (i = 0; i < NUM_THREADS; i++)
        destroy_cond_var(sideline_ready[i]);

    print("All done\n");
    return 0;
}
