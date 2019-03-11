/* **********************************************************
 * Copyright (c) 2011-2019 Google, Inc.  All rights reserved.
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

/* file "opnd.h" -- opnd_t definitions and utilities */

#ifndef _OPND_H_
#define _OPND_H_ 1

#ifdef WINDOWS
/* disabled warning for
 *   "nonstandard extension used : bit field types other than int"
 * so we can use bitfields on our now-byte-sized reg_id_t type in opnd_t.
 */
#    pragma warning(disable : 4214)
#endif

/* to avoid changing all our internal REG_ constants we define this for DR itself */
#define DR_REG_ENUM_COMPATIBILITY 1

/* To avoid duplicating code we use our own exported macros, unless an includer
 * needs to avoid it.
 */
#ifdef DR_NO_FAST_IR
#    undef DR_FAST_IR
#    undef INSTR_INLINE
#else
#    define DR_FAST_IR 1
#endif

/* drpreinject.dll doesn't link in instr_shared.c so we can't include our inline
 * functions.  We want to use our inline functions for the standalone decoder
 * and everything else, so we single out drpreinject.
 */
#ifdef RC_IS_PRELOAD
#    undef DR_FAST_IR
#endif

/*************************
 ***      opnd_t       ***
 *************************/

/* DR_API EXPORT TOFILE dr_ir_opnd.h */
/* DR_API EXPORT BEGIN */
/****************************************************************************
 * OPERAND ROUTINES
 */
/**
 * @file dr_ir_opnd.h
 * @brief Functions and defines to create and manipulate instruction operands.
 */

/* DR_API EXPORT END */
/* DR_API EXPORT VERBATIM */
/* Catch conflicts if ucontext.h is included before us */
#if defined(DR_REG_ENUM_COMPATIBILITY) && (defined(REG_EAX) || defined(REG_RAX))
#    error REG_ enum conflict between DR and ucontext.h!  Use DR_REG_ constants instead.
#endif
/* DR_API EXPORT END */

/* If INSTR_INLINE is already defined, that means we've been included by
 * instr_shared.c, which wants to use C99 extern inline.  Otherwise, DR_FAST_IR
 * determines whether our instr routines are inlined.
 */
/* DR_API EXPORT BEGIN */
/* Inlining macro controls. */
#ifndef INSTR_INLINE
#    ifdef DR_FAST_IR
#        define INSTR_INLINE inline
#    else
#        define INSTR_INLINE
#    endif
#endif

