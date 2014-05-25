/* **********************************************************
 * Copyright (c) 2011-2014 Google, Inc.  All rights reserved.
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

/* file "instr.h" -- x86-specific instr_t definitions and utilities */

#ifndef _INSTR_H_
#define _INSTR_H_ 1

#ifdef WINDOWS
/* disabled warning for
 *   "nonstandard extension used : bit field types other than int"
 * so we can use bitfields on our now-byte-sized reg_id_t type in opnd_t.
 */
# pragma warning( disable : 4214)
#endif

/* to avoid changing all our internal REG_ constants we define this for DR itself */
#define DR_REG_ENUM_COMPATIBILITY 1

/* to avoid duplicating code we use our own exported macros */
#define DR_FAST_IR 1

/* drpreinject.dll doesn't link in instr.c so we can't include our inline
 * functions.  We want to use our inline functions for the standalone decoder
 * and everything else, so we single out drpreinject.
 */
#ifdef RC_IS_PRELOAD
# undef DR_FAST_IR
#endif

/* can't include decode.h, it includes us, just declare struct */
struct instr_info_t;

/* The machine-specific IR consists of instruction lists,
   instructions, and operands.  You can find the instrlist_t stuff in
   the upper-level directory (shared infrastructure).  The
   declarations and interface functions (which insulate the system
   from the specifics of each constructs implementation) for opnd_t and
   instr_t appear below. */


/*************************
 ***       opnd_t        ***
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
# error REG_ enum conflict between DR and ucontext.h!  Use DR_REG_ constants instead.
#endif
/* DR_API EXPORT END */

/* If INSTR_INLINE is already defined, that means we've been included by
 * instr.c, which wants to use C99 extern inline.  Otherwise, DR_FAST_IR
 * determines whether our instr routines are inlined.
 */
/* DR_API EXPORT BEGIN */
/* Inlining macro controls. */
#ifndef INSTR_INLINE
# ifdef DR_FAST_IR
#  define INSTR_INLINE inline
# else
#  define INSTR_INLINE
# endif
#endif

#ifdef AVOID_API_EXPORT
/* We encode this enum plus the OPSZ_ extensions in bytes, so we can have
 * at most 256 total DR_REG_ plus OPSZ_ values.  Currently there are 165-odd.
 * Decoder assumes 32-bit, 16-bit, and 8-bit are in specific order
 * corresponding to modrm encodings.
 * We also assume that the DR_SEG_ constants are invalid as pointers for
 * our use in instr_info_t.code.
 * Also, reg_names array in encode.c corresponds to this enum order.
 * Plus, dr_reg_fixer array in instr.c.
 * Lots of optimizations assume same ordering of registers among
 * 32, 16, and 8  i.e. eax same position (first) in each etc.
 * reg_rm_selectable() assumes the GPR registers, mmx, and xmm are all in a row.
 */
#endif
enum {
#ifdef AVOID_API_EXPORT
    /* compiler gives weird errors for "REG_NONE" */
    /* PR 227381: genapi.pl auto-inserts doxygen comments for lines without any! */
#endif
    DR_REG_NULL, /**< Sentinel value indicating no register, for address modes. */
    /* 64-bit general purpose */
    DR_REG_RAX,  DR_REG_RCX,  DR_REG_RDX,  DR_REG_RBX,
    DR_REG_RSP,  DR_REG_RBP,  DR_REG_RSI,  DR_REG_RDI,
    DR_REG_R8,   DR_REG_R9,   DR_REG_R10,  DR_REG_R11,
    DR_REG_R12,  DR_REG_R13,  DR_REG_R14,  DR_REG_R15,
    /* 32-bit general purpose */
    DR_REG_EAX,  DR_REG_ECX,  DR_REG_EDX,  DR_REG_EBX,
    DR_REG_ESP,  DR_REG_EBP,  DR_REG_ESI,  DR_REG_EDI,
    DR_REG_R8D,  DR_REG_R9D,  DR_REG_R10D, DR_REG_R11D,
    DR_REG_R12D, DR_REG_R13D, DR_REG_R14D, DR_REG_R15D,
    /* 16-bit general purpose */
    DR_REG_AX,   DR_REG_CX,   DR_REG_DX,   DR_REG_BX,
    DR_REG_SP,   DR_REG_BP,   DR_REG_SI,   DR_REG_DI,
    DR_REG_R8W,  DR_REG_R9W,  DR_REG_R10W, DR_REG_R11W,
    DR_REG_R12W, DR_REG_R13W, DR_REG_R14W, DR_REG_R15W,
    /* 8-bit general purpose */
    DR_REG_AL,   DR_REG_CL,   DR_REG_DL,   DR_REG_BL,
    DR_REG_AH,   DR_REG_CH,   DR_REG_DH,   DR_REG_BH,
    DR_REG_R8L,  DR_REG_R9L,  DR_REG_R10L, DR_REG_R11L,
    DR_REG_R12L, DR_REG_R13L, DR_REG_R14L, DR_REG_R15L,
    DR_REG_SPL,  DR_REG_BPL,  DR_REG_SIL,  DR_REG_DIL,
    /* 64-BIT MMX */
    DR_REG_MM0,  DR_REG_MM1,  DR_REG_MM2,  DR_REG_MM3,
    DR_REG_MM4,  DR_REG_MM5,  DR_REG_MM6,  DR_REG_MM7,
    /* 128-BIT XMM */
    DR_REG_XMM0, DR_REG_XMM1, DR_REG_XMM2, DR_REG_XMM3,
    DR_REG_XMM4, DR_REG_XMM5, DR_REG_XMM6, DR_REG_XMM7,
    DR_REG_XMM8, DR_REG_XMM9, DR_REG_XMM10,DR_REG_XMM11,
    DR_REG_XMM12,DR_REG_XMM13,DR_REG_XMM14,DR_REG_XMM15,
    /* floating point registers */
    DR_REG_ST0,  DR_REG_ST1,  DR_REG_ST2,  DR_REG_ST3,
    DR_REG_ST4,  DR_REG_ST5,  DR_REG_ST6,  DR_REG_ST7,
    /* segments (order from "Sreg" description in Intel manual) */
    DR_SEG_ES,   DR_SEG_CS,   DR_SEG_SS,   DR_SEG_DS,   DR_SEG_FS,   DR_SEG_GS,
    /* debug & control registers (privileged access only; 8-15 for future processors) */
    DR_REG_DR0,  DR_REG_DR1,  DR_REG_DR2,  DR_REG_DR3,
    DR_REG_DR4,  DR_REG_DR5,  DR_REG_DR6,  DR_REG_DR7,
    DR_REG_DR8,  DR_REG_DR9,  DR_REG_DR10, DR_REG_DR11,
    DR_REG_DR12, DR_REG_DR13, DR_REG_DR14, DR_REG_DR15,
    /* cr9-cr15 do not yet exist on current x64 hardware */
    DR_REG_CR0,  DR_REG_CR1,  DR_REG_CR2,  DR_REG_CR3,
    DR_REG_CR4,  DR_REG_CR5,  DR_REG_CR6,  DR_REG_CR7,
    DR_REG_CR8,  DR_REG_CR9,  DR_REG_CR10, DR_REG_CR11,
    DR_REG_CR12, DR_REG_CR13, DR_REG_CR14, DR_REG_CR15,
    DR_REG_INVALID, /**< Sentinel value indicating an invalid register. */

#ifdef AVOID_API_EXPORT
    /* Below here overlaps with OPSZ_ enum but all cases where the two
     * are used in the same field (instr_info_t operand sizes) have the type
     * and distinguish properly.
     */
#endif
    /* 256-BIT YMM */
    DR_REG_YMM0, DR_REG_YMM1, DR_REG_YMM2, DR_REG_YMM3,
    DR_REG_YMM4, DR_REG_YMM5, DR_REG_YMM6, DR_REG_YMM7,
    DR_REG_YMM8, DR_REG_YMM9, DR_REG_YMM10,DR_REG_YMM11,
    DR_REG_YMM12,DR_REG_YMM13,DR_REG_YMM14,DR_REG_YMM15,
};

/* we avoid typedef-ing the enum, as its storage size is compiler-specific */
typedef byte reg_id_t; /* contains a DR_REG_ enum value */
typedef byte opnd_size_t; /* contains a DR_REG_ or OPSZ_ enum value */

/* Platform-independent full-register specifiers */
#ifdef X64
# define DR_REG_XAX DR_REG_RAX  /**< Platform-independent way to refer to rax/eax. */
# define DR_REG_XCX DR_REG_RCX  /**< Platform-independent way to refer to rcx/ecx. */
# define DR_REG_XDX DR_REG_RDX  /**< Platform-independent way to refer to rdx/edx. */
# define DR_REG_XBX DR_REG_RBX  /**< Platform-independent way to refer to rbx/ebx. */
# define DR_REG_XSP DR_REG_RSP  /**< Platform-independent way to refer to rsp/esp. */
# define DR_REG_XBP DR_REG_RBP  /**< Platform-independent way to refer to rbp/ebp. */
# define DR_REG_XSI DR_REG_RSI  /**< Platform-independent way to refer to rsi/esi. */
# define DR_REG_XDI DR_REG_RDI  /**< Platform-independent way to refer to rdi/edi. */
#else
# define DR_REG_XAX DR_REG_EAX  /**< Platform-independent way to refer to rax/eax. */
# define DR_REG_XCX DR_REG_ECX  /**< Platform-independent way to refer to rcx/ecx. */
# define DR_REG_XDX DR_REG_EDX  /**< Platform-independent way to refer to rdx/edx. */
# define DR_REG_XBX DR_REG_EBX  /**< Platform-independent way to refer to rbx/ebx. */
# define DR_REG_XSP DR_REG_ESP  /**< Platform-independent way to refer to rsp/esp. */
# define DR_REG_XBP DR_REG_EBP  /**< Platform-independent way to refer to rbp/ebp. */
# define DR_REG_XSI DR_REG_ESI  /**< Platform-independent way to refer to rsi/esi. */
# define DR_REG_XDI DR_REG_EDI  /**< Platform-independent way to refer to rdi/edi. */
#endif

/* DR_API EXPORT END */
/* indexed by enum */
extern const char * const reg_names[];
extern const reg_id_t dr_reg_fixer[];
/* DR_API EXPORT BEGIN */

#define DR_REG_START_GPR DR_REG_XAX /**< Start of general register enum values */
#ifdef X64
# define DR_REG_STOP_GPR DR_REG_R15 /**< End of general register enum values */
#else
# define DR_REG_STOP_GPR DR_REG_XDI /**< End of general register enum values */
#endif
/**< Number of general registers */
#define DR_NUM_GPR_REGS (DR_REG_STOP_GPR - DR_REG_START_GPR + 1)
#define DR_REG_START_64    DR_REG_RAX  /**< Start of 64-bit general register enum values */
#define DR_REG_STOP_64     DR_REG_R15  /**< End of 64-bit general register enum values */
#define DR_REG_START_32    DR_REG_EAX  /**< Start of 32-bit general register enum values */
#define DR_REG_STOP_32     DR_REG_R15D /**< End of 32-bit general register enum values */
#define DR_REG_START_16    DR_REG_AX   /**< Start of 16-bit general register enum values */
#define DR_REG_STOP_16     DR_REG_R15W /**< End of 16-bit general register enum values */
#define DR_REG_START_8     DR_REG_AL   /**< Start of 8-bit general register enum values */
#define DR_REG_STOP_8      DR_REG_DIL  /**< End of 8-bit general register enum values */
#define DR_REG_START_8HL   DR_REG_AL   /**< Start of 8-bit high-low register enum values */
#define DR_REG_STOP_8HL    DR_REG_BH   /**< End of 8-bit high-low register enum values */
#define DR_REG_START_x86_8 DR_REG_AH   /**< Start of 8-bit x86-only register enum values */
#define DR_REG_STOP_x86_8  DR_REG_BH   /**< Stop of 8-bit x86-only register enum values */
#define DR_REG_START_x64_8 DR_REG_SPL  /**< Start of 8-bit x64-only register enum values */
#define DR_REG_STOP_x64_8  DR_REG_DIL  /**< Stop of 8-bit x64-only register enum values */
#define DR_REG_START_MMX   DR_REG_MM0  /**< Start of mmx register enum values */
#define DR_REG_STOP_MMX    DR_REG_MM7  /**< End of mmx register enum values */
#define DR_REG_START_XMM   DR_REG_XMM0 /**< Start of xmm register enum values */
#define DR_REG_STOP_XMM    DR_REG_XMM15/**< End of xmm register enum values */
#define DR_REG_START_YMM   DR_REG_YMM0 /**< Start of ymm register enum values */
#define DR_REG_STOP_YMM    DR_REG_YMM15/**< End of ymm register enum values */
#define DR_REG_START_FLOAT DR_REG_ST0  /**< Start of floating-point-register enum values */
#define DR_REG_STOP_FLOAT  DR_REG_ST7  /**< End of floating-point-register enum values */
#define DR_REG_START_SEGMENT DR_SEG_ES /**< Start of segment register enum values */
#define DR_REG_STOP_SEGMENT  DR_SEG_GS /**< End of segment register enum values */
#define DR_REG_START_DR    DR_REG_DR0  /**< Start of debug register enum values */
#define DR_REG_STOP_DR     DR_REG_DR15 /**< End of debug register enum values */
#define DR_REG_START_CR    DR_REG_CR0  /**< Start of control register enum values */
#define DR_REG_STOP_CR     DR_REG_CR15 /**< End of control register enum values */
/**
 * Last valid register enum value.  Note: DR_REG_INVALID is now smaller
 * than this value.
 */
#define DR_REG_LAST_VALID_ENUM DR_REG_YMM15
#define DR_REG_LAST_ENUM   DR_REG_YMM15 /**< Last value of register enums */
/* DR_API EXPORT END */
#define REG_START_SPILL   DR_REG_XAX
#define REG_STOP_SPILL    DR_REG_XDI
#define REG_SPILL_NUM     (REG_STOP_SPILL - REG_START_SPILL + 1)

/* DR_API EXPORT VERBATIM */
/* Backward compatibility with REG_ constants (we now use DR_REG_ to avoid
 * conflicts with the REG_ enum in <sys/ucontext.h>: i#34).
 * Clients should set(DynamoRIO_REG_COMPATIBILITY ON) prior to
 * configure_DynamoRIO_client() to set this define.
 */
#ifdef DR_REG_ENUM_COMPATIBILITY
# define REG_NULL            DR_REG_NULL
# define REG_RAX             DR_REG_RAX
# define REG_RCX             DR_REG_RCX
# define REG_RDX             DR_REG_RDX
# define REG_RBX             DR_REG_RBX
# define REG_RSP             DR_REG_RSP
# define REG_RBP             DR_REG_RBP
# define REG_RSI             DR_REG_RSI
# define REG_RDI             DR_REG_RDI
# define REG_R8              DR_REG_R8
# define REG_R9              DR_REG_R9
# define REG_R10             DR_REG_R10
# define REG_R11             DR_REG_R11
# define REG_R12             DR_REG_R12
# define REG_R13             DR_REG_R13
# define REG_R14             DR_REG_R14
# define REG_R15             DR_REG_R15
# define REG_EAX             DR_REG_EAX
# define REG_ECX             DR_REG_ECX
# define REG_EDX             DR_REG_EDX
# define REG_EBX             DR_REG_EBX
# define REG_ESP             DR_REG_ESP
# define REG_EBP             DR_REG_EBP
# define REG_ESI             DR_REG_ESI
# define REG_EDI             DR_REG_EDI
# define REG_R8D             DR_REG_R8D
# define REG_R9D             DR_REG_R9D
# define REG_R10D            DR_REG_R10D
# define REG_R11D            DR_REG_R11D
# define REG_R12D            DR_REG_R12D
# define REG_R13D            DR_REG_R13D
# define REG_R14D            DR_REG_R14D
# define REG_R15D            DR_REG_R15D
# define REG_AX              DR_REG_AX
# define REG_CX              DR_REG_CX
# define REG_DX              DR_REG_DX
# define REG_BX              DR_REG_BX
# define REG_SP              DR_REG_SP
# define REG_BP              DR_REG_BP
# define REG_SI              DR_REG_SI
# define REG_DI              DR_REG_DI
# define REG_R8W             DR_REG_R8W
# define REG_R9W             DR_REG_R9W
# define REG_R10W            DR_REG_R10W
# define REG_R11W            DR_REG_R11W
# define REG_R12W            DR_REG_R12W
# define REG_R13W            DR_REG_R13W
# define REG_R14W            DR_REG_R14W
# define REG_R15W            DR_REG_R15W
# define REG_AL              DR_REG_AL
# define REG_CL              DR_REG_CL
# define REG_DL              DR_REG_DL
# define REG_BL              DR_REG_BL
# define REG_AH              DR_REG_AH
# define REG_CH              DR_REG_CH
# define REG_DH              DR_REG_DH
# define REG_BH              DR_REG_BH
# define REG_R8L             DR_REG_R8L
# define REG_R9L             DR_REG_R9L
# define REG_R10L            DR_REG_R10L
# define REG_R11L            DR_REG_R11L
# define REG_R12L            DR_REG_R12L
# define REG_R13L            DR_REG_R13L
# define REG_R14L            DR_REG_R14L
# define REG_R15L            DR_REG_R15L
# define REG_SPL             DR_REG_SPL
# define REG_BPL             DR_REG_BPL
# define REG_SIL             DR_REG_SIL
# define REG_DIL             DR_REG_DIL
# define REG_MM0             DR_REG_MM0
# define REG_MM1             DR_REG_MM1
# define REG_MM2             DR_REG_MM2
# define REG_MM3             DR_REG_MM3
# define REG_MM4             DR_REG_MM4
# define REG_MM5             DR_REG_MM5
# define REG_MM6             DR_REG_MM6
# define REG_MM7             DR_REG_MM7
# define REG_XMM0            DR_REG_XMM0
# define REG_XMM1            DR_REG_XMM1
# define REG_XMM2            DR_REG_XMM2
# define REG_XMM3            DR_REG_XMM3
# define REG_XMM4            DR_REG_XMM4
# define REG_XMM5            DR_REG_XMM5
# define REG_XMM6            DR_REG_XMM6
# define REG_XMM7            DR_REG_XMM7
# define REG_XMM8            DR_REG_XMM8
# define REG_XMM9            DR_REG_XMM9
# define REG_XMM10           DR_REG_XMM10
# define REG_XMM11           DR_REG_XMM11
# define REG_XMM12           DR_REG_XMM12
# define REG_XMM13           DR_REG_XMM13
# define REG_XMM14           DR_REG_XMM14
# define REG_XMM15           DR_REG_XMM15
# define REG_ST0             DR_REG_ST0
# define REG_ST1             DR_REG_ST1
# define REG_ST2             DR_REG_ST2
# define REG_ST3             DR_REG_ST3
# define REG_ST4             DR_REG_ST4
# define REG_ST5             DR_REG_ST5
# define REG_ST6             DR_REG_ST6
# define REG_ST7             DR_REG_ST7
# define SEG_ES              DR_SEG_ES
# define SEG_CS              DR_SEG_CS
# define SEG_SS              DR_SEG_SS
# define SEG_DS              DR_SEG_DS
# define SEG_FS              DR_SEG_FS
# define SEG_GS              DR_SEG_GS
# define REG_DR0             DR_REG_DR0
# define REG_DR1             DR_REG_DR1
# define REG_DR2             DR_REG_DR2
# define REG_DR3             DR_REG_DR3
# define REG_DR4             DR_REG_DR4
# define REG_DR5             DR_REG_DR5
# define REG_DR6             DR_REG_DR6
# define REG_DR7             DR_REG_DR7
# define REG_DR8             DR_REG_DR8
# define REG_DR9             DR_REG_DR9
# define REG_DR10            DR_REG_DR10
# define REG_DR11            DR_REG_DR11
# define REG_DR12            DR_REG_DR12
# define REG_DR13            DR_REG_DR13
# define REG_DR14            DR_REG_DR14
# define REG_DR15            DR_REG_DR15
# define REG_CR0             DR_REG_CR0
# define REG_CR1             DR_REG_CR1
# define REG_CR2             DR_REG_CR2
# define REG_CR3             DR_REG_CR3
# define REG_CR4             DR_REG_CR4
# define REG_CR5             DR_REG_CR5
# define REG_CR6             DR_REG_CR6
# define REG_CR7             DR_REG_CR7
# define REG_CR8             DR_REG_CR8
# define REG_CR9             DR_REG_CR9
# define REG_CR10            DR_REG_CR10
# define REG_CR11            DR_REG_CR11
# define REG_CR12            DR_REG_CR12
# define REG_CR13            DR_REG_CR13
# define REG_CR14            DR_REG_CR14
# define REG_CR15            DR_REG_CR15
# define REG_INVALID         DR_REG_INVALID
# define REG_XAX             DR_REG_XAX
# define REG_XCX             DR_REG_XCX
# define REG_XDX             DR_REG_XDX
# define REG_XBX             DR_REG_XBX
# define REG_XSP             DR_REG_XSP
# define REG_XBP             DR_REG_XBP
# define REG_XSI             DR_REG_XSI
# define REG_XDI             DR_REG_XDI
# define REG_START_64        DR_REG_START_64
# define REG_STOP_64         DR_REG_STOP_64
# define REG_START_32        DR_REG_START_32
# define REG_STOP_32         DR_REG_STOP_32
# define REG_START_16        DR_REG_START_16
# define REG_STOP_16         DR_REG_STOP_16
# define REG_START_8         DR_REG_START_8
# define REG_STOP_8          DR_REG_STOP_8
# define REG_START_8HL       DR_REG_START_8HL
# define REG_STOP_8HL        DR_REG_STOP_8HL
# define REG_START_x86_8     DR_REG_START_x86_8
# define REG_STOP_x86_8      DR_REG_STOP_x86_8
# define REG_START_x64_8     DR_REG_START_x64_8
# define REG_STOP_x64_8      DR_REG_STOP_x64_8
# define REG_START_MMX       DR_REG_START_MMX
# define REG_STOP_MMX        DR_REG_STOP_MMX
# define REG_START_YMM       DR_REG_START_YMM
# define REG_STOP_YMM        DR_REG_STOP_YMM
# define REG_START_XMM       DR_REG_START_XMM
# define REG_STOP_XMM        DR_REG_STOP_XMM
# define REG_START_FLOAT     DR_REG_START_FLOAT
# define REG_STOP_FLOAT      DR_REG_STOP_FLOAT
# define REG_START_SEGMENT   DR_REG_START_SEGMENT
# define REG_STOP_SEGMENT    DR_REG_STOP_SEGMENT
# define REG_START_DR        DR_REG_START_DR
# define REG_STOP_DR         DR_REG_STOP_DR
# define REG_START_CR        DR_REG_START_CR
# define REG_STOP_CR         DR_REG_STOP_CR
# define REG_LAST_VALID_ENUM DR_REG_LAST_VALID_ENUM
# define REG_LAST_ENUM       DR_REG_LAST_ENUM
# define REG_YMM0            DR_REG_YMM0
# define REG_YMM1            DR_REG_YMM1
# define REG_YMM2            DR_REG_YMM2
# define REG_YMM3            DR_REG_YMM3
# define REG_YMM4            DR_REG_YMM4
# define REG_YMM5            DR_REG_YMM5
# define REG_YMM6            DR_REG_YMM6
# define REG_YMM7            DR_REG_YMM7
# define REG_YMM8            DR_REG_YMM8
# define REG_YMM9            DR_REG_YMM9
# define REG_YMM10           DR_REG_YMM10
# define REG_YMM11           DR_REG_YMM11
# define REG_YMM12           DR_REG_YMM12
# define REG_YMM13           DR_REG_YMM13
# define REG_YMM14           DR_REG_YMM14
# define REG_YMM15           DR_REG_YMM15
#endif /* DR_REG_ENUM_COMPATIBILITY */
/* DR_API EXPORT END */

#ifndef INT8_MIN
# define INT8_MIN   SCHAR_MIN
# define INT8_MAX   SCHAR_MAX
# define INT16_MIN  SHRT_MIN
# define INT16_MAX  SHRT_MAX
# define INT32_MIN  INT_MIN
# define INT32_MAX  INT_MAX
#endif

/* typedef is in globals.h */
/* deliberately NOT adding doxygen comments to opnd_t fields b/c users
 * should use our macros
 */
/* DR_API EXPORT BEGIN */

#ifdef DR_FAST_IR

# define REG_SPECIFIER_BITS 8
# define SCALE_SPECIFIER_BITS 4

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
                                                * and ABS_ADDR_kind */
        ushort disp;           /* MEM_INSTR_kind */
        ushort shift;          /* INSTR_kind */
    } seg;
    union {
        /* all are 64 bits or less */
        /* NULL_kind has no value */
        ptr_int_t immed_int;   /* IMMED_INTEGER_kind */
        float immed_float;     /* IMMED_FLOAT_kind */
        /* PR 225937: today we provide no way of specifying a 16-bit immediate
         * (encoded as a data16 prefix, which also implies a 16-bit EIP,
         * making it only useful for far pcs)
         */
        app_pc pc;             /* PC_kind and FAR_PC_kind */
        /* For FAR_PC_kind and FAR_INSTR_kind, we use pc/instr, and keep the
         * segment selector (which is NOT a DR_SEG_ constant) in far_pc_seg_selector
         * above, to save space.
         */
        instr_t *instr;         /* INSTR_kind, FAR_INSTR_kind, and MEM_INSTR_kind */
        reg_id_t reg;           /* REG_kind */
        struct {
            int disp;
            reg_id_t base_reg : REG_SPECIFIER_BITS;
            reg_id_t index_reg : REG_SPECIFIER_BITS;
            /* to get cl to not align to 4 bytes we can't use uint here
             * when we have reg_id_t elsewhere: it won't combine them
             * (gcc will). alternative is all uint and no reg_id_t. */
            byte scale : SCALE_SPECIFIER_BITS;
            byte/*bool*/ encode_zero_disp : 1;
            byte/*bool*/ force_full_disp : 1; /* don't use 8-bit even w/ 8-bit value */
            byte/*bool*/ disp_short_addr : 1; /* 16-bit (32 in x64) addr (disp-only) */
        } base_disp;            /* BASE_DISP_kind */
        void *addr;             /* REL_ADDR_kind and ABS_ADDR_kind */
    } value;
};
#endif /* DR_FAST_IR */
/* DR_API EXPORT END */
#ifndef DR_FAST_IR
struct _opnd_t {
# ifdef X64
    uint black_box_uint;
    uint64 black_box_uint64;
# else
    uint black_box_uint[3];
# endif
};
#endif

/* We assert that our fields are packed properly in arch_init().
 * We could use #pragma pack to shrink x64 back down to 12 bytes (it's at 16
 * b/c the struct is aligned to its max field align which is 8), but
 * probably not much gain since in either case it's passed/returned as a pointer
 * and the temp memory allocated is 16-byte aligned (on Windows; for
 * Linux it is passed in two consecutive registers I believe, but
 * still 12 bytes vs 16 makes no difference).
 */
#define EXPECTED_SIZEOF_OPND (3*sizeof(uint) IF_X64(+4/*struct size padding*/))

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
#ifdef X64
    REL_ADDR_kind,  /* pc-relative address: x64 only */
    ABS_ADDR_kind,  /* 64-bit absolute address: x64 only */
#endif
    MEM_INSTR_kind,
    LAST_kind,      /* sentinal; not a valid opnd kind */
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
 * Returns a register operand corresponding to a part of the multimedia
 * register represented by the DR_REG_ constant \p r, which must be
 * an mmx, xmm, or ymm register.  For partial general-purpose registers,
 * use the appropriate sub-register name with opnd_create_reg() instead.
 */
opnd_t
opnd_create_reg_partial(reg_id_t r, opnd_size_t subsize);

DR_API
/**
 * Returns an immediate integer operand with value \p i and size
 * \p data_size; \p data_size must be a OPSZ_ constant.
 */
opnd_t
opnd_create_immed_int(ptr_int_t i, opnd_size_t data_size);

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
 * \p scale must be either 1, 2, 4, or 8.
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
 * \p scale must be either 1, 2, 4, or 8.
 * Gives control over encoding optimizations:
 * -# If \p encode_zero_disp, a zero value for disp will not be omitted;
 * -# If \p force_full_disp, a small value for disp will not occupy only one byte.
 * -# If \p disp_short_addr, short (16-bit for 32-bit mode, 32-bit for
 *    64-bit mode) addressing will be used (note that this normally only
 *    needs to be specified for an absolute address; otherwise, simply
 *    use the desired short registers for base and/or index).
 *
 * (Both of those are false when using opnd_create_base_disp()).
 */
opnd_t
opnd_create_base_disp_ex(reg_id_t base_reg, reg_id_t index_reg, int scale,
                         int disp, opnd_size_t size,
                         bool encode_zero_disp, bool force_full_disp,
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
 * \p scale must be either 1, 2, 4, or 8.
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
 * The operand has data size \p data_size (must be a OPSZ_ constant).
 * \p seg must be a DR_SEG_ constant.
 * Both \p base_reg and \p index_reg must be DR_REG_ constants.
 * scale must be either 1, 2, 4, or 8.
 * Gives control over encoding optimizations:
 * -# If \p encode_zero_disp, a zero value for disp will not be omitted;
 * -# If \p force_full_disp, a small value for disp will not occupy only one byte.
 * -# If \p disp_short_addr, short (16-bit for 32-bit mode, 32-bit for
 *    64-bit mode) addressing will be used (note that this normally only
 *    needs to be specified for an absolute address; otherwise, simply
 *    use the desired short registers for base and/or index).
 *
 * (All of these are false when using opnd_create_far_base_disp()).
 */
opnd_t
opnd_create_far_base_disp_ex(reg_id_t seg, reg_id_t base_reg, reg_id_t index_reg,
                             int scale, int disp, opnd_size_t size,
                             bool encode_zero_disp, bool force_full_disp,
                             bool disp_short_addr);

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
#ifdef X64
/* DR_API EXPORT END */
DR_API
/**
 * Returns a memory reference operand that refers to the address \p
 * addr, but will be encoded as a pc-relative address.  At emit time,
 * if \p addr is out of reach of a 32-bit signed displacement from the
 * next instruction, encoding will fail.
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
 * absolute form will be used (as though opnd_create_abs_addr() had
 * been called).
 *
 * The operand has data size data_size (must be a OPSZ_ constant).
 *
 * To represent a 32-bit address (i.e., what an address size prefix
 * indicates), simply zero out the top 32 bits of the address before
 * passing it to this routine.
 *
 * \note For 64-bit DR builds only.
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
 * \note For 64-bit DR builds only.
 */
