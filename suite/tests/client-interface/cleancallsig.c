/* **********************************************************
 * Copyright (c) 2018 Google, Inc.  All rights reserved.
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

/* Test delivering async signals that interrupt clean call and IBL gencode.
 * It is not easy to hit some of the corner cases reliably: changes to
 * the signal code requires running this test in a loop to hit everything.
 */

static volatile int num_sigs;

static void
signal_handler(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (sig == SIGALRM) {
        num_sigs++;
    } else
        assert(0);
}

static int
foo(int x)
{
    if (x == 0)
        return x;
    return x + 1;
}

int
main(int argc, char *argv[])
{
    int rc;
    struct itimerval t;
    intercept_signal(SIGALRM, signal_handler, false);
    t.it_interval.tv_sec = 0;
    t.it_interval.tv_usec = 5000;
    t.it_value.tv_sec = 0;
    t.it_value.tv_usec = 5000;
    rc = setitimer(ITIMER_REAL, &t, NULL);
    assert(rc == 0);

    /* Now spend time doing indirect branches to try and stress signals interrupting
     * the IBL.
     */
    int i;
    int (*foo_ptr)(int) = foo;
#define WAIT_FOR_NUM_SIGS 250
    while (num_sigs < WAIT_FOR_NUM_SIGS) {
        rc += (*foo_ptr)(i);
    }

    print("all done\n");
    return rc > 0;
}