#ifdef AVOID_API_EXPORT
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
#endif
/** Register identifiers. */
enum {
#ifdef AVOID_API_EXPORT
/* compiler gives weird errors for "REG_NONE" */
/* PR 227381: genapi.pl auto-inserts doxygen comments for lines without any! */
#endif
    DR_REG_NULL, /**< Sentinel value indicating no register, for address modes. */
#ifdef X86
    /* 64-bit general purpose */
    DR_REG_RAX,
    DR_REG_RCX,
    DR_REG_RDX,
    DR_REG_RBX,
    DR_REG_RSP,
    DR_REG_RBP,
    DR_REG_RSI,
    DR_REG_RDI,
    DR_REG_R8,
    DR_REG_R9,
    DR_REG_R10,
    DR_REG_R11,
    DR_REG_R12,
    DR_REG_R13,
    DR_REG_R14,
    DR_REG_R15,
    /* 32-bit general purpose */
    DR_REG_EAX,
    DR_REG_ECX,
    DR_REG_EDX,
    DR_REG_EBX,
    DR_REG_ESP,
    DR_REG_EBP,
    DR_REG_ESI,
    DR_REG_EDI,
    DR_REG_R8D,
    DR_REG_R9D,
    DR_REG_R10D,
    DR_REG_R11D,
    DR_REG_R12D,
    DR_REG_R13D,
    DR_REG_R14D,
    DR_REG_R15D,
    /* 16-bit general purpose */
    DR_REG_AX,
    DR_REG_CX,
    DR_REG_DX,
    DR_REG_BX,
    DR_REG_SP,
    DR_REG_BP,
    DR_REG_SI,
    DR_REG_DI,
    DR_REG_R8W,
    DR_REG_R9W,
    DR_REG_R10W,
    DR_REG_R11W,
    DR_REG_R12W,
    DR_REG_R13W,
    DR_REG_R14W,
    DR_REG_R15W,
    /* 8-bit general purpose */
    DR_REG_AL,
    DR_REG_CL,
    DR_REG_DL,
    DR_REG_BL,
    DR_REG_AH,
    DR_REG_CH,
    DR_REG_DH,
    DR_REG_BH,
    DR_REG_R8L,
    DR_REG_R9L,
    DR_REG_R10L,
    DR_REG_R11L,
    DR_REG_R12L,
    DR_REG_R13L,
    DR_REG_R14L,
    DR_REG_R15L,
    DR_REG_SPL,
    DR_REG_BPL,
    DR_REG_SIL,
    DR_REG_DIL,
    /* 64-BIT MMX */
    DR_REG_MM0,
    DR_REG_MM1,
    DR_REG_MM2,
    DR_REG_MM3,
    DR_REG_MM4,
    DR_REG_MM5,
    DR_REG_MM6,
    DR_REG_MM7,
    /* 128-BIT XMM */
    DR_REG_XMM0,
    DR_REG_XMM1,
    DR_REG_XMM2,
    DR_REG_XMM3,
    DR_REG_XMM4,
    DR_REG_XMM5,
    DR_REG_XMM6,
    DR_REG_XMM7,
    DR_REG_XMM8,
    DR_REG_XMM9,
    DR_REG_XMM10,
    DR_REG_XMM11,
    DR_REG_XMM12,
    DR_REG_XMM13,
    DR_REG_XMM14,
    DR_REG_XMM15,
    /* floating point registers */
    DR_REG_ST0,
    DR_REG_ST1,
    DR_REG_ST2,
    DR_REG_ST3,
    DR_REG_ST4,
    DR_REG_ST5,
    DR_REG_ST6,
    DR_REG_ST7,
    /* segments (order from "Sreg" description in Intel manual) */
    DR_SEG_ES,
    DR_SEG_CS,
    DR_SEG_SS,
    DR_SEG_DS,
    DR_SEG_FS,
    DR_SEG_GS,
    /* debug & control registers (privileged access only; 8-15 for future processors) */
    DR_REG_DR0,
    DR_REG_DR1,
    DR_REG_DR2,
    DR_REG_DR3,
    DR_REG_DR4,
    DR_REG_DR5,
    DR_REG_DR6,
    DR_REG_DR7,
    DR_REG_DR8,
    DR_REG_DR9,
    DR_REG_DR10,
    DR_REG_DR11,
    DR_REG_DR12,
    DR_REG_DR13,
    DR_REG_DR14,
    DR_REG_DR15,
    /* cr9-cr15 do not yet exist on current x64 hardware */
    DR_REG_CR0,
    DR_REG_CR1,
    DR_REG_CR2,
    DR_REG_CR3,
    DR_REG_CR4,
    DR_REG_CR5,
    DR_REG_CR6,
    DR_REG_CR7,
    DR_REG_CR8,
    DR_REG_CR9,
    DR_REG_CR10,
    DR_REG_CR11,
    DR_REG_CR12,
    DR_REG_CR13,
    DR_REG_CR14,
    DR_REG_CR15,
    DR_REG_INVALID, /**< Sentinel value indicating an invalid register. */

#    ifdef AVOID_API_EXPORT
/* Below here overlaps with OPSZ_ enum but all cases where the two
 * are used in the same field (instr_info_t operand sizes) have the type
 * and distinguish properly.
 */
#    endif
    /* 256-BIT YMM */
    DR_REG_YMM0,
    DR_REG_YMM1,
    DR_REG_YMM2,
    DR_REG_YMM3,
    DR_REG_YMM4,
    DR_REG_YMM5,
    DR_REG_YMM6,
    DR_REG_YMM7,
    DR_REG_YMM8,
    DR_REG_YMM9,
    DR_REG_YMM10,
    DR_REG_YMM11,
    DR_REG_YMM12,
    DR_REG_YMM13,
    DR_REG_YMM14,
    DR_REG_YMM15,

/****************************************************************************/
#elif defined(AARCHXX)
    DR_REG_INVALID, /**< Sentinel value indicating an invalid register. */

#    ifdef AARCH64
    /* 64-bit general purpose */
    DR_REG_X0,
    DR_REG_X1,
    DR_REG_X2,
    DR_REG_X3,
    DR_REG_X4,
    DR_REG_X5,
    DR_REG_X6,
    DR_REG_X7,
    DR_REG_X8,
    DR_REG_X9,
    DR_REG_X10,
    DR_REG_X11,
    DR_REG_X12,
    DR_REG_X13,
    DR_REG_X14,
    DR_REG_X15,
    DR_REG_X16,
    DR_REG_X17,
    DR_REG_X18,
    DR_REG_X19,
    DR_REG_X20,
    DR_REG_X21,
    DR_REG_X22,
    DR_REG_X23,
    DR_REG_X24,
    DR_REG_X25,
    DR_REG_X26,
    DR_REG_X27,
    DR_REG_X28,
    DR_REG_X29,
    DR_REG_X30,
    DR_REG_XSP, /* stack pointer: the last GPR */
    DR_REG_XZR, /* zero register: pseudo-register not included in GPRs */

