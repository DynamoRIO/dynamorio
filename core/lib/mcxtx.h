/* **********************************************************
 * Copyright (c) 2011-2019 Google, Inc.  All rights reserved.
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
/* clang-format off */
/* START INCLUDE */

#ifdef AARCHXX
#    ifdef AVOID_API_EXPORT
    /* FIXME: have special comment syntax instead of bogus ifdef to
     * get genapi to strip out internal-only comments? */
    /* We want to simplify things by keeping this in register lists order.
     * We also want registers used by ibl to be placed together to fit on
     * the same 32-byte cache line, whether on a 32-bit or 64-bit machine,
     * or a 32-byte or 64-byte cache line.
     * Any changes in order here must be mirrored in arch/arm.asm offsets.
     */
#    endif
    reg_t r0;   /**< The r0 register. */
    reg_t r1;   /**< The r1 register. */
    reg_t r2;   /**< The r2 register. */
    reg_t r3;   /**< The r3 register. */
    reg_t r4;   /**< The r4 register. */
    reg_t r5;   /**< The r5 register. */
    reg_t r6;   /**< The r6 register. */
    reg_t r7;   /**< The r7 register. */
    reg_t r8;   /**< The r8 register. */
    reg_t r9;   /**< The r9 register. */
    reg_t r10;  /**< The r10 register. */
    reg_t r11;  /**< The r11 register. */
    reg_t r12;  /**< The r12 register. */
#    ifdef X64 /* 64-bit */
    reg_t r13;  /**< The r13 register. */
    reg_t r14;  /**< The r14 register. */
    reg_t r15;  /**< The r15 register. */
    reg_t r16;  /**< The r16 register. \note For 64-bit DR builds only. */
    reg_t r17;  /**< The r17 register. \note For 64-bit DR builds only. */
    reg_t r18;  /**< The r18 register. \note For 64-bit DR builds only. */
    reg_t r19;  /**< The r19 register. \note For 64-bit DR builds only. */
    reg_t r20;  /**< The r20 register. \note For 64-bit DR builds only. */
    reg_t r21;  /**< The r21 register. \note For 64-bit DR builds only. */
    reg_t r22;  /**< The r22 register. \note For 64-bit DR builds only. */
    reg_t r23;  /**< The r23 register. \note For 64-bit DR builds only. */
    reg_t r24;  /**< The r24 register. \note For 64-bit DR builds only. */
    reg_t r25;  /**< The r25 register. \note For 64-bit DR builds only. */
    reg_t r26;  /**< The r26 register. \note For 64-bit DR builds only. */
    reg_t r27;  /**< The r27 register. \note For 64-bit DR builds only. */
    reg_t r28;  /**< The r28 register. \note For 64-bit DR builds only. */
    reg_t r29;  /**< The r29 register. \note For 64-bit DR builds only. */
    union {
        reg_t r30; /**< The r30 register. \note For 64-bit DR builds only. */
        reg_t lr;  /**< The link register. */
    }; /**< The anonymous union of alternative names for r30/lr register. */
    union {
        reg_t r31; /**< The r31 register. \note For 64-bit DR builds only. */
        reg_t sp;  /**< The stack pointer register. */
        reg_t xsp; /**< The platform-independent name for the stack pointer register. */
    }; /**< The anonymous union of alternative names for r31/sp register. */
    /**
     * The program counter.
     * \note This field is not always set or read by all API routines.
     */
    byte *pc;
    union {
        uint xflags; /**< The platform-independent name for condition flags. */
        struct {
            uint nzcv; /**< Condition flags (status register). */
            uint fpcr; /**< Floating-Point Control Register. */
            uint fpsr; /**< Floating-Point Status Register. */
        }; /**< AArch64 flag registers. */
    }; /**< The anonymous union of alternative names for flag registers. */
