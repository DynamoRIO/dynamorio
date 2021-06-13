/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2009 VMware, Inc.  All rights reserved.
 * ********************************************************** */

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

#ifndef _ASM_DEFINES_ASM_
#define _ASM_DEFINES_ASM_ 1

/* Preprocessor macro definitions shared among all .asm files.
 * Since cpp macros can't generate newlines we have a later
 * script replace @N@ for us.
 */

#include "configure.h"

#ifdef DR_HOST_NOT_TARGET
#   undef X86
#   undef AARCH64
#   undef AARCHXX
#   undef X64
#   ifdef DR_HOST_X86
#       define X86
#   elif defined(DR_HOST_AARCH64)
#       define AARCH64
#   elif defined(DR_HOST_ARM)
#       define ARM
#   endif
#   if defined(DR_HOST_X64)
#       define X64
#   endif
#else
#   if (defined(X86_64) || defined(ARM_64)) && !defined(X64)
#     define X64
#   endif

#   if (defined(X86_64) || defined(X86_32)) && !defined(X86)
#     define X86
#   endif

#   if defined(ARM_64) && !defined(AARCH64)
#     define AARCH64
#   endif

#   if defined(ARM_32) && !defined(ARM)
#     define ARM
#   endif

#   if (defined(ARM_32) || defined(ARM_64)) && !defined(AARCHXX)
#     define AARCHXX
#   endif
#endif

#if (defined(ARM) && defined(X64)) || (defined(AARCH64) && !defined(X64))
# error ARM is only 32-bit; AARCH64 is 64-bit
#endif

#if defined(AARCHXX) && defined(WINDOWS)
# error ARM/AArch64 on Windows is not supported
#endif

#undef WEAK /* avoid conflict with C define */

/* This is the alignment needed by both x64 and 32-bit code except
 * for 32-bit Windows.
 * i#847: Investigate using aligned SSE ops (see get_xmm_caller_saved).
 */
#define FRAME_ALIGNMENT 16

/****************************************************/
#if defined(ASSEMBLE_WITH_GAS)
# define START_DATA .data
# define START_FILE .text
# define END_FILE /* nothing */
# define DECLARE_FUNC(symbol) \
.align 0 @N@\
.global symbol @N@\
.hidden symbol @N@\
.type symbol, %function
# define DECLARE_EXPORTED_FUNC(symbol) \
.align 0 @N@\
.global symbol @N@\
.type symbol, %function
# define END_FUNC(symbol) /* nothing */
# define DECLARE_GLOBAL(symbol) \
.global symbol @N@\
.hidden symbol
# define GLOBAL_LABEL(label) label
# define ADDRTAKEN_LABEL(label) label
# define GLOBAL_REF(label) label
# define WEAK(name) .weak name
# ifdef X86
#  define BYTE byte ptr
#  define WORD word ptr
#  define DWORD dword ptr
#  define QWORD qword ptr
#  define MMWORD qword ptr
#  define XMMWORD oword ptr
#  define YMMWORD ymmword ptr
#  define ZMMWORD zmmword ptr
#  ifdef X64
/* w/o the rip, gas won't use rip-rel and adds relocs that ld trips over */
#   define SYMREF(sym) [rip + sym]
#  else
#   define SYMREF(sym) [sym]
#  endif
# elif defined(AARCHXX)
#  define BYTE /* nothing */
#  define WORD /* nothing */
#  define DWORD /* nothing */
#  define QWORD /* nothing */
#  define MMWORD /* nothing */
#  define XMMWORD /* nothing */
#  define YMMWORD /* nothing */
#  define ZMMWORD /* nothing */
/* XXX: this will NOT produce PIC code!  A multi-instr multi-local-data sequence
 * must be used.  See cleanup_and_terminate() for examples.
 */
#  define SYMREF(sym) =sym
# endif
# ifdef X86
#  define HEX(n) 0x##n
# else
#  define POUND #
#  define HEX(n) POUND 0x##n
# endif
# define SEGMEM(seg,mem) seg:[mem]
# define DECL_EXTERN(symbol) /* nothing */
/* include newline so we can put multiple on one line */
# define RAW(n) .byte HEX(n) @N@
# define BYTES_ARR(symbol, n) symbol: .skip n, 0
# define DECLARE_FUNC_SEH(symbol) DECLARE_FUNC(symbol)
# define PUSH_SEH(reg) push reg
# define PUSH_NONCALLEE_SEH(reg) push reg
# define ADD_STACK_ALIGNMENT_NOSEH sub REG_XSP, FRAME_ALIGNMENT - ARG_SZ
# define ADD_STACK_ALIGNMENT ADD_STACK_ALIGNMENT_NOSEH
# define RESTORE_STACK_ALIGNMENT add REG_XSP, FRAME_ALIGNMENT - ARG_SZ
# define END_PROLOG /* nothing */
/* PR 212290: avoid text relocations.
 * @GOT returns the address and is for extern vars; @GOTOFF gets the value.
 * Note that using ## to paste =>
 *   "error: pasting "initstack_mutex" and "@" does not give a valid preprocessing token"
 * but whitespace separation seems fine.
 */
