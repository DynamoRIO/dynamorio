/* **********************************************************
 * Copyright (c) 2015-2021 Google, Inc.  All rights reserved.
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

/* clang-format off */
/* XXX: clang-format incorrectly detected a tab difference at "clang-format on"
 * below. This is why "clang-format off" has been moved outside the ifdef until
 * bug is fixed.
 */
#ifndef ASM_CODE_ONLY /* C code */
#    include "tools.h"
#    include "drreg-test-shared.h"
#    include <setjmp.h>

/* asm routines */
void
test_asm();
void
test_asm_faultA();
void
test_asm_faultB();
void
test_asm_faultC();
void
test_asm_faultD();
void
test_asm_faultE();
void
test_asm_faultF();
void
test_asm_faultG();
void
test_asm_faultH();
void
test_asm_faultI();
void
test_asm_faultJ();
void
test_asm_faultK();
void
test_asm_faultL();

static SIGJMP_BUF mark;

#    if defined(UNIX)
#        include <signal.h>
static void
handle_signal0(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    print("ERROR: did not expect any signal!\n");
    SIGLONGJMP(mark, 1);
}

static void
handle_signal1(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGILL) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sc->TEST_REG_SIG != DRREG_TEST_3_C)
            print("ERROR: spilled register value was not preserved!\n");
    } else if (signal == SIGSEGV) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (!TESTALL(DRREG_TEST_AFLAGS_C, sc->TEST_FLAGS_SIG))
            print("ERROR: spilled flags value was not preserved!\n");
    }
    SIGLONGJMP(mark, 1);
}

static void
handle_signal2(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGILL) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sc->TEST_REG_SIG != DRREG_TEST_7_C)
            print("ERROR: spilled register value was not preserved!\n");
    }
    SIGLONGJMP(mark, 1);
}

static void
handle_signal3(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
#        ifdef X86
    if (signal == SIGSEGV) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sc->SC_XAX != DRREG_TEST_9_C) {
            print("ERROR: spilled register value was not preserved!\n");
            exit(1);
        }
    }
#        endif
    SIGLONGJMP(mark, 1);
}

static void
handle_signal4(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
#        ifdef X86
    if (signal == SIGSEGV) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sc->SC_XAX != DRREG_TEST_11_C) {
            print("ERROR: spilled register value was not preserved!\n");
            exit(1);
        }
    }
#        endif
    SIGLONGJMP(mark, 1);
}

static void
handle_signal5(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGILL) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sc->TEST_REG_SIG != DRREG_TEST_14_C)
            print("ERROR: spilled register value was not preserved in test #14!\n");
    } else if (signal == SIGSEGV) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sc->TEST_REG_SIG != DRREG_TEST_17_C)
            print("ERROR: spilled register value was not preserved in test #17!\n");
    }
    SIGLONGJMP(mark, 1);
}

static void
handle_signal6(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGILL) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (!TESTALL(DRREG_TEST_AFLAGS_C, sc->TEST_FLAGS_SIG))
            print("ERROR: spilled flags value was not preserved in test #15!\n");
    } else if (signal == SIGSEGV) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sc->TEST_REG_SIG != DRREG_TEST_16_C)
            print("ERROR: spilled register value was not preserved in test #16!\n");
    }
    SIGLONGJMP(mark, 1);
}

static void
handle_signal7(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGILL) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sc->TEST_REG_SIG != DRREG_TEST_18_C)
            print("ERROR: spilled register value was not preserved in test #18!\n");
    } else if (signal == SIGSEGV) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sc->TEST_REG_SIG != DRREG_TEST_19_C)
            print("ERROR: spilled register value was not preserved in test #19!\n");
    }
    SIGLONGJMP(mark, 1);
}

static void
handle_signal8(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGILL) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sc->TEST_REG_SIG != DRREG_TEST_20_C)
            print("ERROR: spilled register value was not preserved in test #20!\n");
    }
    SIGLONGJMP(mark, 1);
}

