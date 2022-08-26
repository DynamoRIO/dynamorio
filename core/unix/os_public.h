/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/*
 * os_public.h - UNIX defines and typedefs shared with non-core
 */

#ifndef _OS_PUBLIC_H_
#define _OS_PUBLIC_H_ 1

#ifdef LINUX
#    include "include/sigcontext.h"
#endif

#ifdef MACOS
#    define _XOPEN_SOURCE                                            \
        700 /* required to get POSIX, etc. defines out of ucontext.h \
             */
#    define __need_struct_ucontext64 /* seems to be missing from Mac headers */
#    include <ucontext.h>
#endif

#ifdef MACOS
/* mcontext_t is a pointer and we want the real thing */
/* We need room for AVX512.  If we end up with !YMM_ENABLED() or !ZMM_ENABLED() we'll
 * just end up wasting some space in synched thread allocations.
 */
#    ifdef X64
/* TODO i#1979/i#1312: This should become _STRUCT_MCONTEXT_AVX512_64 ==
 * __darwin_mcontext_avx512_64.  However, we need to resolve the copy in
 * sigframe_rt_t as well (which currently is not labeled as a sigcontext_t).  We may
 * want to use a similar strategy to handle the different struct sizes for both.
 */
#        if defined(AARCH64)
typedef _STRUCT_MCONTEXT64 sigcontext_t;
#        else
typedef _STRUCT_MCONTEXT_AVX64 sigcontext_t; /* == __darwin_mcontext_avx64 */
#        endif

#    else
/* TODO i#1979/i#1312: Like for X64, this should become _STRUCT_MCONTEXT_AVX512_32 ==
 * __darwin_mcontext_avx512_32.
 */
typedef _STRUCT_MCONTEXT_AVX512_32 sigcontext_t; /* == __darwin_mcontext_avx32 */
#    endif
#else
typedef kernel_sigcontext_t sigcontext_t;
#endif

#ifdef LINUX
#    define SIGCXT_FROM_UCXT(ucxt) ((sigcontext_t *)(&((ucxt)->uc_mcontext)))
#elif defined(MACOS)
#    ifdef X64
#        define SIGCXT_FROM_UCXT(ucxt) ((sigcontext_t *)((ucxt)->uc_mcontext64))
#    else
#        define SIGCXT_FROM_UCXT(ucxt) ((sigcontext_t *)((ucxt)->uc_mcontext))
#    endif
#endif

/* cross-platform sigcontext_t field access */
#ifdef MACOS
/* We're using _XOPEN_SOURCE >= 600 so we have __DARWIN_UNIX03 and thus leading
 * __: */