# define ADDR_VIA_GOT(base, sym) [sym @GOT + base]
# define VAR_VIA_GOT(base, sym) [sym @GOTOFF + base]
/****************************************************/
#elif defined(ASSEMBLE_WITH_MASM)
#define START_DATA .DATA
# ifdef X64
#  define START_FILE \
/* We add blank lines to match the 32-bit line count */ \
/* match .686 */  @N@\
/* match .XMM */  @N@\
/* match .MODEL */@N@\
/* match ASSUME */@N@\
.CODE
# else
#  define START_FILE \
.686 /* default is 8086! need 686 for sysenter */ @N@\
.XMM /* needed for fnclex and fxrstor */ @N@\
.MODEL flat, c @N@\
ASSUME fs:_DATA @N@\
.CODE
# endif
# define END_FILE END
/* we don't seem to need EXTERNDEF or GLOBAL */
# define DECLARE_FUNC(symbol) symbol PROC
# define DECLARE_EXPORTED_FUNC(symbol) symbol PROC EXPORT
# define END_FUNC(symbol) symbol ENDP
# define DECLARE_GLOBAL(symbol) PUBLIC symbol
/* XXX: this should be renamed FUNC_ENTRY_LABEL */
# define GLOBAL_LABEL(label)
/* use double colon (param always has its own colon) to make label visible outside proc */
# define ADDRTAKEN_LABEL(label) label:
# define GLOBAL_REF(label) label
# define WEAK(name) /* no support */
# define BYTE byte ptr
# define WORD word ptr
# define DWORD dword ptr
# define QWORD qword ptr
# define MMWORD mmword ptr
# define XMMWORD xmmword ptr
# define YMMWORD ymmword ptr /* added in VS 2010 */
# define ZMMWORD zmmword ptr /* XXX i#1312: supported by our supported version of VS? */
/* ml64 uses rip-rel automatically */
# define SYMREF(sym) [sym]
# define HEX(n) 0##n##h
# define SEGMEM(seg,mem) seg:[mem]
# define DECL_EXTERN(symbol) EXTERN symbol:PROC
/* include newline so we can put multiple on one line */
# define RAW(n) DB HEX(n) @N@
# define BYTES_ARR(symbol, n) symbol byte n dup (0)
# define ADD_STACK_ALIGNMENT_NOSEH sub REG_XSP, FRAME_ALIGNMENT - ARG_SZ
# define RESTORE_STACK_ALIGNMENT add REG_XSP, FRAME_ALIGNMENT - ARG_SZ
# ifdef X64
/* 64-bit SEH directives */
/* Declare non-leaf function (adjusts stack; makes calls; saves non-volatile regs): */
#  define DECLARE_FUNC_SEH(symbol) symbol PROC FRAME
/* Push a non-volatile register in prolog: */
#  define PUSH_SEH(reg) push reg @N@ .pushreg reg
/* Push a volatile register or an immed in prolog: */
#  define PUSH_NONCALLEE_SEH(reg) push reg @N@ .allocstack 8
#  define ADD_STACK_ALIGNMENT \
        ADD_STACK_ALIGNMENT_NOSEH @N@\
        .allocstack FRAME_ALIGNMENT - ARG_SZ
