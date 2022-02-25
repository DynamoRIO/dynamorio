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
test_asm_fault_restore_gpr();
void
test_asm_fault_restore_aflags_in_slot();
void
test_asm_fault_restore_ignore_3rd_dr_tls_slot();
void
test_asm_fault_restore_non_public_dr_slot();
void
test_asm_fault_restore_non_public_dr_slot_rip_rel_addr_in_reg();
void
test_asm_fault_restore_multi_phase_gpr_nested_spill_regions();
void
test_asm_fault_restore_aflags_in_xax();
void
test_asm_fault_restore_gpr_restored_for_read();
void
test_asm_fault_restore_multi_phase_gpr_overlapping_spill_regions();
void
test_asm_fault_restore_gpr_store_xl8();
void
test_asm_fault_restore_faux_gpr_spill();
void
test_asm_fault_restore_multi_phase_native_gpr_spilled_twice();
void
test_asm_fault_restore_multi_phase_aflags_nested_spill_regions();
void
test_asm_fault_restore_multi_phase_aflags_overlapping_spill_regions();
void
test_asm_fault_restore_aflags_restored_for_read();
void
test_asm_fault_restore_multi_phase_native_aflags_spilled_twice();
void
test_asm_fault_restore_aflags_in_slot_store_xl8();
void
test_asm_fault_restore_aflags_in_xax_store_xl8();
void
test_asm_fault_restore_aflags_xax_already_spilled();
void
test_asm_fault_restore_gpr_spilled_to_mcontext_later();
void
test_asm_fault_restore_aflags_spilled_to_mcontext_later();
void
test_asm_fault_restore_gpr_spilled_during_clean_call_later();
void
test_asm_fault_restore_aflags_spilled_during_clean_call_later();
void
test_asm_fault_restore_gpr_spilled_to_mcontext_between();
void
test_asm_fault_restore_aflags_spilled_to_mcontext_between();
void
test_asm_fault_restore_multi_phase_gpr_nested_spill_regions_insertion_outer();
void
test_asm_fault_restore_multi_phase_aflags_nested_spill_regions_insertion_outer();

static SIGJMP_BUF mark;

#    if defined(UNIX)
#        include <signal.h>
static void
handle_signal_test_asm(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    print("ERROR: did not expect any signal!\n");
    SIGLONGJMP(mark, 1);
}

static void
handle_signal_gpr_aflags_in_slot(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
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
handle_signal_ignore_3rd_slot(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGILL) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sc->TEST_REG_SIG != DRREG_TEST_7_C)
            print("ERROR: spilled register value was not preserved!\n");
    }
    SIGLONGJMP(mark, 1);
}

static void
handle_signal_non_public_slot(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
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
handle_signal_non_public_slot_rip_rel(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
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
handle_signal_multi_phase_gpr(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
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
handle_signal_aflags_xax_gpr_read(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
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
handle_signal_gpr_xl8_faux_gpr_spill(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
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
handle_signal_gpr_multi_spill_aflags_nested(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGILL) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sc->TEST_REG_SIG != DRREG_TEST_20_C)
            print("ERROR: spilled register value was not preserved in test #20!\n");
    } else if (signal == SIGSEGV) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (!TESTALL(DRREG_TEST_AFLAGS_C, sc->TEST_FLAGS_SIG))
            print("ERROR: spilled flags value was not preserved in test #21!\n");
    }
    SIGLONGJMP(mark, 1);
}

static void
handle_signal_multi_phase_aflags_overlapping(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGSEGV) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (!TESTALL(DRREG_TEST_AFLAGS_C, sc->TEST_FLAGS_SIG))
            print("ERROR: spilled flags value was not preserved in test #23!\n");
    }
    SIGLONGJMP(mark, 1);
}

static void
handle_signal_aflags_read(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGSEGV) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (!TESTALL(DRREG_TEST_AFLAGS_C, sc->TEST_FLAGS_SIG))
            print("ERROR: spilled flags value was not preserved in test #24!\n");
    }
    SIGLONGJMP(mark, 1);
}

static void
handle_signal_aflags_multi_spill(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGSEGV) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (!TESTALL(DRREG_TEST_AFLAGS_C, sc->TEST_FLAGS_SIG))
            print("ERROR: spilled flags value was not preserved in test #25!\n");
    }
    SIGLONGJMP(mark, 1);
}

static void
handle_signal_aflags_xl8(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGSEGV) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (!TESTALL(DRREG_TEST_AFLAGS_C, sc->TEST_FLAGS_SIG))
            print("ERROR: spilled flags value was not preserved in test #26!\n");
    } else if (signal == SIGILL) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (!TESTALL(DRREG_TEST_AFLAGS_C, sc->TEST_FLAGS_SIG))
            print("ERROR: spilled flags value was not preserved in test #27!\n");
    }
    SIGLONGJMP(mark, 1);
}

static void
handle_signal_aflags_xax_already_spilled(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGILL) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (!TESTALL(DRREG_TEST_AFLAGS_C, sc->TEST_FLAGS_SIG))
            print("ERROR: spilled flags value was not preserved in test #29!\n");
    }
    SIGLONGJMP(mark, 1);
}

static void
handle_signal_spilled_to_mcontext_later(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGILL) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sc->TEST_REG_SIG != DRREG_TEST_30_C)
            print("ERROR: spilled register value was not preserved in test #30!\n");
    } else if (signal == SIGSEGV) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (!TESTALL(DRREG_TEST_AFLAGS_C, sc->TEST_FLAGS_SIG))
            print("ERROR: spilled flags value was not preserved in test #31!\n");
    }
    SIGLONGJMP(mark, 1);
}

static void
handle_signal_spilled_during_clean_call_later(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGILL) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sc->TEST_REG_CLEAN_CALL_MCONTEXT_SIG != DRREG_TEST_32_C)
            print("ERROR: spilled register value was not preserved in test #32!\n");
    } else if (signal == SIGSEGV) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (!TESTALL(DRREG_TEST_AFLAGS_C, sc->TEST_FLAGS_SIG))
            print("ERROR: spilled flags value was not preserved in test #33!\n");
    }
    SIGLONGJMP(mark, 1);
}

static void
handle_signal_spilled_to_mcontext_between(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGILL) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sc->TEST_REG_SIG != DRREG_TEST_34_C)
            print("ERROR: spilled register value was not preserved in test #34!\n");
    } else if (signal == SIGSEGV) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (!TESTALL(DRREG_TEST_AFLAGS_C, sc->TEST_FLAGS_SIG))
            print("ERROR: spilled flags value was not preserved in test #35!\n");
    }
    SIGLONGJMP(mark, 1);
}

static void
handle_signal_nested_gpr_aflags_spill_insertion_outer(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGILL) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sc->TEST_REG_SIG != DRREG_TEST_36_C)
            print("ERROR: spilled register value was not preserved in test #36!\n");
    } else if (signal == SIGSEGV) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (!TESTALL(DRREG_TEST_AFLAGS_C, sc->TEST_FLAGS_SIG))
            print("ERROR: spilled flags value was not preserved in test #37!\n");
    }
    SIGLONGJMP(mark, 1);
}

