/* **********************************************************
 * Copyright (c) 2017-2023 Google, Inc.  All rights reserved.
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

/* Sets a timer and loops a long-latency fragment ending in a system call to
 * trigger the case of an asynchronous signal being delivered when we go back
 * to dispatch to deliver a delayed signal.
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

timer_t timer;

/* asm routines */
void
syscall_wrapper();

void
fail(const char *s)
{
    perror(s);
    exit(1);
}

void
handler(int signum, siginfo_t *info, void *ucontext_)
{
    /* We just need the handler to exist; it needn't do anything but sigreturn. */
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

void
run_under_timer(uint64_t time)
{
    struct itimerspec spec = { { (size_t)time / 1000000000, time % 1000000000 },
                               /* overflows for 32-bit so cast to pointer-sized */
                               { (size_t)time / 1000000000, time % 1000000000 } };
    if (timer_settime(timer, 0, &spec, 0) != 0)
        fail("timer_settime");
    /* Our 1K * 100 outer loop iters does make the drcacheoff.sysnums test a little
     * slow, but is required to hit the interrupted block case in my tests on my
     * x86 and a64 machines.
     */
    for (int i = 0; i < 1000; ++i)
        syscall_wrapper();
}

int
main(int argc, const char *argv[])
{
    uint64_t time = 10000000;

    setup();
    for (int i = 0; i < 100; i++) {
        run_under_timer(time);
        /* Vary the time a little to hit different timing scenarios. */
        time -= i * 100000;
    }
    print("all done\n");
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
        /* We want a long-latency block to increase the chance our timer signal
         * arrives while we're inside it: FDIV is fairly slow.
         */
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        fdiv     v10.2d, v11.2d, v12.2d
        mov      w8, # SYS_getpid
        svc      #0
        ret
#elif defined(ARM)
        /* XXX i#5438: Add some long-latency instructions. */
        push     {r7}
        mov      r7, # SYS_getpid
        svc      #0
        pop      {r7}
        bx       lr
#elif defined(X86)
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