#  define END_PROLOG .endprolog
# else
#  define DECLARE_FUNC_SEH(symbol) DECLARE_FUNC(symbol)
#  define PUSH_SEH(reg) push reg @N@ /* add a line to match x64 line count */
#  define PUSH_NONCALLEE_SEH(reg) push reg @N@ /* add a line to match x64 line count */
#  define ADD_STACK_ALIGNMENT ADD_STACK_ALIGNMENT_NOSEH
#  define END_PROLOG /* nothing */
# endif
/****************************************************/
#elif defined(ASSEMBLE_WITH_NASM)
# define START_DATA SECTION .data
# define START_FILE SECTION .text
# define END_FILE /* nothing */
/* for MacOS, at least, we have to add _ ourselves */
# define DECLARE_FUNC(symbol) global _##symbol
# define DECLARE_EXPORTED_FUNC(symbol) global _##symbol
# define END_FUNC(symbol) /* nothing */
# define DECLARE_GLOBAL(symbol) global _##symbol
# define GLOBAL_LABEL(label) _##label
# define ADDRTAKEN_LABEL(label) _##label
# define GLOBAL_REF(label) _##label
# define WEAK(name) /* no support */
# define BYTE byte
# define WORD word
# define DWORD dword
# define QWORD qword
# define MMWORD qword
# define XMMWORD oword
# define YMMWORD yword
# define ZMMWORD zword
# ifdef X64
#  define SYMREF(sym) [rel GLOBAL_REF(sym)]
# else
#  define SYMREF(sym) [GLOBAL_REF(sym)]
# endif
# define HEX(n) 0x##n
# define SEGMEM(seg,mem) [seg:mem]
# define DECL_EXTERN(symbol) EXTERN GLOBAL_REF(symbol)
# define RAW(n) DB HEX(n) @N@
# define BYTES_ARR(symbol, n) symbol times n DB 0
# define DECLARE_FUNC_SEH(symbol) DECLARE_FUNC(symbol)
# define PUSH_SEH(reg) push reg
# define PUSH_NONCALLEE_SEH(reg) push reg
# define ADD_STACK_ALIGNMENT_NOSEH sub REG_XSP, FRAME_ALIGNMENT - ARG_SZ
# define ADD_STACK_ALIGNMENT ADD_STACK_ALIGNMENT_NOSEH
# define RESTORE_STACK_ALIGNMENT add REG_XSP, FRAME_ALIGNMENT - ARG_SZ
# define END_PROLOG /* nothing */
/****************************************************/
#else
# error Unknown assembler: set one of the ASSEMBLE_WITH_{GAS,MASM,NASM} defines
#endif

/****************************************************/
/* Macros for writing cross-platform 64-bit + 32-bit code */

/* register set */
#if defined(ARM)
# define REG_SP sp
# define REG_R0  r0
# define REG_R1  r1
# define REG_R2  r2
# define REG_R3  r3
# define REG_R4  r4
# define REG_R5  r5
# define REG_R6  r6
# define REG_R7  r7
# define REG_R8  r8
# define REG_R9  r9
# define REG_R10 r10
# define REG_R11 r11
# define REG_R12 r12
/* {r13, r14, r15} are used for {sp, lr, pc} on AArch32 */
#elif defined(AARCH64)
# define REG_R0  x0
# define REG_R1  x1
# define REG_R2  x2
# define REG_R3  x3
# define REG_R4  x4
# define REG_R5  x5
# define REG_R6  x6
# define REG_R7  x7
# define REG_R8  x8
# define REG_R9  x9
# define REG_R10 x10
# define REG_R11 x11
# define REG_R12 x12
/* skip [x13..x30], not available on AArch32 */
#else /* Intel X86 */
# ifdef X64
#  define REG_XAX rax
#  define REG_XBX rbx
#  define REG_XCX rcx
#  define REG_XDX rdx
#  define REG_XSI rsi
#  define REG_XDI rdi
#  define REG_XBP rbp
#  define REG_XSP rsp
#  define REG_R8  r8
#  define REG_R9  r9
#  define REG_R10 r10
#  define REG_R11 r11
#  define REG_R12 r12
#  define REG_R13 r13
#  define REG_R14 r14
#  define REG_R15 r15
#  define SEG_TLS gs /* keep in sync w/ {unix,win32}/os_exports.h defines */
#  if defined(UNIX) && !defined(MACOS)
#   define LIB_SEG_TLS fs /* keep in sync w/ unix/os_exports.h defines */
#  endif
# else /* 32-bit */
#  define REG_XAX eax
#  define REG_XBX ebx
#  define REG_XCX ecx
#  define REG_XDX edx
#  define REG_XSI esi
#  define REG_XDI edi
#  define REG_XBP ebp
#  define REG_XSP esp
#  define SEG_TLS fs /* keep in sync w/ {unix,win32}/os_exports.h defines */
#  ifdef UNIX
#   define LIB_SEG_TLS gs /* keep in sync w/ unix/os_exports.h defines */
#  endif
# endif /* 64/32-bit */
#  define REG_ZMM0 zmm0
#  define REG_ZMM1 zmm1
#endif /* ARM/X86 */

/* calling convention */
#ifdef X64
# define ARG_SZ 8
# define PTRSZ QWORD
#else /* 32-bit */
# define ARG_SZ 4
# define PTRSZ DWORD
#endif