    /* 32-bit general purpose */
    DR_REG_W0,
    DR_REG_W1,
    DR_REG_W2,
    DR_REG_W3,
    DR_REG_W4,
    DR_REG_W5,
    DR_REG_W6,
    DR_REG_W7,
    DR_REG_W8,
    DR_REG_W9,
    DR_REG_W10,
    DR_REG_W11,
    DR_REG_W12,
    DR_REG_W13,
    DR_REG_W14,
    DR_REG_W15,
    DR_REG_W16,
    DR_REG_W17,
    DR_REG_W18,
    DR_REG_W19,
    DR_REG_W20,
    DR_REG_W21,
    DR_REG_W22,
    DR_REG_W23,
    DR_REG_W24,
    DR_REG_W25,
    DR_REG_W26,
    DR_REG_W27,
    DR_REG_W28,
    DR_REG_W29,
    DR_REG_W30,
    DR_REG_WSP, /* bottom half of stack pointer */
    DR_REG_WZR, /* zero register */
#    else
    /* 32-bit general purpose */
    DR_REG_R0,
    DR_REG_R1,
    DR_REG_R2,
    DR_REG_R3,
    DR_REG_R4,
    DR_REG_R5,
    DR_REG_R6,
    DR_REG_R7,
    DR_REG_R8,
    DR_REG_R9,
    DR_REG_R10,
    DR_REG_R11,
    DR_REG_R12,
    DR_REG_R13,
    DR_REG_R14,
    DR_REG_R15,
#    endif

