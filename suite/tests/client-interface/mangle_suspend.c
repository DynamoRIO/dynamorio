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

/* Test xl8 pc of rip-rel instruction (xref #3307) in mangling epilogue, caused
 * by asynch interrupt.
 */

/* clang-format off */
/* XXX: clang-format incorrectly detects a white space difference at "clang-format on"
 * below. This is why "clang-format off" has been moved outside the ifdef until the
 * bug is fixed.
 */
#ifndef ASM_CODE_ONLY /* C code */
#    include "configure.h"
#    ifndef UNIX
#        error UNIX-only
#    endif
#    include "tools.h"
#    include "mangle_suspend-shared.h"
#    include <setjmp.h>
#    include <signal.h>
#    include <pthread.h>

volatile bool test_ready = false;
volatile bool test_done = false;
volatile bool test_suspend = false;
volatile int loop_inc = 1;

void
test_1_asm();

void
test_2_asm();

static void *
suspend_thread_1_routine(void *arg)
{
#    ifdef X86_64
    /* This thread is executing labels for the client to insert a clean call that
     * does the suspend and subsequent check for correctness.
     */
    while (!test_ready) {
        sched_yield();
    }
    while (!test_done) {
        asm volatile("mov %0, %%rdx\n\t"
                     "mov %0, %%rdx\n"
                     :
                     : "i"(SUSPEND_VAL_TEST_1_C)
                     : "rdx");
        while (!test_suspend && !test_done) {
            sched_yield();
        }
        test_suspend = false;
    }
#    endif
    return NULL;
}

static void *
suspend_thread_2_routine(void *arg)
{
#    ifdef X86_64
    /* This thread is executing labels for the client to insert a clean call that
     * does the suspend and subsequent check for correctness.
     */
    while (!test_ready) {
        sched_yield();
    }
    while (!test_done) {
        asm volatile("mov %0, %%rdx\n\t"
                     "mov %0, %%rdx\n"
                     :
                     : "i"(SUSPEND_VAL_TEST_2_C)
                     : "rdx");
        while (!test_suspend && !test_done) {
            sched_yield();
        }
        test_suspend = false;
    }
#    endif
    return NULL;
}

