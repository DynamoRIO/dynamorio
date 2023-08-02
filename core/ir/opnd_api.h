/* **********************************************************
 * Copyright (c) 2010-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
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

#ifndef _DR_IR_OPND_H_
#define _DR_IR_OPND_H_ 1

/****************************************************************************
 * OPERAND ROUTINES
 */
/**
 * @file dr_ir_opnd.h
 * @brief Functions and defines to create and manipulate instruction operands.
 */

/* Catch conflicts if ucontext.h is included before us */
#if defined(DR_REG_ENUM_COMPATIBILITY) && (defined(REG_EAX) || defined(REG_RAX))
#    error REG_ enum conflict between DR and ucontext.h!  Use DR_REG_ constants instead.
#endif

#ifndef INSTR_INLINE
#    ifdef DR_FAST_IR
#        define INSTR_INLINE inline
#    else
#        define INSTR_INLINE
#    endif
#endif

/* Memory operand sizes (with Intel's corresponding size names noted).
 *
 * Intel's size names are listed in 'Appendix A Opcode Map (Intel SDM Volume 2)'
 * specifically A.2.2 Codes for Operand Type
 *
 * For register operands, the DR_REG_ constants are used, which implicitly
 * state a size (e.g., DR_REG_CX is 2 bytes).
 * Use the type opnd_size_t for these values (we avoid typedef-ing the
 * enum, as its storage size is compiler-specific).  opnd_size_t is a
 * byte, so the largest value here needs to be <= 255.
 */
enum {
    /* For x86, register enum values are used for TYPE_*REG but we only use them
     * as opnd_size_t when we have the type available, so we can overlap
     * the two enums. If needed, the function template_optype_is_reg can be used
     * to check whether the operand type has an implicit size and stores the reg enum
     * instead of the size enum.
     * The reg_id_t type is now wider, but for x86 we ensure our values
     * all fit via an assert in d_r_arch_init().
     */
    OPSZ_NA = 0, /**< Sentinel value: not a valid size. */
    OPSZ_FIRST = OPSZ_NA,
    OPSZ_0,   /**< 0 bytes, for "sizeless" operands (for Intel, code
               * 'm': used for both start addresses (lea, invlpg) and
               * implicit constants (rol, fldl2e, etc.) */
    OPSZ_1,   /**< 1 byte (for Intel, code 'b') */
    OPSZ_2,   /**< 2 bytes (for Intel, code 'w') */
    OPSZ_4,   /**< 4 bytes (for Intel, code 'd','si') */
    OPSZ_6,   /**< 6 bytes (for Intel, code 'p','s') */
    OPSZ_8,   /**< 8 bytes (for Intel, code 'q','pi') */
    OPSZ_10,  /**< Intel 's' 64-bit, or double extended precision floating point
               * (latter used by fld, fstp, fbld, fbstp) */
    OPSZ_16,  /**< 16 bytes (for Intel, code 'dq','ps','pd','ss','sd', or AMD 'o') */
    OPSZ_14,  /**< FPU operating environment with short data size (fldenv, fnstenv) */
    OPSZ_28,  /**< FPU operating environment with normal data size (fldenv, fnstenv) */
    OPSZ_94,  /**< FPU state with short data size (fnsave, frstor) */
    OPSZ_108, /**< FPU state with normal data size (fnsave, frstor) */
    OPSZ_512, /**< FPU, MMX, XMM state (fxsave, fxrstor) */
    /**
     * The following sizes (OPSZ_*_short*) vary according to the cs segment and the
     * operand size prefix.  This IR assumes that the cs segment is set to the
     * default operand size.  The operand size prefix then functions to shrink the
     * size.  The IR does not explicitly mark the prefix; rather, a shortened size is
     * requested in the operands themselves, with the IR adding the prefix at encode
     * time.  Normally the fixed sizes above should be used rather than these
     * variable sizes, which are used internally by the IR and should only be
     * externally specified when building an operand in order to be flexible and
     * allow other operands to decide the size for the instruction (the prefix
     * applies to the entire instruction).
     */
    OPSZ_2_short1,        /**< Intel 'c': 2/1 bytes ("2/1" means 2 bytes normally, but if
                           * another operand requests a short size then this size can
                           * accommodate by shifting to its short size, which is 1
                           * byte). */
    OPSZ_4_short2,        /**< Intel 'z': 4/2 bytes */
    OPSZ_4_rex8_short2,   /**< Intel 'v': 8/4/2 bytes */
    OPSZ_4_rex8,          /**< Intel 'd/q' (like 'v' but never 2 bytes) or 'y'. */
    OPSZ_6_irex10_short4, /**< Intel 'p': On Intel processors this is 10/6/4 bytes for
                           * segment selector + address.  On AMD processors this is
                           * 6/4 bytes for segment selector + address (rex is ignored). */
    OPSZ_8_short2,        /**< partially resolved 4x8_short2 */
    OPSZ_8_short4,        /**< Intel 'a': pair of 4_short2 (bound) */
    OPSZ_28_short14,      /**< FPU operating env variable data size (fldenv, fnstenv) */
    OPSZ_108_short94,     /**< FPU state with variable data size (fnsave, frstor) */
    /** Varies by 32-bit versus 64-bit processor mode. */
    OPSZ_4x8,  /**< Full register size with no variation by prefix.
                *   Used for control and debug register moves and for Intel MPX. */
    OPSZ_6x10, /**< Intel 's': 6-byte (10-byte for 64-bit mode) table base + limit */
    /**
     * Stack operands not only vary by operand size specifications but also
     * by 32-bit versus 64-bit processor mode.
     */
    OPSZ_4x8_short2,    /**< Intel 'v'/'d64' for stack operations.
                         * Also 64-bit address-size specified operands, which are
                         * short4 rather than short2 in 64-bit mode (but short2 in
                         * 32-bit mode).
                         * Note that this IR does not distinguish extra stack
                         * operations performed by OP_enter w/ non-zero immed.
                         */
    OPSZ_4x8_short2xi8, /**< Intel 'f64': 4_short2 for 32-bit, 8_short2 for 64-bit AMD,
                         *   always 8 for 64-bit Intel */
    OPSZ_4_short2xi4,   /**< Intel 'f64': 4_short2 for 32-bit or 64-bit AMD,
                         *   always 4 for 64-bit Intel */
    /**
     * The following 3 sizes differ based on whether the modrm chooses a
     * register or memory.
     */
    OPSZ_1_reg4,  /**< Intel Rd/Mb: zero-extends if reg; used by pextrb */
    OPSZ_2_reg4,  /**< Intel Rd/Mw: zero-extends if reg; used by pextrw */
    OPSZ_4_reg16, /**< Intel Udq/Md: 4 bytes of xmm or 4 bytes of memory;
                   *   used by insertps. */
    /* Sizes used by new instructions */
    OPSZ_xsave,           /**< Size is > 512 bytes: use cpuid to determine.
                           * Used for FPU, MMX, XMM, etc. state by xsave and xrstor. */
    OPSZ_12,              /**< 12 bytes: 32-bit iret */
    OPSZ_32,              /**< 32 bytes: pusha/popa
                           * Also Intel 'qq','pd','ps','x': 32 bytes (256 bits) */
    OPSZ_40,              /**< 40 bytes: 64-bit iret */
    OPSZ_32_short16,      /**< unresolved pusha/popa */
    OPSZ_8_rex16,         /**< cmpxcgh8b/cmpxchg16b */
    OPSZ_8_rex16_short4,  /**< Intel 'v' * 2 (far call/ret) */
    OPSZ_12_rex40_short6, /**< unresolved iret */
    OPSZ_16_vex32,        /**< 16 or 32 bytes depending on VEX.L (AMD/Intel 'x'). */
    OPSZ_15, /**< All but one byte of an xmm register (used by OP_vpinsrb). */

    /* Needed for ARM.  We share the same namespace for now */
    OPSZ_3, /**< 3 bytes */
    /* gpl_list_num_bits assumes OPSZ_ includes every value from 1b to 12b
     * (except 8b/OPSZ_1) in order
     */
    OPSZ_1b,  /**< 1 bit */
    OPSZ_2b,  /**< 2 bits */
    OPSZ_3b,  /**< 3 bits */
    OPSZ_4b,  /**< 4 bits */
    OPSZ_5b,  /**< 5 bits */
    OPSZ_6b,  /**< 6 bits */
    OPSZ_7b,  /**< 7 bits */
    OPSZ_9b,  /**< 9 bits */
    OPSZ_10b, /**< 10 bits */
    OPSZ_11b, /**< 11 bits */
    OPSZ_12b, /**< 12 bits */
    OPSZ_20b, /**< 20 bits */
    OPSZ_25b, /**< 25 bits */
    /**
     * At encode or decode time, the size will match the size of the
     * register list operand in the containing instruction's operands.
     */
    OPSZ_VAR_REGLIST,
    OPSZ_20,  /**< 20 bytes.  Needed for load/store of register lists. */
    OPSZ_24,  /**< 24 bytes.  Needed for load/store of register lists. */
    OPSZ_36,  /**< 36 bytes.  Needed for load/store of register lists. */
    OPSZ_44,  /**< 44 bytes.  Needed for load/store of register lists. */
    OPSZ_48,  /**< 48 bytes.  Needed for load/store of register lists. */
    OPSZ_52,  /**< 52 bytes.  Needed for load/store of register lists. */
    OPSZ_56,  /**< 56 bytes.  Needed for load/store of register lists. */
    OPSZ_60,  /**< 60 bytes.  Needed for load/store of register lists. */
    OPSZ_64,  /**< 64 bytes.  Needed for load/store of register lists.
               * Also Intel: 64 bytes (512 bits) */
    OPSZ_68,  /**< 68 bytes.  Needed for load/store of register lists. */
    OPSZ_72,  /**< 72 bytes.  Needed for load/store of register lists. */
    OPSZ_76,  /**< 76 bytes.  Needed for load/store of register lists. */
    OPSZ_80,  /**< 80 bytes.  Needed for load/store of register lists. */
    OPSZ_84,  /**< 84 bytes.  Needed for load/store of register lists. */
    OPSZ_88,  /**< 88 bytes.  Needed for load/store of register lists. */
    OPSZ_92,  /**< 92 bytes.  Needed for load/store of register lists. */
    OPSZ_96,  /**< 96 bytes.  Needed for load/store of register lists. */
    OPSZ_100, /**< 100 bytes. Needed for load/store of register lists. */
    OPSZ_104, /**< 104 bytes. Needed for load/store of register lists. */
    /* OPSZ_108 already exists */
    OPSZ_112,             /**< 112 bytes. Needed for load/store of register lists. */
    OPSZ_116,             /**< 116 bytes. Needed for load/store of register lists. */
    OPSZ_120,             /**< 120 bytes. Needed for load/store of register lists. */
    OPSZ_124,             /**< 124 bytes. Needed for load/store of register lists. */
    OPSZ_128,             /**< 128 bytes. Needed for load/store of register lists. */
    OPSZ_SCALABLE,        /**< Scalable size for SVE vector registers. */
    OPSZ_SCALABLE_PRED,   /**< Scalable size for SVE predicate registers. */
    OPSZ_16_vex32_evex64, /**< 16, 32, or 64 bytes depending on EVEX.L and EVEX.LL'. */
    OPSZ_vex32_evex64,    /**< 32 or 64 bytes depending on EVEX.L and EVEX.LL'. */
    OPSZ_16_of_32_evex64, /**< 128 bits: half of YMM or quarter of ZMM depending on
                           * EVEX.LL'.
                           */
    OPSZ_32_of_64,        /**< 256 bits: half of ZMM. */
    OPSZ_4_of_32_evex64,  /**< 32 bits: can be part of YMM or ZMM register. */
    OPSZ_8_of_32_evex64,  /**< 64 bits: can be part of YMM or ZMM register. */
    OPSZ_8x16, /**< 8 or 16 bytes, but not based on rex prefix, instead dependent
                * on 32-bit/64-bit mode.
                */
    /* Add new size here.  Also update size_names[] in decode_shared.c along with
     * the size routines in opnd_shared.c.
     */
    OPSZ_LAST,
};

#ifdef X64
#    define OPSZ_PTR OPSZ_8      /**< Operand size for pointer values. */
#    define OPSZ_STACK OPSZ_8    /**< Operand size for stack push/pop operand sizes. */
#    define OPSZ_PTR_DBL OPSZ_16 /**< Double-pointer-sized. */
#    define OPSZ_PTR_HALF OPSZ_4 /**< Half-pointer-sized. */
#else
#    define OPSZ_PTR OPSZ_4      /**< Operand size for pointer values. */
#    define OPSZ_STACK OPSZ_4    /**< Operand size for stack push/pop operand sizes. */
#    define OPSZ_PTR_DBL OPSZ_8  /**< Double-pointer-sized. */
#    define OPSZ_PTR_HALF OPSZ_2 /**< Half-pointer-sized. */
#endif
#define OPSZ_VARSTACK                                          \
    OPSZ_4x8_short2 /**< Operand size for prefix-varying stack \
                     * push/pop operand sizes. */
#define OPSZ_REXVARSTACK                                      \
    OPSZ_4_rex8_short2 /* Operand size for prefix/rex-varying \
                        * stack push/pop like operand sizes. */

#define OPSZ_ret OPSZ_4x8_short2xi8 /**< Operand size for ret instruction. */
#define OPSZ_call OPSZ_ret          /**< Operand size for push portion of call. */

/* Convenience defines for specific opcodes */
#define OPSZ_lea OPSZ_0          /**< Operand size for lea memory reference. */
#define OPSZ_invlpg OPSZ_0       /**< Operand size for invlpg memory reference. */
#define OPSZ_bnd OPSZ_0          /**< Operand size for bndldx, bndstx memory reference. */
#define OPSZ_xlat OPSZ_1         /**< Operand size for xlat memory reference. */
#define OPSZ_clflush OPSZ_1      /**< Operand size for clflush memory reference. */
#define OPSZ_prefetch OPSZ_1     /**< Operand size for prefetch memory references. */
#define OPSZ_lgdt OPSZ_6x10      /**< Operand size for lgdt memory reference. */
#define OPSZ_sgdt OPSZ_6x10      /**< Operand size for sgdt memory reference. */
#define OPSZ_lidt OPSZ_6x10      /**< Operand size for lidt memory reference. */
#define OPSZ_sidt OPSZ_6x10      /**< Operand size for sidt memory reference. */
#define OPSZ_bound OPSZ_8_short4 /**< Operand size for bound memory reference. */
#define OPSZ_maskmovq OPSZ_8     /**< Operand size for maskmovq memory reference. */
#define OPSZ_maskmovdqu OPSZ_16  /**< Operand size for maskmovdqu memory reference. */
#define OPSZ_fldenv OPSZ_28_short14  /**< Operand size for fldenv memory reference. */
#define OPSZ_fnstenv OPSZ_28_short14 /**< Operand size for fnstenv memory reference. */
#define OPSZ_fnsave OPSZ_108_short94 /**< Operand size for fnsave memory reference. */
#define OPSZ_frstor OPSZ_108_short94 /**< Operand size for frstor memory reference. */
#define OPSZ_fxsave OPSZ_512         /**< Operand size for fxsave memory reference. */
#define OPSZ_fxrstor OPSZ_512        /**< Operand size for fxrstor memory reference. */
#define OPSZ_ptwrite OPSZ_4_rex8     /**< Operand size for ptwrite memory reference. */
#ifdef AARCH64
#    define OPSZ_sys OPSZ_1 /**< Operand size for sys instruction memory reference. */
#endif

/* We encode this enum plus the OPSZ_ extensions in bytes, so we can have
 * at most 256 total DR_REG_ plus OPSZ_ values.  Currently there are 165-odd.
 * Decoder assumes 32-bit, 16-bit, and 8-bit are in specific order
 * corresponding to modrm encodings.
 * We also assume that the DR_SEG_ constants are invalid as pointers for
 * our use in instr_info_t.code.
 * Also, reg_names array in encode.c corresponds to this enum order.
 * Plus, dr_reg_fixer array in encode.c.
 * Lots of optimizations assume same ordering of registers among
 * 32, 16, and 8  i.e. eax same position (first) in each etc.
 * reg_rm_selectable() assumes the GPR registers, mmx, and xmm are all in a row.
 */
