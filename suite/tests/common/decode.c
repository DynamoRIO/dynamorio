/* **********************************************************
 * Copyright (c) 2012-2018 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
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
test_modrm16(char *buf);
void
test_nops(void);
void
test_sse3(char *buf);
void
test_3dnow(char *buf);
void
test_far_cti(void);
void
test_data16_mbr(void);
void
test_rip_rel_ind(void);
void
test_bsr(void);
void
test_SSE2(void);
void
test_mangle_seg(void);

SIGJMP_BUF mark;
static int count = 0;
static bool print_access_vio = true;

void (*func_ptr)(void);

#    define VERBOSE 0

#    define ITERS 1500000

static int a[ITERS];

#    ifdef UNIX
#        define ALT_STACK_SIZE (SIGSTKSZ * 3)

int
my_setjmp(sigjmp_buf env)
{
    return SIGSETJMP(env);
}

static void
signal_handler(int sig)
{
    if (sig == SIGILL) {
        count++;
        print("Bad instruction, instance %d\n", count);
        SIGLONGJMP(mark, count);
    } else if (sig == SIGSEGV) {
        count++;
        if (print_access_vio)
            print("Access violation, instance %d\n", count);
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
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_ILLEGAL_INSTRUCTION) {
        count++;
        print("Bad instruction, instance %d\n", count);
        longjmp(mark, count);
    }
    /* Shouldn't get here in normal operation so this isn't #if VERBOSE */
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
#        if VERBOSE
        /* DR gets the target addr wrong for far call/jmp so we don't print it */
        print("\tPC " PFX " tried to %s address " PFX "\n",
              pExceptionInfo->ExceptionRecord->ExceptionAddress,
              (pExceptionInfo->ExceptionRecord->ExceptionInformation[0] == 0) ? "read"
                                                                              : "write",
              pExceptionInfo->ExceptionRecord->ExceptionInformation[1]);
#        endif
        count++;
        if (print_access_vio)
            print("Access violation, instance %d\n", count);
        longjmp(mark, count);
    }
    print("Exception " PFX " occurred, process about to die silently\n",
          pExceptionInfo->ExceptionRecord->ExceptionCode);
    return EXCEPTION_EXECUTE_HANDLER; /* => global unwind and silent death */
}
#    endif

static void
actual_call_target(void)
{
    print("Made it to actual_call_target\n");
}

int
main(int argc, char *argv[])
{
    double res = 0.;
    int i;
#    ifndef X64
    int j;
#    endif
    char *buf;
#    ifdef UNIX
    stack_t sigstack;
#    endif

#    ifdef UNIX
    /* our modrm16 tests clobber esp so we need an alternate stack */
    sigstack.ss_sp = (char *)malloc(ALT_STACK_SIZE);
    sigstack.ss_size = ALT_STACK_SIZE;
    sigstack.ss_flags = SS_ONSTACK;
    i = sigaltstack(&sigstack, NULL);
    assert(i == 0);
    intercept_signal(SIGILL, (handler_3_t)signal_handler, true);
    intercept_signal(SIGSEGV, (handler_3_t)signal_handler, true);
#    else
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)our_top_handler);
#    endif

    buf = allocate_mem(7 * 256 + 1, ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);
    assert(buf != NULL);

