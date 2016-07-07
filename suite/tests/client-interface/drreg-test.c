/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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
#include "tools.h"
#include "drreg-test-shared.h"
#include <setjmp.h>

/* asm routines */
void test_asm();
void test_asm_faultA();
void test_asm_faultB();

static SIGJMP_BUF mark;

#if defined(UNIX)
# include <signal.h>
static void
handle_signal(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGILL) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sc->TEST_REG_SIG != DRREG_TEST_3_C)
            print("ERROR: spilled register value was not preserved!\n");
    } else if (signal == SIGSEGV) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (((sc->TEST_FLAGS_SIG) & DRREG_TEST_AFLAGS_C) != DRREG_TEST_AFLAGS_C)
            print("ERROR: spilled flags value was not preserved!\n");
    }
    SIGLONGJMP(mark, 1);
}
#elif defined(WINDOWS)
# include <windows.h>
static LONG WINAPI
handle_exception(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (ep->ContextRecord->TEST_REG_CXT != DRREG_TEST_3_C)
            print("ERROR: spilled register value was not preserved!\n");
    } else if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        if ((ep->ContextRecord->CXT_XFLAGS & DRREG_TEST_AFLAGS_C) != DRREG_TEST_AFLAGS_C)
            print("ERROR: spilled flags value was not preserved!\n");
    }
    SIGLONGJMP(mark, 1);
}
#endif

int
main(int argc, const char *argv[])
{
#if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal, false);
    intercept_signal(SIGILL, (handler_3_t)&handle_signal, false);
#elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception);
#endif

    print("drreg-test running\n");

    test_asm();

    /* Test fault reg restore */
    if (SIGSETJMP(mark) == 0) {
        test_asm_faultA();
    }

    /* Test fault aflags restore */
    if (SIGSETJMP(mark) == 0) {
        test_asm_faultB();
    }

    /* XXX i#511: add more fault tests and other tricky corner cases */

    print("drreg-test finished\n");
    return 0;
}

#else /* asm code *************************************************************/
#include "asm_defines.asm"
#include "drreg-test-shared.h"
START_FILE

#ifdef X64
# define FRAME_PADDING 8
#else
# define FRAME_PADDING 0
#endif

#define FUNCNAME test_asm
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        /* push callee-saved registers */
        PUSH_SEH(REG_XBX)
        PUSH_SEH(REG_XBP)
        PUSH_SEH(REG_XSI)
        PUSH_SEH(REG_XDI)
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test1
        /* Test 1: separate write and read of reserved reg */
     test1:
        mov      TEST_REG_ASM, DRREG_TEST_1_ASM
        mov      TEST_REG_ASM, DRREG_TEST_1_ASM
        mov      TEST_REG_ASM, REG_XSP
        mov      REG_XBX, PTRSZ [TEST_REG_ASM]

        jmp      test2
        /* Test 2: same instr writes and reads reserved reg */
     test2:
        mov      TEST_REG_ASM, DRREG_TEST_2_ASM
        mov      TEST_REG_ASM, DRREG_TEST_2_ASM
        mov      TEST_REG_ASM, REG_XSP
        mov      TEST_REG_ASM, PTRSZ [TEST_REG_ASM]

        jmp      test4
        /* Test 4: read and write of reserved aflags */
     test4:
        mov      TEST_REG_ASM, DRREG_TEST_4_ASM
        mov      TEST_REG_ASM, DRREG_TEST_4_ASM
        setne    TEST_REG_ASM_LSB
        cmp      TEST_REG_ASM, REG_XSP

        jmp      epilog
     epilog:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        pop      REG_XDI
        pop      REG_XSI
        pop      REG_XBP
        pop      REG_XBX
        ret
#elif defined(ARM)
        b        test1
        /* Test 1: separate write and read of reserved reg */
     test1:
        movw     TEST_REG_ASM, DRREG_TEST_1_ASM
        movw     TEST_REG_ASM, DRREG_TEST_1_ASM
        mov      TEST_REG_ASM, sp
        ldr      r0, PTRSZ [TEST_REG_ASM]

        b        test2
        /* Test 2: same instr writes and reads reserved reg */
     test2:
        movw     TEST_REG_ASM, DRREG_TEST_2_ASM
        movw     TEST_REG_ASM, DRREG_TEST_2_ASM
        mov      TEST_REG_ASM, sp
        ldr      TEST_REG_ASM, PTRSZ [TEST_REG_ASM]

        b        test4
        /* Test 4: read and write of reserved aflags */
     test4:
        movw     TEST_REG_ASM, DRREG_TEST_4_ASM
        movw     TEST_REG_ASM, DRREG_TEST_4_ASM
        sel      TEST_REG_ASM, r0, r0
        cmp      TEST_REG_ASM, sp

        b        epilog
    epilog:
        bx       lr