/** Register identifiers. */
enum {
    /* The entire enum below overlaps with the OPSZ_ enum but all cases where the two are
     * used in the same field (instr_info_t operand sizes) have the type and distinguish
     * properly.
     * XXX i#3528: Switch from guaranteed-contiguous exposed enum ranges, which are not
     * possible to maintain long-term, to function interfaces.
     */
    DR_REG_NULL, /**< Sentinel value indicating no register, for address modes. */
#ifdef X86
    /* 64-bit general purpose */
    DR_REG_RAX, /**< The "rax" register. */
    DR_REG_RCX, /**< The "rcx" register. */
    DR_REG_RDX, /**< The "rdx" register. */
    DR_REG_RBX, /**< The "rbx" register. */
    DR_REG_RSP, /**< The "rsp" register. */
    DR_REG_RBP, /**< The "rbp" register. */
    DR_REG_RSI, /**< The "rsi" register. */
    DR_REG_RDI, /**< The "rdi" register. */
    DR_REG_R8,  /**< The "r8" register. */
    DR_REG_R9,  /**< The "r9" register. */
    DR_REG_R10, /**< The "r10" register. */
    DR_REG_R11, /**< The "r11" register. */
    DR_REG_R12, /**< The "r12" register. */
    DR_REG_R13, /**< The "r13" register. */
    DR_REG_R14, /**< The "r14" register. */
    DR_REG_R15, /**< The "r15" register. */
    /* 32-bit general purpose */
    DR_REG_EAX,  /**< The "eax" register. */
    DR_REG_ECX,  /**< The "ecx" register. */
    DR_REG_EDX,  /**< The "edx" register. */
    DR_REG_EBX,  /**< The "ebx" register. */
    DR_REG_ESP,  /**< The "esp" register. */
    DR_REG_EBP,  /**< The "ebp" register. */
    DR_REG_ESI,  /**< The "esi" register. */
    DR_REG_EDI,  /**< The "edi" register. */
    DR_REG_R8D,  /**< The "r8d" register. */
    DR_REG_R9D,  /**< The "r9d" register. */
    DR_REG_R10D, /**< The "r10d" register. */
    DR_REG_R11D, /**< The "r11d" register. */
    DR_REG_R12D, /**< The "r12d" register. */
    DR_REG_R13D, /**< The "r13d" register. */
    DR_REG_R14D, /**< The "r14d" register. */
    DR_REG_R15D, /**< The "r15d" register. */
    /* 16-bit general purpose */
    DR_REG_AX,   /**< The "ax" register. */
    DR_REG_CX,   /**< The "cx" register. */
    DR_REG_DX,   /**< The "dx" register. */
    DR_REG_BX,   /**< The "bx" register. */
    DR_REG_SP,   /**< The "sp" register. */
    DR_REG_BP,   /**< The "bp" register. */
    DR_REG_SI,   /**< The "si" register. */
    DR_REG_DI,   /**< The "di" register. */
    DR_REG_R8W,  /**< The "r8w" register. */
    DR_REG_R9W,  /**< The "r9w" register. */
    DR_REG_R10W, /**< The "r10w" register. */
    DR_REG_R11W, /**< The "r11w" register. */
    DR_REG_R12W, /**< The "r12w" register. */
    DR_REG_R13W, /**< The "r13w" register. */
    DR_REG_R14W, /**< The "r14w" register. */
    DR_REG_R15W, /**< The "r15w" register. */
    /* 8-bit general purpose */
    DR_REG_AL,   /**< The "al" register. */
    DR_REG_CL,   /**< The "cl" register. */
    DR_REG_DL,   /**< The "dl" register. */
    DR_REG_BL,   /**< The "bl" register. */
    DR_REG_AH,   /**< The "ah" register. */
    DR_REG_CH,   /**< The "ch" register. */
    DR_REG_DH,   /**< The "dh" register. */
    DR_REG_BH,   /**< The "bh" register. */
    DR_REG_R8L,  /**< The "r8l" register. */
    DR_REG_R9L,  /**< The "r9l" register. */
    DR_REG_R10L, /**< The "r10l" register. */
    DR_REG_R11L, /**< The "r11l" register. */
    DR_REG_R12L, /**< The "r12l" register. */
    DR_REG_R13L, /**< The "r13l" register. */
    DR_REG_R14L, /**< The "r14l" register. */
    DR_REG_R15L, /**< The "r15l" register. */
    DR_REG_SPL,  /**< The "spl" register. */
    DR_REG_BPL,  /**< The "bpl" register. */
    DR_REG_SIL,  /**< The "sil" register. */
    DR_REG_DIL,  /**< The "dil" register. */
    /* 64-BIT MMX */
    DR_REG_MM0, /**< The "mm0" register. */
    DR_REG_MM1, /**< The "mm1" register. */
    DR_REG_MM2, /**< The "mm2" register. */
    DR_REG_MM3, /**< The "mm3" register. */
    DR_REG_MM4, /**< The "mm4" register. */
    DR_REG_MM5, /**< The "mm5" register. */
    DR_REG_MM6, /**< The "mm6" register. */
    DR_REG_MM7, /**< The "mm7" register. */
    /* 128-BIT XMM */
    DR_REG_XMM0,  /**< The "xmm0" register. */
    DR_REG_XMM1,  /**< The "xmm1" register. */
    DR_REG_XMM2,  /**< The "xmm2" register. */
    DR_REG_XMM3,  /**< The "xmm3" register. */
    DR_REG_XMM4,  /**< The "xmm4" register. */
    DR_REG_XMM5,  /**< The "xmm5" register. */
    DR_REG_XMM6,  /**< The "xmm6" register. */
    DR_REG_XMM7,  /**< The "xmm7" register. */
    DR_REG_XMM8,  /**< The "xmm8" register. */
    DR_REG_XMM9,  /**< The "xmm9" register. */
    DR_REG_XMM10, /**< The "xmm10" register. */
    DR_REG_XMM11, /**< The "xmm11" register. */
    DR_REG_XMM12, /**< The "xmm12" register. */
    DR_REG_XMM13, /**< The "xmm13" register. */
    DR_REG_XMM14, /**< The "xmm14" register. */
    DR_REG_XMM15, /**< The "xmm15" register. */
    DR_REG_XMM16, /**< The "xmm16" register. */
    DR_REG_XMM17, /**< The "xmm17" register. */
    DR_REG_XMM18, /**< The "xmm18" register. */
    DR_REG_XMM19, /**< The "xmm19" register. */
    DR_REG_XMM20, /**< The "xmm20" register. */
    DR_REG_XMM21, /**< The "xmm21" register. */
    DR_REG_XMM22, /**< The "xmm22" register. */
    DR_REG_XMM23, /**< The "xmm23" register. */
    DR_REG_XMM24, /**< The "xmm24" register. */
    DR_REG_XMM25, /**< The "xmm25" register. */
    DR_REG_XMM26, /**< The "xmm26" register. */
    DR_REG_XMM27, /**< The "xmm27" register. */
    DR_REG_XMM28, /**< The "xmm28" register. */
    DR_REG_XMM29, /**< The "xmm29" register. */
    DR_REG_XMM30, /**< The "xmm30" register. */
    DR_REG_XMM31, /**< The "xmm31" register. */
    /* 32 enums are reserved for future Intel SIMD extensions. */
    RESERVED_XMM = DR_REG_XMM31 + 32,
    /* floating point registers */
    DR_REG_ST0, /**< The "st0" register. */
    DR_REG_ST1, /**< The "st1" register. */
    DR_REG_ST2, /**< The "st2" register. */
    DR_REG_ST3, /**< The "st3" register. */
    DR_REG_ST4, /**< The "st4" register. */
    DR_REG_ST5, /**< The "st5" register. */
    DR_REG_ST6, /**< The "st6" register. */
    DR_REG_ST7, /**< The "st7" register. */
    /* segments (order from "Sreg" description in Intel manual) */
    DR_SEG_ES, /**< The "es" register. */
    DR_SEG_CS, /**< The "cs" register. */
    DR_SEG_SS, /**< The "ss" register. */
    DR_SEG_DS, /**< The "ds" register. */
    DR_SEG_FS, /**< The "fs" register. */
    DR_SEG_GS, /**< The "gs" register. */
    /* debug & control registers (privileged access only; 8-15 for future processors)
     */
    DR_REG_DR0,  /**< The "dr0" register. */
    DR_REG_DR1,  /**< The "dr1" register. */
    DR_REG_DR2,  /**< The "dr2" register. */
    DR_REG_DR3,  /**< The "dr3" register. */
    DR_REG_DR4,  /**< The "dr4" register. */
    DR_REG_DR5,  /**< The "dr5" register. */
    DR_REG_DR6,  /**< The "dr6" register. */
    DR_REG_DR7,  /**< The "dr7" register. */
    DR_REG_DR8,  /**< The "dr8" register. */
    DR_REG_DR9,  /**< The "dr9" register. */
    DR_REG_DR10, /**< The "dr10" register. */
    DR_REG_DR11, /**< The "dr11" register. */
    DR_REG_DR12, /**< The "dr12" register. */
    DR_REG_DR13, /**< The "dr13" register. */
    DR_REG_DR14, /**< The "dr14" register. */
    DR_REG_DR15, /**< The "dr15" register. */
    /* cr9-cr15 do not yet exist on current x64 hardware */
    DR_REG_CR0,  /**< The "cr0" register. */
    DR_REG_CR1,  /**< The "cr1" register. */
    DR_REG_CR2,  /**< The "cr2" register. */
    DR_REG_CR3,  /**< The "cr3" register. */
    DR_REG_CR4,  /**< The "cr4" register. */
    DR_REG_CR5,  /**< The "cr5" register. */
    DR_REG_CR6,  /**< The "cr6" register. */
    DR_REG_CR7,  /**< The "cr7" register. */
    DR_REG_CR8,  /**< The "cr8" register. */
    DR_REG_CR9,  /**< The "cr9" register. */
    DR_REG_CR10, /**< The "cr10" register. */
    DR_REG_CR11, /**< The "cr11" register. */
    DR_REG_CR12, /**< The "cr12" register. */
    DR_REG_CR13, /**< The "cr13" register. */
    DR_REG_CR14, /**< The "cr14" register. */
    DR_REG_CR15, /**< The "cr15" register. */
    /* All registers above this point may be used as opnd_size_t and therefore
     * need to fit into a byte (checked in d_r_arch_init()). Register enums
     * below this point must not be used as opnd_size_t.
     */
    DR_REG_MAX_AS_OPSZ = DR_REG_CR15, /**< The "cr15" register. */
    DR_REG_INVALID, /**< Sentinel value indicating an invalid register. */
    /* 256-BIT YMM */
    DR_REG_YMM0,  /**< The "ymm0" register. */
    DR_REG_YMM1,  /**< The "ymm1" register. */
    DR_REG_YMM2,  /**< The "ymm2" register. */
    DR_REG_YMM3,  /**< The "ymm3" register. */
    DR_REG_YMM4,  /**< The "ymm4" register. */
    DR_REG_YMM5,  /**< The "ymm5" register. */
    DR_REG_YMM6,  /**< The "ymm6" register. */
    DR_REG_YMM7,  /**< The "ymm7" register. */
    DR_REG_YMM8,  /**< The "ymm8" register. */
    DR_REG_YMM9,  /**< The "ymm9" register. */
    DR_REG_YMM10, /**< The "ymm10" register. */
    DR_REG_YMM11, /**< The "ymm11" register. */
    DR_REG_YMM12, /**< The "ymm12" register. */
    DR_REG_YMM13, /**< The "ymm13" register. */
    DR_REG_YMM14, /**< The "ymm14" register. */
    DR_REG_YMM15, /**< The "ymm15" register. */
    DR_REG_YMM16, /**< The "ymm16" register. */
    DR_REG_YMM17, /**< The "ymm17" register. */
    DR_REG_YMM18, /**< The "ymm18" register. */
    DR_REG_YMM19, /**< The "ymm19" register. */
    DR_REG_YMM20, /**< The "ymm20" register. */
    DR_REG_YMM21, /**< The "ymm21" register. */
    DR_REG_YMM22, /**< The "ymm22" register. */
    DR_REG_YMM23, /**< The "ymm23" register. */
    DR_REG_YMM24, /**< The "ymm24" register. */
    DR_REG_YMM25, /**< The "ymm25" register. */
    DR_REG_YMM26, /**< The "ymm26" register. */
    DR_REG_YMM27, /**< The "ymm27" register. */
    DR_REG_YMM28, /**< The "ymm28" register. */
    DR_REG_YMM29, /**< The "ymm29" register. */
    DR_REG_YMM30, /**< The "ymm30" register. */
    DR_REG_YMM31, /**< The "ymm31" register. */
    /* 32 enums are reserved for future Intel SIMD extensions. */
    RESERVED_YMM = DR_REG_YMM31 + 32,
    /* 512-BIT ZMM */
    DR_REG_ZMM0,  /**< The "zmm0" register. */
    DR_REG_ZMM1,  /**< The "zmm1" register. */
    DR_REG_ZMM2,  /**< The "zmm2" register. */
    DR_REG_ZMM3,  /**< The "zmm3" register. */
    DR_REG_ZMM4,  /**< The "zmm4" register. */
    DR_REG_ZMM5,  /**< The "zmm5" register. */
    DR_REG_ZMM6,  /**< The "zmm6" register. */
    DR_REG_ZMM7,  /**< The "zmm7" register. */
    DR_REG_ZMM8,  /**< The "zmm8" register. */
    DR_REG_ZMM9,  /**< The "zmm9" register. */
    DR_REG_ZMM10, /**< The "zmm10" register. */
    DR_REG_ZMM11, /**< The "zmm11" register. */
    DR_REG_ZMM12, /**< The "zmm12" register. */
    DR_REG_ZMM13, /**< The "zmm13" register. */
    DR_REG_ZMM14, /**< The "zmm14" register. */
    DR_REG_ZMM15, /**< The "zmm15" register. */
    DR_REG_ZMM16, /**< The "zmm16" register. */
    DR_REG_ZMM17, /**< The "zmm17" register. */
    DR_REG_ZMM18, /**< The "zmm18" register. */
    DR_REG_ZMM19, /**< The "zmm19" register. */
    DR_REG_ZMM20, /**< The "zmm20" register. */
    DR_REG_ZMM21, /**< The "zmm21" register. */
    DR_REG_ZMM22, /**< The "zmm22" register. */
    DR_REG_ZMM23, /**< The "zmm23" register. */
    DR_REG_ZMM24, /**< The "zmm24" register. */
    DR_REG_ZMM25, /**< The "zmm25" register. */
    DR_REG_ZMM26, /**< The "zmm26" register. */
    DR_REG_ZMM27, /**< The "zmm27" register. */
    DR_REG_ZMM28, /**< The "zmm28" register. */
    DR_REG_ZMM29, /**< The "zmm29" register. */
    DR_REG_ZMM30, /**< The "zmm30" register. */
    DR_REG_ZMM31, /**< The "zmm31" register. */
    /* 32 enums are reserved for future Intel SIMD extensions. */
    RESERVED_ZMM = DR_REG_ZMM31 + 32,
    /* opmask registers */
    DR_REG_K0, /**< The "k0" register. */
    DR_REG_K1, /**< The "k1" register. */
    DR_REG_K2, /**< The "k2" register. */
    DR_REG_K3, /**< The "k3" register. */
    DR_REG_K4, /**< The "k4" register. */
    DR_REG_K5, /**< The "k5" register. */
    DR_REG_K6, /**< The "k6" register. */
    DR_REG_K7, /**< The "k7" register. */
    /* 8 enums are reserved for future Intel SIMD mask extensions. */
    RESERVED_OPMASK = DR_REG_K7 + 8,
    /* Bounds registers for MPX. */
    DR_REG_BND0, /**< The "bnd0" register. */
    DR_REG_BND1, /**< The "bnd1" register. */
    DR_REG_BND2, /**< The "bnd2" register. */
    DR_REG_BND3, /**< The "bnd3" register. */

/****************************************************************************/
#elif defined(AARCHXX)
    DR_REG_INVALID, /**< Sentinel value indicating an invalid register. */

#    ifdef AARCH64
    /* 64-bit general purpose */
    DR_REG_X0,  /**< The "x0" register. */
    DR_REG_X1,  /**< The "x1" register. */
    DR_REG_X2,  /**< The "x2" register. */
    DR_REG_X3,  /**< The "x3" register. */
    DR_REG_X4,  /**< The "x4" register. */
    DR_REG_X5,  /**< The "x5" register. */
    DR_REG_X6,  /**< The "x6" register. */
    DR_REG_X7,  /**< The "x7" register. */
    DR_REG_X8,  /**< The "x8" register. */
    DR_REG_X9,  /**< The "x9" register. */
    DR_REG_X10, /**< The "x10" register. */
    DR_REG_X11, /**< The "x11" register. */
    DR_REG_X12, /**< The "x12" register. */
    DR_REG_X13, /**< The "x13" register. */
    DR_REG_X14, /**< The "x14" register. */
    DR_REG_X15, /**< The "x15" register. */
    DR_REG_X16, /**< The "x16" register. */
    DR_REG_X17, /**< The "x17" register. */
    DR_REG_X18, /**< The "x18" register. */
    DR_REG_X19, /**< The "x19" register. */
    DR_REG_X20, /**< The "x20" register. */
    DR_REG_X21, /**< The "x21" register. */
    DR_REG_X22, /**< The "x22" register. */
    DR_REG_X23, /**< The "x23" register. */
    DR_REG_X24, /**< The "x24" register. */
    DR_REG_X25, /**< The "x25" register. */
    DR_REG_X26, /**< The "x26" register. */
    DR_REG_X27, /**< The "x27" register. */
    DR_REG_X28, /**< The "x28" register. */
    DR_REG_X29, /**< The "x29" register. */
    DR_REG_X30, /**< The "x30" register. */
    DR_REG_XSP, /**< The "xsp" stack pointer register. */
    DR_REG_XZR, /**< The "xzr" zero pseudo-register; not included in GPRs. */

    /* 32-bit general purpose */
    DR_REG_W0,  /**< The "w0" register. */
    DR_REG_W1,  /**< The "w1" register. */
    DR_REG_W2,  /**< The "w2" register. */
    DR_REG_W3,  /**< The "w3" register. */
    DR_REG_W4,  /**< The "w4" register. */
    DR_REG_W5,  /**< The "w5" register. */
    DR_REG_W6,  /**< The "w6" register. */
    DR_REG_W7,  /**< The "w7" register. */
    DR_REG_W8,  /**< The "w8" register. */
    DR_REG_W9,  /**< The "w9" register. */
    DR_REG_W10, /**< The "w10" register. */
    DR_REG_W11, /**< The "w11" register. */
    DR_REG_W12, /**< The "w12" register. */
    DR_REG_W13, /**< The "w13" register. */
    DR_REG_W14, /**< The "w14" register. */
    DR_REG_W15, /**< The "w15" register. */
    DR_REG_W16, /**< The "w16" register. */
    DR_REG_W17, /**< The "w17" register. */
    DR_REG_W18, /**< The "w18" register. */
    DR_REG_W19, /**< The "w19" register. */
    DR_REG_W20, /**< The "w20" register. */
    DR_REG_W21, /**< The "w21" register. */
    DR_REG_W22, /**< The "w22" register. */
    DR_REG_W23, /**< The "w23" register. */
    DR_REG_W24, /**< The "w24" register. */
    DR_REG_W25, /**< The "w25" register. */
    DR_REG_W26, /**< The "w26" register. */
    DR_REG_W27, /**< The "w27" register. */
    DR_REG_W28, /**< The "w28" register. */
    DR_REG_W29, /**< The "w29" register. */
    DR_REG_W30, /**< The "w30" register. */
    DR_REG_WSP, /**< The "wsp" bottom half of the stack pointer register. */
    DR_REG_WZR, /**< The "wzr" zero pseudo-register. */
#    else
    /* 32-bit general purpose */
    DR_REG_R0,  /**< The "r0" register. */
    DR_REG_R1,  /**< The "r1" register. */
    DR_REG_R2,  /**< The "r2" register. */
    DR_REG_R3,  /**< The "r3" register. */
    DR_REG_R4,  /**< The "r4" register. */
    DR_REG_R5,  /**< The "r5" register. */
    DR_REG_R6,  /**< The "r6" register. */
    DR_REG_R7,  /**< The "r7" register. */
    DR_REG_R8,  /**< The "r8" register. */
    DR_REG_R9,  /**< The "r9" register. */
    DR_REG_R10, /**< The "r10" register. */
    DR_REG_R11, /**< The "r11" register. */
    DR_REG_R12, /**< The "r12" register. */
    DR_REG_R13, /**< The "r13" register. */
    DR_REG_R14, /**< The "r14" register. */
    DR_REG_R15, /**< The "r15" register. */
#    endif

#    ifdef AARCH64
    /* SVE vector registers */
    DR_REG_Z0,  /**< The "z0" register. */
    DR_REG_Z1,  /**< The "z1" register. */
    DR_REG_Z2,  /**< The "z2" register. */
    DR_REG_Z3,  /**< The "z3" register. */
    DR_REG_Z4,  /**< The "z4" register. */
    DR_REG_Z5,  /**< The "z5" register. */
    DR_REG_Z6,  /**< The "z6" register. */
    DR_REG_Z7,  /**< The "z7" register. */
    DR_REG_Z8,  /**< The "z8" register. */
    DR_REG_Z9,  /**< The "z9" register. */
    DR_REG_Z10, /**< The "z10" register. */
    DR_REG_Z11, /**< The "z11" register. */
    DR_REG_Z12, /**< The "z12" register. */
    DR_REG_Z13, /**< The "z13" register. */
    DR_REG_Z14, /**< The "z14" register. */
    DR_REG_Z15, /**< The "z15" register. */
    DR_REG_Z16, /**< The "z16" register. */
    DR_REG_Z17, /**< The "z17" register. */
    DR_REG_Z18, /**< The "z18" register. */
    DR_REG_Z19, /**< The "z19" register. */
    DR_REG_Z20, /**< The "z20" register. */
    DR_REG_Z21, /**< The "z21" register. */
    DR_REG_Z22, /**< The "z22" register. */
    DR_REG_Z23, /**< The "z23" register. */
    DR_REG_Z24, /**< The "z24" register. */
    DR_REG_Z25, /**< The "z25" register. */
    DR_REG_Z26, /**< The "z26" register. */
    DR_REG_Z27, /**< The "z27" register. */
    DR_REG_Z28, /**< The "z28" register. */
    DR_REG_Z29, /**< The "z29" register. */
    DR_REG_Z30, /**< The "z30" register. */
    DR_REG_Z31, /**< The "z31" register. */
#    endif

/* All registers that can be used in address operands must be before this point.
 *
 * Base+disp operands do not store the full reg_id_t value, only the lower
 * REG_SPECIFIER_BITS, so any register used in addressing must be less than
 * 1 << REG_SPECIFIER_BITS. This is checked in d_r_arch_init().
 */
#    if defined(AARCH64)
    DR_REG_MAX_ADDRESSING_REG = DR_REG_Z31,
#    else
    DR_REG_MAX_ADDRESSING_REG = DR_REG_R15,
#    endif

