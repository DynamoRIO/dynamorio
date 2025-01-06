/* **********************************************************
 * Copyright (c) 2017-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2016-2024 Arm Limited. All rights reserved.
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

/* Sets a timer and loops a long-latency fragment ending in a system call to
 * trigger the case of an asynchronous signal being delivered when we go back
 * to dispatch to deliver a delayed signal.
 */

#ifndef ASM_CODE_ONLY /* C code */

#    include <signal.h>
#    include <stdio.h>
#    include <stdlib.h>
#    include <string.h>
#    include <time.h>

/* asm routines */
void
syscall_wrapper(volatile int *);

static volatile int flag_src;
static volatile int flag_dst;

void
fail(const char *s)
{
    perror(s);
    exit(1);
}

/* The signal handler copies one global to the other so that we can
 * discover when the signal happened.
 */
void
handler(int signum, siginfo_t *info, void *ucontext)
{
    flag_dst = flag_src;
}

/* Return -1 if the signal happened before the critical block, 0 if it
 * happened during the critical block, or 1 if it happened after the
 * critical block or the timer was cancelled. Under DynamoRIO the
 * signal will never appear to have happened during the critical block
 * because we delay the delivery of async signals to the end of the block.
 */
int
try(timer_t timer, unsigned long long timer_duration_ns)
{
    flag_dst = 1;
    flag_src = -1;

    /* Set timer. */
    struct itimerspec spec = {
        { 0, 0 }, { timer_duration_ns / 1000000000, timer_duration_ns % 1000000000 }
    };
    if (timer_settime(timer, 0, &spec, NULL))
        fail("timer_settime");

    syscall_wrapper(&flag_src);

    /* Cancel timer. */
    struct itimerspec spec0 = { { 0, 0 }, { 0, 0 } };
    if (timer_settime(timer, 0, &spec0, NULL))
        fail("timer_settime");

    return flag_dst;
}

int
main(int argc, char *argv[])
{
    /* Set up signal handler. */
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = handler;
    if (sigaction(SIGUSR1, &act, 0) != 0)
        fail("sigaction");

    /* Create timer. */
    struct sigevent sevp;
    memset(&sevp, 0, sizeof(sevp));
    sevp.sigev_notify = SIGEV_SIGNAL;
    sevp.sigev_signo = SIGUSR1;
    timer_t timer;
    if (timer_create(CLOCK_MONOTONIC, &sevp, &timer))
        fail("timer_create");

    /* Repeatedly call try(), changing timer_duration_ns each time.
     * Typically we halve the step each time, which results in a
     * binary search, but if we have had the same result several times
     * in succession then we suspect that something has changed so we
     * start doubling the step instead.
     */
    unsigned long long timer_duration_ns = 1024; /* arbitrary initial value */
    unsigned long long step = 1024;              /* power of two */
    int result = -1;
    unsigned long long unchanged_results = 0;
    /* A thousand tries took a few seconds and gave a few hundred hits
     * when tested on current hardware.
     */
    for (int i = 0; i < 1000; i++) {
        int previous_result = result;
        result = try(timer, timer_duration_ns);

        /* Stop trying if the signal arrived during the critical block.
         * This never happens (or seems never to happen) under DynamoRIO.
         */
        if (result == 0)
            break;

        /* Update unchanged_results. */
        if (result == previous_result)
            ++unchanged_results;
        else
            unchanged_results = 0;

        /* Update step. */
        if (unchanged_results < 5)
            step = step / 2 > 0 ? step / 2 : step;
        else
            step = step * 2 > 0 ? step * 2 : step;

        /* Adjust timer_duration_ns for next try. */
        if (result < 0)
            timer_duration_ns = timer_duration_ns + step > timer_duration_ns
                ? timer_duration_ns + step
                : (unsigned long long)-1;
        else {
            if (timer_duration_ns > step)
                timer_duration_ns -= step;
            else {
                timer_duration_ns = 1;
                step = 1;
            }
        }
    }

    printf("all done\n");
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
#    include "../../core/unix/include/syscall.h"
/* clang-format off */
START_FILE

/* void syscall_wrapper(); */
#define FUNCNAME syscall_wrapper
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef AARCH64
        str      wzr, [x0]
        /* We want a long-latency block to increase the chance our timer signal
         * arrives while we're inside it: FDIV is fairly slow.
         */
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        fdiv     v0.2d, v0.2d, v1.2d
        mov      w1, #1
        str      w1, [x0]
        mov      w8, # SYS_getpid
        svc      #0
        ret
#elif defined(ARM)
        mov      r2, r7
        mov      r1, #0
        str      r1, [r0]
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        vdiv.f64 d0, d0, d1
        mov      r1, #1
        str      r1, [r0]
        mov      r7, # SYS_getpid
        svc      #0
        mov      r7, r2
        bx       lr
#elif defined(X86)
#    ifdef X64
        mov      dword ptr [%rdi], 0
#    else
        mov      dword ptr [%eax], 0
#    endif
        /* We want a long-latency block to increase the chance our timer signal
         * arrives while we're inside it.  FPATAN is very slow.
         */
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
        fpatan
#    ifdef X64
        mov      dword ptr [%rdi], 1
#    else
        mov      dword ptr [%eax], 1
#    endif
        /* End the block with a very fast syscall. */
        mov      eax, SYS_getpid
#    ifdef X64
        syscall
#    else
        int      HEX(80)
#    endif
        ret
#else
#    error NYI
#endif
        END_FUNC(FUNCNAME)

END_FILE
/* clang-format on */
#endif