#    elif defined(WINDOWS)
#        include <windows.h>
static LONG WINAPI
handle_exception_test_asm(struct _EXCEPTION_POINTERS *ep)
{
    print("ERROR: did not expect any signal!\n");
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception_gpr_aflags_in_slot(struct _EXCEPTION_POINTERS *ep)
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
handle_exception_ignore_3rd_slot(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (ep->ContextRecord->TEST_REG_CXT != DRREG_TEST_7_C)
            print("ERROR: spilled register value was not preserved!\n");
    }
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception_non_public_slot(struct _EXCEPTION_POINTERS *ep)
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
handle_exception_non_public_slot_rip_rel(struct _EXCEPTION_POINTERS *ep)
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
handle_exception_multi_phase_gpr(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (ep->ContextRecord->TEST_REG_CXT != DRREG_TEST_14_C)
            print("ERROR: spilled register value was not preserved!\n");
    }
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception_aflags_xax_gpr_read(struct _EXCEPTION_POINTERS *ep)
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
handle_exception_gpr_xl8_faux_gpr_spill(struct _EXCEPTION_POINTERS *ep)
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
handle_exception_gpr_multi_spill_aflags_nested(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (ep->ContextRecord->TEST_REG_CXT != DRREG_TEST_20_C)
            print("ERROR: spilled register value was not preserved in test #20!\n");
    } else if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        if (!TESTALL(DRREG_TEST_AFLAGS_C, ep->ContextRecord->CXT_XFLAGS))
            print("ERROR: spilled flags value was not preserved in test #21!\n");
    }
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception_multi_phase_aflags_overlapping(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        if (!TESTALL(DRREG_TEST_AFLAGS_C, ep->ContextRecord->CXT_XFLAGS))
            print("ERROR: spilled flags value was not preserved in test #23!\n");
    }
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception_aflags_read(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        if (!TESTALL(DRREG_TEST_AFLAGS_C, ep->ContextRecord->CXT_XFLAGS))
            print("ERROR: spilled flags value was not preserved in test #24!\n");
    }
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception_aflags_multi_spill(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        if (!TESTALL(DRREG_TEST_AFLAGS_C, ep->ContextRecord->CXT_XFLAGS))
            print("ERROR: spilled flags value was not preserved in test #25!\n");
    }
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception_aflags_xl8(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        if (!TESTALL(DRREG_TEST_AFLAGS_C, ep->ContextRecord->CXT_XFLAGS))
            print("ERROR: spilled flags value was not preserved in test #26!\n");
    } else if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (!TESTALL(DRREG_TEST_AFLAGS_C, ep->ContextRecord->CXT_XFLAGS))
            print("ERROR: spilled flags value was not preserved in test #27!\n");
    }
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception_aflags_xax_already_spilled(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (!TESTALL(DRREG_TEST_AFLAGS_C, ep->ContextRecord->CXT_XFLAGS))
            print("ERROR: spilled flags value was not preserved in test #29!\n");
    }
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception_spilled_to_mcontext_later(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (ep->ContextRecord->TEST_REG_CXT != DRREG_TEST_30_C)
            print("ERROR: spilled register value was not preserved in test #30!\n");
    } else if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        if (!TESTALL(DRREG_TEST_AFLAGS_C, ep->ContextRecord->CXT_XFLAGS))
            print("ERROR: spilled flags value was not preserved in test #31!\n");
    }
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception_spilled_during_clean_call_later(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (ep->ContextRecord->TEST_REG_CLEAN_CALL_MCONTEXT_CXT != DRREG_TEST_32_C)
            print("ERROR: spilled register value was not preserved in test #32!\n");
    } else if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        if (!TESTALL(DRREG_TEST_AFLAGS_C, ep->ContextRecord->CXT_XFLAGS))
            print("ERROR: spilled flags value was not preserved in test #33!\n");
    }
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception_spilled_to_mcontext_between(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (ep->ContextRecord->TEST_REG_CXT != DRREG_TEST_34_C)
            print("ERROR: spilled register value was not preserved in test #34!\n");
    } else if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        if (!TESTALL(DRREG_TEST_AFLAGS_C, ep->ContextRecord->CXT_XFLAGS))
            print("ERROR: spilled flags value was not preserved in test #35!\n");
    }
    SIGLONGJMP(mark, 1);
}

static LONG WINAPI
handle_exception_nested_gpr_aflags_spill_insertion_outer(struct _EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (ep->ContextRecord->TEST_REG_CXT != DRREG_TEST_36_C)
            print("ERROR: spilled register value was not preserved in test #36!\n");
    } else if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        if (!TESTALL(DRREG_TEST_AFLAGS_C, ep->ContextRecord->CXT_XFLAGS))
            print("ERROR: spilled flags value was not preserved in test #37!\n");
    }
    SIGLONGJMP(mark, 1);
}
#    endif

int
main(int argc, const char *argv[])
{
#    if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal_test_asm, false);
    intercept_signal(SIGILL, (handler_3_t)&handle_signal_test_asm, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception_test_asm);
#    endif

    print("drreg-test running\n");

    if (SIGSETJMP(mark) == 0) {
        test_asm();
    }

#    if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal_gpr_aflags_in_slot, false);
    intercept_signal(SIGILL, (handler_3_t)&handle_signal_gpr_aflags_in_slot, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception_gpr_aflags_in_slot);
#    endif

    /* Test fault reg restore */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_gpr();
    }

    /* Test fault aflags restore */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_aflags_in_slot();
    }

#    if defined(UNIX)
    intercept_signal(SIGILL, (handler_3_t)&handle_signal_ignore_3rd_slot, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception_ignore_3rd_slot);
#    endif

    /* Test fault check ignore 3rd DR TLS slot */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_ignore_3rd_dr_tls_slot();
    }

#    if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal_non_public_slot, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception_non_public_slot);
#    endif

    /* Test fault restore of non-public DR slot used by mangling.
     * Making sure drreg ignores restoring this slot.
     */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_non_public_dr_slot();
    }

