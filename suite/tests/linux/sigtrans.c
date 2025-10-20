/* **********************************************************
 * Copyright (c) 2025 Arm Limited. All rights reserved.
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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Target a block of instructions that use the stolen register with a
 * synchronous signal to check that the app state is correctly
 * recreated and the instruction is not run a second time after
 * returning from the signal handler.
 */

#include "tools.h"

#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

/* The signal handler will copy src to dst. */
static volatile int volatile_src;
static volatile int volatile_dst;

void
handler(int signum, siginfo_t *info, void *ucontext)
{
    volatile_dst = volatile_src;
}

void
fail(const char *s)
{
    perror(s);
    exit(1);
}

/* XXX: These initial values must be large enough to avoid i#7675, but
 * should probably be reduced when that bug is fixed.
 */
int iters = 1000;
unsigned long init_param = 10000;

bool
test_code(int *adjust)
{
    unsigned long count = 0;
    for (int i = 0; i < iters * 2; i++) {
        /* XXX: Port to other (non-x86) architectures. */
        asm volatile("mov x28, %[count] \n\t"
#define INC "add x28, x28, #1 \n\t"
                     INC INC INC INC INC INC INC INC
#undef INC
                     "mov %[count], x28"
                     : [count] "+r"(count)
                     :
                     : "x28");
        volatile_src = i + 2;
    }
    /* We aim for volatile_dst to equal iters, but we never set *adjust
     * to zero because we want the timer delay to be constantly
     * adjusted.
     */
    int dst = volatile_dst;
    *adjust = (dst == 0 ? iters * 2 + 1 : dst < iters ? dst - iters : dst - iters + 1);
    return count != iters * 2 * 8;
}

bool
try(int *adjust, unsigned long long param, void *arg)
{
    if (param == 1) {
        /* Perhaps we need a larger number of iterations. */
        iters = iters * 2 != 0 ? iters * 2 : iters;
    }

    timer_t timer = *(timer_t *)arg;

    volatile_dst = 0;
    volatile_src = 1;

    /* Set timer. */
    struct itimerspec spec = { { 0, 0 }, { param / 1000000000, param % 1000000000 } };
    if (timer_settime(timer, 0, &spec, NULL))
        fail("timer_settime");

    /* Run test code. */
    bool hit = test_code(adjust);

    /* Cancel timer. */
    struct itimerspec spec0 = { { 0, 0 }, { 0, 0 } };
    struct itimerspec old_spec;
    if (timer_settime(timer, 0, &spec0, &old_spec))
        fail("timer_settime");

    return hit;
}

int
main()
{
    /* This test relies on SIGPIPE, delivered by a timer, being
     * treated as a synchronous signal, like a fault and may become
     * useless if that ceases to be true.
     */
    int signum = SIGPIPE;

    /* Set up signal handler. */
    struct sigaction act = { 0 };
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = handler;
    if (sigaction(signum, &act, 0) != 0)
        fail("sigaction");

    /* Create timer. */
    struct sigevent sevp = { 0 };
    sevp.sigev_notify = SIGEV_SIGNAL;
    sevp.sigev_signo = signum;
    timer_t timer;
    if (timer_create(CLOCK_MONOTONIC, &sevp, &timer))
        fail("timer_create");

    if (adaptive_retry(try, 1000, init_param, &timer, true)) {
        print("failed\n");
        return 1;
    }
    print("all done\n");
    return 0;
}