#    ifndef X64
    print("Jumping to a sequence of every addr16 modrm byte\n");
    for (j = 0; j < 256; j++) {
        int mod = ((j >> 6) & 0x3); /* top 2 bits */
        int reg = ((j >> 3) & 0x7); /* middle 3 bits */
        int rm = (j & 0x7);         /* bottom 3 bits */
#        if defined(UNIX) || defined(X64)
        buf[j * 7 + 0] = 0x65; /* gs: */
#        else
        buf[j * 7 + 0] = 0x64; /* fs: */
#        endif
        buf[j * 7 + 1] = 0x67; /* addr16 */
        buf[j * 7 + 2] = 0x8b; /* load */
#        ifdef WINDOWS
        /* Windows can't handle stack pointer being off */
        if (reg == 4) { /* xsp */
            buf[j * 7 + 3] = j | 0x8;
        } else
            buf[j * 7 + 3] = j; /* nearly every single modrm byte */
#        else
        buf[j * 7 + 3] = j;    /* every single modrm byte */
#        endif
        if (mod == 1) {
            buf[j * 7 + 4] = 0x03; /* disp */
            buf[j * 7 + 5] = 0xc3;
        } else if (mod == 2 || (mod == 0 && rm == 6)) {
            buf[j * 7 + 4] = 0x03; /* disp */
            buf[j * 7 + 5] = 0x00; /* disp */
        } else {
            buf[j * 7 + 4] = 0xc3; /* ret */
            buf[j * 7 + 5] = 0xc3;
        }
        buf[j * 7 + 6] = 0xc3;
    }
    buf[256 * 7] = 0xcc;
    print_access_vio = false;
    for (j = 0; j < 256; j++) {
        i = SIGSETJMP(mark);
        if (i == 0)
            test_modrm16(&buf[j * 7]);
        else
            continue;
    }
    print("Done with modrm test: tested %d\n", j);
    count = 0;
    print_access_vio = true;
#    endif /* !X64 */

    /* multi-byte nop tests (case 9862) */
    i = SIGSETJMP(mark);
    if (i == 0) {
        print("Testing nops\n");
        test_nops();
        print("Done with nops\n");
    }

    /* SSE3 and 3DNow instrs will not run on all processors so we can't have this
     * regression test fully test everything: for now its main use is running
     * manually on the proper machines, or manually verifying decoding of these,
     * but we'll leave as a suite/ regression test.
     */

    /* SSE3 tests: mostly w/ modrm of (%edx) */
    i = SIGSETJMP(mark);
    if (i == 0) {
        print("Testing SSE3\n");
        test_sse3(buf);
        print("Should not get here\n");
    }

    /* 3D-Now tests: mostly w/ modrm of (%ebx) */
    i = SIGSETJMP(mark);
    if (i == 0) {
        print("Testing 3D-Now\n");
        test_3dnow(buf);
        print("Should not get here\n");
    }

    /* case 6962: far call/jmp tests
     * Note that DR currently gets the target address wrong for all of these
     * since we skip the segment and only care about the address:
     * not going to fix that anytime soon.
     */
    print("Testing far call/jmp\n");
    test_far_cti();

#    ifdef WINDOWS /* FIXME i#105: crashing on Linux so disabling for now */
    /* PR 242815: data16 mbr */
    print("Testing data16 mbr\n");
    test_data16_mbr();
#    endif

    /* i#1024: rip-rel ind branch */
    print("Testing rip-rel ind branch\n");
    func_ptr = actual_call_target;
    test_rip_rel_ind();

    /* i#1118: subtle prefix opcode issues */
    test_bsr();
    i = SIGSETJMP(mark);
    if (i == 0) {
        test_SSE2();
    }

    /* i#1493: segment register mangling */
    test_mangle_seg();

#    ifdef UNIX
    free(sigstack.ss_sp);
#    endif

    print("All done\n");
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

