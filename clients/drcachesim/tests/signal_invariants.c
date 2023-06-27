/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

/* Adapted from suite/tests/pthreads/ptsig.c, with extra cases added.
 * This is a test of signal corner cases.  It partners with invariant_checker.cpp,
 * passing annotations to indicate places to check.
 */

#ifndef ASM_CODE_ONLY /* C code */

#    include "tools.h"
#    include <stdio.h>
#    include <stdlib.h>
#    include <pthread.h>
#    include <signal.h>
#    include <ucontext.h>
#    include <unistd.h>
#    include <assert.h>
#    include <setjmp.h>

#    ifndef X86
/* We really just need one test of signal corner cases, so there is little need
 * to port the asm here beyond x86.
 */
#        error ARM is not supported in test assembly code
#    endif

/* asm routines */
void
signal_handler_asm();
void
test_signal_midbb(void);
void
test_signal_startbb(void);
void
test_signal_midmemref(void);
void
test_signal_sigsegv_resume(void);

volatile double pi = 0.0;        /* Approximation to pi (shared). */
pthread_mutex_t pi_lock;         /* The lock for "pi". */
static const int intervals = 10; /* How many intervals should we run? */
static SIGJMP_BUF mark;

static volatile bool resume_sigsegv = false;

void /* non-static since called from asm */
signal_handler(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    switch (sig) {
    case SIGUSR1: {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        void *pc = (void *)sc->SC_XIP;
        break;
    }
    case SIGSEGV:
        /* We have two cases: one where we longjmp and another where we tweak
         * xax to be readable and then re-execute.
         */
        if (!resume_sigsegv)
            SIGLONGJMP(mark, 1);
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        sc->SC_XAX = sc->SC_XSP;
        break;
    case SIGILL: SIGLONGJMP(mark, 1); break;
    default: assert(0);
    }
}

void *
process(void *arg)
{
    char *id = (char *)arg;
    register double width, localsum;
    register int i;
    register int iproc;

    /* More signals for testing. */
    kill(getpid(), SIGUSR1);

    iproc = (*id - '0');

    width = 1.0 / intervals;

    /* Perform the local computations. */
    localsum = 0;
    for (i = iproc; i < intervals; i += 2) {
        register double x = (i + 0.5) * width;
        localsum += 4.0 / (1.0 + x * x);
    }
    localsum *= width;

    /* Lock pi for update, update it, and unlock. */
    pthread_mutex_lock(&pi_lock);
    pi += localsum;
    pthread_mutex_unlock(&pi_lock);

    return (NULL);
}

int
main(int argc, char **argv)
{
    intercept_signal(SIGUSR1, signal_handler_asm, false);
    intercept_signal(SIGSEGV, signal_handler_asm, false);
    intercept_signal(SIGILL, signal_handler_asm, false);

    /* Perform our assembly tests. */
    if (SIGSETJMP(mark) == 0) {
        test_signal_midbb();
    }
    if (SIGSETJMP(mark) == 0) {
        test_signal_startbb();
    }
    if (SIGSETJMP(mark) == 0) {
        test_signal_midmemref();
    }
    resume_sigsegv = true;
    test_signal_sigsegv_resume();

    pthread_t thread0, thread1;
    void *retval;

    pthread_mutex_init(&pi_lock, NULL);

    if (pthread_create(&thread0, NULL, process, (void *)"0") ||
        pthread_create(&thread1, NULL, process, (void *)"1")) {
        print("%s: cannot make thread\n", argv[0]);
        exit(1);
    }

    if (pthread_join(thread0, &retval) || pthread_join(thread1, &retval)) {
        print("%s: thread join failed\n", argv[0]);
        exit(1);
    }

    /* More signals for testing. */
    kill(getpid(), SIGUSR1);

    print("Estimation of pi is %16.15f\n", pi);

    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE
DECL_EXTERN(signal_handler)

#define FUNCNAME signal_handler_asm
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* prefetcht0 with address 1 marks the handler */
        prefetcht0 [1]
        jmp signal_handler
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME test_signal_midbb
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* prefetcht2's address is the instr count until a signal */
        prefetcht2 [3]
        nop
        nop
        ud2
        nop
        nop
        nop
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME test_signal_startbb
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* prefetcht2's address is the instr count until a signal */
        prefetcht2 [2]
        jmp      new_bb
    new_bb:
        ud2
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME test_signal_midmemref
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* Set up a multi-memref instr where the 1st memref faults.
         * XXX i#3958: Today the 2nd movs memref is incorrectly included *before*
         * the fault.
         */
        /* prefetcht2's address is the instr count until a signal */
        prefetcht2 [5]
        /* prefetcht1's address is the memref count until a signal */
        prefetcht1 [3]
        mov      REG_XSI, HEX(42)
        mov      REG_XDI, REG_XSP
        push     REG_XAX
        movsd
        pop      REG_XAX
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME test_signal_sigsegv_resume
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* This is a test case of a signal handler resuming at the interruption point.
         * The handler changes xax to hold a valid address.
         */
        mov      REG_XAX, HEX(42)
        mov      REG_XCX, PTRSZ [REG_XAX]
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

END_FILE
/* clang-format on */
#endif