    /* 128-bit SIMD registers */
    DR_REG_Q0,  /**< The "q0" register. */
    DR_REG_Q1,  /**< The "q1" register. */
    DR_REG_Q2,  /**< The "q2" register. */
    DR_REG_Q3,  /**< The "q3" register. */
    DR_REG_Q4,  /**< The "q4" register. */
    DR_REG_Q5,  /**< The "q5" register. */
    DR_REG_Q6,  /**< The "q6" register. */
    DR_REG_Q7,  /**< The "q7" register. */
    DR_REG_Q8,  /**< The "q8" register. */
    DR_REG_Q9,  /**< The "q9" register. */
    DR_REG_Q10, /**< The "q10" register. */
    DR_REG_Q11, /**< The "q11" register. */
    DR_REG_Q12, /**< The "q12" register. */
    DR_REG_Q13, /**< The "q13" register. */
    DR_REG_Q14, /**< The "q14" register. */
    DR_REG_Q15, /**< The "q15" register. */
    /* x64-only but simpler code to not ifdef it */
    DR_REG_Q16, /**< The "q16" register. */
    DR_REG_Q17, /**< The "q17" register. */
    DR_REG_Q18, /**< The "q18" register. */
    DR_REG_Q19, /**< The "q19" register. */
    DR_REG_Q20, /**< The "q20" register. */
    DR_REG_Q21, /**< The "q21" register. */
    DR_REG_Q22, /**< The "q22" register. */
    DR_REG_Q23, /**< The "q23" register. */
    DR_REG_Q24, /**< The "q24" register. */
    DR_REG_Q25, /**< The "q25" register. */
    DR_REG_Q26, /**< The "q26" register. */
    DR_REG_Q27, /**< The "q27" register. */
    DR_REG_Q28, /**< The "q28" register. */
    DR_REG_Q29, /**< The "q29" register. */
    DR_REG_Q30, /**< The "q30" register. */
    DR_REG_Q31, /**< The "q31" register. */
    /* 64-bit SIMD registers */
    DR_REG_D0,  /**< The "d0" register. */
    DR_REG_D1,  /**< The "d1" register. */
    DR_REG_D2,  /**< The "d2" register. */
    DR_REG_D3,  /**< The "d3" register. */
    DR_REG_D4,  /**< The "d4" register. */
    DR_REG_D5,  /**< The "d5" register. */
    DR_REG_D6,  /**< The "d6" register. */
    DR_REG_D7,  /**< The "d7" register. */
    DR_REG_D8,  /**< The "d8" register. */
    DR_REG_D9,  /**< The "d9" register. */
    DR_REG_D10, /**< The "d10" register. */
    DR_REG_D11, /**< The "d11" register. */
    DR_REG_D12, /**< The "d12" register. */
    DR_REG_D13, /**< The "d13" register. */
    DR_REG_D14, /**< The "d14" register. */
    DR_REG_D15, /**< The "d15" register. */
    DR_REG_D16, /**< The "d16" register. */
    DR_REG_D17, /**< The "d17" register. */
    DR_REG_D18, /**< The "d18" register. */
    DR_REG_D19, /**< The "d19" register. */
    DR_REG_D20, /**< The "d20" register. */
    DR_REG_D21, /**< The "d21" register. */
    DR_REG_D22, /**< The "d22" register. */
    DR_REG_D23, /**< The "d23" register. */
    DR_REG_D24, /**< The "d24" register. */
    DR_REG_D25, /**< The "d25" register. */
    DR_REG_D26, /**< The "d26" register. */
    DR_REG_D27, /**< The "d27" register. */
    DR_REG_D28, /**< The "d28" register. */
    DR_REG_D29, /**< The "d29" register. */
    DR_REG_D30, /**< The "d30" register. */
    DR_REG_D31, /**< The "d31" register. */
    /* 32-bit SIMD registers */
    DR_REG_S0,  /**< The "s0" register. */
    DR_REG_S1,  /**< The "s1" register. */
    DR_REG_S2,  /**< The "s2" register. */
    DR_REG_S3,  /**< The "s3" register. */
    DR_REG_S4,  /**< The "s4" register. */
    DR_REG_S5,  /**< The "s5" register. */
    DR_REG_S6,  /**< The "s6" register. */
    DR_REG_S7,  /**< The "s7" register. */
    DR_REG_S8,  /**< The "s8" register. */
    DR_REG_S9,  /**< The "s9" register. */
    DR_REG_S10, /**< The "s10" register. */
    DR_REG_S11, /**< The "s11" register. */
    DR_REG_S12, /**< The "s12" register. */
    DR_REG_S13, /**< The "s13" register. */
    DR_REG_S14, /**< The "s14" register. */
    DR_REG_S15, /**< The "s15" register. */
    DR_REG_S16, /**< The "s16" register. */
    DR_REG_S17, /**< The "s17" register. */
    DR_REG_S18, /**< The "s18" register. */
    DR_REG_S19, /**< The "s19" register. */
    DR_REG_S20, /**< The "s20" register. */
    DR_REG_S21, /**< The "s21" register. */
    DR_REG_S22, /**< The "s22" register. */
    DR_REG_S23, /**< The "s23" register. */
    DR_REG_S24, /**< The "s24" register. */
    DR_REG_S25, /**< The "s25" register. */
    DR_REG_S26, /**< The "s26" register. */
    DR_REG_S27, /**< The "s27" register. */
    DR_REG_S28, /**< The "s28" register. */
    DR_REG_S29, /**< The "s29" register. */
    DR_REG_S30, /**< The "s30" register. */
    DR_REG_S31, /**< The "s31" register. */
    /* 16-bit SIMD registers */
    DR_REG_H0,  /**< The "h0" register. */
    DR_REG_H1,  /**< The "h1" register. */
    DR_REG_H2,  /**< The "h2" register. */
    DR_REG_H3,  /**< The "h3" register. */
    DR_REG_H4,  /**< The "h4" register. */
    DR_REG_H5,  /**< The "h5" register. */
    DR_REG_H6,  /**< The "h6" register. */
    DR_REG_H7,  /**< The "h7" register. */
    DR_REG_H8,  /**< The "h8" register. */
    DR_REG_H9,  /**< The "h9" register. */
    DR_REG_H10, /**< The "h10" register. */
    DR_REG_H11, /**< The "h11" register. */
    DR_REG_H12, /**< The "h12" register. */
    DR_REG_H13, /**< The "h13" register. */
    DR_REG_H14, /**< The "h14" register. */
    DR_REG_H15, /**< The "h15" register. */
    DR_REG_H16, /**< The "h16" register. */
    DR_REG_H17, /**< The "h17" register. */
    DR_REG_H18, /**< The "h18" register. */
    DR_REG_H19, /**< The "h19" register. */
    DR_REG_H20, /**< The "h20" register. */
    DR_REG_H21, /**< The "h21" register. */
    DR_REG_H22, /**< The "h22" register. */
    DR_REG_H23, /**< The "h23" register. */
    DR_REG_H24, /**< The "h24" register. */
    DR_REG_H25, /**< The "h25" register. */
    DR_REG_H26, /**< The "h26" register. */
    DR_REG_H27, /**< The "h27" register. */
    DR_REG_H28, /**< The "h28" register. */
    DR_REG_H29, /**< The "h29" register. */
    DR_REG_H30, /**< The "h30" register. */
    DR_REG_H31, /**< The "h31" register. */
    /* 8-bit SIMD registers */
    DR_REG_B0,  /**< The "b0" register. */
    DR_REG_B1,  /**< The "b1" register. */
    DR_REG_B2,  /**< The "b2" register. */
    DR_REG_B3,  /**< The "b3" register. */
    DR_REG_B4,  /**< The "b4" register. */
    DR_REG_B5,  /**< The "b5" register. */
    DR_REG_B6,  /**< The "b6" register. */
    DR_REG_B7,  /**< The "b7" register. */
    DR_REG_B8,  /**< The "b8" register. */
    DR_REG_B9,  /**< The "b9" register. */
    DR_REG_B10, /**< The "b10" register. */
    DR_REG_B11, /**< The "b11" register. */
    DR_REG_B12, /**< The "b12" register. */
    DR_REG_B13, /**< The "b13" register. */
    DR_REG_B14, /**< The "b14" register. */
    DR_REG_B15, /**< The "b15" register. */
    DR_REG_B16, /**< The "b16" register. */
    DR_REG_B17, /**< The "b17" register. */
    DR_REG_B18, /**< The "b18" register. */
    DR_REG_B19, /**< The "b19" register. */
    DR_REG_B20, /**< The "b20" register. */
    DR_REG_B21, /**< The "b21" register. */
    DR_REG_B22, /**< The "b22" register. */
    DR_REG_B23, /**< The "b23" register. */
    DR_REG_B24, /**< The "b24" register. */
    DR_REG_B25, /**< The "b25" register. */
    DR_REG_B26, /**< The "b26" register. */
    DR_REG_B27, /**< The "b27" register. */
    DR_REG_B28, /**< The "b28" register. */
    DR_REG_B29, /**< The "b29" register. */
    DR_REG_B30, /**< The "b30" register. */
    DR_REG_B31, /**< The "b31" register. */

#    ifndef AARCH64
    /* Coprocessor registers */
    DR_REG_CR0,  /**< The "cr0" register. */
    DR_REG_CR1,  /**< The "cr1" register. */
    DR_REG_CR2,  /**< The "cr2" register. */
    DR_REG_CR3,  /**< The "cr3" register. */
    DR_REG_CR4,  /**< The "cr4" register. */
    DR_REG_CR5,  /**< The "cr5" register. */
    DR_REG_CR6,  /**< The "cr6" register. */
    DR_REG_CR7,  /**< The "cr7" register. */
    DR_REG_CR8,  /**< The "cr8" register. */
    DR_REG_CR9,  /**< The "cr9" register. */
    DR_REG_CR10, /**< The "cr10" register. */
    DR_REG_CR11, /**< The "cr11" register. */
    DR_REG_CR12, /**< The "cr12" register. */
    DR_REG_CR13, /**< The "cr13" register. */
    DR_REG_CR14, /**< The "cr14" register. */
    DR_REG_CR15, /**< The "cr15" register. */
#    endif

/* We decided against DR_REG_RN_TH (top half), DR_REG_RN_BH (bottom half
 * for 32-bit as we have the W versions for 64-bit), and DR_REG_RN_BB
 * (bottom byte) as they are not available in the ISA and which portion
 * of a GPR is selected purely by the opcode.  Our decoder will create
 * a partial register for these to help tools, but it won't specify which
 * part of the register.
 */

/* Though on x86 we don't list eflags for even things like popf that write
 * other bits beyond aflags, here we do explicitly list cpsr and spsr for
 * OP_mrs and OP_msr to distinguish them and make things clearer.
 */
#    ifdef AARCH64
    DR_REG_NZCV,            /**< The "nzcv" register. */
    DR_REG_FPCR,            /**< The "fpcr" register. */
    DR_REG_FPSR,            /**< The "fpsr" register. */
    DR_REG_MDCCSR_EL0,      /**< The "mdccsr_el0" register. */
    DR_REG_DBGDTR_EL0,      /**< The "dbgdtr_el0" register. */
    DR_REG_DBGDTRRX_EL0,    /**< The "dbgdtrrx_el0" register. */
    DR_REG_SP_EL0,          /**< The "sp_el0" register. */
    DR_REG_SPSEL,           /**< The "spsel" register. */
    DR_REG_DAIFSET,         /**< The "DAIFSet" register. */
    DR_REG_DAIFCLR,         /**< The "DAIFClr" register. */
    DR_REG_CURRENTEL,       /**< The "currentel" register. */
    DR_REG_PAN,             /**< The "pan" register. */
    DR_REG_UAO,             /**< The "uao" register. */
    DR_REG_CTR_EL0,         /**< The "ctr_el0" register. */
    DR_REG_DCZID_EL0,       /**< The "dczid_el0" register. */
    DR_REG_RNDR,            /**< The "rndr" register. */
    DR_REG_RNDRRS,          /**< The "rndrrs" register. */
    DR_REG_DAIF,            /**< The "daif" register. */
    DR_REG_DIT,             /**< The "dit" register. */
    DR_REG_SSBS,            /**< The "ssbs" register. */
    DR_REG_TCO,             /**< The "tco" register. */
    DR_REG_DSPSR_EL0,       /**< The "dspsr_el0" register. */
    DR_REG_DLR_EL0,         /**< The "dlr_el0" register. */
    DR_REG_PMCR_EL0,        /**< The "pmcr_el0" register. */
    DR_REG_PMCNTENSET_EL0,  /**< The "pmcntenset_el0" register. */
    DR_REG_PMCNTENCLR_EL0,  /**< The "pmcntenclr_el0" register. */
    DR_REG_PMOVSCLR_EL0,    /**< The "pmovsclr_el0" register. */
    DR_REG_PMSWINC_EL0,     /**< The "pmswinc_el0" register. */
    DR_REG_PMSELR_EL0,      /**< The "pmselr_el0" register. */
    DR_REG_PMCEID0_EL0,     /**< The "pmceid0_el0" register. */
    DR_REG_PMCEID1_EL0,     /**< The "pmceid1_el0" register. */
    DR_REG_PMCCNTR_EL0,     /**< The "pmccntr_el0" register. */
    DR_REG_PMXEVTYPER_EL0,  /**< The "pmxevtyper_el0" register. */
    DR_REG_PMXEVCNTR_EL0,   /**< The "pmxevcntr_el0" register. */
    DR_REG_PMUSERENR_EL0,   /**< The "pmuserenr_el0" register. */
    DR_REG_PMOVSSET_EL0,    /**< The "pmovsset_el0" register. */
    DR_REG_SCXTNUM_EL0,     /**< The "scxtnum_el0" register. */
    DR_REG_CNTFRQ_EL0,      /**< The "cntfrq_el0" register. */
    DR_REG_CNTPCT_EL0,      /**< The "cntpct_el0" register. */
    DR_REG_CNTP_TVAL_EL0,   /**< The "cntp_tval_el0" register. */
    DR_REG_CNTP_CTL_EL0,    /**< The "cntp_ctl_el0" register. */
    DR_REG_CNTP_CVAL_EL0,   /**< The "cntp_cval_el0" register. */
    DR_REG_CNTV_TVAL_EL0,   /**< The "cntv_tval_el0" register. */
    DR_REG_CNTV_CTL_EL0,    /**< The "cntv_ctl_el0" register. */
    DR_REG_CNTV_CVAL_EL0,   /**< The "cntv_cval_el0" register. */
    DR_REG_PMEVCNTR0_EL0,   /**< The "pmevcntr0_el0" register. */
    DR_REG_PMEVCNTR1_EL0,   /**< The "pmevcntr1_el0" register. */
    DR_REG_PMEVCNTR2_EL0,   /**< The "pmevcntr2_el0" register. */
    DR_REG_PMEVCNTR3_EL0,   /**< The "pmevcntr3_el0" register. */
    DR_REG_PMEVCNTR4_EL0,   /**< The "pmevcntr4_el0" register. */
    DR_REG_PMEVCNTR5_EL0,   /**< The "pmevcntr5_el0" register. */
    DR_REG_PMEVCNTR6_EL0,   /**< The "pmevcntr6_el0" register. */
    DR_REG_PMEVCNTR7_EL0,   /**< The "pmevcntr7_el0" register. */
    DR_REG_PMEVCNTR8_EL0,   /**< The "pmevcntr8_el0" register. */
    DR_REG_PMEVCNTR9_EL0,   /**< The "pmevcntr9_el0" register. */
    DR_REG_PMEVCNTR10_EL0,  /**< The "pmevcntr10_el0" register. */
    DR_REG_PMEVCNTR11_EL0,  /**< The "pmevcntr11_el0" register. */
    DR_REG_PMEVCNTR12_EL0,  /**< The "pmevcntr12_el0" register. */
    DR_REG_PMEVCNTR13_EL0,  /**< The "pmevcntr13_el0" register. */
    DR_REG_PMEVCNTR14_EL0,  /**< The "pmevcntr14_el0" register. */
    DR_REG_PMEVCNTR15_EL0,  /**< The "pmevcntr15_el0" register. */
    DR_REG_PMEVCNTR16_EL0,  /**< The "pmevcntr16_el0" register. */
    DR_REG_PMEVCNTR17_EL0,  /**< The "pmevcntr17_el0" register. */
    DR_REG_PMEVCNTR18_EL0,  /**< The "pmevcntr18_el0" register. */
    DR_REG_PMEVCNTR19_EL0,  /**< The "pmevcntr19_el0" register. */
    DR_REG_PMEVCNTR20_EL0,  /**< The "pmevcntr20_el0" register. */
    DR_REG_PMEVCNTR21_EL0,  /**< The "pmevcntr21_el0" register. */
    DR_REG_PMEVCNTR22_EL0,  /**< The "pmevcntr22_el0" register. */
    DR_REG_PMEVCNTR23_EL0,  /**< The "pmevcntr23_el0" register. */
    DR_REG_PMEVCNTR24_EL0,  /**< The "pmevcntr24_el0" register. */
    DR_REG_PMEVCNTR25_EL0,  /**< The "pmevcntr25_el0" register. */
    DR_REG_PMEVCNTR26_EL0,  /**< The "pmevcntr26_el0" register. */
    DR_REG_PMEVCNTR27_EL0,  /**< The "pmevcntr27_el0" register. */
    DR_REG_PMEVCNTR28_EL0,  /**< The "pmevcntr28_el0" register. */
    DR_REG_PMEVCNTR29_EL0,  /**< The "pmevcntr29_el0" register. */
    DR_REG_PMEVCNTR30_EL0,  /**< The "pmevcntr30_el0" register. */
    DR_REG_PMEVTYPER0_EL0,  /**< The "pmevtyper0_el0" register. */
    DR_REG_PMEVTYPER1_EL0,  /**< The "pmevtyper1_el0" register. */
    DR_REG_PMEVTYPER2_EL0,  /**< The "pmevtyper2_el0" register. */
    DR_REG_PMEVTYPER3_EL0,  /**< The "pmevtyper3_el0" register. */
    DR_REG_PMEVTYPER4_EL0,  /**< The "pmevtyper4_el0" register. */
    DR_REG_PMEVTYPER5_EL0,  /**< The "pmevtyper5_el0" register. */
    DR_REG_PMEVTYPER6_EL0,  /**< The "pmevtyper6_el0" register. */
    DR_REG_PMEVTYPER7_EL0,  /**< The "pmevtyper7_el0" register. */
    DR_REG_PMEVTYPER8_EL0,  /**< The "pmevtyper8_el0" register. */
    DR_REG_PMEVTYPER9_EL0,  /**< The "pmevtyper9_el0" register. */
    DR_REG_PMEVTYPER10_EL0, /**< The "pmevtyper10_el0" register. */
    DR_REG_PMEVTYPER11_EL0, /**< The "pmevtyper11_el0" register. */
    DR_REG_PMEVTYPER12_EL0, /**< The "pmevtyper12_el0" register. */
    DR_REG_PMEVTYPER13_EL0, /**< The "pmevtyper13_el0" register. */
    DR_REG_PMEVTYPER14_EL0, /**< The "pmevtyper14_el0" register. */
    DR_REG_PMEVTYPER15_EL0, /**< The "pmevtyper15_el0" register. */
    DR_REG_PMEVTYPER16_EL0, /**< The "pmevtyper16_el0" register. */
    DR_REG_PMEVTYPER17_EL0, /**< The "pmevtyper17_el0" register. */
    DR_REG_PMEVTYPER18_EL0, /**< The "pmevtyper18_el0" register. */
    DR_REG_PMEVTYPER19_EL0, /**< The "pmevtyper19_el0" register. */
    DR_REG_PMEVTYPER20_EL0, /**< The "pmevtyper20_el0" register. */
    DR_REG_PMEVTYPER21_EL0, /**< The "pmevtyper21_el0" register. */
    DR_REG_PMEVTYPER22_EL0, /**< The "pmevtyper22_el0" register. */
    DR_REG_PMEVTYPER23_EL0, /**< The "pmevtyper23_el0" register. */
    DR_REG_PMEVTYPER24_EL0, /**< The "pmevtyper24_el0" register. */
    DR_REG_PMEVTYPER25_EL0, /**< The "pmevtyper25_el0" register. */
    DR_REG_PMEVTYPER26_EL0, /**< The "pmevtyper26_el0" register. */
    DR_REG_PMEVTYPER27_EL0, /**< The "pmevtyper27_el0" register. */
    DR_REG_PMEVTYPER28_EL0, /**< The "pmevtyper28_el0" register. */
    DR_REG_PMEVTYPER29_EL0, /**< The "pmevtyper29_el0" register. */
    DR_REG_PMEVTYPER30_EL0, /**< The "pmevtyper30_el0" register. */
    DR_REG_PMCCFILTR_EL0,   /**< The "pmccfiltr_el0" register. */
    DR_REG_SPSR_IRQ,        /**< The "spsr_irq" register. */
    DR_REG_SPSR_ABT,        /**< The "spsr_abt" register. */
    DR_REG_SPSR_UND,        /**< The "spsr_und" register. */
    DR_REG_SPSR_FIQ,        /**< The "spsr_fiq" register. */
#    else
    DR_REG_CPSR,                              /**< The "cpsr" register. */
    DR_REG_SPSR,                              /**< The "spsr" register. */
    DR_REG_FPSCR,                             /**< The "fpscr" register. */
#    endif

    /* AArch32 Thread Registers */
    DR_REG_TPIDRURW, /**< User Read/Write Thread ID Register */
    DR_REG_TPIDRURO, /**< User Read-Only Thread ID Register */

#    ifdef AARCH64
    /* SVE predicate registers */
    DR_REG_P0,  /**< The "p0" register. */
    DR_REG_P1,  /**< The "p1" register. */
    DR_REG_P2,  /**< The "p2" register. */
    DR_REG_P3,  /**< The "p3" register. */
    DR_REG_P4,  /**< The "p4" register. */
    DR_REG_P5,  /**< The "p5" register. */
    DR_REG_P6,  /**< The "p6" register. */
    DR_REG_P7,  /**< The "p7" register. */
    DR_REG_P8,  /**< The "p8" register. */
    DR_REG_P9,  /**< The "p9" register. */
    DR_REG_P10, /**< The "p10" register. */
    DR_REG_P11, /**< The "p11" register. */
    DR_REG_P12, /**< The "p12" register. */
    DR_REG_P13, /**< The "p13" register. */
    DR_REG_P14, /**< The "p14" register. */
    DR_REG_P15, /**< The "p15" register. */