#    else /* 32-bit */
    union {
        reg_t r13; /**< The r13 register. */
        reg_t sp;  /**< The stack pointer register.*/
        reg_t xsp; /**< The platform-independent name for the stack pointer register. */
    }; /**< The anonymous union of alternative names for r13/sp register. */
    union {
        reg_t r14; /**< The r14 register. */
        reg_t lr;  /**< The link register. */
    }; /**< The anonymous union of alternative names for r14/lr register. */
    /**
     * The anonymous union of alternative names for r15/pc register.
     * \note This field is not always set or read by all API routines.
     */
    union {
        reg_t r15; /**< The r15 register. */
        byte *pc;  /**< The program counter. */
    };
    union {
        uint xflags; /**< The platform-independent name for full APSR register. */
        uint apsr; /**< The application program status registers in AArch32. */
        uint cpsr; /**< The current program status registers in AArch32. */
    }; /**< The anonymous union of alternative names for apsr/cpsr register. */
#    endif /* 64/32-bit */
    /**
     * The SIMD registers.  We would probably be ok if we did not preserve the
     * callee-saved registers (q4-q7 == d8-d15) but to be safe we preserve them
     * all.  We do not need anything more than word alignment for OP_vldm/OP_vstm,
     * and dr_simd_t has no fields larger than 32 bits, so we have no padding.
     */
    dr_simd_t simd[MCXT_NUM_SIMD_SLOTS];
#else /* X86 */
#    ifdef AVOID_API_EXPORT
    /* FIXME: have special comment syntax instead of bogus ifdef to
     * get genapi to strip out internal-only comments? */
    /* Our inlined ibl uses eax-edx, so we place them together to fit
     * on the same 32-byte cache line; yet we also want to simplify
     * things by keeping this in pusha order.  Whether on a 32-bit or
     * 64-bit machine, or a 32-byte or 64-byte cache line, they will
     * still be on the same line, assuming this struct is
     * cache-line-aligned (which it is if in dcontext).
     * Any changes in order here must be mirrored in arch/x86.asm offsets.
     * UPDATE: actually we now use TLS for scratch slots.
     * See the list above of places that assume dr_mcxt_t layout.
     */
#    endif
    union {
        reg_t xdi; /**< The platform-independent name for full rdi/edi register. */
        reg_t IF_X64_ELSE(rdi, edi); /**< The platform-dependent name for
                                          rdi/edi register. */
    }; /**< The anonymous union of alternative names for rdi/edi register. */
    union {
        reg_t xsi; /**< The platform-independent name for full rsi/esi register. */
        reg_t IF_X64_ELSE(rsi, esi); /**< The platform-dependent name for
                                          rsi/esi register. */
    }; /**< The anonymous union of alternative names for rsi/esi register. */
    union {
        reg_t xbp; /**< The platform-independent name for full rbp/ebp register. */
        reg_t IF_X64_ELSE(rbp, ebp); /**< The platform-dependent name for
                                          rbp/ebp register. */
    }; /**< The anonymous union of alternative names for rbp/ebp register. */
    union {
        reg_t xsp; /**< The platform-independent name for full rsp/esp register. */
        reg_t IF_X64_ELSE(rsp, esp); /**< The platform-dependent name for
                                          rsp/esp register. */
    }; /**< The anonymous union of alternative names for rsp/esp register. */
    union {
        reg_t xbx; /**< The platform-independent name for full rbx/ebx register. */
        reg_t IF_X64_ELSE(rbx, ebx); /**< The platform-dependent name for
                                          rbx/ebx register. */
    }; /**< The anonymous union of alternative names for rbx/ebx register. */
    union {
        reg_t xdx; /**< The platform-independent name for full rdx/edx register. */
        reg_t IF_X64_ELSE(rdx, edx); /**< The platform-dependent name for
                                          rdx/edx register. */
    }; /**< The anonymous union of alternative names for rdx/edx register. */
    union {
        reg_t xcx; /**< The platform-independent name for full rcx/ecx register. */
        reg_t IF_X64_ELSE(rcx, ecx); /**< The platform-dependent name for
                                          rcx/ecx register. */
    }; /**< The anonymous union of alternative names for rcx/ecx register. */
    union {
        reg_t xax; /**< The platform-independent name for full rax/eax register. */
        reg_t IF_X64_ELSE(rax, eax); /**< The platform-dependent name for
                                          rax/eax register. */
    }; /**< The anonymous union of alternative names for rax/eax register. */