#    if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal_non_public_slot_rip_rel, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception_non_public_slot_rip_rel);
#    endif

    /* Test 10: test fault restore of non-public DR slot used by mangling,
     * when rip-rel address is forced to be in register. Making sure drreg
     * ignores restoring this slot. Exposes transparency limitation of DR
     * if reg is optimized to be app's dead reg.
     */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_non_public_dr_slot_rip_rel_addr_in_reg();
    }

    #    if defined(UNIX)
    intercept_signal(SIGILL, (handler_3_t)&handle_signal_multi_phase_gpr, false);
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal_multi_phase_gpr, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception_multi_phase_gpr);
#    endif

    /* Test restore on fault for aflags reserved in multiple phases, with
     * nested spill regions, and the app2app phase spill being the outer one.
     */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_multi_phase_gpr_nested_spill_regions();
    }

    /* Test fault reg restore for multi-phase non-nested overlapping reservations. */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_multi_phase_gpr_overlapping_spill_regions();
    }

    #    if defined(UNIX)
    intercept_signal(SIGILL, (handler_3_t)&handle_signal_aflags_xax_gpr_read, false);
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal_aflags_xax_gpr_read, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception_aflags_xax_gpr_read);
#    endif

    /* Test fault aflags restore from xax. */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_aflags_in_xax();
    }

    /* Test fault gpr restore on fault when it has been restored before for an
     * app read.
     */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_gpr_restored_for_read();
    }

    #    if defined(UNIX)
    intercept_signal(SIGILL, (handler_3_t)&handle_signal_gpr_xl8_faux_gpr_spill, false);
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal_gpr_xl8_faux_gpr_spill, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception_gpr_xl8_faux_gpr_spill);
#    endif

    /* Test fault reg restore for fragments emitting DR_EMIT_STORE_TRANSLATIONS */
    if (SIGSETJMP(mark) == 0) {
       test_asm_fault_restore_gpr_store_xl8();
    }

    /* Test fault reg restore for fragments with a faux spill instr. */
    if (SIGSETJMP(mark) == 0) {
         test_asm_fault_restore_faux_gpr_spill();
    }

    #    if defined(UNIX)
    intercept_signal(SIGILL, (handler_3_t)&handle_signal_gpr_multi_spill_aflags_nested, false);
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal_gpr_multi_spill_aflags_nested, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception_gpr_multi_spill_aflags_nested);
#    endif

    /* Test fault reg restore for multi-phase nested reservation where
     * the first phase doesn't write the reg before the second
     * reservation.
     */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_multi_phase_native_gpr_spilled_twice();
    }

    /* XXX i#4849: For some aflags restore tests below we do not use SIGILL to
     * raise the fault. This is because the undefined instr on AArchXX is assumed
     * to read aflags, and therefore restores aflags automatically. So the
     * restore logic doesn't come into play.
     */

    /* Test restore on fault for aflags reserved in multiple phases, with
     * nested spill regions, and the app2app phase spill being the outer one.
     */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_multi_phase_aflags_nested_spill_regions();
    }

    #    if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal_multi_phase_aflags_overlapping, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception_multi_phase_aflags_overlapping);
#    endif

    /* Test restore on fault for aflags reserved in multiple phases
     * with overlapping but not nested spill regions.
     */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_multi_phase_aflags_overlapping_spill_regions();
    }

    #    if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal_aflags_read, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception_aflags_read);
#    endif

    /* Test restore on fault for aflags restored once (for app read)
     * before crash.
     */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_aflags_restored_for_read();
    }
    #    if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal_aflags_multi_spill, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception_aflags_multi_spill);
#    endif

    /* Test restore on fault for aflags when native aflags are spilled
     * to multiple slots initially.
     */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_multi_phase_native_aflags_spilled_twice();
    }

    #    if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal_aflags_xl8, false);
    intercept_signal(SIGILL, (handler_3_t)&handle_signal_aflags_xl8, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception_aflags_xl8);
#    endif

    /* Test restore on fault for aflags spilled to slot for fragment
     * emitting DR_EMIT_STORE_TRANSLATIONS.
     */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_aflags_in_slot_store_xl8();
    }

    /* Test restore on fault for aflags spilled to xax for fragment
     * emitting DR_EMIT_STORE_TRANSLATIONS.
     */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_aflags_in_xax_store_xl8();
    }

    #    if defined(UNIX)
    intercept_signal(SIGILL, (handler_3_t)&handle_signal_aflags_xax_already_spilled, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception_aflags_xax_already_spilled);
#    endif

    /* Test restore on fault for aflags stored in slot, when xax was
     * already spilled and in-use by instrumentation. This is to
     * verify that aflags are spilled using xax only.
     */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_aflags_xax_already_spilled();
    }

    #    if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal_spilled_to_mcontext_later, false);
    intercept_signal(SIGILL, (handler_3_t)&handle_signal_spilled_to_mcontext_later, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception_spilled_to_mcontext_later);
#    endif

    /* Test restore on fault for gpr spilled to mcontext later by non-drreg routines. */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_gpr_spilled_to_mcontext_later();
    }

    /* Test restore on fault for aflags spilled to mcontext later by non-drreg routines. */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_aflags_spilled_to_mcontext_later();
    }

    #    if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal_spilled_during_clean_call_later, false);
    intercept_signal(SIGILL, (handler_3_t)&handle_signal_spilled_during_clean_call_later, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception_spilled_during_clean_call_later);
#    endif

    /* Test restore on fault for gpr spilled during clean call instrumentation later. */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_gpr_spilled_during_clean_call_later();
    }

    /* Test restore on fault for aflags spilled during clean call instrumentation later. */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_aflags_spilled_during_clean_call_later();
    }

    #    if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal_spilled_to_mcontext_between, false);
    intercept_signal(SIGILL, (handler_3_t)&handle_signal_spilled_to_mcontext_between, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception_spilled_to_mcontext_between);
#    endif

    /* Test restore on fault for gpr spilled to mcontext in between its drreg spill region. */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_gpr_spilled_to_mcontext_between();
    }

    /* Test restore on fault for aflags spilled to mcontext in between its drreg spill region. */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_aflags_spilled_to_mcontext_between();
    }

    #    if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal_nested_gpr_aflags_spill_insertion_outer, false);
    intercept_signal(SIGILL, (handler_3_t)&handle_signal_nested_gpr_aflags_spill_insertion_outer, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception_nested_gpr_aflags_spill_insertion_outer);