    /* 128-bit SIMD registers */
    DR_REG_Q0,
    DR_REG_Q1,
    DR_REG_Q2,
    DR_REG_Q3,
    DR_REG_Q4,
    DR_REG_Q5,
    DR_REG_Q6,
    DR_REG_Q7,
    DR_REG_Q8,
    DR_REG_Q9,
    DR_REG_Q10,
    DR_REG_Q11,
    DR_REG_Q12,
    DR_REG_Q13,
    DR_REG_Q14,
    DR_REG_Q15,
    /* x64-only but simpler code to not ifdef it */
    DR_REG_Q16,
    DR_REG_Q17,
    DR_REG_Q18,
    DR_REG_Q19,
    DR_REG_Q20,
    DR_REG_Q21,
    DR_REG_Q22,
    DR_REG_Q23,
    DR_REG_Q24,
    DR_REG_Q25,
    DR_REG_Q26,
    DR_REG_Q27,
    DR_REG_Q28,
    DR_REG_Q29,
    DR_REG_Q30,
    DR_REG_Q31,
    /* 64-bit SIMD registers */
    DR_REG_D0,
    DR_REG_D1,
    DR_REG_D2,
    DR_REG_D3,
    DR_REG_D4,
    DR_REG_D5,
    DR_REG_D6,
    DR_REG_D7,
    DR_REG_D8,
    DR_REG_D9,
    DR_REG_D10,
    DR_REG_D11,
    DR_REG_D12,
    DR_REG_D13,
    DR_REG_D14,
    DR_REG_D15,
    DR_REG_D16,
    DR_REG_D17,
    DR_REG_D18,
    DR_REG_D19,
    DR_REG_D20,
    DR_REG_D21,
    DR_REG_D22,
    DR_REG_D23,
    DR_REG_D24,
    DR_REG_D25,
    DR_REG_D26,
    DR_REG_D27,
    DR_REG_D28,
    DR_REG_D29,
    DR_REG_D30,
    DR_REG_D31,
    /* 32-bit SIMD registers */
    DR_REG_S0,
    DR_REG_S1,
    DR_REG_S2,
    DR_REG_S3,
    DR_REG_S4,
    DR_REG_S5,
    DR_REG_S6,
    DR_REG_S7,
    DR_REG_S8,
    DR_REG_S9,
    DR_REG_S10,
    DR_REG_S11,
    DR_REG_S12,
    DR_REG_S13,
    DR_REG_S14,
    DR_REG_S15,
    DR_REG_S16,
    DR_REG_S17,
    DR_REG_S18,
    DR_REG_S19,
    DR_REG_S20,
    DR_REG_S21,
    DR_REG_S22,
    DR_REG_S23,
    DR_REG_S24,
    DR_REG_S25,
    DR_REG_S26,
    DR_REG_S27,
    DR_REG_S28,
    DR_REG_S29,
    DR_REG_S30,
    DR_REG_S31,
    /* 16-bit SIMD registers */
    DR_REG_H0,
    DR_REG_H1,
    DR_REG_H2,
    DR_REG_H3,
    DR_REG_H4,
    DR_REG_H5,
    DR_REG_H6,
    DR_REG_H7,
    DR_REG_H8,
    DR_REG_H9,
    DR_REG_H10,
    DR_REG_H11,
    DR_REG_H12,
    DR_REG_H13,
    DR_REG_H14,
    DR_REG_H15,
    DR_REG_H16,
    DR_REG_H17,
    DR_REG_H18,
    DR_REG_H19,
    DR_REG_H20,
    DR_REG_H21,
    DR_REG_H22,
    DR_REG_H23,
    DR_REG_H24,
    DR_REG_H25,
    DR_REG_H26,
    DR_REG_H27,
    DR_REG_H28,
    DR_REG_H29,
    DR_REG_H30,
    DR_REG_H31,
    /* 8-bit SIMD registers */
    DR_REG_B0,
    DR_REG_B1,
    DR_REG_B2,
    DR_REG_B3,
    DR_REG_B4,
    DR_REG_B5,
    DR_REG_B6,
    DR_REG_B7,
    DR_REG_B8,
    DR_REG_B9,
    DR_REG_B10,
    DR_REG_B11,
    DR_REG_B12,
    DR_REG_B13,
    DR_REG_B14,
    DR_REG_B15,
    DR_REG_B16,
    DR_REG_B17,
    DR_REG_B18,
    DR_REG_B19,
    DR_REG_B20,
    DR_REG_B21,
    DR_REG_B22,
    DR_REG_B23,
    DR_REG_B24,
    DR_REG_B25,
    DR_REG_B26,
    DR_REG_B27,
    DR_REG_B28,
    DR_REG_B29,
    DR_REG_B30,
    DR_REG_B31,

#    ifndef AARCH64
    /* Coprocessor registers */
    DR_REG_CR0,
    DR_REG_CR1,
    DR_REG_CR2,
    DR_REG_CR3,
    DR_REG_CR4,
    DR_REG_CR5,
    DR_REG_CR6,
    DR_REG_CR7,
    DR_REG_CR8,
    DR_REG_CR9,
    DR_REG_CR10,
    DR_REG_CR11,
    DR_REG_CR12,
    DR_REG_CR13,
    DR_REG_CR14,
    DR_REG_CR15,
#    endif

/* We decided against DR_REG_RN_TH (top half), DR_REG_RN_BH (bottom half
 * for 32-bit as we have the W versions for 64-bit), and DR_REG_RN_BB
 * (bottom byte) as they are not available in the ISA and which portion
 * of a GPR is selected purely by the opcode.  Our decoder will create
 * a partial register for these to help tools, but it won't specify which
 * part of the register.
 */
#    ifdef AVOID_API_EXPORT
/* XXX i#1551: do we want to model the any-16-bits-of-Xn target
 * of OP_movk?
 */
#    endif

#    ifdef AVOID_API_EXPORT
/* Though on x86 we don't list eflags for even things like popf that write
 * other bits beyond aflags, here we do explicitly list cpsr and spsr for
 * OP_mrs and OP_msr to distinguish them and make things clearer.
 */
#    endif
#    ifdef AARCH64
    DR_REG_NZCV,
    DR_REG_FPCR,
    DR_REG_FPSR,
#    else
    DR_REG_CPSR,
    DR_REG_SPSR,
    DR_REG_FPSCR,
#    endif

    /* AArch32 Thread Registers */
    DR_REG_TPIDRURW, /**< User Read/Write Thread ID Register */
    DR_REG_TPIDRURO, /**< User Read-Only Thread ID Register */

#    ifdef AARCH64
    /* SVE vector registers */
    DR_REG_Z0,
    DR_REG_Z1,
    DR_REG_Z2,
    DR_REG_Z3,
    DR_REG_Z4,
    DR_REG_Z5,
    DR_REG_Z6,
    DR_REG_Z7,
    DR_REG_Z8,
    DR_REG_Z9,
    DR_REG_Z10,
    DR_REG_Z11,
    DR_REG_Z12,
    DR_REG_Z13,
    DR_REG_Z14,
    DR_REG_Z15,
    DR_REG_Z16,
    DR_REG_Z17,
    DR_REG_Z18,
    DR_REG_Z19,
    DR_REG_Z20,
    DR_REG_Z21,
    DR_REG_Z22,
    DR_REG_Z23,
    DR_REG_Z24,
    DR_REG_Z25,
    DR_REG_Z26,
    DR_REG_Z27,
    DR_REG_Z28,
    DR_REG_Z29,
    DR_REG_Z30,
    DR_REG_Z31,

