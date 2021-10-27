/* **********************************************************
 * Copyright (c) 2012-2020 Google, Inc.  All rights reserved.
 * Copyright (c) 2009-2010 VMware, Inc.  All rights reserved.
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

/*
 * API regression test for signal event
 */

#include "tools.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <signal.h>
#include <ucontext.h>
#include <assert.h>
#include <setjmp.h>

static SIGJMP_BUF mark;

static void
foo(void)
{
    print("In foo\n");
}

EXPORT void
hook_and_long_jump(void)
{
    print("Redirected\n");
    foo();
    SIGLONGJMP(mark, 1);
}

static void
signal_handler(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (sig == SIGUSR1)
        print("Got SIGUSR1\n");
    else if (sig == SIGUSR2)
        print("Got SIGUSR2\n");
    else if (sig == SIGURG)
        print("Got SIGURG\n");
    else if (sig == SIGSEGV)
        print("Got SIGSEGV\n");
}

static void
unintercept_signal(int sig)
{
    int rc;
    struct sigaction act;
    act.sa_sigaction = (void (*)(int, siginfo_t *, void *))SIG_DFL;
    /* disarm the signal */
    rc = sigaction(sig, &act, NULL);
    assert(rc == 0);
}

int
main(int argc, char **argv)
{
    int bar;
    intercept_signal(SIGUSR1, signal_handler, false);
    intercept_signal(SIGUSR2, signal_handler, false);
    intercept_signal(SIGURG, signal_handler, false);
    intercept_signal(SIGSEGV, signal_handler, false);

    print("Sending SIGURG\n");
    kill(getpid(), SIGURG);
    print("Sending SIGURG\n");
    kill(getpid(), SIGURG);
    print("Sending SIGURG\n");
    kill(getpid(), SIGURG);

    unintercept_signal(SIGURG);

    print("Sending SIGURG\n");
    kill(getpid(), SIGURG);
    print("Sending SIGURG\n");
    kill(getpid(), SIGURG);
    print("Sending SIGURG\n");
    kill(getpid(), SIGURG);

    print("Sending SIGTERM\n");
    kill(getpid(), SIGTERM);

    if (SIGSETJMP(mark) == 0) {
        /* execute so that client sees the spot */
        hook_and_long_jump();
    }
    if (SIGSETJMP(mark) == 0) {
        print("Sending SIGUSR2\n");
        kill(getpid(), SIGUSR2);
    }

    /* Generate SIGSEGV in a manner that will re-crash post-handler unless the client
     * modifies the mcontext register values.
     */
#ifdef X86
#    ifdef X64
#        define EAX "%rax"
#        define ECX "%rcx"
#    else
#        define EAX "%eax"
#        define ECX "%ecx"
#    endif
    asm("push " EAX);
    asm("push " ECX);
    asm("mov %0, %" ECX "" : : "g"(&bar)); /* ok place to read from */
    asm("mov $0, " EAX);
    asm("mov (" EAX "), " EAX);
    asm("pop " ECX);
    asm("pop " EAX);
#elif defined(AARCH64)
    __asm__ __volatile__("stp x0, x1, [sp, #-16]!\n\t"
                         "mov x1, %0\n\t"
                         "mov x0, #0\n\t"
                         "ldr x0, [x0]\n\t"
                         "ldp x0, x1, [sp], #16\n\t"
                         :
                         : "r"(&bar)
                         : "x0", "x1", "memory");
#elif defined(ARM)
    __asm__ __volatile__("stm sp!, {r0, r1}\n\t"
                         "mov r1, %0\n\t"
                         "mov r0, #0\n\t"
                         "ldr r0, [r0]\n\t"
                         "ldm sp!, {r0, r1}\n\t"
                         :
                         : "r"(&bar)
                         : "r0", "r1", "memory");
#else
#    error Unsupported arch
#endif

    print("Sending SIGUSR1\n");
    kill(getpid(), SIGUSR1);

    print("Done\n");
}
