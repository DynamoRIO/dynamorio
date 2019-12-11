/* **********************************************************
 * Copyright (c) 2012-2018 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
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

/* not really a header file, but we don't want to run this standalone
 * to use, defines these, then include this file:

#define BLOCK_IN_HANDLER 0
#define USE_SIGSTACK 0
#define USE_TIMER 0

#include "sigplain-base.h"
 *
 */

#include "tools.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <sys/time.h> /* itimer */
#include <string.h>   /*  memset */

#if USE_SIGSTACK
#    include <stdlib.h> /* malloc */
/* need more space if might get nested signals */
#    if USE_TIMER
#        define ALT_STACK_SIZE (SIGSTKSZ * 4)
#    else
#        define ALT_STACK_SIZE (SIGSTKSZ * 2)
#    endif
#endif

#if USE_TIMER
/* need to run long enough to get itimer hit */
#    define ITERS 3500000
#else
#    define ITERS 500000
#endif

static int a[ITERS];

/* strategy: anything that won't be the same across multiple runs,
 * hide inside #if VERBOSE.
 * timer hits won't be the same, just make sure we get at least one.
 */
#if USE_TIMER
static int timer_hits = 0;
#endif

#include <errno.h>

static void
signal_handler(int sig)
{
#if USE_SIGSTACK
    /* Ensure setting a new stack while on the current one fails with EPERM. */
    stack_t sigstack;
    sigstack.ss_sp = a; /* will fail: just need sthg */
    sigstack.ss_size = ALT_STACK_SIZE;
    sigstack.ss_flags = 0;
    int rc = sigaltstack(&sigstack, NULL);
    assert(rc == -1 && errno == EPERM);
#endif

#if USE_TIMER
    if (sig == SIGVTALRM)
        timer_hits++;
    else
#endif
        print("in signal handler\n");
}

/* set up signal_handler as the handler for signal "sig" */
static void
custom_intercept_signal(int sig, handler_1_t handler)
{
    int rc;
    struct sigaction act;

    act.sa_sigaction = (void (*)(int, siginfo_t *, void *))handler;
#if BLOCK_IN_HANDLER
    rc = sigfillset(&act.sa_mask); /* block all signals within handler */
#else
    rc = sigemptyset(&act.sa_mask); /* no signals are blocked within handler */
#endif
    ASSERT_NOERR(rc);
    act.sa_flags = SA_ONSTACK;

    /* arm the signal */
    rc = sigaction(sig, &act, NULL);
    ASSERT_NOERR(rc);
}

int
main(int argc, char *argv[])
{
    double res = 0.;
    int i, j, rc;
#if USE_SIGSTACK
    stack_t sigstack;
#endif
#if USE_TIMER
    struct itimerval t;
#endif

    /* Block a few signals */
    sigset_t mask = {
        0, /* Set padding to 0 so we can use memcmp */
    };
    sigemptyset(&mask);
    sigaddset(&mask, SIGURG);
    sigaddset(&mask, SIGALRM);
    rc = sigprocmask(SIG_SETMASK, &mask, NULL);
    ASSERT_NOERR(rc);

#if USE_SIGSTACK
    sigstack.ss_sp = (char *)malloc(ALT_STACK_SIZE);
    sigstack.ss_size = ALT_STACK_SIZE;
    sigstack.ss_flags = 0;
    rc = sigaltstack(&sigstack, NULL);
    ASSERT_NOERR(rc);
#    if VERBOSE
    print("Set up sigstack: 0x%08x - 0x%08x\n", sigstack.ss_sp,
          sigstack.ss_sp + sigstack.ss_size);
#    endif
#endif

#if USE_TIMER
    custom_intercept_signal(SIGVTALRM, signal_handler);
    t.it_interval.tv_sec = 0;
    t.it_interval.tv_usec = 20000;
    t.it_value.tv_sec = 0;
    t.it_value.tv_usec = 20000;
    rc = setitimer(ITIMER_VIRTUAL, &t, NULL);
    ASSERT_NOERR(rc);
#endif

    custom_intercept_signal(SIGSEGV, signal_handler);
    custom_intercept_signal(SIGUSR1, signal_handler);
    custom_intercept_signal(SIGUSR2, SIG_IGN);

    res = cos(0.56);

    print("Sending SIGUSR2\n");
    kill(getpid(), SIGUSR2);

    print("Sending SIGUSR1\n");
    kill(getpid(), SIGUSR1);

    for (i = 0; i < ITERS; i++) {
        if (i % 2 == 0) {
            res += cos(1. / (double)(i + 1));
        } else {
            res += sin(1. / (double)(i + 1));
        }
        j = (i << 4) / (i | 0x38);
        a[i] += j;
    }
    print("%f\n", res);

    sigset_t check_mask = {
        0, /* Set padding to 0 so we can use memcmp */
    };
    rc = sigprocmask(SIG_BLOCK, NULL, &check_mask);
    ASSERT_NOERR(rc);
    assert(memcmp(&mask, &check_mask, sizeof(mask)) == 0);

#if USE_TIMER
    memset(&t, 0, sizeof(t));
    rc = setitimer(ITIMER_VIRTUAL, &t, NULL);
    ASSERT_NOERR(rc);

    if (timer_hits == 0)
        print("Got 0 timer hits!\n");
    else
        print("Got some timer hits!\n");
#endif

        /* We leave the sigstack in place for the timer so any racy alarm arriving
         * after we disabled the itimer will be on the alt stack.
         */
#if USE_SIGSTACK && !USE_TIMER
    stack_t check_stack;
    rc = sigaltstack(NULL, &check_stack);
    ASSERT_NOERR(rc);
    assert(check_stack.ss_sp == sigstack.ss_sp &&
           check_stack.ss_size == sigstack.ss_size &&
           check_stack.ss_flags == sigstack.ss_flags);
    free(sigstack.ss_sp);
#endif
    return 0;
}
