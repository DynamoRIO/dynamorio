/* **********************************************************
 * Copyright (c) 2015-2019 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef ASM_CODE_ONLY /* C code */
#    include "tools.h"
#    include "mangle_asynch-shared.h"
#    include <setjmp.h>
#    include <signal.h>
#    include <pthread.h>

volatile bool test_ready = false;
volatile bool test_done = false;
volatile int loop_inc = 1;

/* asm routines */
void
test_asm();

static SIGJMP_BUF mark;

static void
handle_signal(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
#    ifdef X86_64
    if (signal == SIGILL) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sc->LOOP_COUNT_REG_SIG != LOOP_COUNT)
            print("ERROR: incorrect result!\n");
        SIGLONGJMP(mark, 1);
    } else if (signal != SIGUSR2) {
        print("Unexpected signal!\n");
    }
#    endif
}

static void *
thread_routine(void *arg)
{
#    ifdef X86_64
    pthread_t main_thread = *(pthread_t *)arg;
    while (!test_ready) {
        /* Empty. */
    }
    struct timespec sleeptime;
    sleeptime.tv_sec = 0;
    sleeptime.tv_nsec = 1000 * 1111;
    while (!test_done) {
        pthread_kill(main_thread, SIGUSR2);
        nanosleep(&sleeptime, NULL);
    }
#    endif
    return NULL;
}

int
main(int argc, const char *argv[])
{
#    define NUM_THREADS 8
    pthread_t thread[NUM_THREADS];
    void *retval;
    pthread_t main_thread = pthread_self();

    for (int i = 0; i < NUM_THREADS; ++i) {
        if (pthread_create(&thread[i], NULL, thread_routine, &main_thread) != 0) {
            perror("Failed to create thread");
            exit(1);
        }
    }

    intercept_signal(SIGILL, (handler_3_t)&handle_signal, false);
    intercept_signal(SIGUSR2, (handler_3_t)&handle_signal, false);

    /* Test loop counter using mangled rip-rel instruction, interrupted by asynch
     * signals.
     */
    if (SIGSETJMP(mark) == 0) {
        test_asm();
    }

    for (int i = 0; i < NUM_THREADS; ++i) {
        if (pthread_join(thread[i], &retval) != 0)
            perror("Failed to join thread");
    }

    print("Test finished\n");
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
#    include "mangle_asynch-shared.h"
/* clang-format off */
START_FILE

#ifdef X64
# define FRAME_PADDING 0
#else
# define FRAME_PADDING 0
#endif

DECL_EXTERN(test_ready)
DECL_EXTERN(test_done)
DECL_EXTERN(loop_inc)

#define FUNCNAME test_asm
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
#ifdef X64
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test
     test:
        mov      BYTE SYMREF(test_ready), HEX(1)
        mov      LOOP_TEST_REG_ASM, LOOP_COUNT
        mov      LOOP_COUNT_REG_ASM, 0
      loop:
        /* The address will get mangled into REG_XAX. */
        add      LOOP_COUNT_REG_ASM, PTRSZ SYMREF(loop_inc)
        sub      LOOP_TEST_REG_ASM, 1
        cmp      LOOP_TEST_REG_ASM, 0
        jnz      loop
        jmp      epilog
     epilog:
        mov      BYTE SYMREF(test_done), HEX(1)
        /* This will trigger the SIGILL handler which ensures there was no extra
         * add within the loop from re-execution due to translating back.
         */
        ud2
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
#endif
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        /* Test 8: not implemented for ARM */
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        /* Test 8: not implemented for AARCH64 */
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

END_FILE

/* clang-format on */
#endif