int
main(int argc, const char *argv[])
{
    pthread_t suspend_thread;
    void *retval;

    if (pthread_create(&suspend_thread, NULL, suspend_thread_1_routine, NULL) != 0) {
        perror("Failed to create thread");
        exit(1);
    }

    /* Test xl8 pc of rip-rel instruction (xref #3307) caused by
     * asynch interrupt.
     */
    test_1_asm();

    if (pthread_join(suspend_thread, &retval) != 0)
        perror("Failed to join thread");

    print("Test 1 finished\n");

    test_ready = false;
    test_done = false;
    test_suspend = false;

    if (pthread_create(&suspend_thread, NULL, suspend_thread_2_routine, NULL) != 0) {
        perror("Failed to create thread");
        exit(1);
    }

    /* Test xl8 pc of rip-rel instruction (xref #3307) caused by
     * asynch interrupt.
     */
    test_2_asm();

    if (pthread_join(suspend_thread, &retval) != 0)
        perror("Failed to join thread");

    print("Test 2 finished\n");

    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
#    include "mangle_suspend-shared.h"
START_FILE

#ifdef X64
# define FRAME_PADDING 0
#else
# define FRAME_PADDING 0
#endif

DECL_EXTERN(test_ready)
DECL_EXTERN(test_suspend)
DECL_EXTERN(test_done)
DECL_EXTERN(loop_inc)

#define FUNCNAME test_1_asm
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
#ifdef X64
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test_1
     test_1:
        mov      PTRSZ [REG_XSP], TEST_1_LOOP_COUNT_REG_ASM
        sub      REG_XSP, 8
        mov      SUSPEND_TEST_REG_ASM, TEST_VAL
        mov      SUSPEND_TEST_REG_ASM, TEST_VAL
        nop
        mov      BYTE SYMREF(test_ready), HEX(1)
        mov      LOOP_TEST_REG_OUTER_ASM, LOOP_COUNT_OUTER
        mov      TEST_1_LOOP_COUNT_REG_ASM, 2

        /* Code changes here must stay in synch with the loop bounds
         * check hardcoded in the dll.
         */
     loop_a_outer:
        mov      LOOP_TEST_REG_INNER_ASM, LOOP_COUNT_INNER
     loop_a_inner:
        mov      TEST_1_LOOP_COUNT_REG_ASM, 1
        add      TEST_1_LOOP_COUNT_REG_ASM, PTRSZ SYMREF(loop_inc)
        mov      TEST_1_LOOP_COUNT_REG_ASM, 2
        sub      LOOP_TEST_REG_INNER_ASM, 1
        cmp      LOOP_TEST_REG_INNER_ASM, 0
        jnz      loop_a_inner
        mov      BYTE SYMREF(test_suspend), HEX(1)
        sub      LOOP_TEST_REG_OUTER_ASM, 1
        cmp      LOOP_TEST_REG_OUTER_ASM, 0
        jnz      loop_a_outer

        jmp      epilog_a
     epilog_a:
        mov      BYTE SYMREF(test_done), HEX(1)
        add      REG_XSP, 8
        mov      TEST_1_LOOP_COUNT_REG_ASM, PTRSZ [REG_XSP]
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
#endif
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        /* Test: not implemented for ARM */
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        /* Test: not implemented for AARCH64 */
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME test_2_asm
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
#ifdef X64
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test_2
     test_2:
        mov      PTRSZ [REG_XSP], TEST_2_LOOP_COUNT_REG_ASM
        sub      REG_XSP, 8
        mov      PTRSZ [REG_XSP], TEST_2_CHECK_REG_ASM
        sub      REG_XSP, 8
        mov      SUSPEND_TEST_REG_ASM, TEST_VAL
        mov      SUSPEND_TEST_REG_ASM, TEST_VAL
        nop
        mov      BYTE SYMREF(test_ready), HEX(1)
        mov      LOOP_TEST_REG_OUTER_ASM, LOOP_COUNT_OUTER
        mov      TEST_2_LOOP_COUNT_REG_ASM, 2
        mov      TEST_2_CHECK_REG_ASM, HEX(0)

        /* Code changes here must stay in synch with the loop bounds
         * check hardcoded in the dll.
         */
     loop_b_outer:
        mov      LOOP_TEST_REG_INNER_ASM, LOOP_COUNT_INNER
     loop_b_inner:
        mov      TEST_2_LOOP_COUNT_REG_ASM, 1
        add      TEST_2_LOOP_COUNT_REG_ASM, PTRSZ SYMREF(loop_inc)
        mov      TEST_2_LOOP_COUNT_REG_ASM, 2
        sub      LOOP_TEST_REG_INNER_ASM, 1
        cmp      LOOP_TEST_REG_INNER_ASM, 0
        jnz      loop_b_inner
        mov      BYTE SYMREF(test_suspend), HEX(1)
        sub      LOOP_TEST_REG_OUTER_ASM, 1
        cmp      LOOP_TEST_REG_OUTER_ASM, 0
        jnz      loop_b_outer

        jmp      epilog_b
     epilog_b:
        mov      BYTE SYMREF(test_done), HEX(1)
        add      REG_XSP, 8
        mov      TEST_2_CHECK_REG_ASM, PTRSZ [REG_XSP]
        add      REG_XSP, 8
        mov      TEST_2_LOOP_COUNT_REG_ASM, PTRSZ [REG_XSP]
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
#endif
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        /* Test: not implemented for ARM */
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        /* Test: not implemented for AARCH64 */
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

END_FILE

#endif
/* clang-format on */