#    define SC_FIELD(name) __ss.__##name
#else
#    define SC_FIELD(name) name
#endif
#ifdef X86
#    ifdef X64
#        define SC_XIP SC_FIELD(rip)
#        define SC_XAX SC_FIELD(rax)
#        define SC_XCX SC_FIELD(rcx)
#        define SC_XDX SC_FIELD(rdx)
#        define SC_XBX SC_FIELD(rbx)
#        define SC_XSP SC_FIELD(rsp)
#        define SC_XBP SC_FIELD(rbp)
#        define SC_XSI SC_FIELD(rsi)
#        define SC_XDI SC_FIELD(rdi)
#        define SC_R8 SC_FIELD(r9)
#        define SC_R9 SC_FIELD(r9)
#        define SC_R10 SC_FIELD(r10)
#        define SC_R11 SC_FIELD(r11)
#        define SC_R12 SC_FIELD(r12)
#        define SC_R13 SC_FIELD(r13)
#        define SC_R14 SC_FIELD(r14)
#        define SC_R15 SC_FIELD(r15)
#        ifdef MACOS
#            define SC_XFLAGS SC_FIELD(rflags)
#        else
#            define SC_XFLAGS SC_FIELD(eflags)
#        endif
#    else /* 32-bit */
#        define SC_XIP SC_FIELD(eip)
#        define SC_XAX SC_FIELD(eax)
#        define SC_XCX SC_FIELD(ecx)
#        define SC_XDX SC_FIELD(edx)
#        define SC_XBX SC_FIELD(ebx)
#        define SC_XSP SC_FIELD(esp)
#        define SC_XBP SC_FIELD(ebp)
#        define SC_XSI SC_FIELD(esi)
#        define SC_XDI SC_FIELD(edi)
#        define SC_XFLAGS SC_FIELD(eflags)
#    endif /* 64/32-bit */
#    define SC_FP SC_XBP
#    define SC_SYSNUM_REG SC_XAX
#    define SC_RETURN_REG SC_XAX
#elif defined(AARCH64)
#    ifdef MACOS
#        define SC_FIELD_AARCH64(n) SC_FIELD(x[n])
#        define SC_LR SC_FIELD(lr)
#        define SC_XFLAGS SC_FIELD(cpsr)
#        define SC_SYSNUM_REG SC_FIELD_AARCH64(16)
#    else
#        define SC_FIELD_AARCH64(n) SC_FIELD(regs[n])
#        define SC_LR SC_FIELD_AARCH64(30)
#        define SC_XFLAGS SC_FIELD(pstate)
#        define SC_SYSNUM_REG SC_FIELD_AARCH64(8)
#    endif
#    define SC_R0 SC_FIELD_AARCH64(0)
#    define SC_R1 SC_FIELD_AARCH64(1)
#    define SC_R2 SC_FIELD_AARCH64(2)
#    define SC_R3 SC_FIELD_AARCH64(3)
#    define SC_R4 SC_FIELD_AARCH64(4)
#    define SC_R5 SC_FIELD_AARCH64(5)
#    define SC_R6 SC_FIELD_AARCH64(6)
#    define SC_R7 SC_FIELD_AARCH64(7)
#    define SC_R8 SC_FIELD_AARCH64(8)
#    define SC_R9 SC_FIELD_AARCH64(9)
#    define SC_R10 SC_FIELD_AARCH64(10)
#    define SC_R11 SC_FIELD_AARCH64(11)
#    define SC_R12 SC_FIELD_AARCH64(12)
#    define SC_R13 SC_FIELD_AARCH64(13)
#    define SC_R14 SC_FIELD_AARCH64(14)
#    define SC_R15 SC_FIELD_AARCH64(15)
#    define SC_R16 SC_FIELD_AARCH64(16)
#    define SC_R17 SC_FIELD_AARCH64(17)
#    define SC_R18 SC_FIELD_AARCH64(18)
#    define SC_R19 SC_FIELD_AARCH64(19)
#    define SC_R20 SC_FIELD_AARCH64(20)
#    define SC_R21 SC_FIELD_AARCH64(21)
#    define SC_R22 SC_FIELD_AARCH64(22)
#    define SC_R23 SC_FIELD_AARCH64(23)
#    define SC_R24 SC_FIELD_AARCH64(24)
#    define SC_R25 SC_FIELD_AARCH64(25)
#    define SC_R26 SC_FIELD_AARCH64(26)
#    define SC_R27 SC_FIELD_AARCH64(27)
#    define SC_R28 SC_FIELD_AARCH64(28)
#    define SC_FP SC_FIELD_AARCH64(29)
#    define SC_XIP SC_FIELD(pc)
#    define SC_XSP SC_FIELD(sp)
#    define SC_RETURN_REG SC_R0
#elif defined(ARM)
#    define SC_XIP SC_FIELD(arm_pc)
#    define SC_FP SC_FIELD(arm_fp)
#    define SC_R0 SC_FIELD(arm_r0)
#    define SC_R1 SC_FIELD(arm_r1)
#    define SC_R2 SC_FIELD(arm_r2)
#    define SC_R3 SC_FIELD(arm_r3)
#    define SC_R4 SC_FIELD(arm_r4)
#    define SC_R5 SC_FIELD(arm_r5)
#    define SC_R6 SC_FIELD(arm_r6)
#    define SC_R7 SC_FIELD(arm_r7)
#    define SC_R8 SC_FIELD(arm_r8)
#    define SC_R9 SC_FIELD(arm_r9)
#    define SC_R10 SC_FIELD(arm_r10)
#    define SC_R11 SC_FIELD(arm_fp)
#    define SC_R12 SC_FIELD(arm_ip)
#    define SC_XSP SC_FIELD(arm_sp)
#    define SC_LR SC_FIELD(arm_lr)
#    define SC_XFLAGS SC_FIELD(arm_cpsr)
#    define SC_SYSNUM_REG SC_R7
#    define SC_RETURN_REG SC_R0
#elif defined(RISCV64)
#    define SC_A0 SC_FIELD(sc_regs.a0)
#    define SC_A1 SC_FIELD(sc_regs.a1)
#    define SC_A2 SC_FIELD(sc_regs.a2)
#    define SC_A3 SC_FIELD(sc_regs.a3)
#    define SC_A4 SC_FIELD(sc_regs.a4)
#    define SC_A5 SC_FIELD(sc_regs.a5)
#    define SC_A6 SC_FIELD(sc_regs.a6)
#    define SC_A7 SC_FIELD(sc_regs.a7)
#    define SC_FP SC_FIELD(sc_regs.s0)
#    define SC_RA SC_FIELD(sc_regs.ra)
#    define SC_XIP SC_FIELD(sc_regs.pc)
#    define SC_XSP SC_FIELD(sc_regs.sp)
#    define SC_SYSNUM_REG SC_A7
#    define SC_RETURN_REG SC_A0
#endif /* X86/ARM */

#endif /* _OS_PUBLIC_H_ 1 */