#ifdef AARCHXX
/* ARM AArch64 calling convention:
 * SP:       stack pointer
 * x30(LR):  link register
 * x29(FP):  frame pointer
 * x19..x28: callee-saved registers
 * x18:      platform register if needed, otherwise, temp register
 * x17(IP1): the 2nd intra-procedure-call temp reg (for call veneers and PLT code)
 * x16(IP0): the 1st intra-procedure-call temp reg (for call veneers and PLT code)
 * x9..x15:  temp registers
 * x8:       indirect result location register
 * x0..x7:   parameter/result registers
 *
 * ARM AArch32 calling convention:
 * r15(PC):  program counter
 * r14(LR):  link register
 * r13(SP):  stack pointer
 * r12:      the Intra-Procedure-call scratch register
 * r4..r11:  callee-saved registers
 * r0..r3:   parameter/result registers
 * r0:       indirect result location register
 */
# define ARG1 REG_R0
# define ARG2 REG_R1
# define ARG3 REG_R2
# define ARG4 REG_R3
# ifdef X64
#  define ARG5 REG_R4
#  define ARG6 REG_R5
#  define ARG7 REG_R6
#  define ARG8 REG_R7
/* Arguments are passed on stack right-to-left. */
#  define ARG9  QWORD [REG_SP, #(0*ARG_SZ)] /* no ret addr */
#  define ARG10 QWORD [REG_SP, #(1*ARG_SZ)]
# else /* 32-bit */
#  define ARG5  DWORD [REG_SP, #(0*ARG_SZ)] /* no ret addr */
#  define ARG6  DWORD [REG_SP, #(1*ARG_SZ)]
#  define ARG7  DWORD [REG_SP, #(2*ARG_SZ)]
#  define ARG8  DWORD [REG_SP, #(3*ARG_SZ)]
#  define ARG9  DWORD [REG_SP, #(4*ARG_SZ)]
#  define ARG10 DWORD [REG_SP, #(5*ARG_SZ)]
# endif /* 64/32-bit */
# define ARG1_NORETADDR  ARG1
# define ARG2_NORETADDR  ARG2
# define ARG3_NORETADDR  ARG3
# define ARG4_NORETADDR  ARG4
# define ARG5_NORETADDR  ARG5
# define ARG6_NORETADDR  ARG6
# define ARG7_NORETADDR  ARG7
# define ARG8_NORETADDR  ARG8
# define ARG9_NORETADDR  ARG9
# define ARG10_NORETADDR ARG10

/* The macros SAVE_PRESERVED_REGS and RESTORE_PRESERVED_REGS save and restore the link
 * register and REG_PRESERVED_* so must be updated if more REG_PRESERVED_ regs are added.
 */
# ifndef AARCH64
#  define FP r11
#  define LR r14
#  define INDJMP bx
#  define REG_PRESERVED_1 r4
#  define SAVE_PRESERVED_REGS    push {REG_PRESERVED_1, LR}
#  define RESTORE_PRESERVED_REGS pop  {REG_PRESERVED_1, LR}
# else
#  define FP x29
#  define LR x30
#  define INDJMP br
#  define REG_PRESERVED_1 x19
#  define SAVE_PRESERVED_REGS    stp REG_PRESERVED_1, LR, [sp, #-16]!
#  define RESTORE_PRESERVED_REGS ldp REG_PRESERVED_1, LR, [sp], #16
# endif

#else /* Intel X86 */
# ifdef X64
#  ifdef WINDOWS
/* Arguments are passed in: rcx, rdx, r8, r9, then on stack right-to-left, but
 * leaving space on stack for the 1st 4.
 */
#   define ARG1  rcx
#   define ARG2  rdx
#   define ARG3  r8
#   define ARG4  r9
#   define ARG5  QWORD [5*ARG_SZ  + REG_XSP] /* includes ret addr */
#   define ARG6  QWORD [6*ARG_SZ  + REG_XSP]
#   define ARG7  QWORD [7*ARG_SZ  + REG_XSP]
#   define ARG8  QWORD [8*ARG_SZ  + REG_XSP]
#   define ARG9  QWORD [9*ARG_SZ  + REG_XSP]
#   define ARG10 QWORD [10*ARG_SZ + REG_XSP]
#   define ARG1_NORETADDR  ARG1
#   define ARG2_NORETADDR  ARG2
#   define ARG3_NORETADDR  ARG3
#   define ARG4_NORETADDR  ARG4
#   define ARG5_NORETADDR  QWORD [4*ARG_SZ + REG_XSP]
#   define ARG6_NORETADDR  QWORD [5*ARG_SZ + REG_XSP]
#   define ARG7_NORETADDR  QWORD [6*ARG_SZ + REG_XSP]
#   define ARG8_NORETADDR  QWORD [7*ARG_SZ + REG_XSP]
#   define ARG9_NORETADDR  QWORD [8*ARG_SZ + REG_XSP]
#   define ARG10_NORETADDR QWORD [9*ARG_SZ + REG_XSP]
#   define PUSH_CALLEE_SAVED_REGS() \
         PUSH_SEH(REG_XBX) @N@\
         PUSH_SEH(REG_XBP) @N@\
         PUSH_SEH(REG_XSI) @N@\
         PUSH_SEH(REG_XDI) @N@\
         PUSH_SEH(REG_R12) @N@\
         PUSH_SEH(REG_R13) @N@\
         PUSH_SEH(REG_R14) @N@\
         PUSH_SEH(REG_R15)
