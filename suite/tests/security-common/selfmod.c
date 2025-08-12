/* **********************************************************
 * Copyright (c) 2012-2025 Google, Inc.  All rights reserved.
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

#    include "tools.h"

#    ifdef UNIX
#        include <unistd.h>
#        include <signal.h>
#        include <ucontext.h>
#        include <errno.h>
#        include <stdlib.h>
#    endif

#    include <setjmp.h>

static SIGJMP_BUF mark;
static int count = 0;

#    ifdef AARCH64
void
clear_icache_if_required(void *beg, void *end)
{
    /* If CTR_EL0.DIC is set then the icache does not need to be cleared after code has
     * been modified. */
    uint64 CTR_EL0 = 0;
    asm("mrs %0, CTR_EL0" : "=r"(CTR_EL0));

    /* TODO i#5771: Enable this check when we have support for automatic icache coherence.
     */
    /* if ((CTR_EL0 >> 29) & 1 == 0) { */
    tools_clear_icache(beg, end);
    /* } */
}
#    endif

static void
print_fault_code(unsigned char *pc)
{
    /* For the seg fault, expecting:
     *   0x0804b056  b9 07 00 00 00       mov    $0x00000007 -> %ecx
     *   0x0804b05b  89 01                mov    %eax -> (%ecx)
     * X64:
     *   0x0000000000403059  48 c7 c1 07 00 00 00 mov    $0x00000007 -> %rcx
     *   0x0000000000403060  89 01                mov    %eax -> (%rcx)
     * AArch64:
     *   d28000ec  mov x12, #0x7
     *   f9000180  str x0, [x12]
     */
#    ifdef X86
    print("fault bytes are %02x %02x preceded by %02x %02x %02x %02x\n", *pc, *(pc + 1),
          *(pc - 4), *(pc - 3), *(pc - 2), *(pc - 1));
#    else
    print("fault instruction is 0x%08x preceded by 0x%08x\n", *(uint *)pc,
          *(uint *)(pc - 4));
#    endif
}

#    ifdef UNIX
static void
signal_handler(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (sig == SIGSEGV || sig == SIGILL) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (sig == SIGILL)
            print("Illegal instruction\n");
        else if (sig == SIGSEGV)
            print("Segmentation fault\n");
        print_fault_code((unsigned char *)sc->SC_XIP);
        SIGLONGJMP(mark, count++);
    }
    exit(-1);
}
#    else
#        include <windows.h>
/* top-level exception handler */
static LONG
our_top_handler(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
        pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (pExceptionInfo->ExceptionRecord->ExceptionCode ==
            EXCEPTION_ILLEGAL_INSTRUCTION)
            print("Illegal instruction\n");
        else
            print("Segmentation fault\n");
        print_fault_code(
            (unsigned char *)pExceptionInfo->ExceptionRecord->ExceptionAddress);
        SIGLONGJMP(mark, count++);
    }
    return EXCEPTION_EXECUTE_HANDLER; /* => global unwind and silent death */
}
#    endif

static byte global_buf[8];

void
sandbox_cross_page(int i, byte buf[8]);

int
sandbox_last_byte(void);

#    ifdef X86
void
make_last_byte_selfmod(void);
#    else
void
make_code_page_selfmod(void);
#    endif

void
sandbox_fault(int i);

void
sandbox_illegal_instr(int i);

void
sandbox_cti_tgt(void);

#    ifdef X86
void
sandbox_direction_flag(void);
#    endif

/* These *_no_ilt variants of the prototypes avoid indirection through the
 * Incremental Linking Table (ILT).  With inremental linking on Windows, all
 * functions are directed through a jump table at the beginning of .text.  Those
 * addresses remain stable across incremental links while function bodies in
 * .text can be moved.  dumpbin /disasm shows something like:
 * @ILT+0(_sandbox_cross_page):
 *   00401005: E9 E6 7F 02 00     jmp         _sandbox_cross_page
 * @ILT+5(_sandbox_fault):
 *   0040100A: E9 71 04 00 00     jmp         _sandbox_fault
 * @ILT+10(_sandbox_last_byte):
 *   0040100F: E9 E6 9F 02 00     jmp         _sandbox_last_byte
 */
void
sandbox_cross_page_no_ilt(void);
void
sandbox_fault_no_ilt(void);
void
sandbox_illegal_no_ilt(void);

