/* **********************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
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

/*
 * mcxtx.h
 *
 * Machine context struct.  Included into two separate
 * structs for internal and external use.
 *
 */
/* START INCLUDE */

#ifdef AVOID_API_EXPORT
    /* FIXME: have special comment syntax instead of bogus ifdef to
     * get genapi to strip out internal-only comments? */
    /* Our inlined ibl uses eax-edx, so we place them together to fit
     * on the same 32-byte cache line; yet we also want to simplify
     * things by keeping this in pusha order.  Whether on a 32-bit or
     * 64-bit machine, or a 32-byte or 64-byte cache line, they will
     * still be on the same line, assuming this struct is
     * cache-line-aligned (which it is if in dcontext).
     * Any changes in order here must be mirrored in x86/x86.asm offsets.
     * UPDATE: actually we now use TLS for scratch slots.
     * See the list above of places that assume dr_mcxt_t layout.
     */
#endif
    union {
        reg_t xdi; /**< platform-independent name for full rdi/edi register */
        reg_t IF_X64_ELSE(rdi, edi); /**< platform-dependent name for rdi/edi register */
    }; /**< anonymous union of alternative names for rdi/edi register */
    union {
        reg_t xsi; /**< platform-independent name for full rsi/esi register */
        reg_t IF_X64_ELSE(rsi, esi); /**< platform-dependent name for rsi/esi register */
    }; /**< anonymous union of alternative names for rsi/esi register */
    union {
        reg_t xbp; /**< platform-independent name for full rbp/ebp register */
        reg_t IF_X64_ELSE(rbp, ebp); /**< platform-dependent name for rbp/ebp register */
    }; /**< anonymous union of alternative names for rbp/ebp register */
    union {
        reg_t xsp; /**< platform-independent name for full rsp/esp register */
        reg_t IF_X64_ELSE(rsp, esp); /**< platform-dependent name for rsp/esp register */
    }; /**< anonymous union of alternative names for rsp/esp register */
    union {
        reg_t xbx; /**< platform-independent name for full rbx/ebx register */
        reg_t IF_X64_ELSE(rbx, ebx); /**< platform-dependent name for rbx/ebx register */
    }; /**< anonymous union of alternative names for rbx/ebx register */
    union {
        reg_t xdx; /**< platform-independent name for full rdx/edx register */
        reg_t IF_X64_ELSE(rdx, edx); /**< platform-dependent name for rdx/edx register */
    }; /**< anonymous union of alternative names for rdx/edx register */
    union {
        reg_t xcx; /**< platform-independent name for full rcx/ecx register */
        reg_t IF_X64_ELSE(rcx, ecx); /**< platform-dependent name for rcx/ecx register */
    }; /**< anonymous union of alternative names for rcx/ecx register */
    union {
        reg_t xax; /**< platform-independent name for full rax/eax register */
        reg_t IF_X64_ELSE(rax, eax); /**< platform-dependent name for rax/eax register */
    }; /**< anonymous union of alternative names for rax/eax register */
#ifdef X64
    reg_t r8;  /**< r8 register. \note For 64-bit DR builds only. */
    reg_t r9;  /**< r9 register. \note For 64-bit DR builds only. */
    reg_t r10; /**< r10 register. \note For 64-bit DR builds only. */
    reg_t r11; /**< r11 register. \note For 64-bit DR builds only. */
    reg_t r12; /**< r12 register. \note For 64-bit DR builds only. */
    reg_t r13; /**< r13 register. \note For 64-bit DR builds only. */
    reg_t r14; /**< r14 register. \note For 64-bit DR builds only. */
    reg_t r15; /**< r15 register. \note For 64-bit DR builds only. */
#endif
    union {
        reg_t xflags; /**< platform-independent name for full rflags/eflags register */
        reg_t IF_X64_ELSE(rflags, eflags); /**< platform-dependent name for
                                                rflags/eflags register */
    }; /**< anonymous union of alternative names for rflags/eflags register */
    /**
     * Anonymous union of alternative names for the program counter /
     * instruction pointer (eip/rip).  This field is not always set or
     * read by all API routines.
     */
    union {
        byte *xip; /**< platform-independent name for full rip/eip register */
        byte *pc; /**< platform-independent alt name for full rip/eip register */
        byte *IF_X64_ELSE(rip, eip); /**< platform-dependent name for rip/eip register */
    };
    byte padding[PRE_XMM_PADDING]; /**< padding to get ymm field 32-byte aligned */
    /**
     * The SSE registers xmm0-xmm5 (-xmm15 on Linux) are volatile
     * (caller-saved) for 64-bit and WOW64, and are actually zeroed out on
     * Windows system calls.  These fields are ignored for 32-bit processes
     * that are not WOW64, or if the underlying processor does not support
     * SSE.  Use dr_mcontext_xmm_fields_valid() to determine whether the
     * fields are valid.
     *
     * When the fields are valid, on processors with AVX enabled (i.e.,
     * proc_has_feature(FEATURE_AVX) returns true), these fields will
     * contain the full ymm register values; otherwise, the top 128
     * bits of each slot will be undefined.
     */
#ifdef AVOID_API_EXPORT
    /* PR 264138: we must preserve xmm0-5 if on a 64-bit Windows kernel,
     * and xmm0-15 if in a 64-bit Linux app (PR 302107).  (Note that
     * mmx0-7 are also caller-saved on linux but we assume they're not
     * going to be used by DR, libc, or client routines: overlap w/
     * floating point).  For Windows we assume that none of our routines
     * (or libc routines that we call, except the floating-point ones,
     * where we explicitly save state) clobber beyond xmm0-5.  Rather than
     * have a separate WOW64 build, we have them in the struct but ignored
     * for normal 32-bit.
     * PR 306394: we preserve xmm0-7 for 32-bit linux too.
     */
#endif
    dr_ymm_t ymm[NUM_XMM_SLOTS];