#define FUNCNAME test_modrm16
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* can't push ARG1 here b/c of SEH64 epilog restrictions */
        mov      REG_XAX, ARG1
        PUSH_CALLEE_SAVED_REGS()
        PUSH_NONCALLEE_SEH(REG_XAX)
        END_PROLOG
        mov      ax, 4
        mov      bx, 8
        mov      cx, 4
        mov      dx, 8
        mov      si, 4
        mov      di, 8
        mov      bp, 8
        CALLC0(PTRSZ [REG_XSP] /* ARG1 */)
        pop      REG_XAX /* arg1 */
        add      REG_XSP, 0 /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME test_nops
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        RAW(66) RAW(90)
        RAW(67) RAW(90)
        RAW(f2) RAW(90)
        RAW(f3) RAW(90)
        RAW(66) RAW(66) RAW(66) RAW(66) RAW(66) RAW(90)
        RAW(0f) RAW(1f) RAW(00)
        RAW(0f) RAW(1f) RAW(40) RAW(00)
        RAW(0f) RAW(1f) RAW(44) RAW(00) RAW(00)
        RAW(66) RAW(0f) RAW(1f) RAW(44) RAW(00) RAW(00)
        RAW(0f) RAW(1f) RAW(80) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(0f) RAW(1f) RAW(84) RAW(00) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(66) RAW(0f) RAW(1f) RAW(84) RAW(00) RAW(00) RAW(00) RAW(00) RAW(00)
        ret
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME test_sse3
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov    REG_XAX, ARG1
        RAW(f2) RAW(0f) RAW(7c) RAW(20) /* haddps (%rax), xmm4 */
        RAW(f2) RAW(0f) RAW(7d) RAW(20) /* hsubps (%rax), xmm4 */
        RAW(f2) RAW(0f) RAW(d0) RAW(20) /* addsubps (%rax), xmm4 */
        RAW(f2) RAW(0f) RAW(f0) RAW(20) /* lddqu (%rax), xmm4 */
        RAW(f3) RAW(0f) RAW(12) RAW(20) /* movsldup (%rax), xmm4 */
        RAW(f2) RAW(0f) RAW(12) RAW(20) /* movddup (%rax), xmm4 */
        RAW(f3) RAW(0f) RAW(16) RAW(20) /* movshdup (%rax), xmm4 */
#ifdef X64
        /* i#319: these 2 are in original sse but adding here until get api/dis up */
        RAW(41) RAW(0f) RAW(12) RAW(f4) /* i#319: movlhps %xmm12, xmm6 */
        RAW(41) RAW(0f) RAW(16) RAW(f4) /* i#319: movhlps %xmm12, xmm6 */
#endif
        mov  eax, 0
        mov  ecx, 0
        mov  edx, 0
        RAW(0f) RAW(01) RAW(c8) /* monitor */
        RAW(0f) RAW(01) RAW(c9) /* mwait */
        /* we want failure on sse3 machine, to have constant output
         * (this will produce "Invalid opcode encountered" warning) */
        RAW(f3) RAW(0f) RAW(7c) RAW(20) /* bad */
        ret
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME test_3dnow
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov    REG_XAX, ARG1 /* these instrs use (%xax) */
        RAW(0f) RAW(0e)                   /* femms */
        RAW(0f) RAW(0f) RAW(08) RAW(bf) /* pavgusb (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(9e) /* pfadd (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(ae) /* pfacc (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(90) /* pfcmpge (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(a0) /* pfcmpgt (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(b0) /* pfcmpeq (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(94) /* pfmin (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(a4) /* pfmax (%xax),%mm1 */
        /* try 10(%eax) */
        RAW(0f) RAW(0f) RAW(48) RAW(0a) RAW(b4) /* pfmul 10(%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(96) /* pfrcp (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(a6) /* pfrcpit1 (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(b6) /* pfrcpit2 (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(97) /* pfrsqrt (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(a7) /* pfrsqit1 (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(b7) /* pmulhrw (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(9a) /* pfsub (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(aa) /* pfsubr (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(0d) /* pi2fd (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(1d) /* pf2id (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(0c) /* pi2fw (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(1c) /* pf2iw (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(8a) /* pfnacc (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(8e) /* pfpnacc (%xax),%mm1 */
        RAW(0f) RAW(0f) RAW(08) RAW(bb) /* pswapd (%xax),%mm1 */
        /* try unknown opcode: we want failure on amd machine
         * anyway, to have constant output */
        RAW(0f) RAW(0f) RAW(08) RAW(00) /* unknown */
        ret
        END_FUNC(FUNCNAME)

DECL_EXTERN(mark)

#ifdef WINDOWS
# ifdef X64
DECL_EXTERN(setjmp)
#  define CALL_SETJMP \
        lea   REG_XAX, mark @N@\
        CALLC2(setjmp, REG_XAX, REG_XSP)
# else
DECL_EXTERN(_setjmp3)
#  define CALL_SETJMP \
        lea   REG_XAX, mark @N@\
        CALLC2(_setjmp3, REG_XAX, 0)