#    ifdef X86
void
last_byte_jmp_no_ilt(void);
void
sandbox_direction_flag_no_ilt(void);
#    else
void
last_byte_jmp_label(void);
#    endif

#    ifdef X64
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
    static const int BUF_SIZE = 4096;
    char *rwx_mem = allocate_mem(BUF_SIZE, ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);
    char *pc = rwx_mem;
    void *(*do_selfmod_abs)(void);
    void *out_val = 0;
    uint64 *global_addr = (uint64 *)pc;

    /* Put a 64-bit 0xdeadbeefdeadbeef into mapped memory.  Typically most
     * memory from mmap is outside the low 4 GB, so this makes sure that any
     * mangling we do avoids address truncation.
     */
    *(uint64 *)pc = 0xdeadbeefdeadbeefULL;
    pc += 8;

    /* Encode an absolute load and store. It has to be on the
     * same page as the data to trigger sandboxing.
     */
    do_selfmod_abs = (void *(*)(void))pc;

#        ifdef X86
    /* If we write it in assembly, gas picks
     * the wrong encoding, so we manually encode it here.
     */
    *pc++ = 0x48; /* REX.W */
    *pc++ = 0xa1; /* movabs load -> rax */
    *(uint64 **)pc = global_addr;
    pc += 8;
    *pc++ = 0x48; /* REX.W */
    *pc++ = 0xa3; /* movabs store <- rax */
    *(uint64 **)pc = global_addr;
    pc += 8;
    *pc++ = 0xc3; /* ret */
#        else
    char *src = NULL;

    /* Copy into the buffer some instructions (from the assembly below) to read the
     * "0xdeadbeefdeadbeef" value into a register and return it.
     */
    __asm("adr %[result], start" : [result] "=r"(src));

    memcpy(pc, src, 12); /* 3 * 4 byte instruction size. */

    __asm volatile("b end");
    __asm volatile("start:");
    __asm volatile("adr x1, #-8"); /* this instruction and the next two are copied by the
                                      memcpy above. */
    __asm volatile("ldr x0, [x1]");
    __asm volatile("ret");
    __asm volatile("end:");
#        endif

    print("before do_selfmod_abs\n");
    out_val = do_selfmod_abs();
    print(PFX "\n", out_val);

    free_mem(rwx_mem, BUF_SIZE);
}

/* XXX: Test reladdr. */
#    endif /* X64 */

static void
test_code_self_mod(void)
{
    /* Make the code writable.  Note that main and the exception handler
     * __except_handler3 are on this page too.
     */
    protect_mem(code_self_mod, 1024, ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);
    print("Executed 0x%x iters\n", code_self_mod(0xabcd));
    print("Executed 0x%x iters\n", code_self_mod(0x1234));
    print("Executed 0x%x iters\n", code_self_mod(0xef01));
}

void
cross_page_check(int a)
{
    int i;
    for (i = 0; i < sizeof(global_buf); i++) {
        if (a != global_buf[i]) /* Can't do more than 256 iters. */
            print("global_buf not set right");
    }
}

static void
test_sandbox_cross_page(void)
{
    int i;
    print("start cross-page test\n");
    /* Make sandbox_cross_page code writable */
    protect_mem(sandbox_cross_page_no_ilt, 1024, ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);
    for (i = 0; i < 50; i++) {
        sandbox_cross_page(i, global_buf);
    }
    print("end cross-page test\n");
}

/* i#993: Test case for a bug where the last byte of a fragment was in a
 * different vmarea.
 */