#    endif

    /* Test restore on fault for gpr reserved in multiple phases, with
     * nested spill regions, and the insertion phase spill being the outer one.
     */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_multi_phase_gpr_nested_spill_regions_insertion_outer();
    }

    /* Test restore on fault for aflags reserved in multiple phases, with
     * nested spill regions, and the insertion phase spill being the outer one.
     */
    if (SIGSETJMP(mark) == 0) {
        test_asm_fault_restore_multi_phase_aflags_nested_spill_regions_insertion_outer();
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
        /* app2app phase will reserve TEST_REG_ASM here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        /* insertion phase will reserve TEST_REG_ASM here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* insertion phase will unreserve TEST_REG_ASM here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3
        /* app2app phase will unreserve TEST_REG_ASM here. */
        jmp      test13_done
     test13_done:
        /* Fail if reg was not restored correctly. */
        cmp      TEST_REG_ASM, DRREG_TEST_13_ASM
        je       test22
        ud2
        /* Test 22: Multi-phase aflags spill slot conflicts. */
     test22:
        mov      TEST_REG_ASM, DRREG_TEST_22_ASM
        mov      TEST_REG_ASM, DRREG_TEST_22_ASM
        /* Set overflow bit. */
        mov      al, 100
        add      al, 100
        /* Set other aflags. */
        mov      ah, DRREG_TEST_AFLAGS_ASM
        sahf

        /* app2app phase will reserve aflags here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        /* insertion phase will reserve aflags here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* insertion phase will unreserve aflags here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3
        /* app2app phase will unreserve aflags here. */
        jmp      test22_done
     test22_done:
        /* Fail if aflags were not restored correctly. */
        lahf
        seto     al
        cmp      ah, DRREG_TEST_AFLAGS_ASM
        jne      test22_fail
        cmp      al, 1
        jne      test22_fail
        jmp      test28
     test22_fail:
        ud2
        /* Unreachable, but we want this bb to end here. */
        jmp      test28

        /* Test 28: Aflags spilled to xax, and xax statelessly restored. */
     test28:
        mov      TEST_REG_ASM, DRREG_TEST_28_ASM
        mov      TEST_REG_ASM, DRREG_TEST_28_ASM
        /* Set overflow bit. */
        mov      al, 100
        add      al, 100
        /* Set other aflags. */
        mov      ah, DRREG_TEST_AFLAGS_ASM
        sahf
        /* aflags reserved here; spilled to xax. */
        mov      TEST_REG2_ASM, 1
        /* xax statelessly restored here. */
        mov      TEST_REG2_ASM, 2
        jmp      test28_done
     test28_done:
        /* Fail if aflags were not restored correctly. */
        lahf
        seto     al
        cmp      ah, DRREG_TEST_AFLAGS_ASM
        jne      test28_fail
        cmp      al, 1
        jne      test28_fail
        jmp      test38
     test28_fail:
        ud2
        /* Unreachable, but we want this bb to end here. */
        jmp      test38

        /* Test 38: Tests that the insertion phase slot contains the
         * correct app value when there's overlapping spill regions for
         * some reg due to multi-phase drreg use in app2app and insertion
         * phases. The insertion phase should update the reg value in its own
         * slot by re-spilling it after an app2app instruction that restored
         * the app value for an app read.
         */
     test38:
        mov      TEST_REG_ASM, DRREG_TEST_38_ASM
        mov      TEST_REG_ASM, DRREG_TEST_38_ASM
        /* app2app phase reserves TEST_REG_ASM here. */
        /* app2app phase writes TEST_REG_ASM here. */
        /* insertion phase reserves TEST_REG_ASM here,
         * storing the app2app value in its slot.
         */
        /* insertion phase writes TEST_REG_ASM here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        /* app2app unreserves TEST_REG_ASM here.
         * Seeing this app2app write, insertion phase automatically
         * re-spills TEST_REG_ASM to its slot.
         */
         /* insertion phase automatically restores TEST_REG_ASM
          * here, for the app read below.
          */
        mov      TEST_REG2_ASM, TEST_REG_ASM
        cmp      TEST_REG2_ASM, DRREG_TEST_38_ASM
        jne      test38_fail
     test38_done:
        jmp      epilog
     test38_fail:
        ud2
        /* Unreachable, but we want this bb to end here. */
        jmp      epilog

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
        /* app2app phase will reserve TEST_REG_ASM here. */
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        /* insertion phase will reserve TEST_REG_ASM here. */
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* insertion phase will unreserve TEST_REG_ASM here. */
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3
        /* app2app phase will unreserve TEST_REG_ASM here. */
        b        test13_done
     test13_done:
        /* Fail if reg was not restored correctly. */
        movw     TEST_REG2_ASM, DRREG_TEST_13_ASM
        cmp      TEST_REG_ASM, TEST_REG2_ASM
        beq      test22
        .word 0xe7f000f0 /* udf */
     test22:
        movw     TEST_REG_ASM, DRREG_TEST_22_ASM
        movw     TEST_REG_ASM, DRREG_TEST_22_ASM
        msr      APSR_nzcvq, DRREG_TEST_AFLAGS_ASM
        /* app2app phase will reserve aflags here. */
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        /* insertion phase will reserve aflags here. */
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* insertion phase will unreserve aflags here. */
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3
        /* app2app phase will unreserve aflags here. */
        b        test22_done
     test22_done:
        /* Fail if aflags were not restored correctly. */
        mrs      TEST_REG_ASM, APSR
        cmp      TEST_REG_ASM, DRREG_TEST_AFLAGS_ASM
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
        /* app2app phase will reserve TEST_REG_ASM here. */
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        /* insertion phase will reserve TEST_REG_ASM here. */
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* insertion phase will unreserve TEST_REG_ASM here. */
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3
        /* app2app phase will unreserve TEST_REG_ASM here. */
        b        test13_done
     test13_done:
        /* Fail if reg was not restored correctly. */
        movz     TEST_REG2_ASM, DRREG_TEST_13_ASM
        cmp      TEST_REG_ASM, TEST_REG2_ASM
        beq      test22
        .inst 0xf36d19 /* udf */

        /* Test 22: Multi-phase aflags spill slot conflicts. */
    test22:
        movz     TEST_REG_ASM, DRREG_TEST_22_ASM
        movz     TEST_REG_ASM, DRREG_TEST_22_ASM
        movz     TEST_REG2_ASM, DRREG_TEST_AFLAGS_H_ASM, LSL 16
        msr      nzcv, TEST_REG2_ASM
        /* app2app phase will reserve aflags here. */
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        /* insertion phase will reserve aflags here. */
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* insertion phase will unreserve aflags here. */
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3
        /* app2app phase will unreserve aflags here. */
        b        test22_done
     test22_done:
        /* Fail if aflags were not restored correctly. */
        movz     TEST_REG2_ASM, DRREG_TEST_AFLAGS_H_ASM, LSL 16
        mrs      TEST_REG_ASM, nzcv
        cmp      TEST_REG2_ASM, TEST_REG_ASM
        beq      epilog
        .inst 0xf36d19 /* udf */

    epilog:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME test_asm_fault_restore_gpr
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

#define FUNCNAME test_asm_fault_restore_aflags_in_slot
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

        not      REG_XAX /* ensure xax isn't dead */
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
        movz     TEST_REG_ASM, DRREG_TEST_5_ASM
        movz     TEST_REG_ASM, DRREG_TEST_5_ASM
        movz     TEST_REG2_ASM, DRREG_TEST_AFLAGS_H_ASM, LSL 16
        msr      nzcv, TEST_REG2_ASM
        nop

        mov      x0, HEX(0)
        ldr      x0, PTRSZ [x0] /* crash */

        b        epilog3
    epilog3:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME test_asm_fault_restore_ignore_3rd_dr_tls_slot
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

#define FUNCNAME test_asm_fault_restore_non_public_dr_slot
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

#define FUNCNAME test_asm_fault_restore_non_public_dr_slot_rip_rel_addr_in_reg
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
#define FUNCNAME test_asm_fault_restore_multi_phase_gpr_nested_spill_regions
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

        /* app2app phase will reserve TEST_REG_ASM here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        ud2
        /* insertion phase will reserve TEST_REG_ASM here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* insertion phase will unreserve TEST_REG_ASM here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3
        /* app2app phase will unreserve TEST_REG_ASM here. */

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

        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        .word 0xe7f000f0 /* udf */
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3

        b        epilog14
    epilog14:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test14
     test14:
        movz     TEST_REG_ASM, DRREG_TEST_14_ASM
        movz     TEST_REG_ASM, DRREG_TEST_14_ASM

        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        .inst 0xf36d19 /* udf */
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3

        b        epilog14
    epilog14:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 15: restore on fault for aflags stored in xax without preceding
         * xax spill.
         */
#define FUNCNAME test_asm_fault_restore_aflags_in_xax
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
        /* xax is dead, so should not need to spill aflags to slot. */
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
         * before crash. This is to verify that the drreg state restoration
         * logic doesn't forget a spill slot after it sees one restore (like
         * for an app read instr),
         */