opnd_t
opnd_create_far_rel_addr(reg_id_t seg, void *addr, opnd_size_t data_size);
/* DR_API EXPORT BEGIN */
#endif
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
#ifdef X64
/* DR_API EXPORT END */
DR_API
/**
 * Returns true iff \p opnd is a (near or far) pc-relative memory reference operand.
 *
 * \note For 64-bit DR builds only.
 */
bool
opnd_is_rel_addr(opnd_t opnd);

DR_API
INSTR_INLINE
/**
 * Returns true iff \p opnd is a near (i.e., default segment) pc-relative memory
 * reference operand.
 *
 * \note For 64-bit DR builds only.
 */
bool
opnd_is_near_rel_addr(opnd_t opnd);

DR_API
INSTR_INLINE
/**
 * Returns true iff \p opnd is a far pc-relative memory reference operand.
 *
 * \note For 64-bit DR builds only.
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
/** Assumes opnd is an immediate integer, returns its value. */
ptr_int_t
opnd_get_immed_int(opnd_t opnd);

DR_API
/**
 * Assumes \p opnd is an immediate float and returns its value.
 * The caller's code should use proc_save_fpstate() or be inside a
 * clean call that has requested to preserve the floating-point state.
 */
float
opnd_get_immed_float(opnd_t opnd);

DR_API
/** Assumes \p opnd is a (near or far) program address, returns its value. */
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
instr_t*
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
/** Assumes \p opnd is a (near or far) base+disp memory reference.  Returns the scale. */
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

#ifdef DEBUG
void
reg_check_reg_fixer(void);
#endif

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
/** Set the displacement of a memory reference operand \p opnd to \p disp. */
void
opnd_set_disp(opnd_t *opnd, int disp);

DR_API
/**
 * Set the displacement and encoding controls of a memory reference operand:
 * - If \p encode_zero_disp, a zero value for \p disp will not be omitted;
 * - If \p force_full_disp, a small value for \p disp will not occupy only one byte.
 * -# If \p disp_short_addr, short (16-bit for 32-bit mode, 32-bit for
 *    64-bit mode) addressing will be used (note that this normally only
 *    needs to be specified for an absolute address; otherwise, simply
 *    use the desired short registers for base and/or index).
 */
void
opnd_set_disp_ex(opnd_t *opnd, int disp, bool encode_zero_disp, bool force_full_disp,
                 bool disp_short_addr);

DR_API
/**
 * Assumes that both \p old_reg and \p new_reg are DR_REG_ constants.
 * Replaces all occurrences of \p old_reg in \p *opnd with \p new_reg.
 */
bool
opnd_replace_reg(opnd_t *opnd, reg_id_t old_reg, reg_id_t new_reg);

DR_API
/** Returns true iff \p op1 and \p op2 are indistinguishable.
 *  If either uses variable operand sizes, the default size is assumed.
 */
bool
opnd_same(opnd_t op1,opnd_t op2);

DR_API
/**
 * Returns true iff \p op1 and \p op2 are both memory references and they
 * are indistinguishable, ignoring data size.
 */
bool
opnd_same_address(opnd_t op1,opnd_t op2);

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
 * Assumes \p size is a OPSZ_ or a DR_REG_ constant.
 * If \p size is a DR_REG_ constant, first calls reg_get_size(\p size)
 * to get a OPSZ_ constant that assumes the entire register is used.
 * Returns the number of bytes the OPSZ_ constant represents.
 * If OPSZ_ is a variable-sized size, returns the default size,
 * which may or may not match the actual size decided up on at
 * encoding time (that final size depends on other operands).
 */
uint
opnd_size_in_bytes(opnd_size_t size);

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

/* internal version */
app_pc
opnd_compute_address_priv(opnd_t opnd, priv_mcontext_t *mc);


/*************************
 ***       instr_t       ***
 *************************/

/* instr_t structure
 * An instruction represented by instr_t can be in a number of states,
 * depending on whether it points to raw bits that are valid,
 * whether its operand and opcode fields are up to date, and whether
 * its eflags field is up to date.
 * Invariant: if opcode == OP_UNDECODED, raw bits should be valid.
 *            if opcode == OP_INVALID, raw bits may point to real bits,
 *              but they are not a valid instruction stream.
 *
 * CORRESPONDENCE WITH CGO LEVELS
 * Level 0 = raw bits valid, !opcode_valid, decode_sizeof(instr) != instr->len
 *   opcode_valid is equivalent to opcode != OP_INVALID && opcode != OP_UNDECODED
 * Level 1 = raw bits valid, !opcode_valid, decode_sizeof(instr) == instr->len
 * Level 2 = raw bits valid, opcode_valid, !operands_valid
 *   (eflags info is auto-derived on demand so not an issue)
 * Level 3 = raw bits valid, operands valid
 *   (we assume that if operands_valid then opcode_valid)
 * Level 4 = !raw bits valid, operands valid
 *
 * Independent of these is whether its raw bits were allocated for
 * the instr or not.
 */

/* DR_API EXPORT TOFILE dr_ir_instr.h */
/* For inlining, we need to expose some of these flags.  We bracket the ones we
 * want in export begin/end.  AVOID_API_EXPORT does not work because there are
 * nested ifdefs.
 */
/* DR_API EXPORT BEGIN */
#ifdef DR_FAST_IR
/* flags */
enum {
/* DR_API EXPORT END */
    /* these first flags are shared with the LINK_ flags and are
     * used to pass on info to link stubs
     */
    /* used to determine type of indirect branch for exits */
    INSTR_DIRECT_EXIT           = LINK_DIRECT,
    INSTR_INDIRECT_EXIT         = LINK_INDIRECT,
    INSTR_RETURN_EXIT           = LINK_RETURN,
    /* JMP|CALL marks an indirect jmp preceded by a call (== a PLT-style ind call)
     * so use EXIT_IS_{JMP,CALL} rather than these raw bits
     */
    INSTR_CALL_EXIT             = LINK_CALL,
    INSTR_JMP_EXIT              = LINK_JMP,
    INSTR_IND_JMP_PLT_EXIT      = (INSTR_JMP_EXIT | INSTR_CALL_EXIT),
    INSTR_FAR_EXIT              = LINK_FAR,
    INSTR_BRANCH_SPECIAL_EXIT   = LINK_SPECIAL_EXIT,
#ifdef UNSUPPORTED_API
    INSTR_BRANCH_TARGETS_PREFIX = LINK_TARGET_PREFIX,
#endif
#ifdef X64
    /* PR 257963: since we don't store targets of ind branches, we need a flag
     * so we know whether this is a trace cmp exit, which has its own ibl entry
     */
    INSTR_TRACE_CMP_EXIT        = LINK_TRACE_CMP,
#endif
#ifdef WINDOWS
    INSTR_CALLBACK_RETURN       = LINK_CALLBACK_RETURN,
#else
    INSTR_NI_SYSCALL_INT        = LINK_NI_SYSCALL_INT,
#endif
    INSTR_NI_SYSCALL            = LINK_NI_SYSCALL,
    INSTR_NI_SYSCALL_ALL        = LINK_NI_SYSCALL_ALL,
    /* meta-flag */
    EXIT_CTI_TYPES              = (INSTR_DIRECT_EXIT | INSTR_INDIRECT_EXIT |
                                   INSTR_RETURN_EXIT | INSTR_CALL_EXIT |
                                   INSTR_JMP_EXIT |
                                   INSTR_FAR_EXIT |
                                   INSTR_BRANCH_SPECIAL_EXIT |
#ifdef UNSUPPORTED_API
                                   INSTR_BRANCH_TARGETS_PREFIX |
#endif
#ifdef X64
                                   INSTR_TRACE_CMP_EXIT |
#endif
#ifdef WINDOWS
                                   INSTR_CALLBACK_RETURN |
#else
                                   INSTR_NI_SYSCALL_INT |
#endif
                                   INSTR_NI_SYSCALL),

    /* instr_t-internal flags (not shared with LINK_) */
    INSTR_OPERANDS_VALID        = 0x00010000,
    /* meta-flag */
    INSTR_FIRST_NON_LINK_SHARED_FLAG = INSTR_OPERANDS_VALID,
    INSTR_EFLAGS_VALID          = 0x00020000,
    INSTR_EFLAGS_6_VALID        = 0x00040000,
    INSTR_RAW_BITS_VALID        = 0x00080000,
    INSTR_RAW_BITS_ALLOCATED    = 0x00100000,
/* DR_API EXPORT BEGIN */
    INSTR_DO_NOT_MANGLE         = 0x00200000,
/* DR_API EXPORT END */
    INSTR_HAS_CUSTOM_STUB       = 0x00400000,
    /* used to indicate that an indirect call can be treated as a direct call */
    INSTR_IND_CALL_DIRECT       = 0x00800000,
#ifdef WINDOWS
    /* used to indicate that a syscall should be executed via shared syscall */
    INSTR_SHARED_SYSCALL        = 0x01000000,
#endif

#ifdef CLIENT_INTERFACE
    INSTR_CLOBBER_RETADDR       = 0x02000000,
#endif

    /* Signifies that this instruction may need to be hot patched and should
     * therefore not cross a cache line. It is not necessary to set this for
     * exit cti's or linkstubs since it is mainly intended for clients etc.
     * Handling of this flag is not yet implemented */
    INSTR_HOT_PATCHABLE         = 0x04000000,
#ifdef DEBUG
    /* case 9151: only report invalid instrs for normal code decoding */
    INSTR_IGNORE_INVALID        = 0x08000000,
#endif
    /* Currently used for frozen coarse fragments with final jmps and
     * jmps to ib stubs that are elided: we need the jmp instr there
     * to build the linkstub_t but we do not want to emit it. */
    INSTR_DO_NOT_EMIT           = 0x10000000,
    /* PR 251479: re-relativization support: is instr->rip_rel_pos valid? */
    INSTR_RIP_REL_VALID         = 0x20000000,
#ifdef X64
    /* PR 278329: each instr stores its own x64/x86 mode */
    INSTR_X86_MODE              = 0x40000000,
#endif
    /* PR 267260: distinguish our own mangling from client-added instrs */
    INSTR_OUR_MANGLING          = 0x80000000,
/* DR_API EXPORT BEGIN */
};
#endif /* DR_FAST_IR */

/**
 * Data slots available in a label (instr_create_label()) instruction
 * for storing client-controlled data.  Accessible via
 * instr_get_label_data_area().
 */
typedef struct _dr_instr_label_data_t {
    ptr_uint_t data[4]; /**< Generic fields for storing user-controlled data */
} dr_instr_label_data_t;

#ifdef DR_FAST_IR
/* DR_API EXPORT END */
/* FIXME: could shrink prefixes, eflags, opcode, and flags fields
 * this struct isn't a memory bottleneck though b/c it isn't persistent
 */
/* DR_API EXPORT BEGIN */

/**
 * instr_t type exposed for optional "fast IR" access.  Note that DynamoRIO
 * reserves the right to change this structure across releases and does
 * not guarantee binary or source compatibility when this structure's fields
 * are directly accessed.  If the instr_ accessor routines are used, DynamoRIO does
 * guarantee source compatibility, but not binary compatibility.  If binary
 * compatibility is desired, do not use the fast IR feature.
 */
struct _instr_t {
    /* flags contains the constants defined above */
    uint    flags;

    /* raw bits of length length are pointed to by the bytes field */
    byte    *bytes;
    uint    length;

    /* translation target for this instr */
    app_pc  translation;

    uint    opcode;

#ifdef X64
    /* PR 251479: offset into instr's raw bytes of rip-relative 4-byte displacement */
    byte    rip_rel_pos;
#endif

    /* we dynamically allocate dst and src arrays b/c x86 instrs can have
     * up to 8 of each of them, but most have <=2 dsts and <=3 srcs, and we
     * use this struct for un-decoded instrs too
     */
    byte    num_dsts;
    byte    num_srcs;

    union {
        struct {
            /* for efficiency everyone has a 1st src opnd, since we often just
             * decode jumps, which all have a single source (==target)
             * yes this is an extra 10 bytes, but the whole struct is still < 64 bytes!
             */
            opnd_t    src0;
            opnd_t    *srcs; /* this array has 2nd src and beyond */
            opnd_t    *dsts;
        };
        dr_instr_label_data_t label_data;
    };

    uint    prefixes; /* data size, addr size, or lock prefix info */
    uint    eflags;   /* contains EFLAGS_ bits, but amount of info varies
                       * depending on how instr was decoded/built */

    /* this field is for the use of passes as an annotation.
     * it is also used to hold the offset of an instruction when encoding
     * pc-relative instructions.
     */
    void *note;

    /* fields for building instructions into instruction lists */
    instr_t   *prev;
    instr_t   *next;

}; /* instr_t */
#endif /* DR_FAST_IR */

/****************************************************************************
 * INSTR ROUTINES
 */
/**
 * @file dr_ir_instr.h
 * @brief Functions to create and manipulate instructions.
 */

/* DR_API EXPORT END */

DR_API
/**
 * Returns an initialized instr_t allocated on the thread-local heap.
 * Sets the x86/x64 mode of the returned instr_t to the mode of dcontext.
 */
/* For -x86_to_x64, sets the mode of the instr to the code cache mode instead of
the app mode. */
instr_t*
instr_create(dcontext_t *dcontext);

DR_API
/** Initializes \p instr.
 * Sets the x86/x64 mode of \p instr to the mode of dcontext.
 */
void
instr_init(dcontext_t *dcontext, instr_t *instr);

DR_API
/**
 * Deallocates all memory that was allocated by \p instr.  This
 * includes raw bytes allocated by instr_allocate_raw_bits() and
 * operands allocated by instr_set_num_opnds().  Does not deallocate
 * the storage for \p instr itself.
 */
void
instr_free(dcontext_t *dcontext, instr_t *instr);

DR_API
/**
 * Performs both instr_free() and instr_init().
 * \p instr must have been initialized.
 */
void
instr_reset(dcontext_t *dcontext, instr_t *instr);

DR_API
/**
 * Frees all dynamically allocated storage that was allocated by \p instr,
 * except for allocated bits.
 * Also zeroes out \p instr's fields, except for raw bit fields,
 * whether \p instr is instr_ok_to_mangle(), and the x86 mode of \p instr.
 * \p instr must have been initialized.
 */
void
instr_reuse(dcontext_t *dcontext, instr_t *instr);

DR_API
/**
 * Performs instr_free() and then deallocates the thread-local heap
 * storage for \p instr.
 */
void
instr_destroy(dcontext_t *dcontext, instr_t *instr);

DR_API
INSTR_INLINE
/**
 * Returns the next instr_t in the instrlist_t that contains \p instr.
 * \note The next pointer for an instr_t is inside the instr_t data
 * structure itself, making it impossible to have on instr_t in
 * two different InstrLists (but removing the need for an extra data
 * structure for each element of the instrlist_t).
 */
instr_t*
instr_get_next(instr_t *instr);

DR_API
INSTR_INLINE
/** Returns the previous instr_t in the instrlist_t that contains \p instr. */
instr_t*
instr_get_prev(instr_t *instr);

DR_API
INSTR_INLINE
/** Sets the next field of \p instr to point to \p next. */
void
instr_set_next(instr_t *instr, instr_t *next);

DR_API
INSTR_INLINE
/** Sets the prev field of \p instr to point to \p prev. */
void
instr_set_prev(instr_t *instr, instr_t *prev);

DR_API
INSTR_INLINE
/**
 * Gets the value of the user-controlled note field in \p instr.
 * \note Important: is also used when emitting for targets that are other
 * instructions.  Thus it will be overwritten when calling instrlist_encode()
 * or instrlist_encode_to_copy() with \p has_instr_jmp_targets set to true.
 * \note The note field is copied (shallowly) by instr_clone().
 */
void *
instr_get_note(instr_t *instr);

DR_API
INSTR_INLINE
/** Sets the user-controlled note field in \p instr to \p value. */
void
instr_set_note(instr_t *instr, void *value);

DR_API
/** Return the taken target pc of the (direct branch) instruction. */
app_pc
instr_get_branch_target_pc(instr_t *cti_instr);

DR_API
/** Set the taken target pc of the (direct branch) instruction. */
void
instr_set_branch_target_pc(instr_t *cti_instr, app_pc pc);

DR_API
/**
 * Returns true iff \p instr is a conditional branch, unconditional branch,
 * or indirect branch with a program address target (NOT an instr_t address target)
 * and \p instr is ok to mangle.
 */
#ifdef UNSUPPORTED_API
/**
 * This routine does NOT try to decode an opcode in a Level 1 or Level
 * 0 routine, and can thus be called on Level 0 routines.
 */
#endif
bool
instr_is_exit_cti(instr_t *instr);

DR_API
/** Return true iff \p instr's opcode is OP_int, OP_into, or OP_int3. */
bool
instr_is_interrupt(instr_t *instr);

#ifdef UNSUPPORTED_API
DR_API
/**
 * Returns true iff \p instr has been marked as targeting the prefix of its
 * target fragment.
 *
 * Some code manipulations need to store a target address in a
 * register and then jump there, but need the register to be restored
 * as well.  DR provides a single-instruction prefix that is
 * placed on all fragments (basic blocks as well as traces) that
 * restores ecx.  It is on traces for internal DR use.  To have
 * it added to basic blocks as well, call
 * dr_add_prefixes_to_basic_blocks() during initialization.
 */
bool
instr_branch_targets_prefix(instr_t *instr);

DR_API
/**
 * If \p val is true, indicates that \p instr's target fragment should be
 *   entered through its prefix, which restores ecx.
 * If \p val is false, indicates that \p instr should target the normal entry
 *   point and not the prefix.
 *
 * Some code manipulations need to store a target address in a
 * register and then jump there, but need the register to be restored
 * as well.  DR provides a single-instruction prefix that is
 * placed on all fragments (basic blocks as well as traces) that
 * restores ecx.  It is on traces for internal DR use.  To have
 * it added to basic blocks as well, call
 * dr_add_prefixes_to_basic_blocks() during initialization.
 */
void
instr_branch_set_prefix_target(instr_t *instr, bool val);
#endif /* UNSUPPORTED_API */

/* Returns true iff \p instr has been marked as a special fragment
 * exit cti.
 */
bool
instr_branch_special_exit(instr_t *instr);

/* If \p val is true, indicates that \p instr is a special exit cti.
 * If \p val is false, indicates otherwise.
 */
void
instr_branch_set_special_exit(instr_t *instr, bool val);

DR_API
INSTR_INLINE
/**
 * Return true iff \p instr is not a meta-instruction
 * (see instr_set_ok_to_mangle() for more information).
 */
bool
instr_ok_to_mangle(instr_t *instr);

DR_API
/**
 * Sets \p instr to "ok to mangle" if \p val is true and "not ok to
 * mangle" if \p val is false.  An instruction that is "not ok to
 * mangle" is treated by DR as a "meta-instruction", distinct from
 * normal application instructions, and is not mangled in any way.
 * This is necessary to have DR not create an exit stub for a direct
 * jump.  All non-meta instructions that are added to basic blocks or
 * traces should have their translation fields set (via
 * #instr_set_translation(), or the convenience routine
 * #instr_set_meta_no_translation()) when recreating state at a fault;
 * meta instructions should not fault (unless such faults are handled
 * by the client) and are not considered
 * application instructions but rather added instrumentation code (see
 * #dr_register_bb_event() for further information on recreating).
 */
void
instr_set_ok_to_mangle(instr_t *instr, bool val);

DR_API
/**
 * A convenience routine that calls both
 * #instr_set_ok_to_mangle (instr, false) and
 * #instr_set_translation (instr, NULL).
 */
void
instr_set_meta_no_translation(instr_t *instr);

DR_API
#ifdef AVOID_API_EXPORT
/* This is hot internally, but unlikely to be used by clients. */
INSTR_INLINE
#endif
/** Return true iff \p instr is to be emitted into the cache. */
bool
instr_ok_to_emit(instr_t *instr);

DR_API
/**
 * Set \p instr to "ok to emit" if \p val is true and "not ok to emit"
 * if \p val is false.  An instruction that should not be emitted is
 * treated normally by DR for purposes of exits but is not placed into
 * the cache.  It is used for final jumps that are to be elided.
 */
void
instr_set_ok_to_emit(instr_t *instr, bool val);

#ifdef CUSTOM_EXIT_STUBS
DR_API
/**
 * If \p instr is not an exit cti, does nothing.
 * If \p instr is an exit cti, sets \p stub to be custom exit stub code
 * that will be inserted in the exit stub prior to the normal exit
 * stub code.  If \p instr already has custom exit stub code, that
 * existing instrlist_t is cleared and destroyed (using current thread's
 * context).  (If \p stub is NULL, any existing stub code is NOT destroyed.)
 * The creator of the instrlist_t containing \p instr is
 * responsible for destroying stub.
 * \note Custom exit stubs containing control transfer instructions to
 * other instructions inside a fragment besides the custom stub itself
 * are not fully supported in that they will not be decoded from the
 * cache properly as having instr_t targets.
 */
void
instr_set_exit_stub_code(instr_t *instr, instrlist_t *stub);

DR_API
/**
 * Returns the custom exit stub code instruction list that has been
 * set for this instruction.  If none exists, returns NULL.
 */
instrlist_t *
instr_exit_stub_code(instr_t *instr);
#endif

DR_API
/**
 * Returns the length of \p instr.
 * As a side effect, if instr_ok_to_mangle(instr) and \p instr's raw bits
 * are invalid, encodes \p instr into bytes allocated with
 * instr_allocate_raw_bits(), after which instr is marked as having
 * valid raw bits.
 */
int
instr_length(dcontext_t *dcontext, instr_t *instr);

/* not exported */
void instr_shift_raw_bits(instr_t *instr, ssize_t offs);
uint instr_branch_type(instr_t *cti_instr);
int instr_exit_branch_type(instr_t *instr);
void instr_exit_branch_set_type(instr_t *instr, uint type);

DR_API
/** Returns number of bytes of heap used by \p instr. */
int
instr_mem_usage(instr_t *instr);

DR_API
/**
 * Returns a copy of \p orig with separately allocated memory for
 * operands and raw bytes if they were present in \p orig.
 * Only a shallow copy of the \p note field is made.
 */
instr_t *
instr_clone(dcontext_t *dcontext, instr_t *orig);

DR_API
/**
 * Convenience routine: calls
 * - instr_create(dcontext)
 * - instr_set_opcode(opcode)
 * - instr_set_num_opnds(dcontext, instr, num_dsts, num_srcs)
 *
 * and returns the resulting instr_t.
 */
instr_t *
instr_build(dcontext_t *dcontext, int opcode, int num_dsts, int num_srcs);

DR_API
/**
 * Convenience routine: calls
 * - instr_create(dcontext)
 * - instr_set_opcode(instr, opcode)
 * - instr_allocate_raw_bits(dcontext, instr, num_bytes)
 *
 * and returns the resulting instr_t.
 */
instr_t *
instr_build_bits(dcontext_t *dcontext, int opcode, uint num_bytes);

DR_API
/**
 * Returns true iff \p instr's opcode is NOT OP_INVALID.
 * Not to be confused with an invalid opcode, which can be OP_INVALID or
 * OP_UNDECODED.  OP_INVALID means an instruction with no valid fields:
 * raw bits (may exist but do not correspond to a valid instr), opcode,
 * eflags, or operands.  It could be an uninitialized
 * instruction or the result of decoding an invalid sequence of bytes.
 */
bool
instr_valid(instr_t *instr);

DR_API
/** Get the original application PC of \p instr if it exists. */
app_pc
instr_get_app_pc(instr_t *instr);

DR_API
/** Returns \p instr's opcode (an OP_ constant). */
int
instr_get_opcode(instr_t *instr);

DR_API
/** Assumes \p opcode is an OP_ constant and sets it to be instr's opcode. */
void
instr_set_opcode(instr_t *instr, int opcode);

const struct instr_info_t *
instr_get_instr_info(instr_t *instr);

const struct instr_info_t *
get_instr_info(int opcode);

DR_API
INSTR_INLINE
/**
 * Returns the number of source operands of \p instr.
 *
 * \note Addressing registers used in destination memory references
 * (i.e., base, index, or segment registers) are not separately listed
 * as source operands.
 */
int
instr_num_srcs(instr_t *instr);

DR_API
INSTR_INLINE
/**
 * Returns the number of destination operands of \p instr.
 */
int
instr_num_dsts(instr_t *instr);

DR_API
/**
 * Assumes that \p instr has been initialized but does not have any
 * operands yet.  Allocates storage for \p num_srcs source operands
 * and \p num_dsts destination operands.
 */
void
instr_set_num_opnds(dcontext_t *dcontext, instr_t *instr, int num_dsts, int num_srcs);

DR_API
/**
 * Returns \p instr's source operand at position \p pos (0-based).
 */
opnd_t
instr_get_src(instr_t *instr, uint pos);

DR_API
/**
 * Returns \p instr's destination operand at position \p pos (0-based).
 */
opnd_t
instr_get_dst(instr_t *instr, uint pos);

DR_API
/**
 * Sets \p instr's source operand at position \p pos to be \p opnd.
 * Also calls instr_set_raw_bits_valid(\p instr, false) and
 * instr_set_operands_valid(\p instr, true).
 */
void
instr_set_src(instr_t *instr, uint pos, opnd_t opnd);

DR_API
/**
 * Sets \p instr's destination operand at position \p pos to be \p opnd.
 * Also calls instr_set_raw_bits_valid(\p instr, false) and
 * instr_set_operands_valid(\p instr, true).
 */
void
instr_set_dst(instr_t *instr, uint pos, opnd_t opnd);

DR_API
/**
 * Assumes that \p cti_instr is a control transfer instruction
 * Returns the first source operand of \p cti_instr (its target).
 */
opnd_t
instr_get_target(instr_t *cti_instr);

DR_API
/**
 * Assumes that \p cti_instr is a control transfer instruction.
 * Sets the first source operand of \p cti_instr to be \p target.
 * Also calls instr_set_raw_bits_valid(\p instr, false) and
 * instr_set_operands_valid(\p instr, true).
 */
void
instr_set_target(instr_t *cti_instr, opnd_t target);

#ifdef AVOID_API_EXPORT
INSTR_INLINE  /* hot internally */
#endif
DR_API
/** Returns true iff \p instr's operands are up to date. */
bool
instr_operands_valid(instr_t *instr);

DR_API
/** Sets \p instr's operands to be valid if \p valid is true, invalid otherwise. */
void
instr_set_operands_valid(instr_t *instr, bool valid);

DR_API
/**
 * Returns true iff \p instr's opcode is valid.
 * If the opcode is ever set to other than OP_INVALID or OP_UNDECODED it is assumed
 * to be valid.  However, calling instr_get_opcode() will attempt to
 * decode a valid opcode, hence the purpose of this routine.
 */
bool
instr_opcode_valid(instr_t *instr);

/******************************************************************
 * Eflags validity is not exported!  It's hidden.  Calling get_eflags or
 * get_arith_flags will make them valid if they're not.
 */

bool
instr_arith_flags_valid(instr_t *instr);

/* Sets instr's arithmetic flags (the 6 bottom eflags) to be valid if
 * valid is true, invalid otherwise. */
void
instr_set_arith_flags_valid(instr_t *instr, bool valid);

/* Returns true iff instr's eflags are up to date. */
bool
instr_eflags_valid(instr_t *instr);

/* Sets instr's eflags to be valid if valid is true, invalid otherwise. */
void
instr_set_eflags_valid(instr_t *instr, bool valid);

DR_API
/** Returns \p instr's eflags use as EFLAGS_ constants or'ed together. */
uint
instr_get_eflags(instr_t *instr);

DR_API
/** Returns the eflags usage of instructions with opcode \p opcode,
 * as EFLAGS_ constants or'ed together.
 */
uint
instr_get_opcode_eflags(int opcode);

DR_API
/**
 * Returns \p instr's arithmetic flags (bottom 6 eflags) use
 * as EFLAGS_ constants or'ed together.
 * If \p instr's eflags behavior has not been calculated yet or is
 * invalid, the entire eflags use is calculated and returned (not
 * just the arithmetic flags).
 */
uint
instr_get_arith_flags(instr_t *instr);

/*
 ******************************************************************/

DR_API
/**
 * Assumes that \p instr does not currently have any raw bits allocated.
 * Sets \p instr's raw bits to be \p length bytes starting at \p addr.
 * Does not set the operands invalid.
 */
void
instr_set_raw_bits(instr_t *instr, byte * addr, uint length);

DR_API
/** Sets \p instr's raw bits to be valid if \p valid is true, invalid otherwise. */
void
instr_set_raw_bits_valid(instr_t *instr, bool valid);

#ifdef AVOID_API_EXPORT
INSTR_INLINE  /* internal inline */
#endif
DR_API
/** Returns true iff \p instr's raw bits are a valid encoding of instr. */
bool
instr_raw_bits_valid(instr_t *instr);

#ifdef AVOID_API_EXPORT
INSTR_INLINE  /* internal inline */
#endif
DR_API
/** Returns true iff \p instr has its own allocated memory for raw bits. */
bool
instr_has_allocated_bits(instr_t *instr);

#ifdef AVOID_API_EXPORT
INSTR_INLINE  /* internal inline */
#endif
DR_API
/** Returns true iff \p instr's raw bits are not a valid encoding of \p instr. */
bool
instr_needs_encoding(instr_t *instr);

DR_API
/**
 * Return true iff \p instr is not a meta-instruction that can fault
 * (see instr_set_meta_may_fault() for more information).
 *
 * \deprecated Any meta instruction can fault if it has a non-NULL
 * translation field and the client fully handles all of its faults,
 * so this routine is no longer needed.
 */
bool
instr_is_meta_may_fault(instr_t *instr);

DR_API
/**
 * \deprecated Any meta instruction can fault if it has a non-NULL
 * translation field and the client fully handles all of its faults,
 * so this routine is no longer needed.
 */
void
instr_set_meta_may_fault(instr_t *instr, bool val);

DR_API
/**
 * Allocates \p num_bytes of memory for \p instr's raw bits.
 * If \p instr currently points to raw bits, the allocated memory is
 * initialized with the bytes pointed to.
 * \p instr is then set to point to the allocated memory.
 */
void
instr_allocate_raw_bits(dcontext_t *dcontext, instr_t *instr, uint num_bytes);