#   define POP_CALLEE_SAVED_REGS() \
         pop REG_R15 @N@\
         pop REG_R14 @N@\
         pop REG_R13 @N@\
         pop REG_R12 @N@\
         pop REG_XDI @N@\
         pop REG_XSI @N@\
         pop REG_XBP @N@\
         pop REG_XBX
#  else /* UNIX */
/* Arguments are passed in: rdi, rsi, rdx, rcx, r8, r9, then on stack right-to-left,
 * without leaving any space on stack for the 1st 6.
 */
#   define ARG1  rdi
#   define ARG2  rsi
#   define ARG3  rdx
#   define ARG4  rcx
#   define ARG5  r8
#   define ARG6  r9
#   define ARG7  QWORD [1*ARG_SZ + rsp] /* includes ret addr */
#   define ARG8  QWORD [2*ARG_SZ + rsp]
#   define ARG9  QWORD [3*ARG_SZ + rsp]
#   define ARG10 QWORD [4*ARG_SZ + rsp]
#   define ARG1_NORETADDR  ARG1
#   define ARG2_NORETADDR  ARG2
#   define ARG3_NORETADDR  ARG3
#   define ARG4_NORETADDR  ARG4
#   define ARG5_NORETADDR  ARG5
#   define ARG6_NORETADDR  ARG6
#   define ARG7_NORETADDR  QWORD [0*ARG_SZ + rsp]
#   define ARG8_NORETADDR  QWORD [1*ARG_SZ + rsp]
#   define ARG9_NORETADDR  QWORD [2*ARG_SZ + rsp]
#   define ARG10_NORETADDR QWORD [3*ARG_SZ + rsp]
#   define PUSH_CALLEE_SAVED_REGS() \
         PUSH_SEH(REG_XBX) @N@\
         PUSH_SEH(REG_XBP) @N@\
         PUSH_SEH(REG_R12) @N@\
         PUSH_SEH(REG_R13) @N@\
         PUSH_SEH(REG_R14) @N@\
         PUSH_SEH(REG_R15)
#   define POP_CALLEE_SAVED_REGS() \
         pop REG_R15 @N@\
         pop REG_R14 @N@\
         pop REG_R13 @N@\
         pop REG_R12 @N@\
         pop REG_XBP @N@\
         pop REG_XBX
#  endif /* WINDOWS/UNIX */
# else /* 32-bit */
/* Arguments are passed on stack right-to-left. */
#  define ARG1  DWORD [ 1*ARG_SZ + esp] /* includes ret addr */
#  define ARG2  DWORD [ 2*ARG_SZ + esp]
#  define ARG3  DWORD [ 3*ARG_SZ + esp]
#  define ARG4  DWORD [ 4*ARG_SZ + esp]
#  define ARG5  DWORD [ 5*ARG_SZ + esp]
#  define ARG6  DWORD [ 6*ARG_SZ + esp]
#  define ARG7  DWORD [ 7*ARG_SZ + esp]
#  define ARG8  DWORD [ 8*ARG_SZ + esp]
#  define ARG9  DWORD [ 9*ARG_SZ + esp]
#  define ARG10 DWORD [10*ARG_SZ + esp]
#  define ARG1_NORETADDR  DWORD [0*ARG_SZ + esp]
#  define ARG2_NORETADDR  DWORD [1*ARG_SZ + esp]
#  define ARG3_NORETADDR  DWORD [2*ARG_SZ + esp]
#  define ARG4_NORETADDR  DWORD [3*ARG_SZ + esp]
#  define ARG5_NORETADDR  DWORD [4*ARG_SZ + esp]
#  define ARG6_NORETADDR  DWORD [5*ARG_SZ + esp]
#  define ARG7_NORETADDR  DWORD [6*ARG_SZ + esp]
#  define ARG8_NORETADDR  DWORD [7*ARG_SZ + esp]
#  define ARG9_NORETADDR  DWORD [8*ARG_SZ + esp]
#  define ARG10_NORETADDR DWORD [9*ARG_SZ + esp]
#  ifdef WINDOWS
#  define PUSH_CALLEE_SAVED_REGS() \
        PUSH_SEH(REG_XBX) @N@\
        PUSH_SEH(REG_XBP) @N@\
        PUSH_SEH(REG_XSI) @N@\
        PUSH_SEH(REG_XDI)
