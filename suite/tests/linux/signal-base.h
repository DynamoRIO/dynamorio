/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
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

/* Not really a header file, but we don't want to run this standalone.
 * To use, define these, then include this file:
#define USE_LONGJMP 0
#define BLOCK_IN_HANDLER 0
#define USE_SIGSTACK 0
#define USE_TIMER 0

#include "signal-base.h"
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

/* handler with SA_SIGINFO flag set gets three arguments: */
typedef void (*handler_t)(int, siginfo_t *, void *);

#if USE_LONGJMP
#    include <setjmp.h>
static sigjmp_buf env;
#endif

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

#ifdef AARCHXX
/* i#4719: Work around QEMU bugs where QEMU can't handle signals 63 or 64. */
#    undef SIGRTMAX
#    define SIGRTMAX 62
#    undef __SIGRTMAX
#    define __SIGRTMAX SIGRTMAX
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
#if defined(ARM) && !defined(USE_SIGSTACK)
    /* Test a variety of ISA transitions by tying this to USE_SIGSTACK. */
    __attribute__((target("arm")))
#endif
    signal_handler(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
#if VERBOSE
    print("signal_handler: sig=%d, retaddr=0x%08x, ucxt=0x%08x\n", sig, *(&sig - 1),
          ucxt);
#else
#    if USE_TIMER
    if (sig != SIGVTALRM)
#    endif
        print("in signal handler\n");
#endif

#if USE_SIGSTACK
    /* Ensure setting a new stack while on the current one fails with EPERM. */
    stack_t sigstack;
    sigstack.ss_sp = siginfo; /* will fail: just need sthg */
    sigstack.ss_size = ALT_STACK_SIZE;
    sigstack.ss_flags = SS_ONSTACK;
    int rc = sigaltstack(&sigstack, NULL);
    assert(rc == -1 && errno == EPERM);
#endif

    switch (sig) {

    case SIGSEGV: {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        void *pc = (void *)sc->SC_XIP;
#if VERBOSE
        print("Got SIGSEGV @ 0x%08x\n", pc);
#else
        print("Got SIGSEGV\n");
#endif
#if USE_LONGJMP
        siglongjmp(env, 1);
#endif
        break;
    }

    case SIGUSR1: {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        void *pc = (void *)sc->SC_XIP;
#if VERBOSE
        print("Got SIGUSR1 @ 0x%08x\n", pc);
#else
        print("Got SIGUSR1\n");
#endif
        break;
    }

#ifdef LINUX
    case __SIGRTMAX: {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        void *pc = (void *)sc->SC_XIP;
        /* SIGRTMAX has been 64 on Linux since kernel 2.1, from looking at glibc
         * sources. */
#    ifndef AARCHXX /* i#4719: Work around QEMU bugs handling signals 63,64. */
        assert(__SIGRTMAX == 64);
#    endif
        assert(__SIGRTMAX == SIGRTMAX);
#    if VERBOSE
        print("Got SIGRTMAX @ 0x%08x\n", pc);
#    else
        print("Got SIGRTMAX\n");
#    endif
        break;
    }
#endif

#if USE_TIMER
    case SIGVTALRM: {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        void *pc = (void *)sc->SC_XIP;
#    if VERBOSE
        print("Got SIGVTALRM @ 0x%08x\n", pc);
#    endif
        timer_hits++;
        break;
    }
#endif

    default: assert(0);
    }
}

/* set up signal_handler as the handler for signal "sig" */
static void
custom_intercept_signal(int sig, handler_t handler)
{
    int rc;
    struct sigaction act;

    act.sa_sigaction = handler;
#if BLOCK_IN_HANDLER
    rc = sigfillset(&act.sa_mask); /* block all signals within handler */
#else
    rc = sigemptyset(&act.sa_mask); /* no signals are blocked within handler */
#endif
    ASSERT_NOERR(rc);
    act.sa_flags = SA_SIGINFO | SA_ONSTACK; /* send 3 args to handler */

    /* arm the signal */
    rc = sigaction(sig, &act, NULL);
    ASSERT_NOERR(rc);
}

int
#if defined(ARM) && !defined(BLOCK_IN_HANDLER)
    /* Test a variety of ISA transitions by tying this to BLOCK_IN_HANDLER. */
    __attribute__((target("arm")))
#endif
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
    custom_intercept_signal(SIGVTALRM, (handler_t)signal_handler);
    t.it_interval.tv_sec = 0;
    t.it_interval.tv_usec = 10000;
    t.it_value.tv_sec = 0;
    t.it_value.tv_usec = 10000;
    rc = setitimer(ITIMER_VIRTUAL, &t, NULL);
    ASSERT_NOERR(rc);
#endif

    custom_intercept_signal(SIGSEGV, (handler_t)signal_handler);
    custom_intercept_signal(SIGUSR1, (handler_t)signal_handler);
    custom_intercept_signal(SIGUSR2, (handler_t)SIG_IGN);
#ifdef LINUX
    custom_intercept_signal(__SIGRTMAX, (handler_t)signal_handler);
#endif

    res = cos(0.56);

    print("Sending SIGUSR2\n");
    kill(getpid(), SIGUSR2);

    print("Sending SIGUSR1\n");
    kill(getpid(), SIGUSR1);

#ifdef LINUX
    print("Sending SIGRTMAX\n");
    kill(getpid(), SIGRTMAX);
#else
    /* Match same output */
    print("Sending SIGRTMAX\n");
    print("in signal handler\n");
    print("Got SIGRTMAX\n");
#endif

    print("Generating SIGSEGV\n");
#if USE_LONGJMP
    res = sigsetjmp(env, 1);
    if (res == 0) {
        *((volatile int *)0) = 4;
    }
#else
    kill(getpid(), SIGSEGV);
#endif

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