#define FUNCNAME test_asm_fault_restore_gpr_restored_for_read
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

        mov      REG_XCX, 0
        mov      REG_XCX, PTRSZ [REG_XCX] /* crash */

        /* Read reg so that it is restored once. */
        add      TEST_REG2_ASM, TEST_REG_ASM
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

        mov      r0, HEX(0)
        ldr      r0, PTRSZ [r0] /* crash */

        /* Read reg so that it is restored once. */
        add      TEST_REG2_ASM, TEST_REG_ASM, TEST_REG_ASM
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

        mov      x0, HEX(0)
        ldr      x0, PTRSZ [x0] /* crash */

        /* Read reg so that it is restored once. */
        add      TEST_REG2_ASM, TEST_REG_ASM, TEST_REG_ASM
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
#define FUNCNAME test_asm_fault_restore_multi_phase_gpr_overlapping_spill_regions
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
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        mov      REG_XCX, 0
        mov      REG_XCX, PTRSZ [REG_XCX] /* crash */

        /* insertion phase will reserve TEST_REG_ASM here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* app2app phase will release TEST_REG_ASM here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3
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
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        mov      r0, HEX(0)
        ldr      r0, PTRSZ [r0] /* crash */

        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3
        b        epilog17
    epilog17:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test17
     test17:
        movz     TEST_REG_ASM, DRREG_TEST_17_ASM
        movz     TEST_REG_ASM, DRREG_TEST_17_ASM
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        mov      x0, HEX(0)
        ldr      x0, PTRSZ [x0] /* crash */

        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3
        b        epilog17
    epilog17:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 18: fault reg restore for fragments with DR_EMIT_STORE_TRANSLATIONS */
#define FUNCNAME test_asm_fault_restore_gpr_store_xl8
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
#define FUNCNAME test_asm_fault_restore_faux_gpr_spill
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
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        mov      x0, HEX(0)
        ldr      x0, PTRSZ [x0] /* crash */

        /* TEST_REG_ASM is un-reserved here. */
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2

        /* Read TEST_REG_ASM just so that it isn't dead. */
        add      TEST_REG_ASM, TEST_REG_ASM, TEST_REG_ASM
        adr      TEST_REG_STOLEN_ASM, some_data
        /* A faux restore instr -- looks like a drreg restore but isn't.
         * It will prevent us from recognising the actual spill slot for
         * TEST_REG_ASM.
         */
        ldr      TEST_REG_ASM, PTRSZ [TEST_REG_STOLEN_ASM, #TEST_FAUX_SPILL_TLS_OFFS]
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
#define FUNCNAME test_asm_fault_restore_multi_phase_native_gpr_spilled_twice
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
         *   TEST_REG_ASM as the new slot also has its native value.
         */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        ud2

        /* - insertion phase unreserves TEST_REG_ASM and frees the spill
         *   slot.
         */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* - insertion phase reserves TEST_REG2_ASM which would use the
         *   same spill slot as freed above, and overwrite TEST_REG_ASM
         *   value stored there currently. After this TEST_REG_ASM can
         *   only be found in its app2app spill slot.
         * - insertion phase writes to TEST_REG_ASM so that we need to
         *   restore it.
         */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3
        /* app2app phase unreserves TEST_REG_ASM. */

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
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        .word 0xe7f000f0 /* udf */

        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3

        b        epilog20
    epilog20:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test20
     test20:
        movz     TEST_REG_ASM, DRREG_TEST_20_ASM
        movz     TEST_REG_ASM, DRREG_TEST_20_ASM
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        .inst 0xf36d19 /* udf */

        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3

        b        epilog20
    epilog20:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 21: restore on fault for aflags reserved in multiple phases
         * with nested spill regions.
         */
#define FUNCNAME test_asm_fault_restore_multi_phase_aflags_nested_spill_regions
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test21
     test21:
        mov      TEST_REG_ASM, DRREG_TEST_21_ASM
        mov      TEST_REG_ASM, DRREG_TEST_21_ASM
        mov      ah, DRREG_TEST_AFLAGS_ASM
        sahf

        /* app2app phase will reserve aflags here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        not      REG_XAX /* ensure xax isn't dead */
        mov      REG_XAX, 0
        mov      REG_XAX, PTRSZ [REG_XAX] /* crash */

        /* insertion phase will reserve aflags here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* insertion phase will unreserve aflags here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3
        /* app2app phase will unreserve aflags here. */

        jmp      epilog21
     epilog21:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        b        test21
     test21:
        movw     TEST_REG_ASM, DRREG_TEST_21_ASM
        movw     TEST_REG_ASM, DRREG_TEST_21_ASM
        msr      APSR_nzcvq, DRREG_TEST_AFLAGS_ASM

        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        mov      r0, HEX(0)
        ldr      r0, PTRSZ [r0] /* crash */

        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3

        b        epilog21
    epilog21:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test21
     test21:
        movz     TEST_REG_ASM, DRREG_TEST_21_ASM
        movz     TEST_REG_ASM, DRREG_TEST_21_ASM
        movz     TEST_REG2_ASM, DRREG_TEST_AFLAGS_H_ASM, LSL 16
        msr      nzcv, TEST_REG2_ASM

        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        mov      x0, HEX(0)
        ldr      x0, PTRSZ [x0] /* crash */

        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3

        b        epilog21
    epilog21:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 23: restore on fault for aflags reserved in multiple phases
         * with overlapping but not nested spill regions. In this case,
         * the native aflags are stored in the app2app slot initially. Then,
         * they are swapped to the insertion phase slot after the app2app
         * unreservation.
         * Note that we do not respill aflags to the same slot, but select
         * a new slot at each re-spill, so the app2app phase slot gets
         * recycled and used by the insertion phase slot to re-spill the app
         * aflags.
         */
#define FUNCNAME test_asm_fault_restore_multi_phase_aflags_overlapping_spill_regions
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test23
     test23:
        mov      TEST_REG_ASM, DRREG_TEST_23_ASM
        mov      TEST_REG_ASM, DRREG_TEST_23_ASM
        mov      ah, DRREG_TEST_AFLAGS_ASM
        sahf

        /* app2app phase will reserve aflags here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        not      REG_XAX /* ensure xax isn't dead */
        mov      REG_XAX, 0
        mov      REG_XAX, PTRSZ [REG_XAX] /* crash */

        /* insertion phase will reserve aflags here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* app2app phase will release aflags here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3
        /* insertion phase will release aflags here. */
        jmp      epilog23
     epilog23:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        b        test23
     test23:
        movw     TEST_REG_ASM, DRREG_TEST_23_ASM
        movw     TEST_REG_ASM, DRREG_TEST_23_ASM
        msr      APSR_nzcvq, DRREG_TEST_AFLAGS_ASM

        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        mov      r0, HEX(0)
        ldr      r0, PTRSZ [r0] /* crash */

        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3

        b        epilog23
    epilog23:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test23
     test23:
        movz     TEST_REG_ASM, DRREG_TEST_23_ASM
        movz     TEST_REG_ASM, DRREG_TEST_23_ASM
        movz     TEST_REG2_ASM, DRREG_TEST_AFLAGS_H_ASM, LSL 16
        msr      nzcv, TEST_REG2_ASM

        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        mov      x0, HEX(0)
        ldr      x0, PTRSZ [x0] /* crash */

        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3

        b        epilog23
    epilog23:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 24: restore on fault for aflags restored once (for app read)
         * before crash. This is to verify that the drreg state restoration
         * logic doesn't forget a spill slot after it sees one restore (like
         * for an app read instr),
         */