    DR_REG_FFR, /**< The SVE First-Fault Register. */
#    endif

#    ifdef AARCH64
    /* AArch64 Counter/Timer Register(s) */
    DR_REG_CNTVCT_EL0, /**< Virtual Timer Count Register, EL0. */
#    endif

/* Aliases below here: */
#    ifdef AARCH64
    DR_REG_R0 = DR_REG_X0,   /**< Alias for the x0 register. */
    DR_REG_R1 = DR_REG_X1,   /**< Alias for the x1 register. */
    DR_REG_R2 = DR_REG_X2,   /**< Alias for the x2 register. */
    DR_REG_R3 = DR_REG_X3,   /**< Alias for the x3 register. */
    DR_REG_R4 = DR_REG_X4,   /**< Alias for the x4 register. */
    DR_REG_R5 = DR_REG_X5,   /**< Alias for the x5 register. */
    DR_REG_R6 = DR_REG_X6,   /**< Alias for the x6 register. */
    DR_REG_R7 = DR_REG_X7,   /**< Alias for the x7 register. */
    DR_REG_R8 = DR_REG_X8,   /**< Alias for the x8 register. */
    DR_REG_R9 = DR_REG_X9,   /**< Alias for the x9 register. */
    DR_REG_R10 = DR_REG_X10, /**< Alias for the x10 register. */
    DR_REG_R11 = DR_REG_X11, /**< Alias for the x11 register. */
    DR_REG_R12 = DR_REG_X12, /**< Alias for the x12 register. */
    DR_REG_R13 = DR_REG_X13, /**< Alias for the x13 register. */
    DR_REG_R14 = DR_REG_X14, /**< Alias for the x14 register. */
    DR_REG_R15 = DR_REG_X15, /**< Alias for the x15 register. */
    DR_REG_R16 = DR_REG_X16, /**< Alias for the x16 register. */
    DR_REG_R17 = DR_REG_X17, /**< Alias for the x17 register. */
    DR_REG_R18 = DR_REG_X18, /**< Alias for the x18 register. */
    DR_REG_R19 = DR_REG_X19, /**< Alias for the x19 register. */
    DR_REG_R20 = DR_REG_X20, /**< Alias for the x20 register. */
    DR_REG_R21 = DR_REG_X21, /**< Alias for the x21 register. */
    DR_REG_R22 = DR_REG_X22, /**< Alias for the x22 register. */
    DR_REG_R23 = DR_REG_X23, /**< Alias for the x23 register. */
    DR_REG_R24 = DR_REG_X24, /**< Alias for the x24 register. */
    DR_REG_R25 = DR_REG_X25, /**< Alias for the x25 register. */
    DR_REG_R26 = DR_REG_X26, /**< Alias for the x26 register. */
    DR_REG_R27 = DR_REG_X27, /**< Alias for the x27 register. */
    DR_REG_R28 = DR_REG_X28, /**< Alias for the x28 register. */
    DR_REG_R29 = DR_REG_X29, /**< Alias for the x29 register. */
    DR_REG_R30 = DR_REG_X30, /**< Alias for the x30 register. */
    DR_REG_SP = DR_REG_XSP,  /**< The stack pointer register. */
    DR_REG_LR = DR_REG_X30,  /**< The link register. */
#    else
    DR_REG_SP = DR_REG_R13,                   /**< The stack pointer register. */
    DR_REG_LR = DR_REG_R14,                   /**< The link register. */
    DR_REG_PC = DR_REG_R15,                   /**< The program counter register. */
#    endif
    DR_REG_SL = DR_REG_R10,  /**< Alias for the r10 register. */
    DR_REG_FP = DR_REG_R11,  /**< Alias for the r11 register. */
    DR_REG_IP = DR_REG_R12,  /**< Alias for the r12 register. */
#    ifndef AARCH64
    /** Alias for cpsr register (thus this is the full cpsr, not just the apsr bits). */
    DR_REG_APSR = DR_REG_CPSR,
#    endif

    /* AArch64 Thread Registers */
    /** Thread Pointer/ID Register, EL0. */
    DR_REG_TPIDR_EL0 = DR_REG_TPIDRURW,
    /** Thread Pointer/ID Register, Read-Only, EL0. */
    DR_REG_TPIDRRO_EL0 = DR_REG_TPIDRURO,
    /* ARMv7 Thread Registers */
    DR_REG_CP15_C13_2 = DR_REG_TPIDRURW,        /**< User Read/Write Thread ID Register */
    DR_REG_CP15_C13_3 = DR_REG_TPIDRURO,        /**< User Read-Only Thread ID Register */

#    ifdef AARCH64
    DR_REG_LAST_VALID_ENUM = DR_REG_CNTVCT_EL0, /**< Last valid register enum */
    DR_REG_LAST_ENUM = DR_REG_CNTVCT_EL0,       /**< Last value of register enums */
#    else
    DR_REG_LAST_VALID_ENUM = DR_REG_TPIDRURO, /**< Last valid register enum */
    DR_REG_LAST_ENUM = DR_REG_TPIDRURO,       /**< Last value of register enums */
#    endif

#    ifdef AARCH64
    DR_REG_START_64 = DR_REG_X0,  /**< Start of 64-bit general register enum values */
    DR_REG_STOP_64 = DR_REG_XSP,  /**< End of 64-bit general register enum values */
    DR_REG_START_32 = DR_REG_W0,  /**< Start of 32-bit general register enum values */
    DR_REG_STOP_32 = DR_REG_WSP,  /**< End of 32-bit general register enum values */
    DR_REG_START_GPR = DR_REG_X0, /**< Start of full-size general-purpose registers */
    DR_REG_STOP_GPR = DR_REG_XSP, /**< End of full-size general-purpose registers */
#    else
    DR_REG_START_32 = DR_REG_R0,  /**< Start of 32-bit general register enum values */
    DR_REG_STOP_32 = DR_REG_R15,  /**< End of 32-bit general register enum values */
    DR_REG_START_GPR = DR_REG_R0, /**< Start of general register registers */
    DR_REG_STOP_GPR = DR_REG_R15, /**< End of general register registers */
#    endif

    DR_NUM_GPR_REGS = DR_REG_STOP_GPR - DR_REG_START_GPR + 1, /**< Count of GPR regs. */
#    ifdef AARCH64
    DR_NUM_SIMD_VECTOR_REGS = DR_REG_Z31 - DR_REG_Z0 + 1,     /**< Count of SIMD regs. */
#    else
    /* XXX: maybe we want more distinct names that provide counts for 64-bit D or 32-bit
     * S registers.
     */
    DR_NUM_SIMD_VECTOR_REGS = DR_REG_Q15 - DR_REG_Q0 + 1, /**< Count of SIMD regs. */
#    endif

#    ifndef AARCH64
    /** Platform-independent way to refer to stack pointer. */
    DR_REG_XSP = DR_REG_SP,
#    endif
#elif defined(RISCV64)
    DR_REG_INVALID, /**< Sentinel value indicating an invalid register. */
    DR_REG_X0,      /**< The hard-wired x0(zero) register. */
    DR_REG_X1,      /**< The x1(ra) register. */
    DR_REG_X2,      /**< The x2(sp) register. */
    DR_REG_X3,      /**< The x3(gp) register. */
    DR_REG_X4,      /**< The x4(tp) register. */
    DR_REG_X5,      /**< The x5(t0) register. */
    DR_REG_X6,      /**< The x6(t1) register. */
    DR_REG_X7,      /**< The x7(t2) register. */
    DR_REG_X8,      /**< The x8(s0/fp) register. */
    DR_REG_X9,      /**< The x9(s1) register. */
    DR_REG_X10,     /**< The x10(a0) register. */
    DR_REG_X11,     /**< The x11(a1) register. */
    DR_REG_X12,     /**< The x12(a2) register. */
    DR_REG_X13,     /**< The x13(a3) register. */
    DR_REG_X14,     /**< The x14(a4) register. */
    DR_REG_X15,     /**< The x15(a5) register. */
    DR_REG_X16,     /**< The x16(a6) register. */
    DR_REG_X17,     /**< The x17(a7) register. */
    DR_REG_X18,     /**< The x18(s2) register. */
    DR_REG_X19,     /**< The x19(s3) register. */
    DR_REG_X20,     /**< The x20(s4) register. */
    DR_REG_X21,     /**< The x21(s5) register. */
    DR_REG_X22,     /**< The x22(s6) register. */
    DR_REG_X23,     /**< The x23(s7) register. */
    DR_REG_X24,     /**< The x24(s8) register. */
    DR_REG_X25,     /**< The x25(s9) register. */
    DR_REG_X26,     /**< The x26(s10) register. */
    DR_REG_X27,     /**< The x27(s11) register. */
    DR_REG_X28,     /**< The x28(t3) register. */
    DR_REG_X29,     /**< The x29(t4) register. */
    DR_REG_X30,     /**< The x30(t5) register. */
    DR_REG_X31,     /**< The x31(t6) register. */
    /* GPR aliases */
    DR_REG_ZERO = DR_REG_X0, /**< The hard-wired zero (x0) register. */
    DR_REG_RA = DR_REG_X1,   /**< The return address (x1) register. */
    DR_REG_SP = DR_REG_X2,   /**< The stack pointer (x2) register. */
    DR_REG_GP = DR_REG_X3,   /**< The global pointer (x3) register. */
    DR_REG_TP = DR_REG_X4,   /**< The thread pointer (x4) register. */
    DR_REG_T0 = DR_REG_X5,   /**< The 1st temporary (x5) register. */
    DR_REG_T1 = DR_REG_X6,   /**< The 2nd temporary (x6) register. */
    DR_REG_T2 = DR_REG_X7,   /**< The 3rd temporary (x7) register. */
    DR_REG_S0 = DR_REG_X8,   /**< The 1st callee-saved (x8) register. */
    DR_REG_FP = DR_REG_X8,   /**< The frame pointer (x8) register. */
    DR_REG_S1 = DR_REG_X9,   /**< The 2nd callee-saved (x9) register. */
    DR_REG_A0 = DR_REG_X10,  /**< The 1st argument/return value (x10) register. */
    DR_REG_A1 = DR_REG_X11,  /**< The 2nd argument/return value (x11) register. */
    DR_REG_A2 = DR_REG_X12,  /**< The 3rd argument (x12) register. */
    DR_REG_A3 = DR_REG_X13,  /**< The 4th argument (x13) register. */
    DR_REG_A4 = DR_REG_X14,  /**< The 5th argument (x14) register. */
    DR_REG_A5 = DR_REG_X15,  /**< The 6th argument (x15) register. */
    DR_REG_A6 = DR_REG_X16,  /**< The 7th argument (x16) register. */
    DR_REG_A7 = DR_REG_X17,  /**< The 8th argument (x17) register. */
    DR_REG_S2 = DR_REG_X18,  /**< The 3rd callee-saved (x18) register. */
    DR_REG_S3 = DR_REG_X19,  /**< The 4th callee-saved (x19) register. */
    DR_REG_S4 = DR_REG_X20,  /**< The 5th callee-saved (x20) register. */
    DR_REG_S5 = DR_REG_X21,  /**< The 6th callee-saved (x21) register. */
    DR_REG_S6 = DR_REG_X22,  /**< The 7th callee-saved (x22) register. */
    DR_REG_S7 = DR_REG_X23,  /**< The 8th callee-saved (x23) register. */
    DR_REG_S8 = DR_REG_X24,  /**< The 9th callee-saved (x24) register. */
    DR_REG_S9 = DR_REG_X25,  /**< The 10th callee-saved (x25) register. */
    DR_REG_S10 = DR_REG_X26, /**< The 11th callee-saved (x26) register. */
    DR_REG_S11 = DR_REG_X27, /**< The 12th callee-saved (x27) register. */
    DR_REG_T3 = DR_REG_X28,  /**< The 4th temporary (x28) register. */
    DR_REG_T4 = DR_REG_X29,  /**< The 5th temporary (x29) register. */
    DR_REG_T5 = DR_REG_X30,  /**< The 6th temporary (x30) register. */
    DR_REG_T6 = DR_REG_X31,  /**< The 7th temporary (x31) register. */
    DR_REG_PC,               /**< The program counter. */
    /* Floating point registers */
    DR_REG_F0,   /**< The f0(ft0) floating-point register. */
    DR_REG_F1,   /**< The f1(ft1) floating-point register. */
    DR_REG_F2,   /**< The f2(ft2) floating-point register. */
    DR_REG_F3,   /**< The f3(ft3) floating-point register. */
    DR_REG_F4,   /**< The f4(ft4) floating-point register. */
    DR_REG_F5,   /**< The f5(ft5) floating-point register. */
    DR_REG_F6,   /**< The f6(ft6) floating-point register. */
    DR_REG_F7,   /**< The f7(ft7) floating-point register. */
    DR_REG_F8,   /**< The f8(fs0) floating-point register. */
    DR_REG_F9,   /**< The f9(fs1) floating-point register. */
    DR_REG_F10,  /**< The f10(fa0) floating-point register. */
    DR_REG_F11,  /**< The f11(fa1) floating-point register. */
    DR_REG_F12,  /**< The f12(fa2) floating-point register. */
    DR_REG_F13,  /**< The f13(fa3) floating-point register. */
    DR_REG_F14,  /**< The f14(fa4) floating-point register. */
    DR_REG_F15,  /**< The f15(fa5) floating-point register. */
    DR_REG_F16,  /**< The f16(fa6) floating-point register. */
    DR_REG_F17,  /**< The f17(fa7) floating-point register. */
    DR_REG_F18,  /**< The f18(fs2) floating-point register. */
    DR_REG_F19,  /**< The f19(fs3) floating-point register. */
    DR_REG_F20,  /**< The f20(fs4) floating-point register. */
    DR_REG_F21,  /**< The f21(fs5) floating-point register. */
    DR_REG_F22,  /**< The f22(fs6) floating-point register. */
    DR_REG_F23,  /**< The f23(fs7) floating-point register. */
    DR_REG_F24,  /**< The f24(fs8) floating-point register. */
    DR_REG_F25,  /**< The f25(fs9) floating-point register. */
    DR_REG_F26,  /**< The f26(fs10) floating-point register. */
    DR_REG_F27,  /**< The f27(fs11) floating-point register. */
    DR_REG_F28,  /**< The f28(ft8) floating-point register. */
    DR_REG_F29,  /**< The f29(ft9) floating-point register. */
    DR_REG_F30,  /**< The f30(ft10) floating-point register. */
    DR_REG_F31,  /**< The f31(ft11) floating-point register. */
    DR_REG_FCSR, /**< The floating-point control and status register. */
    /* FPR aliases */
    DR_REG_FT0 = DR_REG_F0, /**< The 1st temporary floating-point (f0) register. */
    DR_REG_FT1 = DR_REG_F1, /**< The 2nd temporary floating-point (f1) register. */
    DR_REG_FT2 = DR_REG_F2, /**< The 3rd temporary floating-point (f2) register. */
    DR_REG_FT3 = DR_REG_F3, /**< The 4th temporary floating-point (f3) register. */
    DR_REG_FT4 = DR_REG_F4, /**< The 5th temporary floating-point (f4) register. */
    DR_REG_FT5 = DR_REG_F5, /**< The 6th temporary floating-point (f5) register. */
    DR_REG_FT6 = DR_REG_F6, /**< The 7th temporary floating-point (f6) register. */
    DR_REG_FT7 = DR_REG_F7, /**< The 8th temporary floating-point (f7) register. */
    DR_REG_FS0 = DR_REG_F8, /**< The 1st callee-saved floating-point (f8) register. */
    DR_REG_FS1 = DR_REG_F9, /**< The 2nd callee-saved floating-point (f9) register. */
    /** The 1st argument/return value floating-point (f10) register. */
    DR_REG_FA0 = DR_REG_F10,
    /** The 2nd argument/return value floating-point (f11) register. */
    DR_REG_FA1 = DR_REG_F11,
    DR_REG_FA2 = DR_REG_F12,  /**< The 3rd argument floating-point (f12) register. */
    DR_REG_FA3 = DR_REG_F13,  /**< The 4th argument floating-point (f13) register. */
    DR_REG_FA4 = DR_REG_F14,  /**< The 5th argument floating-point (f14) register. */
    DR_REG_FA5 = DR_REG_F15,  /**< The 6th argument floating-point (f15) register. */
    DR_REG_FA6 = DR_REG_F16,  /**< The 7th argument floating-point (f16) register. */
    DR_REG_FA7 = DR_REG_F17,  /**< The 8th argument floating-point (f17) register. */
    DR_REG_FS2 = DR_REG_F18,  /**< The 3rd callee-saved floating-point (f18) register. */
    DR_REG_FS3 = DR_REG_F19,  /**< The 4th callee-saved floating-point (f19) register. */
    DR_REG_FS4 = DR_REG_F20,  /**< The 5th callee-saved floating-point (f20) register. */
    DR_REG_FS5 = DR_REG_F21,  /**< The 6th callee-saved floating-point (f21) register. */
    DR_REG_FS6 = DR_REG_F22,  /**< The 7th callee-saved floating-point (f22) register. */
    DR_REG_FS7 = DR_REG_F23,  /**< The 8th callee-saved floating-point (f23) register. */
    DR_REG_FS8 = DR_REG_F24,  /**< The 9th callee-saved floating-point (f24) register. */
    DR_REG_FS9 = DR_REG_F25,  /**< The 10th callee-saved floating-point (f25) register. */
    DR_REG_FS10 = DR_REG_F26, /**< The 11th callee-saved floating-point (f26) register. */
    DR_REG_FS11 = DR_REG_F27, /**< The 12th callee-saved floating-point (f27) register. */
    DR_REG_FT8 = DR_REG_F28,  /**< The 9th temporary floating-point (f28) register. */
    DR_REG_FT9 = DR_REG_F29,  /**< The 10th temporary floating-point (f29) register. */
    DR_REG_FT10 = DR_REG_F30, /**< The 11th temporary floating-point (f30) register. */
    DR_REG_FT11 = DR_REG_F31, /**< The 12th temporary floating-point (f31) register. */

    /* FIXME i#3544: CCSRs */

    DR_REG_LAST_VALID_ENUM = DR_REG_FCSR, /**< Last valid register enum. */
    DR_REG_LAST_ENUM = DR_REG_FCSR,       /**< Last value of register enums. */

    DR_REG_START_64 = DR_REG_X0,  /**< Start of 64-bit register enum values. */
    DR_REG_STOP_64 = DR_REG_F31,  /**< End of 64-bit register enum values. */
    DR_REG_START_32 = DR_REG_X0,  /**< Start of 32-bit register enum values. */
    DR_REG_STOP_32 = DR_REG_F31,  /**< End of 32-bit register enum values. */
    DR_REG_START_GPR = DR_REG_X0, /**< Start of general register registers. */
    DR_REG_STOP_GPR = DR_REG_X31, /**< End of general register registers. */
    DR_REG_XSP = DR_REG_SP, /**< Platform-independent way to refer to stack pointer. */

    DR_NUM_GPR_REGS = DR_REG_STOP_GPR - DR_REG_START_GPR + 1, /**< Count of GPR regs. */
    DR_NUM_SIMD_VECTOR_REGS = 0,                              /**< Count of SIMD regs. */
#else /* RISCV64 */
#    error Register definitions missing for this platform.
#endif
};

/* we avoid typedef-ing the enum, as its storage size is compiler-specific */
typedef ushort reg_id_t; /**< The type of a DR_REG_ enum value. */
/* For x86 we do store reg_id_t here, but the x86 DR_REG_ enum is small enough
 * (checked in d_r_arch_init()).
 */
typedef byte opnd_size_t; /**< The type of an OPSZ_ enum value. */

#ifdef X86
/* Platform-independent full-register specifiers */
#    ifdef X64
#        define DR_REG_XAX \
            DR_REG_RAX /**< Platform-independent way to refer to rax/eax. */
#        define DR_REG_XCX \
            DR_REG_RCX /**< Platform-independent way to refer to rcx/ecx. */
#        define DR_REG_XDX \
            DR_REG_RDX /**< Platform-independent way to refer to rdx/edx. */
#        define DR_REG_XBX \
            DR_REG_RBX /**< Platform-independent way to refer to rbx/ebx. */