#elif defined(AARCH64)
        b        test1
        /* Test 1: separate write and read of reserved reg */
     test1:
        movz     TEST_REG_ASM, DRREG_TEST_1_ASM
        movz     TEST_REG_ASM, DRREG_TEST_1_ASM
        mov      TEST_REG_ASM, sp
        ldr      x0, PTRSZ [TEST_REG_ASM]

        b        test2
        /* Test 2: same instr writes and reads reserved reg */
     test2:
        movz     TEST_REG_ASM, DRREG_TEST_2_ASM
        movz     TEST_REG_ASM, DRREG_TEST_2_ASM
        mov      TEST_REG_ASM, sp
        ldr      TEST_REG_ASM, PTRSZ [TEST_REG_ASM]

        b        test4
        /* Test 4: read and write of reserved aflags */
     test4:
        movz     TEST_REG_ASM, DRREG_TEST_4_ASM
        movz     TEST_REG_ASM, DRREG_TEST_4_ASM
        csel     TEST_REG_ASM, x0, x0, gt
        cmp      TEST_REG_ASM, x0

        b        epilog
    epilog:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME test_asm_faultA
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        /* push callee-saved registers */
        PUSH_SEH(REG_XBX)
        PUSH_SEH(REG_XBP)
        PUSH_SEH(REG_XSI)
        PUSH_SEH(REG_XDI)
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test3
        /* Test 3: fault reg restore */
     test3:
        mov      TEST_REG_ASM, DRREG_TEST_3_ASM
        mov      TEST_REG_ASM, DRREG_TEST_3_ASM
        nop
        ud2

        jmp      epilog2
     epilog2:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        pop      REG_XDI
        pop      REG_XSI
        pop      REG_XBP
        pop      REG_XBX
        ret
#elif defined(ARM)
        b        test3
        /* Test 3: fault reg restore */
     test3:
        movw     TEST_REG_ASM, DRREG_TEST_3_ASM
        movw     TEST_REG_ASM, DRREG_TEST_3_ASM
        nop
        .word 0xe7f000f0 /* udf */

        b        epilog2
    epilog2:
        bx       lr
#elif defined(AARCH64)
        b        test3
        /* Test 3: fault reg restore */
     test3:
        movz     TEST_REG_ASM, DRREG_TEST_3_ASM
        movz     TEST_REG_ASM, DRREG_TEST_3_ASM
        nop
        .inst 0xf36d19 /* udf */

        b        epilog2
    epilog2:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME test_asm_faultB
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        /* push callee-saved registers */
        PUSH_SEH(REG_XBX)
        PUSH_SEH(REG_XBP)
        PUSH_SEH(REG_XSI)
        PUSH_SEH(REG_XDI)
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test5
        /* Test 5: fault aflags restore */
     test5:
        mov      TEST_REG_ASM, DRREG_TEST_5_ASM
        mov      TEST_REG_ASM, DRREG_TEST_5_ASM
        mov      ah, DRREG_TEST_AFLAGS_ASM
        sahf
        nop
        mov      REG_XAX, 0
        mov      REG_XAX, PTRSZ [REG_XAX] /* crash */

        jmp      epilog3
     epilog3:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        pop      REG_XDI
        pop      REG_XSI
        pop      REG_XBP
        pop      REG_XBX
        ret
#elif defined(ARM)
        b        test5
        /* Test 5: fault aflags restore */
     test5:
        movw     TEST_REG_ASM, DRREG_TEST_5_ASM
        movw     TEST_REG_ASM, DRREG_TEST_5_ASM
        /* XXX: also test GE flags */
        msr      APSR_nzcvq, DRREG_TEST_AFLAGS_ASM
        nop
        mov      r0, HEX(0)
        ldr      r0, PTRSZ [r0] /* crash */

        b        epilog3
    epilog3:
        bx       lr
#elif defined(AARCH64)
        b        test5
        /* Test 5: fault aflags restore */
     test5:
        movz     TEST_REG_ASM, DRREG_TEST_5_ASM
        movz     TEST_REG_ASM, DRREG_TEST_5_ASM
        movz     TEST_REG_ASM, DRREG_TEST_AFLAGS_H_ASM, LSL 16
        msr      nzcv, TEST_REG_ASM
        nop
        mov      x0, HEX(0)
        ldr      x0, PTRSZ [x0] /* crash */

        b        epilog3
    epilog3:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

END_FILE
#endif
