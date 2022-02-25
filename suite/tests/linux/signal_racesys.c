/* **********************************************************
 * Copyright (c) 2017-2022 Google, Inc.  All rights reserved.
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
 * before a system call, which is nanosleep, in this case. This can expose a
 * race condition. Part of this is implemented in assembler as I do not know
 * of a portable way of detecting whether the system call has started (and
 * was interrupted), which the program needs to know in order to adjust the
 * timer.
 */

#ifndef ASM_CODE_ONLY /* C code */

#    include "configure.h"
#    include <setjmp.h>
#    include <signal.h>
#    include <stdint.h>
#    include <stdio.h>
#    include <stdlib.h>
#    include <string.h>
#    include <time.h>
#    include <unistd.h>
#    include "tools.h"

/* asm routines */
void
nanosleep_wrapper(const struct timespec *req, struct timespec *rem);
void
nanosleep_interrupted(void);

timer_t timer;
sigjmp_buf env;

void
fail(const char *s)
{
    perror(s);
    exit(1);
}

void
handler(int signum, siginfo_t *info, void *ucontext_)
{
    ucontext_t *ucontext = ucontext_;
    sigcontext_t *sc = SIGCXT_FROM_UCXT(ucontext);
    siglongjmp(env, 1 + (sc->SC_XIP == (uintptr_t)nanosleep_interrupted));
}

void
setup(void)
{
    struct sigaction act;
    struct sigevent sevp;

    memset(&act, 0, sizeof(act));
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = handler;
    if (sigaction(SIGUSR1, &act, 0) != 0)
        fail("sigaction");

    memset(&sevp, 0, sizeof(sevp));
    sevp.sigev_notify = SIGEV_SIGNAL;
    sevp.sigev_signo = SIGUSR1;
    if (timer_create(CLOCK_REALTIME, &sevp, &timer) != 0)
        fail("timer_create");
}

int
try
    (uint64_t time)
    {
        struct itimerspec spec = { { 0, 0 },
                                   /* overflows for 32-bit so cast to pointer-sized */
                                   { (size_t)time / 1000000000, time % 1000000000 } };
        struct timespec ts = { 3155760000U, 0 };
        int result;

        result = sigsetjmp(env, 1);
        if (result == 0) {
            if (timer_settime(timer, 0, &spec, 0) != 0)
                fail("timer_settime");
            nanosleep_wrapper(&ts, 0);
            return 0;
        }
        return result - 1;
    }

int
main(int argc, const char *argv[])
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
#    if VERBOSE
        print("%8d %llu\n", i, (unsigned long long)time);
#    endif
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
#    if VERBOSE
    print("Summary: %d %d %d\n", counts[0], counts[1], (int)time);
#    endif
    print("all done\n");
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
#    include "../../core/unix/include/syscall.h"
/* clang-format off */
START_FILE

DECLARE_GLOBAL(nanosleep_interrupted)

/* void nanosleep_wrapper(const struct timespec *req, struct timespec *rem); */
#define FUNCNAME nanosleep_wrapper
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef AARCH64
        /* Calling convention args == syscall args */
        mov      w8, # SYS_nanosleep
        svc      #0
ADDRTAKEN_LABEL(nanosleep_interrupted:)
        ret
#elif defined(ARM)
        /* Calling convention args == syscall args */
        push     {r7}
        mov      r7, # SYS_nanosleep
        svc      #0
ADDRTAKEN_LABEL(nanosleep_interrupted:)
        pop      {r7}
        bx       lr
#elif defined(X86) && defined(X64)
        /* Calling convention args == syscall args */
        mov      eax, SYS_nanosleep
        syscall
ADDRTAKEN_LABEL(nanosleep_interrupted:)
        ret
#elif defined(X86) && !defined(X64)
        push     ebx
        mov      ebx, ARG1
        mov      ecx, ARG2
        mov      eax, SYS_nanosleep
        int      HEX(80)
ADDRTAKEN_LABEL(nanosleep_interrupted:)
        pop      ebx
        ret
#else
# error NYI
#endif
        END_FUNC(FUNCNAME)

END_FILE
/* clang-format on */
#endif