# endif
#else
DECL_EXTERN(my_setjmp)
# ifdef X64
#  define CALL_SETJMP \
        lea   REG_XAX, SYMREF(mark) @N@ \
        CALLC1(my_setjmp, REG_XAX)
# else
#  define CALL_SETJMP \
        lea   REG_XAX, mark @N@\
        CALLC1(my_setjmp, REG_XAX)
# endif
#endif

/* FIXME PR 271834: we need some far ctis that actually succeed, to fully test
 * DR's handling of them.  We should test the resulting target address
 * and stack pointer.  We can do that initially by targeting the current
 * cs, and perhaps adding other segments once we have PR 271317.
 *
 * I did some manual testing to confirm AMD vs Intel 64-bit behavior
 * on far cti operand sizes, and should incorporate those tests here.
 * Note that for data16 I don't yet know how to get SEH64 to recover
 * (from a call or ret).  Here are my tests in my notes:
 *
 * ret_far:
 *  on Intel:  push 0x00331234; RAW(66) RAW(cb) => rip=0x1234
 *    mov rax, HEX(000000339abcdef0); push rax; RAW(cb) => rip=0x9abcdef0
 *    push 0x33; mov rax, HEX(000000339abcdef0); push rax; RAW(48) RAW(cb)
 *      => rip=0x339abcdef0
 *  AMD: same behavior
 *  => ret_far is like iret: default is still 32-bits, and expands and shrinks
 *
 * call_far:
 *   push HEX(00331234); RAW(66) RAW(ff) RAW(1c) RAW(24) == lcallw (%rsp)
 *     => rip=0x1234, pushed 2-byte segment and 2-byte retaddr (truncated)
 *   mov rax, HEX(000000339abcdef0); push rax; lcallq (%rsp)
 *     => rip=0x9abcdef0, pushed 4-byte segment and 4-byte retaddr
 *   push 0x33; mov rax, HEX(000000339abcdef0); push rax; rex.w lcallq (%rsp)
 *     => rip=0x9abcdef0, pushed 8-byte segment and 8-byte retaddr
 *     for AMD, same, except rip=0x9abcdef0 (this is PR 270274)
 * so AMD honors the rex.w to give you a 64-bit retaddr, but still truncates your
 *   target address.  crazy.
 *
 * Should add rex.w tests (xref PR 241573).
 * Should also add iret tests (xref PR 191977).
 */
#undef FUNCNAME
#define FUNCNAME test_far_cti
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        END_PROLOG
        CALL_SETJMP
        cmp   REG_XAX, 0
        jne   test_far_cti_1
        /* ljmp to kernel address space: ljmp %0xbc9a:0xf8563412 */
        RAW(ea) RAW(12) RAW(34) RAW(56) RAW(f8) RAW(9a) RAW(bc)
    test_far_cti_1:
        CALL_SETJMP
        cmp   REG_XAX, 0
        jne   test_far_cti_2
        /* ljmp to user address space: ljmp %0xbc9a:0x78563412 */
        RAW(ea) RAW(12) RAW(34) RAW(56) RAW(78) RAW(9a) RAW(bc)
    test_far_cti_2:
        CALL_SETJMP
        cmp   REG_XAX, 0
        jne   test_far_cti_3
        /* lcall to kernel address space */
        RAW(9a) RAW(12) RAW(34) RAW(56) RAW(f8) RAW(9a) RAW(bc)
    test_far_cti_3:
        CALL_SETJMP
        cmp   REG_XAX, 0
        jne   test_far_cti_4
        /* lcall to user address space */
        RAW(9a) RAW(12) RAW(34) RAW(56) RAW(78) RAW(9a) RAW(bc)
    test_far_cti_4:
        CALL_SETJMP
        cmp   REG_XAX, 0
        jne   test_far_cti_5
        /* targeting what's stored there, but a crash is a crash */
        mov   eax, HEX(deadbeef)
        RAW(ff) RAW(28) /* ljmp (%eax) */
    test_far_cti_5:
        CALL_SETJMP
        cmp   REG_XAX, 0
        jne   test_far_cti_6
        /* targeting what's stored there, but a crash is a crash */
        mov   eax, HEX(deadbeef)
        RAW(ff) RAW(18) /* lcall (%eax) */
    test_far_cti_6:
        ret
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME test_data16_mbr
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* if we don't push something we'll clobber the real retaddr
         * when we do our data16 ret and data16 lret below,
         * and not even longjmp will save us (seems to happen to work
         * natively though but not for DR: didn't look further: seems like
         * no guarantee natively) */
        /* I can't get push imm to work w/ gas using Intel syntax:
         * it doesn't like hex constants, or $, or decimal w/ over 8 digits!
         * Definitely a bug there.  So instead we go through a register.
         * We don't care about zero vs sign extend.
         * Ugh, SEH64 fails when going through a register: I don't see why.
         * I don't see any documentation saying I can't do a mov imm into
         * a volatile register.  Not worth my time: just splitting.
         */
