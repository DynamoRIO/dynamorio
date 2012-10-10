/* **********************************************************
 * Copyright (c) 2012 Google, Inc.  All rights reserved.
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

#ifndef ASM_CODE_ONLY

#include "tools.h"

#ifdef LINUX
# include <unistd.h>
# include <signal.h>
# include <ucontext.h>
# include <errno.h>
# include <stdlib.h>
#endif

#include <setjmp.h>

static SIGJMP_BUF mark;
static int count = 0;

static void
print_fault_code(unsigned char *pc)
{
    /* Expecting:
     *   0x0804b056  b9 07 00 00 00       mov    $0x00000007 -> %ecx
     *   0x0804b05b  89 01                mov    %eax -> (%ecx)
     * X64:
     *   0x0000000000403059  48 c7 c1 07 00 00 00 mov    $0x00000007 -> %rcx
     *   0x0000000000403060  89 01                mov    %eax -> (%rcx)
     */
    print("fault bytes are %02x %02x preceded by %02x %02x %02x %02x\n",
          *pc, *(pc+1), *(pc-4), *(pc-3), *(pc-2), *(pc-1));
}

#ifdef LINUX
static void
signal_handler(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (sig == SIGSEGV) {
        struct sigcontext *sc = (struct sigcontext *) &(ucxt->uc_mcontext);
        print_fault_code((unsigned char *)sc->SC_XIP);
        SIGLONGJMP(mark, count++);
    }
    exit(-1);
}
#else
# include <windows.h>
/* top-level exception handler */
static LONG
our_top_handler(struct _EXCEPTION_POINTERS * pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        print_fault_code((unsigned char *)
                         pExceptionInfo->ExceptionRecord->ExceptionAddress);
        SIGLONGJMP(mark, count++);
    }
    return EXCEPTION_EXECUTE_HANDLER; /* => global unwind and silent death */
}
#endif

typedef unsigned char byte;

static byte global_buf[8];

void
sandbox_cross_page(int i, byte buf[8]);

void
sandbox_fault(int i);

#ifdef X64
/* Reduced from V8, which uses x64 absolute addresses in code which ends up
 * being sandboxed.  The original code is not self-modifying, but is flushed
 * enough to trigger sandboxing.
 *
 * 0x000034b7f8b11366:  48 a1 48 33 51 36 ff 7e 00 00   movabs 0x7eff36513348,%rax
 * ...
 * 0x000034b7f8b11311:  48 a3 20 13 51 36 ff 7e 00 00   movabs %rax,0x7eff36511320
 */
void
test_mov_abs(void)
{
    char *rwx_mem = allocate_mem(4096, ALLOW_READ|ALLOW_WRITE|ALLOW_EXEC);
    char *pc = rwx_mem;
    void *(*do_selfmod_abs)(void);
    void *out_val = 0;
    uint64 *global_addr = (uint64 *) pc;

    /* Put a 64-bit 0xdeadbeefdeadbeef into mapped memory.  Typically most
     * memory from mmap is outside the low 4 GB, so this makes sure that any
     * mangling we do avoids address truncation.
     */
    *(uint64 *)pc = 0xdeadbeefdeadbeefULL;
    pc += 8;

    /* Encode an absolute load and store.  If we write it in assembly, gas picks
     * the wrong encoding, so we manually encode it here.  It has to be on the
     * same page as the data to trigger sandboxing.
     */
    do_selfmod_abs = (void *(*)(void))pc;
    *pc++ = 0x48;  /* REX.W */
    *pc++ = 0xa1;  /* movabs load -> rax */
    *(uint64 **)pc = global_addr;
    pc += 8;
    *pc++ = 0x48;  /* REX.W */
    *pc++ = 0xa3;  /* movabs store <- rax */
    *(uint64 **)pc = global_addr;
    pc += 8;
    *pc++ = 0xc3;  /* ret */

    print("before do_selfmod_abs\n");
    out_val = do_selfmod_abs();
    print(PFX"\n", out_val);
    /* rwx_mem is leaked, tools.h doesn't give us a way to free it. */
}

/* FIXME: Test reladdr. */
#endif /* X64 */

static void
test_code_self_mod(void)
{
    /* Make the code writable.  Note that main and the exception handler
     * __except_handler3 are on this page too.
     */
    protect_mem(code_self_mod, 1024, ALLOW_READ|ALLOW_WRITE|ALLOW_EXEC);
    print("Executed 0x%x iters\n", code_self_mod(0xabcd));
    print("Executed 0x%x iters\n", code_self_mod(0x1234));
    print("Executed 0x%x iters\n", code_self_mod(0xef01));
}