#define FUNCNAME test_asm_fault_restore_aflags_restored_for_read
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test24
     test24:
        mov      TEST_REG_ASM, DRREG_TEST_24_ASM
        mov      TEST_REG_ASM, DRREG_TEST_24_ASM
        mov      ah, DRREG_TEST_AFLAGS_ASM
        sahf

        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        not      REG_XAX /* ensure xax isn't dead */
        mov      REG_XAX, 0
        mov      REG_XAX, PTRSZ [REG_XAX] /* crash */

        /* Read aflags so that it is restored once. */
        seto     al
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2

        jmp      epilog24
     epilog24:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        b        test24
     test24:
        movw     TEST_REG_ASM, DRREG_TEST_24_ASM
        movw     TEST_REG_ASM, DRREG_TEST_24_ASM
        msr      APSR_nzcvq, DRREG_TEST_AFLAGS_ASM

        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        mov      r0, HEX(0)
        ldr      r0, PTRSZ [r0] /* crash */

        /* Read aflags so that it is restored once. */
        mrs      TEST_REG2_ASM, APSR
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2

        b        epilog24
    epilog24:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test24
     test24:
        movz     TEST_REG_ASM, DRREG_TEST_24_ASM
        movz     TEST_REG_ASM, DRREG_TEST_24_ASM
        movz     TEST_REG2_ASM, DRREG_TEST_AFLAGS_H_ASM, LSL 16
        msr      nzcv, TEST_REG2_ASM

        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        mov      x0, HEX(0)
        ldr      x0, PTRSZ [x0] /* crash */

        /* Read aflags so that it is restored once. */
        mrs      TEST_REG2_ASM, nzcv
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2

        b        epilog24
    epilog24:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 25: Test restore on fault for aflags reserved in multiple
         * phases, where the two spill regions are nested, and the first
         * phase doesn't write the aflags before the second reservation. This
         * is to verify that drreg state restoration logic remembers that
         * the app value can be found in both the spill slots.
         */
#define FUNCNAME test_asm_fault_restore_multi_phase_native_aflags_spilled_twice
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test25
     test25:
        mov      TEST_REG_ASM, DRREG_TEST_25_ASM
        mov      TEST_REG_ASM, DRREG_TEST_25_ASM
        mov      ah, DRREG_TEST_AFLAGS_ASM
        sahf

        /* - app2app reserves aflags here, but doesn't write it.
         * - insertion reserves aflags here, which may confuse the
         *   state restoration logic into overwritting the spill slot for
         *   aflags as the new slot also has its native value.
         */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        not      REG_XAX /* ensure xax isn't dead */
        mov      REG_XAX, 0
        mov      REG_XAX, PTRSZ [REG_XAX] /* crash */

        /* - insertion phase unreserves aflags and frees the spill
         *   slot.
         */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* - insertion phase reserves TEST_REG_ASM which would use the
         *   same spill slot as freed above, and overwrite the aflags
         *   value stored there currently. After this native aflags can
         *   only be found in its app2app spill slot.
         * - insertion phase writes to aflags so that we need to
         *   restore it.
         */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3

        /* app2app phase unreserves aflags. */
        jmp      epilog25
     epilog25:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        b        test25
     test25:
        movw     TEST_REG_ASM, DRREG_TEST_25_ASM
        movw     TEST_REG_ASM, DRREG_TEST_25_ASM
        msr      APSR_nzcvq, DRREG_TEST_AFLAGS_ASM

        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        mov      r0, HEX(0)
        ldr      r0, PTRSZ [r0] /* crash */

        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3

        b        epilog25
    epilog25:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test25
     test25:
        movz     TEST_REG_ASM, DRREG_TEST_25_ASM
        movz     TEST_REG_ASM, DRREG_TEST_25_ASM
        movz     TEST_REG2_ASM, DRREG_TEST_AFLAGS_H_ASM, LSL 16
        msr      nzcv, TEST_REG2_ASM

        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        mov      x0, HEX(0)
        ldr      x0, PTRSZ [x0] /* crash */

        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3

        b        epilog25
    epilog25:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 26: fault aflags restore from spill slot for fragment emitting
         * DR_EMIT_STORE_TRANSLATIONS. This uses the state restoration logic
         * without the faulting fragment's ilist.
         */
#define FUNCNAME test_asm_fault_restore_aflags_in_slot_store_xl8
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test26
     test26:
        mov      TEST_REG_ASM, DRREG_TEST_26_ASM
        mov      TEST_REG_ASM, DRREG_TEST_26_ASM
        mov      ah, DRREG_TEST_AFLAGS_ASM
        sahf
        nop

        not      REG_XAX /* ensure xax isn't dead */
        mov      REG_XAX, 0
        mov      REG_XAX, PTRSZ [REG_XAX] /* crash */

        jmp      epilog26
     epilog26:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        b        test26
     test26:
        movw     TEST_REG_ASM, DRREG_TEST_26_ASM
        movw     TEST_REG_ASM, DRREG_TEST_26_ASM
        /* XXX: also test GE flags */
        msr      APSR_nzcvq, DRREG_TEST_AFLAGS_ASM
        nop

        mov      r0, HEX(0)
        ldr      r0, PTRSZ [r0] /* crash */

        b        epilog26
    epilog26:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test26
     test26:
        movz     TEST_REG_ASM, DRREG_TEST_26_ASM
        movz     TEST_REG_ASM, DRREG_TEST_26_ASM
        movz     TEST_REG2_ASM, DRREG_TEST_AFLAGS_H_ASM, LSL 16
        msr      nzcv, TEST_REG2_ASM
        nop

        mov      x0, HEX(0)
        ldr      x0, PTRSZ [x0] /* crash */

        b        epilog26
    epilog26:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 27: restore on fault for aflags stored in xax without preceding
         * xax spill, for fragments emitting DR_EMIT_STORE_TRANSLATIONS. This
         * uses the state restoration logic without ilist.
         */