#  define POP_CALLEE_SAVED_REGS() \
        pop REG_XDI @N@\
        pop REG_XSI @N@\
        pop REG_XBP @N@\
        pop REG_XBX
#  else /* UNIX */
#   define PUSH_CALLEE_SAVED_REGS() \
         PUSH_SEH(REG_XBX) @N@\
         PUSH_SEH(REG_XBP) @N@\
         PUSH_SEH(REG_XSI) @N@\
         PUSH_SEH(REG_XDI)
#   define POP_CALLEE_SAVED_REGS() \
         pop REG_XDI @N@\
         pop REG_XSI @N@\
         pop REG_XBP @N@\
         pop REG_XBX
#  endif /* WINDOWS/UNIX */
# endif /* 64/32-bit */
#endif /* ARM/X86 */

/* From globals_shared.h, but we can't include that into asm code. */
#ifdef X64
# define IF_X64(x) x
# define IF_X64_ELSE(x, y) x
# define IF_NOT_X64(x)
#else
# define IF_X64(x)
# define IF_X64_ELSE(x, y) y
# define IF_NOT_X64(x) x
#endif

#ifdef X64
# define PUSHF   pushfq
# define POPF    popfq
# ifdef WINDOWS
#  define STACK_PAD(tot, gt4, mod4) \
        lea      REG_XSP, [-32 - ARG_SZ*gt4 + REG_XSP]
#  define STACK_PAD_NOPUSH(tot, gt4, mod4) STACK_PAD(tot, gt4, mod4)
#  define STACK_UNPAD(tot, gt4, mod4) \
        lea      REG_XSP, [32 + ARG_SZ*gt4 + REG_XSP]
/* we split these out just to avoid nop lea for x64 linux */
#  define STACK_PAD_LE4(tot, mod4) STACK_PAD(0/*doesn't matter*/, 0, mod4)
#  define STACK_UNPAD_LE4(tot, mod4) STACK_UNPAD(tot, 0, mod4)
# else /* UNIX */
#  define STACK_PAD(tot, gt4, mod4) \
        lea      REG_XSP, [-ARG_SZ*gt4 + REG_XSP]
#  define STACK_PAD_NOPUSH(tot, gt4, mod4) STACK_PAD(tot, gt4, mod4)
#  define STACK_UNPAD(tot, gt4, mod4) \
        lea      REG_XSP, [ARG_SZ*gt4 + REG_XSP]
#  define STACK_PAD_LE4(tot, mod4) /* nothing */
#  define STACK_UNPAD_LE4(tot, mod4) /* nothing */
# endif /* Win/UNIX */
/* we split these out just to avoid nop lea for x86 mac */
# define STACK_PAD_ZERO STACK_PAD_LE4(0, 4)
# define STACK_UNPAD_ZERO STACK_UNPAD_LE4(0, 4)
# define SETARG(argoffs, argreg, p) \
        mov      argreg, p
#else /* 32-bit */
# define PUSHF   pushfd
# define POPF    popfd
# ifndef WINDOWS
/* Mac and Linux requires 32-bit to have 16-byte stack alignment (at call site).
 * Pass 4 instead of 0 for mod4 to avoid wasting stack space.
 */
#  define STACK_PAD(tot, gt4, mod4) \
        lea      REG_XSP, [-ARG_SZ*((tot) + 4 - (mod4)) + REG_XSP]
#  define STACK_PAD_NOPUSH(tot, gt4, mod4) STACK_PAD(tot, gt4, mod4)
#  define STACK_UNPAD(tot, gt4, mod4) \
        lea      REG_XSP, [ARG_SZ*((tot) + 4 - (mod4)) + REG_XSP]
#  define STACK_PAD_LE4(tot, mod4) STACK_PAD(tot, 0, mod4)
#  define STACK_UNPAD_LE4(tot, mod4) STACK_UNPAD(tot, 0, mod4)
#  define STACK_PAD_ZERO /* nothing */
#  define STACK_UNPAD_ZERO /* nothing */
/* p cannot be a memory operand, naturally */
#  define SETARG(argoffs, argreg, p) \
        mov      PTRSZ [ARG_SZ*argoffs + REG_XSP], p
