/* **********************************************************
 * Copyright (c) 2011-2016 Google, Inc.  All rights reserved.
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

#ifdef MACOS
#  define _XOPEN_SOURCE 700 /* required to get POSIX, etc. defines out of ucontext.h */
#  define __need_struct_ucontext64 /* seems to be missing from Mac headers */
#  include <ucontext.h>
#endif

#ifdef MACOS
/* mcontext_t is a pointer and we want the real thing */
/* We need room for avx.  If we end up with !YMM_ENABLED() we'll just end
 * up wasting some space in synched thread allocations.
 */
#  ifdef X64
typedef _STRUCT_MCONTEXT_AVX64 sigcontext_t; /* == __darwin_mcontext_avx64 */
#  else
typedef _STRUCT_MCONTEXT_AVX32 sigcontext_t; /* == __darwin_mcontext_avx32 */
#  endif
#else
typedef struct sigcontext sigcontext_t;
#endif

#ifdef LINUX
# define SIGCXT_FROM_UCXT(ucxt)  ((sigcontext_t*)(&((ucxt)->uc_mcontext)))
#elif defined(MACOS)
# ifdef X64
#  define SIGCXT_FROM_UCXT(ucxt) ((sigcontext_t*)((ucxt)->uc_mcontext64))
# else
#  define SIGCXT_FROM_UCXT(ucxt) ((sigcontext_t*)((ucxt)->uc_mcontext))
# endif
#endif

/* cross-platform sigcontext_t field access */
#ifdef MACOS
/* We're using _XOPEN_SOURCE >= 600 so we have __DARWIN_UNIX03 and thus leading __: */
# define SC_FIELD(name) __ss.__##name
#else
# define SC_FIELD(name) name
#endif
#ifdef X86
# ifdef X64
#  define SC_XIP SC_FIELD(rip)
#  define SC_XAX SC_FIELD(rax)
#  define SC_XCX SC_FIELD(rcx)
#  define SC_XDX SC_FIELD(rdx)
#  define SC_XBX SC_FIELD(rbx)
#  define SC_XSP SC_FIELD(rsp)
#  define SC_XBP SC_FIELD(rbp)
#  define SC_XSI SC_FIELD(rsi)
#  define SC_XDI SC_FIELD(rdi)
#  ifdef MACOS
#   define SC_XFLAGS SC_FIELD(rflags)
#  else
#   define SC_XFLAGS SC_FIELD(eflags)
#  endif
# else /* 32-bit */
#  define SC_XIP SC_FIELD(eip)
#  define SC_XAX SC_FIELD(eax)
#  define SC_XCX SC_FIELD(ecx)
#  define SC_XDX SC_FIELD(edx)
#  define SC_XBX SC_FIELD(ebx)
#  define SC_XSP SC_FIELD(esp)
#  define SC_XBP SC_FIELD(ebp)
#  define SC_XSI SC_FIELD(esi)
#  define SC_XDI SC_FIELD(edi)
#  define SC_XFLAGS SC_FIELD(eflags)
# endif /* 64/32-bit */
# define SC_FP SC_XBP
# define SC_SYSNUM_REG SC_XAX
# define SC_RETURN_REG SC_XAX
#elif defined(AARCH64)
   /* FIXME i#1569: NYI */
# define SC_XIP SC_FIELD(pc)
# define SC_FP  SC_FIELD(regs[29])
# define SC_R0  SC_FIELD(regs[0])
# define SC_R1  SC_FIELD(regs[1])
# define SC_R2  SC_FIELD(regs[2])
# define SC_LR  SC_FIELD(regs[30])
# define SC_XSP SC_FIELD(sp)
# define SC_XFLAGS SC_FIELD(pstate)
#elif defined(ARM)
# define SC_XIP SC_FIELD(arm_pc)
# define SC_FP  SC_FIELD(arm_fp)
# define SC_R0  SC_FIELD(arm_r0)
# define SC_R1  SC_FIELD(arm_r1)
# define SC_R2  SC_FIELD(arm_r2)
# define SC_R3  SC_FIELD(arm_r3)
# define SC_R4  SC_FIELD(arm_r4)
# define SC_R5  SC_FIELD(arm_r5)
# define SC_R6  SC_FIELD(arm_r6)
# define SC_R7  SC_FIELD(arm_r7)
# define SC_R8  SC_FIELD(arm_r8)
# define SC_R9  SC_FIELD(arm_r9)
# define SC_R10 SC_FIELD(arm_r10)
# define SC_R11 SC_FIELD(arm_fp)
# define SC_R12 SC_FIELD(arm_ip)
# define SC_XSP SC_FIELD(arm_sp)
# define SC_LR  SC_FIELD(arm_lr)
# define SC_XFLAGS SC_FIELD(arm_cpsr)
# define SC_SYSNUM_REG SC_R7
# define SC_RETURN_REG SC_R0
#endif /* X86/ARM */

#endif /* _OS_PUBLIC_H_ 1 */