#ifdef WINDOWS
        PUSH_NONCALLEE_SEH(HEX(deadbeef))
#else
        mov   REG_XAX, HEX(deadbeef)
        PUSH_NONCALLEE_SEH(REG_XAX)
#endif
        END_PROLOG
        CALL_SETJMP
        cmp   REG_XAX, 0
        jne   test_data16_mbr_1
        mov   ecx, HEX(deadbeef) /* needed for 64-bit where we target rcx
                                  * (may still get NX fault here, but won't
                                  * under DR: PR 210383) */
        /* FIXME PR 271863: this dies on 64-bit AMD: not sure how to get
         * SEH64 to recover from the stack misalignment.
         */
        RAW(66) RAW(ff) RAW(d1) /* call %cx */
    test_data16_mbr_1:
        CALL_SETJMP
        cmp   REG_XAX, 0
        jne   test_data16_mbr_2
        mov   ecx, HEX(deadbeef) /* needed for 64-bit where we target rcx */
        RAW(66) RAW(ff) RAW(e1) /* jmp %cx */
    test_data16_mbr_2:
        CALL_SETJMP
        cmp   REG_XAX, 0
        jne   test_data16_mbr_3
        /* the push above means we won't clobber retaddr (can't push here: SEH64!) */
        /* 64-bit:
         * Note that on Intel this is a 64-bit ret, and since SEH64 looks only
         * from RIP forward it thinks this is an epilog, which is why the
         * unwind succeeds (else it would pop rax and mess up the stack).
         * FIXME PR 271863: this will die on AMD, natively and in DR: can't have
         * handler for this routine (matches epilog), so can we recover w/ an
         * outer handler, or will we die since stack is not aligned?
         */
        RAW(66) RAW(c3) /* data16 ret */
    test_data16_mbr_3:
    /* repeat all the far tests w/ data16 */
        CALL_SETJMP
        cmp   REG_XAX, 0
        jne   test_data16_mbr_4
        /* data16 ljmp %0xbc9a:0x7856 */
        RAW(66) RAW(ea) RAW(56) RAW(78) RAW(9a) RAW(bc)
    test_data16_mbr_4:
        CALL_SETJMP
        cmp   REG_XAX, 0
        jne   test_data16_mbr_5
        /* data16 lcall %0xbc9a:0x7856 */
        RAW(66) RAW(9a) RAW(56) RAW(f8) RAW(9a) RAW(bc)
    test_data16_mbr_5:
        CALL_SETJMP
        cmp   REG_XAX, 0
        jne   test_data16_mbr_6
        mov   eax, HEX(deadbeef)
        RAW(66) RAW(ff) RAW(28) /* data16 ljmp (%eax) */
    test_data16_mbr_6:
        CALL_SETJMP
        cmp   REG_XAX, 0
        jne   test_data16_mbr_7
        mov   eax, HEX(deadbeef)
        RAW(66) RAW(ff) RAW(18) /* data16 lcall (%eax) */
    test_data16_mbr_7:
        CALL_SETJMP
        cmp   REG_XAX, 0
        jne   test_data16_mbr_8
        /* the push above means we won't clobber retaddr (can't push here: SEH64!) */