#    elif defined(WINDOWS)
#        include <windows.h>
static LONG WINAPI
handle_exception0(struct _EXCEPTION_POINTERS *ep)
{
    print("ERROR: did not expect any signal!\n");
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception1(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (ep->ContextRecord->TEST_REG_CXT != DRREG_TEST_3_C)
            print("ERROR: spilled register value was not preserved!\n");
    } else if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        if (!TESTALL(DRREG_TEST_AFLAGS_C, ep->ContextRecord->CXT_XFLAGS))
            print("ERROR: spilled flags value was not preserved!\n");
    }
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception2(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (ep->ContextRecord->TEST_REG_CXT != DRREG_TEST_7_C)
            print("ERROR: spilled register value was not preserved!\n");
    }
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception3(struct _EXCEPTION_POINTERS *ep)
{
#        ifdef X86
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (ep->ContextRecord->TEST_XAX_CXT != DRREG_TEST_9_C)
            print("ERROR: spilled register value was not preserved!\n");
    }
#        endif
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception4(struct _EXCEPTION_POINTERS *ep)
{
#        ifdef X86
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (ep->ContextRecord->TEST_XAX_CXT != DRREG_TEST_11_C)
            print("ERROR: spilled register value was not preserved!\n");
    }
#        endif
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception5(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (ep->ContextRecord->TEST_REG_CXT != DRREG_TEST_14_C)
            print("ERROR: spilled register value was not preserved!\n");
    }
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception6(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (!TESTALL(DRREG_TEST_AFLAGS_C, ep->ContextRecord->CXT_XFLAGS))
            print("ERROR: spilled flags value was not preserved in test #15!\n");
    }
    else if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        if (ep->ContextRecord->TEST_REG_CXT != DRREG_TEST_16_C)
            print("ERROR: spilled register value was not preserved in test #16!\n");
    }
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception7(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (ep->ContextRecord->TEST_REG_CXT != DRREG_TEST_18_C)
            print("ERROR: spilled register value was not preserved in test #18!\n");
    } else if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        if (ep->ContextRecord->TEST_REG_CXT != DRREG_TEST_19_C)
            print("ERROR: spilled register value was not preserved in test #19!\n");
    }
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception8(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (ep->ContextRecord->TEST_REG_CXT != DRREG_TEST_20_C)
            print("ERROR: spilled register value was not preserved in test #20!\n");
    }
    SIGLONGJMP(mark, 1);
}
#    endif

int
main(int argc, const char *argv[])
{
#    if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal0, false);
    intercept_signal(SIGILL, (handler_3_t)&handle_signal0, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception0);
#    endif

    print("drreg-test running\n");

    if (SIGSETJMP(mark) == 0) {
        test_asm();
    }

#    if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal1, false);
    intercept_signal(SIGILL, (handler_3_t)&handle_signal1, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception1);
#    endif

    /* Test fault reg restore */
    if (SIGSETJMP(mark) == 0) {
        test_asm_faultA();
    }

    /* Test fault aflags restore */
    if (SIGSETJMP(mark) == 0) {
        test_asm_faultB();
    }

#    if defined(UNIX)
    intercept_signal(SIGILL, (handler_3_t)&handle_signal2, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception2);
#    endif

    /* Test fault check ignore 3rd DR TLS slot */
    if (SIGSETJMP(mark) == 0) {
        test_asm_faultC();
    }

#    if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal3, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception3);
#    endif

    /* Test fault restore of non-public DR slot used by mangling.
     * Making sure drreg ignores restoring this slot.
     */
    if (SIGSETJMP(mark) == 0) {
        test_asm_faultD();
    }

#    if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal4, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception4);
#    endif

    /* Test 10: test fault restore of non-public DR slot used by mangling,
     * when rip-rel address is forced to be in register. Making sure drreg
     * ignores restoring this slot. Exposes transparency limitation of DR
     * if reg is optimized to be app's dead reg.
     */
    if (SIGSETJMP(mark) == 0) {
        test_asm_faultE();
    }

    #    if defined(UNIX)
    intercept_signal(SIGILL, (handler_3_t)&handle_signal5, false);
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal5, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception5);
#    endif

    /* Test fault reg restore for multi-phase nested reservation. */
    if (SIGSETJMP(mark) == 0) {
        test_asm_faultF();
    }
    /* Test fault reg restore for multi-phase non-nested overlapping reservations. */
    if (SIGSETJMP(mark) == 0) {
        test_asm_faultI();
    }

    #    if defined(UNIX)
    intercept_signal(SIGILL, (handler_3_t)&handle_signal6, false);
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal6, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception6);
#    endif

    /* Test fault aflags restore from xax. */
    if (SIGSETJMP(mark) == 0) {
        test_asm_faultG();
    }

    /* Test fault reg restore bug */
    if (SIGSETJMP(mark) == 0) {
        test_asm_faultH();
    }

    #    if defined(UNIX)
    intercept_signal(SIGILL, (handler_3_t)&handle_signal7, false);
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal7, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception7);
#    endif

    /* Test fault reg restore for fragments with DR_EMIT_STORE_TRANSLATIONS */
    if (SIGSETJMP(mark) == 0) {
       test_asm_faultJ();
    }

    /* Test fault reg restore for fragments with a faux spill instr. */
    if (SIGSETJMP(mark) == 0) {
         test_asm_faultK();
    }

    #    if defined(UNIX)
    intercept_signal(SIGILL, (handler_3_t)&handle_signal8, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception8);
