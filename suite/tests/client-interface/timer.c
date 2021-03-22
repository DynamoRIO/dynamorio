/* **********************************************************
 * Copyright (c) 2012-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2010 VMware, Inc.  All rights reserved.
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

#include "tools.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <sys/time.h> /* itimer */
#include <time.h>     /* for nanosleep */

/* test PR 204556: support DR+client itimers in presence of app itimers
 * and  i#283/PR 368737: add client timer support
 */

static void
signal_handler(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (sig == SIGALRM)
        print("app got SIGALRM\n");
    else
        assert(0);
}

static int
func1(int x)
{
    return (x > 0 ? 4 * x : x / 4);
}

static int
func2(int x)
{
    return (x < 0 ? 4 * x : x / 4);
}

int
main(int argc, char *argv[])
{
    int rc;
    struct itimerval t;
    intercept_signal(SIGALRM, signal_handler, false);
    t.it_interval.tv_sec = 0;
    t.it_interval.tv_usec = 10000;
    t.it_value.tv_sec = 0;
    t.it_value.tv_usec = 10000;
    rc = setitimer(ITIMER_REAL, &t, NULL);
    assert(rc == 0);

    /* Do some work so we're in fragments more often.
     * We run with -disable_traces and include a lot of indirect transfers here to
     * hit i#4669 on translating bb prefixes.
     */
    double sum = 0.;
    for (int i = 0; i < 1000; ++i) {
        for (int j = 0; j < 1000; ++j) {
            int (*func)(int) = i < j ? func2 : func1;
            sum += (i % 2 == 0) ? ((double)func(j) / 43) : (func(j) - func(i * 6));
            if (func(i) < 0)
                func = func1;
            else if (func(j) > 0)
                func = func2;
            else
                func = func1;
            sum *= (double)(func(i) - (func(j) + func(1)));
        }
    }

    struct timespec sleeptime;
    sleeptime.tv_sec = 0;
    sleeptime.tv_nsec = 25 * 1000 * 1000; /* 25ms */
    /* Doing a few more syscalls makes the test more reliable than one long
     * sleep, since we hit d_r_dispatch more often.
     */
    nanosleep(&sleeptime, NULL);
    nanosleep(&sleeptime, NULL);
    nanosleep(&sleeptime, NULL);
    nanosleep(&sleeptime, NULL);
    nanosleep(&sleeptime, NULL);
    nanosleep(&sleeptime, NULL);
    nanosleep(&sleeptime, NULL);

    return 0;
}