    /* SVE predicate registers */
    DR_REG_P0,
    DR_REG_P1,
    DR_REG_P2,
    DR_REG_P3,
    DR_REG_P4,
    DR_REG_P5,
    DR_REG_P6,
    DR_REG_P7,
    DR_REG_P8,
    DR_REG_P9,
    DR_REG_P10,
    DR_REG_P11,
    DR_REG_P12,
    DR_REG_P13,
    DR_REG_P14,
    DR_REG_P15,
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
    DR_REG_CP15_C13_2 = DR_REG_TPIDRURW, /**< User Read/Write Thread ID Register */
    DR_REG_CP15_C13_3 = DR_REG_TPIDRURO, /**< User Read-Olny Thread ID Register */

#    ifdef AARCH64
    DR_REG_LAST_VALID_ENUM = DR_REG_P15, /**< Last valid register enum */
    DR_REG_LAST_ENUM = DR_REG_P15,       /**< Last value of register enums */
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

    DR_NUM_GPR_REGS = DR_REG_STOP_GPR - DR_REG_START_GPR + 1,

#    ifndef AARCH64
    /** Platform-independent way to refer to stack pointer. */
    DR_REG_XSP = DR_REG_SP,
#    endif

#endif /* X86/ARM */
};

/* we avoid typedef-ing the enum, as its storage size is compiler-specific */
typedef ushort reg_id_t; /**< The type of a DR_REG_ enum value. */
/* For x86 we do store reg_id_t here, but the x86 DR_REG_ enum is small enough
 * (checked in d_r_arch_init().
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

/* DR_API EXPORT END */
/* indexed by enum */
extern const char *const reg_names[];
extern const reg_id_t dr_reg_fixer[];
/* DR_API EXPORT BEGIN */

#ifdef X86 /* We don't need these for ARM which uses clear numbering */
#    define DR_REG_START_GPR DR_REG_XAX /**< Start of general register enum values */
#    ifdef X64
#        define DR_REG_STOP_GPR DR_REG_R15 /**< End of general register enum values */
#    else
#        define DR_REG_STOP_GPR DR_REG_XDI /**< End of general register enum values */
#    endif
/** Number of general registers */
#    define DR_NUM_GPR_REGS (DR_REG_STOP_GPR - DR_REG_START_GPR + 1)
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
#    define DR_REG_START_XMM DR_REG_XMM0 /**< Start of xmm register enum values */
#    define DR_REG_STOP_XMM DR_REG_XMM15 /**< End of xmm register enum values */
#    define DR_REG_START_YMM DR_REG_YMM0 /**< Start of ymm register enum values */
#    define DR_REG_STOP_YMM DR_REG_YMM15 /**< End of ymm register enum values */
#    define DR_REG_START_FLOAT \
        DR_REG_ST0 /**< Start of floating-point-register enum values*/
#    define DR_REG_STOP_FLOAT \
        DR_REG_ST7 /**< End of floating-point-register enum values */
#    define DR_REG_START_SEGMENT DR_SEG_ES /**< Start of segment register enum values */
#    define DR_REG_STOP_SEGMENT DR_SEG_GS  /**< End of segment register enum values */
#    define DR_REG_START_DR DR_REG_DR0     /**< Start of debug register enum values */
#    define DR_REG_STOP_DR DR_REG_DR15     /**< End of debug register enum values */
#    define DR_REG_START_CR DR_REG_CR0     /**< Start of control register enum values */
#    define DR_REG_STOP_CR DR_REG_CR15     /**< End of control register enum values */
/**
 * Last valid register enum value.  Note: DR_REG_INVALID is now smaller
 * than this value.
 */
#    define DR_REG_LAST_VALID_ENUM DR_REG_YMM15
#    define DR_REG_LAST_ENUM DR_REG_YMM15 /**< Last value of register enums */
#endif                                    /* X86 */
/* DR_API EXPORT END */

#ifdef X86
#    define REG_START_SPILL DR_REG_XAX
#    define REG_STOP_SPILL DR_REG_XDI
#elif defined(AARCHXX)
/* We only normally use r0-r3 but we support more in translation code */
#    define REG_START_SPILL DR_REG_R0
#    define REG_STOP_SPILL DR_REG_R10 /* r10 might be used in syscall mangling */
#endif                                /* X86/ARM */
#define REG_SPILL_NUM (REG_STOP_SPILL - REG_START_SPILL + 1)

