/* **********************************************************
 * Copyright (c) 2012-2018 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
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

#ifndef ASM_CODE_ONLY  /* C code */
#    include "tools.h" /* for print() */
#    include <assert.h>
#    include <stdio.h>
#    include <math.h>

#    ifdef UNIX
#        include <unistd.h>
#        include <signal.h>
#        include <ucontext.h>
#        include <errno.h>
#        include <stdlib.h>
#    endif

#    include <setjmp.h>

/* asm routines */
void
test_priv_0(void);
void
test_priv_1(void);
void
test_priv_2(void);
void
test_priv_3(void);
void
test_prefix_0(void);
void
test_prefix_1(void);
void
test_inval_0(void);
void
test_inval_1(void);
void
test_inval_2(void);
void
test_inval_3(void);
void
test_inval_4(void);
void
test_inval_5(void);
void
test_inval_6(void);
void
test_inval_7(void);

SIGJMP_BUF mark;
static int count = 0;
static bool invalid_lock;

#    ifdef UNIX
#        ifndef X86
#            error NYI /* FIXME i#1551, i#1569: port asm to ARM and AArch64 */
#        endif
static void
signal_handler(int sig, siginfo_t *info, ucontext_t *ucxt)
{
    if (sig == SIGILL) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (info->si_addr != (void *)sc->SC_XIP) {
            print("ERROR: si_addr=%p does not match rip=%p\n", info->si_addr, sc->SC_XIP);
        }
        count++;
        if (invalid_lock) {
            print("Invalid lock sequence, instance %d\n", count);
            /* add this so output matches test on windows, FIXME : would like to test
             * this on linux too, but won't work now (bug 651, 832) */
            print("eax=1 ebx=2 ecx=3 edx=4 edi=5 esi=6 ebp=7\n");
        } else
            print("Bad instruction, instance %d\n", count);
        SIGLONGJMP(mark, count);
    }
    if (sig == SIGSEGV) {
        count++;
        /* We can't distinguish but this is the only segv we expect */
        print("Privileged instruction, instance %d\n", count);
        SIGLONGJMP(mark, count);
    }
    exit(-1);
}
#    else
/* sort of a hack to avoid the MessageBox of the unhandled exception spoiling
 * our batch runs
 */
#        include <windows.h>
/* top-level exception handler */
static LONG
our_top_handler(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode ==
            /* Windows doesn't have an EXCEPTION_ constant to equal the invalid lock code
             */
            0xc000001e /*STATUS_INVALID_LOCK_SEQUENCE*/
        /* FIXME: DR doesn't generate the invalid lock exception */
        ||
        (invalid_lock &&
         pExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_ILLEGAL_INSTRUCTION)) {
        CONTEXT *cxt = pExceptionInfo->ContextRecord;
        count++;
        print("Invalid lock sequence, instance %d\n", count);
        /* FIXME : add CXT_XFLAGS (currently comes back incorrect), eip, esp? */
        print("eax=" SZFMT " ebx=" SZFMT " ecx=" SZFMT " edx=" SZFMT " "
              "edi=" SZFMT " esi=" SZFMT " ebp=" SZFMT "\n",
              cxt->CXT_XAX, cxt->CXT_XBX, cxt->CXT_XCX, cxt->CXT_XDX, cxt->CXT_XDI,
              cxt->CXT_XSI, cxt->CXT_XBP);
        SIGLONGJMP(mark, count);
    }
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_ILLEGAL_INSTRUCTION) {
        count++;
        print("Bad instruction, instance %d\n", count);
        SIGLONGJMP(mark, count);
    }
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_PRIVILEGED_INSTRUCTION) {
        count++;
        print("Privileged instruction, instance %d\n", count);
        SIGLONGJMP(mark, count);
    }
    /* Shouldn't get here in normal operation so this isn't #if VERBOSE */
    print("Exception 0x" PFMT " occurred, process about to die silently\n",
          pExceptionInfo->ExceptionRecord->ExceptionCode);
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        print("\tPC " PFX " tried to %s address " PFX "\n",
              pExceptionInfo->ExceptionRecord->ExceptionAddress,
              (pExceptionInfo->ExceptionRecord->ExceptionInformation[0] == 0) ? "read"
                                                                              : "write",
              pExceptionInfo->ExceptionRecord->ExceptionInformation[1]);
    }
    return EXCEPTION_EXECUTE_HANDLER; /* => global unwind and silent death */
}
#    endif

