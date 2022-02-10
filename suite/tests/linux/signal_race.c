/* **********************************************************
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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

/* Repeatedly set a short-duration timer, adjusting it to arrive immediately
 * after the return from the system call. This can expose a race condition.
 */

#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "tools.h"

volatile int polling_started;
volatile int signal_arrived;
timer_t timer;

static void
fail(const char *s)
{
    perror(s);
    exit(1);
}

static void
handler(int signum)
{
    signal_arrived = 1;
}

static void
setup(void)
{
    struct sigaction act;
    struct sigevent sevp;

    memset(&act, 0, sizeof(act));
    act.sa_handler = handler;
    if (sigaction(SIGUSR1, &act, 0) != 0)
        fail("sigaction");

    memset(&sevp, 0, sizeof(sevp));
    sevp.sigev_notify = SIGEV_SIGNAL;
    sevp.sigev_signo = SIGUSR1;
    if (timer_create(CLOCK_REALTIME, &sevp, &timer) != 0)
        fail("timer_create");
}

static int
try
    (uint64_t time)
    {
        struct itimerspec spec = { { 0, 0 },
                                   /* overflows for 32-bit so cast to pointer-sized */
                                   { (size_t)time / 1000000000, time % 1000000000 } };
        polling_started = 0;
        signal_arrived = 0;
        if (timer_settime(timer, 0, &spec, 0) != 0)
            fail("timer_settime");
        if (!signal_arrived) {
            polling_started = 1;
            while (!signal_arrived)
                ;
        }
        return polling_started;
    }

int
main()
{
    int counts[2] = { 0, 0 };
    uint64_t time = 1;
    uint64_t step = 1;
    int direction = 0;
    int count_max = 4;
    int count = count_max;
    int i;

    setup();
    for (i = 0; i < 10000; i++) {
#if VERBOSE
        print("%8d %llu\n", i, (unsigned long long)time);
#endif
        int r =
        try
            (time);
        ++counts[r];

        /* Count number of successive steps in same direction. */
        if (r == direction)
            count = count >= count_max ? count_max : count + 1;
        else
            count = 0;
        direction = r;

        /* Halve or double step. */
        if (count < count_max - 1)
            step = step >> 1 ? step >> 1 : 1;
        else if (count >= count_max)
            step = step << 1 ? step << 1 : step;

        /* Adjust time, avoiding zero and overflow. */
        if (direction) {
            if (step < time)
                time -= step;
            else {
                time = 1;
                step = 1;
            }
        } else {
            if (time + step > time)
                time += step;
            else {
                time = -1;
                step = 1;
            }
        }
    }
#if VERBOSE
    print("Summary: %d %d %d\n", counts[0], counts[1], (int)time);
#endif
    print("all done\n");
    return 0;
}