#        define DR_REG_XSP \
            DR_REG_RSP /**< Platform-independent way to refer to rsp/esp. */
#        define DR_REG_XBP \
            DR_REG_RBP /**< Platform-independent way to refer to rbp/ebp. */
#        define DR_REG_XSI \
            DR_REG_RSI /**< Platform-independent way to refer to rsi/esi. */
#        define DR_REG_XDI \
            DR_REG_RDI /**< Platform-independent way to refer to rdi/edi. */
#    else
#        define DR_REG_XAX \
            DR_REG_EAX /**< Platform-independent way to refer to rax/eax. */
#        define DR_REG_XCX \
            DR_REG_ECX /**< Platform-independent way to refer to rcx/ecx. */
#        define DR_REG_XDX \
            DR_REG_EDX /**< Platform-independent way to refer to rdx/edx. */
#        define DR_REG_XBX \
            DR_REG_EBX /**< Platform-independent way to refer to rbx/ebx. */
#        define DR_REG_XSP \
            DR_REG_ESP /**< Platform-independent way to refer to rsp/esp. */
#        define DR_REG_XBP \
            DR_REG_EBP /**< Platform-independent way to refer to rbp/ebp. */
#        define DR_REG_XSI \
            DR_REG_ESI /**< Platform-independent way to refer to rsi/esi. */
#        define DR_REG_XDI \
            DR_REG_EDI /**< Platform-independent way to refer to rdi/edi. */
#    endif
#endif /* X86 */

#ifdef X86 /* We don't need these for ARM which uses clear numbering */
#    define DR_REG_START_GPR DR_REG_XAX /**< Start of general register enum values */
#    ifdef X64
#        define DR_REG_STOP_GPR DR_REG_R15 /**< End of general register enum values */
#    else
#        define DR_REG_STOP_GPR DR_REG_XDI /**< End of general register enum values */
#    endif
/** Number of general registers */
#    define DR_NUM_GPR_REGS (DR_REG_STOP_GPR - DR_REG_START_GPR + 1)
/** The number of SIMD vector registers. */
#    define DR_NUM_SIMD_VECTOR_REGS (DR_REG_STOP_ZMM - DR_REG_START_ZMM + 1)
#    define DR_REG_START_64 \
        DR_REG_RAX                    /**< Start of 64-bit general register enum values */
#    define DR_REG_STOP_64 DR_REG_R15 /**< End of 64-bit general register enum values */
#    define DR_REG_START_32 \
        DR_REG_EAX /**< Start of 32-bit general register enum values */
#    define DR_REG_STOP_32                                          \
        DR_REG_R15D /**< End of 32-bit general register enum values \
                     */
#    define DR_REG_START_16                                         \
        DR_REG_AX /**< Start of 16-bit general register enum values \
                   */
#    define DR_REG_STOP_16                                                           \
        DR_REG_R15W                  /**< End of 16-bit general register enum values \
                                      */
#    define DR_REG_START_8 DR_REG_AL /**< Start of 8-bit general register enum values */
#    define DR_REG_STOP_8 DR_REG_DIL /**< End of 8-bit general register enum values */
#    define DR_REG_START_8HL \
        DR_REG_AL                     /**< Start of 8-bit high-low register enum values */
#    define DR_REG_STOP_8HL DR_REG_BH /**< End of 8-bit high-low register enum values */
#    define DR_REG_START_x86_8 \
        DR_REG_AH /**< Start of 8-bit x86-only register enum values */
#    define DR_REG_STOP_x86_8 \
        DR_REG_BH /**< Stop of 8-bit x86-only register enum values */
#    define DR_REG_START_x64_8 \
        DR_REG_SPL /**< Start of 8-bit x64-only register enum values*/
#    define DR_REG_STOP_x64_8 \
        DR_REG_DIL /**< Stop of 8-bit x64-only register enum values */
#    define DR_REG_START_MMX DR_REG_MM0  /**< Start of mmx register enum values */
#    define DR_REG_STOP_MMX DR_REG_MM7   /**< End of mmx register enum values */
#    define DR_REG_START_XMM DR_REG_XMM0 /**< Start of sse xmm register enum values */
#    define DR_REG_START_YMM DR_REG_YMM0 /**< Start of ymm register enum values */
#    define DR_REG_START_ZMM DR_REG_ZMM0 /**< Start of zmm register enum values */
#    ifdef X64
#        define DR_REG_STOP_XMM DR_REG_XMM31 /**< End of sse xmm register enum values */
#        define DR_REG_STOP_YMM DR_REG_YMM31 /**< End of ymm register enum values */
#        define DR_REG_STOP_ZMM DR_REG_ZMM31 /**< End of zmm register enum values */
#    else
#        define DR_REG_STOP_XMM DR_REG_XMM7 /**< End of sse xmm register enum values */
#        define DR_REG_STOP_YMM DR_REG_YMM7 /**< End of ymm register enum values */
#        define DR_REG_STOP_ZMM DR_REG_ZMM7 /**< End of zmm register enum values */
#    endif
#    define DR_REG_START_OPMASK DR_REG_K0 /**< Start of opmask register enum values */
#    define DR_REG_STOP_OPMASK DR_REG_K7  /**< End of opmask register enum values */
#    define DR_REG_START_BND DR_REG_BND0  /**< Start of bounds register enum values */
#    define DR_REG_STOP_BND DR_REG_BND3   /**< End of bounds register enum values */
#    define DR_REG_START_FLOAT \
        DR_REG_ST0 /**< Start of floating-point-register enum values*/
#    define DR_REG_STOP_FLOAT \
        DR_REG_ST7 /**< End of floating-point-register enum values */
#    define DR_REG_START_SEGMENT DR_SEG_ES /**< Start of segment register enum values */
#    define DR_REG_START_SEGMENT_x64 \
        DR_SEG_FS /**< Start of segment register enum values for x64 */
#    define DR_REG_STOP_SEGMENT DR_SEG_GS /**< End of segment register enum values */
#    define DR_REG_START_DR DR_REG_DR0    /**< Start of debug register enum values */
#    define DR_REG_STOP_DR DR_REG_DR15    /**< End of debug register enum values */
#    define DR_REG_START_CR DR_REG_CR0    /**< Start of control register enum values */
#    define DR_REG_STOP_CR DR_REG_CR15    /**< End of control register enum values */
/**
 * Last valid register enum value.  Note: DR_REG_INVALID is now smaller
 * than this value.
 */
#    define DR_REG_LAST_VALID_ENUM DR_REG_K7
#    define DR_REG_LAST_ENUM DR_REG_BND3 /**< Last value of register enums */
#endif                                   /* X86 */

#define REG_NULL DR_REG_NULL
#define REG_INVALID DR_REG_INVALID
#ifndef ARM
#    define REG_START_64 DR_REG_START_64
#    define REG_STOP_64 DR_REG_STOP_64
#endif
#define REG_START_32 DR_REG_START_32
#define REG_STOP_32 DR_REG_STOP_32
#define REG_LAST_VALID_ENUM DR_REG_LAST_VALID_ENUM
#define REG_LAST_ENUM DR_REG_LAST_ENUM
#define REG_XSP DR_REG_XSP
/* Backward compatibility with REG_ constants (we now use DR_REG_ to avoid
 * conflicts with the REG_ enum in <sys/ucontext.h>: i#34).
 * Clients should set(DynamoRIO_REG_COMPATIBILITY ON) prior to
 * configure_DynamoRIO_client() to set this define.
 */
#if defined(X86) && defined(DR_REG_ENUM_COMPATIBILITY)
#    define REG_START_16 DR_REG_START_16
#    define REG_STOP_16 DR_REG_STOP_16
#    define REG_START_8 DR_REG_START_8
#    define REG_STOP_8 DR_REG_STOP_8
#    define REG_RAX DR_REG_RAX
#    define REG_RCX DR_REG_RCX
#    define REG_RDX DR_REG_RDX
#    define REG_RBX DR_REG_RBX
#    define REG_RSP DR_REG_RSP
#    define REG_RBP DR_REG_RBP
#    define REG_RSI DR_REG_RSI
#    define REG_RDI DR_REG_RDI
#    define REG_R8 DR_REG_R8
#    define REG_R9 DR_REG_R9
#    define REG_R10 DR_REG_R10
#    define REG_R11 DR_REG_R11
#    define REG_R12 DR_REG_R12
#    define REG_R13 DR_REG_R13
#    define REG_R14 DR_REG_R14
#    define REG_R15 DR_REG_R15
#    define REG_EAX DR_REG_EAX
#    define REG_ECX DR_REG_ECX
#    define REG_EDX DR_REG_EDX
#    define REG_EBX DR_REG_EBX
#    define REG_ESP DR_REG_ESP
#    define REG_EBP DR_REG_EBP
#    define REG_ESI DR_REG_ESI
#    define REG_EDI DR_REG_EDI
#    define REG_R8D DR_REG_R8D
#    define REG_R9D DR_REG_R9D
#    define REG_R10D DR_REG_R10D
#    define REG_R11D DR_REG_R11D
#    define REG_R12D DR_REG_R12D
#    define REG_R13D DR_REG_R13D
#    define REG_R14D DR_REG_R14D
#    define REG_R15D DR_REG_R15D
#    define REG_AX DR_REG_AX
#    define REG_CX DR_REG_CX
#    define REG_DX DR_REG_DX
#    define REG_BX DR_REG_BX
#    define REG_SP DR_REG_SP
#    define REG_BP DR_REG_BP
#    define REG_SI DR_REG_SI
#    define REG_DI DR_REG_DI
#    define REG_R8W DR_REG_R8W
#    define REG_R9W DR_REG_R9W
#    define REG_R10W DR_REG_R10W
#    define REG_R11W DR_REG_R11W
#    define REG_R12W DR_REG_R12W
#    define REG_R13W DR_REG_R13W
#    define REG_R14W DR_REG_R14W
#    define REG_R15W DR_REG_R15W
#    define REG_AL DR_REG_AL
#    define REG_CL DR_REG_CL
#    define REG_DL DR_REG_DL
#    define REG_BL DR_REG_BL
#    define REG_AH DR_REG_AH
#    define REG_CH DR_REG_CH
#    define REG_DH DR_REG_DH
#    define REG_BH DR_REG_BH
#    define REG_R8L DR_REG_R8L
#    define REG_R9L DR_REG_R9L
#    define REG_R10L DR_REG_R10L
#    define REG_R11L DR_REG_R11L
#    define REG_R12L DR_REG_R12L
#    define REG_R13L DR_REG_R13L
#    define REG_R14L DR_REG_R14L
#    define REG_R15L DR_REG_R15L
#    define REG_SPL DR_REG_SPL
#    define REG_BPL DR_REG_BPL
#    define REG_SIL DR_REG_SIL
#    define REG_DIL DR_REG_DIL
#    define REG_MM0 DR_REG_MM0
#    define REG_MM1 DR_REG_MM1
#    define REG_MM2 DR_REG_MM2
#    define REG_MM3 DR_REG_MM3
#    define REG_MM4 DR_REG_MM4
#    define REG_MM5 DR_REG_MM5
#    define REG_MM6 DR_REG_MM6
#    define REG_MM7 DR_REG_MM7
#    define REG_XMM0 DR_REG_XMM0
#    define REG_XMM1 DR_REG_XMM1
#    define REG_XMM2 DR_REG_XMM2
#    define REG_XMM3 DR_REG_XMM3
#    define REG_XMM4 DR_REG_XMM4
#    define REG_XMM5 DR_REG_XMM5
#    define REG_XMM6 DR_REG_XMM6
#    define REG_XMM7 DR_REG_XMM7
#    define REG_XMM8 DR_REG_XMM8
#    define REG_XMM9 DR_REG_XMM9
#    define REG_XMM10 DR_REG_XMM10
#    define REG_XMM11 DR_REG_XMM11
#    define REG_XMM12 DR_REG_XMM12
#    define REG_XMM13 DR_REG_XMM13
#    define REG_XMM14 DR_REG_XMM14
#    define REG_XMM15 DR_REG_XMM15
#    define REG_ST0 DR_REG_ST0
#    define REG_ST1 DR_REG_ST1
#    define REG_ST2 DR_REG_ST2
#    define REG_ST3 DR_REG_ST3
#    define REG_ST4 DR_REG_ST4
#    define REG_ST5 DR_REG_ST5
#    define REG_ST6 DR_REG_ST6
#    define REG_ST7 DR_REG_ST7
#    define SEG_ES DR_SEG_ES
#    define SEG_CS DR_SEG_CS
#    define SEG_SS DR_SEG_SS
#    define SEG_DS DR_SEG_DS
#    define SEG_FS DR_SEG_FS
#    define SEG_GS DR_SEG_GS
#    define REG_DR0 DR_REG_DR0
#    define REG_DR1 DR_REG_DR1
#    define REG_DR2 DR_REG_DR2
#    define REG_DR3 DR_REG_DR3
#    define REG_DR4 DR_REG_DR4
#    define REG_DR5 DR_REG_DR5
#    define REG_DR6 DR_REG_DR6
#    define REG_DR7 DR_REG_DR7
#    define REG_DR8 DR_REG_DR8
#    define REG_DR9 DR_REG_DR9
#    define REG_DR10 DR_REG_DR10
#    define REG_DR11 DR_REG_DR11
#    define REG_DR12 DR_REG_DR12
#    define REG_DR13 DR_REG_DR13
#    define REG_DR14 DR_REG_DR14
#    define REG_DR15 DR_REG_DR15
#    define REG_CR0 DR_REG_CR0
#    define REG_CR1 DR_REG_CR1
#    define REG_CR2 DR_REG_CR2
#    define REG_CR3 DR_REG_CR3
#    define REG_CR4 DR_REG_CR4
#    define REG_CR5 DR_REG_CR5
#    define REG_CR6 DR_REG_CR6
#    define REG_CR7 DR_REG_CR7
#    define REG_CR8 DR_REG_CR8
#    define REG_CR9 DR_REG_CR9
#    define REG_CR10 DR_REG_CR10
#    define REG_CR11 DR_REG_CR11
#    define REG_CR12 DR_REG_CR12
#    define REG_CR13 DR_REG_CR13
#    define REG_CR14 DR_REG_CR14
#    define REG_CR15 DR_REG_CR15
#    define REG_XAX DR_REG_XAX
#    define REG_XCX DR_REG_XCX
#    define REG_XDX DR_REG_XDX
#    define REG_XBX DR_REG_XBX
#    define REG_XBP DR_REG_XBP
#    define REG_XSI DR_REG_XSI
#    define REG_XDI DR_REG_XDI
#    define REG_START_8HL DR_REG_START_8HL
#    define REG_STOP_8HL DR_REG_STOP_8HL
#    define REG_START_x86_8 DR_REG_START_x86_8
#    define REG_STOP_x86_8 DR_REG_STOP_x86_8
#    define REG_START_x64_8 DR_REG_START_x64_8
#    define REG_STOP_x64_8 DR_REG_STOP_x64_8
#    define REG_START_MMX DR_REG_START_MMX
#    define REG_STOP_MMX DR_REG_STOP_MMX
#    define REG_START_XMM DR_REG_START_XMM
#    define REG_STOP_XMM DR_REG_STOP_XMM
#    define REG_START_YMM DR_REG_START_YMM
#    define REG_STOP_YMM DR_REG_STOP_YMM
#    define REG_START_FLOAT DR_REG_START_FLOAT
#    define REG_STOP_FLOAT DR_REG_STOP_FLOAT
#    define REG_START_SEGMENT DR_REG_START_SEGMENT
#    define REG_START_SEGMENT_x64 DR_REG_START_SEGMENT_x64
#    define REG_STOP_SEGMENT DR_REG_STOP_SEGMENT
#    define REG_START_DR DR_REG_START_DR
#    define REG_STOP_DR DR_REG_STOP_DR
#    define REG_START_CR DR_REG_START_CR
#    define REG_STOP_CR DR_REG_STOP_CR
#    define REG_YMM0 DR_REG_YMM0
#    define REG_YMM1 DR_REG_YMM1
#    define REG_YMM2 DR_REG_YMM2
#    define REG_YMM3 DR_REG_YMM3
#    define REG_YMM4 DR_REG_YMM4
#    define REG_YMM5 DR_REG_YMM5
#    define REG_YMM6 DR_REG_YMM6
#    define REG_YMM7 DR_REG_YMM7
#    define REG_YMM8 DR_REG_YMM8
#    define REG_YMM9 DR_REG_YMM9
#    define REG_YMM10 DR_REG_YMM10
#    define REG_YMM11 DR_REG_YMM11
#    define REG_YMM12 DR_REG_YMM12
#    define REG_YMM13 DR_REG_YMM13
#    define REG_YMM14 DR_REG_YMM14
#    define REG_YMM15 DR_REG_YMM15
#endif /* X86 && DR_REG_ENUM_COMPATIBILITY */

/**
 * These flags describe how the index register in a memory reference is shifted
 * before being added to or subtracted from the base register.  They also describe
 * how a general source register is shifted before being used in its containing
 * instruction.
 */
typedef enum _dr_shift_type_t {
    DR_SHIFT_LSL, /**< Logical shift left. */
    DR_SHIFT_LSR, /**< Logical shift right. */
    DR_SHIFT_ASR, /**< Arithmetic shift right. */
    DR_SHIFT_ROR, /**< Rotate right. */
    DR_SHIFT_MUL, /**< Multiply. */
    /**
     * The register is rotated right by 1 bit, with the carry flag (rather than
     * bit 0) being shifted in to the most-significant bit.  (For shifts of
     * general source registers, if the instruction writes the condition codes,
     * bit 0 is then shifted into the carry flag: but for memory references bit
     * 0 is simply dropped.)
     * Only valid for shifts whose amount is stored in an immediate, not a register.
     */
    DR_SHIFT_RRX,
    /**
     * No shift.
     * Only valid for shifts whose amount is stored in an immediate, not a register.
     */
    DR_SHIFT_NONE,
} dr_shift_type_t;

/**
 * These flags describe how the index register in a memory reference is extended
 * before being optionally shifted and added to the base register. They also describe
 * how a general source register is extended before being used in its containing
 * instruction.
 */
typedef enum _dr_extend_type_t {
    DR_EXTEND_DEFAULT = 0, /**< Default value. */
    DR_EXTEND_UXTB = 0,    /**< Unsigned extend byte. */
    DR_EXTEND_UXTH,        /**< Unsigned extend halfword. */
    DR_EXTEND_UXTW,        /**< Unsigned extend word. */
    DR_EXTEND_UXTX,        /**< Unsigned extend doubleword (a no-op). */
    DR_EXTEND_SXTB,        /**< Signed extend byte. */
    DR_EXTEND_SXTH,        /**< Signed extend halfword. */
    DR_EXTEND_SXTW,        /**< Signed extend word. */
    DR_EXTEND_SXTX,        /**< Signed extend doubleword (a no-op). */
} dr_extend_type_t;

/**
 * These flags describe the values for "pattern" operands for aarch64
 * predicate count instructions. They are always set for imms with the
 * flag #DR_OPND_IS_PREDICATE_CONSTRAINT
 */
typedef enum _dr_pred_constr_type_t {
    DR_PRED_CONSTR_POW2 = 0, /**< POW2 pattern. */
    DR_PRED_CONSTR_VL1,      /**< 1 active elements. */
    DR_PRED_CONSTR_VL2,      /**< 2 active elements. */
    DR_PRED_CONSTR_VL3,      /**< 3 active elements. */
    DR_PRED_CONSTR_VL4,      /**< 4 active elements. */
    DR_PRED_CONSTR_VL5,      /**< 5 active elements. */
    DR_PRED_CONSTR_VL6,      /**< 6 active elements. */
    DR_PRED_CONSTR_VL7,      /**< 7 active elements. */
    DR_PRED_CONSTR_VL8,      /**< 8 active elements. */
    DR_PRED_CONSTR_VL16,     /**< 16 active elements. */
    DR_PRED_CONSTR_VL32,     /**< 32 active elements. */
    DR_PRED_CONSTR_VL64,     /**< 64 active elements. */
    DR_PRED_CONSTR_VL128,    /**< 128 active elements. */
    DR_PRED_CONSTR_VL256,    /**< 256 active elements. */
    DR_PRED_CONSTR_UIMM5_14, /**< Unspecified constraint. */
    DR_PRED_CONSTR_UIMM5_15, /**< Unspecified constraint. */
    DR_PRED_CONSTR_UIMM5_16, /**< Unspecified constraint. */
    DR_PRED_CONSTR_UIMM5_17, /**< Unspecified constraint. */
    DR_PRED_CONSTR_UIMM5_18, /**< Unspecified constraint. */
    DR_PRED_CONSTR_UIMM5_19, /**< Unspecified constraint. */
    DR_PRED_CONSTR_UIMM5_20, /**< Unspecified constraint. */
    DR_PRED_CONSTR_UIMM5_21, /**< Unspecified constraint. */
    DR_PRED_CONSTR_UIMM5_22, /**< Unspecified constraint. */
    DR_PRED_CONSTR_UIMM5_23, /**< Unspecified constraint. */
    DR_PRED_CONSTR_UIMM5_24, /**< Unspecified constraint. */
    DR_PRED_CONSTR_UIMM5_25, /**< Unspecified constraint. */
    DR_PRED_CONSTR_UIMM5_26, /**< Unspecified constraint. */
    DR_PRED_CONSTR_UIMM5_27, /**< Unspecified constraint. */
    DR_PRED_CONSTR_UIMM5_28, /**< Unspecified constraint. */
    DR_PRED_CONSTR_MUL4,     /**< Largest multiple of 4 active elements. */
    DR_PRED_CONSTR_MUL3,     /**< Largest multiple of 3 active elements. */
    DR_PRED_CONSTR_ALL,      /**< all elements active. */

    DR_PRED_CONSTR_FIRST_NUMBER = DR_PRED_CONSTR_UIMM5_14,
    DR_PRED_CONSTR_LAST_NUMBER = DR_PRED_CONSTR_UIMM5_28,
} dr_pred_constr_type_t;