#    endif

    /* Test fault reg restore for multi-phase nested reservation where
     * the first phase doesn't write the reg before the second
     * reservation.
     */
    if (SIGSETJMP(mark) == 0) {
        test_asm_faultL();
    }

    /* XXX i#511: add more fault tests and other tricky corner cases */

    print("drreg-test finished\n");
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
#    include "drreg-test-shared.h"
START_FILE

#ifdef X64
# define FRAME_PADDING 0
#else
# define FRAME_PADDING 0
#endif

#define FUNCNAME test_asm
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test1
        /* Test 1: separate write and read of reserved reg */
     test1:
        mov      TEST_REG_ASM, DRREG_TEST_1_ASM
        mov      TEST_REG_ASM, DRREG_TEST_1_ASM
        mov      TEST_REG_ASM, REG_XSP
        mov      REG_XBX, PTRSZ [TEST_REG_ASM]

        jmp      test2_init
     test2_init:
        /* Initializing register for additional test on top of this one, see
         * instru2instru.
         */
        mov      TEST_REG2_ASM, MAKE_HEX_ASM(0)
        jmp      test2
     test2:
        /* Test 2: same instr writes and reads reserved reg */
        mov      TEST_REG_ASM, DRREG_TEST_2_ASM
        mov      TEST_REG_ASM, DRREG_TEST_2_ASM
        mov      TEST_REG_ASM, REG_XSP
        mov      PTRSZ [TEST_REG_ASM - 8], TEST_REG_ASM
        mov      TEST_REG_ASM, PTRSZ [TEST_REG_ASM - 8]
        /* Test accessing the reg again to ensure the app spill slot and tool value
         * are handled in the proper order:
         */
        mov      TEST_REG_ASM, PTRSZ [TEST_REG_ASM]

        jmp      test4
        /* Test 4: read and write of reserved aflags */
     test4:
        mov      TEST_REG_ASM, DRREG_TEST_4_ASM
        mov      TEST_REG_ASM, DRREG_TEST_4_ASM
        setne    TEST_REG_ASM_LSB
        cmp      TEST_REG_ASM, REG_XSP

        jmp      test11
        /* Store aflags to dead XAX, and restore when XAX is live */
     test11:
        mov      TEST_REG_ASM, DRREG_TEST_11_ASM
        mov      TEST_REG_ASM, DRREG_TEST_11_ASM
        cmp      TEST_REG_ASM, TEST_REG_ASM
        push     TEST_11_CONST
        pop      REG_XAX
        mov      REG_XAX, TEST_REG_ASM
        mov      TEST_REG_ASM, REG_XAX
        je       test11_done
        /* Null deref if we have incorrect eflags */
        xor      TEST_REG_ASM, TEST_REG_ASM
        mov      PTRSZ [TEST_REG_ASM], TEST_REG_ASM
        jmp      test11_done
     test11_done:
        jmp     test12
        /* Test 12: drreg_statelessly_restore_app_value  */
     test12:
        mov      TEST_REG_ASM, DRREG_TEST_12_ASM
        mov      TEST_REG_ASM, DRREG_TEST_12_ASM
        mov      REG_XAX, TEST_12_CONST
        cmp      REG_XAX, TEST_12_CONST
        je       test12_done
        /* Null deref if we have incorrect eflags */
        xor      TEST_REG_ASM, TEST_REG_ASM
        mov      PTRSZ [TEST_REG_ASM], TEST_REG_ASM
        jmp      test12_done
     test12_done:
        jmp     test13
        /* Test 13: Multi-phase reg spill slot conflicts. */
     test13:
        mov      TEST_REG_ASM, DRREG_TEST_13_ASM
        mov      TEST_REG_ASM, DRREG_TEST_13_ASM
        nop
        jmp      test13_done
     test13_done:
        /* Fail if reg was not restored correctly. */
        cmp      TEST_REG_ASM, DRREG_TEST_13_ASM
        je       epilog
        ud2

     epilog:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
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

        b        test13
        /* Test 13: Multi-phase reg spill slot conflicts. */
     test13:
        movw     TEST_REG_ASM, DRREG_TEST_13_ASM
        movw     TEST_REG_ASM, DRREG_TEST_13_ASM
        nop
        b        test13_done
     test13_done:
        /* Fail if reg was not restored correctly. */
        movw     TEST_REG2_ASM, DRREG_TEST_13_ASM
        cmp      TEST_REG_ASM, TEST_REG2_ASM
        beq      epilog
        .word 0xe7f000f0 /* udf */

    epilog:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
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

        b        test13
        /* Test 13: Multi-phase reg spill slot conflicts. */
     test13:
        movz     TEST_REG_ASM, DRREG_TEST_13_ASM
        movz     TEST_REG_ASM, DRREG_TEST_13_ASM
        nop
        b        test13_done
     test13_done:
        /* Fail if reg was not restored correctly. */
        movz     TEST_REG2_ASM, DRREG_TEST_13_ASM
        cmp      TEST_REG_ASM, TEST_REG2_ASM
        beq      epilog
        .inst 0xf36d19 /* udf */

    epilog:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME test_asm_faultA
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
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
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
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
        /* XXX i#3289: prologue missing */
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
        PUSH_CALLEE_SAVED_REGS()
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
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
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
        /* XXX i#3289: prologue missing */
        b        test5
        /* Test 5: fault aflags restore */
     test5:
        movz     TEST_REG_ASM, DRREG_TEST_AFLAGS_H_ASM, LSL 16
        movz     xzr, DRREG_TEST_5_ASM
        movz     xzr, DRREG_TEST_5_ASM
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