#ifdef X64
        /* Since we don't preserve the cs change we don't get the same fault
         * that we get natively, and our current SEH64 setup won't catch
         * the fault from a misaligned ret mid-routine (see above as well).
         * PR 271317 covers handing the cs change. */
        RAW(cb) /* lret */
#else
        RAW(66) RAW(cb) /* data16 lret */
#endif
    test_data16_mbr_8:
        CALL_SETJMP
        cmp   REG_XAX, 0
        jne   test_data16_mbr_9
        RAW(66) RAW(e9) RAW(00) RAW(00) /* data16 jmp next instr */
        /* Intel x64: 1st 2 are part of disp (ignores data16), then crash; AMD: crash */
        RAW(00) RAW(00) RAW(00) RAW(00)
    test_data16_mbr_9:
        CALL_SETJMP
        cmp   REG_XAX, 0
        jne   test_data16_mbr_10
        RAW(66) RAW(e8) RAW(00) RAW(00) /* data16 call next instr */
        /* Intel x64: 1st 2 are part of disp (ignores data16), then crash; AMD: crash */
        RAW(00) RAW(00) RAW(00) RAW(00)
    test_data16_mbr_10:
        add      REG_XSP, ARG_SZ /* make a legal SEH64 epilog, and clean up push */
        ret
        END_FUNC(FUNCNAME)

DECL_EXTERN(func_ptr)

#undef FUNCNAME
#define FUNCNAME test_rip_rel_ind
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        END_PROLOG
        CALL_SETJMP
        call     PTRSZ SYMREF(func_ptr)
        ret
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME test_bsr
       /* If we make this a leaf function (no SEH directives), our top-level handler
        * does not get called!  Annoying.
        */
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        PUSH_CALLEE_SAVED_REGS()
        END_PROLOG

        /* test i#1118 sequences: all should be valid */
        RAW(66) RAW(0F) RAW(BB) RAW(E9) /* btc */
        RAW(66) RAW(0F) RAW(BC) RAW(E9) /* bsf */
        RAW(66) RAW(0F) RAW(BD) RAW(E9) /* bsr */
        RAW(f2) RAW(0F) RAW(BB) RAW(E9) /* btc */
        RAW(f2) RAW(0F) RAW(BC) RAW(E9) /* bsf */
        RAW(f2) RAW(0F) RAW(BD) RAW(E9) /* bsr */
        RAW(f3) RAW(0F) RAW(BB) RAW(E9) /* btc */
        RAW(f3) RAW(0F) RAW(BC) RAW(E9) /* bsf */
        RAW(f3) RAW(0F) RAW(BD) RAW(E9) /* bsr */

        add      REG_XSP, 0 /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME test_SSE2
       /* If we make this a leaf function (no SEH directives), our top-level handler
        * does not get called!  Annoying.
        */
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        PUSH_CALLEE_SAVED_REGS()
        END_PROLOG

        RAW(66) RAW(0F) RAW(D8) RAW(E9) /* psubusb */
        /* These two fault, despite gdb + dumpbin listing as fine: */
        RAW(f2) RAW(0F) RAW(D8) RAW(E9) /* psubusb */
        RAW(f3) RAW(0F) RAW(D8) RAW(E9) /* psubusb */

        add      REG_XSP, 0 /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME test_mangle_seg
        /* i#1493: test seg reg mangling code */
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        END_PROLOG
        push     REG_XAX
        mov      ax, fs
        mov      WORD [REG_XSP], fs
#ifdef X64
        mov      rax, fs
        /* Having trouble getting the assembler to emit this so going raw: */
        RAW(48) RAW(8c) RAW(24) RAW(24) /* mov QWORD [REG_XSP], fs */
#endif
        pop      REG_XAX
        add      REG_XSP, 0 /* make a legal SEH64 epilog */
        ret
        END_FUNC(FUNCNAME)
END_FILE
/* clang-format on */
#endif /* ASM_CODE_ONLY */