#define FUNCNAME test_asm_fault_restore_aflags_in_xax_store_xl8
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test27
     test27:
        mov      TEST_REG_ASM, DRREG_TEST_27_ASM
        mov      TEST_REG_ASM, DRREG_TEST_27_ASM
        mov      ah, DRREG_TEST_AFLAGS_ASM
        sahf
        nop

        ud2
        /* xax is dead, so should not need to spill aflags to slot. */
        mov      REG_XAX, 0
        jmp      epilog27
     epilog27:
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

        /* Test 29: restore on fault for aflags stored in slot. In this test,
         * when aflags are spilled, xax was already reserved and in-use. This
         * is to verify that aflags are spilled using xax only.
         */
#define FUNCNAME test_asm_fault_restore_aflags_xax_already_spilled
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test29
     test29:
        mov      TEST_REG_ASM, DRREG_TEST_29_ASM
        mov      TEST_REG_ASM, DRREG_TEST_29_ASM
        mov      ah, DRREG_TEST_AFLAGS_ASM
        sahf
        /* xax is reserved here */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        /* aflags are reserved here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2

        ud2
        jmp      epilog29
     epilog29:
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

        /* Test 30: Test restoration of gpr when it was spilled to mcontext
         * later by non-drreg routines. This is to verify that drreg's state
         * restoration works even in presence of non-drreg spills and restores.
         */
#define FUNCNAME test_asm_fault_restore_gpr_spilled_to_mcontext_later
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test30
     test30:
        mov      TEST_REG_ASM, DRREG_TEST_30_ASM
        mov      TEST_REG_ASM, DRREG_TEST_30_ASM

        /* TEST_REG_ASM will be spilled using drreg here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        ud2

        /* TEST_REG_ASM will be restored using drreg here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* TEST_REG_ASM will be spilled and restored from mcontext here. */

        jmp      epilog30
     epilog30:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        b        test30
     test30:
        movw     TEST_REG_ASM, DRREG_TEST_30_ASM
        movw     TEST_REG_ASM, DRREG_TEST_30_ASM

        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        .word 0xe7f000f0 /* udf */
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2

        b        epilog30
    epilog30:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test30
     test30:
        movz     TEST_REG_ASM, DRREG_TEST_30_ASM
        movz     TEST_REG_ASM, DRREG_TEST_30_ASM

        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        .inst 0xf36d19 /* udf */
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2

        b        epilog30
    epilog30:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 31: Test restoration of aflags when they were spilled to
         * mcontext later by non-drreg routines. This is to verify that
         * drreg's state restoration works even in presence of non-drreg
         * spills and restores.
         */
#define FUNCNAME test_asm_fault_restore_aflags_spilled_to_mcontext_later
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test31
     test31:
        mov      TEST_REG_ASM, DRREG_TEST_31_ASM
        mov      TEST_REG_ASM, DRREG_TEST_31_ASM
        mov      ah, DRREG_TEST_AFLAGS_ASM
        sahf

        /* aflags will be spilled using drreg here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        mov      REG_XCX, 0
        mov      REG_XCX, PTRSZ [REG_XCX] /* crash */

        /* aflags will be restored using drreg here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* aflags will be spilled and restored from mcontext here. */

        jmp      epilog31
     epilog31:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        b        test31
     test31:
        movw     TEST_REG_ASM, DRREG_TEST_31_ASM
        movw     TEST_REG_ASM, DRREG_TEST_31_ASM
        msr      APSR_nzcvq, DRREG_TEST_AFLAGS_ASM

        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        mov      r1, HEX(0)
        ldr      r1, PTRSZ [r1] /* crash */
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2

        b        epilog31
    epilog31:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test31
     test31:
        movz     TEST_REG_ASM, DRREG_TEST_31_ASM
        movz     TEST_REG_ASM, DRREG_TEST_31_ASM
        movz     TEST_REG2_ASM, DRREG_TEST_AFLAGS_H_ASM, LSL 16
        msr      nzcv, TEST_REG2_ASM

        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        mov      x1, HEX(0)
        ldr      x1, PTRSZ [x1] /* crash */
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2

        b        epilog31
    epilog31:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 32: Test restoration of mcontext reg that was reserved also
         * using non-drreg routines during clean call instrumentation.
         */
#define FUNCNAME test_asm_fault_restore_gpr_spilled_during_clean_call_later
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test32
     test32:
        mov      TEST_REG_ASM, DRREG_TEST_32_ASM
        mov      TEST_REG_ASM, DRREG_TEST_32_ASM
        mov      TEST_REG_CLEAN_CALL_MCONTEXT_ASM, DRREG_TEST_32_ASM

        /* TEST_REG_CLEAN_CALL_MCONTEXT_ASM will be spilled using drreg here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        ud2

        /* TEST_REG_CLEAN_CALL_MCONTEXT_ASM will be restored using drreg here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* Clean call will be added here. */

        jmp      epilog32
     epilog32:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        b        test32
     test32:
        movw     TEST_REG_ASM, DRREG_TEST_32_ASM
        movw     TEST_REG_ASM, DRREG_TEST_32_ASM
        movw     TEST_REG_CLEAN_CALL_MCONTEXT_ASM, DRREG_TEST_32_ASM

        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        .word 0xe7f000f0 /* udf */
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2

        b        epilog32
    epilog32:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test32
     test32:
        movz     TEST_REG_ASM, DRREG_TEST_32_ASM
        movz     TEST_REG_ASM, DRREG_TEST_32_ASM
        movz     TEST_REG_CLEAN_CALL_MCONTEXT_ASM, DRREG_TEST_32_ASM

        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        .inst 0xf36d19 /* udf */
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2

        b        epilog32
    epilog32:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME


        /* Test 33: Test restoration for aflags reserved also during clean call
         * instrumentation.
         */
#define FUNCNAME test_asm_fault_restore_aflags_spilled_during_clean_call_later
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test33
     test33:
        mov      TEST_REG_ASM, DRREG_TEST_33_ASM
        mov      TEST_REG_ASM, DRREG_TEST_33_ASM
        mov      ah, DRREG_TEST_AFLAGS_ASM
        sahf

        /* aflags will be spilled using drreg here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        mov      REG_XCX, 0
        mov      REG_XCX, PTRSZ [REG_XCX] /* crash */

        /* aflags will be restored using drreg here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* Clean call will be added here. */

        jmp      epilog33
     epilog33:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        b        test33
     test33:
        movw     TEST_REG_ASM, DRREG_TEST_33_ASM
        movw     TEST_REG_ASM, DRREG_TEST_33_ASM
        msr      APSR_nzcvq, DRREG_TEST_AFLAGS_ASM

        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        mov      r1, HEX(0)
        ldr      r1, PTRSZ [r1] /* crash */
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2

        b        epilog33
    epilog33:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test33
     test33:
        movz     TEST_REG_ASM, DRREG_TEST_33_ASM
        movz     TEST_REG_ASM, DRREG_TEST_33_ASM
        movz     TEST_REG2_ASM, DRREG_TEST_AFLAGS_H_ASM, LSL 16
        msr      nzcv, TEST_REG2_ASM

        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        mov      x1, HEX(0)
        ldr      x1, PTRSZ [x1] /* crash */
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2

        b        epilog33
    epilog33:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME


        /* Test 34: Test restoration of gpr when it was spilled to mcontext
         * during its drreg spill region. This is to verify that drreg's
         * state restoration works even in presence of non-drreg spills
         * and restores.
         */