#    ifdef X64
    reg_t r8;  /**< The r8 register. \note For 64-bit DR builds only. */
    reg_t r9;  /**< The r9 register. \note For 64-bit DR builds only. */
    reg_t r10; /**< The r10 register. \note For 64-bit DR builds only. */
    reg_t r11; /**< The r11 register. \note For 64-bit DR builds only. */
    reg_t r12; /**< The r12 register. \note For 64-bit DR builds only. */
    reg_t r13; /**< The r13 register. \note For 64-bit DR builds only. */
    reg_t r14; /**< The r14 register. \note For 64-bit DR builds only. */
    reg_t r15; /**< The r15 register. \note For 64-bit DR builds only. */
#    endif
    union {
        reg_t xflags; /**< The platform-independent name for
                           full rflags/eflags register. */
        reg_t IF_X64_ELSE(rflags, eflags); /**< The platform-dependent name for
                                                rflags/eflags register. */
    }; /**< The anonymous union of alternative names for rflags/eflags register. */
    /**
     * Anonymous union of alternative names for the program counter /
     * instruction pointer (eip/rip). \note This field is not always set or
     * read by all API routines.
     */
    union {
        byte *xip; /**< The platform-independent name for full rip/eip register. */
        byte *pc; /**< The platform-independent alt name for full rip/eip register. */
        byte *IF_X64_ELSE(rip, eip); /**< The platform-dependent name for
                                          rip/eip register. */
    };
    byte padding[PRE_XMM_PADDING]; /**< The padding to get zmm field 64-byte aligned. */
    /**
     * Anonymous union of alternative names for the simd structure in dr_mcontext_t. The
     * deprecated name ymm is provided for backward compatibility.
     */
    union {
        /**
         * The SSE registers xmm0-xmm5 (-xmm15 on Linux) are volatile
         * (caller-saved) for 64-bit and WOW64, and are actually zeroed out on
         * Windows system calls.  These fields are ignored for 32-bit processes
         * that are not WOW64, or if the underlying processor does not support
         * SSE.  Use dr_mcontext_xmm_fields_valid() to determine whether the
         * fields are valid. Use dr_mcontext_zmm_fields_valid() to determine
         * whether zmm registers are preserved.
         *
         * When the xmm fields are valid, on processors with AVX enabled (i.e.,
         * proc_has_feature() with #FEATURE_AVX returns true), these fields will
         * contain the full ymm register values; otherwise, the top 128
         * bits of each slot will be undefined.
         *
         * When the zmm fields are valid, it implies that
         * proc_has_feature() with #FEATURE_AVX512F is true. This is because DynamoRIO
         * will not attempt to fill zmm fields w/o support by the processor and OS. The
         * fields then will contain the full zmm register values.
         */
#    ifdef AVOID_API_EXPORT
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
         * DrMi#665: we now preserve all of the xmm registers.
         *
         * The size of mcontext's simd strucure has become a potential risk for
         * DynamoRIO's stack- and signal stack size or for general memory usage becoming
         * too large. Compared to AVX's ymm registers, the AVX-512 zmm register slots are
         * adding 1536 bytes on 64-bit on Linux. On 32-bit Linux, it is adding 256 bytes.
         * XXX i#1312: If this will become a problem, we may want to separate this out
         * into a heap structure and only maintain a pointer on the stack. This would save
         * space on memory constraint platforms as well as keep our signal stack size
         * smaller.
         * XXX i#1312: Currently, only 512 bytes are added on 64-bit until
         * MCXT_NUM_SIMD_SLOTS will be 32. This excludes AVX-512 k mask registers, which
         * will add another 64 bytes.
         */
#    endif
        dr_zmm_t simd[MCXT_NUM_SIMD_SLOTS];
        /**
         * \deprecated The ymm field is provided for backward compatibility and is an
         * alias for the simd field.
         */
        dr_zmm_t ymm[MCXT_NUM_SIMD_SLOTS];
    };
    /** Storage for #MCXT_NUM_OPMASK_SLOTS mask registers as part of AVX-512. */
    dr_opmask_t opmask[MCXT_NUM_OPMASK_SLOTS];
#endif /* ARM/X86 */