/* DR_API EXPORT VERBATIM */
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
#    define REG_START_YMM DR_REG_START_YMM
#    define REG_STOP_YMM DR_REG_STOP_YMM
#    define REG_START_XMM DR_REG_START_XMM
#    define REG_STOP_XMM DR_REG_STOP_XMM
#    define REG_START_FLOAT DR_REG_START_FLOAT
#    define REG_STOP_FLOAT DR_REG_STOP_FLOAT
#    define REG_START_SEGMENT DR_REG_START_SEGMENT
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
/* DR_API EXPORT END */

#ifndef INT8_MIN
#    define INT8_MIN SCHAR_MIN
#    define INT8_MAX SCHAR_MAX
#    define INT16_MIN SHRT_MIN
#    define INT16_MAX SHRT_MAX
#    define INT32_MIN INT_MIN
#    define INT32_MAX INT_MAX
#endif

/* typedef is in globals.h */
/* deliberately NOT adding doxygen comments to opnd_t fields b/c users
 * should use our macros
 */
/* DR_API EXPORT BEGIN */

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
    DR_EXTEND_UXTB = 0, /**< Unsigned extend byte. */
    DR_EXTEND_UXTH,     /**< Unsigned extend halfword. */
    DR_EXTEND_UXTW,     /**< Unsigned extend word. */
    DR_EXTEND_UXTX,     /**< Unsigned extend doubleword (a no-op). */
    DR_EXTEND_SXTB,     /**< Signed extend byte. */
    DR_EXTEND_SXTH,     /**< Signed extend halfword. */
    DR_EXTEND_SXTW,     /**< Signed extend word. */
    DR_EXTEND_SXTX,     /**< Signed extend doubleword (a no-op). */
} dr_extend_type_t;

/**
 * These flags describe operations performed on the value of a source register
 * before it is combined with other sources as part of the behavior of the
 * containing instruction, or operations performed on an index register or
 * displacement before it is added to or subtracted from the base register.
 */
typedef enum _dr_opnd_flags_t {
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
        reg_id_t reg;   /* REG_kind */
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
            /* Shift register offset left by amount implied by size of memory operand: */
            byte /*bool*/ scaled : 1;
#    elif defined(ARM)
            byte /*dr_shift_type_t*/ shift_type : 3;
            byte shift_amount_minus_1 : 5; /* 1..31 so we store (val - 1) */
#    elif defined(X86)
            byte scale : SCALE_SPECIFIER_BITS;
            byte /*bool*/ encode_zero_disp : 1;
            byte /*bool*/ force_full_disp : 1; /* don't use 8-bit even w/ 8-bit value */
            byte /*bool*/ disp_short_addr : 1; /* 16-bit (32 in x64) addr (disp-only) */
#    endif
        } base_disp; /* BASE_DISP_kind */
        void *addr;  /* REL_ADDR_kind and ABS_ADDR_kind */
    } value;
};
#endif /* DR_FAST_IR */
/* DR_API EXPORT END */
#ifndef DR_FAST_IR
struct _opnd_t {
#    ifdef X64
    uint black_box_uint;
    uint64 black_box_uint64;
#    else
    uint black_box_uint[3];
#    endif
};
#endif

/* We assert that our fields are packed properly in d_r_arch_init().
 * We could use #pragma pack to shrink x64 back down to 12 bytes (it's at 16
 * b/c the struct is aligned to its max field align which is 8), but
 * probably not much gain since in either case it's passed/returned as a pointer
 * and the temp memory allocated is 16-byte aligned (on Windows; for
 * Linux it is passed in two consecutive registers I believe, but
 * still 12 bytes vs 16 makes no difference).
 */
#define EXPECTED_SIZEOF_OPND (3 * sizeof(uint) IF_X64(+4 /*struct size padding*/))

/* deliberately NOT adding doxygen comments b/c users should use our macros */
/* DR_API EXPORT BEGIN */
#ifdef DR_FAST_IR
/** x86 operand kinds */
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
    LAST_kind, /* sentinal; not a valid opnd kind */
};
#endif /* DR_FAST_IR */
/* DR_API EXPORT END */

/* functions to build an operand */

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
 * On x86, \p r must be a multimedia (mmx, xmm, or ymm) register.  For
 * partial general-purpose registers on x86, use the appropriate
 * sub-register name with opnd_create_reg() instead.
 */
opnd_t
opnd_create_reg_partial(reg_id_t r, opnd_size_t subsize);

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
 * Returns an immediate float operand with value \p f.
 * The caller's code should use proc_save_fpstate() or be inside a
 * clean call that has requested to preserve the floating-point state.
 */