#define FUNCNAME test_asm_faultC
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test6
        /* Test 6: fault check ignore 3rd DR TLS slot */
     test6:
        mov      TEST_REG_ASM, DRREG_TEST_6_ASM
        mov      TEST_REG_ASM, DRREG_TEST_6_ASM
        nop
        mov      TEST_REG_ASM, DRREG_TEST_7_ASM
        nop
        ud2

        jmp      epilog6
     epilog6:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        /* Test 6: doesn't exist for ARM */
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        /* Test 6: doesn't exist for AARCH64 */
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME test_asm_faultD
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
#ifdef X64
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG
        /* XXX i#3312: Temporarily disable test until bug has been fixed. */
#if 0

        jmp      test8
        /* Test 8: test fault restore of non-public DR slot used by mangling.
         * Making sure drreg ignores restoring this slot.
         */
     test8:
        mov      PTRSZ [REG_XSP], REG_XAX
        sub      REG_XSP, 8
        mov      TEST_REG_ASM, DRREG_TEST_8_ASM
        mov      TEST_REG_ASM, DRREG_TEST_8_ASM
        nop
        mov      REG_XAX, DRREG_TEST_9_ASM
        /* The address will get mangled into register REG_XAX. */
        add      TEST_REG_ASM, PTRSZ SYMREF(-0x7fffffff) /* crash */

        jmp      epilog8
     epilog8:
        add      REG_XSP, 8
        mov      REG_XAX, PTRSZ [REG_XSP]