static void
test_sandbox_last_byte(void)
{
#    ifdef X86
    byte *first_instr_of_page = (byte *)last_byte_jmp_no_ilt + 1;
#    else
    byte *first_instr_of_page = (byte *)last_byte_jmp_label;
#    endif
    if (!ALIGNED(first_instr_of_page, PAGE_SIZE)) {
        print("first_instr_of_page is not page-aligned: " PFX "\n"
              "Instruction sizes in sandbox_last_byte must be wrong.\n",
              first_instr_of_page);
    }
    print("start last byte test\n");
    protect_mem(first_instr_of_page, PAGE_SIZE, ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);
    /* Execute self-modifying code to create a sandboxed page. */
#    ifdef X86
    make_last_byte_selfmod();
#    else
    make_code_page_selfmod();
#    endif

    int r = sandbox_last_byte();
    print("sandbox_last_byte: %d\n", r); /* Should be 0. */

#    ifdef X86
    /* Make the relative jmp offset zero, so it goes to the next instruction. */
    *first_instr_of_page = 0;
#    else
    /* Increment the branch value, so it jumps to "last_byte_ret_one" */
    uint32 *first_instruction = (uint32 *)last_byte_jmp_label;
    (*first_instruction)++; /* The jump distance is encoded in number of instructions, not
                               bytes. */
    clear_icache_if_required(first_instruction, first_instruction + 4);
#    endif

    r = sandbox_last_byte();
    print("sandbox_last_byte: %d\n", r); /* Should be 1. */
    print("end last byte test\n");
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
    protect_mem(sandbox_fault_no_ilt, 1024, ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);

    i = SIGSETJMP(mark);
    if (i == 0)
        sandbox_fault(42);
    /* i#1441: test max writes with illegal instr */
    protect_mem(sandbox_illegal_no_ilt, 1024, ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);

    i = SIGSETJMP(mark);
    if (i == 0)
        sandbox_illegal_instr(42);
    print("end fault test\n");
}

static void
test_sandbox_cti_tgt(void)
{
    protect_mem(sandbox_cti_tgt, 1024, ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);
    sandbox_cti_tgt();
    print("end selfmod loop test\n");
}

#    ifdef X86
static void
test_sandbox_direction_flag(void)
{
    /* i#2155: test sandboxing with direction flag set */
    protect_mem(sandbox_cti_tgt, 1024, ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);
    sandbox_direction_flag();
    print("end selfmod direction flag test\n");
}
#    endif

int
main(void)
{
    INIT();

#    ifdef UNIX
    intercept_signal(SIGSEGV, signal_handler, false);
    intercept_signal(SIGILL, signal_handler, false);
#    else
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)our_top_handler);
#    endif

    test_code_self_mod();

#    ifdef X64
    test_mov_abs();
#    endif

    test_sandbox_cross_page();

    test_sandbox_last_byte();

    test_sandbox_fault();

    test_sandbox_cti_tgt();

#    ifdef X86
    test_sandbox_direction_flag();
#    endif

    return 0;
}

#else /* ASM_CODE_ONLY */

#    include "asm_defines.asm"

/* clang-format off */
START_FILE

DECL_EXTERN(cross_page_check)
DECL_EXTERN(print_int)

#ifdef ASSEMBLE_WITH_GAS
#    define ALIGN_WITH_NOPS(bytes) .balign (bytes)
#    ifdef AARCH64
#        define FILL_WITH_NOPS(bytes) .fill (bytes) / 4, 4, 0xd503201f
#    else
#        define FILL_WITH_NOPS(bytes) .fill (bytes), 1, 0x90
#    endif
#elif defined(ASSEMBLE_WITH_MASM)
/* Cannot align to a value greater than the section alignment.  See .mytext
 * below.
 */
# define ALIGN_WITH_NOPS(bytes) ALIGN bytes
# define FILL_WITH_NOPS(bytes) \
    REPEAT (bytes) @N@\
        nop @N@\
        ENDM
#else
# define ALIGN_WITH_NOPS(bytes) ALIGN bytes
# define FILL_WITH_NOPS(bytes) TIMES bytes nop
#endif

#if defined(ASSEMBLE_WITH_MASM)
/* The .text section on Windows is only 16 byte aligned.  If we use plain
 * ALIGN_WITH_NOPS(), we get error A2189.  We can't re-declare .text, but we can
 * declare a new section with greater alignment.  The EXECUTE gets the right
 * protections and makes ALIGN_WITH_NOPS() do nop fill.
 */
_MYTEXT SEGMENT ALIGN(4096) READ EXECUTE SHARED ALIAS(".mytext")
#endif

    /* The following code needs to cross a page boundary. */
#    ifdef AARCH64
#        define MAX_PAGE_SIZE_BYTES 65536 /* this is also a multiple of 16K and 4K so
                                           * works for all pages sizes
                                           */
#    else
#        define MAX_PAGE_SIZE_BYTES 4096
#    endif
ALIGN_WITH_NOPS(MAX_PAGE_SIZE_BYTES)
FILL_WITH_NOPS(MAX_PAGE_SIZE_BYTES - 16)