# else /* WINDOWS */
#  define STACK_PAD(tot, gt4, mod4) /* nothing */
#  define STACK_PAD_NOPUSH(tot, gt4, mod4) \
        lea      REG_XSP, [-ARG_SZ*tot + REG_XSP]
#  define STACK_UNPAD(tot, gt4, mod4) \
        lea      REG_XSP, [ARG_SZ*tot + REG_XSP]
#  define STACK_PAD_LE4(tot, mod4) /* nothing */
#  define STACK_UNPAD_LE4(tot, mod4) STACK_UNPAD(tot, 0, mod4)
#  define STACK_PAD_ZERO /* nothing */
#  define STACK_UNPAD_ZERO /* nothing */
/* SETARG usage is order-dependent on 32-bit.  we could avoid that
 * by having STACK_PAD allocate the stack space and do a mov here.
 */
#  define SETARG(argoffs, argreg, p) \
        push     p
# endif /* WINDOWS */
#endif /* 64/32-bit */

/* CALLC* are for C calling convention callees only.
 * Caller must ensure that if params are passed in regs there are no conflicts.
 * Caller can rely on us storing each parameter in reverse order.
 * For x64, caller must arrange for 16-byte alignment at end of arg setup.
 */
#ifdef X86
# define CALLC0(callee)    \
        STACK_PAD_ZERO  @N@\
        call     callee @N@\
        STACK_UNPAD_ZERO
# define CALLC1(callee, p1)    \
        STACK_PAD_LE4(1, 1) @N@\
        SETARG(0, ARG1, p1) @N@\
        call     callee     @N@\
        STACK_UNPAD_LE4(1, 1)
# define CALLC2(callee, p1, p2)   \
        STACK_PAD_LE4(2, 2)    @N@\
        SETARG(1, ARG2, p2)    @N@\
        SETARG(0, ARG1, p1)    @N@\
        call     callee        @N@\
        STACK_UNPAD_LE4(2, 2)
# define CALLC3(callee, p1, p2, p3)   \
        STACK_PAD_LE4(3, 3)        @N@\
        SETARG(2, ARG3, p3)        @N@\
        SETARG(1, ARG2, p2)        @N@\
        SETARG(0, ARG1, p1)        @N@\
        call     callee            @N@\
        STACK_UNPAD_LE4(3, 3)
# define CALLC4(callee, p1, p2, p3, p4)   \
        STACK_PAD_LE4(4, 4)            @N@\
        SETARG(3, ARG4, p4)            @N@\
        SETARG(2, ARG3, p3)            @N@\
        SETARG(1, ARG2, p2)            @N@\
        SETARG(0, ARG1, p1)            @N@\
        call     callee                @N@\
        STACK_UNPAD_LE4(4, 4)
# define CALLC5(callee, p1, p2, p3, p4, p5)   \
        STACK_PAD(5, 1, 1)                 @N@\
        SETARG(4, ARG5_NORETADDR, p5)      @N@\
        SETARG(3, ARG4, p4)                @N@\
        SETARG(2, ARG3, p3)                @N@\
        SETARG(1, ARG2, p2)                @N@\
        SETARG(0, ARG1, p1)                @N@\
        call     callee                    @N@\
        STACK_UNPAD(5, 1, 1)
# define CALLC6(callee, p1, p2, p3, p4, p5, p6)\
        STACK_PAD(6, 2, 2)                 @N@\
        SETARG(5, ARG6_NORETADDR, p6)      @N@\
        SETARG(4, ARG5_NORETADDR, p5)      @N@\
        SETARG(3, ARG4, p4)                @N@\
        SETARG(2, ARG3, p3)                @N@\
        SETARG(1, ARG2, p2)                @N@\
        SETARG(0, ARG1, p1)                @N@\
        call     callee                    @N@\
        STACK_UNPAD(6, 2, 2)
# define CALLC7(callee, p1, p2, p3, p4, p5, p6, p7)\
        STACK_PAD(7, 3, 3)                 @N@\
        SETARG(6, ARG7_NORETADDR, p7)      @N@\
        SETARG(5, ARG6_NORETADDR, p6)      @N@\
        SETARG(4, ARG5_NORETADDR, p5)      @N@\
        SETARG(3, ARG4, p4)                @N@\
        SETARG(2, ARG3, p3)                @N@\
        SETARG(1, ARG2, p2)                @N@\
        SETARG(0, ARG1, p1)                @N@\
        call     callee                    @N@\
        STACK_UNPAD(7, 3, 3)