#endif
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

#define FUNCNAME test_asm_faultE
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
#ifdef X64
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

          /* XXX i#3312: Temporarily disable test until bug has been fixed. */
#if 0
OB        jmp      test10
        /* Test 10: test fault restore of non-public DR slot used by mangling,
         * when rip-rel address is forced to be in register. Making sure drreg ignores
         * restoring this slot.
         */
     test10:
        mov      PTRSZ [REG_XSP], REG_XAX
        sub      REG_XSP, 8
        mov      TEST_REG_ASM, DRREG_TEST_10_ASM
        mov      TEST_REG_ASM, DRREG_TEST_10_ASM
        nop
        mov      REG_XAX, DRREG_TEST_11_ASM
        /* The address will get mangled into register REG_XAX. */
        add      REG_XAX, PTRSZ SYMREF(-0x7fffffff) /* crash */

        jmp      epilog10
     epilog10:
        add      REG_XSP, 8
        mov      REG_XAX, PTRSZ [REG_XSP]
#endif
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
#endif
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        /* Test 10: not implemented for ARM */
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        /* Test 10: not implemented for AARCH64 */
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 14: restore on fault for gpr reserved in multiple phases,
         * where the two spill regions are nested. In this case, the reg
         * will be restored from the spill slot used by the first (app2app)
         * phase.
         */
#define FUNCNAME test_asm_faultF
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test14
     test14:
        mov      TEST_REG_ASM, DRREG_TEST_14_ASM
        mov      TEST_REG_ASM, DRREG_TEST_14_ASM
        nop
        ud2

        jmp      epilog14
     epilog14:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        b        test14
     test14:
        movw     TEST_REG_ASM, DRREG_TEST_14_ASM
        movw     TEST_REG_ASM, DRREG_TEST_14_ASM
        nop
        .word 0xe7f000f0 /* udf */

        b        epilog14
    epilog14:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test14
     test14:
        movz     TEST_REG_ASM, DRREG_TEST_14_ASM
        movz     TEST_REG_ASM, DRREG_TEST_14_ASM
        nop
        .inst 0xf36d19 /* udf */

        b        epilog14
    epilog14:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 15: restore on fault for aflags stored in xax without preceding
         * xax spill.
         */
#define FUNCNAME test_asm_faultG
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test15
     test15:
        mov      TEST_REG_ASM, DRREG_TEST_15_ASM
        mov      TEST_REG_ASM, DRREG_TEST_15_ASM
        mov      ah, DRREG_TEST_AFLAGS_ASM
        sahf
        nop
        ud2
        /* xax is dead, so should not need to spill when reserving aflags. */
        mov      REG_XAX, 0
        jmp      epilog15
     epilog15:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
        /* This test does not have AArchXX variants. */
#elif defined(ARM)
        bx       lr
#elif defined(AARCH64)
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 16: restore on fault for reg restored once (for app read)
         * before crash.
         */