DECLARE_GLOBAL(sandbox_cross_page_no_ilt)
#define FUNCNAME sandbox_cross_page
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        nop /* ensure VS2013 doesn't route label through ILT */
ADDRTAKEN_LABEL(sandbox_cross_page_no_ilt:)
        mov      REG_XAX, ARG1
        mov      REG_XCX, ARG2
        push     REG_XBP
        push     REG_XDX
        push     REG_XDI  /* for 16-alignment on x64 */

        /* Don't use ALIGN_WITH_NOPS().  Masm will turn it into a jmp over
         * int3s.
         */

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
#else
ADDRTAKEN_LABEL(sandbox_cross_page_no_ilt:)
        stp x29, x30, [sp, #-16]!
        /* Do enough writes to cause the sandboxing code to split the block. */
        strb w0, [x1], #1
        strb w0, [x1], #1
        strb w0, [x1], #1
        strb w0, [x1], #1

        /* modify the "movz x9, #0" instruction. */
        adr x10, immediate_addr_plus_four - 4
        ldr w11, [x10]
        /* set the imm16 part of the movz instruction to w0 (the first argument.) */
        and w11, w11, #0xffe0001f /* set imm16 to 0. */
        orr w11, w11, w0, LSL #5 /* set imm16 to w0. */
        str w11, [x10]

        /* need to invalidate the instruction cache. This tests explicit app icache management. */
        stp x0, x1, [sp, #-16]!
        adr x0, sandbox_cross_page
        adr x1, sandbox_cross_page_end
        bl clear_icache_if_required
        ldp x0, x1, [sp], #16

        /* More writes to split the block. */
        strb w0, [x1], #1
        strb w0, [x1], #1
        strb w0, [x1], #1
        strb w0, [x1], #1

        movz x9, #0 /* this will get updated with the actual first argument */
ADDRTAKEN_LABEL(immediate_addr_plus_four:)
        mov x0, x9
        bl cross_page_check

        ldp x29, x30, [sp], #16

        ret
ADDRTAKEN_LABEL(sandbox_cross_page_end:)
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

ALIGN_WITH_NOPS(MAX_PAGE_SIZE_BYTES)
#ifdef X86
/* Get last_byte_jmp to have one byte in a sandboxed page. */
FILL_WITH_NOPS(MAX_PAGE_SIZE_BYTES - 6)  /* 6 bytes from instr sizes below. */
#else
FILL_WITH_NOPS(MAX_PAGE_SIZE_BYTES - 4)  /* 1 instruction at the end of the page */
#endif

#define FUNCNAME sandbox_last_byte
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        /* All these jmps have to be short for the test to pass.  Both gas and
         * masm get it right, so long as we always use local labels.  In
         * particular, global labels get 4 byte offsets in case they are
         * relocated.
         */
        jmp      last_byte_jmp          /* 2 bytes */
last_byte_ret_zero:
        xor      eax, eax               /* 2 bytes */
        ret                             /* 1 byte */
/* We use last_byte_jmp_no_ilt in C code to overwrite the offset. */
DECLARE_GLOBAL(last_byte_jmp_no_ilt)
ADDRTAKEN_LABEL(last_byte_jmp_no_ilt:)
last_byte_jmp:
        jmp      last_byte_ret_zero     /* 1 byte opcode + 1 byte rel offset */
last_byte_ret_one:
        mov      eax, HEX(1)
#else
        nop
DECLARE_GLOBAL(last_byte_jmp_label)
ADDRTAKEN_LABEL(last_byte_jmp_label:)
        b last_byte_jmp
last_byte_ret_zero:
        mov x0, #0
        ret
last_byte_jmp:
        b last_byte_ret_zero
last_byte_ret_one:
        mov x0, #1
#endif
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

ALIGN_WITH_NOPS(16)

#ifdef X86
#define FUNCNAME make_last_byte_selfmod
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        lea      REG_XAX, SYMREF(last_byte_immed_plus_four - 4)
        mov      DWORD [REG_XAX], HEX(0)     /* selfmod write */
        mov      REG_XAX, HEX(0)             /* mov_imm to modify */
ADDRTAKEN_LABEL(last_byte_immed_plus_four:)
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME
#else
#define FUNCNAME make_code_page_selfmod
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        adr x9, last_instruction - 4 /* modify some code to make the page sandboxed. */
        ldr w10, [x9]
        mov w11, #1
        orr w10, w10, w11, LSL #5
        str w10, [x9]
        movz x9, #0 /* this will get updated by the code above */
ADDRTAKEN_LABEL(last_instruction:)
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME
#endif /* X86 */

ALIGN_WITH_NOPS(4096)

#ifdef ASSEMBLE_WITH_MASM
_MYTEXT ENDS
#endif

#define FUNCNAME sandbox_fault
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
DECLARE_GLOBAL(sandbox_fault_no_ilt)
ADDRTAKEN_LABEL(sandbox_fault_no_ilt:)
#ifdef X86
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
#else
        stp x29, x30, [sp, #-16]!
        adr x9, fault_immediate_addr_plus_four - 4
        ldr w10, [x9]
        orr w10, w10, w0, LSL #5
        str w10, [x9]

        /* need to invalidate the instruction cache. This tests explicit app icache management. */
        stp x0, x1, [sp, #-16]!
        adr x0, sandbox_fault
        adr x1, sandbox_fault_end
        bl clear_icache_if_required
        ldp x0, x1, [sp], #16

        movz x0, #0 /* this will get updated by the code above */
ADDRTAKEN_LABEL(fault_immediate_addr_plus_four:)
        bl print_int

        movz x12, #7
        str x0, [x12] /* fault */

        ldp x29, x30, [sp], #16
        ret
ADDRTAKEN_LABEL(sandbox_fault_end:)
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME sandbox_illegal_instr
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
DECLARE_GLOBAL(sandbox_illegal_no_ilt)
ADDRTAKEN_LABEL(sandbox_illegal_no_ilt:)
#ifdef X86
        mov      REG_XAX, ARG1
        push     REG_XBP
        push     REG_XDX
        push     REG_XDI  /* for 16-alignment on x64 */

        lea      REG_XDX, SYMREF(illegal_immediate_addr_plus_four - 4)
        mov      DWORD [REG_XDX], eax        /* selfmod write */

        mov      REG_XDX, HEX(0)             /* mov_imm to modify */
ADDRTAKEN_LABEL(illegal_immediate_addr_plus_four:)
        lea      REG_XAX, SYMREF(print_int)
        CALLC1(REG_XAX, REG_XDX)

        /* Test the i#1441 scenario with 5 memory writes */
        mov      REG_XCX, REG_XSP
        mov      BYTE [REG_XCX - 1], 1
        mov      BYTE [REG_XCX - 2], 2
        mov      BYTE [REG_XCX - 3], 3
        mov      BYTE [REG_XCX - 4], 4
        mov      BYTE [REG_XCX - 5], 5
        ud2                                  /* fault */
        /* now this will be excluded, triggering i#1441: */
        mov      BYTE [REG_XCX - 6], 6

        /* restore */
        pop      REG_XDI
        pop      REG_XDX
        pop      REG_XBP
        ret
#else
        stp x29, x30, [sp, #-16]!

        /* modify the "movz x0, #0" instruction. */
        adr x9, illegal_immediate_addr_plus_four - 4
        ldr w10, [x9]
        orr w10, w10, w0, LSL #5
        str w10, [x9]

        /* need to invalidate the instruction cache. This tests explicit app icache management. */
        stp x0, x1, [sp, #-16]!
        adr x0, sandbox_illegal_instr
        adr x1, sandbox_illegal_instr_end
        bl clear_icache_if_required
        ldp x0, x1, [sp], #16

        movz x0, #0 /* this will get updated by the code above */
ADDRTAKEN_LABEL(illegal_immediate_addr_plus_four:)
        bl print_int

        /* Test the i#1441 scenario with 5 memory writes */
        sub sp, sp, #48 /* 6 * 8 */
        str x0, [sp, #(8 * 0)]
        str x0, [sp, #(8 * 1)]
        str x0, [sp, #(8 * 2)]
        str x0, [sp, #(8 * 3)]
        str x0, [sp, #(8 * 4)]
        .inst 0x00000000 /* illegal instruction. TODO Use udf opcode when supported. */
        /* now this will be excluded, triggering i#1441: */
        str x0, [sp, #(8 * 5)]

        add sp, sp, #48

        ldp x29, x30, [sp], #16
        ret
ADDRTAKEN_LABEL(sandbox_illegal_instr_end:)
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME


#define FUNCNAME sandbox_cti_tgt
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        /* modify OP_loop target via eax (so eflags save conflict) */
        lea      REG_XAX, SYMREF(loop_target_end - 1)
        mov      BYTE [REG_XAX], HEX(4)     /* selfmod write: skip both ud2 */
        mov      REG_XCX, 4
        loop     loop_orig_target
ADDRTAKEN_LABEL(loop_target_end:)
        ud2
loop_orig_target:
        ud2

        /* modify OP_loop target via OP_stosb which modifies its addr reg */
        push     REG_XDI
        lea      REG_XDI, SYMREF(loop_target_end2 - 1)
        mov      al, 4
        stosb    /* selfmod write: skip both ud2 */
        mov      REG_XCX, 4
        loop     loop_orig_target2
ADDRTAKEN_LABEL(loop_target_end2:)
        ud2
loop_orig_target2:
        ud2
        pop      REG_XDI

#    ifndef X64 /* to make the push immed easier */
        /* modify OP_loop target via OP_push */
        mov      REG_XAX, REG_XSP
        /* we clobber the 2 with 4 and overwrite the 2 ud2 with themselves */
        lea      REG_XSP, SYMREF(loop_target_end3 + 3)
        push     HEX(0b0f0b04)    /* selfmod write: skip both ud2 */
        mov      REG_XCX, 4
        loop     loop_orig_target3
ADDRTAKEN_LABEL(loop_target_end3:)
        ud2
loop_orig_target3:
        ud2
        mov      REG_XSP, REG_XAX
#    endif

        ret
#else
        stp x29, x30, [sp, #-16]!

        /* modify the "b branch_orig_target" instruction. */
        adr x9, branch_target_end - 4
        ldr w10, [x9]
        add w10, w10, 1 /* branch to 1 instr. further. */
        str w10, [x9]

        /* need to invalidate the instruction cache. This tests explicit app icache management. */
        adr x0, sandbox_cti_tgt
        adr x1, sandbox_cti_tgt_end
        bl clear_icache_if_required

        b branch_orig_target
ADDRTAKEN_LABEL(branch_target_end:)
        .inst 0x00000000 /* illegal instruction. TODO Use udf opcode when supported. */
branch_orig_target:
        .inst 0x00000000 /* illegal instruction. TODO Use udf opcode when supported. */

        /* modify "br reg" to avoid illegal instruction */
        adr x13, branch_orig_target2 + 4
        adr x9, branch_target_end2 - 4
        ldr w10, [x9]
        and w10, w10, #0XFFFFFC1F /* set Rn to 0 */
        mov w11, #(13 << 5)
        orr w10, w10, w11
        str w10, [x9]

        /* need to invalidate the instruction cache. This tests explicit app icache management. */
        stp x13, x13, [sp, #-16]!
        adr x0, sandbox_cti_tgt
        adr x1, sandbox_cti_tgt_end
        bl clear_icache_if_required
        ldr x13, [sp]
        add sp, sp, #16

        adr x12, branch_orig_target2
        br x12
ADDRTAKEN_LABEL(branch_target_end2:)
        .inst 0x00000000 /* illegal instruction. TODO Use udf opcode when supported. */
branch_orig_target2:
        .inst 0x00000000 /* illegal instruction. TODO Use udf opcode when supported. */

        ldp x29, x30, [sp], #16
        ret
ADDRTAKEN_LABEL(sandbox_cti_tgt_end:)
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#ifdef X86
/* First we do a self modification to have basic blocks in sandboxing mode.
 * Then we set the direction flag.
 * Then we enter a new basic block.
 * Hence we test sandboxing code with direction flag set.
 */
#define FUNCNAME sandbox_direction_flag
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
DECLARE_GLOBAL(sandbox_direction_flag_no_ilt)
ADDRTAKEN_LABEL(sandbox_direction_flag_no_ilt:)
        mov      REG_XAX, HEX(1)
        lea      REG_XDX, SYMREF(direction_flag_immediate_addr_plus_four - 4)
        mov      DWORD [REG_XDX], eax        /* selfmod write */
        mov      REG_XDX, HEX(0)             /* mov_imm to modify */
ADDRTAKEN_LABEL(direction_flag_immediate_addr_plus_four:)

        std                                  /* set direction flag */
        jmp      direction_flag_afterstd     /* jump begin a new basic block */

direction_flag_afterstd:
        nop                                  /* an unmodified basic block */
        cld                                  /* clearing direction flag */
        ret
END_FUNC(FUNCNAME)
#undef FUNCNAME
#endif /* X86 */

END_FILE
/* clang-format on */

#endif /* ASM_CODE_ONLY */