/* Versions to help with Mac stack aligmnment.  _FRESH means that the
 * stack has not been changed since function entry and thus for Mac 32-bit
 * esp ends in 0xc.  We simply add 1 to the # args the pad code should assume,
 * resulting in correct alignment when combined with the retaddr.
 */
# define CALLC1_FRESH(callee, p1) \
        STACK_PAD_LE4(1, 2) @N@\
        SETARG(0, ARG1, p1) @N@\
        call     callee     @N@\
        STACK_UNPAD_LE4(1, 2)
# define CALLC2_FRESH(callee, p1, p2) \
        STACK_PAD_LE4(2, 3)    @N@\
        SETARG(1, ARG2, p2)    @N@\
        SETARG(0, ARG1, p1)    @N@\
        call     callee        @N@\
        STACK_UNPAD_LE4(2, 3)
#elif defined(AARCH64)
# define CALLC0(callee)    \
        bl       callee
# define CALLC1(callee, p1)    \
        mov      ARG1, p1   @N@\
        bl       callee
# define CALLC2(callee, p1, p2)    \
        mov      ARG2, p2   @N@\
        mov      ARG1, p1   @N@\
        bl       callee
# define CALLC3(callee, p1, p2, p3)    \
        mov      ARG3, p3   @N@\
        mov      ARG2, p2   @N@\
        mov      ARG1, p1   @N@\
        bl       callee
# define CALLC4(callee, p1, p2, p3, p4)    \
        mov      ARG4, p4   @N@\
        mov      ARG3, p3   @N@\
        mov      ARG2, p2   @N@\
        mov      ARG1, p1   @N@\
        bl       callee
#elif defined(ARM)
/* Our assembly is ARM but our C code is Thumb so we use blx */
# define CALLC0(callee)    \
        blx      callee
# define CALLC1(callee, p1)    \
        mov      ARG1, p1   @N@\
        blx      callee
# define CALLC2(callee, p1, p2)    \
        mov      ARG2, p2   @N@\
        mov      ARG1, p1   @N@\
        blx      callee
# define CALLC3(callee, p1, p2, p3)    \
        mov      ARG3, p3   @N@\
        mov      ARG2, p2   @N@\
        mov      ARG1, p1   @N@\
        blx      callee
# define CALLC4(callee, p1, p2, p3, p4)    \
        mov      ARG4, p4   @N@\
        mov      ARG3, p3   @N@\
        mov      ARG2, p2   @N@\
        mov      ARG1, p1   @N@\
        blx      callee
#endif

/* For stdcall callees */
#ifdef X64
# define CALLWIN0 CALLC0
# define CALLWIN1 CALLC1
# define CALLWIN2 CALLC2
#else
# define CALLWIN0(callee)     \
        STACK_PAD_ZERO     @N@\
        call     callee
# define CALLWIN1(callee, p1)    \
        STACK_PAD_LE4(1, 1)   @N@\
        SETARG(0, ARG1, p1)   @N@\
        call     callee
# define CALLWIN2(callee, p1, p2)    \
        STACK_PAD_LE4(2, 2)       @N@\
        SETARG(1, ARG2, p2)       @N@\
        SETARG(0, ARG1, p1)       @N@\
        call     callee
#endif

/* For limited cross-platform asm */
#ifdef X86
# define REG_SCRATCH0 REG_XAX
# define REG_SCRATCH1 REG_XCX
# define REG_SCRATCH2 REG_XDX
# define REG_SP REG_XSP
# define JUMP     jmp
# define JUMP_NOT_EQUAL     jne
# define RETURN   ret
# define INC(reg) inc reg
# define DEC(reg) dec reg
#elif defined(AARCHXX)
# define REG_SCRATCH0 REG_R0
# define REG_SCRATCH1 REG_R1
# define REG_SCRATCH2 REG_R2
# define REG_SP sp
# define JUMP     b
# define JUMP_NOT_EQUAL     b.ne
# ifdef X64
#  define RETURN  ret
# else
#  define RETURN  bx lr
# endif
# ifdef X64
#  define MOV16   mov
# else
#  define MOV16   movw
# endif
# define INC(reg) add reg, reg, POUND 1
# define DEC(reg) sub reg, reg, POUND 1
#endif /* X86/ARM */

# define TRY_CXT_SETJMP_OFFS 0 /* offsetof(try_except_context_t, context) */

#endif /* _ASM_DEFINES_ASM_ */