#define FUNCNAME test_asm_faultH
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test16
     test16:
        mov      TEST_REG_ASM, DRREG_TEST_16_ASM
        mov      TEST_REG_ASM, DRREG_TEST_16_ASM
        nop
        /* Read reg so that it is restored once. */
        add      TEST_REG2_ASM, TEST_REG_ASM
        mov      REG_XCX, 0
        mov      REG_XCX, PTRSZ [REG_XCX] /* crash */
        jmp      epilog16
     epilog16:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        b        test16
     test16:
        movw     TEST_REG_ASM, DRREG_TEST_16_ASM
        movw     TEST_REG_ASM, DRREG_TEST_16_ASM
        nop
        /* Read reg so that it is restored once. */
        add      TEST_REG2_ASM, TEST_REG_ASM, TEST_REG_ASM
        mov      r0, HEX(0)
        ldr      r0, PTRSZ [r0] /* crash */

        b        epilog16
    epilog16:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test16
     test16:
        movz     TEST_REG_ASM, DRREG_TEST_16_ASM
        movz     TEST_REG_ASM, DRREG_TEST_16_ASM
        nop
        /* Read reg so that it is restored once. */
        add      TEST_REG2_ASM, TEST_REG_ASM, TEST_REG_ASM
        mov      x0, HEX(0)
        ldr      x0, PTRSZ [x0] /* crash */

        b        epilog16
    epilog16:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 17: restore on fault for gpr reserved in multiple phases
         * with overlapping but not nested spill regions. In this case,
         * the app value changes slots, from the one used in app2app
         * phase, to the one used in insertion phase.
         */
#define FUNCNAME test_asm_faultI
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test17
     test17:
        mov      TEST_REG_ASM, DRREG_TEST_17_ASM
        mov      TEST_REG_ASM, DRREG_TEST_17_ASM
        /* app2app phase will reserve TEST_REG_ASM here. */
        mov      TEST_REG2_ASM, 1
        /* insertion phase will reserve TEST_REG_ASM here. */
        mov      TEST_REG2_ASM, 2
        /* app2app phase will release TEST_REG_ASM here. */
        mov      TEST_REG2_ASM, 3
        mov      REG_XCX, 0
        mov      REG_XCX, PTRSZ [REG_XCX] /* crash */
        /* insertion phase will release TEST_REG_ASM here. */
        jmp      epilog17
     epilog17:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        b        test17
     test17:
        movw     TEST_REG_ASM, DRREG_TEST_17_ASM
        movw     TEST_REG_ASM, DRREG_TEST_17_ASM
        movw     TEST_REG2_ASM, 1
        movw     TEST_REG2_ASM, 2
        movw     TEST_REG2_ASM, 3
        mov      r0, HEX(0)
        ldr      r0, PTRSZ [r0] /* crash */

        b        epilog17
    epilog17:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        /* TODO PR#4917: This AArch64 variant doesn't work completely as
         * intended. This test currently won't fail even if the expected
         * TEST_REG_ASM restore doesn't happen. This is because at the
         * faulting instr, the TEST_REG_ASM app val is present in the
         * spill slot reserved by the insertion phase (spill slot 2; slot
         * 0 is reserved for aflags, slot 1 is used by app2app phase).
         * Slot 2 is a DR slot and all regs spilled to DR slot are
         * automatically restored before each app instr.
         * After PR#4917 though, aflags slot won't be hard-coded and
         * will be available for gprs (here, TEST_REG_ASM). Therefore,
         * TEST_REG_ASM will be stored in a TLS slot instead of a DR slot
         * and won't be automatically restored, and this test will really
         * verify the restore logic.
         * Note that the above isn't true for X86 because drreg internally
         * adds one extra spill slot for X86.
         */
        b        test17
     test17:
        movz     TEST_REG_ASM, DRREG_TEST_17_ASM
        movz     TEST_REG_ASM, DRREG_TEST_17_ASM
        movz     TEST_REG2_ASM, 1
        movz     TEST_REG2_ASM, 2
        movz     TEST_REG2_ASM, 3
        mov      x0, HEX(0)
        ldr      x0, PTRSZ [x0] /* crash */

        b        epilog17
    epilog17:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 18: fault reg restore for fragments with DR_EMIT_STORE_TRANSLATIONS */