/**
 * These flags describe operations performed on the value of a source register
 * before it is combined with other sources as part of the behavior of the
 * containing instruction, or operations performed on an index register or
 * displacement before it is added to or subtracted from the base register.
 */
typedef enum _dr_opnd_flags_t {
    /** Default (no additional flags). */
    DR_OPND_DEFAULT = 0x00,
    /** This register's value is negated prior to use in the containing instruction. */
    DR_OPND_NEGATED = 0x01,
    /**
     * This register's value is shifted prior to use in the containing instruction.
     * This flag is for informational purposes only and is not guaranteed to
     * be consistent with the shift type of an index register or displacement
     * if the latter are set without using opnd_set_index_shift() or if an
     * instruction is created without using high-level API routines.
     * This flag is also ignored for encoding and will not apply a shift
     * on its own.
     */
    DR_OPND_SHIFTED = 0x02,
    /**
     * This operand should be combined with an adjacent operand to create a
     * single value.  This flag is typically used on immediates: e.g., for ARM's
     * OP_vbic_i64, two 32-bit immediate operands should be interpreted as the
     * low and high parts of a 64-bit value.
     */
    DR_OPND_MULTI_PART = 0x04,
    /**
     * This immediate integer operand should be interpreted as an ARM/AArch64 shift type.
     */
    DR_OPND_IS_SHIFT = 0x08,
    /** A hint indicating that this register operand is part of a register list. */
    DR_OPND_IN_LIST = 0x10,
    /**
     * This register's value is extended prior to use in the containing instruction.
     * This flag is for informational purposes only and is not guaranteed to
     * be consistent with the shift type of an index register or displacement
     * if the latter are set without using opnd_set_index_extend() or if an
     * instruction is created without using high-level API routines.
     * This flag is also ignored for encoding and will not apply a shift
     * on its own.
     */
    DR_OPND_EXTENDED = 0x20,
    /** This immediate integer operand should be interpreted as an AArch64 extend type. */
    DR_OPND_IS_EXTEND = 0x40,

    /** This immediate integer operand should be interpreted as an AArch64 condition. */
    DR_OPND_IS_CONDITION = 0x80,
    /**
     * Registers with this flag should be considered vectors and have an element size
     * representing their element size.
     */
    DR_OPND_IS_VECTOR = 0x100,
    /**
     * Predicate registers can either be merging, zero or neither. If one of these
     * are set then they are either a merge or zero otherwise aren't either.
     */
    DR_OPND_IS_MERGE_PREDICATE = 0x200,
    DR_OPND_IS_ZERO_PREDICATE = 0x400,
    /**
     * This immediate integer operand should be treated as an AArch64
     * SVE predicate constraint
     */
    DR_OPND_IS_PREDICATE_CONSTRAINT = 0x800,

    /**
     * This is used by RISCV64 for immediates display format.
     */
    DR_OPND_IMM_PRINT_DECIMAL = 0x1000,
} dr_opnd_flags_t;

#ifdef DR_FAST_IR

/* We assume all addressing regs are in the lower 256 of the DR_REG_ enum. */
#    define REG_SPECIFIER_BITS 8
#    define SCALE_SPECIFIER_BITS 4

/**
 * opnd_t type exposed for optional "fast IR" access.  Note that DynamoRIO
 * reserves the right to change this structure across releases and does
 * not guarantee binary or source compatibility when this structure's fields
 * are directly accessed.  If the OPND_ macros are used, DynamoRIO does
 * guarantee source compatibility, but not binary compatibility.  If binary
 * compatibility is desired, do not use the fast IR feature.
 */
struct _opnd_t {
    byte kind;
    /* Size field: used for immed_ints and addresses and registers,
     * but for registers, if 0, the full size of the register is assumed.
     * It holds a OPSZ_ field from decode.h.
     * We need it so we can pick the proper instruction form for
     * encoding -- an alternative would be to split all the opcodes
     * up into different data size versions.
     */
    opnd_size_t size;
    /* To avoid increasing our union beyond 64 bits, we store additional data
     * needed for x64 operand types here in the alignment padding.
     */
    union {
        ushort far_pc_seg_selector; /* FAR_PC_kind and FAR_INSTR_kind */
        /* We could fit segment in value.base_disp but more consistent here */
        reg_id_t segment : REG_SPECIFIER_BITS; /* BASE_DISP_kind, REL_ADDR_kind,
                                                * and ABS_ADDR_kind, on x86 */
        ushort disp;                           /* MEM_INSTR_kind */
        ushort shift;                          /* INSTR_kind */
        /* We have to use a shorter type, not the enum type, to get cl to not align */
        /* Used for ARM: REG_kind, BASE_DISP_kind, and IMMED_INTEGER_kind */
        ushort /*dr_opnd_flags_t*/ flags;
    } aux;
    union {
        /* all are 64 bits or less */
        /* NULL_kind has no value */
        ptr_int_t immed_int; /* IMMED_INTEGER_kind */
        struct {
            int low;  /* IMMED_INTEGER_kind with DR_OPND_MULTI_PART */
            int high; /* IMMED_INTEGER_kind with DR_OPND_MULTI_PART */
        } immed_int_multi_part;
        float immed_float; /* IMMED_FLOAT_kind */
#    ifndef WINDOWS
        /* XXX i#4488: x87 floating point immediates should be double precision.
         * Currently not included for Windows because sizeof(opnd_t) does not
         * equal EXPECTED_SIZEOF_OPND, triggering the ASSERT in d_r_arch_init().
         */
        /* For 32-bit ARM we keep alignment at 4 to avoid changing the opnd_t shape.
         * Marking this field as packed seems to do it and avoids other changes
         * that might occur if packing the whole struct.
         * XXX i#4488: Do any double-loading instructions require 8-byte alignment?
         * Perhaps we should just break compatibility and align this to 8 for
         * x86 and ARM 32-bit.
         */
        double immed_double IF_ARM(__attribute__((__packed__))); /* IMMED_DOUBLE_kind */
#    endif
        /* PR 225937: today we provide no way of specifying a 16-bit immediate
         * (encoded as a data16 prefix, which also implies a 16-bit EIP,
         * making it only useful for far pcs)
         */
        app_pc pc; /* PC_kind and FAR_PC_kind */
        /* For FAR_PC_kind and FAR_INSTR_kind, we use pc/instr, and keep the
         * segment selector (which is NOT a DR_SEG_ constant) in far_pc_seg_selector
         * above, to save space.
         */
        instr_t *instr; /* INSTR_kind, FAR_INSTR_kind, and MEM_INSTR_kind */
        struct {
            reg_id_t reg;
            /* XXX #5638: Fill in the element size for x86 and aarch32. */
            opnd_size_t element_size;
        } reg_and_element_size; /* REG_kind */
        struct {
            /* For ARM, either disp==0 or index_reg==DR_REG_NULL: can't have both */
            int disp;
            reg_id_t base_reg : REG_SPECIFIER_BITS;
            reg_id_t index_reg : REG_SPECIFIER_BITS;
            /* to get cl to not align to 4 bytes we can't use uint here
             * when we have reg_id_t elsewhere: it won't combine them
             * (gcc will). alternative is all uint and no reg_id_t.
             * We also have to use byte and not dr_shift_type_t
             * to get cl to not align.
             */
#    if defined(AARCH64)
            /* This is only used to distinguish pre-index from post-index when the
             * offset is zero, for example: ldr w1,[x2,#0]! from ldr w1,[x0],#0.
             */
            byte /*bool*/ pre_index : 1;
            /* Access this using opnd_get_index_extend and opnd_set_index_extend. */
            byte /*dr_extend_type_t*/ extend_type : 3;
            /* Enable shift register offset left */
            byte /*bool*/ scaled : 1;
            /* Shift offset amount */
            byte /*uint*/ scaled_value : 3;
            /* Indicates the element size for vector base and index registers.
             * Only 2 element sizes are used for vector base/index registers in SVE:
             *     Single (OPSZ_4)
             *     Double (OPSZ_8)
             * so we only need one bit to store the value (see ELEMENT_SIZE_* enum in
             * opnd_shared.c).
             * This is ignored if the base and index registers are scalar registers.
             */
            byte /*enum*/ element_size : 1;
#    elif defined(ARM)
            byte /*dr_shift_type_t*/ shift_type : 3;
            byte shift_amount_minus_1 : 5; /* 1..31 so we store (val - 1) */
#    elif defined(X86)
            byte scale : SCALE_SPECIFIER_BITS;
            byte /*bool*/ encode_zero_disp : 1;
            byte /*bool*/ force_full_disp : 1;  /* don't use 8-bit even w/ 8-bit value */
            byte /*bool*/ disp_short_addr : 1;  /* 16-bit (32 in x64) addr (disp-only) */
            byte /*bool*/ index_reg_is_zmm : 1; /* Indicates that the index_reg of the
                                                 * VSIB address is of length ZMM. This
                                                 * flag is not exposed and serves as an
                                                 * internal AVX-512 extension of
                                                 * index_reg, leaving index_reg binary
                                                 * compatible at 8 bits.
                                                 */
#    endif
        } base_disp; /* BASE_DISP_kind */
        void *addr;  /* REL_ADDR_kind and ABS_ADDR_kind */
    } value;
};
#endif /* DR_FAST_IR */

#ifdef DR_FAST_IR
/** x86 operand kinds */
/* We're deliberately NOT adding doxygen comments b/c users should use our macros. */
enum {
    NULL_kind,
    IMMED_INTEGER_kind,
    IMMED_FLOAT_kind,
    PC_kind,
    INSTR_kind,
    REG_kind,
    BASE_DISP_kind, /* optional DR_SEG_ reg + base reg + scaled index reg + disp */
    FAR_PC_kind,    /* a segment is specified as a selector value */
    FAR_INSTR_kind, /* a segment is specified as a selector value */
#    if defined(X64) || defined(ARM)
    REL_ADDR_kind, /* pc-relative address: ARM or 64-bit X86 only */
#    endif
#    ifdef X64
    ABS_ADDR_kind, /* 64-bit absolute address: x64 only */
#    endif
    MEM_INSTR_kind,
    IMMED_DOUBLE_kind,
    LAST_kind, /* sentinal; not a valid opnd kind */
};
#endif /* DR_FAST_IR */

DR_API
INSTR_INLINE
/** Returns an empty operand. */
opnd_t
opnd_create_null(void);

DR_API
INSTR_INLINE
/** Returns a register operand (\p r must be a DR_REG_ constant). */
opnd_t
opnd_create_reg(reg_id_t r);

DR_API
INSTR_INLINE
/**
 * Returns a register operand corresponding to a part of the
 * register represented by the DR_REG_ constant \p r.
 *
 * On x86, \p r must be a multimedia (mmx, xmm, ymm, zmm) register.  For
 * partial general-purpose registers on x86, use the appropriate
 * sub-register name with opnd_create_reg() instead.
 */
opnd_t
opnd_create_reg_partial(reg_id_t r, opnd_size_t subsize);

DR_API
INSTR_INLINE
/**
 * Returns a register operand corresponding to a vector
 * register that has an element size.
 */
opnd_t
opnd_create_reg_element_vector(reg_id_t r, opnd_size_t element_size);

#ifdef AARCH64
DR_API
INSTR_INLINE
/**
 * Returns a SVE predicate register for use as a governing predicate
 * with either "/m" merge mode set or "/z" zeroing mode set depending
 * on /p is_merge
 * For creating general (non-governing) predicate registers,
 * use opnd_create_reg() for scalar predicates and
 * opnd_create_reg_element_vector() for vector predicates.
 */
opnd_t
opnd_create_predicate_reg(reg_id_t r, bool is_merge);
#endif

DR_API
INSTR_INLINE
/**
 * Returns a register operand with additional properties specified by \p flags.
 * If \p subsize is 0, creates a full-sized register; otherwise, creates a
 * partial register in the manner of opnd_create_reg_partial().
 */
opnd_t
opnd_create_reg_ex(reg_id_t r, opnd_size_t subsize, dr_opnd_flags_t flags);

DR_API
/**
 * Returns a signed immediate integer operand with value \p i and size
 * \p data_size; \p data_size must be a OPSZ_ constant.
 */
opnd_t
opnd_create_immed_int(ptr_int_t i, opnd_size_t data_size);

DR_API
/**
 * Returns an unsigned immediate integer operand with value \p i and size
 * \p data_size; \p data_size must be a OPSZ_ constant.
 */
opnd_t
opnd_create_immed_uint(ptr_uint_t i, opnd_size_t data_size);

DR_API
/**
 * Returns an unsigned immediate integer operand with value \p i and size
 * \p data_size; \p data_size must be a OPSZ_ constant.
 * This operand can be distinguished from a regular immediate integer
 * operand by the flag #DR_OPND_MULTI_PART in opnd_get_flags() which tells
 * the caller to use opnd_get_immed_int64() to retrieve the full value.
 * \note 32-bit only: use opnd_create_immed_int() for 64-bit architectures.
 */
opnd_t
opnd_create_immed_int64(int64 i, opnd_size_t data_size);

DR_API
/**
 * Performs a bitwise NOT operation on the integer value in \p opnd, but only on the LSB
 * bits provided by opnd_size_in_bits(opnd). \p opnd must carry an immed integer.
 */
opnd_t
opnd_invert_immed_int(opnd_t opnd);

DR_API
/**
 * Returns an immediate float operand with value \p f.
 * The caller's code should use proc_save_fpstate() or be inside a
 * clean call that has requested to preserve the floating-point state.
 */
opnd_t
opnd_create_immed_float(float f);

#ifndef WINDOWS
/* XXX i#4488: x87 floating point immediates should be double precision.
 * Type double currently not included for Windows because sizeof(opnd_t) does
 * not equal EXPECTED_SIZEOF_OPND, triggering the ASSERT in d_r_arch_init().
 */
DR_API
/**
 * Returns an immediate double operand with value \p d.
 * The caller's code should use proc_save_fpstate() or be inside a
 * clean call that has requested to preserve the floating-point state.
 */
opnd_t
opnd_create_immed_double(double d);
#endif

#ifdef AARCH64
DR_API
/**
 * Returns an immediate operand for use in SVE predicate constraint
 * operands.
 */
opnd_t
opnd_create_immed_pred_constr(dr_pred_constr_type_t p);
#endif

DR_API
INSTR_INLINE
/** Returns a program address operand with value \p pc. */
opnd_t
opnd_create_pc(app_pc pc);

DR_API
/**
 * Returns a far program address operand with value \p seg_selector:pc.
 * \p seg_selector is a segment selector, not a DR_SEG_ constant.
 */
opnd_t
opnd_create_far_pc(ushort seg_selector, app_pc pc);

DR_API
/**
 * Returns an operand whose value will be the encoded address of \p
 * instr.  This operand can be used as an immediate integer or as a
 * direct call or jump target.  Its size is always #OPSZ_PTR.
 */
opnd_t
opnd_create_instr(instr_t *instr);

DR_API
/**
 * Returns an operand whose value will be the encoded address of \p
 * instr.  This operand can be used as an immediate integer or as a
 * direct call or jump target.  Its size is the specified \p size.
 * Its value can be optionally right-shifted by \p shift from the
 * encoded address.
 */
opnd_t
opnd_create_instr_ex(instr_t *instr, opnd_size_t size, ushort shift);

DR_API
/**
 * Returns a far instr_t pointer address with value \p seg_selector:instr.
 * \p seg_selector is a segment selector, not a DR_SEG_ constant.
 */
opnd_t
opnd_create_far_instr(ushort seg_selector, instr_t *instr);

DR_API
/**
 * Returns a memory reference operand whose value will be the encoded
 * address of \p instr plus the 16-bit displacement \p disp.  For 32-bit
 * mode, it will be encoded just like an absolute address
 * (opnd_create_abs_addr()); for 64-bit mode, it will be encoded just
 * like a pc-relative address (opnd_create_rel_addr()). This operand
 * can be used anywhere a regular memory operand can be used.  Its
 * size is \p data_size.
 *
 * \note This operand will return false to opnd_is_instr(), opnd_is_rel_addr(),
 * and opnd_is_abs_addr().  It is a separate type.
 */
opnd_t
opnd_create_mem_instr(instr_t *instr, short disp, opnd_size_t data_size);

DR_API
/**
 * Returns a memory reference operand that refers to the address:
 * - disp(base_reg, index_reg, scale)
 *
 * or, in other words,
 * - base_reg + index_reg*scale + disp
 *
 * The operand has data size data_size (must be a OPSZ_ constant).
 * Both \p base_reg and \p index_reg must be DR_REG_ constants.
 * \p scale must be either 0, 1, 2, 4, or 8.
 * On ARM, opnd_set_index_shift() can be used for further manipulation
 * of the index register.
 * On ARM, a negative value for \p disp will be converted into a positive
 * value with #DR_OPND_NEGATED set in opnd_get_flags().
 * On ARM, either \p index_reg must be #DR_REG_NULL or disp must be 0.
 *
 * Also use this function to create VSIB operands, passing a SIMD register as
 * the index register.
 */
opnd_t
opnd_create_base_disp(reg_id_t base_reg, reg_id_t index_reg, int scale, int disp,
                      opnd_size_t data_size);

DR_API
/**
 * Returns a memory reference operand that refers to the address:
 * - disp(base_reg, index_reg, scale)
 *
 * or, in other words,
 * - base_reg + index_reg*scale + disp
 *
 * The operand has data size \p data_size (must be a OPSZ_ constant).
 * Both \p base_reg and \p index_reg must be DR_REG_ constants.
 * \p scale must be either 0, 1, 2, 4, or 8.
 * On ARM, a negative value for \p disp will be converted into a positive
 * value with #DR_OPND_NEGATED set in opnd_get_flags().
 * On ARM, either \p index_reg must be #DR_REG_NULL or disp must be 0.
 *
 * On x86, three boolean parameters give control over encoding optimizations
 * (these are ignored on other architectures):
 * - If \p encode_zero_disp, a zero value for disp will not be omitted;
 * - If \p force_full_disp, a small value for disp will not occupy only one byte.
 * - If \p disp_short_addr, short (16-bit for 32-bit mode, 32-bit for
 *    64-bit mode) addressing will be used (note that this normally only
 *    needs to be specified for an absolute address; otherwise, simply
 *    use the desired short registers for base and/or index).
 *
 * (The encoding optimization flags are all false when using
 * opnd_create_base_disp()).
 */
opnd_t
opnd_create_base_disp_ex(reg_id_t base_reg, reg_id_t index_reg, int scale, int disp,
                         opnd_size_t size, bool encode_zero_disp, bool force_full_disp,
                         bool disp_short_addr);

DR_API
/**
 * Returns a far memory reference operand that refers to the address:
 * - seg : disp(base_reg, index_reg, scale)
 *
 * or, in other words,
 * - seg : base_reg + index_reg*scale + disp
 *
 * The operand has data size \p data_size (must be a OPSZ_ constant).
 * \p seg must be a DR_SEG_ constant.
 * Both \p base_reg and \p index_reg must be DR_REG_ constants.
 * \p scale must be either 0, 1, 2, 4, or 8.
 * On ARM, a negative value for \p disp will be converted into a positive
 * value with #DR_OPND_NEGATED set in opnd_get_flags().
 * On ARM, either \p index_reg must be #DR_REG_NULL or disp must be 0.
 */