DR_API
/**
 * Sets the translation pointer for \p instr, used to recreate the
 * application address corresponding to this instruction.  When adding
 * or modifying instructions that are to be considered application
 * instructions (i.e., non meta-instructions: see
 * #instr_ok_to_mangle), the translation should always be set.  Pick
 * the application address that if executed will be equivalent to
 * restarting \p instr.  Currently the translation address must lie
 * within the existing bounds of the containing code block.
 * Returns the supplied \p instr (for easy chaining).  Use
 * #instr_get_app_pc to see the current value of the translation.
 */
instr_t *
instr_set_translation(instr_t *instr, app_pc addr);

DR_UNS_API
/**
 * If the translation pointer is set for \p instr, returns that
 * else returns NULL.
 * \note The translation pointer is not automatically set when
 * decoding instructions from raw bytes (via decode(), e.g.); it is
 * set for instructions in instruction lists generated by DR (see
 * dr_register_bb_event()).
 *
 */
app_pc
instr_get_translation(instr_t *instr);

DR_API
/**
 * Calling this function with \p instr makes it safe to keep the
 * instruction around indefinitely when its raw bits point into the
 * cache.  The function allocates memory local to \p instr to hold a
 * copy of the raw bits. If this was not done, the original raw bits
 * could be deleted at some point.  Making an instruction persistent
 * is necessary if you want to keep it beyond returning from the call
 * that produced the instruction.
 */
void
instr_make_persistent(dcontext_t *dcontext, instr_t *instr);

DR_API
/**
 * Assumes that \p instr's raw bits are valid.
 * Returns a pointer to \p instr's raw bits.
 * \note A freshly-decoded instruction has valid raw bits that point to the
 * address from which it was decoded.  However, for instructions presented
 * in the basic block or trace events, use instr_get_app_pc() to retrieve
 * the corresponding application address, as the raw bits will not be set
 * for instructions added after decoding, and may point to a different location
 * for insructions that have been modified.
 */
byte *
instr_get_raw_bits(instr_t *instr);

DR_API
/** If \p instr has raw bits allocated, frees them. */
void
instr_free_raw_bits(dcontext_t *dcontext, instr_t *instr);

DR_API
/**
 * Assumes that \p instr's raw bits are valid and have > \p pos bytes.
 * Returns a pointer to \p instr's raw byte at position \p pos (beginning with 0).
 */
byte
instr_get_raw_byte(instr_t *instr, uint pos);

DR_API
/**
 * Assumes that \p instr's raw bits are valid and allocated by \p instr
 * and have > \p pos bytes.
 * Sets instr's raw byte at position \p pos (beginning with 0) to the value \p byte.
 */
void
instr_set_raw_byte(instr_t *instr, uint pos, byte byte);

DR_API
/**
 * Assumes that \p instr's raw bits are valid and allocated by \p instr
 * and have >= num_bytes bytes.
 * Copies the \p num_bytes beginning at start to \p instr's raw bits.
 */
void
instr_set_raw_bytes(instr_t *instr, byte *start, uint num_bytes);

DR_API
/**
 * Assumes that \p instr's raw bits are valid and allocated by \p instr
 * and have > pos+3 bytes.
 * Sets the 4 bytes beginning at position \p pos (0-based) to the value word.
 */
void
instr_set_raw_word(instr_t *instr, uint pos, uint word);

DR_API
/**
 * Assumes that \p instr's raw bits are valid and have > \p pos + 3 bytes.
 * Returns the 4 bytes beginning at position \p pos (0-based).
 */
uint
instr_get_raw_word(instr_t *instr, uint pos);

DR_API
/**
 * Assumes that \p prefix is a PREFIX_ constant.
 * Ors \p instr's prefixes with \p prefix.
 * Returns the supplied instr (for easy chaining).
 */
instr_t *
instr_set_prefix_flag(instr_t *instr, uint prefix);

DR_API
/**
 * Assumes that \p prefix is a PREFIX_ constant.
 * Returns true if \p instr's prefixes contain the flag \p prefix.
 */
bool
instr_get_prefix_flag(instr_t *instr, uint prefix);

/* NOT EXPORTED because we want to limit a client to seeing only the
 * handful of PREFIX_ flags we're exporting.
 * Assumes that prefix is a group of PREFIX_ constants or-ed together.
 * Sets instr's prefixes to be exactly those flags in prefixes.
 */
void
instr_set_prefixes(instr_t *instr, uint prefixes);

/* NOT EXPORTED because we want to limit a client to seeing only the
 * handful of PREFIX_ flags we're exporting.
 * Returns instr's prefixes as PREFIX_ constants or-ed together.
 */
uint
instr_get_prefixes(instr_t *instr);

/* DR_API EXPORT BEGIN */
#ifdef X64
/* DR_API EXPORT END */
DR_API
/**
 * Each instruction stores whether it should be interpreted in 32-bit
 * (x86) or 64-bit (x64) mode.  This routine sets the mode for \p instr.
 *
 * \note For 64-bit DR builds only.
 */
void
instr_set_x86_mode(instr_t *instr, bool x86);

DR_API
/**
 * Returns true if \p instr is an x86 instruction (32-bit) and false
 * if \p instr is an x64 instruction (64-bit).
 *
 * \note For 64-bit DR builds only.
 */
bool
instr_get_x86_mode(instr_t *instr);
/* DR_API EXPORT BEGIN */
#endif
/* DR_API EXPORT END */

/***********************************************************************/
/* decoding routines */

DR_UNS_API
/**
 * If instr is at Level 0 (i.e., a bundled group of instrs as raw bits),
 * expands instr into a sequence of Level 1 instrs using decode_raw() which
 * are added in place to ilist.
 * Returns the replacement of instr, if any expansion is performed
 * (in which case the old instr is destroyed); otherwise returns
 * instr unchanged.
 */
instr_t *
instr_expand(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr);

DR_UNS_API
/**
 * Returns true if instr is at Level 0 (i.e. a bundled group of instrs
 * as raw bits).
 */
bool
instr_is_level_0(instr_t *inst);

DR_UNS_API
/**
 * If the next instr is at Level 0 (i.e., a bundled group of instrs as raw bits),
 * expands it into a sequence of Level 1 instrs using decode_raw() which
 * are added in place to ilist.  Then returns the new next instr.
 */
instr_t *
instr_get_next_expanded(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr);

DR_UNS_API
/**
 * If the prev instr is at Level 0 (i.e., a bundled group of instrs as raw bits),
 * expands it into a sequence of Level 1 instrs using decode_raw() which
 * are added in place to ilist.  Then returns the new prev instr.
 */
instr_t *
instr_get_prev_expanded(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr);

DR_UNS_API
/**
 * If instr is not already at the level of decode_cti, decodes enough
 * from the raw bits pointed to by instr to bring it to that level.
 * Assumes that instr is a single instr (i.e., NOT Level 0).
 *
 * decode_cti decodes only enough of instr to determine
 * its size, its effects on the 6 arithmetic eflags, and whether it is
 * a control-transfer instruction.  If it is, the operands fields of
 * instr are filled in.  If not, only the raw bits fields of instr are
 * filled in.  This corresponds to a Level 3 decoding for control
 * transfer instructions but a Level 1 decoding plus arithmetic eflags
 * information for all other instructions.
 */
void
instr_decode_cti(dcontext_t *dcontext, instr_t *instr);

DR_UNS_API
/**
 * If instr is not already at the level of decode_opcode, decodes enough
 * from the raw bits pointed to by instr to bring it to that level.
 * Assumes that instr is a single instr (i.e., NOT Level 0).
 *
 * decode_opcode decodes the opcode and eflags usage of the instruction.
 * This corresponds to a Level 2 decoding.
 */
void
instr_decode_opcode(dcontext_t *dcontext, instr_t *instr);

DR_UNS_API
/**
 * If instr is not already fully decoded, decodes enough
 * from the raw bits pointed to by instr to bring it Level 3.
 * Assumes that instr is a single instr (i.e., NOT Level 0).
 */
void
instr_decode(dcontext_t *dcontext, instr_t *instr);

/* Calls instr_decode() with the current dcontext.  *Not* exported.  Mostly
 * useful as the slow path for IR routines that get inlined.
 */
instr_t *
instr_decode_with_current_dcontext(instr_t *instr);

/* DR_API EXPORT TOFILE dr_ir_instrlist.h */
DR_UNS_API
/**
 * If the first instr is at Level 0 (i.e., a bundled group of instrs as raw bits),
 * expands it into a sequence of Level 1 instrs using decode_raw() which
 * are added in place to ilist.  Then returns the new first instr.
 */
instr_t*
instrlist_first_expanded(dcontext_t *dcontext, instrlist_t *ilist);

DR_UNS_API
/**
 * If the last instr is at Level 0 (i.e., a bundled group of instrs as raw bits),
 * expands it into a sequence of Level 1 instrs using decode_raw() which
 * are added in place to ilist.  Then returns the new last instr.
 */
instr_t*
instrlist_last_expanded(dcontext_t *dcontext, instrlist_t *ilist);

DR_UNS_API
/**
 * Brings all instrs in ilist up to the decode_cti level, and
 * hooks up intra-ilist cti targets to use instr_t targets, by
 * matching pc targets to each instruction's raw bits.
 *
 * decode_cti decodes only enough of instr to determine
 * its size, its effects on the 6 arithmetic eflags, and whether it is
 * a control-transfer instruction.  If it is, the operands fields of
 * instr are filled in.  If not, only the raw bits fields of instr are
 * filled in.  This corresponds to a Level 3 decoding for control
 * transfer instructions but a Level 1 decoding plus arithmetic eflags
 * information for all other instructions.
 */
void
instrlist_decode_cti(dcontext_t *dcontext, instrlist_t *ilist);
/* DR_API EXPORT TOFILE dr_ir_instr.h */

/***********************************************************************/
/* utility functions */

DR_API
/**
 * Shrinks all registers not used as addresses, and all immed integer and
 * address sizes, to 16 bits.
 * Does not shrink DR_REG_ESI or DR_REG_EDI used in string instructions.
 */
void
instr_shrink_to_16_bits(instr_t *instr);

/* DR_API EXPORT BEGIN */
#ifdef X64
/* DR_API EXPORT END */
DR_API
/**
 * Shrinks all registers, including addresses, and all immed integer and
 * address sizes, to 32 bits.
 *
 * \note For 64-bit DR builds only.
 */
void
instr_shrink_to_32_bits(instr_t *instr);
/* DR_API EXPORT BEGIN */
#endif
/* DR_API EXPORT END */

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff at least one of \p instr's operands references a
 * register that overlaps \p reg.
 *
 * Returns false for multi-byte nops with an operand using reg.
 */
bool
instr_uses_reg(instr_t *instr, reg_id_t reg);

DR_API
/**
 * Returns true iff at least one of \p instr's operands references a floating
 * point register.
 */
bool
instr_uses_fp_reg(instr_t *instr);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff at least one of \p instr's source operands references \p reg.
 *
 * Returns false for multi-byte nops with a source operand using reg.
 *
 * \note Use instr_reads_from_reg() to also consider addressing
 * registers in destination operands.
 */
