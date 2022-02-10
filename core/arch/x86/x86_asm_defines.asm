/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
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
#  define MCXT_NUM_SIMD_SLOTS 6 /* [xy]mm0-5 */
# else
#  define MCXT_NUM_SIMD_SLOTS 32 /* [xyz]mm0-31 */
# endif
# define PRE_XMM_PADDING 48
#else
# define MCXT_NUM_SIMD_SLOTS 8 /* [xyz]mm0-7 */
# define PRE_XMM_PADDING 24
#endif
#define MCXT_NUM_OPMASK_SLOTS 8
#define OPMASK_AVX512BW_REG_SIZE 8
#define OPMASK_AVX512F_REG_SIZE 2
#define ZMM_REG_SIZE 64
#define MCXT_SIMD_SLOT_SIZE ZMM_REG_SIZE
/* xmm0-5/7/15 for PR 264138/i#139/PR 302107 */
#define MCXT_TOTAL_SIMD_SLOTS_SIZE ((MCXT_NUM_SIMD_SLOTS)*(MCXT_SIMD_SLOT_SIZE))
#define MCXT_TOTAL_OPMASK_SLOTS_SIZE ((MCXT_NUM_OPMASK_SLOTS)*(OPMASK_AVX512BW_REG_SIZE))

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
# define PRIV_MCXT_SIZE (18*ARG_SZ + PRE_XMM_PADDING + MCXT_TOTAL_SIMD_SLOTS_SIZE + \
                         MCXT_TOTAL_OPMASK_SLOTS_SIZE)
# define dstack_OFFSET     (PRIV_MCXT_SIZE+UPCXT_EXTRA+3*ARG_SZ)
# define MCONTEXT_PC_OFFS  (17*ARG_SZ)
#else
# define PUSHGPR \
        pusha
# define POPGPR  \
        popa
# define PRIV_MCXT_SIZE (10*ARG_SZ + PRE_XMM_PADDING + MCXT_TOTAL_SIMD_SLOTS_SIZE + \
                         MCXT_TOTAL_OPMASK_SLOTS_SIZE)
# define dstack_OFFSET     (PRIV_MCXT_SIZE+UPCXT_EXTRA+3*ARG_SZ)
# define MCONTEXT_PC_OFFS  (9*ARG_SZ)
#endif
/* offsetof(dcontext_t, is_exiting) */
#define is_exiting_OFFSET (dstack_OFFSET+1*ARG_SZ)
#define PUSHGPR_XAX_OFFS  (7*ARG_SZ)
#define PUSHGPR_XSP_OFFS  (3*ARG_SZ)
#define MCONTEXT_XSP_OFFS (PUSHGPR_XSP_OFFS)
#define MCONTEXT_XCX_OFFS (MCONTEXT_XSP_OFFS + 3*ARG_SZ)
#define MCONTEXT_XAX_OFFS (MCONTEXT_XSP_OFFS + 4*ARG_SZ)
#define PUSH_PRIV_MCXT_PRE_PC_SHIFT (- MCXT_TOTAL_SIMD_SLOTS_SIZE - \
                                     MCXT_TOTAL_OPMASK_SLOTS_SIZE - PRE_XMM_PADDING)

#if defined(WINDOWS) && !defined(X64)
/* FIXME: check these selector values on all platforms: these are for XPSP2.
 * Keep in synch w/ defines in arch.h.
 */
# define CS32_SELECTOR HEX(23)
# define CS64_SELECTOR HEX(33)
#endif

/* Defines shared between safe_read_asm() and the memcpy() and memset()
 * implementations in x86_asm_shared.asm.
 */
#ifdef X64
# define PTRSZ_SHIFT_BITS 3
# define PTRSZ_SUFFIXED(string_op) string_op##q
# ifdef UNIX
#  define ARGS_TO_XDI_XSI_XDX()         /* ABI handles this. */
#  define RESTORE_XDI_XSI()             /* Not needed. */
# else /* WINDOWS */
/* Get args 1, 2, 3 into rdi, rsi, and rdx. */
#  define ARGS_TO_XDI_XSI_XDX() \
        push     rdi                            @N@\
        push     rsi                            @N@\
        mov      rdi, ARG1                      @N@\
        mov      rsi, ARG2                      @N@\
        mov      rdx, ARG3
#  define RESTORE_XDI_XSI() \
        pop      rsi                            @N@\
        pop      rdi
# endif /* WINDOWS */
#else
# define PTRSZ_SHIFT_BITS 2
# define PTRSZ_SUFFIXED(string_op) string_op##d

/* Get args 1, 2, 3 into edi, esi, and edx to match Linux x64 ABI.  Need to save
 * edi and esi since they are callee-saved.  The ARGN macros can't handle
 * stack adjustments, so use the scratch regs eax and ecx to hold the args
 * before the pushes.
 */
# define ARGS_TO_XDI_XSI_XDX() \
        mov     eax, ARG1                       @N@\
        mov     ecx, ARG2                       @N@\
        mov     edx, ARG3                       @N@\
        push    edi                             @N@\
        push    esi                             @N@\
        mov     edi, eax                        @N@\
        mov     esi, ecx
# define RESTORE_XDI_XSI() \
        pop esi                                 @N@\
        pop edi
#endif

/* Repeats string_op for XDX bytes using aligned pointer-sized operations when
 * possible.  Assumes that string_op works by counting down until XCX reaches
 * zero.  The pointer-sized string ops are aligned based on ptr_to_align.
 * For string ops that have both a src and dst, aligning based on src is
 * preferred, subject to micro-architectural differences.
 *
 * XXX: glibc memcpy uses SSE instructions to copy, which is 10% faster on x64
 * and ~2x faster for 20kb copies on plain x86.  Using SSE is quite complicated,
 * because it means doing cpuid checks and loop unrolling.  Many of our string
 * operations are short anyway.  For safe_read, it also increases the number of
 * potentially faulting PCs.
 */
#define REP_STRING_OP(funcname, ptr_to_align, string_op) \
        mov     REG_XCX, ptr_to_align                           @N@\
        and     REG_XCX, (ARG_SZ - 1)                           @N@\
        jz      funcname##_aligned                              @N@\
        neg     REG_XCX                                         @N@\
        add     REG_XCX, ARG_SZ                                 @N@\
        cmp     REG_XDX, REG_XCX  /* if (n < xcx) */            @N@\
        cmovb   REG_XCX, REG_XDX  /*     xcx = n; */            @N@\
        sub     REG_XDX, REG_XCX                                @N@\
ADDRTAKEN_LABEL(funcname##_pre:)                                @N@\
        rep string_op##b                                        @N@\
funcname##_aligned:                                             @N@\
        /* Aligned word-size ops. */                            @N@\
        mov     REG_XCX, REG_XDX                                @N@\
        shr     REG_XCX, PTRSZ_SHIFT_BITS                       @N@\
ADDRTAKEN_LABEL(funcname##_mid:)                                @N@\
        rep PTRSZ_SUFFIXED(string_op)                           @N@\
        /* Handle trailing bytes. */                            @N@\
        mov     REG_XCX, REG_XDX                                @N@\
        and     REG_XCX, (ARG_SZ - 1)                           @N@\
ADDRTAKEN_LABEL(funcname##_post:)                               @N@\
        rep string_op##b


#endif /* _X86_ASM_DEFINES_ASM_ */
