/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
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
 * mcxtx_api.h
 *
 * Machine context struct.  Included into two separate
 * structs for internal and external use and so it has no include guard.
 * Not meant to be directly includedt into an implementation file!
 *
 */
/* clang-format off */
#if defined(AARCHXX)
    /* We want to simplify things by keeping this in register lists order.
     * We also want registers used by ibl to be placed together to fit on
     * the same 32-byte cache line, whether on a 32-bit or 64-bit machine,
     * or a 32-byte or 64-byte cache line.
     * Any changes in order here must be mirrored in arch/arm.asm offsets.
     */
    /* The stolen register slot only holds the app's value while in DR.
     * While in the cache, the app's value is stored in TLS in
     * dcontext->local_state->spill_space.reg_stolen, and the mcontext slot
     * actually holds DR's TLS base just due to a quirk of how fcache_enter
     * operates.
     */
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

#    ifdef X64 /* 64-bit */
    /**
     * The Arm AArch64 SIMD (DR_REG_Q0->DR_REG_Q31) and Scalable Vector
     * Extension (SVE) vector registers (DR_REG_Z0->DR_REG_Z31).
     */
    dr_simd_t simd[MCXT_NUM_SIMD_SVE_SLOTS];
    /**
     * The Arm AArch64 Scalable Vector Extension (SVE) predicate registers
     * DR_REG_P0 to DR_REG_P15.
     */
    dr_simd_t svep[MCXT_NUM_SVEP_SLOTS];
    /**
     * The Arm AArch64 Scalable Vector Extension (SVE) first fault register
     * DR_REG_FFR, for vector load instrcutions.
     */
    dr_simd_t ffr;
#   else
    /*
     * For the Arm AArch32 SIMD registers, we would probably be ok if we did
     * not preserve the callee-saved registers (q4-q7 == d8-d15) but to be safe
     * we preserve them all. We do not need anything more than word alignment
     * for OP_vldm/OP_vstm, and dr_simd_t has no fields larger than 32 bits, so
     * we have no padding.
     */
    /**
     * The Arm AArch32 SIMD registers.
     */
    dr_simd_t simd[MCXT_NUM_SIMD_SLOTS];
#   endif
#elif defined(X86)
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
        dr_zmm_t simd[MCXT_NUM_SIMD_SLOTS];
        /**
         * \deprecated The ymm field is provided for backward compatibility and is an
         * alias for the simd field.
         */
        dr_zmm_t ymm[MCXT_NUM_SIMD_SLOTS];
    };
    /** Storage for #MCXT_NUM_OPMASK_SLOTS mask registers as part of AVX-512. */
    dr_opmask_t opmask[MCXT_NUM_OPMASK_SLOTS];
