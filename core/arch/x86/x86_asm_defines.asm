/* **********************************************************
 * Copyright (c) 2011-2015 Google, Inc.  All rights reserved.
 * Copyright (c) 2001-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2001 Hewlett-Packard Company */

/*
 * x86_asm_defines.asm - shared assembly defines
 */

#ifndef _X86_ASM_DEFINES_ASM_
#define _X86_ASM_DEFINES_ASM_ 1

/* We should give asm_defines.asm all unique names and then include globals.h
 * and avoid all this duplication!
 */
#ifdef X64
# ifdef WINDOWS
#  define NUM_XMM_SLOTS 6 /* xmm0-5 */
# else
#  define NUM_XMM_SLOTS 16 /* xmm0-15 */
# endif
# define PRE_XMM_PADDING 16
#else
# define NUM_XMM_SLOTS 8 /* xmm0-7 */
# define PRE_XMM_PADDING 24
#endif
#define XMM_SAVED_REG_SIZE 32 /* for ymm */
/* xmm0-5/7/15 for PR 264138/i#139/PR 302107 */
#define XMM_SAVED_SIZE ((NUM_XMM_SLOTS)*(XMM_SAVED_REG_SIZE))

#ifdef X64
/* push GPR registers in priv_mcontext_t order.  does NOT make xsp have a
 * pre-push value as no callers need that (they all use PUSH_PRIV_MCXT).
 * Leaves space for, but does NOT fill in, the xmm0-5 slots (PR 264138),
 * since it's hard to dynamically figure out during bootstrapping whether
 * movdqu or movups are legal instructions.  The caller is expected
 * to fill in the xmm values prior to any calls that may clobber them.
 */
# define PUSHGPR \
        push     r15 @N@\
        push     r14 @N@\
        push     r13 @N@\
        push     r12 @N@\
        push     r11 @N@\
        push     r10 @N@\
        push     r9  @N@\
        push     r8  @N@\
        push     rax @N@\
        push     rcx @N@\
        push     rdx @N@\
        push     rbx @N@\
        /* not the pusha pre-push rsp value but see above */ @N@\
        push     rsp @N@\
        push     rbp @N@\
        push     rsi @N@\
        push     rdi
# define POPGPR        \
        pop      rdi @N@\
        pop      rsi @N@\
        pop      rbp @N@\
        pop      rbx /* rsp into dead rbx */ @N@\
        pop      rbx @N@\
        pop      rdx @N@\
        pop      rcx @N@\
        pop      rax @N@\
        pop      r8  @N@\
        pop      r9  @N@\
        pop      r10 @N@\
        pop      r11 @N@\
        pop      r12 @N@\
        pop      r13 @N@\
        pop      r14 @N@\
        pop      r15 @N@
# define PRIV_MCXT_SIZE (18*ARG_SZ + PRE_XMM_PADDING + XMM_SAVED_SIZE)
# define dstack_OFFSET     (PRIV_MCXT_SIZE+UPCXT_EXTRA+3*ARG_SZ)
# define MCONTEXT_PC_OFFS  (17*ARG_SZ)
#else
# define PUSHGPR \
        pusha
# define POPGPR  \
        popa
# define PRIV_MCXT_SIZE (10*ARG_SZ + PRE_XMM_PADDING + XMM_SAVED_SIZE)
# define dstack_OFFSET     (PRIV_MCXT_SIZE+UPCXT_EXTRA+3*ARG_SZ)
# define MCONTEXT_PC_OFFS  (9*ARG_SZ)
#endif
/* offsetof(dcontext_t, is_exiting) */
#define is_exiting_OFFSET (dstack_OFFSET+1*ARG_SZ)
#define PUSHGPR_XSP_OFFS  (3*ARG_SZ)
#define MCONTEXT_XSP_OFFS (PUSHGPR_XSP_OFFS)
#define PUSH_PRIV_MCXT_PRE_PC_SHIFT (- XMM_SAVED_SIZE - PRE_XMM_PADDING)

#if defined(WINDOWS) && !defined(X64)
/* FIXME: check these selector values on all platforms: these are for XPSP2.
 * Keep in synch w/ defines in arch.h.
 */
# define CS32_SELECTOR HEX(23)
# define CS64_SELECTOR HEX(33)
#endif

#endif /* _X86_ASM_DEFINES_ASM_ */