bool
instr_reg_in_src(instr_t *instr, reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff at least one of \p instr's destination operands references \p reg.
 */
bool
instr_reg_in_dst(instr_t *instr, reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff at least one of \p instr's destination operands is
 * a register operand for a register that overlaps \p reg.
 */
bool
instr_writes_to_reg(instr_t *instr, reg_id_t reg);

DR_API
/**
 * Assumes that reg is a DR_REG_ constant.
 * Returns true iff at least one of instr's operands reads
 * from a register that overlaps reg (checks both source operands
 * and addressing registers used in destination operands).
 *
 * Returns false for multi-byte nops with an operand using reg.
 */
bool
instr_reads_from_reg(instr_t *instr, reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff at least one of \p instr's destination operands is
 * the same register (not enough to just overlap) as \p reg.
 */
bool
instr_writes_to_exact_reg(instr_t *instr, reg_id_t reg);

DR_API
/**
 * Replaces all instances of \p old_opnd in \p instr's source operands with
 * \p new_opnd (uses opnd_same() to detect sameness).
 */
bool
instr_replace_src_opnd(instr_t *instr, opnd_t old_opnd, opnd_t new_opnd);

DR_API
/**
 * Returns true iff \p instr1 and \p instr2 have the same opcode, prefixes,
 * and source and destination operands (uses opnd_same() to compare the operands).
 */
bool
instr_same(instr_t *instr1, instr_t *instr2);

DR_API
/**
 * Returns true iff any of \p instr's source operands is a memory reference.
 *
 * Unlike opnd_is_memory_reference(), this routine conisders the
 * semantics of the instruction and returns false for both multi-byte
 * nops with a memory operand and for the #OP_lea instruction, as they
 * do not really reference the memory.  It does return true for
 * prefetch instructions.
 */
bool
instr_reads_memory(instr_t *instr);

DR_API
/** Returns true iff any of \p instr's destination operands is a memory reference. */
bool
instr_writes_memory(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr writes to an xmm register and zeroes the top half
 * of the corresponding ymm register as a result (some instructions preserve
 * the top half while others zero it when writing to the bottom half).
 */
bool
instr_zeroes_ymmh(instr_t *instr);

/* DR_API EXPORT BEGIN */
#ifdef X64
/* DR_API EXPORT END */
DR_API
/**
 * Returns true iff any of \p instr's operands is a rip-relative memory reference.
 *
 * \note For 64-bit DR builds only.
 */
bool
instr_has_rel_addr_reference(instr_t *instr);

DR_API
/**
 * If any of \p instr's operands is a rip-relative memory reference, returns the
 * address that reference targets.  Else returns false.
 *
 * \note For 64-bit DR builds only.
 */
bool
instr_get_rel_addr_target(instr_t *instr, /*OUT*/ app_pc *target);

DR_API
/**
 * If any of \p instr's destination operands is a rip-relative memory
 * reference, returns the operand position.  If there is no such
 * destination operand, returns -1.
 *
 * \note For 64-bit DR builds only.
 */
int
instr_get_rel_addr_dst_idx(instr_t *instr);

DR_API
/**
 * If any of \p instr's source operands is a rip-relative memory
 * reference, returns the operand position.  If there is no such
 * source operand, returns -1.
 *
 * \note For 64-bit DR builds only.
 */
int
instr_get_rel_addr_src_idx(instr_t *instr);

/* We're not exposing the low-level rip_rel_pos routines directly to clients,
 * who should only use this level 1-3 feature via decode_cti + encode.
 */

/* Returns true iff instr's raw bits are valid and the offset within
 * those raw bits of a rip-relative displacement is set (to 0 if there
 * is no such displacement).
 */
bool
instr_rip_rel_valid(instr_t *instr);

/* Sets whether instr's rip-relative displacement offset is valid. */
void
instr_set_rip_rel_valid(instr_t *instr, bool valid);

/* Assumes that instr_rip_rel_valid() is true.
 * Returns the offset within the encoded bytes of instr of the
 * displacement used for rip-relative addressing; returns 0
 * if instr has no rip-relative operand.
 * There can be at most one rip-relative operand in one instruction.
 */
uint
instr_get_rip_rel_pos(instr_t *instr);

/* Sets the offset within instr's encoded bytes of instr's
 * rip-relative displacement (the offset should be 0 if there is no
 * rip-relative operand) and marks it valid.  \p pos must be less
 * than 256.
 */
void
instr_set_rip_rel_pos(instr_t *instr, uint pos);
/* DR_API EXPORT BEGIN */
#endif /* X64 */
/* DR_API EXPORT END */

/* not exported: for PR 267260 */
bool
instr_is_our_mangling(instr_t *instr);

/* Sets whether instr came from our mangling. */
void
instr_set_our_mangling(instr_t *instr, bool ours);


DR_API
/**
 * Returns NULL if none of \p instr's operands is a memory reference.
 * Otherwise, returns the effective address of the first memory operand
 * when the operands are considered in this order: destinations and then
 * sources.  The address is computed using the passed-in registers.
 * \p mc->flags must include DR_MC_CONTROL and DR_MC_INTEGER.
 * For instructions that use vector addressing (VSIB, introduced in AVX2),
 * mc->flags must additionally include DR_MC_MULTIMEDIA.
 *
 * Like instr_reads_memory(), this routine does not consider
 * multi-byte nops that use addressing operands, or the #OP_lea
 * instruction's source operand, to be memory references.
 */
app_pc
instr_compute_address(instr_t *instr, dr_mcontext_t *mc);

app_pc
instr_compute_address_priv(instr_t *instr, priv_mcontext_t *mc);

DR_API
/**
 * Performs address calculation in the same manner as
 * instr_compute_address() but handles multiple memory operands.  The
 * \p index parameter should be initially set to 0 and then
 * incremented with each successive call until this routine returns
 * false, which indicates that there are no more memory operands.  The
 * address of each is computed in the same manner as
 * instr_compute_address() and returned in \p addr; whether it is a
 * write is returned in \p is_write.  Either or both OUT variables can
 * be NULL.
 * \p mc->flags must include DR_MC_CONTROL and DR_MC_INTEGER.
 * For instructions that use vector addressing (VSIB, introduced in AVX2),
 * mc->flags must additionally include DR_MC_MULTIMEDIA.
 *
 * Like instr_reads_memory(), this routine does not consider
 * multi-byte nops that use addressing operands, or the #OP_lea
 * instruction's source operand, to be memory references.
 */
bool
instr_compute_address_ex(instr_t *instr, dr_mcontext_t *mc, uint index,
                         OUT app_pc *addr, OUT bool *write);

DR_API
/**
 * Performs address calculation in the same manner as
 * instr_compute_address_ex() with additional information
 * of which opnd is used for address computation returned
 * in \p pos. If \p pos is NULL, it is the same as
 * instr_compute_address_ex().
 *
 * Like instr_reads_memory(), this routine does not consider
 * multi-byte nops that use addressing operands, or the #OP_lea
 * instruction's source operand, to be memory references.
 */
bool
instr_compute_address_ex_pos(instr_t *instr, dr_mcontext_t *mc, uint index,
                             OUT app_pc *addr, OUT bool *is_write,
                             OUT uint *pos);

bool
instr_compute_address_ex_priv(instr_t *instr, priv_mcontext_t *mc, uint index,
                              OUT app_pc *addr, OUT bool *write, OUT uint *pos);

DR_API
/**
 * Calculates the size, in bytes, of the memory read or write of \p instr.
 * If \p instr does not reference memory, or is invalid, returns 0.
 * If \p instr is a repeated string instruction, considers only one iteration.
 * If \p instr uses vector addressing (VSIB, introduced in AVX2), considers
 * only the size of each separate memory access.
 */
uint
instr_memory_reference_size(instr_t *instr);

DR_API
/**
 * \return a pointer to user-controlled data fields in a label instruction.
 * These fields are available for use by clients for their own purposes.
 * Returns NULL if \p instr is not a label instruction.
 * \note These data fields are copied (shallowly) across instr_clone().
 */
dr_instr_label_data_t *
instr_get_label_data_area(instr_t *instr);

/* DR_API EXPORT TOFILE dr_ir_utils.h */
/* DR_API EXPORT BEGIN */

/***************************************************************************
 * DECODE / DISASSEMBLY ROUTINES
 */
/* DR_API EXPORT END */

DR_API
/**
 * Calculates the size, in bytes, of the memory read or write of
 * the instr at \p pc.  If the instruction is a repeating string instruction,
 * considers only one iteration.
 * Returns the pc of the following instruction.
 * If the instruction at \p pc does not reference memory, or is invalid,
 * returns NULL.
 */
app_pc
decode_memory_reference_size(dcontext_t *dcontext, app_pc pc, uint *size_in_bytes);

/* DR_API EXPORT TOFILE dr_ir_instr.h */
DR_API
/**
 * Returns true iff \p instr is an IA-32/AMD64 "mov" instruction: either OP_mov_st,
 * OP_mov_ld, OP_mov_imm, OP_mov_seg, or OP_mov_priv.
 */
bool
instr_is_mov(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr's opcode is OP_call, OP_call_far, OP_call_ind,
 * or OP_call_far_ind.
 */
bool
instr_is_call(instr_t *instr);

DR_API
/** Returns true iff \p instr's opcode is OP_call or OP_call_far. */
bool
instr_is_call_direct(instr_t *instr);

DR_API
/** Returns true iff \p instr's opcode is OP_call. */
bool
instr_is_near_call_direct(instr_t *instr);

DR_API
/** Returns true iff \p instr's opcode is OP_call_ind or OP_call_far_ind. */
bool
instr_is_call_indirect(instr_t *instr);

DR_API
/** Returns true iff \p instr's opcode is OP_ret, OP_ret_far, or OP_iret. */
bool
instr_is_return(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr is a control transfer instruction of any kind
 * This includes OP_jcc, OP_jcc_short, OP_loop*, OP_jecxz, OP_call*, and OP_jmp*.
 */
bool
instr_is_cti(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr is a control transfer instruction that takes an
 * 8-bit offset: OP_loop*, OP_jecxz, OP_jmp_short, or OP_jcc_short
 */
#ifdef UNSUPPORTED_API
/**
 * This routine does NOT try to decode an opcode in a Level 1 or Level
 * 0 routine, and can thus be called on Level 0 routines.
 */
#endif
bool
instr_is_cti_short(instr_t *instr);

DR_API
/** Returns true iff \p instr is one of OP_loop* or OP_jecxz. */
bool
instr_is_cti_loop(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr's opcode is OP_loop* or OP_jecxz and instr has
 * been transformed to a sequence of instruction that will allow a 32-bit
 * offset.
 * If \p pc != NULL, \p pc is expected to point the the beginning of the encoding of
 * \p instr, and the following instructions are assumed to be encoded in sequence
 * after \p instr.
 * Otherwise, the encoding is expected to be found in \p instr's allocated bits.
 */
#ifdef UNSUPPORTED_API
/**
 * This routine does NOT try to decode an opcode in a Level 1 or Level
 * 0 routine, and can thus be called on Level 0 routines.
 */
#endif
bool
instr_is_cti_short_rewrite(instr_t *instr, byte *pc);

byte *
remangle_short_rewrite(dcontext_t *dcontext, instr_t *instr, byte *pc, app_pc target);

DR_API
/**
 * Returns true iff \p instr is a conditional branch: OP_jcc, OP_jcc_short,
 * OP_loop*, or OP_jecxz.
 */
bool
instr_is_cbr(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr is a multi-way (indirect) branch: OP_jmp_ind,
 * OP_call_ind, OP_ret, OP_jmp_far_ind, OP_call_far_ind, OP_ret_far, or
 * OP_iret.
 */
bool
instr_is_mbr(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr is an unconditional direct branch: OP_jmp,
 * OP_jmp_short, or OP_jmp_far.
 */
bool
instr_is_ubr(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr is a near unconditional direct branch: OP_jmp,
 * or OP_jmp_short.
 */
bool
instr_is_near_ubr(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr is a far control transfer instruction: OP_jmp_far,
 * OP_call_far, OP_jmp_far_ind, OP_call_far_ind, OP_ret_far, or OP_iret.
 */
bool
instr_is_far_cti(instr_t *instr);

DR_API
/** Returns true if \p instr is an absolute call or jmp that is far. */
bool
instr_is_far_abs_cti(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr is used to implement system calls: OP_int with a
 * source operand of 0x80 on linux or 0x2e on windows, or OP_sysenter,
 * or OP_syscall, or #instr_is_wow64_syscall() for WOW64.
 */
bool
instr_is_syscall(instr_t *instr);

/* DR_API EXPORT BEGIN */
#ifdef WINDOWS
/* DR_API EXPORT END */
DR_API
/**
 * Returns true iff \p instr is the indirect transfer from the 32-bit
 * ntdll.dll to the wow64 system call emulation layer.  This
 * instruction will also return true for instr_is_syscall, as well as
 * appear as an indirect call, so clients modifying indirect calls may
 * want to avoid modifying this type.
 *
 * \note Windows-only
 */
bool
instr_is_wow64_syscall(instr_t *instr);
/* DR_API EXPORT BEGIN */
#endif
/* DR_API EXPORT END */

DR_API
/**
 * Returns true iff \p instr is a prefetch instruction: OP_prefetchnta,
 * OP_prefetchnt0, OP_prefetchnt1, OP_prefetchnt2, OP_prefetch, or
 * OP_prefetchw.
 */
bool
instr_is_prefetch(instr_t *instr);

DR_API
/**
 * Tries to identify common cases of moving a constant into either a
 * register or a memory address.
 * Returns true and sets \p *value to the constant being moved for the following
 * cases: mov_imm, mov_st, and xor where the source equals the destination.
 */
bool
instr_is_mov_constant(instr_t *instr, ptr_int_t *value);

DR_API
/** Returns true iff \p instr is a floating point instruction. */
bool
instr_is_floating(instr_t *instr);

bool
instr_saves_float_pc(instr_t *instr);

/* DR_API EXPORT BEGIN */
/**
 * Indicates which type of floating-point operation and instruction performs.
 */
typedef enum {
    DR_FP_STATE,   /**< Loads, stores, or queries general floating point state. */
    DR_FP_MOVE,    /**< Moves floating point values from one location to another. */
    DR_FP_CONVERT, /**< Converts to or from floating point values. */
    DR_FP_MATH,    /**< Performs arithmetic or conditional operations. */
} dr_fp_type_t;
/* DR_API EXPORT END */

DR_API
/**
 * Returns true iff \p instr is a floating point instruction.
 * @param[in] instr  The instruction to query
 * @param[out] type  If the return value is true and \p type is
 *   non-NULL, the type of the floating point operation is written to \p type.
 */
bool
instr_is_floating_ex(instr_t *instr, dr_fp_type_t *type);

DR_API
/** Returns true iff \p instr is part of Intel's MMX instructions. */
bool
instr_is_mmx(instr_t *instr);

DR_API
/** Returns true iff \p instr is part of Intel's SSE or SSE2 instructions. */
bool
instr_is_sse_or_sse2(instr_t *instr);

DR_API
/** Returns true iff \p instr is a "mov $imm -> (%esp)". */
bool
instr_is_mov_imm_to_tos(instr_t *instr);

DR_API
/** Returns true iff \p instr is a label meta-instruction. */
bool
instr_is_label(instr_t *instr);

DR_API
/** Returns true iff \p instr is an "undefined" instruction (ud2) */
bool
instr_is_undefined(instr_t *instr);

DR_API
/**
 * Assumes that \p instr's opcode is OP_int and that either \p instr's
 * operands or its raw bits are valid.
 * Returns the first source operand if \p instr's operands are valid,
 * else if \p instr's raw bits are valid returns the first raw byte.
 */
int
instr_get_interrupt_number(instr_t *instr);

DR_API
/**
 * Assumes that \p instr is a conditional branch instruction
 * Reverses the logic of \p instr's conditional
 * e.g., changes OP_jb to OP_jnb.
 * Works on cti_short_rewrite as well.
 */
void
instr_invert_cbr(instr_t *instr);

/* PR 266292 */
DR_API
/**
 * Assumes that instr is a meta instruction (!instr_ok_to_mangle())
 * and an instr_is_cti_short() (8-bit reach).  Converts instr's opcode
 * to a long form (32-bit reach).  If instr's opcode is OP_loop* or
 * OP_jecxz, converts it to a sequence of multiple instructions (which
 * is different from instr_is_cti_short_rewrite()).  Each added instruction
 * is marked !instr_ok_to_mangle().
 * Returns the long form of the instruction, which is identical to \p instr
 * unless \p instr is OP_loop* or OP_jecxz, in which case the return value
 * is the final instruction in the sequence, the one that has long reach.
 * \note DR automatically converts non-meta short ctis to long form.
 */
instr_t *
instr_convert_short_meta_jmp_to_long(dcontext_t *dcontext, instrlist_t *ilist,
                                     instr_t *instr);

DR_API
/**
 * Given \p eflags, returns whether or not the conditional branch, \p
 * instr, would be taken.
 */
bool
instr_jcc_taken(instr_t *instr, reg_t eflags);

/* Given a machine state, returns whether or not the cbr instr would be taken
 * if the state is before execution (pre == true) or after (pre == false).
 * (not exported since machine state isn't)
 */
bool
instr_cbr_taken(instr_t *instr, priv_mcontext_t *mcontext, bool pre);

DR_API
/**
 * Converts a cmovcc opcode \p cmovcc_opcode to the OP_jcc opcode that
 * tests the same bits in eflags.
 */
int
instr_cmovcc_to_jcc(int cmovcc_opcode);

DR_API
/**
 * Given \p eflags, returns whether or not the conditional move
 * instruction \p instr would execute the move.  The conditional move
 * can be an OP_cmovcc or an OP_fcmovcc instruction.
 */
bool
instr_cmovcc_triggered(instr_t *instr, reg_t eflags);

/* utility routines that are in optimize.c */
opnd_t
instr_get_src_mem_access(instr_t *instr);

void
loginst(dcontext_t *dcontext, uint level, instr_t *instr, const char *string);

void
logopnd(dcontext_t *dcontext, uint level, opnd_t opnd, const char *string);

DR_API
/**
 * Returns true if \p instr is one of a class of common nops.
 * currently checks:
 * - nop
 * - nop reg/mem
 * - xchg reg, reg
 * - mov reg, reg
 * - lea reg, (reg)
 */
bool
instr_is_nop(instr_t *instr);

DR_UNS_API
/**
 * Convenience routine to create a nop of a certain size.  If \p raw
 * is true, sets raw bytes rather than filling in the operands or opcode.
 */
instr_t *
instr_create_nbyte_nop(dcontext_t *dcontext, uint num_bytes, bool raw);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode and no sources or destinations.
 */
instr_t *
instr_create_0dst_0src(dcontext_t *dcontext, int opcode);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode and a single source (\p src).
 */
instr_t *
instr_create_0dst_1src(dcontext_t *dcontext, int opcode,
                       opnd_t src);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode and two sources (\p src1, \p src2).
 */
instr_t *
instr_create_0dst_2src(dcontext_t *dcontext, int opcode,
                       opnd_t src1, opnd_t src2);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode and three sources
 * (\p src1, \p src2, \p src3).
 */
instr_t *
instr_create_0dst_3src(dcontext_t *dcontext, int opcode,
                       opnd_t src1, opnd_t src2, opnd_t src3);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode and one destination (\p dst).
 */
instr_t *
instr_create_1dst_0src(dcontext_t *dcontext, int opcode,
                       opnd_t dst);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, one destination(\p dst),
 * and one source (\p src).
 */
instr_t *
instr_create_1dst_1src(dcontext_t *dcontext, int opcode,
                       opnd_t dst, opnd_t src);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, one destination (\p dst),
 * and two sources (\p src1, \p src2).
 */
instr_t *
instr_create_1dst_2src(dcontext_t *dcontext, int opcode,
                       opnd_t dst, opnd_t src1, opnd_t src2);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, one destination (\p dst),
 * and three sources (\p src1, \p src2, \p src3).
 */
instr_t *
instr_create_1dst_3src(dcontext_t *dcontext, int opcode,
                       opnd_t dst, opnd_t src1, opnd_t src2, opnd_t src3);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, one destination (\p dst),
 * and five sources (\p src1, \p src2, \p src3, \p src4, \p src5).
 */
instr_t *
instr_create_1dst_5src(dcontext_t *dcontext, int opcode,
                       opnd_t dst, opnd_t src1, opnd_t src2, opnd_t src3,
                       opnd_t src4, opnd_t src5);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, two destinations (\p dst1, \p dst2)
 * and no sources.
 */
instr_t *
instr_create_2dst_0src(dcontext_t *dcontext, int opcode,
                       opnd_t dst1, opnd_t dst2);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, two destinations (\p dst1, \p dst2)
 * and one source (\p src).
 */
instr_t *
instr_create_2dst_1src(dcontext_t *dcontext, int opcode,
                       opnd_t dst1, opnd_t dst2, opnd_t src);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, two destinations (\p dst1, \p dst2)
 * and two sources (\p src1, \p src2).
 */
instr_t *
instr_create_2dst_2src(dcontext_t *dcontext, int opcode,
                       opnd_t dst1, opnd_t dst2, opnd_t src1, opnd_t src2);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, two destinations (\p dst1, \p dst2)
 * and three sources (\p src1, \p src2, \p src3).
 */
instr_t *
instr_create_2dst_3src(dcontext_t *dcontext, int opcode,
                       opnd_t dst1, opnd_t dst2,
                       opnd_t src1, opnd_t src2, opnd_t src3);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, two destinations (\p dst1, \p dst2)
 * and four sources (\p src1, \p src2, \p src3, \p src4).
 */
instr_t *
instr_create_2dst_4src(dcontext_t *dcontext, int opcode,
                       opnd_t dst1, opnd_t dst2,
                       opnd_t src1, opnd_t src2, opnd_t src3, opnd_t src4);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, three destinations
 * (\p dst1, \p dst2, \p dst3) and no sources.
 */
instr_t *
instr_create_3dst_0src(dcontext_t *dcontext, int opcode,
                       opnd_t dst1, opnd_t dst2, opnd_t dst3);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, three destinations
 * (\p dst1, \p dst2, \p dst3) and three sources
 * (\p src1, \p src2, \p src3).
 */
instr_t *
instr_create_3dst_3src(dcontext_t *dcontext, int opcode,
                       opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t src1, opnd_t src2, opnd_t src3);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, three destinations
 * (\p dst1, \p dst2, \p dst3) and four sources
 * (\p src1, \p src2, \p src3, \p src4).
 */
instr_t *
instr_create_3dst_4src(dcontext_t *dcontext, int opcode,
                       opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t src1, opnd_t src2, opnd_t src3, opnd_t src4);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, three destinations
 * (\p dst1, \p dst2, \p dst3) and five sources
 * (\p src1, \p src2, \p src3, \p src4, \p src5).
 */
instr_t *
instr_create_3dst_5src(dcontext_t *dcontext, int opcode,
                       opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t src1, opnd_t src2, opnd_t src3,
                       opnd_t src4, opnd_t src5);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, four destinations
 * (\p dst1, \p dst2, \p dst3, \p dst4) and 1 source (\p src).
 */
instr_t *
instr_create_4dst_1src(dcontext_t *dcontext, int opcode,
                       opnd_t dst1, opnd_t dst2, opnd_t dst3, opnd_t dst4,
                       opnd_t src);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, four destinations
 * (\p dst1, \p dst2, \p dst3, \p dst4) and four sources
 * (\p src1, \p src2, \p src3, \p src4).
 */
instr_t *
instr_create_4dst_4src(dcontext_t *dcontext, int opcode,
                       opnd_t dst1, opnd_t dst2, opnd_t dst3, opnd_t dst4,
                       opnd_t src1, opnd_t src2, opnd_t src3, opnd_t src4);

DR_API
/** Convenience routine that returns an initialized instr_t for OP_popa. */
instr_t *
instr_create_popa(dcontext_t *dcontext);

DR_API
/** Convenience routine that returns an initialized instr_t for OP_pusha. */
instr_t *
instr_create_pusha(dcontext_t *dcontext);

/* build instructions from raw bits */

DR_UNS_API
/**
 * Convenience routine that returns an initialized instr_t with invalid operands
 * and allocated raw bits with 1 byte (byte).
 */
instr_t *
instr_create_raw_1byte(dcontext_t *dcontext, byte byte);

DR_UNS_API
/**
 * Convenience routine that returns an initialized instr_t with invalid operands
 * and allocated raw bits with 2 bytes (byte1, byte2).
 */
instr_t *
instr_create_raw_2bytes(dcontext_t *dcontext, byte byte1, byte byte2);

DR_UNS_API
/**
 * Convenience routine that returns an initialized instr_t with invalid operands
 * and allocated raw bits with 3 bytes (byte1, byte2, byte3).
 */
instr_t *
instr_create_raw_3bytes(dcontext_t *dcontext, byte byte1,
                        byte byte2, byte byte3);

DR_UNS_API
/**
 * Convenience routine that returns an initialized instr_t with invalid operands
 * and allocated raw bits with 4 bytes (byte1, byte2, byte3, byte4).
 */
instr_t *
instr_create_raw_4bytes(dcontext_t *dcontext, byte byte1,
                        byte byte2, byte byte3, byte byte4);

DR_UNS_API
/**
 * Convenience routine that returns an initialized instr_t with invalid operands
 * and allocated raw bits with 5 bytes (byte1, byte2, byte3, byte4, byte5).
 */
instr_t *
instr_create_raw_5bytes(dcontext_t *dcontext, byte byte1,
                        byte byte2, byte byte3, byte byte4, byte byte5);

DR_UNS_API
/**
 * Convenience routine that returns an initialized instr_t with invalid operands
 * and allocated raw bits with 6 bytes (byte1, byte2, byte3, byte4, byte5, byte6).
 */
instr_t *
instr_create_raw_6bytes(dcontext_t *dcontext, byte byte1, byte byte2,
                        byte byte3, byte byte4, byte byte5, byte byte6);

DR_UNS_API
/**
 * Convenience routine that returns an initialized instr_t with invalid operands
 * and allocated raw bits with 7 bytes (byte1, byte2, byte3, byte4, byte5, byte6,
 * byte7).
 */
instr_t *
instr_create_raw_7bytes(dcontext_t *dcontext, byte byte1, byte byte2,
                        byte byte3, byte byte4, byte byte5,
                        byte byte6, byte byte7);

DR_UNS_API
/**
 * Convenience routine that returns an initialized instr_t with invalid operands
 * and allocated raw bits with 7 bytes (byte1, byte2, byte3, byte4, byte5, byte6,
 * byte7, byte8).
 */
instr_t *
instr_create_raw_8bytes(dcontext_t *dcontext, byte byte1, byte byte2,
                        byte byte3, byte byte4, byte byte5,
                        byte byte6, byte byte7, byte byte8);

opnd_t opnd_create_dcontext_field(dcontext_t *dcontext, int offs);
opnd_t opnd_create_dcontext_field_byte(dcontext_t *dcontext, int offs);
opnd_t opnd_create_dcontext_field_sz(dcontext_t *dcontext, int offs, opnd_size_t sz);
instr_t * instr_create_save_to_dcontext(dcontext_t *dcontext, reg_id_t reg, int offs);
instr_t * instr_create_save_immed_to_dcontext(dcontext_t *dcontext, int immed, int offs);
instr_t *
instr_create_save_immed_to_dc_via_reg(dcontext_t *dcontext, reg_id_t basereg,
                                      int offs, ptr_int_t immed, opnd_size_t sz);
instr_t * instr_create_restore_from_dcontext(dcontext_t *dcontext, reg_id_t reg, int offs);

/* basereg, if left as REG_NULL, is assumed to be xdi (xsi for upcontext) */
opnd_t
opnd_create_dcontext_field_via_reg_sz(dcontext_t *dcontext, reg_id_t basereg,
                                      int offs, opnd_size_t sz);
opnd_t opnd_create_dcontext_field_via_reg(dcontext_t *dcontext, reg_id_t basereg,
                                        int offs);

instr_t * instr_create_save_to_dc_via_reg(dcontext_t *dcontext, reg_id_t basereg,
                                        reg_id_t reg, int offs);
instr_t * instr_create_restore_from_dc_via_reg(dcontext_t *dcontext, reg_id_t basereg,
                                             reg_id_t reg, int offs);

instr_t * instr_create_jump_via_dcontext(dcontext_t *dcontext, int offs);
instr_t * instr_create_save_dynamo_stack(dcontext_t *dcontext);
instr_t * instr_create_restore_dynamo_stack(dcontext_t *dcontext);
opnd_t update_dcontext_address(opnd_t op, dcontext_t *old_dcontext,
                               dcontext_t *new_dcontext);
opnd_t opnd_create_tls_slot(int offs);
/* For size, use a OPSZ_ value from decode.h, typically OPSZ_1 or OPSZ_4 */
opnd_t opnd_create_sized_tls_slot(int offs, opnd_size_t size);
bool instr_raw_is_tls_spill(byte *pc, reg_id_t reg, ushort offs);
bool instr_is_tls_spill(instr_t *instr, reg_id_t reg, ushort offs);
bool instr_is_tls_xcx_spill(instr_t *instr);
/* Pass REG_NULL to not care about the reg */
bool
instr_is_tls_restore(instr_t *instr, reg_id_t reg, ushort offs);
bool
instr_is_reg_spill_or_restore(dcontext_t *dcontext, instr_t *instr,
                              bool *tls, bool *spill, reg_id_t *reg);

/* N.B. : client meta routines (dr_insert_* etc.) should never use anything other
 * then TLS_XAX_SLOT unless the client has specified a slot to use as we let the
 * client use the rest. */
instr_t * instr_create_save_to_tls(dcontext_t *dcontext, reg_id_t reg, ushort offs);
instr_t * instr_create_restore_from_tls(dcontext_t *dcontext, reg_id_t reg, ushort offs);
/* For -x86_to_x64, we can spill to 64-bit extra registers (xref i#751). */
instr_t * instr_create_save_to_reg(dcontext_t *dcontext, reg_id_t reg1, reg_id_t reg2);
instr_t * instr_create_restore_from_reg(dcontext_t *dcontext,
                                        reg_id_t reg1, reg_id_t reg2);

#ifdef X64
byte *
instr_raw_is_rip_rel_lea(byte *pc, byte *read_end);
#endif

/* DR_API EXPORT TOFILE dr_ir_instr.h */
/* DR_API EXPORT BEGIN */


/****************************************************************************
 * EFLAGS
 */
/* we only care about these 11 flags, and mostly only about the first 6
 * we consider an undefined effect on a flag to be a write
 */
#define EFLAGS_READ_CF   0x00000001 /**< Reads CF (Carry Flag). */
#define EFLAGS_READ_PF   0x00000002 /**< Reads PF (Parity Flag). */
#define EFLAGS_READ_AF   0x00000004 /**< Reads AF (Auxiliary Carry Flag). */
#define EFLAGS_READ_ZF   0x00000008 /**< Reads ZF (Zero Flag). */
#define EFLAGS_READ_SF   0x00000010 /**< Reads SF (Sign Flag). */
#define EFLAGS_READ_TF   0x00000020 /**< Reads TF (Trap Flag). */
#define EFLAGS_READ_IF   0x00000040 /**< Reads IF (Interrupt Enable Flag). */
#define EFLAGS_READ_DF   0x00000080 /**< Reads DF (Direction Flag). */
#define EFLAGS_READ_OF   0x00000100 /**< Reads OF (Overflow Flag). */
#define EFLAGS_READ_NT   0x00000200 /**< Reads NT (Nested Task). */
#define EFLAGS_READ_RF   0x00000400 /**< Reads RF (Resume Flag). */
#define EFLAGS_WRITE_CF  0x00000800 /**< Writes CF (Carry Flag). */
#define EFLAGS_WRITE_PF  0x00001000 /**< Writes PF (Parity Flag). */
#define EFLAGS_WRITE_AF  0x00002000 /**< Writes AF (Auxiliary Carry Flag). */
#define EFLAGS_WRITE_ZF  0x00004000 /**< Writes ZF (Zero Flag). */
#define EFLAGS_WRITE_SF  0x00008000 /**< Writes SF (Sign Flag). */
#define EFLAGS_WRITE_TF  0x00010000 /**< Writes TF (Trap Flag). */
#define EFLAGS_WRITE_IF  0x00020000 /**< Writes IF (Interrupt Enable Flag). */
#define EFLAGS_WRITE_DF  0x00040000 /**< Writes DF (Direction Flag). */
#define EFLAGS_WRITE_OF  0x00080000 /**< Writes OF (Overflow Flag). */
#define EFLAGS_WRITE_NT  0x00100000 /**< Writes NT (Nested Task). */
#define EFLAGS_WRITE_RF  0x00200000 /**< Writes RF (Resume Flag). */

#define EFLAGS_READ_ALL  0x000007ff /**< Reads all flags. */
#define EFLAGS_WRITE_ALL 0x003ff800 /**< Writes all flags. */
/* 6 most common flags ("arithmetic flags"): CF, PF, AF, ZF, SF, OF */
/** Reads all 6 arithmetic flags (CF, PF, AF, ZF, SF, OF). */
#define EFLAGS_READ_6    0x0000011f
/** Writes all 6 arithmetic flags (CF, PF, AF, ZF, SF, OF). */
#define EFLAGS_WRITE_6   0x0008f800

/** Converts an EFLAGS_WRITE_* value to the corresponding EFLAGS_READ_* value. */
#define EFLAGS_WRITE_TO_READ(x) ((x) >> 11)
/** Converts an EFLAGS_READ_* value to the corresponding EFLAGS_WRITE_* value. */
#define EFLAGS_READ_TO_WRITE(x) ((x) << 11)

/**
 * The actual bits in the eflags register that we care about:\n<pre>
 *   11 10  9  8  7  6  5  4  3  2  1  0
 *   OF DF       SF ZF    AF    PF    CF  </pre>
 */
enum {
    EFLAGS_CF = 0x00000001, /**< The bit in the eflags register of CF (Carry Flag). */
    EFLAGS_PF = 0x00000004, /**< The bit in the eflags register of PF (Parity Flag). */
    EFLAGS_AF = 0x00000010, /**< The bit in the eflags register of AF (Aux Carry Flag). */
    EFLAGS_ZF = 0x00000040, /**< The bit in the eflags register of ZF (Zero Flag). */
    EFLAGS_SF = 0x00000080, /**< The bit in the eflags register of SF (Sign Flag). */
    EFLAGS_DF = 0x00000400, /**< The bit in the eflags register of DF (Direction Flag). */
    EFLAGS_OF = 0x00000800, /**< The bit in the eflags register of OF (Overflow Flag). */
};

/* DR_API EXPORT END */

/* even on x64, displacements are 32 bits, so we keep the "int" type and 4-byte size */
#define PC_RELATIVE_TARGET(addr) ( *((int *)(addr)) + (addr) + 4 )

enum {
    RAW_OPCODE_nop             = 0x90,
    RAW_OPCODE_jmp_short       = 0xeb,
    RAW_OPCODE_call            = 0xe8,
    RAW_OPCODE_ret             = 0xc3,
    RAW_OPCODE_jmp             = 0xe9,
    RAW_OPCODE_push_imm32      = 0x68,
    RAW_OPCODE_jcc_short_start = 0x70,
    RAW_OPCODE_jcc_short_end   = 0x7f,
    RAW_OPCODE_jcc_byte1       = 0x0f,
    RAW_OPCODE_jcc_byte2_start = 0x80,
    RAW_OPCODE_jcc_byte2_end   = 0x8f,
    RAW_OPCODE_loop_start      = 0xe0,
    RAW_OPCODE_loop_end        = 0xe3,
    RAW_OPCODE_lea             = 0x8d,
    RAW_PREFIX_jcc_not_taken   = 0x2e,
    RAW_PREFIX_jcc_taken       = 0x3e,
    RAW_PREFIX_lock            = 0xf0,
    RAW_PREFIX_xacquire        = 0xf2,
    RAW_PREFIX_xrelease        = 0xf3,
};

enum { /* FIXME: vs RAW_OPCODE_* enum */
    FS_SEG_OPCODE        = 0x64,
    GS_SEG_OPCODE        = 0x65,

    /* For Windows, we piggyback on native TLS via gs for x64 and fs for x86.
     * For Linux, we steal a segment register, and so use fs for x86 (where
     * pthreads uses gs) and gs for x64 (where pthreads uses fs) (presumably
     * to avoid conflicts w/ wine).
     */
#ifdef X64
    TLS_SEG_OPCODE       = GS_SEG_OPCODE,
#else
    TLS_SEG_OPCODE       = FS_SEG_OPCODE,
#endif

    DATA_PREFIX_OPCODE   = 0x66,
    ADDR_PREFIX_OPCODE   = 0x67,
    REPNE_PREFIX_OPCODE  = 0xf2,
    REP_PREFIX_OPCODE    = 0xf3,
    REX_PREFIX_BASE_OPCODE = 0x40,
    REX_PREFIX_W_OPFLAG    = 0x8,
    REX_PREFIX_R_OPFLAG    = 0x4,
    REX_PREFIX_X_OPFLAG    = 0x2,
    REX_PREFIX_B_OPFLAG    = 0x1,
    REX_PREFIX_ALL_OPFLAGS = 0xf,
    MOV_REG2MEM_OPCODE   = 0x89,
    MOV_MEM2REG_OPCODE   = 0x8b,
    MOV_XAX2MEM_OPCODE   = 0xa3, /* no ModRm */
    MOV_MEM2XAX_OPCODE   = 0xa1, /* no ModRm */
    MOV_IMM2XAX_OPCODE   = 0xb8, /* no ModRm */
    MOV_IMM2XBX_OPCODE   = 0xbb, /* no ModRm */
    MOV_IMM2MEM_OPCODE   = 0xc7, /* has ModRm */
    JECXZ_OPCODE         = 0xe3,
    JMP_SHORT_OPCODE     = 0xeb,
    JMP_OPCODE           = 0xe9,
    JNE_OPCODE_1         = 0x0f,
    SAHF_OPCODE          = 0x9e,
    LAHF_OPCODE          = 0x9f,
    SETO_OPCODE_1        = 0x0f,
    SETO_OPCODE_2        = 0x90,
    ADD_AL_OPCODE        = 0x04,
    INC_MEM32_OPCODE_1   = 0xff, /* has /0 as well */
    MODRM16_DISP16       = 0x06, /* see vol.2 Table 2-1 for modR/M */
    SIB_DISP32           = 0x25, /* see vol.2 Table 2-1 for modR/M */
};

/* length of our mangling of jecxz/loop*, beyond a possible addr prefix byte */
#define CTI_SHORT_REWRITE_LENGTH 9

/* This should be kept in sync w/ the defines in x86/x86.asm */
enum {
#ifdef X64
# ifdef UNIX
    /* SysV ABI calling convention */
    NUM_REGPARM          = 6,
    REGPARM_0            = REG_RDI,
    REGPARM_1            = REG_RSI,
    REGPARM_2            = REG_RDX,
    REGPARM_3            = REG_RCX,
    REGPARM_4            = REG_R8,
    REGPARM_5            = REG_R9,
    REGPARM_MINSTACK     = 0,
    REDZONE_SIZE         = 128,
# else
    /* Intel/Microsoft calling convention */
    NUM_REGPARM          = 4,
    REGPARM_0            = REG_RCX,
    REGPARM_1            = REG_RDX,
    REGPARM_2            = REG_R8,
    REGPARM_3            = REG_R9,
    REGPARM_MINSTACK     = 4*sizeof(XSP_SZ),
    REDZONE_SIZE         = 0,
# endif
    /* In fact, for Windows the stack pointer is supposed to be
     * 16-byte aligned at all times except in a prologue or epilogue.
     * The prologue will always adjust by 16*n+8 since push of retaddr
     * always makes stack pointer not 16-byte aligned.
     */
    REGPARM_END_ALIGN    = 16,
#else
    NUM_REGPARM          = 0,
    REGPARM_MINSTACK     = 0,
    REDZONE_SIZE         = 0,
# ifdef MACOS
    REGPARM_END_ALIGN    = 16,
# else
    REGPARM_END_ALIGN    = sizeof(XSP_SZ),
# endif
#endif
};
extern const reg_id_t regparms[];

/* DR_API EXPORT TOFILE dr_ir_opcodes.h */
/* DR_API EXPORT BEGIN */

/****************************************************************************
 * OPCODES
 */
/**
 * @file dr_ir_opcodes.h
 * @brief Instruction opcode constants.
 */
#ifdef AVOID_API_EXPORT
/*
 * This enum corresponds with the array in decode_table.c
 * IF YOU CHANGE ONE YOU MUST CHANGE THE OTHER
 * The Perl script tools/x86opnums.pl is useful for re-numbering these
 * if you add or delete in the middle (feed it the array from decode_table.c).
 * When adding new instructions, be sure to update all of these places:
 *   1) decode_table op_instr array
 *   2) decode_table decoding table entries
 *   3) OP_ enum (here) via x86opnums.pl
 *   4) update OP_LAST at end of enum here
 *   5) decode_fast tables if necessary (they are conservative)
 *   6) instr_create macros
 *   7) suite/tests/api/ir* tests
 */
#endif
/** Opcode constants for use in the instr_t data structure. */
enum {
/*   0 */     OP_INVALID,  /* NULL, */ /**< INVALID opcode */
/*   1 */     OP_UNDECODED,  /* NULL, */ /**< UNDECODED opcode */
/*   2 */     OP_CONTD,    /* NULL, */ /**< CONTD opcode */
/*   3 */     OP_LABEL,    /* NULL, */ /**< LABEL opcode */

/*   4 */     OP_add,      /* &first_byte[0x05], */ /**< add opcode */
/*   5 */     OP_or,       /* &first_byte[0x0d], */ /**< or opcode */
/*   6 */     OP_adc,      /* &first_byte[0x15], */ /**< adc opcode */
/*   7 */     OP_sbb,      /* &first_byte[0x1d], */ /**< sbb opcode */
/*   8 */     OP_and,      /* &first_byte[0x25], */ /**< and opcode */
/*   9 */     OP_daa,      /* &first_byte[0x27], */ /**< daa opcode */
/*  10 */     OP_sub,      /* &first_byte[0x2d], */ /**< sub opcode */
/*  11 */     OP_das,      /* &first_byte[0x2f], */ /**< das opcode */
/*  12 */     OP_xor,      /* &first_byte[0x35], */ /**< xor opcode */
/*  13 */     OP_aaa,      /* &first_byte[0x37], */ /**< aaa opcode */
/*  14 */     OP_cmp,      /* &first_byte[0x3d], */ /**< cmp opcode */
/*  15 */     OP_aas,      /* &first_byte[0x3f], */ /**< aas opcode */
/*  16 */     OP_inc,      /* &x64_extensions[0][0], */ /**< inc opcode */
/*  17 */     OP_dec,      /* &x64_extensions[8][0], */ /**< dec opcode */
/*  18 */     OP_push,     /* &first_byte[0x50], */ /**< push opcode */
/*  19 */     OP_push_imm, /* &first_byte[0x68], */ /**< push_imm opcode */
/*  20 */     OP_pop,      /* &first_byte[0x58], */ /**< pop opcode */
/*  21 */     OP_pusha,    /* &first_byte[0x60], */ /**< pusha opcode */
/*  22 */     OP_popa,     /* &first_byte[0x61], */ /**< popa opcode */
/*  23 */     OP_bound,    /* &first_byte[0x62], */ /**< bound opcode */
/*  24 */     OP_arpl,     /* &x64_extensions[16][0], */ /**< arpl opcode */
/*  25 */     OP_imul,     /* &extensions[10][5], */ /**< imul opcode */

/*  26 */     OP_jo_short,     /* &first_byte[0x70], */ /**< jo_short opcode */
/*  27 */     OP_jno_short,    /* &first_byte[0x71], */ /**< jno_short opcode */
/*  28 */     OP_jb_short,     /* &first_byte[0x72], */ /**< jb_short opcode */
/*  29 */     OP_jnb_short,    /* &first_byte[0x73], */ /**< jnb_short opcode */
/*  30 */     OP_jz_short,     /* &first_byte[0x74], */ /**< jz_short opcode */
/*  31 */     OP_jnz_short,    /* &first_byte[0x75], */ /**< jnz_short opcode */
/*  32 */     OP_jbe_short,    /* &first_byte[0x76], */ /**< jbe_short opcode */
/*  33 */     OP_jnbe_short,   /* &first_byte[0x77], */ /**< jnbe_short opcode */
/*  34 */     OP_js_short,     /* &first_byte[0x78], */ /**< js_short opcode */
/*  35 */     OP_jns_short,    /* &first_byte[0x79], */ /**< jns_short opcode */
/*  36 */     OP_jp_short,     /* &first_byte[0x7a], */ /**< jp_short opcode */
/*  37 */     OP_jnp_short,    /* &first_byte[0x7b], */ /**< jnp_short opcode */
/*  38 */     OP_jl_short,     /* &first_byte[0x7c], */ /**< jl_short opcode */
/*  39 */     OP_jnl_short,    /* &first_byte[0x7d], */ /**< jnl_short opcode */
/*  40 */     OP_jle_short,    /* &first_byte[0x7e], */ /**< jle_short opcode */
/*  41 */     OP_jnle_short,   /* &first_byte[0x7f], */ /**< jnle_short opcode */

/*  42 */     OP_call,           /* &first_byte[0xe8], */ /**< call opcode */
/*  43 */     OP_call_ind,       /* &extensions[12][2], */ /**< call_ind opcode */
/*  44 */     OP_call_far,       /* &first_byte[0x9a], */ /**< call_far opcode */
/*  45 */     OP_call_far_ind,   /* &extensions[12][3], */ /**< call_far_ind opcode */
/*  46 */     OP_jmp,            /* &first_byte[0xe9], */ /**< jmp opcode */
/*  47 */     OP_jmp_short,      /* &first_byte[0xeb], */ /**< jmp_short opcode */
/*  48 */     OP_jmp_ind,        /* &extensions[12][4], */ /**< jmp_ind opcode */
/*  49 */     OP_jmp_far,        /* &first_byte[0xea], */ /**< jmp_far opcode */
/*  50 */     OP_jmp_far_ind,    /* &extensions[12][5], */ /**< jmp_far_ind opcode */

/*  51 */     OP_loopne,   /* &first_byte[0xe0], */ /**< loopne opcode */
/*  52 */     OP_loope,    /* &first_byte[0xe1], */ /**< loope opcode */
/*  53 */     OP_loop,     /* &first_byte[0xe2], */ /**< loop opcode */
/*  54 */     OP_jecxz,    /* &first_byte[0xe3], */ /**< jecxz opcode */

    /* point ld & st at eAX & al instrs, they save 1 byte (no modrm),
     * hopefully time taken considering them doesn't offset that */
/*  55 */     OP_mov_ld,      /* &first_byte[0xa1], */ /**< mov_ld opcode */
/*  56 */     OP_mov_st,      /* &first_byte[0xa3], */ /**< mov_st opcode */
    /* PR 250397: store of immed is mov_st not mov_imm, even though can be immed->reg,
     * which we address by sharing part of the mov_st template chain */
/*  57 */     OP_mov_imm,     /* &first_byte[0xb8], */ /**< mov_imm opcode */
/*  58 */     OP_mov_seg,     /* &first_byte[0x8e], */ /**< mov_seg opcode */
/*  59 */     OP_mov_priv,    /* &second_byte[0x20], */ /**< mov_priv opcode */

/*  60 */     OP_test,     /* &first_byte[0xa9], */ /**< test opcode */
/*  61 */     OP_lea,      /* &first_byte[0x8d], */ /**< lea opcode */
/*  62 */     OP_xchg,     /* &first_byte[0x91], */ /**< xchg opcode */
/*  63 */     OP_cwde,     /* &first_byte[0x98], */ /**< cwde opcode */
/*  64 */     OP_cdq,      /* &first_byte[0x99], */ /**< cdq opcode */
/*  65 */     OP_fwait,    /* &first_byte[0x9b], */ /**< fwait opcode */
/*  66 */     OP_pushf,    /* &first_byte[0x9c], */ /**< pushf opcode */
/*  67 */     OP_popf,     /* &first_byte[0x9d], */ /**< popf opcode */
/*  68 */     OP_sahf,     /* &first_byte[0x9e], */ /**< sahf opcode */
/*  69 */     OP_lahf,     /* &first_byte[0x9f], */ /**< lahf opcode */

/*  70 */     OP_ret,       /* &first_byte[0xc2], */ /**< ret opcode */
/*  71 */     OP_ret_far,   /* &first_byte[0xca], */ /**< ret_far opcode */

/*  72 */     OP_les,      /* &vex_prefix_extensions[0][0], */ /**< les opcode */
/*  73 */     OP_lds,      /* &vex_prefix_extensions[1][0], */ /**< lds opcode */
/*  74 */     OP_enter,    /* &first_byte[0xc8], */ /**< enter opcode */
/*  75 */     OP_leave,    /* &first_byte[0xc9], */ /**< leave opcode */
/*  76 */     OP_int3,     /* &first_byte[0xcc], */ /**< int3 opcode */
/*  77 */     OP_int,      /* &first_byte[0xcd], */ /**< int opcode */
/*  78 */     OP_into,     /* &first_byte[0xce], */ /**< into opcode */
/*  79 */     OP_iret,     /* &first_byte[0xcf], */ /**< iret opcode */
/*  80 */     OP_aam,      /* &first_byte[0xd4], */ /**< aam opcode */
/*  81 */     OP_aad,      /* &first_byte[0xd5], */ /**< aad opcode */
/*  82 */     OP_xlat,     /* &first_byte[0xd7], */ /**< xlat opcode */
/*  83 */     OP_in,       /* &first_byte[0xe5], */ /**< in opcode */
/*  84 */     OP_out,      /* &first_byte[0xe7], */ /**< out opcode */
/*  85 */     OP_hlt,      /* &first_byte[0xf4], */ /**< hlt opcode */
/*  86 */     OP_cmc,      /* &first_byte[0xf5], */ /**< cmc opcode */
/*  87 */     OP_clc,      /* &first_byte[0xf8], */ /**< clc opcode */
/*  88 */     OP_stc,      /* &first_byte[0xf9], */ /**< stc opcode */
/*  89 */     OP_cli,      /* &first_byte[0xfa], */ /**< cli opcode */
/*  90 */     OP_sti,      /* &first_byte[0xfb], */ /**< sti opcode */
/*  91 */     OP_cld,      /* &first_byte[0xfc], */ /**< cld opcode */
/*  92 */     OP_std,      /* &first_byte[0xfd], */ /**< std opcode */


/*  93 */     OP_lar,          /* &second_byte[0x02], */ /**< lar opcode */
/*  94 */     OP_lsl,          /* &second_byte[0x03], */ /**< lsl opcode */
/*  95 */     OP_syscall,      /* &second_byte[0x05], */ /**< syscall opcode */
/*  96 */     OP_clts,         /* &second_byte[0x06], */ /**< clts opcode */
/*  97 */     OP_sysret,       /* &second_byte[0x07], */ /**< sysret opcode */
/*  98 */     OP_invd,         /* &second_byte[0x08], */ /**< invd opcode */
/*  99 */     OP_wbinvd,       /* &second_byte[0x09], */ /**< wbinvd opcode */
/* 100 */     OP_ud2a,         /* &second_byte[0x0b], */ /**< ud2a opcode */
/* 101 */     OP_nop_modrm,    /* &second_byte[0x1f], */ /**< nop_modrm opcode */
/* 102 */     OP_movntps,      /* &prefix_extensions[11][0], */ /**< movntps opcode */
/* 103 */     OP_movntpd,      /* &prefix_extensions[11][2], */ /**< movntpd opcode */
/* 104 */     OP_wrmsr,        /* &second_byte[0x30], */ /**< wrmsr opcode */
/* 105 */     OP_rdtsc,        /* &second_byte[0x31], */ /**< rdtsc opcode */
/* 106 */     OP_rdmsr,        /* &second_byte[0x32], */ /**< rdmsr opcode */
/* 107 */     OP_rdpmc,        /* &second_byte[0x33], */ /**< rdpmc opcode */
/* 108 */     OP_sysenter,     /* &second_byte[0x34], */ /**< sysenter opcode */
/* 109 */     OP_sysexit,      /* &second_byte[0x35], */ /**< sysexit opcode */

/* 110 */     OP_cmovo,        /* &second_byte[0x40], */ /**< cmovo opcode */
/* 111 */     OP_cmovno,       /* &second_byte[0x41], */ /**< cmovno opcode */
/* 112 */     OP_cmovb,        /* &second_byte[0x42], */ /**< cmovb opcode */
/* 113 */     OP_cmovnb,       /* &second_byte[0x43], */ /**< cmovnb opcode */
/* 114 */     OP_cmovz,        /* &second_byte[0x44], */ /**< cmovz opcode */
/* 115 */     OP_cmovnz,       /* &second_byte[0x45], */ /**< cmovnz opcode */
/* 116 */     OP_cmovbe,       /* &second_byte[0x46], */ /**< cmovbe opcode */
/* 117 */     OP_cmovnbe,      /* &second_byte[0x47], */ /**< cmovnbe opcode */
/* 118 */     OP_cmovs,        /* &second_byte[0x48], */ /**< cmovs opcode */
/* 119 */     OP_cmovns,       /* &second_byte[0x49], */ /**< cmovns opcode */
/* 120 */     OP_cmovp,        /* &second_byte[0x4a], */ /**< cmovp opcode */
/* 121 */     OP_cmovnp,       /* &second_byte[0x4b], */ /**< cmovnp opcode */
/* 122 */     OP_cmovl,        /* &second_byte[0x4c], */ /**< cmovl opcode */
/* 123 */     OP_cmovnl,       /* &second_byte[0x4d], */ /**< cmovnl opcode */
/* 124 */     OP_cmovle,       /* &second_byte[0x4e], */ /**< cmovle opcode */
/* 125 */     OP_cmovnle,      /* &second_byte[0x4f], */ /**< cmovnle opcode */

/* 126 */     OP_punpcklbw,    /* &prefix_extensions[32][0], */ /**< punpcklbw opcode */
/* 127 */     OP_punpcklwd,    /* &prefix_extensions[33][0], */ /**< punpcklwd opcode */
/* 128 */     OP_punpckldq,    /* &prefix_extensions[34][0], */ /**< punpckldq opcode */
/* 129 */     OP_packsswb,     /* &prefix_extensions[35][0], */ /**< packsswb opcode */
/* 130 */     OP_pcmpgtb,      /* &prefix_extensions[36][0], */ /**< pcmpgtb opcode */
/* 131 */     OP_pcmpgtw,      /* &prefix_extensions[37][0], */ /**< pcmpgtw opcode */
/* 132 */     OP_pcmpgtd,      /* &prefix_extensions[38][0], */ /**< pcmpgtd opcode */
/* 133 */     OP_packuswb,     /* &prefix_extensions[39][0], */ /**< packuswb opcode */
/* 134 */     OP_punpckhbw,    /* &prefix_extensions[40][0], */ /**< punpckhbw opcode */
/* 135 */     OP_punpckhwd,    /* &prefix_extensions[41][0], */ /**< punpckhwd opcode */
/* 136 */     OP_punpckhdq,    /* &prefix_extensions[42][0], */ /**< punpckhdq opcode */
/* 137 */     OP_packssdw,     /* &prefix_extensions[43][0], */ /**< packssdw opcode */
/* 138 */     OP_punpcklqdq,   /* &prefix_extensions[44][2], */ /**< punpcklqdq opcode */
/* 139 */     OP_punpckhqdq,   /* &prefix_extensions[45][2], */ /**< punpckhqdq opcode */
/* 140 */     OP_movd,         /* &prefix_extensions[46][0], */ /**< movd opcode */
/* 141 */     OP_movq,         /* &prefix_extensions[112][0], */ /**< movq opcode */
/* 142 */     OP_movdqu,       /* &prefix_extensions[112][1],  */ /**< movdqu opcode */
/* 143 */     OP_movdqa,       /* &prefix_extensions[112][2], */ /**< movdqa opcode */
/* 144 */     OP_pshufw,       /* &prefix_extensions[47][0], */ /**< pshufw opcode */
/* 145 */     OP_pshufd,       /* &prefix_extensions[47][2], */ /**< pshufd opcode */
/* 146 */     OP_pshufhw,      /* &prefix_extensions[47][1], */ /**< pshufhw opcode */
/* 147 */     OP_pshuflw,      /* &prefix_extensions[47][3], */ /**< pshuflw opcode */
/* 148 */     OP_pcmpeqb,      /* &prefix_extensions[48][0], */ /**< pcmpeqb opcode */
/* 149 */     OP_pcmpeqw,      /* &prefix_extensions[49][0], */ /**< pcmpeqw opcode */
/* 150 */     OP_pcmpeqd,      /* &prefix_extensions[50][0], */ /**< pcmpeqd opcode */
/* 151 */     OP_emms,         /* &vex_L_extensions[0][0], */ /**< emms opcode */

/* 152 */     OP_jo,       /* &second_byte[0x80], */ /**< jo opcode */
/* 153 */     OP_jno,      /* &second_byte[0x81], */ /**< jno opcode */
/* 154 */     OP_jb,       /* &second_byte[0x82], */ /**< jb opcode */
/* 155 */     OP_jnb,      /* &second_byte[0x83], */ /**< jnb opcode */
/* 156 */     OP_jz,       /* &second_byte[0x84], */ /**< jz opcode */
/* 157 */     OP_jnz,      /* &second_byte[0x85], */ /**< jnz opcode */
/* 158 */     OP_jbe,      /* &second_byte[0x86], */ /**< jbe opcode */
/* 159 */     OP_jnbe,     /* &second_byte[0x87], */ /**< jnbe opcode */
/* 160 */     OP_js,       /* &second_byte[0x88], */ /**< js opcode */
/* 161 */     OP_jns,      /* &second_byte[0x89], */ /**< jns opcode */
/* 162 */     OP_jp,       /* &second_byte[0x8a], */ /**< jp opcode */
/* 163 */     OP_jnp,      /* &second_byte[0x8b], */ /**< jnp opcode */
/* 164 */     OP_jl,       /* &second_byte[0x8c], */ /**< jl opcode */
/* 165 */     OP_jnl,      /* &second_byte[0x8d], */ /**< jnl opcode */
/* 166 */     OP_jle,      /* &second_byte[0x8e], */ /**< jle opcode */
/* 167 */     OP_jnle,     /* &second_byte[0x8f], */ /**< jnle opcode */

/* 168 */     OP_seto,         /* &second_byte[0x90], */ /**< seto opcode */
/* 169 */     OP_setno,        /* &second_byte[0x91], */ /**< setno opcode */
/* 170 */     OP_setb,         /* &second_byte[0x92], */ /**< setb opcode */
/* 171 */     OP_setnb,        /* &second_byte[0x93], */ /**< setnb opcode */
/* 172 */     OP_setz,         /* &second_byte[0x94], */ /**< setz opcode */
/* 173 */     OP_setnz,        /* &second_byte[0x95], */ /**< setnz opcode */
/* 174 */     OP_setbe,        /* &second_byte[0x96], */ /**< setbe opcode */
/* 175 */     OP_setnbe,         /* &second_byte[0x97], */ /**< setnbe opcode */
/* 176 */     OP_sets,         /* &second_byte[0x98], */ /**< sets opcode */
/* 177 */     OP_setns,        /* &second_byte[0x99], */ /**< setns opcode */
/* 178 */     OP_setp,         /* &second_byte[0x9a], */ /**< setp opcode */
/* 179 */     OP_setnp,        /* &second_byte[0x9b], */ /**< setnp opcode */
/* 180 */     OP_setl,         /* &second_byte[0x9c], */ /**< setl opcode */
/* 181 */     OP_setnl,        /* &second_byte[0x9d], */ /**< setnl opcode */
/* 182 */     OP_setle,        /* &second_byte[0x9e], */ /**< setle opcode */
/* 183 */     OP_setnle,         /* &second_byte[0x9f], */ /**< setnle opcode */

/* 184 */     OP_cpuid,        /* &second_byte[0xa2], */ /**< cpuid opcode */
/* 185 */     OP_bt,           /* &second_byte[0xa3], */ /**< bt opcode */
/* 186 */     OP_shld,         /* &second_byte[0xa4], */ /**< shld opcode */
/* 187 */     OP_rsm,          /* &second_byte[0xaa], */ /**< rsm opcode */
/* 188 */     OP_bts,          /* &second_byte[0xab], */ /**< bts opcode */
/* 189 */     OP_shrd,         /* &second_byte[0xac], */ /**< shrd opcode */
/* 190 */     OP_cmpxchg,      /* &second_byte[0xb1], */ /**< cmpxchg opcode */
/* 191 */     OP_lss,          /* &second_byte[0xb2], */ /**< lss opcode */
/* 192 */     OP_btr,          /* &second_byte[0xb3], */ /**< btr opcode */
/* 193 */     OP_lfs,          /* &second_byte[0xb4], */ /**< lfs opcode */
/* 194 */     OP_lgs,          /* &second_byte[0xb5], */ /**< lgs opcode */
/* 195 */     OP_movzx,        /* &second_byte[0xb7], */ /**< movzx opcode */
/* 196 */     OP_ud2b,         /* &second_byte[0xb9], */ /**< ud2b opcode */
/* 197 */     OP_btc,          /* &second_byte[0xbb], */ /**< btc opcode */
/* 198 */     OP_bsf,          /* &prefix_extensions[140][0], */ /**< bsf opcode */
/* 199 */     OP_bsr,          /* &prefix_extensions[136][0], */ /**< bsr opcode */
/* 200 */     OP_movsx,        /* &second_byte[0xbf], */ /**< movsx opcode */
/* 201 */     OP_xadd,         /* &second_byte[0xc1], */ /**< xadd opcode */
/* 202 */     OP_movnti,       /* &second_byte[0xc3], */ /**< movnti opcode */
/* 203 */     OP_pinsrw,       /* &prefix_extensions[53][0], */ /**< pinsrw opcode */
/* 204 */     OP_pextrw,       /* &prefix_extensions[54][0], */ /**< pextrw opcode */
/* 205 */     OP_bswap,        /* &second_byte[0xc8], */ /**< bswap opcode */
/* 206 */     OP_psrlw,        /* &prefix_extensions[56][0], */ /**< psrlw opcode */
/* 207 */     OP_psrld,        /* &prefix_extensions[57][0], */ /**< psrld opcode */
/* 208 */     OP_psrlq,        /* &prefix_extensions[58][0], */ /**< psrlq opcode */
/* 209 */     OP_paddq,        /* &prefix_extensions[59][0], */ /**< paddq opcode */
/* 210 */     OP_pmullw,       /* &prefix_extensions[60][0], */ /**< pmullw opcode */
/* 211 */     OP_pmovmskb,     /* &prefix_extensions[62][0], */ /**< pmovmskb opcode */
/* 212 */     OP_psubusb,      /* &prefix_extensions[63][0], */ /**< psubusb opcode */
/* 213 */     OP_psubusw,      /* &prefix_extensions[64][0], */ /**< psubusw opcode */
/* 214 */     OP_pminub,       /* &prefix_extensions[65][0], */ /**< pminub opcode */
/* 215 */     OP_pand,         /* &prefix_extensions[66][0], */ /**< pand opcode */
/* 216 */     OP_paddusb,      /* &prefix_extensions[67][0], */ /**< paddusb opcode */
/* 217 */     OP_paddusw,      /* &prefix_extensions[68][0], */ /**< paddusw opcode */
/* 218 */     OP_pmaxub,       /* &prefix_extensions[69][0], */ /**< pmaxub opcode */
/* 219 */     OP_pandn,        /* &prefix_extensions[70][0], */ /**< pandn opcode */
/* 220 */     OP_pavgb,        /* &prefix_extensions[71][0], */ /**< pavgb opcode */
/* 221 */     OP_psraw,        /* &prefix_extensions[72][0], */ /**< psraw opcode */
/* 222 */     OP_psrad,        /* &prefix_extensions[73][0], */ /**< psrad opcode */
/* 223 */     OP_pavgw,        /* &prefix_extensions[74][0], */ /**< pavgw opcode */
/* 224 */     OP_pmulhuw,      /* &prefix_extensions[75][0], */ /**< pmulhuw opcode */
/* 225 */     OP_pmulhw,       /* &prefix_extensions[76][0], */ /**< pmulhw opcode */
/* 226 */     OP_movntq,       /* &prefix_extensions[78][0], */ /**< movntq opcode */
/* 227 */     OP_movntdq,      /* &prefix_extensions[78][2], */ /**< movntdq opcode */
/* 228 */     OP_psubsb,       /* &prefix_extensions[79][0], */ /**< psubsb opcode */
/* 229 */     OP_psubsw,       /* &prefix_extensions[80][0], */ /**< psubsw opcode */
/* 230 */     OP_pminsw,       /* &prefix_extensions[81][0], */ /**< pminsw opcode */
/* 231 */     OP_por,          /* &prefix_extensions[82][0], */ /**< por opcode */
/* 232 */     OP_paddsb,       /* &prefix_extensions[83][0], */ /**< paddsb opcode */
/* 233 */     OP_paddsw,       /* &prefix_extensions[84][0], */ /**< paddsw opcode */
/* 234 */     OP_pmaxsw,       /* &prefix_extensions[85][0], */ /**< pmaxsw opcode */
/* 235 */     OP_pxor,         /* &prefix_extensions[86][0], */ /**< pxor opcode */
/* 236 */     OP_psllw,        /* &prefix_extensions[87][0], */ /**< psllw opcode */
/* 237 */     OP_pslld,        /* &prefix_extensions[88][0], */ /**< pslld opcode */
/* 238 */     OP_psllq,        /* &prefix_extensions[89][0], */ /**< psllq opcode */
/* 239 */     OP_pmuludq,      /* &prefix_extensions[90][0], */ /**< pmuludq opcode */
/* 240 */     OP_pmaddwd,      /* &prefix_extensions[91][0], */ /**< pmaddwd opcode */
/* 241 */     OP_psadbw,       /* &prefix_extensions[92][0], */ /**< psadbw opcode */
/* 242 */     OP_maskmovq,     /* &prefix_extensions[93][0], */ /**< maskmovq opcode */
/* 243 */     OP_maskmovdqu,   /* &prefix_extensions[93][2], */ /**< maskmovdqu opcode */
/* 244 */     OP_psubb,        /* &prefix_extensions[94][0], */ /**< psubb opcode */
/* 245 */     OP_psubw,        /* &prefix_extensions[95][0], */ /**< psubw opcode */
/* 246 */     OP_psubd,        /* &prefix_extensions[96][0], */ /**< psubd opcode */
/* 247 */     OP_psubq,        /* &prefix_extensions[97][0], */ /**< psubq opcode */
/* 248 */     OP_paddb,        /* &prefix_extensions[98][0], */ /**< paddb opcode */
/* 249 */     OP_paddw,        /* &prefix_extensions[99][0], */ /**< paddw opcode */
/* 250 */     OP_paddd,        /* &prefix_extensions[100][0], */ /**< paddd opcode */
/* 251 */     OP_psrldq,       /* &prefix_extensions[101][2], */ /**< psrldq opcode */
/* 252 */     OP_pslldq,       /* &prefix_extensions[102][2], */ /**< pslldq opcode */


/* 253 */     OP_rol,           /* &extensions[ 4][0], */ /**< rol opcode */
/* 254 */     OP_ror,           /* &extensions[ 4][1], */ /**< ror opcode */
/* 255 */     OP_rcl,           /* &extensions[ 4][2], */ /**< rcl opcode */
/* 256 */     OP_rcr,           /* &extensions[ 4][3], */ /**< rcr opcode */
/* 257 */     OP_shl,           /* &extensions[ 4][4], */ /**< shl opcode */
/* 258 */     OP_shr,           /* &extensions[ 4][5], */ /**< shr opcode */
/* 259 */     OP_sar,           /* &extensions[ 4][7], */ /**< sar opcode */
/* 260 */     OP_not,           /* &extensions[10][2], */ /**< not opcode */
/* 261 */     OP_neg,           /* &extensions[10][3], */ /**< neg opcode */
/* 262 */     OP_mul,           /* &extensions[10][4], */ /**< mul opcode */
/* 263 */     OP_div,           /* &extensions[10][6], */ /**< div opcode */
/* 264 */     OP_idiv,          /* &extensions[10][7], */ /**< idiv opcode */
/* 265 */     OP_sldt,          /* &extensions[13][0], */ /**< sldt opcode */
/* 266 */     OP_str,           /* &extensions[13][1], */ /**< str opcode */
/* 267 */     OP_lldt,          /* &extensions[13][2], */ /**< lldt opcode */
/* 268 */     OP_ltr,           /* &extensions[13][3], */ /**< ltr opcode */
/* 269 */     OP_verr,          /* &extensions[13][4], */ /**< verr opcode */
/* 270 */     OP_verw,          /* &extensions[13][5], */ /**< verw opcode */
/* 271 */     OP_sgdt,          /* &mod_extensions[0][0], */ /**< sgdt opcode */
/* 272 */     OP_sidt,          /* &mod_extensions[1][0], */ /**< sidt opcode */
/* 273 */     OP_lgdt,          /* &mod_extensions[5][0], */ /**< lgdt opcode */
/* 274 */     OP_lidt,          /* &mod_extensions[4][0], */ /**< lidt opcode */
/* 275 */     OP_smsw,          /* &extensions[14][4], */ /**< smsw opcode */
/* 276 */     OP_lmsw,          /* &extensions[14][6], */ /**< lmsw opcode */
/* 277 */     OP_invlpg,        /* &mod_extensions[2][0], */ /**< invlpg opcode */
/* 278 */     OP_cmpxchg8b,     /* &extensions[16][1], */ /**< cmpxchg8b opcode */
/* 279 */     OP_fxsave32,      /* &rex_w_extensions[0][0], */ /**< fxsave opcode */
/* 280 */     OP_fxrstor32,     /* &rex_w_extensions[1][0], */ /**< fxrstor opcode */
/* 281 */     OP_ldmxcsr,       /* &vex_extensions[61][0], */ /**< ldmxcsr opcode */
/* 282 */     OP_stmxcsr,       /* &vex_extensions[62][0], */ /**< stmxcsr opcode */
/* 283 */     OP_lfence,        /* &mod_extensions[6][1], */ /**< lfence opcode */
/* 284 */     OP_mfence,        /* &mod_extensions[7][1], */ /**< mfence opcode */
/* 285 */     OP_clflush,       /* &mod_extensions[3][0], */ /**< clflush opcode */
/* 286 */     OP_sfence,        /* &mod_extensions[3][1], */ /**< sfence opcode */
/* 287 */     OP_prefetchnta,   /* &extensions[23][0], */ /**< prefetchnta opcode */
/* 288 */     OP_prefetcht0,    /* &extensions[23][1], */ /**< prefetcht0 opcode */
/* 289 */     OP_prefetcht1,    /* &extensions[23][2], */ /**< prefetcht1 opcode */
/* 290 */     OP_prefetcht2,    /* &extensions[23][3], */ /**< prefetcht2 opcode */
/* 291 */     OP_prefetch,      /* &extensions[24][0], */ /**< prefetch opcode */
/* 292 */     OP_prefetchw,     /* &extensions[24][1], */ /**< prefetchw opcode */


/* 293 */     OP_movups,      /* &prefix_extensions[ 0][0], */ /**< movups opcode */
/* 294 */     OP_movss,       /* &prefix_extensions[ 0][1], */ /**< movss opcode */
/* 295 */     OP_movupd,      /* &prefix_extensions[ 0][2], */ /**< movupd opcode */
/* 296 */     OP_movsd,       /* &prefix_extensions[ 0][3], */ /**< movsd opcode */
/* 297 */     OP_movlps,      /* &prefix_extensions[ 2][0], */ /**< movlps opcode */
/* 298 */     OP_movlpd,      /* &prefix_extensions[ 2][2], */ /**< movlpd opcode */
/* 299 */     OP_unpcklps,    /* &prefix_extensions[ 4][0], */ /**< unpcklps opcode */
/* 300 */     OP_unpcklpd,    /* &prefix_extensions[ 4][2], */ /**< unpcklpd opcode */
/* 301 */     OP_unpckhps,    /* &prefix_extensions[ 5][0], */ /**< unpckhps opcode */
/* 302 */     OP_unpckhpd,    /* &prefix_extensions[ 5][2], */ /**< unpckhpd opcode */
/* 303 */     OP_movhps,      /* &prefix_extensions[ 6][0], */ /**< movhps opcode */
/* 304 */     OP_movhpd,      /* &prefix_extensions[ 6][2], */ /**< movhpd opcode */
/* 305 */     OP_movaps,      /* &prefix_extensions[ 8][0], */ /**< movaps opcode */
/* 306 */     OP_movapd,      /* &prefix_extensions[ 8][2], */ /**< movapd opcode */
/* 307 */     OP_cvtpi2ps,    /* &prefix_extensions[10][0], */ /**< cvtpi2ps opcode */
/* 308 */     OP_cvtsi2ss,    /* &prefix_extensions[10][1], */ /**< cvtsi2ss opcode */
/* 309 */     OP_cvtpi2pd,    /* &prefix_extensions[10][2], */ /**< cvtpi2pd opcode */
/* 310 */     OP_cvtsi2sd,    /* &prefix_extensions[10][3], */ /**< cvtsi2sd opcode */
/* 311 */     OP_cvttps2pi,   /* &prefix_extensions[12][0], */ /**< cvttps2pi opcode */
/* 312 */     OP_cvttss2si,   /* &prefix_extensions[12][1], */ /**< cvttss2si opcode */
/* 313 */     OP_cvttpd2pi,   /* &prefix_extensions[12][2], */ /**< cvttpd2pi opcode */
/* 314 */     OP_cvttsd2si,   /* &prefix_extensions[12][3], */ /**< cvttsd2si opcode */
/* 315 */     OP_cvtps2pi,    /* &prefix_extensions[13][0], */ /**< cvtps2pi opcode */
/* 316 */     OP_cvtss2si,    /* &prefix_extensions[13][1], */ /**< cvtss2si opcode */
/* 317 */     OP_cvtpd2pi,    /* &prefix_extensions[13][2], */ /**< cvtpd2pi opcode */
/* 318 */     OP_cvtsd2si,    /* &prefix_extensions[13][3], */ /**< cvtsd2si opcode */
/* 319 */     OP_ucomiss,     /* &prefix_extensions[14][0], */ /**< ucomiss opcode */
/* 320 */     OP_ucomisd,     /* &prefix_extensions[14][2], */ /**< ucomisd opcode */
/* 321 */     OP_comiss,      /* &prefix_extensions[15][0], */ /**< comiss opcode */
/* 322 */     OP_comisd,      /* &prefix_extensions[15][2], */ /**< comisd opcode */
/* 323 */     OP_movmskps,    /* &prefix_extensions[16][0], */ /**< movmskps opcode */
/* 324 */     OP_movmskpd,    /* &prefix_extensions[16][2], */ /**< movmskpd opcode */
/* 325 */     OP_sqrtps,      /* &prefix_extensions[17][0], */ /**< sqrtps opcode */
/* 326 */     OP_sqrtss,      /* &prefix_extensions[17][1], */ /**< sqrtss opcode */
/* 327 */     OP_sqrtpd,      /* &prefix_extensions[17][2], */ /**< sqrtpd opcode */
/* 328 */     OP_sqrtsd,      /* &prefix_extensions[17][3], */ /**< sqrtsd opcode */
/* 329 */     OP_rsqrtps,     /* &prefix_extensions[18][0], */ /**< rsqrtps opcode */
/* 330 */     OP_rsqrtss,     /* &prefix_extensions[18][1], */ /**< rsqrtss opcode */
/* 331 */     OP_rcpps,       /* &prefix_extensions[19][0], */ /**< rcpps opcode */
/* 332 */     OP_rcpss,       /* &prefix_extensions[19][1], */ /**< rcpss opcode */
/* 333 */     OP_andps,       /* &prefix_extensions[20][0], */ /**< andps opcode */
/* 334 */     OP_andpd,       /* &prefix_extensions[20][2], */ /**< andpd opcode */
/* 335 */     OP_andnps,      /* &prefix_extensions[21][0], */ /**< andnps opcode */
/* 336 */     OP_andnpd,      /* &prefix_extensions[21][2], */ /**< andnpd opcode */
/* 337 */     OP_orps,        /* &prefix_extensions[22][0], */ /**< orps opcode */
/* 338 */     OP_orpd,        /* &prefix_extensions[22][2], */ /**< orpd opcode */
/* 339 */     OP_xorps,       /* &prefix_extensions[23][0], */ /**< xorps opcode */
/* 340 */     OP_xorpd,       /* &prefix_extensions[23][2], */ /**< xorpd opcode */
/* 341 */     OP_addps,       /* &prefix_extensions[24][0], */ /**< addps opcode */
/* 342 */     OP_addss,       /* &prefix_extensions[24][1], */ /**< addss opcode */
/* 343 */     OP_addpd,       /* &prefix_extensions[24][2], */ /**< addpd opcode */
/* 344 */     OP_addsd,       /* &prefix_extensions[24][3], */ /**< addsd opcode */
/* 345 */     OP_mulps,       /* &prefix_extensions[25][0], */ /**< mulps opcode */
/* 346 */     OP_mulss,       /* &prefix_extensions[25][1], */ /**< mulss opcode */
/* 347 */     OP_mulpd,       /* &prefix_extensions[25][2], */ /**< mulpd opcode */
/* 348 */     OP_mulsd,       /* &prefix_extensions[25][3], */ /**< mulsd opcode */
/* 349 */     OP_cvtps2pd,    /* &prefix_extensions[26][0], */ /**< cvtps2pd opcode */
/* 350 */     OP_cvtss2sd,    /* &prefix_extensions[26][1], */ /**< cvtss2sd opcode */
/* 351 */     OP_cvtpd2ps,    /* &prefix_extensions[26][2], */ /**< cvtpd2ps opcode */
/* 352 */     OP_cvtsd2ss,    /* &prefix_extensions[26][3], */ /**< cvtsd2ss opcode */
/* 353 */     OP_cvtdq2ps,    /* &prefix_extensions[27][0], */ /**< cvtdq2ps opcode */
/* 354 */     OP_cvttps2dq,   /* &prefix_extensions[27][1], */ /**< cvttps2dq opcode */
/* 355 */     OP_cvtps2dq,    /* &prefix_extensions[27][2], */ /**< cvtps2dq opcode */
/* 356 */     OP_subps,       /* &prefix_extensions[28][0], */ /**< subps opcode */
/* 357 */     OP_subss,       /* &prefix_extensions[28][1], */ /**< subss opcode */
/* 358 */     OP_subpd,       /* &prefix_extensions[28][2], */ /**< subpd opcode */
/* 359 */     OP_subsd,       /* &prefix_extensions[28][3], */ /**< subsd opcode */
/* 360 */     OP_minps,       /* &prefix_extensions[29][0], */ /**< minps opcode */
/* 361 */     OP_minss,       /* &prefix_extensions[29][1], */ /**< minss opcode */
/* 362 */     OP_minpd,       /* &prefix_extensions[29][2], */ /**< minpd opcode */
/* 363 */     OP_minsd,       /* &prefix_extensions[29][3], */ /**< minsd opcode */
/* 364 */     OP_divps,       /* &prefix_extensions[30][0], */ /**< divps opcode */
/* 365 */     OP_divss,       /* &prefix_extensions[30][1], */ /**< divss opcode */
/* 366 */     OP_divpd,       /* &prefix_extensions[30][2], */ /**< divpd opcode */
/* 367 */     OP_divsd,       /* &prefix_extensions[30][3], */ /**< divsd opcode */
/* 368 */     OP_maxps,       /* &prefix_extensions[31][0], */ /**< maxps opcode */
/* 369 */     OP_maxss,       /* &prefix_extensions[31][1], */ /**< maxss opcode */
/* 370 */     OP_maxpd,       /* &prefix_extensions[31][2], */ /**< maxpd opcode */
/* 371 */     OP_maxsd,       /* &prefix_extensions[31][3], */ /**< maxsd opcode */
/* 372 */     OP_cmpps,       /* &prefix_extensions[52][0], */ /**< cmpps opcode */
/* 373 */     OP_cmpss,       /* &prefix_extensions[52][1], */ /**< cmpss opcode */
/* 374 */     OP_cmppd,       /* &prefix_extensions[52][2], */ /**< cmppd opcode */
/* 375 */     OP_cmpsd,       /* &prefix_extensions[52][3], */ /**< cmpsd opcode */
/* 376 */     OP_shufps,      /* &prefix_extensions[55][0], */ /**< shufps opcode */
/* 377 */     OP_shufpd,      /* &prefix_extensions[55][2], */ /**< shufpd opcode */
/* 378 */     OP_cvtdq2pd,    /* &prefix_extensions[77][1], */ /**< cvtdq2pd opcode */
/* 379 */     OP_cvttpd2dq,   /* &prefix_extensions[77][2], */ /**< cvttpd2dq opcode */
/* 380 */     OP_cvtpd2dq,    /* &prefix_extensions[77][3], */ /**< cvtpd2dq opcode */
/* 381 */     OP_nop,         /* &rex_b_extensions[0][0], */ /**< nop opcode */
/* 382 */     OP_pause,       /* &prefix_extensions[103][1], */ /**< pause opcode */

/* 383 */     OP_ins,          /* &rep_extensions[1][0], */ /**< ins opcode */
/* 384 */     OP_rep_ins,      /* &rep_extensions[1][2], */ /**< rep_ins opcode */
/* 385 */     OP_outs,         /* &rep_extensions[3][0], */ /**< outs opcode */
/* 386 */     OP_rep_outs,     /* &rep_extensions[3][2], */ /**< rep_outs opcode */
/* 387 */     OP_movs,         /* &rep_extensions[5][0], */ /**< movs opcode */
/* 388 */     OP_rep_movs,     /* &rep_extensions[5][2], */ /**< rep_movs opcode */
/* 389 */     OP_stos,         /* &rep_extensions[7][0], */ /**< stos opcode */
/* 390 */     OP_rep_stos,     /* &rep_extensions[7][2], */ /**< rep_stos opcode */
/* 391 */     OP_lods,         /* &rep_extensions[9][0], */ /**< lods opcode */
/* 392 */     OP_rep_lods,     /* &rep_extensions[9][2], */ /**< rep_lods opcode */
/* 393 */     OP_cmps,         /* &repne_extensions[1][0], */ /**< cmps opcode */
/* 394 */     OP_rep_cmps,     /* &repne_extensions[1][2], */ /**< rep_cmps opcode */
/* 395 */     OP_repne_cmps,   /* &repne_extensions[1][4], */ /**< repne_cmps opcode */
/* 396 */     OP_scas,         /* &repne_extensions[3][0], */ /**< scas opcode */
/* 397 */     OP_rep_scas,     /* &repne_extensions[3][2], */ /**< rep_scas opcode */
/* 398 */     OP_repne_scas,   /* &repne_extensions[3][4], */ /**< repne_scas opcode */


/* 399 */     OP_fadd,      /* &float_low_modrm[0x00], */ /**< fadd opcode */
/* 400 */     OP_fmul,      /* &float_low_modrm[0x01], */ /**< fmul opcode */
/* 401 */     OP_fcom,      /* &float_low_modrm[0x02], */ /**< fcom opcode */
/* 402 */     OP_fcomp,     /* &float_low_modrm[0x03], */ /**< fcomp opcode */
/* 403 */     OP_fsub,      /* &float_low_modrm[0x04], */ /**< fsub opcode */
/* 404 */     OP_fsubr,     /* &float_low_modrm[0x05], */ /**< fsubr opcode */
/* 405 */     OP_fdiv,      /* &float_low_modrm[0x06], */ /**< fdiv opcode */
/* 406 */     OP_fdivr,     /* &float_low_modrm[0x07], */ /**< fdivr opcode */
/* 407 */     OP_fld,       /* &float_low_modrm[0x08], */ /**< fld opcode */
/* 408 */     OP_fst,       /* &float_low_modrm[0x0a], */ /**< fst opcode */
/* 409 */     OP_fstp,      /* &float_low_modrm[0x0b], */ /**< fstp opcode */
/* 410 */     OP_fldenv,    /* &float_low_modrm[0x0c], */ /**< fldenv opcode */
/* 411 */     OP_fldcw,     /* &float_low_modrm[0x0d], */ /**< fldcw opcode */
/* 412 */     OP_fnstenv,   /* &float_low_modrm[0x0e], */ /**< fnstenv opcode */
/* 413 */     OP_fnstcw,    /* &float_low_modrm[0x0f], */ /**< fnstcw opcode */
/* 414 */     OP_fiadd,     /* &float_low_modrm[0x10], */ /**< fiadd opcode */
/* 415 */     OP_fimul,     /* &float_low_modrm[0x11], */ /**< fimul opcode */
/* 416 */     OP_ficom,     /* &float_low_modrm[0x12], */ /**< ficom opcode */
/* 417 */     OP_ficomp,    /* &float_low_modrm[0x13], */ /**< ficomp opcode */
/* 418 */     OP_fisub,     /* &float_low_modrm[0x14], */ /**< fisub opcode */
/* 419 */     OP_fisubr,    /* &float_low_modrm[0x15], */ /**< fisubr opcode */
/* 420 */     OP_fidiv,     /* &float_low_modrm[0x16], */ /**< fidiv opcode */
/* 421 */     OP_fidivr,    /* &float_low_modrm[0x17], */ /**< fidivr opcode */
/* 422 */     OP_fild,      /* &float_low_modrm[0x18], */ /**< fild opcode */
/* 423 */     OP_fist,      /* &float_low_modrm[0x1a], */ /**< fist opcode */
/* 424 */     OP_fistp,     /* &float_low_modrm[0x1b], */ /**< fistp opcode */
/* 425 */     OP_frstor,    /* &float_low_modrm[0x2c], */ /**< frstor opcode */
/* 426 */     OP_fnsave,    /* &float_low_modrm[0x2e], */ /**< fnsave opcode */
/* 427 */     OP_fnstsw,    /* &float_low_modrm[0x2f], */ /**< fnstsw opcode */

/* 428 */     OP_fbld,      /* &float_low_modrm[0x3c], */ /**< fbld opcode */
/* 429 */     OP_fbstp,     /* &float_low_modrm[0x3e], */ /**< fbstp opcode */


/* 430 */     OP_fxch,       /* &float_high_modrm[1][0x08], */ /**< fxch opcode */
/* 431 */     OP_fnop,       /* &float_high_modrm[1][0x10], */ /**< fnop opcode */
/* 432 */     OP_fchs,       /* &float_high_modrm[1][0x20], */ /**< fchs opcode */
/* 433 */     OP_fabs,       /* &float_high_modrm[1][0x21], */ /**< fabs opcode */
/* 434 */     OP_ftst,       /* &float_high_modrm[1][0x24], */ /**< ftst opcode */
/* 435 */     OP_fxam,       /* &float_high_modrm[1][0x25], */ /**< fxam opcode */
/* 436 */     OP_fld1,       /* &float_high_modrm[1][0x28], */ /**< fld1 opcode */
/* 437 */     OP_fldl2t,     /* &float_high_modrm[1][0x29], */ /**< fldl2t opcode */
/* 438 */     OP_fldl2e,     /* &float_high_modrm[1][0x2a], */ /**< fldl2e opcode */
/* 439 */     OP_fldpi,      /* &float_high_modrm[1][0x2b], */ /**< fldpi opcode */
/* 440 */     OP_fldlg2,     /* &float_high_modrm[1][0x2c], */ /**< fldlg2 opcode */
/* 441 */     OP_fldln2,     /* &float_high_modrm[1][0x2d], */ /**< fldln2 opcode */
/* 442 */     OP_fldz,       /* &float_high_modrm[1][0x2e], */ /**< fldz opcode */
/* 443 */     OP_f2xm1,      /* &float_high_modrm[1][0x30], */ /**< f2xm1 opcode */
/* 444 */     OP_fyl2x,      /* &float_high_modrm[1][0x31], */ /**< fyl2x opcode */
/* 445 */     OP_fptan,      /* &float_high_modrm[1][0x32], */ /**< fptan opcode */
/* 446 */     OP_fpatan,     /* &float_high_modrm[1][0x33], */ /**< fpatan opcode */
/* 447 */     OP_fxtract,    /* &float_high_modrm[1][0x34], */ /**< fxtract opcode */
/* 448 */     OP_fprem1,     /* &float_high_modrm[1][0x35], */ /**< fprem1 opcode */
/* 449 */     OP_fdecstp,    /* &float_high_modrm[1][0x36], */ /**< fdecstp opcode */
/* 450 */     OP_fincstp,    /* &float_high_modrm[1][0x37], */ /**< fincstp opcode */
/* 451 */     OP_fprem,      /* &float_high_modrm[1][0x38], */ /**< fprem opcode */
/* 452 */     OP_fyl2xp1,    /* &float_high_modrm[1][0x39], */ /**< fyl2xp1 opcode */
/* 453 */     OP_fsqrt,      /* &float_high_modrm[1][0x3a], */ /**< fsqrt opcode */
/* 454 */     OP_fsincos,    /* &float_high_modrm[1][0x3b], */ /**< fsincos opcode */
/* 455 */     OP_frndint,    /* &float_high_modrm[1][0x3c], */ /**< frndint opcode */
/* 456 */     OP_fscale,     /* &float_high_modrm[1][0x3d], */ /**< fscale opcode */
/* 457 */     OP_fsin,       /* &float_high_modrm[1][0x3e], */ /**< fsin opcode */
/* 458 */     OP_fcos,       /* &float_high_modrm[1][0x3f], */ /**< fcos opcode */
/* 459 */     OP_fcmovb,     /* &float_high_modrm[2][0x00], */ /**< fcmovb opcode */
/* 460 */     OP_fcmove,     /* &float_high_modrm[2][0x08], */ /**< fcmove opcode */
/* 461 */     OP_fcmovbe,    /* &float_high_modrm[2][0x10], */ /**< fcmovbe opcode */
/* 462 */     OP_fcmovu,     /* &float_high_modrm[2][0x18], */ /**< fcmovu opcode */
/* 463 */     OP_fucompp,    /* &float_high_modrm[2][0x29], */ /**< fucompp opcode */
/* 464 */     OP_fcmovnb,    /* &float_high_modrm[3][0x00], */ /**< fcmovnb opcode */
/* 465 */     OP_fcmovne,    /* &float_high_modrm[3][0x08], */ /**< fcmovne opcode */
/* 466 */     OP_fcmovnbe,   /* &float_high_modrm[3][0x10], */ /**< fcmovnbe opcode */
/* 467 */     OP_fcmovnu,    /* &float_high_modrm[3][0x18], */ /**< fcmovnu opcode */
/* 468 */     OP_fnclex,     /* &float_high_modrm[3][0x22], */ /**< fnclex opcode */
/* 469 */     OP_fninit,     /* &float_high_modrm[3][0x23], */ /**< fninit opcode */
/* 470 */     OP_fucomi,     /* &float_high_modrm[3][0x28], */ /**< fucomi opcode */
/* 471 */     OP_fcomi,      /* &float_high_modrm[3][0x30], */ /**< fcomi opcode */
/* 472 */     OP_ffree,      /* &float_high_modrm[5][0x00], */ /**< ffree opcode */
/* 473 */     OP_fucom,      /* &float_high_modrm[5][0x20], */ /**< fucom opcode */
/* 474 */     OP_fucomp,     /* &float_high_modrm[5][0x28], */ /**< fucomp opcode */
/* 475 */     OP_faddp,      /* &float_high_modrm[6][0x00], */ /**< faddp opcode */
/* 476 */     OP_fmulp,      /* &float_high_modrm[6][0x08], */ /**< fmulp opcode */
/* 477 */     OP_fcompp,     /* &float_high_modrm[6][0x19], */ /**< fcompp opcode */
/* 478 */     OP_fsubrp,     /* &float_high_modrm[6][0x20], */ /**< fsubrp opcode */
/* 479 */     OP_fsubp,      /* &float_high_modrm[6][0x28], */ /**< fsubp opcode */
/* 480 */     OP_fdivrp,     /* &float_high_modrm[6][0x30], */ /**< fdivrp opcode */
/* 481 */     OP_fdivp,      /* &float_high_modrm[6][0x38], */ /**< fdivp opcode */
/* 482 */     OP_fucomip,    /* &float_high_modrm[7][0x28], */ /**< fucomip opcode */
/* 483 */     OP_fcomip,     /* &float_high_modrm[7][0x30], */ /**< fcomip opcode */

    /* SSE3 instructions */
/* 484 */     OP_fisttp,       /* &float_low_modrm[0x29], */ /**< fisttp opcode */
/* 485 */     OP_haddpd,       /* &prefix_extensions[114][2], */ /**< haddpd opcode */
/* 486 */     OP_haddps,       /* &prefix_extensions[114][3], */ /**< haddps opcode */
/* 487 */     OP_hsubpd,       /* &prefix_extensions[115][2], */ /**< hsubpd opcode */
/* 488 */     OP_hsubps,       /* &prefix_extensions[115][3], */ /**< hsubps opcode */
/* 489 */     OP_addsubpd,     /* &prefix_extensions[116][2], */ /**< addsubpd opcode */
/* 490 */     OP_addsubps,     /* &prefix_extensions[116][3], */ /**< addsubps opcode */
/* 491 */     OP_lddqu,        /* &prefix_extensions[117][3], */ /**< lddqu opcode */
/* 492 */     OP_monitor,      /* &rm_extensions[1][0], */ /**< monitor opcode */
/* 493 */     OP_mwait,        /* &rm_extensions[1][1], */ /**< mwait opcode */
/* 494 */     OP_movsldup,     /* &prefix_extensions[ 2][1], */ /**< movsldup opcode */
/* 495 */     OP_movshdup,     /* &prefix_extensions[ 6][1], */ /**< movshdup opcode */
/* 496 */     OP_movddup,      /* &prefix_extensions[ 2][3], */ /**< movddup opcode */

    /* 3D-Now! instructions */
/* 497 */     OP_femms,          /* &second_byte[0x0e], */ /**< femms opcode */
/* 498 */     OP_unknown_3dnow,  /* &suffix_extensions[0], */ /**< unknown_3dnow opcode */
/* 499 */     OP_pavgusb,        /* &suffix_extensions[1], */ /**< pavgusb opcode */
/* 500 */     OP_pfadd,          /* &suffix_extensions[2], */ /**< pfadd opcode */
/* 501 */     OP_pfacc,          /* &suffix_extensions[3], */ /**< pfacc opcode */
/* 502 */     OP_pfcmpge,        /* &suffix_extensions[4], */ /**< pfcmpge opcode */
/* 503 */     OP_pfcmpgt,        /* &suffix_extensions[5], */ /**< pfcmpgt opcode */
/* 504 */     OP_pfcmpeq,        /* &suffix_extensions[6], */ /**< pfcmpeq opcode */
/* 505 */     OP_pfmin,          /* &suffix_extensions[7], */ /**< pfmin opcode */
/* 506 */     OP_pfmax,          /* &suffix_extensions[8], */ /**< pfmax opcode */
/* 507 */     OP_pfmul,          /* &suffix_extensions[9], */ /**< pfmul opcode */
/* 508 */     OP_pfrcp,          /* &suffix_extensions[10], */ /**< pfrcp opcode */
/* 509 */     OP_pfrcpit1,       /* &suffix_extensions[11], */ /**< pfrcpit1 opcode */
/* 510 */     OP_pfrcpit2,       /* &suffix_extensions[12], */ /**< pfrcpit2 opcode */
/* 511 */     OP_pfrsqrt,        /* &suffix_extensions[13], */ /**< pfrsqrt opcode */
/* 512 */     OP_pfrsqit1,       /* &suffix_extensions[14], */ /**< pfrsqit1 opcode */
/* 513 */     OP_pmulhrw,        /* &suffix_extensions[15], */ /**< pmulhrw opcode */
/* 514 */     OP_pfsub,          /* &suffix_extensions[16], */ /**< pfsub opcode */
/* 515 */     OP_pfsubr,         /* &suffix_extensions[17], */ /**< pfsubr opcode */
/* 516 */     OP_pi2fd,          /* &suffix_extensions[18], */ /**< pi2fd opcode */
/* 517 */     OP_pf2id,          /* &suffix_extensions[19], */ /**< pf2id opcode */
/* 518 */     OP_pi2fw,          /* &suffix_extensions[20], */ /**< pi2fw opcode */
/* 519 */     OP_pf2iw,          /* &suffix_extensions[21], */ /**< pf2iw opcode */
/* 520 */     OP_pfnacc,         /* &suffix_extensions[22], */ /**< pfnacc opcode */
/* 521 */     OP_pfpnacc,        /* &suffix_extensions[23], */ /**< pfpnacc opcode */
/* 522 */     OP_pswapd,         /* &suffix_extensions[24], */ /**< pswapd opcode */

    /* SSSE3 */
/* 523 */     OP_pshufb,         /* &prefix_extensions[118][0], */ /**< pshufb opcode */
/* 524 */     OP_phaddw,         /* &prefix_extensions[119][0], */ /**< phaddw opcode */
/* 525 */     OP_phaddd,         /* &prefix_extensions[120][0], */ /**< phaddd opcode */
/* 526 */     OP_phaddsw,        /* &prefix_extensions[121][0], */ /**< phaddsw opcode */
/* 527 */     OP_pmaddubsw,      /* &prefix_extensions[122][0], */ /**< pmaddubsw opcode */
/* 528 */     OP_phsubw,         /* &prefix_extensions[123][0], */ /**< phsubw opcode */
/* 529 */     OP_phsubd,         /* &prefix_extensions[124][0], */ /**< phsubd opcode */
/* 530 */     OP_phsubsw,        /* &prefix_extensions[125][0], */ /**< phsubsw opcode */
/* 531 */     OP_psignb,         /* &prefix_extensions[126][0], */ /**< psignb opcode */
/* 532 */     OP_psignw,         /* &prefix_extensions[127][0], */ /**< psignw opcode */
/* 533 */     OP_psignd,         /* &prefix_extensions[128][0], */ /**< psignd opcode */
/* 534 */     OP_pmulhrsw,       /* &prefix_extensions[129][0], */ /**< pmulhrsw opcode */
/* 535 */     OP_pabsb,          /* &prefix_extensions[130][0], */ /**< pabsb opcode */
/* 536 */     OP_pabsw,          /* &prefix_extensions[131][0], */ /**< pabsw opcode */
/* 537 */     OP_pabsd,          /* &prefix_extensions[132][0], */ /**< pabsd opcode */
/* 538 */     OP_palignr,        /* &prefix_extensions[133][0], */ /**< palignr opcode */

    /* SSE4 (incl AMD (SSE4A) and Intel-specific (SSE4.1, SSE4.2) extensions */
/* 539 */     OP_popcnt,         /* &second_byte[0xb8], */ /**< popcnt opcode */
/* 540 */     OP_movntss,        /* &prefix_extensions[11][1], */ /**< movntss opcode */
/* 541 */     OP_movntsd,        /* &prefix_extensions[11][3], */ /**< movntsd opcode */
/* 542 */     OP_extrq,          /* &prefix_extensions[134][2], */ /**< extrq opcode */
/* 543 */     OP_insertq,        /* &prefix_extensions[134][3], */ /**< insertq opcode */
/* 544 */     OP_lzcnt,          /* &prefix_extensions[136][1], */ /**< lzcnt opcode */
/* 545 */     OP_pblendvb,       /* &third_byte_38[16], */ /**< pblendvb opcode */
/* 546 */     OP_blendvps,       /* &third_byte_38[17], */ /**< blendvps opcode */
/* 547 */     OP_blendvpd,       /* &third_byte_38[18], */ /**< blendvpd opcode */
/* 548 */     OP_ptest,          /* &vex_extensions[3][0], */ /**< ptest opcode */
/* 549 */     OP_pmovsxbw,       /* &vex_extensions[4][0], */ /**< pmovsxbw opcode */
/* 550 */     OP_pmovsxbd,       /* &vex_extensions[5][0], */ /**< pmovsxbd opcode */
/* 551 */     OP_pmovsxbq,       /* &vex_extensions[6][0], */ /**< pmovsxbq opcode */
/* 552 */     OP_pmovsxwd,       /* &vex_extensions[7][0], */ /**< pmovsxwd opcode */
/* 553 */     OP_pmovsxwq,       /* &vex_extensions[8][0], */ /**< pmovsxwq opcode */
/* 554 */     OP_pmovsxdq,       /* &vex_extensions[9][0], */ /**< pmovsxdq opcode */
/* 555 */     OP_pmuldq,         /* &vex_extensions[10][0], */ /**< pmuldq opcode */
/* 556 */     OP_pcmpeqq,        /* &vex_extensions[11][0], */ /**< pcmpeqq opcode */
/* 557 */     OP_movntdqa,       /* &vex_extensions[12][0], */ /**< movntdqa opcode */
/* 558 */     OP_packusdw,       /* &vex_extensions[13][0], */ /**< packusdw opcode */
/* 559 */     OP_pmovzxbw,       /* &vex_extensions[14][0], */ /**< pmovzxbw opcode */
/* 560 */     OP_pmovzxbd,       /* &vex_extensions[15][0], */ /**< pmovzxbd opcode */
/* 561 */     OP_pmovzxbq,       /* &vex_extensions[16][0], */ /**< pmovzxbq opcode */
/* 562 */     OP_pmovzxwd,       /* &vex_extensions[17][0], */ /**< pmovzxwd opcode */
/* 563 */     OP_pmovzxwq,       /* &vex_extensions[18][0], */ /**< pmovzxwq opcode */
/* 564 */     OP_pmovzxdq,       /* &vex_extensions[19][0], */ /**< pmovzxdq opcode */
/* 565 */     OP_pcmpgtq,        /* &vex_extensions[20][0], */ /**< pcmpgtq opcode */
/* 566 */     OP_pminsb,         /* &vex_extensions[21][0], */ /**< pminsb opcode */
/* 567 */     OP_pminsd,         /* &vex_extensions[22][0], */ /**< pminsd opcode */
/* 568 */     OP_pminuw,         /* &vex_extensions[23][0], */ /**< pminuw opcode */
/* 569 */     OP_pminud,         /* &vex_extensions[24][0], */ /**< pminud opcode */
/* 570 */     OP_pmaxsb,         /* &vex_extensions[25][0], */ /**< pmaxsb opcode */
/* 571 */     OP_pmaxsd,         /* &vex_extensions[26][0], */ /**< pmaxsd opcode */
/* 572 */     OP_pmaxuw,         /* &vex_extensions[27][0], */ /**< pmaxuw opcode */
/* 573 */     OP_pmaxud,         /* &vex_extensions[28][0], */ /**< pmaxud opcode */
/* 574 */     OP_pmulld,         /* &vex_extensions[29][0], */ /**< pmulld opcode */
/* 575 */     OP_phminposuw,     /* &vex_extensions[30][0], */ /**< phminposuw opcode */
/* 576 */     OP_crc32,          /* &prefix_extensions[139][3], */ /**< crc32 opcode */
/* 577 */     OP_pextrb,         /* &vex_extensions[36][0], */ /**< pextrb opcode */
/* 578 */     OP_pextrd,         /* &vex_extensions[38][0], */ /**< pextrd opcode */
/* 579 */     OP_extractps,      /* &vex_extensions[39][0], */ /**< extractps opcode */
/* 580 */     OP_roundps,        /* &vex_extensions[40][0], */ /**< roundps opcode */
/* 581 */     OP_roundpd,        /* &vex_extensions[41][0], */ /**< roundpd opcode */
/* 582 */     OP_roundss,        /* &vex_extensions[42][0], */ /**< roundss opcode */
/* 583 */     OP_roundsd,        /* &vex_extensions[43][0], */ /**< roundsd opcode */
/* 584 */     OP_blendps,        /* &vex_extensions[44][0], */ /**< blendps opcode */
/* 585 */     OP_blendpd,        /* &vex_extensions[45][0], */ /**< blendpd opcode */
/* 586 */     OP_pblendw,        /* &vex_extensions[46][0], */ /**< pblendw opcode */
/* 587 */     OP_pinsrb,         /* &vex_extensions[47][0], */ /**< pinsrb opcode */
/* 588 */     OP_insertps,       /* &vex_extensions[48][0], */ /**< insertps opcode */
/* 589 */     OP_pinsrd,         /* &vex_extensions[49][0], */ /**< pinsrd opcode */
/* 590 */     OP_dpps,           /* &vex_extensions[50][0], */ /**< dpps opcode */
/* 591 */     OP_dppd,           /* &vex_extensions[51][0], */ /**< dppd opcode */
/* 592 */     OP_mpsadbw,        /* &vex_extensions[52][0], */ /**< mpsadbw opcode */
/* 593 */     OP_pcmpestrm,      /* &vex_extensions[53][0], */ /**< pcmpestrm opcode */
/* 594 */     OP_pcmpestri,      /* &vex_extensions[54][0], */ /**< pcmpestri opcode */
/* 595 */     OP_pcmpistrm,      /* &vex_extensions[55][0], */ /**< pcmpistrm opcode */
/* 596 */     OP_pcmpistri,      /* &vex_extensions[56][0], */ /**< pcmpistri opcode */

    /* x64 */
/* 597 */     OP_movsxd,         /* &x64_extensions[16][1], */ /**< movsxd opcode */
/* 598 */     OP_swapgs,         /* &rm_extensions[2][0], */ /**< swapgs opcode */

    /* VMX */
/* 599 */     OP_vmcall,         /* &rm_extensions[0][1], */ /**< vmcall opcode */
/* 600 */     OP_vmlaunch,       /* &rm_extensions[0][2], */ /**< vmlaunch opcode */
/* 601 */     OP_vmresume,       /* &rm_extensions[0][3], */ /**< vmresume opcode */
/* 602 */     OP_vmxoff,         /* &rm_extensions[0][4], */ /**< vmxoff opcode */
/* 603 */     OP_vmptrst,        /* &mod_extensions[13][0], */ /**< vmptrst opcode */
/* 604 */     OP_vmptrld,        /* &prefix_extensions[137][0], */ /**< vmptrld opcode */
/* 605 */     OP_vmxon,          /* &prefix_extensions[137][1], */ /**< vmxon opcode */
/* 606 */     OP_vmclear,        /* &prefix_extensions[137][2], */ /**< vmclear opcode */
/* 607 */     OP_vmread,         /* &prefix_extensions[134][0], */ /**< vmread opcode */
/* 608 */     OP_vmwrite,        /* &prefix_extensions[135][0], */ /**< vmwrite opcode */

    /* undocumented */
/* 609 */     OP_int1,           /* &first_byte[0xf1], */ /**< int1 opcode */
/* 610 */     OP_salc,           /* &first_byte[0xd6], */ /**< salc opcode */
/* 611 */     OP_ffreep,         /* &float_high_modrm[7][0x00], */ /**< ffreep opcode */

    /* AMD SVM */
/* 612 */     OP_vmrun,          /* &rm_extensions[3][0], */ /**< vmrun opcode */
/* 613 */     OP_vmmcall,        /* &rm_extensions[3][1], */ /**< vmmcall opcode */
/* 614 */     OP_vmload,         /* &rm_extensions[3][2], */ /**< vmload opcode */
/* 615 */     OP_vmsave,         /* &rm_extensions[3][3], */ /**< vmsave opcode */
/* 616 */     OP_stgi,           /* &rm_extensions[3][4], */ /**< stgi opcode */
/* 617 */     OP_clgi,           /* &rm_extensions[3][5], */ /**< clgi opcode */
/* 618 */     OP_skinit,         /* &rm_extensions[3][6], */ /**< skinit opcode */
/* 619 */     OP_invlpga,        /* &rm_extensions[3][7], */ /**< invlpga opcode */
    /* AMD though not part of SVM */
/* 620 */     OP_rdtscp,         /* &rm_extensions[2][1], */ /**< rdtscp opcode */

    /* Intel VMX additions */
/* 621 */     OP_invept,         /* &third_byte_38[49], */ /**< invept opcode */
/* 622 */     OP_invvpid,        /* &third_byte_38[50], */ /**< invvpid opcode */

    /* added in Intel Westmere */
/* 623 */     OP_pclmulqdq,      /* &vex_extensions[57][0], */ /**< pclmulqdq opcode */
/* 624 */     OP_aesimc,         /* &vex_extensions[31][0], */ /**< aesimc opcode */
/* 625 */     OP_aesenc,         /* &vex_extensions[32][0], */ /**< aesenc opcode */
/* 626 */     OP_aesenclast,     /* &vex_extensions[33][0], */ /**< aesenclast opcode */
/* 627 */     OP_aesdec,         /* &vex_extensions[34][0], */ /**< aesdec opcode */
/* 628 */     OP_aesdeclast,     /* &vex_extensions[35][0], */ /**< aesdeclast opcode */
/* 629 */     OP_aeskeygenassist, /* &vex_extensions[58][0], */ /**< aeskeygenassist opcode */

    /* added in Intel Atom */
/* 630 */     OP_movbe,          /* &prefix_extensions[138][0], */ /**< movbe opcode */

    /* added in Intel Sandy Bridge */
/* 631 */     OP_xgetbv,         /* &rm_extensions[4][0], */ /**< xgetbv opcode */
/* 632 */     OP_xsetbv,         /* &rm_extensions[4][1], */ /**< xsetbv opcode */
/* 633 */     OP_xsave32,        /* &rex_w_extensions[2][0], */ /**< xsave opcode */
/* 634 */     OP_xrstor32,       /* &rex_w_extensions[3][0], */ /**< xrstor opcode */
/* 635 */     OP_xsaveopt32,     /* &rex_w_extensions[4][0], */ /**< xsaveopt opcode */

    /* AVX */
/* 636 */     OP_vmovss,         /* &mod_extensions[ 8][0], */ /**< vmovss opcode */
/* 637 */     OP_vmovsd,         /* &mod_extensions[ 9][0], */ /**< vmovsd opcode */
/* 638 */     OP_vmovups,        /* &prefix_extensions[ 0][4], */ /**< vmovups opcode */
/* 639 */     OP_vmovupd,        /* &prefix_extensions[ 0][6], */ /**< vmovupd opcode */
/* 640 */     OP_vmovlps,        /* &prefix_extensions[ 2][4], */ /**< vmovlps opcode */
/* 641 */     OP_vmovsldup,      /* &prefix_extensions[ 2][5], */ /**< vmovsldup opcode */
/* 642 */     OP_vmovlpd,        /* &prefix_extensions[ 2][6], */ /**< vmovlpd opcode */
/* 643 */     OP_vmovddup,       /* &prefix_extensions[ 2][7], */ /**< vmovddup opcode */
/* 644 */     OP_vunpcklps,      /* &prefix_extensions[ 4][4], */ /**< vunpcklps opcode */
/* 645 */     OP_vunpcklpd,      /* &prefix_extensions[ 4][6], */ /**< vunpcklpd opcode */
/* 646 */     OP_vunpckhps,      /* &prefix_extensions[ 5][4], */ /**< vunpckhps opcode */
/* 647 */     OP_vunpckhpd,      /* &prefix_extensions[ 5][6], */ /**< vunpckhpd opcode */
/* 648 */     OP_vmovhps,        /* &prefix_extensions[ 6][4], */ /**< vmovhps opcode */
/* 649 */     OP_vmovshdup,      /* &prefix_extensions[ 6][5], */ /**< vmovshdup opcode */
/* 650 */     OP_vmovhpd,        /* &prefix_extensions[ 6][6], */ /**< vmovhpd opcode */
/* 651 */     OP_vmovaps,        /* &prefix_extensions[ 8][4], */ /**< vmovaps opcode */
/* 652 */     OP_vmovapd,        /* &prefix_extensions[ 8][6], */ /**< vmovapd opcode */
/* 653 */     OP_vcvtsi2ss,      /* &prefix_extensions[10][5], */ /**< vcvtsi2ss opcode */
/* 654 */     OP_vcvtsi2sd,      /* &prefix_extensions[10][7], */ /**< vcvtsi2sd opcode */
/* 655 */     OP_vmovntps,       /* &prefix_extensions[11][4], */ /**< vmovntps opcode */
/* 656 */     OP_vmovntpd,       /* &prefix_extensions[11][6], */ /**< vmovntpd opcode */
/* 657 */     OP_vcvttss2si,     /* &prefix_extensions[12][5], */ /**< vcvttss2si opcode */
/* 658 */     OP_vcvttsd2si,     /* &prefix_extensions[12][7], */ /**< vcvttsd2si opcode */
/* 659 */     OP_vcvtss2si,      /* &prefix_extensions[13][5], */ /**< vcvtss2si opcode */
/* 660 */     OP_vcvtsd2si,      /* &prefix_extensions[13][7], */ /**< vcvtsd2si opcode */
/* 661 */     OP_vucomiss,       /* &prefix_extensions[14][4], */ /**< vucomiss opcode */
/* 662 */     OP_vucomisd,       /* &prefix_extensions[14][6], */ /**< vucomisd opcode */
/* 663 */     OP_vcomiss,        /* &prefix_extensions[15][4], */ /**< vcomiss opcode */
/* 664 */     OP_vcomisd,        /* &prefix_extensions[15][6], */ /**< vcomisd opcode */
/* 665 */     OP_vmovmskps,      /* &prefix_extensions[16][4], */ /**< vmovmskps opcode */
/* 666 */     OP_vmovmskpd,      /* &prefix_extensions[16][6], */ /**< vmovmskpd opcode */
/* 667 */     OP_vsqrtps,        /* &prefix_extensions[17][4], */ /**< vsqrtps opcode */
/* 668 */     OP_vsqrtss,        /* &prefix_extensions[17][5], */ /**< vsqrtss opcode */
/* 669 */     OP_vsqrtpd,        /* &prefix_extensions[17][6], */ /**< vsqrtpd opcode */
/* 670 */     OP_vsqrtsd,        /* &prefix_extensions[17][7], */ /**< vsqrtsd opcode */
/* 671 */     OP_vrsqrtps,       /* &prefix_extensions[18][4], */ /**< vrsqrtps opcode */
/* 672 */     OP_vrsqrtss,       /* &prefix_extensions[18][5], */ /**< vrsqrtss opcode */
/* 673 */     OP_vrcpps,         /* &prefix_extensions[19][4], */ /**< vrcpps opcode */
/* 674 */     OP_vrcpss,         /* &prefix_extensions[19][5], */ /**< vrcpss opcode */
/* 675 */     OP_vandps,         /* &prefix_extensions[20][4], */ /**< vandps opcode */
/* 676 */     OP_vandpd,         /* &prefix_extensions[20][6], */ /**< vandpd opcode */
/* 677 */     OP_vandnps,        /* &prefix_extensions[21][4], */ /**< vandnps opcode */
/* 678 */     OP_vandnpd,        /* &prefix_extensions[21][6], */ /**< vandnpd opcode */
/* 679 */     OP_vorps,          /* &prefix_extensions[22][4], */ /**< vorps opcode */
/* 680 */     OP_vorpd,          /* &prefix_extensions[22][6], */ /**< vorpd opcode */
/* 681 */     OP_vxorps,         /* &prefix_extensions[23][4], */ /**< vxorps opcode */
/* 682 */     OP_vxorpd,         /* &prefix_extensions[23][6], */ /**< vxorpd opcode */
/* 683 */     OP_vaddps,         /* &prefix_extensions[24][4], */ /**< vaddps opcode */
/* 684 */     OP_vaddss,         /* &prefix_extensions[24][5], */ /**< vaddss opcode */
/* 685 */     OP_vaddpd,         /* &prefix_extensions[24][6], */ /**< vaddpd opcode */
/* 686 */     OP_vaddsd,         /* &prefix_extensions[24][7], */ /**< vaddsd opcode */
/* 687 */     OP_vmulps,         /* &prefix_extensions[25][4], */ /**< vmulps opcode */
/* 688 */     OP_vmulss,         /* &prefix_extensions[25][5], */ /**< vmulss opcode */
/* 689 */     OP_vmulpd,         /* &prefix_extensions[25][6], */ /**< vmulpd opcode */
/* 690 */     OP_vmulsd,         /* &prefix_extensions[25][7], */ /**< vmulsd opcode */
/* 691 */     OP_vcvtps2pd,      /* &prefix_extensions[26][4], */ /**< vcvtps2pd opcode */
/* 692 */     OP_vcvtss2sd,      /* &prefix_extensions[26][5], */ /**< vcvtss2sd opcode */
/* 693 */     OP_vcvtpd2ps,      /* &prefix_extensions[26][6], */ /**< vcvtpd2ps opcode */
/* 694 */     OP_vcvtsd2ss,      /* &prefix_extensions[26][7], */ /**< vcvtsd2ss opcode */
/* 695 */     OP_vcvtdq2ps,      /* &prefix_extensions[27][4], */ /**< vcvtdq2ps opcode */
/* 696 */     OP_vcvttps2dq,     /* &prefix_extensions[27][5], */ /**< vcvttps2dq opcode */
/* 697 */     OP_vcvtps2dq,      /* &prefix_extensions[27][6], */ /**< vcvtps2dq opcode */
/* 698 */     OP_vsubps,         /* &prefix_extensions[28][4], */ /**< vsubps opcode */
/* 699 */     OP_vsubss,         /* &prefix_extensions[28][5], */ /**< vsubss opcode */
/* 700 */     OP_vsubpd,         /* &prefix_extensions[28][6], */ /**< vsubpd opcode */
/* 701 */     OP_vsubsd,         /* &prefix_extensions[28][7], */ /**< vsubsd opcode */
/* 702 */     OP_vminps,         /* &prefix_extensions[29][4], */ /**< vminps opcode */
/* 703 */     OP_vminss,         /* &prefix_extensions[29][5], */ /**< vminss opcode */
/* 704 */     OP_vminpd,         /* &prefix_extensions[29][6], */ /**< vminpd opcode */
/* 705 */     OP_vminsd,         /* &prefix_extensions[29][7], */ /**< vminsd opcode */
/* 706 */     OP_vdivps,         /* &prefix_extensions[30][4], */ /**< vdivps opcode */
/* 707 */     OP_vdivss,         /* &prefix_extensions[30][5], */ /**< vdivss opcode */
/* 708 */     OP_vdivpd,         /* &prefix_extensions[30][6], */ /**< vdivpd opcode */
/* 709 */     OP_vdivsd,         /* &prefix_extensions[30][7], */ /**< vdivsd opcode */
/* 710 */     OP_vmaxps,         /* &prefix_extensions[31][4], */ /**< vmaxps opcode */
/* 711 */     OP_vmaxss,         /* &prefix_extensions[31][5], */ /**< vmaxss opcode */
/* 712 */     OP_vmaxpd,         /* &prefix_extensions[31][6], */ /**< vmaxpd opcode */
/* 713 */     OP_vmaxsd,         /* &prefix_extensions[31][7], */ /**< vmaxsd opcode */
/* 714 */     OP_vpunpcklbw,     /* &prefix_extensions[32][6], */ /**< vpunpcklbw opcode */
/* 715 */     OP_vpunpcklwd,     /* &prefix_extensions[33][6], */ /**< vpunpcklwd opcode */
/* 716 */     OP_vpunpckldq,     /* &prefix_extensions[34][6], */ /**< vpunpckldq opcode */
/* 717 */     OP_vpacksswb,      /* &prefix_extensions[35][6], */ /**< vpacksswb opcode */
/* 718 */     OP_vpcmpgtb,       /* &prefix_extensions[36][6], */ /**< vpcmpgtb opcode */
/* 719 */     OP_vpcmpgtw,       /* &prefix_extensions[37][6], */ /**< vpcmpgtw opcode */
/* 720 */     OP_vpcmpgtd,       /* &prefix_extensions[38][6], */ /**< vpcmpgtd opcode */
/* 721 */     OP_vpackuswb,      /* &prefix_extensions[39][6], */ /**< vpackuswb opcode */
/* 722 */     OP_vpunpckhbw,     /* &prefix_extensions[40][6], */ /**< vpunpckhbw opcode */
/* 723 */     OP_vpunpckhwd,     /* &prefix_extensions[41][6], */ /**< vpunpckhwd opcode */
/* 724 */     OP_vpunpckhdq,     /* &prefix_extensions[42][6], */ /**< vpunpckhdq opcode */
/* 725 */     OP_vpackssdw,      /* &prefix_extensions[43][6], */ /**< vpackssdw opcode */
/* 726 */     OP_vpunpcklqdq,    /* &prefix_extensions[44][6], */ /**< vpunpcklqdq opcode */
/* 727 */     OP_vpunpckhqdq,    /* &prefix_extensions[45][6], */ /**< vpunpckhqdq opcode */
/* 728 */     OP_vmovd,          /* &prefix_extensions[46][6], */ /**< vmovd opcode */
/* 729 */     OP_vpshufhw,       /* &prefix_extensions[47][5], */ /**< vpshufhw opcode */
/* 730 */     OP_vpshufd,        /* &prefix_extensions[47][6], */ /**< vpshufd opcode */
/* 731 */     OP_vpshuflw,       /* &prefix_extensions[47][7], */ /**< vpshuflw opcode */
/* 732 */     OP_vpcmpeqb,       /* &prefix_extensions[48][6], */ /**< vpcmpeqb opcode */
/* 733 */     OP_vpcmpeqw,       /* &prefix_extensions[49][6], */ /**< vpcmpeqw opcode */
/* 734 */     OP_vpcmpeqd,       /* &prefix_extensions[50][6], */ /**< vpcmpeqd opcode */
/* 735 */     OP_vmovq,          /* &prefix_extensions[51][5], */ /**< vmovq opcode */
/* 736 */     OP_vcmpps,         /* &prefix_extensions[52][4], */ /**< vcmpps opcode */
/* 737 */     OP_vcmpss,         /* &prefix_extensions[52][5], */ /**< vcmpss opcode */
/* 738 */     OP_vcmppd,         /* &prefix_extensions[52][6], */ /**< vcmppd opcode */
/* 739 */     OP_vcmpsd,         /* &prefix_extensions[52][7], */ /**< vcmpsd opcode */
/* 740 */     OP_vpinsrw,        /* &prefix_extensions[53][6], */ /**< vpinsrw opcode */
/* 741 */     OP_vpextrw,        /* &prefix_extensions[54][6], */ /**< vpextrw opcode */
/* 742 */     OP_vshufps,        /* &prefix_extensions[55][4], */ /**< vshufps opcode */
/* 743 */     OP_vshufpd,        /* &prefix_extensions[55][6], */ /**< vshufpd opcode */
/* 744 */     OP_vpsrlw,         /* &prefix_extensions[56][6], */ /**< vpsrlw opcode */
/* 745 */     OP_vpsrld,         /* &prefix_extensions[57][6], */ /**< vpsrld opcode */
/* 746 */     OP_vpsrlq,         /* &prefix_extensions[58][6], */ /**< vpsrlq opcode */
/* 747 */     OP_vpaddq,         /* &prefix_extensions[59][6], */ /**< vpaddq opcode */
/* 748 */     OP_vpmullw,        /* &prefix_extensions[60][6], */ /**< vpmullw opcode */
/* 749 */     OP_vpmovmskb,      /* &prefix_extensions[62][6], */ /**< vpmovmskb opcode */
/* 750 */     OP_vpsubusb,       /* &prefix_extensions[63][6], */ /**< vpsubusb opcode */
/* 751 */     OP_vpsubusw,       /* &prefix_extensions[64][6], */ /**< vpsubusw opcode */
/* 752 */     OP_vpminub,        /* &prefix_extensions[65][6], */ /**< vpminub opcode */
/* 753 */     OP_vpand,          /* &prefix_extensions[66][6], */ /**< vpand opcode */
/* 754 */     OP_vpaddusb,       /* &prefix_extensions[67][6], */ /**< vpaddusb opcode */
/* 755 */     OP_vpaddusw,       /* &prefix_extensions[68][6], */ /**< vpaddusw opcode */
/* 756 */     OP_vpmaxub,        /* &prefix_extensions[69][6], */ /**< vpmaxub opcode */
/* 757 */     OP_vpandn,         /* &prefix_extensions[70][6], */ /**< vpandn opcode */
/* 758 */     OP_vpavgb,         /* &prefix_extensions[71][6], */ /**< vpavgb opcode */
/* 759 */     OP_vpsraw,         /* &prefix_extensions[72][6], */ /**< vpsraw opcode */
/* 760 */     OP_vpsrad,         /* &prefix_extensions[73][6], */ /**< vpsrad opcode */
/* 761 */     OP_vpavgw,         /* &prefix_extensions[74][6], */ /**< vpavgw opcode */
/* 762 */     OP_vpmulhuw,       /* &prefix_extensions[75][6], */ /**< vpmulhuw opcode */
/* 763 */     OP_vpmulhw,        /* &prefix_extensions[76][6], */ /**< vpmulhw opcode */
/* 764 */     OP_vcvtdq2pd,      /* &prefix_extensions[77][5], */ /**< vcvtdq2pd opcode */
/* 765 */     OP_vcvttpd2dq,     /* &prefix_extensions[77][6], */ /**< vcvttpd2dq opcode */
/* 766 */     OP_vcvtpd2dq,      /* &prefix_extensions[77][7], */ /**< vcvtpd2dq opcode */
/* 767 */     OP_vmovntdq,       /* &prefix_extensions[78][6], */ /**< vmovntdq opcode */
/* 768 */     OP_vpsubsb,        /* &prefix_extensions[79][6], */ /**< vpsubsb opcode */
/* 769 */     OP_vpsubsw,        /* &prefix_extensions[80][6], */ /**< vpsubsw opcode */
/* 770 */     OP_vpminsw,        /* &prefix_extensions[81][6], */ /**< vpminsw opcode */
/* 771 */     OP_vpor,           /* &prefix_extensions[82][6], */ /**< vpor opcode */
/* 772 */     OP_vpaddsb,        /* &prefix_extensions[83][6], */ /**< vpaddsb opcode */
/* 773 */     OP_vpaddsw,        /* &prefix_extensions[84][6], */ /**< vpaddsw opcode */
/* 774 */     OP_vpmaxsw,        /* &prefix_extensions[85][6], */ /**< vpmaxsw opcode */
/* 775 */     OP_vpxor,          /* &prefix_extensions[86][6], */ /**< vpxor opcode */
/* 776 */     OP_vpsllw,         /* &prefix_extensions[87][6], */ /**< vpsllw opcode */
/* 777 */     OP_vpslld,         /* &prefix_extensions[88][6], */ /**< vpslld opcode */
/* 778 */     OP_vpsllq,         /* &prefix_extensions[89][6], */ /**< vpsllq opcode */
/* 779 */     OP_vpmuludq,       /* &prefix_extensions[90][6], */ /**< vpmuludq opcode */
/* 780 */     OP_vpmaddwd,       /* &prefix_extensions[91][6], */ /**< vpmaddwd opcode */
/* 781 */     OP_vpsadbw,        /* &prefix_extensions[92][6], */ /**< vpsadbw opcode */
/* 782 */     OP_vmaskmovdqu,    /* &prefix_extensions[93][6], */ /**< vmaskmovdqu opcode */
/* 783 */     OP_vpsubb,         /* &prefix_extensions[94][6], */ /**< vpsubb opcode */
/* 784 */     OP_vpsubw,         /* &prefix_extensions[95][6], */ /**< vpsubw opcode */
/* 785 */     OP_vpsubd,         /* &prefix_extensions[96][6], */ /**< vpsubd opcode */
/* 786 */     OP_vpsubq,         /* &prefix_extensions[97][6], */ /**< vpsubq opcode */
/* 787 */     OP_vpaddb,         /* &prefix_extensions[98][6], */ /**< vpaddb opcode */
/* 788 */     OP_vpaddw,         /* &prefix_extensions[99][6], */ /**< vpaddw opcode */
/* 789 */     OP_vpaddd,         /* &prefix_extensions[100][6], */ /**< vpaddd opcode */
/* 790 */     OP_vpsrldq,        /* &prefix_extensions[101][6], */ /**< vpsrldq opcode */
/* 791 */     OP_vpslldq,        /* &prefix_extensions[102][6], */ /**< vpslldq opcode */
/* 792 */     OP_vmovdqu,        /* &prefix_extensions[112][5], */ /**< vmovdqu opcode */
/* 793 */     OP_vmovdqa,        /* &prefix_extensions[112][6], */ /**< vmovdqa opcode */
/* 794 */     OP_vhaddpd,        /* &prefix_extensions[114][6], */ /**< vhaddpd opcode */
/* 795 */     OP_vhaddps,        /* &prefix_extensions[114][7], */ /**< vhaddps opcode */
/* 796 */     OP_vhsubpd,        /* &prefix_extensions[115][6], */ /**< vhsubpd opcode */
/* 797 */     OP_vhsubps,        /* &prefix_extensions[115][7], */ /**< vhsubps opcode */
/* 798 */     OP_vaddsubpd,      /* &prefix_extensions[116][6], */ /**< vaddsubpd opcode */
/* 799 */     OP_vaddsubps,      /* &prefix_extensions[116][7], */ /**< vaddsubps opcode */
/* 800 */     OP_vlddqu,         /* &prefix_extensions[117][7], */ /**< vlddqu opcode */
/* 801 */     OP_vpshufb,        /* &prefix_extensions[118][6], */ /**< vpshufb opcode */
/* 802 */     OP_vphaddw,        /* &prefix_extensions[119][6], */ /**< vphaddw opcode */
/* 803 */     OP_vphaddd,        /* &prefix_extensions[120][6], */ /**< vphaddd opcode */
/* 804 */     OP_vphaddsw,       /* &prefix_extensions[121][6], */ /**< vphaddsw opcode */
/* 805 */     OP_vpmaddubsw,     /* &prefix_extensions[122][6], */ /**< vpmaddubsw opcode */
/* 806 */     OP_vphsubw,        /* &prefix_extensions[123][6], */ /**< vphsubw opcode */
/* 807 */     OP_vphsubd,        /* &prefix_extensions[124][6], */ /**< vphsubd opcode */
/* 808 */     OP_vphsubsw,       /* &prefix_extensions[125][6], */ /**< vphsubsw opcode */
/* 809 */     OP_vpsignb,        /* &prefix_extensions[126][6], */ /**< vpsignb opcode */
/* 810 */     OP_vpsignw,        /* &prefix_extensions[127][6], */ /**< vpsignw opcode */
/* 811 */     OP_vpsignd,        /* &prefix_extensions[128][6], */ /**< vpsignd opcode */
/* 812 */     OP_vpmulhrsw,      /* &prefix_extensions[129][6], */ /**< vpmulhrsw opcode */
/* 813 */     OP_vpabsb,         /* &prefix_extensions[130][6], */ /**< vpabsb opcode */
/* 814 */     OP_vpabsw,         /* &prefix_extensions[131][6], */ /**< vpabsw opcode */
/* 815 */     OP_vpabsd,         /* &prefix_extensions[132][6], */ /**< vpabsd opcode */
/* 816 */     OP_vpalignr,       /* &prefix_extensions[133][6], */ /**< vpalignr opcode */
/* 817 */     OP_vpblendvb,      /* &vex_extensions[ 2][1], */ /**< vpblendvb opcode */
/* 818 */     OP_vblendvps,      /* &vex_extensions[ 0][1], */ /**< vblendvps opcode */
/* 819 */     OP_vblendvpd,      /* &vex_extensions[ 1][1], */ /**< vblendvpd opcode */
/* 820 */     OP_vptest,         /* &vex_extensions[ 3][1], */ /**< vptest opcode */
/* 821 */     OP_vpmovsxbw,      /* &vex_extensions[ 4][1], */ /**< vpmovsxbw opcode */
/* 822 */     OP_vpmovsxbd,      /* &vex_extensions[ 5][1], */ /**< vpmovsxbd opcode */
/* 823 */     OP_vpmovsxbq,      /* &vex_extensions[ 6][1], */ /**< vpmovsxbq opcode */
/* 824 */     OP_vpmovsxwd,      /* &vex_extensions[ 7][1], */ /**< vpmovsxwd opcode */
/* 825 */     OP_vpmovsxwq,      /* &vex_extensions[ 8][1], */ /**< vpmovsxwq opcode */
/* 826 */     OP_vpmovsxdq,      /* &vex_extensions[ 9][1], */ /**< vpmovsxdq opcode */
/* 827 */     OP_vpmuldq,        /* &vex_extensions[10][1], */ /**< vpmuldq opcode */
/* 828 */     OP_vpcmpeqq,       /* &vex_extensions[11][1], */ /**< vpcmpeqq opcode */
/* 829 */     OP_vmovntdqa,      /* &vex_extensions[12][1], */ /**< vmovntdqa opcode */
/* 830 */     OP_vpackusdw,      /* &vex_extensions[13][1], */ /**< vpackusdw opcode */
/* 831 */     OP_vpmovzxbw,      /* &vex_extensions[14][1], */ /**< vpmovzxbw opcode */
/* 832 */     OP_vpmovzxbd,      /* &vex_extensions[15][1], */ /**< vpmovzxbd opcode */
/* 833 */     OP_vpmovzxbq,      /* &vex_extensions[16][1], */ /**< vpmovzxbq opcode */
/* 834 */     OP_vpmovzxwd,      /* &vex_extensions[17][1], */ /**< vpmovzxwd opcode */
/* 835 */     OP_vpmovzxwq,      /* &vex_extensions[18][1], */ /**< vpmovzxwq opcode */
/* 836 */     OP_vpmovzxdq,      /* &vex_extensions[19][1], */ /**< vpmovzxdq opcode */
/* 837 */     OP_vpcmpgtq,       /* &vex_extensions[20][1], */ /**< vpcmpgtq opcode */
/* 838 */     OP_vpminsb,        /* &vex_extensions[21][1], */ /**< vpminsb opcode */
/* 839 */     OP_vpminsd,        /* &vex_extensions[22][1], */ /**< vpminsd opcode */
/* 840 */     OP_vpminuw,        /* &vex_extensions[23][1], */ /**< vpminuw opcode */
/* 841 */     OP_vpminud,        /* &vex_extensions[24][1], */ /**< vpminud opcode */
/* 842 */     OP_vpmaxsb,        /* &vex_extensions[25][1], */ /**< vpmaxsb opcode */
/* 843 */     OP_vpmaxsd,        /* &vex_extensions[26][1], */ /**< vpmaxsd opcode */
/* 844 */     OP_vpmaxuw,        /* &vex_extensions[27][1], */ /**< vpmaxuw opcode */
/* 845 */     OP_vpmaxud,        /* &vex_extensions[28][1], */ /**< vpmaxud opcode */
/* 846 */     OP_vpmulld,        /* &vex_extensions[29][1], */ /**< vpmulld opcode */
/* 847 */     OP_vphminposuw,    /* &vex_extensions[30][1], */ /**< vphminposuw opcode */
/* 848 */     OP_vaesimc,        /* &vex_extensions[31][1], */ /**< vaesimc opcode */
/* 849 */     OP_vaesenc,        /* &vex_extensions[32][1], */ /**< vaesenc opcode */
/* 850 */     OP_vaesenclast,    /* &vex_extensions[33][1], */ /**< vaesenclast opcode */
/* 851 */     OP_vaesdec,        /* &vex_extensions[34][1], */ /**< vaesdec opcode */
/* 852 */     OP_vaesdeclast,    /* &vex_extensions[35][1], */ /**< vaesdeclast opcode */
/* 853 */     OP_vpextrb,        /* &vex_extensions[36][1], */ /**< vpextrb opcode */
/* 854 */     OP_vpextrd,        /* &vex_extensions[38][1], */ /**< vpextrd opcode */
/* 855 */     OP_vextractps,     /* &vex_extensions[39][1], */ /**< vextractps opcode */
/* 856 */     OP_vroundps,       /* &vex_extensions[40][1], */ /**< vroundps opcode */
/* 857 */     OP_vroundpd,       /* &vex_extensions[41][1], */ /**< vroundpd opcode */
/* 858 */     OP_vroundss,       /* &vex_extensions[42][1], */ /**< vroundss opcode */
/* 859 */     OP_vroundsd,       /* &vex_extensions[43][1], */ /**< vroundsd opcode */
/* 860 */     OP_vblendps,       /* &vex_extensions[44][1], */ /**< vblendps opcode */
/* 861 */     OP_vblendpd,       /* &vex_extensions[45][1], */ /**< vblendpd opcode */
/* 862 */     OP_vpblendw,       /* &vex_extensions[46][1], */ /**< vpblendw opcode */
/* 863 */     OP_vpinsrb,        /* &vex_extensions[47][1], */ /**< vpinsrb opcode */
/* 864 */     OP_vinsertps,      /* &vex_extensions[48][1], */ /**< vinsertps opcode */
/* 865 */     OP_vpinsrd,        /* &vex_extensions[49][1], */ /**< vpinsrd opcode */
/* 866 */     OP_vdpps,          /* &vex_extensions[50][1], */ /**< vdpps opcode */
/* 867 */     OP_vdppd,          /* &vex_extensions[51][1], */ /**< vdppd opcode */
/* 868 */     OP_vmpsadbw,       /* &vex_extensions[52][1], */ /**< vmpsadbw opcode */
/* 869 */     OP_vpcmpestrm,     /* &vex_extensions[53][1], */ /**< vpcmpestrm opcode */
/* 870 */     OP_vpcmpestri,     /* &vex_extensions[54][1], */ /**< vpcmpestri opcode */
/* 871 */     OP_vpcmpistrm,     /* &vex_extensions[55][1], */ /**< vpcmpistrm opcode */
/* 872 */     OP_vpcmpistri,     /* &vex_extensions[56][1], */ /**< vpcmpistri opcode */
/* 873 */     OP_vpclmulqdq,     /* &vex_extensions[57][1], */ /**< vpclmulqdq opcode */
/* 874 */     OP_vaeskeygenassist, /* &vex_extensions[58][1], */ /**< vaeskeygenassist opcode */
/* 875 */     OP_vtestps,        /* &vex_extensions[59][1], */ /**< vtestps opcode */
/* 876 */     OP_vtestpd,        /* &vex_extensions[60][1], */ /**< vtestpd opcode */
/* 877 */     OP_vzeroupper,     /* &vex_L_extensions[0][1], */ /**< vzeroupper opcode */
/* 878 */     OP_vzeroall,       /* &vex_L_extensions[0][2], */ /**< vzeroall opcode */
/* 879 */     OP_vldmxcsr,       /* &vex_extensions[61][1], */ /**< vldmxcsr opcode */
/* 880 */     OP_vstmxcsr,       /* &vex_extensions[62][1], */ /**< vstmxcsr opcode */
/* 881 */     OP_vbroadcastss,   /* &vex_extensions[64][1], */ /**< vbroadcastss opcode */
/* 882 */     OP_vbroadcastsd,   /* &vex_extensions[65][1], */ /**< vbroadcastsd opcode */
/* 883 */     OP_vbroadcastf128, /* &vex_extensions[66][1], */ /**< vbroadcastf128 opcode */
/* 884 */     OP_vmaskmovps,     /* &vex_extensions[67][1], */ /**< vmaskmovps opcode */
/* 885 */     OP_vmaskmovpd,     /* &vex_extensions[68][1], */ /**< vmaskmovpd opcode */
/* 886 */     OP_vpermilps,      /* &vex_extensions[71][1], */ /**< vpermilps opcode */
/* 887 */     OP_vpermilpd,      /* &vex_extensions[72][1], */ /**< vpermilpd opcode */
/* 888 */     OP_vperm2f128,     /* &vex_extensions[73][1], */ /**< vperm2f128 opcode */
/* 889 */     OP_vinsertf128,    /* &vex_extensions[74][1], */ /**< vinsertf128 opcode */
/* 890 */     OP_vextractf128,   /* &vex_extensions[75][1], */ /**< vextractf128 opcode */

    /* added in Ivy Bridge I believe, and covered by F16C cpuid flag */
/* 891 */     OP_vcvtph2ps,      /* &vex_extensions[63][1], */ /**< vcvtph2ps opcode */
/* 892 */     OP_vcvtps2ph,      /* &vex_extensions[76][1], */ /**< vcvtps2ph opcode */

    /* FMA */
/* 893 */     OP_vfmadd132ps,    /* &vex_W_extensions[ 0][0], */ /**< vfmadd132ps opcode */
/* 894 */     OP_vfmadd132pd,    /* &vex_W_extensions[ 0][1], */ /**< vfmadd132pd opcode */
/* 895 */     OP_vfmadd213ps,    /* &vex_W_extensions[ 1][0], */ /**< vfmadd213ps opcode */
/* 896 */     OP_vfmadd213pd,    /* &vex_W_extensions[ 1][1], */ /**< vfmadd213pd opcode */
/* 897 */     OP_vfmadd231ps,    /* &vex_W_extensions[ 2][0], */ /**< vfmadd231ps opcode */
/* 898 */     OP_vfmadd231pd,    /* &vex_W_extensions[ 2][1], */ /**< vfmadd231pd opcode */
/* 899 */     OP_vfmadd132ss,    /* &vex_W_extensions[ 3][0], */ /**< vfmadd132ss opcode */
/* 900 */     OP_vfmadd132sd,    /* &vex_W_extensions[ 3][1], */ /**< vfmadd132sd opcode */
/* 901 */     OP_vfmadd213ss,    /* &vex_W_extensions[ 4][0], */ /**< vfmadd213ss opcode */
/* 902 */     OP_vfmadd213sd,    /* &vex_W_extensions[ 4][1], */ /**< vfmadd213sd opcode */
/* 903 */     OP_vfmadd231ss,    /* &vex_W_extensions[ 5][0], */ /**< vfmadd231ss opcode */
/* 904 */     OP_vfmadd231sd,    /* &vex_W_extensions[ 5][1], */ /**< vfmadd231sd opcode */
/* 905 */     OP_vfmaddsub132ps, /* &vex_W_extensions[ 6][0], */ /**< vfmaddsub132ps opcode */
/* 906 */     OP_vfmaddsub132pd, /* &vex_W_extensions[ 6][1], */ /**< vfmaddsub132pd opcode */
/* 907 */     OP_vfmaddsub213ps, /* &vex_W_extensions[ 7][0], */ /**< vfmaddsub213ps opcode */
/* 908 */     OP_vfmaddsub213pd, /* &vex_W_extensions[ 7][1], */ /**< vfmaddsub213pd opcode */
/* 909 */     OP_vfmaddsub231ps, /* &vex_W_extensions[ 8][0], */ /**< vfmaddsub231ps opcode */
/* 910 */     OP_vfmaddsub231pd, /* &vex_W_extensions[ 8][1], */ /**< vfmaddsub231pd opcode */
/* 911 */     OP_vfmsubadd132ps, /* &vex_W_extensions[ 9][0], */ /**< vfmsubadd132ps opcode */
/* 912 */     OP_vfmsubadd132pd, /* &vex_W_extensions[ 9][1], */ /**< vfmsubadd132pd opcode */
/* 913 */     OP_vfmsubadd213ps, /* &vex_W_extensions[10][0], */ /**< vfmsubadd213ps opcode */
/* 914 */     OP_vfmsubadd213pd, /* &vex_W_extensions[10][1], */ /**< vfmsubadd213pd opcode */
/* 915 */     OP_vfmsubadd231ps, /* &vex_W_extensions[11][0], */ /**< vfmsubadd231ps opcode */
/* 916 */     OP_vfmsubadd231pd, /* &vex_W_extensions[11][1], */ /**< vfmsubadd231pd opcode */
/* 917 */     OP_vfmsub132ps,    /* &vex_W_extensions[12][0], */ /**< vfmsub132ps opcode */
/* 918 */     OP_vfmsub132pd,    /* &vex_W_extensions[12][1], */ /**< vfmsub132pd opcode */
/* 919 */     OP_vfmsub213ps,    /* &vex_W_extensions[13][0], */ /**< vfmsub213ps opcode */
/* 920 */     OP_vfmsub213pd,    /* &vex_W_extensions[13][1], */ /**< vfmsub213pd opcode */
/* 921 */     OP_vfmsub231ps,    /* &vex_W_extensions[14][0], */ /**< vfmsub231ps opcode */
/* 922 */     OP_vfmsub231pd,    /* &vex_W_extensions[14][1], */ /**< vfmsub231pd opcode */
/* 923 */     OP_vfmsub132ss,    /* &vex_W_extensions[15][0], */ /**< vfmsub132ss opcode */
/* 924 */     OP_vfmsub132sd,    /* &vex_W_extensions[15][1], */ /**< vfmsub132sd opcode */
/* 925 */     OP_vfmsub213ss,    /* &vex_W_extensions[16][0], */ /**< vfmsub213ss opcode */
/* 926 */     OP_vfmsub213sd,    /* &vex_W_extensions[16][1], */ /**< vfmsub213sd opcode */
/* 927 */     OP_vfmsub231ss,    /* &vex_W_extensions[17][0], */ /**< vfmsub231ss opcode */
/* 928 */     OP_vfmsub231sd,    /* &vex_W_extensions[17][1], */ /**< vfmsub231sd opcode */
/* 929 */     OP_vfnmadd132ps,   /* &vex_W_extensions[18][0], */ /**< vfnmadd132ps opcode */
/* 930 */     OP_vfnmadd132pd,   /* &vex_W_extensions[18][1], */ /**< vfnmadd132pd opcode */
/* 931 */     OP_vfnmadd213ps,   /* &vex_W_extensions[19][0], */ /**< vfnmadd213ps opcode */
/* 932 */     OP_vfnmadd213pd,   /* &vex_W_extensions[19][1], */ /**< vfnmadd213pd opcode */
/* 933 */     OP_vfnmadd231ps,   /* &vex_W_extensions[20][0], */ /**< vfnmadd231ps opcode */
/* 934 */     OP_vfnmadd231pd,   /* &vex_W_extensions[20][1], */ /**< vfnmadd231pd opcode */
/* 935 */     OP_vfnmadd132ss,   /* &vex_W_extensions[21][0], */ /**< vfnmadd132ss opcode */
/* 936 */     OP_vfnmadd132sd,   /* &vex_W_extensions[21][1], */ /**< vfnmadd132sd opcode */
/* 937 */     OP_vfnmadd213ss,   /* &vex_W_extensions[22][0], */ /**< vfnmadd213ss opcode */
/* 938 */     OP_vfnmadd213sd,   /* &vex_W_extensions[22][1], */ /**< vfnmadd213sd opcode */
/* 939 */     OP_vfnmadd231ss,   /* &vex_W_extensions[23][0], */ /**< vfnmadd231ss opcode */
/* 940 */     OP_vfnmadd231sd,   /* &vex_W_extensions[23][1], */ /**< vfnmadd231sd opcode */
/* 941 */     OP_vfnmsub132ps,   /* &vex_W_extensions[24][0], */ /**< vfnmsub132ps opcode */
/* 942 */     OP_vfnmsub132pd,   /* &vex_W_extensions[24][1], */ /**< vfnmsub132pd opcode */
/* 943 */     OP_vfnmsub213ps,   /* &vex_W_extensions[25][0], */ /**< vfnmsub213ps opcode */
/* 944 */     OP_vfnmsub213pd,   /* &vex_W_extensions[25][1], */ /**< vfnmsub213pd opcode */
/* 945 */     OP_vfnmsub231ps,   /* &vex_W_extensions[26][0], */ /**< vfnmsub231ps opcode */
/* 946 */     OP_vfnmsub231pd,   /* &vex_W_extensions[26][1], */ /**< vfnmsub231pd opcode */
/* 947 */     OP_vfnmsub132ss,   /* &vex_W_extensions[27][0], */ /**< vfnmsub132ss opcode */
/* 948 */     OP_vfnmsub132sd,   /* &vex_W_extensions[27][1], */ /**< vfnmsub132sd opcode */
/* 949 */     OP_vfnmsub213ss,   /* &vex_W_extensions[28][0], */ /**< vfnmsub213ss opcode */
/* 950 */     OP_vfnmsub213sd,   /* &vex_W_extensions[28][1], */ /**< vfnmsub213sd opcode */
/* 951 */     OP_vfnmsub231ss,   /* &vex_W_extensions[29][0], */ /**< vfnmsub231ss opcode */
/* 952 */     OP_vfnmsub231sd,   /* &vex_W_extensions[29][1], */ /**< vfnmsub231sd opcode */

/* 953 */     OP_movq2dq,        /* &prefix_extensions[61][1], */ /**< movq2dq opcode */
/* 954 */     OP_movdq2q,        /* &prefix_extensions[61][3], */ /**< movdq2q opcode */

/* 955 */     OP_fxsave64,       /* &rex_w_extensions[0][1], */ /**< fxsave64 opcode */
/* 956 */     OP_fxrstor64,      /* &rex_w_extensions[1][1], */ /**< fxrstor64 opcode */
/* 957 */     OP_xsave64,        /* &rex_w_extensions[2][1], */ /**< xsave64 opcode */
/* 958 */     OP_xrstor64,       /* &rex_w_extensions[3][1], */ /**< xrstor64 opcode */
/* 959 */     OP_xsaveopt64,     /* &rex_w_extensions[4][1], */ /**< xsaveopt64 opcode */

    /* added in Intel Ivy Bridge: RDRAND and FSGSBASE cpuid flags */
/* 960 */     OP_rdrand,         /* &mod_extensions[12][1], */ /**< rdrand opcode */
/* 961 */     OP_rdfsbase,       /* &mod_extensions[14][1], */ /**< rdfsbase opcode */
/* 962 */     OP_rdgsbase,       /* &mod_extensions[15][1], */ /**< rdgsbase opcode */
/* 963 */     OP_wrfsbase,       /* &mod_extensions[16][1], */ /**< wrfsbase opcode */
/* 964 */     OP_wrgsbase,       /* &mod_extensions[17][1], */ /**< wrgsbase opcode */

    /* coming in the future but adding now since enough details are known */
/* 965 */     OP_rdseed,         /* &mod_extensions[13][1], */ /**< rdseed opcode */

    /* AMD FMA4 */
/* 966 */     OP_vfmaddsubps,    /* &vex_W_extensions[30][0], */ /**< vfmaddsubps opcode */
/* 967 */     OP_vfmaddsubpd,    /* &vex_W_extensions[31][0], */ /**< vfmaddsubpd opcode */
/* 968 */     OP_vfmsubaddps,    /* &vex_W_extensions[32][0], */ /**< vfmsubaddps opcode */
/* 969 */     OP_vfmsubaddpd,    /* &vex_W_extensions[33][0], */ /**< vfmsubaddpd opcode */
/* 970 */     OP_vfmaddps,       /* &vex_W_extensions[34][0], */ /**< vfmaddps opcode */
/* 971 */     OP_vfmaddpd,       /* &vex_W_extensions[35][0], */ /**< vfmaddpd opcode */
/* 972 */     OP_vfmaddss,       /* &vex_W_extensions[36][0], */ /**< vfmaddss opcode */
/* 973 */     OP_vfmaddsd,       /* &vex_W_extensions[37][0], */ /**< vfmaddsd opcode */
/* 974 */     OP_vfmsubps,       /* &vex_W_extensions[38][0], */ /**< vfmsubps opcode */
/* 975 */     OP_vfmsubpd,       /* &vex_W_extensions[39][0], */ /**< vfmsubpd opcode */
/* 976 */     OP_vfmsubss,       /* &vex_W_extensions[40][0], */ /**< vfmsubss opcode */
/* 977 */     OP_vfmsubsd,       /* &vex_W_extensions[41][0], */ /**< vfmsubsd opcode */
/* 978 */     OP_vfnmaddps,      /* &vex_W_extensions[42][0], */ /**< vfnmaddps opcode */
/* 979 */     OP_vfnmaddpd,      /* &vex_W_extensions[43][0], */ /**< vfnmaddpd opcode */
/* 980 */     OP_vfnmaddss,      /* &vex_W_extensions[44][0], */ /**< vfnmaddss opcode */
/* 981 */     OP_vfnmaddsd,      /* &vex_W_extensions[45][0], */ /**< vfnmaddsd opcode */
/* 982 */     OP_vfnmsubps,      /* &vex_W_extensions[46][0], */ /**< vfnmsubps opcode */
/* 983 */     OP_vfnmsubpd,      /* &vex_W_extensions[47][0], */ /**< vfnmsubpd opcode */
/* 984 */     OP_vfnmsubss,      /* &vex_W_extensions[48][0], */ /**< vfnmsubss opcode */
/* 985 */     OP_vfnmsubsd,      /* &vex_W_extensions[49][0], */ /**< vfnmsubsd opcode */

    /* AMD XOP */
/* 986 */     OP_vfrczps,        /* &xop_extensions[27], */ /**< vfrczps opcode */
/* 987 */     OP_vfrczpd,        /* &xop_extensions[28], */ /**< vfrczpd opcode */
/* 988 */     OP_vfrczss,        /* &xop_extensions[29], */ /**< vfrczss opcode */
/* 989 */     OP_vfrczsd,        /* &xop_extensions[30], */ /**< vfrczsd opcode */
/* 990 */     OP_vpcmov,         /* &vex_W_extensions[50][0], */ /**< vpcmov opcode */
/* 991 */     OP_vpcomb,         /* &xop_extensions[19], */ /**< vpcomb opcode */
/* 992 */     OP_vpcomw,         /* &xop_extensions[20], */ /**< vpcomw opcode */
/* 993 */     OP_vpcomd,         /* &xop_extensions[21], */ /**< vpcomd opcode */
/* 994 */     OP_vpcomq,         /* &xop_extensions[22], */ /**< vpcomq opcode */
/* 995 */     OP_vpcomub,        /* &xop_extensions[23], */ /**< vpcomub opcode */
/* 996 */     OP_vpcomuw,        /* &xop_extensions[24], */ /**< vpcomuw opcode */
/* 997 */     OP_vpcomud,        /* &xop_extensions[25], */ /**< vpcomud opcode */
/* 998 */     OP_vpcomuq,        /* &xop_extensions[26], */ /**< vpcomuq opcode */
/* 999 */     OP_vpermil2pd,     /* &vex_W_extensions[65][0], */ /**< vpermil2pd opcode */
/* 1000 */     OP_vpermil2ps,     /* &vex_W_extensions[64][0], */ /**< vpermil2ps opcode */
/* 1001 */     OP_vphaddbw,       /* &xop_extensions[43], */ /**< vphaddbw opcode */
/* 1002 */     OP_vphaddbd,       /* &xop_extensions[44], */ /**< vphaddbd opcode */
/* 1003 */     OP_vphaddbq,       /* &xop_extensions[45], */ /**< vphaddbq opcode */
/* 1004 */     OP_vphaddwd,       /* &xop_extensions[46], */ /**< vphaddwd opcode */
/* 1005 */     OP_vphaddwq,       /* &xop_extensions[47], */ /**< vphaddwq opcode */
/* 1006 */     OP_vphadddq,       /* &xop_extensions[48], */ /**< vphadddq opcode */
/* 1007 */     OP_vphaddubw,      /* &xop_extensions[49], */ /**< vphaddubw opcode */
/* 1008 */     OP_vphaddubd,      /* &xop_extensions[50], */ /**< vphaddubd opcode */
/* 1009 */     OP_vphaddubq,      /* &xop_extensions[51], */ /**< vphaddubq opcode */
/* 1010 */     OP_vphadduwd,      /* &xop_extensions[52], */ /**< vphadduwd opcode */
/* 1011 */     OP_vphadduwq,      /* &xop_extensions[53], */ /**< vphadduwq opcode */
/* 1012 */     OP_vphaddudq,      /* &xop_extensions[54], */ /**< vphaddudq opcode */
/* 1013 */     OP_vphsubbw,       /* &xop_extensions[55], */ /**< vphsubbw opcode */
/* 1014 */     OP_vphsubwd,       /* &xop_extensions[56], */ /**< vphsubwd opcode */
/* 1015 */     OP_vphsubdq,       /* &xop_extensions[57], */ /**< vphsubdq opcode */
/* 1016 */     OP_vpmacssww,      /* &xop_extensions[ 1], */ /**< vpmacssww opcode */
/* 1017 */     OP_vpmacsswd,      /* &xop_extensions[ 2], */ /**< vpmacsswd opcode */
/* 1018 */     OP_vpmacssdql,     /* &xop_extensions[ 3], */ /**< vpmacssdql opcode */
/* 1019 */     OP_vpmacssdd,      /* &xop_extensions[ 4], */ /**< vpmacssdd opcode */
/* 1020 */     OP_vpmacssdqh,     /* &xop_extensions[ 5], */ /**< vpmacssdqh opcode */
/* 1021 */     OP_vpmacsww,       /* &xop_extensions[ 6], */ /**< vpmacsww opcode */
/* 1022 */     OP_vpmacswd,       /* &xop_extensions[ 7], */ /**< vpmacswd opcode */
/* 1023 */     OP_vpmacsdql,      /* &xop_extensions[ 8], */ /**< vpmacsdql opcode */
/* 1024 */     OP_vpmacsdd,       /* &xop_extensions[ 9], */ /**< vpmacsdd opcode */
/* 1025 */     OP_vpmacsdqh,      /* &xop_extensions[10], */ /**< vpmacsdqh opcode */
/* 1026 */     OP_vpmadcsswd,     /* &xop_extensions[13], */ /**< vpmadcsswd opcode */
/* 1027 */     OP_vpmadcswd,      /* &xop_extensions[14], */ /**< vpmadcswd opcode */
/* 1028 */     OP_vpperm,         /* &vex_W_extensions[51][0], */ /**< vpperm opcode */
/* 1029 */     OP_vprotb,         /* &xop_extensions[15], */ /**< vprotb opcode */
/* 1030 */     OP_vprotw,         /* &xop_extensions[16], */ /**< vprotw opcode */
/* 1031 */     OP_vprotd,         /* &xop_extensions[17], */ /**< vprotd opcode */
/* 1032 */     OP_vprotq,         /* &xop_extensions[18], */ /**< vprotq opcode */
/* 1033 */     OP_vpshlb,         /* &vex_W_extensions[56][0], */ /**< vpshlb opcode */
/* 1034 */     OP_vpshlw,         /* &vex_W_extensions[57][0], */ /**< vpshlw opcode */
/* 1035 */     OP_vpshld,         /* &vex_W_extensions[58][0], */ /**< vpshld opcode */
/* 1036 */     OP_vpshlq,         /* &vex_W_extensions[59][0], */ /**< vpshlq opcode */
/* 1037 */     OP_vpshab,         /* &vex_W_extensions[60][0], */ /**< vpshab opcode */
/* 1038 */     OP_vpshaw,         /* &vex_W_extensions[61][0], */ /**< vpshaw opcode */
/* 1039 */     OP_vpshad,         /* &vex_W_extensions[62][0], */ /**< vpshad opcode */
/* 1040 */     OP_vpshaq,         /* &vex_W_extensions[63][0], */ /**< vpshaq opcode */

    /* AMD TBM */
/* 1041 */     OP_bextr,          /* &prefix_extensions[141][4], */ /**< bextr opcode */
/* 1042 */     OP_blcfill,        /* &extensions[27][1], */ /**< blcfill opcode */
/* 1043 */     OP_blci,           /* &extensions[28][6], */ /**< blci opcode */
/* 1044 */     OP_blcic,          /* &extensions[27][5], */ /**< blcic opcode */
/* 1045 */     OP_blcmsk,         /* &extensions[28][1], */ /**< blcmsk opcode */
/* 1046 */     OP_blcs,           /* &extensions[27][3], */ /**< blcs opcode */
/* 1047 */     OP_blsfill,        /* &extensions[27][2], */ /**< blsfill opcode */
/* 1048 */     OP_blsic,          /* &extensions[27][6], */ /**< blsic opcode */
/* 1049 */     OP_t1mskc,         /* &extensions[27][7], */ /**< t1mskc opcode */
/* 1050 */     OP_tzmsk,          /* &extensions[27][4], */ /**< tzmsk opcode */

    /* AMD LWP */
/* 1051 */     OP_llwpcb,         /* &extensions[29][0], */ /**< llwpcb opcode */
/* 1052 */     OP_slwpcb,         /* &extensions[29][1], */ /**< slwpcb opcode */
/* 1053 */     OP_lwpins,         /* &extensions[30][0], */ /**< lwpins opcode */
/* 1054 */     OP_lwpval,         /* &extensions[30][1], */ /**< lwpval opcode */

    /* Intel BMI1 */
    /* (includes non-immed form of OP_bextr) */
/* 1055 */     OP_andn,           /* &third_byte_38[100], */ /**< andn opcode */
/* 1056 */     OP_blsr,           /* &extensions[31][1], */ /**< blsr opcode */
/* 1057 */     OP_blsmsk,         /* &extensions[31][2], */ /**< blsmsk opcode */
/* 1058 */     OP_blsi,           /* &extensions[31][3], */ /**< blsi opcode */
/* 1059 */     OP_tzcnt,          /* &prefix_extensions[140][1], */ /**< tzcnt opcode */

    /* Intel BMI2 */
/* 1060 */     OP_bzhi,           /* &prefix_extensions[142][4], */ /**< bzhi opcode */
/* 1061 */     OP_pext,           /* &prefix_extensions[142][6], */ /**< pext opcode */
/* 1062 */     OP_pdep,           /* &prefix_extensions[142][7], */ /**< pdep opcode */
/* 1063 */     OP_sarx,           /* &prefix_extensions[141][5], */ /**< sarx opcode */
/* 1064 */     OP_shlx,           /* &prefix_extensions[141][6], */ /**< shlx opcode */
/* 1065 */     OP_shrx,           /* &prefix_extensions[141][7], */ /**< shrx opcode */
/* 1066 */     OP_rorx,           /* &third_byte_3a[56], */ /**< rorx opcode */
/* 1067 */     OP_mulx,           /* &prefix_extensions[143][7], */ /**< mulx opcode */

    /* Intel Safer Mode Extensions */
/* 1068 */     OP_getsec,         /* &second_byte[0x37], */ /**< getsec opcode */

    /* Misc Intel additions */
/* 1069 */     OP_vmfunc,         /* &rm_extensions[4][4], */ /**< vmfunc opcode */
/* 1070 */     OP_invpcid,        /* &third_byte_38[103], */ /**< invpcid opcode */

    /* Intel TSX */
/* 1071 */     OP_xabort,         /* &extensions[17][7], */ /**< xabort opcode */
/* 1072 */     OP_xbegin,         /* &extensions[18][7], */ /**< xbegin opcode */
/* 1073 */     OP_xend,           /* &rm_extensions[4][5], */ /**< xend opcode */
/* 1074 */     OP_xtest,          /* &rm_extensions[4][6], */ /**< xtest opcode */

    /* AVX2 */
/* 1075 */     OP_vpgatherdd,     /* &vex_W_extensions[66][0], */ /**< vpgatherdd opcode */
/* 1076 */     OP_vpgatherdq,     /* &vex_W_extensions[66][1], */ /**< vpgatherdq opcode */
/* 1077 */     OP_vpgatherqd,     /* &vex_W_extensions[67][0], */ /**< vpgatherqd opcode */
/* 1078 */     OP_vpgatherqq,     /* &vex_W_extensions[67][1], */ /**< vpgatherqq opcode */
/* 1079 */     OP_vgatherdps,     /* &vex_W_extensions[68][0], */ /**< vgatherdps opcode */
/* 1080 */     OP_vgatherdpd,     /* &vex_W_extensions[68][1], */ /**< vgatherdpd opcode */
/* 1081 */     OP_vgatherqps,     /* &vex_W_extensions[69][0], */ /**< vgatherqps opcode */
/* 1082 */     OP_vgatherqpd,     /* &vex_W_extensions[69][1], */ /**< vgatherqpd opcode */
/* 1083 */     OP_vbroadcasti128,  /* &third_byte_38[108], */ /**< vbroadcasti128 opcode */
/* 1084 */     OP_vinserti128,    /* &third_byte_3a[57], */ /**< vinserti128 opcode */
/* 1085 */     OP_vextracti128,   /* &third_byte_3a[58], */ /**< vextracti128 opcode */
/* 1086 */     OP_vpmaskmovd,     /* &vex_W_extensions[70][0], */ /**< vpmaskmovd opcode */
/* 1087 */     OP_vpmaskmovq,     /* &vex_W_extensions[70][1], */ /**< vpmaskmovq opcode */
/* 1088 */     OP_vperm2i128,     /* &vex_W_extensions[69][1], */ /**< vperm2i128 opcode */
/* 1089 */     OP_vpermd,         /* &third_byte_38[112], */ /**< vpermd opcode */
/* 1090 */     OP_vpermps,        /* &third_byte_38[111], */ /**< vpermps opcode */
/* 1091 */     OP_vpermq,         /* &third_byte_3a[59], */ /**< vpermq opcode */
/* 1092 */     OP_vpermpd,        /* &third_byte_3a[60], */ /**< vpermpd opcode */
/* 1093 */     OP_vpblendd,       /* &third_byte_3a[61], */ /**< vpblendd opcode */
/* 1094 */     OP_vpsllvd,        /* &vex_W_extensions[73][0], */ /**< vpsllvd opcode */
/* 1095 */     OP_vpsllvq,        /* &vex_W_extensions[73][1], */ /**< vpsllvq opcode */
/* 1096 */     OP_vpsravd,        /* &third_byte_38[114], */ /**< vpsravd opcode */
/* 1097 */     OP_vpsrlvd,        /* &vex_W_extensions[72][0], */ /**< vpsrlvd opcode */
/* 1098 */     OP_vpsrlvq,        /* &vex_W_extensions[72][1], */ /**< vpsrlvq opcode */

    /* Keep these at the end so that ifdefs don't change internal enum values */
#ifdef IA32_ON_IA64
/* 1099 */     OP_jmpe,       /* &extensions[13][6], */ /**< jmpe opcode */
/* 1100 */     OP_jmpe_abs,   /* &second_byte[0xb8], */ /**< jmpe_abs opcode */
#endif

    OP_AFTER_LAST,
    OP_FIRST = OP_add,            /**< First real opcode. */
    OP_LAST  = OP_AFTER_LAST - 1, /**< Last real opcode. */
};

#ifdef IA32_ON_IA64
/* redefine instead of if else so works with genapi.pl script */
#define OP_LAST OP_jmpe_abs
#endif

/* alternative names */
/* we do not equate the fwait+op opcodes
 *   fstsw, fstcw, fstenv, finit, fclex
 * for us that has to be a sequence of instructions: a separate fwait
 */
/* XXX i#1307: we could add extra decode table layers to print the proper name
 * when we disassemble these, but it's not clear it's worth the effort.
 */
/* 16-bit versions that have different names */
#define OP_cbw        OP_cwde /**< Alternative opcode name for 16-bit version. */
#define OP_cwd        OP_cdq /**< Alternative opcode name for 16-bit version. */
#define OP_jcxz       OP_jecxz /**< Alternative opcode name for 16-bit version. */
/* 64-bit versions that have different names */
#define OP_cdqe       OP_cwde /**< Alternative opcode name for 64-bit version. */
#define OP_cqo        OP_cdq /**< Alternative opcode name for 64-bit version. */
#define OP_jrcxz      OP_jecxz     /**< Alternative opcode name for 64-bit version. */
#define OP_cmpxchg16b OP_cmpxchg8b /**< Alternative opcode name for 64-bit version. */
#define OP_pextrq     OP_pextrd    /**< Alternative opcode name for 64-bit version. */
#define OP_pinsrq     OP_pinsrd    /**< Alternative opcode name for 64-bit version. */
#define OP_vpextrq     OP_vpextrd    /**< Alternative opcode name for 64-bit version. */
#define OP_vpinsrq     OP_vpinsrd    /**< Alternative opcode name for 64-bit version. */
/* reg-reg version has different name */
#define OP_movhlps    OP_movlps /**< Alternative opcode name for reg-reg version. */
#define OP_movlhps    OP_movhps /**< Alternative opcode name for reg-reg version. */
#define OP_vmovhlps    OP_vmovlps /**< Alternative opcode name for reg-reg version. */
#define OP_vmovlhps    OP_vmovhps /**< Alternative opcode name for reg-reg version. */
/* condition codes */
#define OP_jae_short  OP_jnb_short  /**< Alternative opcode name. */
#define OP_jnae_short OP_jb_short   /**< Alternative opcode name. */
#define OP_ja_short   OP_jnbe_short /**< Alternative opcode name. */
#define OP_jna_short  OP_jbe_short  /**< Alternative opcode name. */
#define OP_je_short   OP_jz_short   /**< Alternative opcode name. */
#define OP_jne_short  OP_jnz_short  /**< Alternative opcode name. */
#define OP_jge_short  OP_jnl_short  /**< Alternative opcode name. */
#define OP_jg_short   OP_jnle_short /**< Alternative opcode name. */
#define OP_jae  OP_jnb        /**< Alternative opcode name. */
#define OP_jnae OP_jb         /**< Alternative opcode name. */
#define OP_ja   OP_jnbe       /**< Alternative opcode name. */
#define OP_jna  OP_jbe        /**< Alternative opcode name. */
#define OP_je   OP_jz         /**< Alternative opcode name. */
#define OP_jne  OP_jnz        /**< Alternative opcode name. */
#define OP_jge  OP_jnl        /**< Alternative opcode name. */
#define OP_jg   OP_jnle       /**< Alternative opcode name. */
#define OP_setae  OP_setnb    /**< Alternative opcode name. */
#define OP_setnae OP_setb     /**< Alternative opcode name. */
#define OP_seta   OP_setnbe   /**< Alternative opcode name. */
#define OP_setna  OP_setbe    /**< Alternative opcode name. */
#define OP_sete   OP_setz     /**< Alternative opcode name. */
#define OP_setne  OP_setnz    /**< Alternative opcode name. */
#define OP_setge  OP_setnl    /**< Alternative opcode name. */
#define OP_setg   OP_setnle   /**< Alternative opcode name. */
#define OP_cmovae  OP_cmovnb  /**< Alternative opcode name. */
#define OP_cmovnae OP_cmovb   /**< Alternative opcode name. */
#define OP_cmova   OP_cmovnbe /**< Alternative opcode name. */
#define OP_cmovna  OP_cmovbe  /**< Alternative opcode name. */
#define OP_cmove   OP_cmovz   /**< Alternative opcode name. */
#define OP_cmovne  OP_cmovnz  /**< Alternative opcode name. */
#define OP_cmovge  OP_cmovnl  /**< Alternative opcode name. */
#define OP_cmovg   OP_cmovnle /**< Alternative opcode name. */
#ifndef X64
# define OP_fxsave   OP_fxsave32   /**< Alternative opcode name. */
# define OP_fxrstor  OP_fxrstor32  /**< Alternative opcode name. */
# define OP_xsave    OP_xsave32    /**< Alternative opcode name. */
# define OP_xrstor   OP_xrstor32   /**< Alternative opcode name. */
# define OP_xsaveopt OP_xsaveopt32 /**< Alternative opcode name. */
#endif
#define OP_wait   OP_fwait /**< Alternative opcode name. */
#define OP_sal    OP_shl /**< Alternative opcode name. */
/* undocumented opcodes */
#define OP_icebp OP_int1
#define OP_setalc OP_salc

/****************************************************************************/
/* DR_API EXPORT END */

#include "instr_inline.h"

#endif /* _INSTR_H_ */