opnd_t
opnd_create_immed_float(float f);

/* not exported */
opnd_t
opnd_create_immed_float_for_opcode(uint opcode);

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
 * (The encoding optimization flags are all false when using opnd_create_base_disp()).
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
 * Returns a memory reference operand that refers to either a base
 * register with a constant displacement:
 * - [base_reg, disp]
 *
 * Or a base register plus an optionally extended and shifted index register:
 * - [base_reg, index_reg, extend_type, shift_amount]
 *
 * The shift_amount is zero or, if \p scaled, a value determined by the
 * size of the operand.
 *
 * The resulting operand has data size \p size (must be an OPSZ_ constant).
 * Both \p base_reg and \p index_reg must be DR_REG_ constants.
 * Either \p index_reg must be #DR_REG_NULL or disp must be 0.
 *
 * \note AArch64-only.
 */
opnd_t
opnd_create_base_disp_aarch64(reg_id_t base_reg, reg_id_t index_reg,
                              dr_extend_type_t extend_type, bool scaled, int disp,
                              dr_opnd_flags_t flags, opnd_size_t size);
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

/* DR_API EXPORT BEGIN */
#if defined(X64) || defined(ARM)
/* DR_API EXPORT END */
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
/* DR_API EXPORT BEGIN */
#endif /* X64 || ARM */
/* DR_API EXPORT END */

/* predicate functions */

/* Check if the operand kind and size fields are valid */
bool
opnd_is_valid(opnd_t opnd);

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

/* DR_API EXPORT BEGIN */
#if defined(X64) || defined(ARM)
/* DR_API EXPORT END */
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
/* DR_API EXPORT BEGIN */
#endif
/* DR_API EXPORT END */

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
 * Returns whether successful. If the offset is scaled the amount it is shifted
 * by is determined by the size of the memory operand.
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

/* DR_API EXPORT BEGIN */
#ifdef X64
/* DR_API EXPORT END */
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
 * Returns true iff \p reg refers to an extended register only available
 * in 64-bit mode and not in 32-bit mode (e.g., R8-R15, XMM8-XMM15, etc.)
 *
 * \note For 64-bit DR builds only.
 */
bool
reg_is_extended(reg_id_t reg);
/* DR_API EXPORT BEGIN */
#endif
/* DR_API EXPORT END */

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
 * Given a general-purpose register of any size, returns a register in the same
 * class of the given size.  For example, given \p DR_REG_AX or \p DR_REG_RAX
 * and \p OPSZ_1, this routine will return \p DR_REG_AL.
 * Returns \p DR_REG_NULL when trying to get the 8-bit subregister of \p
 * DR_REG_ESI, \p DR_REG_EDI, \p DR_REG_EBP, or \p DR_REG_ESP in 32-bit mode.
 * For 64-bit versions of this library, if \p sz == OPSZ_8, returns the 64-bit
 * version of \p reg.
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
 * Returns true iff it refers to an xmm (128-bit SSE/SSE2) register
 * or a ymm (256-bit multimedia) register.
 */