opnd_t
opnd_create_far_base_disp(reg_id_t seg, reg_id_t base_reg, reg_id_t index_reg, int scale,
                          int disp, opnd_size_t data_size);

DR_API
/**
 * Returns a far memory reference operand that refers to the address:
 * - seg : disp(base_reg, index_reg, scale)
 *
 * or, in other words,
 * - seg : base_reg + index_reg*scale + disp
 *
 * The operand has data size \p size (must be an OPSZ_ constant).
 * \p seg must be a DR_SEG_ constant.
 * Both \p base_reg and \p index_reg must be DR_REG_ constants.
 * scale must be either 0, 1, 2, 4, or 8.
 * On ARM, a negative value for \p disp will be converted into a positive
 * value with #DR_OPND_NEGATED set in opnd_get_flags().
 * On ARM, either \p index_reg must be #DR_REG_NULL or disp must be 0.
 *
 * On x86, three boolean parameters give control over encoding optimizations
 * (these are ignored on ARM):
 * - If \p encode_zero_disp, a zero value for disp will not be omitted;
 * - If \p force_full_disp, a small value for disp will not occupy only one byte.
 * - If \p disp_short_addr, short (16-bit for 32-bit mode, 32-bit for
 *    64-bit mode) addressing will be used (note that this normally only
 *    needs to be specified for an absolute address; otherwise, simply
 *    use the desired short registers for base and/or index).
 *
 * (All of these are false when using opnd_create_far_base_disp()).
 */
opnd_t
opnd_create_far_base_disp_ex(reg_id_t seg, reg_id_t base_reg, reg_id_t index_reg,
                             int scale, int disp, opnd_size_t size, bool encode_zero_disp,
                             bool force_full_disp, bool disp_short_addr);

#ifdef ARM
DR_API
/**
 * Returns a memory reference operand that refers to either a base
 * register plus or minus a constant displacement:
 * - [base_reg, disp]
 *
 * Or a base register plus or minus an optionally shifted index register:
 * - [base_reg, index_reg, shift_type, shift_amount]
 *
 * For an index register, the plus or minus is determined by the presence
 * or absence of #DR_OPND_NEGATED in \p flags.
 *
 * The resulting operand has data size \p size (must be an OPSZ_ constant).
 * Both \p base_reg and \p index_reg must be DR_REG_ constants.
 * A negative value for \p disp will be converted into a positive
 * value with #DR_OPND_NEGATED set in opnd_get_flags().
 * Either \p index_reg must be #DR_REG_NULL or disp must be 0.
 *
 * \note ARM-only.
 */
opnd_t
opnd_create_base_disp_arm(reg_id_t base_reg, reg_id_t index_reg,
                          dr_shift_type_t shift_type, uint shift_amount, int disp,
                          dr_opnd_flags_t flags, opnd_size_t size);
#endif

#ifdef AARCH64
DR_API
/**
 * Returns the left shift amount from \p size.
 */
uint
opnd_size_to_shift_amount(opnd_size_t size);

DR_API
/**
 * Returns a memory reference operand that refers to either a base
 * register with a constant displacement:
 * - [base_reg, disp]
 *
 * Or a base register plus an optionally extended and shifted index register:
 * - [base_reg, index_reg, extend_type, shift_amount]
 *
 * If \p scaled is enabled, \p shift determines the shift amount.
 *
 * The resulting operand has data size \p size (must be an OPSZ_ constant).
 * Both \p base_reg and \p index_reg must be DR_REG_ constants.
 * Either \p index_reg must be #DR_REG_NULL or disp must be 0.
 *
 * TODO i#3044: WARNING this function may change during SVE development of
 * DynamoRIO. The function will be considered stable when this warning has been
 * removed.
 *
 * \note AArch64-only.
 */
opnd_t
opnd_create_base_disp_shift_aarch64(reg_id_t base_reg, reg_id_t index_reg,
                                    dr_extend_type_t extend_type, bool scaled, int disp,
                                    dr_opnd_flags_t flags, opnd_size_t size, uint shift);

DR_API
/**
 * Same as opnd_create_base_disp_shift_aarch64 but if \p scaled is true then the extend
 * amount is calculated from the operand size (otherwise it is zero).
 *
 * \note AArch64-only.
 */
opnd_t
opnd_create_base_disp_aarch64(reg_id_t base_reg, reg_id_t index_reg,
                              dr_extend_type_t extend_type, bool scaled, int disp,
                              dr_opnd_flags_t flags, opnd_size_t size);

DR_API
/**
 * Same as opnd_create_base_disp_shift_aarch64 but creates an operand that uses vector
 * registers for the base and/or index.
 * At least one of \p base_reg and \p index_reg should be a vector register.
 * \p element_size indicates the element size for any vector registers used and must be
 * one of:
 *     OPSZ_4 (single, 32-bit)
 *     OPSZ_8 (double, 64-bit)
 *
 * TODO i#3044: WARNING this function may change during SVE development of
 * DynamoRIO. The function will be considered stable when this warning has been
 * removed.
 *
 * \note AArch64-only.
 */
opnd_t
opnd_create_vector_base_disp_aarch64(reg_id_t base_reg, reg_id_t index_reg,
                                     opnd_size_t element_size,
                                     dr_extend_type_t extend_type, bool scaled, int disp,
                                     dr_opnd_flags_t flags, opnd_size_t size, uint shift);
#endif

DR_API
/**
 * Returns a memory reference operand that refers to the address \p addr.
 * The operand has data size \p data_size (must be a OPSZ_ constant).
 *
 * If \p addr <= 2^32 (which is always true in 32-bit mode), this routine
 * is equivalent to
 * opnd_create_base_disp(DR_REG_NULL, DR_REG_NULL, 0, (int)addr, data_size).
 *
 * Otherwise, this routine creates a separate operand type with an
 * absolute 64-bit memory address.  Such an operand can only be
 * guaranteed to be encodable in absolute form as a load or store from
 * or to the rax (or eax) register.  It will automatically be
 * converted to a pc-relative operand (as though
 * opnd_create_rel_addr() had been called) if it is used in any other
 * way.
 */
opnd_t
opnd_create_abs_addr(void *addr, opnd_size_t data_size);

DR_API
/**
 * Returns a memory reference operand that refers to the address
 * \p seg: \p addr.
 * The operand has data size \p data_size (must be a OPSZ_ constant).
 *
 * If \p addr <= 2^32 (which is always true in 32-bit mode), this routine
 * is equivalent to
 * opnd_create_far_base_disp(seg, DR_REG_NULL, DR_REG_NULL, 0, (int)addr, data_size).
 *
 * Otherwise, this routine creates a separate operand type with an
 * absolute 64-bit memory address.  Such an operand can only be
 * guaranteed to be encodable in absolute form as a load or store from
 * or to the rax (or eax) register.  It will automatically be
 * converted to a pc-relative operand (as though
 * opnd_create_far_rel_addr() had been called) if it is used in any
 * other way.
 */
opnd_t
opnd_create_far_abs_addr(reg_id_t seg, void *addr, opnd_size_t data_size);

#if defined(X64) || defined(ARM)
DR_API
/**
 * Returns a memory reference operand that refers to the address \p
 * addr, but will be encoded as a pc-relative address.  At emit time,
 * if \p addr is out of reach of the maximum encodable displacement
 * (signed 32-bit for x86) from the next instruction, encoding will
 * fail.
 *
 * DR guarantees that all of its code caches, all client libraries and
 * Extensions (though not copies of system libraries), and all client
 * memory allocated through dr_thread_alloc(), dr_global_alloc(),
 * dr_nonheap_alloc(), or dr_custom_alloc() with
 * #DR_ALLOC_CACHE_REACHABLE, can reach each other with a 32-bit
 * displacement.  Thus, any normally-allocated data or any static data
 * or code in a client library is guaranteed to be reachable from code
 * cache code.  Memory allocated through system libraries (including
 * malloc, operator new, and HeapAlloc) is not guaranteed to be
 * reachable: only memory directly allocated via DR's API.  The
 * runtime option -reachable_heap can be used to guarantee that
 * all memory is reachable.
 *
 * On x86, if \p addr is not pc-reachable at encoding time and this
 * operand is used in a load or store to or from the rax (or eax)
 * register, an absolute form will be used (as though
 * opnd_create_abs_addr() had been called).
 *
 * The operand has data size data_size (must be a OPSZ_ constant).
 *
 * To represent a 32-bit address (i.e., what an address size prefix
 * indicates), simply zero out the top 32 bits of the address before
 * passing it to this routine.
 *
 * On ARM, the resulting operand will not contain an explicit PC
 * register, and thus will not return true on queries to whether the
 * operand reads the PC.  Explicit use of opnd_is_rel_addr() is
 * required.  However, DR does not decode any PC-using instructions
 * into this type of relative address operand: decoding will always
 * produce a regular base + displacement operand.
 *
 * \note For ARM or 64-bit X86 DR builds only.
 */
opnd_t
opnd_create_rel_addr(void *addr, opnd_size_t data_size);

DR_API
/**
 * Returns a memory reference operand that refers to the address \p
 * seg : \p addr, but will be encoded as a pc-relative address.  It is
 * up to the caller to ensure that the resulting address is reachable
 * via a 32-bit signed displacement from the next instruction at emit
 * time.
 *
 * DR guarantees that all of its code caches, all client libraries and
 * Extensions (though not copies of system libraries), and all client
 * memory allocated through dr_thread_alloc(), dr_global_alloc(),
 * dr_nonheap_alloc(), or dr_custom_alloc() with
 * #DR_ALLOC_CACHE_REACHABLE, can reach each other with a 32-bit
 * displacement.  Thus, any normally-allocated data or any static data
 * or code in a client library is guaranteed to be reachable from code
 * cache code.  Memory allocated through system libraries (including
 * malloc, operator new, and HeapAlloc) is not guaranteed to be
 * reachable: only memory directly allocated via DR's API.  The
 * runtime option -reachable_heap can be used to guarantee that
 * all memory is reachable.
 *
 * If \p addr is not pc-reachable at encoding time and this operand is
 * used in a load or store to or from the rax (or eax) register, an
 * absolute form will be used (as though opnd_create_far_abs_addr()
 * had been called).
 *
 * The operand has data size \p data_size (must be a OPSZ_ constant).
 *
 * To represent a 32-bit address (i.e., what an address size prefix
 * indicates), simply zero out the top 32 bits of the address before
 * passing it to this routine.
 *
 * \note For 64-bit X86 DR builds only.
 */
opnd_t
opnd_create_far_rel_addr(reg_id_t seg, void *addr, opnd_size_t data_size);
#endif /* X64 || ARM */

DR_API
/** Returns true iff \p opnd is an empty operand. */
bool
opnd_is_null(opnd_t opnd);

DR_API
/** Returns true iff \p opnd is a register operand. */
bool
opnd_is_reg(opnd_t opnd);

DR_API
/** Returns true iff \p opnd is a partial multimedia register operand. */
bool
opnd_is_reg_partial(opnd_t opnd);

DR_API
INSTR_INLINE
/** Returns true iff \p opnd is an immediate (integer or float) operand. */
bool
opnd_is_immed(opnd_t opnd);

DR_API
/** Returns true iff \p opnd is an immediate integer operand. */
bool
opnd_is_immed_int(opnd_t opnd);

DR_API
/**
 * Returns true iff \p opnd is a special 64-bit immediate integer operand
 * on a 32-bit architecture.
 */
bool
opnd_is_immed_int64(opnd_t opnd);

DR_API
/** Returns true iff \p opnd is an immediate float operand. */
bool
opnd_is_immed_float(opnd_t opnd);

DR_API
INSTR_INLINE
/** Returns true iff \p opnd is a (near or far) program address operand. */
bool
opnd_is_pc(opnd_t opnd);

DR_API
/** Returns true iff \p opnd is a near (i.e., default segment) program address operand. */
bool
opnd_is_near_pc(opnd_t opnd);

DR_API
/** Returns true iff \p opnd is a far program address operand. */
bool
opnd_is_far_pc(opnd_t opnd);

DR_API
INSTR_INLINE
/** Returns true iff \p opnd is a (near or far) instr_t pointer address operand. */
bool
opnd_is_instr(opnd_t opnd);

DR_API
/** Returns true iff \p opnd is a near instr_t pointer address operand. */
bool
opnd_is_near_instr(opnd_t opnd);

DR_API
/** Returns true iff \p opnd is a far instr_t pointer address operand. */
bool
opnd_is_far_instr(opnd_t opnd);

DR_API
/** Returns true iff \p opnd is a memory reference to an instr_t address operand. */
bool
opnd_is_mem_instr(opnd_t opnd);

DR_API
/** Returns true iff \p opnd is a (near or far) base+disp memory reference operand. */
bool
opnd_is_base_disp(opnd_t opnd);

DR_API
INSTR_INLINE
/**
 * Returns true iff \p opnd is a near (i.e., default segment) base+disp memory
 * reference operand.
 */
bool
opnd_is_near_base_disp(opnd_t opnd);

DR_API
INSTR_INLINE
/** Returns true iff \p opnd is a far base+disp memory reference operand. */
bool
opnd_is_far_base_disp(opnd_t opnd);

DR_API
INSTR_INLINE
/** Returns true iff \p opnd is a vector reg operand. */
bool
opnd_is_element_vector_reg(opnd_t opnd);

DR_API
INSTR_INLINE
/** Returns true iff \p opnd is a predicate register. */
bool
opnd_is_predicate_reg(opnd_t opnd);

DR_API
INSTR_INLINE
/** Returns true iff \p opnd is a merging predicate register. */
bool
opnd_is_predicate_merge(opnd_t opnd);

DR_API
INSTR_INLINE
/** Returns true iff \p opnd is a zeroing predicate register. */
bool
opnd_is_predicate_zero(opnd_t opnd);

DR_API
/**
 * Returns true iff \p opnd uses vector indexing via a VSIB byte.  This
 * memory addressing form was introduced in Intel AVX2.
 */
bool
opnd_is_vsib(opnd_t opnd);

DR_API
/**
 * Returns true iff \p opnd is a (near or far) absolute address operand.
 * Returns true for both base-disp operands with no base or index and
 * 64-bit non-base-disp absolute address operands.
 */
bool
opnd_is_abs_addr(opnd_t opnd);

DR_API
/**
 * Returns true iff \p opnd is a near (i.e., default segment) absolute address operand.
 * Returns true for both base-disp operands with no base or index and
 * 64-bit non-base-disp absolute address operands.
 */
bool
opnd_is_near_abs_addr(opnd_t opnd);

DR_API
/**
 * Returns true iff \p opnd is a far absolute address operand.
 * Returns true for both base-disp operands with no base or index and
 * 64-bit non-base-disp absolute address operands.
 */
bool
opnd_is_far_abs_addr(opnd_t opnd);

#if defined(X64) || defined(ARM)
DR_API
/**
 * Returns true iff \p opnd is a (near or far) pc-relative memory reference operand.
 * Returns true for base-disp operands on ARM that use the PC as the base register.
 */
bool
opnd_is_rel_addr(opnd_t opnd);

DR_API
INSTR_INLINE
/**
 * Returns true iff \p opnd is a near (i.e., default segment) pc-relative memory
 * reference operand.
 *
 * \note For 64-bit x86 DR builds only.  Equivalent to opnd_is_rel_addr() for ARM.
 */
bool
opnd_is_near_rel_addr(opnd_t opnd);

DR_API
INSTR_INLINE
/**
 * Returns true iff \p opnd is a far pc-relative memory reference operand.
 *
 * \note For 64-bit x86 DR builds only.  Always returns false on ARM.
 */
bool
opnd_is_far_rel_addr(opnd_t opnd);
#endif

DR_API
/**
 * Returns true iff \p opnd is a (near or far) memory reference operand
 * of any type: base-disp, absolute address, or pc-relative address.
 *
 * This routine (along with all other opnd_ routines) does consider
 * multi-byte nops that use addressing operands, or the #OP_lea
 * instruction's source operand, to be memory references: i.e., it
 * only considers whether the operand calculates an address.  Use
 * instr_reads_memory() to operate on a higher semantic level and rule
 * out these corner cases.
 */
bool
opnd_is_memory_reference(opnd_t opnd);

DR_API
/**
 * Returns true iff \p opnd is a far memory reference operand
 * of any type: base-disp, absolute address, or pc-relative address.
 */
bool
opnd_is_far_memory_reference(opnd_t opnd);

DR_API
/**
 * Returns true iff \p opnd is a near memory reference operand
 * of any type: base-disp, absolute address, or pc-relative address.
 */
bool
opnd_is_near_memory_reference(opnd_t opnd);

/* accessor functions */

DR_API
/**
 * Return the data size of \p opnd as a OPSZ_ constant.
 * Returns OPSZ_NA if \p opnd does not have a valid size.
 * \note A register operand may have a size smaller than the full size
 * of its DR_REG_* register specifier.
 */
opnd_size_t
opnd_get_size(opnd_t opnd);

DR_API
/**
 * Sets the data size of \p opnd.
 * Assumes \p opnd is an immediate integer, a memory reference,
 * or an instr_t pointer address operand.
 */
void
opnd_set_size(opnd_t *opnd, opnd_size_t newsize);

DR_API
/**
 * Return the element size of \p opnd as a OPSZ_ constant.
 * Returns #OPSZ_NA if \p opnd does not have a valid size.
 */
opnd_size_t
opnd_get_vector_element_size(opnd_t opnd);

DR_API
/**
 * Assumes \p opnd is a register operand.
 * Returns the register it refers to (a DR_REG_ constant).
 */
reg_id_t
opnd_get_reg(opnd_t opnd);

DR_API
/**
 * Assumes \p opnd is a register operand, base+disp memory reference, or
 * an immediate integer.
 * Returns the flags describing additional properties of the register,
 * the index register or displacement component of the memory reference,
 * or the immediate operand \p opnd.
 */
dr_opnd_flags_t
opnd_get_flags(opnd_t opnd);

DR_API
/**
 * Assumes \p opnd is a register operand, base+disp memory reference, or
 * an immediate integer.
 * Sets the flags describing additional properties of the operand to \p flags.
 */
void
opnd_set_flags(opnd_t *opnd, dr_opnd_flags_t flags);

DR_API
/**
 * Assumes \p opnd is a register operand, base+disp memory reference, or
 * an immediate integer.
 * Sets the flags describing additional properties of the operand to
 * be the current flags plus the \p flags parameter and returns the
 * new operand value.
 */
opnd_t
opnd_add_flags(opnd_t opnd, dr_opnd_flags_t flags);

DR_API
/** Assumes opnd is an immediate integer and returns its value. */
ptr_int_t
opnd_get_immed_int(opnd_t opnd);

DR_API
/**
 * Assumes opnd is an immediate integer with DR_OPND_MULTI_PART set.
 * Returns its value.
 * \note 32-bit only.
 */
int64
opnd_get_immed_int64(opnd_t opnd);

DR_API
/**
 * Assumes \p opnd is an immediate float and returns its value.
 * The caller's code should use proc_save_fpstate() or be inside a
 * clean call that has requested to preserve the floating-point state.
 */
float
opnd_get_immed_float(opnd_t opnd);

#ifndef WINDOWS
/* XXX i#4488: x87 floating point immediates should be double precision.
 * Type double currently not included for Windows because sizeof(opnd_t) does
 * not equal EXPECTED_SIZEOF_OPND, triggering the ASSERT in d_r_arch_init().
 */
DR_API
/**
 * Assumes \p opnd is an immediate double and returns its value.
 * The caller's code should use proc_save_fpstate() or be inside a
 * clean call that has requested to preserve the floating-point state.
 */
double
opnd_get_immed_double(opnd_t opnd);
#endif

DR_API
/** Assumes \p opnd is a (near or far) program address and returns its value. */
app_pc
opnd_get_pc(opnd_t opnd);

DR_API
/**
 * Assumes \p opnd is a far program address.
 * Returns \p opnd's segment, a segment selector (not a DR_SEG_ constant).
 */
ushort
opnd_get_segment_selector(opnd_t opnd);

DR_API
/** Assumes \p opnd is an instr_t (near, far, or memory) operand and returns its value. */
instr_t *
opnd_get_instr(opnd_t opnd);

DR_API
/** Assumes \p opnd is a near instr_t operand and returns its shift value. */
ushort
opnd_get_shift(opnd_t opnd);

DR_API
/**
 * Assumes \p opnd is a memory instr operand.  Returns its displacement.
 */