#define FUNCNAME test_asm_faultJ
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test18
     test18:
        mov      TEST_REG_ASM, DRREG_TEST_18_ASM
        mov      TEST_REG_ASM, DRREG_TEST_18_ASM
        nop
        ud2

        jmp      epilog18
     epilog18:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        b        test18
     test18:
        movw     TEST_REG_ASM, DRREG_TEST_18_ASM
        movw     TEST_REG_ASM, DRREG_TEST_18_ASM
        nop
        .word 0xe7f000f0 /* udf */

        b        epilog18
    epilog18:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test18
     test18:
        movz     TEST_REG_ASM, DRREG_TEST_18_ASM
        movz     TEST_REG_ASM, DRREG_TEST_18_ASM
        nop
        .inst 0xf36d19 /* udf */

        b        epilog18
    epilog18:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

    /* Test 19: Test fault reg restore for fragments with a faux spill
     * instr -- an app instr that looks like a drreg spill instr, which
     * may corrupt drreg state restoration. This cannot happen on x86 as
     * an app instr that uses the %gs register will be mangled into a
     * non-far memref.
     */
#define FUNCNAME test_asm_faultK
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        END_PROLOG
        ret
#elif defined(ARM)
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test19
     test19:
        movz     TEST_REG_ASM, DRREG_TEST_19_ASM
        movz     TEST_REG_ASM, DRREG_TEST_19_ASM
        /* TEST_REG_ASM is reserved here. */
        movz     TEST_REG2_ASM, 1
        adr      TEST_REG_STOLEN_ASM, some_data
        /* A faux spill instr -- looks like a drreg spill but isn't.
         * It will seem as if the spill slot used for TEST_REG_ASM
         * is being overwritten.
         */
        str      TEST_REG2_ASM, PTRSZ [TEST_REG_STOLEN_ASM, #TEST_FAUX_SPILL_TLS_OFFS]

        mov      x0, HEX(0)
        ldr      x0, PTRSZ [x0] /* crash */

        b        epilog19
    epilog19:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 20: Test restore on fault for gpr reserved in multiple
         * phases, where the two spill regions are nested, and the first
         * phase doesn't write the reg before the second reservation. This
         * is to verify that drreg state restoration logic remembers that
         * the app value can be found in both the spill slots.
         */
#define FUNCNAME test_asm_faultL
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test20
     test20:
        mov      TEST_REG_ASM, DRREG_TEST_20_ASM
        mov      TEST_REG_ASM, DRREG_TEST_20_ASM
        /* - app2app reserves TEST_REG_ASM here, but doesn't write it.
         * - insertion reserves TEST_REG_ASM here, which may confuse the
         *   state restoration logic into overwritting the spill slot for
         *   TEST_REG_ASM as it still has its native value.
         */
        mov      TEST_REG2_ASM, 1
        /* - insertion phase unreserves TEST_REG_ASM and frees the spill
         *   slot.
         */
        mov      TEST_REG2_ASM, 2
        /* - insertion phase reserves TEST_REG2_ASM which would use the
         *   same spill slot as freed above, and overwrite TEST_REG_ASM
         *   value stored there currently. After this TEST_REG_ASM can
         *   only be found in its app2app spill slot.
         * - insertion phase writes to TEST_REG_ASM so that we need to
         *   restore it.
         */
        mov      TEST_REG2_ASM, 3
        ud2

        jmp      epilog20
     epilog20:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        b        test20
     test20:
        movw     TEST_REG_ASM, DRREG_TEST_20_ASM
        movw     TEST_REG_ASM, DRREG_TEST_20_ASM
        movw     TEST_REG2_ASM, 1
        movw     TEST_REG2_ASM, 2
        movw     TEST_REG2_ASM, 3
        .word 0xe7f000f0 /* udf */

        b        epilog20
    epilog20:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test20
     test20:
        movz     TEST_REG_ASM, DRREG_TEST_20_ASM
        movz     TEST_REG_ASM, DRREG_TEST_20_ASM
        movz     TEST_REG2_ASM, 1
        movz     TEST_REG2_ASM, 2
        movz     TEST_REG2_ASM, 3
        .inst 0xf36d19 /* udf */

        b        epilog20
    epilog20:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME
START_DATA
    /* Should be atleast (TEST_FAUX_SPILL_TLS_OFFS+1)*8 bytes.
     * Cannot use the macro as the expression needs to be
     * absolute.
     */
    BYTES_ARR(some_data, (1000+1)*8)
END_FILE
#endif
/* clang-format on */