int
main(int argc, char *argv[])
{
    double res = 0.;
    int i;

#    ifdef UNIX
    intercept_signal(SIGILL, (handler_3_t)signal_handler, false);
    intercept_signal(SIGSEGV, (handler_3_t)signal_handler, false);
#    else
#        ifdef X64_DEBUGGER
    /* FIXME: the vectored handler works fine in the debugger, but natively
     * the app crashes here: yet the SetUnhandled hits infinite fault loops
     * in the debugger, and works fine natively!
     */
    AddVectoredExceptionHandler(1 /*first*/,
                                (PVECTORED_EXCEPTION_HANDLER)our_top_handler);
#        else
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)our_top_handler);
#        endif
#    endif

    /* privileged instructions */
    print("Privileged instructions about to happen\n");
    count = 0;
    i = SIGSETJMP(mark);
    switch (i) {
    case 0: test_priv_0();
    case 1: test_priv_1();
    case 2: test_priv_2();
    case 3: test_priv_3();
    }

    /* prefix tests */
    print("OK instr about to happen\n");
    /* multiple prefixes */
    /* FIXME: actually these prefixes on a jmp are "reserved" but this seems to work */
    test_prefix_0();
    print("Bad instr about to happen\n");
    /* lock prefix, which is illegal instruction if placed on jmp */
    count = 0;
    invalid_lock = true;
    if (SIGSETJMP(mark) == 0) {
        test_prefix_1();
    }
    invalid_lock = false;

    print("Invalid instructions about to happen\n");
    count = 0;
    i = SIGSETJMP(mark);
    switch (i) {
        /* note that we decode until a CTI, so for every case the suffix is decoded
         * and changes in later cases may fail even the earlier ones.
         */
    case 0: test_inval_0();
    case 1: test_inval_1();
    case 2: test_inval_2();
    case 3: test_inval_3();
    case 4: test_inval_4();
    case 5: test_inval_5();
    case 6: test_inval_6();
    case 7: test_inval_7();
    default:;
    }

    print("All done\n");
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

#define FUNCNAME test_priv_0
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov  REG_XAX, dr0
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME
#define FUNCNAME test_priv_1
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov  dr7, REG_XAX
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME
#define FUNCNAME test_priv_2
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov  REG_XAX, cr0
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME
#define FUNCNAME test_priv_3
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov  cr3, REG_XAX
        ret
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME test_prefix_0
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
# ifdef X64
        /* avoid ASSERT "no support yet for application using non-NPTL segment" */
        RAW(64) RAW(f2) RAW(f3) RAW(eb) RAW(00)
# else
        RAW(65) RAW(f2) RAW(f3) RAW(eb) RAW(00)
# endif
        ret
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME test_prefix_1
       /* If we make this a leaf function (no SEH directives), our top-level handler
        * does not get called!  Annoying.
        */
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        PUSH_CALLEE_SAVED_REGS()
        END_PROLOG
        mov  eax, 1
        mov  ebx, 2
        mov  ecx, 3
        mov  edx, 4
        mov  edi, 5
        mov  esi, 6
        mov  ebp, 7
        RAW(f0) RAW(eb) RAW(00)
        add      REG_XSP, 0 /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME test_inval_0
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        RAW(df) RAW(fa)
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME
#define FUNCNAME test_inval_1
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        RAW(0f) RAW(04)
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME
#define FUNCNAME test_inval_2
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        RAW(fe) RAW(30)
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME
#define FUNCNAME test_inval_3
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        RAW(ff) RAW(38)
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME
#define FUNCNAME test_inval_4
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        RAW(f3) RAW(0f) RAW(13)
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME
#define FUNCNAME test_inval_5
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* case 8840 we crash after going through this bb */
        /* ud2 */
        RAW(0f) RAW(0b)
        RAW(20) RAW(0f)
#if 0
        RAW(8c) RAW(c6)
#endif
        RAW(ff) RAW(ff)
        RAW(ff) RAW(d9)
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME
#define FUNCNAME test_inval_6
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* case 6962 - invalid Mod byte for a call far,
         * so should get #UD as well
         * we should make sure we either mark invalid during decode
         * or properly detect it is invalid during execution
         */
        /* we just have to force ecx = 0x13 to make sure the same result */
        RAW(ff) RAW(d9)
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME
#define FUNCNAME test_inval_7
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
         /* Although data16 means it is 4 bytes and fits in
         * a register, this is invalid.
         */
        RAW(66) RAW(ff) RAW(d9)
        ret
        END_FUNC(FUNCNAME)

END_FILE
/* clang-format on */
#endif