short
opnd_get_mem_instr_disp(opnd_t opnd);

DR_API
/**
 * Assumes \p opnd is a (near or far) base+disp memory reference.  Returns the base
 * register (a DR_REG_ constant).
 */
reg_id_t
opnd_get_base(opnd_t opnd);

DR_API
/**
 * Assumes \p opnd is a (near or far) base+disp memory reference.
 * Returns the displacement.
 * On ARM, the displacement is always a non-negative value, and the
 * presence or absence of #DR_OPND_NEGATED in opnd_get_flags()
 * determines whether to add or subtract from the base register.
 */
int
opnd_get_disp(opnd_t opnd);

DR_API
/**
 * Assumes \p opnd is a (near or far) base+disp memory reference; returns whether
 * encode_zero_disp has been specified for \p opnd.
 */
bool
opnd_is_disp_encode_zero(opnd_t opnd);

DR_API
/**
 * Assumes \p opnd is a (near or far) base+disp memory reference; returns whether
 * force_full_disp has been specified for \p opnd.
 */
bool
opnd_is_disp_force_full(opnd_t opnd);

DR_API
/**
 * Assumes \p opnd is a (near or far) base+disp memory reference; returns whether
 * disp_short_addr has been specified for \p opnd.
 */
bool
opnd_is_disp_short_addr(opnd_t opnd);

DR_API
/**
 * Assumes \p opnd is a (near or far) base+disp memory reference.
 * Returns the index register (a DR_REG_ constant).
 */
reg_id_t
opnd_get_index(opnd_t opnd);

DR_API
/**
 * Assumes \p opnd is a (near or far) base+disp memory reference.  Returns the scale.
 * \note x86-only.  On ARM use opnd_get_index_shift().
 */
int
opnd_get_scale(opnd_t opnd);

DR_API
/**
 * Assumes \p opnd is a (near or far) memory reference of any type.
 * Returns \p opnd's segment (a DR_SEG_ constant), or DR_REG_NULL if it is a near
 * memory reference.
 */
reg_id_t
opnd_get_segment(opnd_t opnd);

#ifdef ARM
DR_API
/**
 * Assumes \p opnd is a (near or far) base+disp memory reference.
 * Returns DR_SHIFT_NONE if the index register is not shifted.
 * Returns the shift type and \p amount if the index register is shifted (this
 * shift will occur prior to being added to or subtracted from the base
 * register).
 * \note ARM-only.
 */
dr_shift_type_t
opnd_get_index_shift(opnd_t opnd, uint *amount OUT);

DR_API
/**
 * Assumes \p opnd is a near base+disp memory reference.
 * Sets the index register to be shifted by \p amount according to \p shift.
 * Returns whether successful.
 * If the shift amount is out of allowed ranges, returns false.
 * \note ARM-only.
 */
bool
opnd_set_index_shift(opnd_t *opnd, dr_shift_type_t shift, uint amount);
#endif /* ARM */

#ifdef AARCH64
DR_API
/**
 * Assumes \p opnd is a base+disp memory reference.
 * Returns the extension type, whether the offset is \p scaled, and the shift \p amount.
 * The register offset will be extended, then shifted, then added to the base register.
 * If there is no extension and no shift the values returned will be #DR_EXTEND_UXTX,
 * false, and zero.
 * \note AArch64-only.
 */
dr_extend_type_t
opnd_get_index_extend(opnd_t opnd, OUT bool *scaled, OUT uint *amount);

DR_API
/**
 * Assumes \p opnd is a base+disp memory reference.
 * Sets the index register to be extended by \p extend and optionally \p scaled.
 * Returns whether successful. If \p scaled is zero, the offset is not scaled.
 * \note AArch64-only.
 */
bool
opnd_set_index_extend_value(opnd_t *opnd, dr_extend_type_t extend, bool scaled,
                            uint scaled_value);

DR_API
/**
 * Assumes \p opnd is a base+disp memory reference.
 * Sets the index register to be extended by \p extend and optionally \p scaled.
 * Returns whether successful. If \p scaled is zero, the offset is not scaled; otherwise
 * is calculated from the operand size.
 * \note AArch64-only.
 */
bool
opnd_set_index_extend(opnd_t *opnd, dr_extend_type_t extend, bool scaled);
#endif /* AARCH64 */

DR_API
/**
 * Assumes \p opnd is a (near or far) absolute or pc-relative memory reference,
 * or a base+disp memory reference with no base or index register.
 * Returns \p opnd's absolute address (which will be pc-relativized on encoding
 * for pc-relative memory references).
 */
void *
opnd_get_addr(opnd_t opnd);

DR_API
/**
 * Returns the number of registers referred to by \p opnd. This will only
 * be non-zero for register operands and memory references.
 */
int
opnd_num_regs_used(opnd_t opnd);

DR_API
/**
 * Used in conjunction with opnd_num_regs_used(), this routine can be used
 * to iterate through all registers used by \p opnd.
 * The index values begin with 0 and proceed through opnd_num_regs_used(opnd)-1.
 */
reg_id_t
opnd_get_reg_used(opnd_t opnd, int index);

/* utility functions */

DR_API
/**
 * Assumes that \p reg is a DR_REG_ 32-bit register constant.
 * Returns the string name for \p reg.
 */
const char *
get_register_name(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ 32-bit register constant.
 * Returns the 16-bit version of \p reg.
 * \note x86-only.
 */
reg_id_t
reg_32_to_16(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ 32-bit register constant.
 * Returns the 8-bit version of \p reg (the least significant byte:
 * DR_REG_AL instead of DR_REG_AH if passed DR_REG_EAX, e.g.).  For 32-bit DR
 * builds, returns DR_REG_NULL if passed DR_REG_ESP, DR_REG_EBP, DR_REG_ESI, or
 * DR_REG_EDI.
 * \note x86-only.
 */
reg_id_t
reg_32_to_8(reg_id_t reg);

#ifdef X64
DR_API
/**
 * Assumes that \p reg is a DR_REG_ 32-bit register constant.
 * Returns the 64-bit version of \p reg.
 *
 * \note For 64-bit DR builds only.
 */
reg_id_t
reg_32_to_64(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ 64-bit register constant.
 * Returns the 32-bit version of \p reg.
 *
 * \note For 64-bit DR builds only.
 */
reg_id_t
reg_64_to_32(reg_id_t reg);

DR_API
/**
 * Returns true iff \p reg refers to an extended register only available in 64-bit
 * mode and not in 32-bit mode. For AVX-512, it also returns true for the upper 8
 * SIMD registers (e.g., R8-R15, XMM8-XMM15, XMM24-XMM31, ZMM24-ZMM31 etc.)
 *
 * \note For 64-bit DR builds only.
 */
bool
reg_is_extended(reg_id_t reg);

DR_API
/**
 * Returns true iff \p reg refers to an extended AVX-512 register only available
 * in 64-bit mode and not in 32-bit mode (e.g., XMM16-XMM31, ZMM16-ZMM31 etc.)
 *
 * \note For 64-bit DR builds only.
 */
bool
reg_is_avx512_extended(reg_id_t reg);
#endif

DR_API
/**
 * Assumes that \p reg is a DR_REG_ 32-bit register constant.
 * If \p sz == OPSZ_2, returns the 16-bit version of \p reg.
 * For 64-bit versions of this library, if \p sz == OPSZ_8, returns
 * the 64-bit version of \p reg.
 * Returns \p DR_REG_NULL when trying to get the 8-bit subregister of \p
 * DR_REG_ESI, \p DR_REG_EDI, \p DR_REG_EBP, or \p DR_REG_ESP in 32-bit mode.
 *
 * \deprecated Prefer reg_resize_to_opsz() which is more general.
 */
reg_id_t
reg_32_to_opsz(reg_id_t reg, opnd_size_t sz);

DR_API
/**
 * Given a general-purpose or SIMD register of any size, returns a register in the same
 * class of the given size.
 *
 * For example, given \p DR_REG_AX or \p DR_REG_RAX and \p OPSZ_1, this routine will
 * return \p DR_REG_AL. Given \p DR_REG_XMM0 and \p OPSZ_64, it will return \p
 * DR_REG_ZMM0.
 *
 * Returns \p DR_REG_NULL when trying to get the 8-bit subregister of \p
 * DR_REG_ESI, \p DR_REG_EDI, \p DR_REG_EBP, or \p DR_REG_ESP in 32-bit mode.
 * For 64-bit versions of this library, if \p sz == OPSZ_8, returns the 64-bit
 * version of \p reg.
 *
 * MMX registers are not yet supported.
 * Moreover, ARM is not yet supported for resizing SIMD registers.
 */
reg_id_t
reg_resize_to_opsz(reg_id_t reg, opnd_size_t sz);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ register constant.
 * If reg is used as part of the calling convention, returns which
 * parameter ordinal it matches (0-based); otherwise, returns -1.
 */
int
reg_parameter_num(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff it refers to a General Purpose Register,
 * i.e., rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, or a subset.
 */
bool
reg_is_gpr(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff it refers to a segment (i.e., it's really a DR_SEG_
 * constant).
 */
bool
reg_is_segment(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff it refers to a multimedia register used for
 * SIMD instructions.
 */
bool
reg_is_simd(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff it refers to an SSE or AVX register.
 * In particular, the register must be either an xmm, ymm, or
 * zmm for the function to return true.
 *
 * This function is subject to include any future vector register
 * that x86 may add.
 */
bool
reg_is_vector_simd(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff it refers to an xmm (128-bit SSE/SSE2) x86 register
 * or a ymm (256-bit multimedia) register.
 * \deprecated Prefer reg_is_strictly_xmm() || reg_is_strictly_ymm().
 */
bool
reg_is_xmm(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff it refers to an xmm (128-bit SSE/SSE2) x86 register.
 */
bool
reg_is_strictly_xmm(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff it refers to a ymm (256-bit multimedia) x86 register.
 * \deprecated Prefer reg_is_strictly_ymm().
 */
bool
reg_is_ymm(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff it refers to a ymm (256-bit multimedia) x86 register.
 */
bool
reg_is_strictly_ymm(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff it refers to a zmm (512-bit multimedia) x86 register.
 */
bool
reg_is_strictly_zmm(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff it refers to an opmask x86 register.
 */
bool
reg_is_opmask(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff it refers to an x86 MPX bounds register.
 */
bool
reg_is_bnd(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff it refers to an mmx (64-bit) register.
 */
bool
reg_is_mmx(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff it refers to a floating-point register.
 */
bool
reg_is_fp(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff it refers to a 32-bit general-purpose register.
 */
bool
reg_is_32bit(reg_id_t reg);

#if defined(AARCH64)
DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff it refers to a Z (SVE scalable vector) register.
 */
bool
reg_is_z(reg_id_t reg);
#endif

DR_API
/**
 * Returns true iff \p opnd is a register operand that refers to a 32-bit
 * general-purpose register.
 */
bool
opnd_is_reg_32bit(opnd_t opnd);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff it refers to a 64-bit general-purpose register.
 */
bool
reg_is_64bit(reg_id_t reg);

DR_API
/**
 * Returns true iff \p opnd is a register operand that refers to a 64-bit
 * general-purpose register.
 */
bool
opnd_is_reg_64bit(opnd_t opnd);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff it refers to a pointer-sized general-purpose register.
 */
bool
reg_is_pointer_sized(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ 32-bit register constant.
 * Returns the pointer-sized version of \p reg.
 */
reg_id_t
reg_to_pointer_sized(reg_id_t reg);

DR_API
/**
 * Returns true iff \p opnd is a register operand that refers to a
 * pointer-sized general-purpose register.
 */
bool
opnd_is_reg_pointer_sized(opnd_t opnd);

DR_API
/**
 * Assumes that \p r1 and \p r2 are both DR_REG_ constants.
 * Returns true iff \p r1's register overlaps \p r2's register
 * (e.g., if \p r1 == DR_REG_AX and \p r2 == DR_REG_EAX).
 */
bool
reg_overlap(reg_id_t r1, reg_id_t r2);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns \p reg's representation as 3 bits in a modrm byte
 * (the 3 bits are the lower-order bits in the return value).
 */
byte
reg_get_bits(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns the OPSZ_ constant corresponding to the register size.
 * Returns OPSZ_NA if reg is not a DR_REG_ constant.
 */
opnd_size_t
reg_get_size(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff \p opnd refers to reg directly or refers to a register
 * that overlaps \p reg (e.g., DR_REG_AX overlaps DR_REG_EAX).
 */
bool
opnd_uses_reg(opnd_t opnd, reg_id_t reg);

DR_API
/**
 * Set the displacement of a memory reference operand \p opnd to \p disp.
 * On ARM, a negative value for \p disp will be converted into a positive
 * value with #DR_OPND_NEGATED set in opnd_get_flags().
 */
void
opnd_set_disp(opnd_t *opnd, int disp);

#ifdef X86
DR_API
/**
 * Set the displacement and the encoding controls of a memory
 * reference operand:
 * - If \p encode_zero_disp, a zero value for \p disp will not be omitted;
 * - If \p force_full_disp, a small value for \p disp will not occupy only one byte.
 * - If \p disp_short_addr, short (16-bit for 32-bit mode, 32-bit for
 *    64-bit mode) addressing will be used (note that this normally only
 *    needs to be specified for an absolute address; otherwise, simply
 *    use the desired short registers for base and/or index).
 * \note x86-only.
 */
void
opnd_set_disp_ex(opnd_t *opnd, int disp, bool encode_zero_disp, bool force_full_disp,
                 bool disp_short_addr);
#endif

DR_API
/**
 * Assumes that both \p old_reg and \p new_reg are DR_REG_ constants.
 * Replaces all occurrences of \p old_reg in \p *opnd with \p new_reg.
 * Only replaces exact matches (use opnd_replace_reg_resize() to match
 * size variants).
 * Returns whether it replaced anything.
 */
bool
opnd_replace_reg(opnd_t *opnd, reg_id_t old_reg, reg_id_t new_reg);

DR_API
/**
 * Creates a reg incremented from an existing \p opnd by the \p increment value,
 * modulo the reg size.
 * Returns the new reg.
 */
opnd_t
opnd_create_increment_reg(opnd_t opnd, uint increment);

DR_API
/**
 * Replaces all instances of \p old_reg (or any size variant) in \p *opnd
 * with \p new_reg.  Resizes \p new_reg to match sub-full-size uses of \p old_reg.
 * Returns whether it replaced anything.
 */
bool
opnd_replace_reg_resize(opnd_t *opnd, reg_id_t old_reg, reg_id_t new_reg);

/* Arch-specific */
bool
opnd_same_sizes_ok(opnd_size_t s1, opnd_size_t s2, bool is_reg);

DR_API
/** Returns true iff \p op1 and \p op2 are indistinguishable.
 *  If either uses variable operand sizes, the default size is assumed.
 */
bool
opnd_same(opnd_t op1, opnd_t op2);

DR_API
/**
 * Returns true iff \p op1 and \p op2 are both memory references and they
 * are indistinguishable, ignoring data size.
 */
bool
opnd_same_address(opnd_t op1, opnd_t op2);

DR_API
/**
 * Returns true iff there exists some register that is referred to (directly
 * or overlapping) by both \p op1 and \p op2.
 */
bool
opnd_share_reg(opnd_t op1, opnd_t op2);

DR_API
/**
 * Returns true iff \p def, considered as a write, affects \p use.
 * Is conservative, so if both \p def and \p use are memory references,
 * will return true unless it can disambiguate them based on their
 * registers and displacement.
 */
bool
opnd_defines_use(opnd_t def, opnd_t use);

DR_API
/**
 * Assumes \p size is an OPSZ_ constant, typically obtained from
 * opnd_get_size() or reg_get_size().
 * Returns the number of bytes the OPSZ_ constant represents.
 * If OPSZ_ is a variable-sized size, returns the default size,
 * which may or may not match the actual size decided up on at
 * encoding time (that final size depends on other operands).
 */
uint
opnd_size_in_bytes(opnd_size_t size);

DR_API
/**
 * Assumes \p size is an OPSZ_ constant, typically obtained from
 * opnd_get_size() or reg_get_size().
 * Returns the number of bits the OPSZ_ constant represents.
 * If OPSZ_ is a variable-sized size, returns the default size,
 * which may or may not match the actual size decided up on at
 * encoding time (that final size depends on other operands).
 */
uint
opnd_size_in_bits(opnd_size_t size);

DR_API
/**
 * Returns the appropriate OPSZ_ constant for the given number of bytes.
 * Returns OPSZ_NA if there is no such constant.
 * The intended use case is something like "opnd_size_in_bytes(sizeof(foo))" for
 * integer/pointer types.  This routine returns simple single-size
 * types and will not return complex/variable size types.
 */
opnd_size_t
opnd_size_from_bytes(uint bytes);

DR_API
/**
 * Shrinks all 32-bit registers in \p opnd to their 16-bit versions.
 * Also shrinks the size of immediate integers and memory references from
 * OPSZ_4 to OPSZ_2.
 */
opnd_t
opnd_shrink_to_16_bits(opnd_t opnd);

#ifdef X64
DR_API
/**
 * Shrinks all 64-bit registers in \p opnd to their 32-bit versions.
 * Also shrinks the size of immediate integers and memory references from
 * OPSZ_8 to OPSZ_4.
 *
 * \note For 64-bit DR builds only.
 */
opnd_t
opnd_shrink_to_32_bits(opnd_t opnd);
#endif

DR_API
/**
 * Returns the value of the register \p reg, selected from the passed-in
 * register values.  Supports only general-purpose registers.
 * \p mc->flags must include DR_MC_CONTROL and DR_MC_INTEGER.
 */
reg_t
reg_get_value(reg_id_t reg, dr_mcontext_t *mc);

DR_API
/**
 * Returns the value of the register \p reg as stored in \p mc, or
 * for an mmx register as stored in the physical register.
 * Up to sizeof(dr_zmm_t) bytes will be written to \p val.
 *
 * This routine also supports reading AVX-512 mask registers. In this
 * case, sizeof(dr_opmask_t) bytes will be written to \p val.
 *
 * This routine does not support floating-point registers.
 *
 *
 * \note \p mc->flags must include the appropriate flag for the
 * requested register.
 */
bool
reg_get_value_ex(reg_id_t reg, dr_mcontext_t *mc, OUT byte *val);

DR_API
/**
 * Sets the register \p reg in the passed in mcontext \p mc to \p value.
 * \p mc->flags must include DR_MC_CONTROL and DR_MC_INTEGER.
 * \note This function is limited to setting pointer-sized registers only
 * (no sub-registers, and no non-general-purpose registers).
 * See \p reg_set_value_ex for setting other register values.
 */
void
reg_set_value(reg_id_t reg, dr_mcontext_t *mc, reg_t value);

DR_API
/**
 * Sets the register \p reg in the passed in mcontext \p mc to the value
 * stored in the buffer \p val_buf.
 *
 * \p mc->flags must include DR_MC_CONTROL and DR_MC_INTEGER.
 *
 * Unlike \p reg_set_value, this function supports not only general purpose
 * registers, but SIMD registers too. Does not yet support MMX registers.
 *
 * Up to sizeof(dr_zmm_t) bytes will be read from \p val_buf. It is up
 * to the user to ensure correct buffer size.
 *
 * Returns false if the register is not supported.
 */
bool
reg_set_value_ex(reg_id_t reg, dr_mcontext_t *mc, IN byte *val_buf);

DR_API
/**
 * Returns the effective address of \p opnd, computed using the passed-in
 * register values.  If \p opnd is a far address, ignores that aspect
 * except for TLS references on Windows (fs: for 32-bit, gs: for 64-bit)
 * or typical fs: or gs: references on Linux.  For far addresses the
 * calling thread's segment selector is used.
 * \p mc->flags must include DR_MC_CONTROL and DR_MC_INTEGER.
 *
 * \note This routine does not support vector addressing (via VSIB,
 * introduced in AVX2).  Use instr_compute_address(),
 * instr_compute_address_ex(), or instr_compute_address_ex_pos()
 * instead.
 */
app_pc
opnd_compute_address(opnd_t opnd, dr_mcontext_t *mc);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff \p reg is stolen by DynamoRIO for internal use.
 *
 * \note The register stolen by DynamoRIO may not be used by the client
 * for instrumentation. Use dr_insert_get_stolen_reg() and
 * dr_insert_set_stolen_reg() to get and set the application value of
 * the stolen register in the instrumentation.
 * Reference \ref sec_reg_stolen for more information.
 */
bool
reg_is_stolen(reg_id_t reg);

#endif /* _DR_IR_OPND_H_ */