bool
reg_is_xmm(reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff it refers to a ymm (256-bit multimedia) register.
 */
bool
reg_is_ymm(reg_id_t reg);

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

/* not exported */
int
opnd_get_reg_dcontext_offs(reg_id_t reg);

int
opnd_get_reg_mcontext_offs(reg_id_t reg);

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
 */
bool
opnd_replace_reg(opnd_t *opnd, reg_id_t old_reg, reg_id_t new_reg);

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

/* DR_API EXPORT BEGIN */
#ifdef X64
/* DR_API EXPORT END */
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
/* DR_API EXPORT BEGIN */
#endif
/* DR_API EXPORT END */

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
 * Up to sizeof(dr_ymm_t) bytes will be written to \p val.
 *
 * This routine does not support floating-point registers.
 *
 * \note \p mc->flags must include the appropriate flag for the
 * requested register.
 */
bool
reg_get_value_ex(reg_id_t reg, dr_mcontext_t *mc, OUT byte *val);

/* internal version */
reg_t
reg_get_value_priv(reg_id_t reg, priv_mcontext_t *mc);

DR_API
/**
 * Sets the register \p reg in the passed in mcontext \p mc to \p value.
 * \p mc->flags must include DR_MC_CONTROL and DR_MC_INTEGER.
 * \note Current release is limited to setting pointer-sized registers only
 * (no sub-registers, and no non-general-purpose registers).
 */
void
reg_set_value(reg_id_t reg, dr_mcontext_t *mc, reg_t value);

/* internal version */
void
reg_set_value_priv(reg_id_t reg, priv_mcontext_t *mc, reg_t value);

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

/* internal version */
app_pc
opnd_compute_address_priv(opnd_t opnd, priv_mcontext_t *mc);

app_pc
opnd_compute_address_helper(opnd_t opnd, priv_mcontext_t *mc, ptr_int_t scaled_index);

bool
opnd_is_abs_base_disp(opnd_t opnd);

#ifndef STANDALONE_DECODER
opnd_t
opnd_create_dcontext_field(dcontext_t *dcontext, int offs);
opnd_t
opnd_create_dcontext_field_byte(dcontext_t *dcontext, int offs);
opnd_t
opnd_create_dcontext_field_sz(dcontext_t *dcontext, int offs, opnd_size_t sz);

/* basereg, if left as REG_NULL, is assumed to be xdi (xsi for upcontext) */
opnd_t
opnd_create_dcontext_field_via_reg_sz(dcontext_t *dcontext, reg_id_t basereg, int offs,
                                      opnd_size_t sz);
opnd_t
opnd_create_dcontext_field_via_reg(dcontext_t *dcontext, reg_id_t basereg, int offs);
opnd_t
update_dcontext_address(opnd_t op, dcontext_t *old_dcontext, dcontext_t *new_dcontext);
opnd_t
opnd_create_tls_slot(int offs);
/* For size, use a OPSZ_ value from decode.h, typically OPSZ_1 or OPSZ_4 */
opnd_t
opnd_create_sized_tls_slot(int offs, opnd_size_t size);
#endif /* !STANDALONE_DECODER */

/* This should be kept in sync w/ the defines in x86/x86.asm */
enum {
#ifdef X86
#    ifdef X64
#        ifdef UNIX
    /* SysV ABI calling convention */
    NUM_REGPARM = 6,
    REGPARM_0 = REG_RDI,
    REGPARM_1 = REG_RSI,
    REGPARM_2 = REG_RDX,
    REGPARM_3 = REG_RCX,
    REGPARM_4 = REG_R8,
    REGPARM_5 = REG_R9,
    REGPARM_MINSTACK = 0,
    REDZONE_SIZE = 128,
#        else
    /* Intel/Microsoft calling convention */
    NUM_REGPARM = 4,
    REGPARM_0 = REG_RCX,
    REGPARM_1 = REG_RDX,
    REGPARM_2 = REG_R8,
    REGPARM_3 = REG_R9,
    REGPARM_MINSTACK = 4 * sizeof(XSP_SZ),
    REDZONE_SIZE = 0,
#        endif
    /* In fact, for Windows the stack pointer is supposed to be
     * 16-byte aligned at all times except in a prologue or epilogue.
     * The prologue will always adjust by 16*n+8 since push of retaddr
     * always makes stack pointer not 16-byte aligned.
     */
    REGPARM_END_ALIGN = 16,
#    else
    NUM_REGPARM = 0,
    REGPARM_MINSTACK = 0,
    REDZONE_SIZE = 0,
#        ifdef MACOS
    REGPARM_END_ALIGN = 16,
#        else
    REGPARM_END_ALIGN = sizeof(XSP_SZ),
#        endif
#    endif /* 64/32 */
#elif defined(AARCHXX)
    REGPARM_0 = DR_REG_R0,
    REGPARM_1 = DR_REG_R1,
    REGPARM_2 = DR_REG_R2,
    REGPARM_3 = DR_REG_R3,
#    ifdef X64
    REGPARM_4 = DR_REG_R4,
    REGPARM_5 = DR_REG_R5,
    REGPARM_6 = DR_REG_R6,
    REGPARM_7 = DR_REG_R7,
    NUM_REGPARM = 8,
#    else
    NUM_REGPARM = 4,
#    endif /* 64/32 */
    REDZONE_SIZE = 0,
    REGPARM_MINSTACK = 0,
    REGPARM_END_ALIGN = 8,
#endif
};
extern const reg_id_t d_r_regparms[];

/* arch-specific */
uint
opnd_immed_float_arch(uint opcode);

#ifdef AARCHXX
#    define DR_REG_STOLEN_MIN IF_X64_ELSE(DR_REG_X9, DR_REG_R8) /* DR_REG_SYSNUM + 1 */
#    define DR_REG_STOLEN_MAX IF_X64_ELSE(DR_REG_X29, DR_REG_R12)
/* DR's stolen register for TLS access */
extern reg_id_t dr_reg_stolen;
#endif

#endif /* _OPND_H_ */