#define FUNCNAME test_asm_fault_restore_gpr_spilled_to_mcontext_between
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test34
     test34:
        mov      TEST_REG_ASM, DRREG_TEST_34_ASM
        mov      TEST_REG_ASM, DRREG_TEST_34_ASM

        /* TEST_REG_ASM will be spilled using drreg here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1

        ud2

        /* TEST_REG_ASM will be spilled and restored to mcontext here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* TEST_REG_ASM will be restored using drreg here. */

        jmp      epilog34
     epilog34:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        b        test34
     test34:
        movw     TEST_REG_ASM, DRREG_TEST_34_ASM
        movw     TEST_REG_ASM, DRREG_TEST_34_ASM

        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        .word 0xe7f000f0 /* udf */
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2

        b        epilog34
    epilog34:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test34
     test34:
        movz     TEST_REG_ASM, DRREG_TEST_34_ASM
        movz     TEST_REG_ASM, DRREG_TEST_34_ASM

        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        .inst 0xf36d19 /* udf */
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2

        b        epilog34
    epilog34:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 35: Test restoration of aflags when they were spilled to
         * mcontext during its drreg spill region by non-drreg routines.
         * This is to verify that drreg's state restoration works even
         * in presence of non-drreg spills and restores.
         */
#define FUNCNAME test_asm_fault_restore_aflags_spilled_to_mcontext_between
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test35
     test35:
        mov      TEST_REG_ASM, DRREG_TEST_35_ASM
        mov      TEST_REG_ASM, DRREG_TEST_35_ASM
        mov      ah, DRREG_TEST_AFLAGS_ASM
        sahf

        /* aflags will be spilled using drreg here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        mov      REG_XCX, 0
        mov      REG_XCX, PTRSZ [REG_XCX] /* crash */
        /* aflags will be spilled and restored to mcontext here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* aflags will be restored using drreg here. */

        jmp      epilog35
     epilog35:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        b        test35
     test35:
        movw     TEST_REG_ASM, DRREG_TEST_35_ASM
        movw     TEST_REG_ASM, DRREG_TEST_35_ASM
        msr      APSR_nzcvq, DRREG_TEST_AFLAGS_ASM

        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        mov      r1, HEX(0)
        ldr      r1, PTRSZ [r1] /* crash */
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2

        b        epilog35
    epilog35:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test35
     test35:
        movz     TEST_REG_ASM, DRREG_TEST_35_ASM
        movz     TEST_REG_ASM, DRREG_TEST_35_ASM
        movz     TEST_REG2_ASM, DRREG_TEST_AFLAGS_H_ASM, LSL 16
        msr      nzcv, TEST_REG2_ASM

        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        mov      x1, HEX(0)
        ldr      x1, PTRSZ [x1] /* crash */
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2

        b        epilog35
    epilog35:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 36: restore on fault for gpr reserved in multiple phases,
         * where the two spill regions are nested, and the insertion phase
         * spill region is the outer one.
         */
#define FUNCNAME test_asm_fault_restore_multi_phase_gpr_nested_spill_regions_insertion_outer
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test36
     test36:
        mov      TEST_REG_ASM, DRREG_TEST_36_ASM
        mov      TEST_REG_ASM, DRREG_TEST_36_ASM

        /* insertion phase will reserve TEST_REG_ASM here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        ud2
        /* app2app phase will reserve TEST_REG_ASM here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* app2app phase will unreserve TEST_REG_ASM here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3
        /* insertion phase will unreserve TEST_REG_ASM here. */

        jmp      epilog36
     epilog36:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        b        test36
     test36:
        movw     TEST_REG_ASM, DRREG_TEST_36_ASM
        movw     TEST_REG_ASM, DRREG_TEST_36_ASM

        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        .word 0xe7f000f0 /* udf */
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3

        b        epilog36
    epilog36:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test36
     test36:
        movz     TEST_REG_ASM, DRREG_TEST_36_ASM
        movz     TEST_REG_ASM, DRREG_TEST_36_ASM

        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        .inst 0xf36d19 /* udf */
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3

        b        epilog36
    epilog36:
        ret
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

        /* Test 37: restore on fault for aflags reserved in multiple phases,
         * where the two spill regions are nested, and the insertion phase
         * spill region is the outer one.
         */
#define FUNCNAME test_asm_fault_restore_multi_phase_aflags_nested_spill_regions_insertion_outer
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test37
     test37:
        mov      TEST_REG_ASM, DRREG_TEST_37_ASM
        mov      TEST_REG_ASM, DRREG_TEST_37_ASM
        mov      ah, DRREG_TEST_AFLAGS_ASM
        sahf

        /* insertion phase will reserve aflags here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        not      REG_XAX /* ensure xax isn't dead */
        mov      REG_XAX, 0
        mov      REG_XAX, PTRSZ [REG_XAX] /* crash */
        /* app2app phase will reserve aflags here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        /* app2app phase will unreserve aflags here. */
        mov      TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3
        /* insertion phase will unreserve aflags here. */

        jmp      epilog37
     epilog37:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(ARM)
        /* XXX i#3289: prologue missing */
        b        test37
     test37:
        movw     TEST_REG_ASM, DRREG_TEST_37_ASM
        movw     TEST_REG_ASM, DRREG_TEST_37_ASM
        msr      APSR_nzcvq, DRREG_TEST_AFLAGS_ASM

        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        mov      r0, HEX(0)
        ldr      r0, PTRSZ [r0] /* crash */
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        movw     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3

        b        epilog37
    epilog37:
        bx       lr
#elif defined(AARCH64)
        /* XXX i#3289: prologue missing */
        b        test37
     test37:
        movz     TEST_REG_ASM, DRREG_TEST_37_ASM
        movz     TEST_REG_ASM, DRREG_TEST_37_ASM
        movz     TEST_REG2_ASM, DRREG_TEST_AFLAGS_H_ASM, LSL 16
        msr      nzcv, TEST_REG2_ASM

        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_1
        mov      x0, HEX(0)
        ldr      x0, PTRSZ [x0] /* crash */
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_2
        movz     TEST_REG2_ASM, TEST_INSTRUMENTATION_MARKER_3

        b        epilog37
    epilog37:
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