#elif defined(RISCV64)
    /* FIXME i#3544: add rest of machine context and register aliases. */
    /* Any changes in order here must be mirrored in arch/riscv64.asm offsets.
     */
    union {
        /* FIXME i#3544: This is hard-wired to zero so could be removed */
        reg_t x0;   /**< The x0 register. */
        reg_t zero; /**< The hard-wired zero register. */
    }; /**< The anonymous union of alternative names for x0/zero register. */
    union {
        reg_t x1;   /**< The x1 register. */
        reg_t ra;  /**< The return address register. */
    };  /**< The anonymous union of alternative names for the x1/ra register. */
    union {
        reg_t x2;  /**< The x2 register. */
        reg_t sp;  /**< The stack pointer register.*/
        reg_t xsp; /**< The platform-independent name for the stack pointer register. */
    };  /**< The anonymous union of alternative names for the x2/sp register. */
    union {
        reg_t x3;  /**< The x3 register. */
        reg_t gp;  /**< The global pointer register. */
    };  /**< The anonymous union of alternative names for the x3/gp register. */
    union {
        reg_t x4;  /**< The x4 register. */
        reg_t tp;  /**< The thread pointer register. */
    };  /**< The anonymous union of alternative names for the x4/tp register. */
    union {
        reg_t x5;  /**< The x5 register. */
        reg_t t0;  /**< The 1st temporary register. */
    };  /**< The anonymous union of alternative names for the x5/t0 register. */
    union {
        reg_t x6;  /**< The x6 register. */
        reg_t t1;  /**< The 2nd temporary register. */
    };  /**< The anonymous union of alternative names for the x6/t1 register. */
    union {
        reg_t x7;  /**< The x7 register. */
        reg_t t2;  /**< The 3rd temporary register. */
    };  /**< The anonymous union of alternative names for the x7/t2 register. */
    union {
        reg_t x8;  /**< The x8 register. */
        reg_t s0;  /**< The 1st callee-saved register. */
        reg_t fp;  /**< The frame pointer register. */
    };  /**< The anonymous union of alternative names for the x8/s0/fp register. */
    union {
        reg_t x9;  /**< The x9 register. */
        reg_t s1;  /**< The 2nd callee-saved register. */
    };  /**< The anonymous union of alternative names for the x9/s1 register. */
    union {
        reg_t x10; /**< The x10 register. */
        reg_t a0;  /**< The 1st argument/return value register. */
    };  /**< The anonymous union of alternative names for the x10/a0 register. */
    union {
        reg_t x11; /**< The x11 register. */
        reg_t a1;  /**< The 2nd argument/return value register. */
    };  /**< The anonymous union of alternative names for the x11/a1 register. */
    union {
        reg_t x12; /**< The x12 register. */
        reg_t a2;  /**< The 3rd argument register. */
    };  /**< The anonymous union of alternative names for the x12/a2 register. */
    union {
        reg_t x13; /**< The x13 register. */
        reg_t a3;  /**< The 4th argument register. */
    };  /**< The anonymous union of alternative names for the x13/a3 register. */
    union {
        reg_t x14; /**< The x14 register. */
        reg_t a4;  /**< The 5th argument register. */
    };  /**< The anonymous union of alternative names for the x14/a4 register. */
    union {
        reg_t x15; /**< The x15 register. */
        reg_t a5;  /**< The 6th argument register. */
    };  /**< The anonymous union of alternative names for the x15/a5 register. */
    union {
        reg_t x16; /**< The x16 register. */
        reg_t a6;  /**< The 7th argument register. */
    };  /**< The anonymous union of alternative names for the x16/a6 register. */
    union {
        reg_t x17; /**< The x17 register. */
        reg_t a7;  /**< The 8th argument register. */
    };  /**< The anonymous union of alternative names for the x17/a7 register. */
    union {
        reg_t x18; /**< The x18 register. */
        reg_t s2;  /**< The 3rd callee-saved register. */
    };  /**< The anonymous union of alternative names for the x18/s2 register. */
    union {
        reg_t x19; /**< The x19 register. */
        reg_t s3;  /**< The 4th callee-saved register. */
    };  /**< The anonymous union of alternative names for the x19/s3 register. */
    union {
        reg_t x20; /**< The x20 register. */
        reg_t s4;  /**< The 5th callee-saved register. */
    };  /**< The anonymous union of alternative names for the x20/s4 register. */
    union {
        reg_t x21; /**< The x21 register. */
        reg_t s5;  /**< The 6th callee-saved register. */
    };  /**< The anonymous union of alternative names for the x21/s5 register. */
    union {
        reg_t x22; /**< The x22 register. */
        reg_t s6;  /**< The 7th callee-saved register. */
    };  /**< The anonymous union of alternative names for the x22/s6 register. */
    union {
        reg_t x23; /**< The x23 register. */
        reg_t s7;  /**< The 8th callee-saved register. */
    };  /**< The anonymous union of alternative names for the x23/s7 register. */
    union {
        reg_t x24; /**< The x24 register. */
        reg_t s8;  /**< The 9th callee-saved register. */
    };  /**< The anonymous union of alternative names for the x24/s8 register. */
    union {
        reg_t x25; /**< The x25 register. */
        reg_t s9;  /**< The 10th callee-saved register. */
    };  /**< The anonymous union of alternative names for the x25/s9 register. */
    union {
        reg_t x26; /**< The x26 register. */
        reg_t s10; /**< The 11th callee-saved register. */
    };  /**< The anonymous union of alternative names for the x26/s10 register. */
    union {
        reg_t x27; /**< The x27 register. */
        reg_t s11; /**< The 12th callee-saved register. */
    };  /**< The anonymous union of alternative names for the x27/s11 register. */
    union {
        reg_t x28; /**< The x28 register. */
        reg_t t3;  /**< The 4th temporary register. */
    };  /**< The anonymous union of alternative names for the s28/t3 register. */
    union {
        reg_t x29; /**< The x29 register. */
        reg_t t4;  /**< The 5th temporary register. */
    };  /**< The anonymous union of alternative names for the x29/t4 register. */
    union {
        reg_t x30; /**< The x30 register. */
        reg_t t5;  /**< The 6th temporary register. */
    };  /**< The anonymous union of alternative names for the x30/t5 register. */
    union {
        reg_t x31; /**< The x31 register. */
        reg_t t6;  /**< The 7th temporary register. */
    };  /**< The anonymous union of alternative names for the x31/t6 register. */
    /**
     * The program counter.
     * \note This field is not always set or read by all API routines.
     */
    byte *pc;
    union {
        reg_t f0;   /**< The f0 register. */
        reg_t ft0;  /**< The 1st temporary floating-point register. */
    };  /**< The anonymous union of alternative names for the f0/ft0 register. */
    union {
        reg_t f1;   /**< The f1 register. */
        reg_t ft1;  /**< The 2nd temporary floating-point register. */
    };  /**< The anonymous union of alternative names for the f1/ft1 register. */
    union {
        reg_t f2;   /**< The f2 register. */
        reg_t ft2;  /**< The 3rd temporary floating-point register. */
    };  /**< The anonymous union of alternative names for the f2/ft2 register. */
    union {
        reg_t f3;   /**< The f3 register. */
        reg_t ft3;  /**< The 4th temporary floating-point register. */
    };  /**< The anonymous union of alternative names for the f3/ft3 register. */
    union {
        reg_t f4;   /**< The f4 register. */
        reg_t ft4;  /**< The 5th temporary floating-point register. */
    };  /**< The anonymous union of alternative names for the f4/ft4 register. */
    union {
        reg_t f5;   /**< The f5 register. */
        reg_t ft5;  /**< The 6th temporary floating-point register. */
    };  /**< The anonymous union of alternative names for the f5/ft5 register. */
    union {
        reg_t f6;   /**< The f6 register. */
        reg_t ft6;  /**< The 7th temporary floating-point register. */
    };  /**< The anonymous union of alternative names for the f6/ft6 register. */
    union {
        reg_t f7;   /**< The f7 register. */
        reg_t ft7;  /**< The 8th temporary floating-point register. */
    };  /**< The anonymous union of alternative names for the f7/ft7 register. */
    union {
        reg_t f8;   /**< The f8 register. */
        reg_t fs0;  /**< The 1st callee-saved floating-point register. */
    };  /**< The anonymous union of alternative names for the f8/fs0 register. */
    union {
        reg_t f9;   /**< The f9 register. */
        reg_t fs1;  /**< The 2nd callee-saved floating-point register. */
    };  /**< The anonymous union of alternative names for the f9/fs1 register. */
    union {
        reg_t f10;  /**< The f10 register. */
        reg_t fa0;  /**< The 1st argument/return value floating-point register. */
    };  /**< The anonymous union of alternative names for the f10/fa0 register. */
    union {
        reg_t f11;  /**< The f11 register. */
        reg_t fa1;  /**< The 2nd argument/return value floating-point register. */
    };  /**< The anonymous union of alternative names for the f11/fa1 register. */
    union {
        reg_t f12;  /**< The f12 register. */
        reg_t fa2;  /**< The 3rd argument floating-point register. */
    };  /**< The anonymous union of alternative names for the f12/fa2 register. */
    union {
        reg_t f13;  /**< The f13 register. */
        reg_t fa3;  /**< The 4th argument floating-point register. */
    };  /**< The anonymous union of alternative names for the f13/fa3 register. */
    union {
        reg_t f14;  /**< The f14 register. */
        reg_t fa4;  /**< The 5th argument floating-point register. */
    };  /**< The anonymous union of alternative names for the f14/fa4 register. */
    union {
        reg_t f15;  /**< The f15 register. */
        reg_t fa5;  /**< The 6th argument floating-point register. */
    };  /**< The anonymous union of alternative names for the f15/fa5 register. */
    union {
        reg_t f16;  /**< The f16 register. */
        reg_t fa6;  /**< The 7th argument floating-point register. */
    };  /**< The anonymous union of alternative names for the f16/fa6 register. */
    union {
        reg_t f17;  /**< The f17 register. */
        reg_t fa7;  /**< The 8th argument floating-point register. */
    };  /**< The anonymous union of alternative names for the f17/fa7 register. */
    union {
        reg_t f18;  /**< The f18 register. */
        reg_t fs2;  /**< The 3rd callee-saved floating-point register. */
    };  /**< The anonymous union of alternative names for the f18/fs2 register. */
    union {
        reg_t f19;  /**< The f19 register. */
        reg_t fs3;  /**< The 4th callee-saved floating-point register. */
    };  /**< The anonymous union of alternative names for the f19/fs3 register. */
    union {
        reg_t f20;  /**< The f20 register. */
        reg_t fs4;  /**< The 5th callee-saved floating-point register. */
    };  /**< The anonymous union of alternative names for the f20/fs4 register. */
    union {
        reg_t f21;  /**< The f21 register. */
        reg_t fs5;  /**< The 6th callee-saved floating-point register. */
    };  /**< The anonymous union of alternative names for the f21/fs5 register. */
    union {
        reg_t f22;  /**< The f22 register. */
        reg_t fs6;  /**< The 7th callee-saved floating-point register. */
    };  /**< The anonymous union of alternative names for the f22/fs6 register. */
    union {
        reg_t f23;  /**< The f23 register. */
        reg_t fs7;  /**< The 8th callee-saved floating-point register. */
    };  /**< The anonymous union of alternative names for the f23/fs7 register. */
    union {
        reg_t f24;  /**< The f24 register. */
        reg_t fs8;  /**< The 9th callee-saved floating-point register. */
    };  /**< The anonymous union of alternative names for the f24/fs8 register. */
    union {
        reg_t f25;  /**< The f25 register. */
        reg_t fs9;  /**< The 10th callee-saved floating-point register. */
    };  /**< The anonymous union of alternative names for the f25/fs9 register. */
    union {
        reg_t f26;  /**< The f26 register. */
        reg_t fs10; /**< The 11th callee-saved floating-point register. */
    };  /**< The anonymous union of alternative names for the f26/fs10 register. */
    union {
        reg_t f27;  /**< The f27 register. */
        reg_t fs11; /**< The 12th callee-saved floating-point register. */
    };  /**< The anonymous union of alternative names for the f27/fs11 register. */
    union {
        reg_t f28;  /**< The f28 register. */
        reg_t ft8;  /**< The 9th temporary floating-point register. */
    };  /**< The anonymous union of alternative names for the f28/ft8 register. */
    union {
        reg_t f29;  /**< The f29 register. */
        reg_t ft9;  /**< The 10th temporary floating-point register. */
    };  /**< The anonymous union of alternative names for the f29/ft9 register. */
    union {
        reg_t f30;  /**< The f30 register. */
        reg_t ft10; /**< The 11th temporary floating-point register. */
    };  /**< The anonymous union of alternative names for the f30/ft10 register. */
    union {
        reg_t f31;  /**< The f31 register. */
        reg_t ft11; /**< The 12th temporary floating-point register. */
    };  /**< The anonymous union of alternative names for the f31/ft11 register. */
    reg_t fcsr; /**< Floating-Point Control Register. */
    /** The SIMD registers. No support for SIMD on RISC-V so far. */
    dr_simd_t simd[MCXT_NUM_SIMD_SLOTS];
#else /* RISCV64 */
#error Unsupported architecture
#endif