void
cross_page_check(int a)
{
    int i;
    for (i = 0; i < sizeof(global_buf); i++) {
        if (a != global_buf[i])  /* Can't do more than 256 iters. */
            print("global_buf not set right");
    }
}

static void
test_sandbox_cross_page(void)
{
    int i;
    print("start cross-page test\n");
    /* Make sandbox_cross_page code writable */
    protect_mem(sandbox_cross_page, 1024, ALLOW_READ|ALLOW_WRITE|ALLOW_EXEC);
    for (i = 0; i < 50; i++) {
        sandbox_cross_page(i, global_buf);
    }
    print("end cross-page test\n");
}

void
print_int(int x)
{
    print("int is %d\n", x);
}

static void
test_sandbox_fault(void)
{
    int i;
    print("start fault test\n");
    protect_mem(sandbox_fault, 1024, ALLOW_READ|ALLOW_WRITE|ALLOW_EXEC);
    i = SIGSETJMP(mark);
    if (i == 0)
        sandbox_fault(42);
    print("end fault test\n");
}

int
main(void)
{
    INIT();

#ifdef LINUX
    intercept_signal(SIGSEGV, signal_handler, false);
#else
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER) our_top_handler);
#endif

    test_code_self_mod();

#ifdef X64
    test_mov_abs();
#endif

    test_sandbox_cross_page();

    test_sandbox_fault();

    return 0;
}

#else /* ASM_CODE_ONLY */

#include "asm_defines.asm"

START_FILE

DECL_EXTERN(cross_page_check)
DECL_EXTERN(print_int)

    /* The following code needs to cross a page boundary. */
#if defined(ASSEMBLE_WITH_GAS)
.align 4096           /* nop fill */
.fill 4080, 1, 0x90   /* nop fill */
#elif defined(ASSEMBLE_WITH_MASM)
/* MASM thinks the .text segment is not 4096 byte aligned.  If we use plain
 * ALIGN, we get error A2189.  Declaring our own segment seems to work.
 */
_MYTEXT SEGMENT ALIGN(4096) ALIAS(".mytext")
REPEAT 4080
        nop
        ENDM
#else
# error NASM NYI
#endif

#define FUNCNAME sandbox_cross_page
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      REG_XAX, ARG1
        mov      REG_XCX, ARG2
        push     REG_XBP
        push     REG_XDX
        push     REG_XDI  /* for 16-alignment on x64 */

#if defined(ASSEMBLE_WITH_GAS)
        .align 4096
#elif defined(ASSEMBLE_WITH_MASM)
        ALIGN 4096
#else
# error NASM NYI
#endif

        /* Do enough writes to cause the sandboxing code to split the block. */
        mov      [REG_XCX + 0], al
        mov      [REG_XCX + 1], al
        mov      [REG_XCX + 2], al
        mov      [REG_XCX + 3], al

        lea      REG_XDX, SYMREF(immediate_addr_plus_four - 4)
        mov      DWORD [REG_XDX], eax        /* selfmod write */

        /* More writes to split the block. */
        mov      [REG_XCX + 4], al
        mov      [REG_XCX + 5], al
        mov      [REG_XCX + 6], al
        mov      [REG_XCX + 7], al

        mov      REG_XDX, HEX(0)             /* mov_imm to modify */
ADDRTAKEN_LABEL(immediate_addr_plus_four:)
        lea      REG_XAX, SYMREF(cross_page_check)
        CALLC1(REG_XAX, REG_XDX)

        /* restore */
        pop      REG_XDI
        pop      REG_XDX
        pop      REG_XBP
        ret
        END_FUNC(FUNCNAME)

#ifdef ASSEMBLE_WITH_MASM
_MYTEXT ENDS
#endif
#undef FUNCNAME

#define FUNCNAME sandbox_fault
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      REG_XAX, ARG1
        push     REG_XBP
        push     REG_XDX
        push     REG_XDI  /* for 16-alignment on x64 */

        lea      REG_XDX, SYMREF(fault_immediate_addr_plus_four - 4)
        mov      DWORD [REG_XDX], eax        /* selfmod write */

        mov      REG_XDX, HEX(0)             /* mov_imm to modify */
ADDRTAKEN_LABEL(fault_immediate_addr_plus_four:)
        lea      REG_XAX, SYMREF(print_int)
        CALLC1(REG_XAX, REG_XDX)

        mov      REG_XCX, HEX(7)
        mov      [REG_XCX], eax              /* fault */

        /* restore */
        pop      REG_XDI
        pop      REG_XDX
        pop      REG_XBP
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

END_FILE

#endif /* ASM_CODE_ONLY */
