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

#include "opnd_api.h"

/*************************
 ***      opnd_t       ***
 *************************/

/* If INSTR_INLINE is already defined, that means we've been included by
 * instr_shared.c, which wants to use C99 extern inline.  Otherwise, DR_FAST_IR
 * determines whether our instr routines are inlined.
 */
/* Inlining macro controls. */
#ifndef INSTR_INLINE
#    ifdef DR_FAST_IR
#        define INSTR_INLINE inline
#    else
#        define INSTR_INLINE
#    endif
#endif

/* indexed by enum */
extern const char *const reg_names[];
extern const reg_id_t dr_reg_fixer[];

#ifdef X86
#    define REG_START_SPILL DR_REG_XAX
#    define REG_STOP_SPILL DR_REG_XDI
#elif defined(AARCHXX)
/* We only normally use r0-r5 but we support more in translation code */
#    define REG_START_SPILL DR_REG_R0
#    define REG_STOP_SPILL DR_REG_R10 /* r10 might be used in syscall mangling */
#elif defined(RISCV64)
#    define REG_START_SPILL DR_REG_A0
#    define REG_STOP_SPILL DR_REG_A5
#endif /* RISCV64 */
#define REG_SPILL_NUM (REG_STOP_SPILL - REG_START_SPILL + 1)

#ifndef INT8_MIN
#    define INT8_MIN SCHAR_MIN
#    define INT8_MAX SCHAR_MAX
#    define INT16_MIN SHRT_MIN
#    define INT16_MAX SHRT_MAX
#    define INT32_MIN INT_MIN
#    define INT32_MAX INT_MAX
#endif

/* typedef is in globals.h */
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

#ifdef X86
/* Debug registers are used for breakpoint with x86.
 * DynamoRIO needs to keep track of their values process-wide.
 */
#    define DEBUG_REGISTERS_NB 4
/* Dr7 flags mask to enable debug registers */
#    define DEBUG_REGISTERS_FLAG_ENABLE_DR0 0x3
#    define DEBUG_REGISTERS_FLAG_ENABLE_DR1 0xc
#    define DEBUG_REGISTERS_FLAG_ENABLE_DR2 0x30
#    define DEBUG_REGISTERS_FLAG_ENABLE_DR3 0xc0
extern app_pc d_r_debug_register[DEBUG_REGISTERS_NB];

/* Tells if instruction will trigger an exception because of debug register. */
static inline bool
debug_register_fire_on_addr(app_pc pc)
{
    ASSERT(DEBUG_REGISTERS_NB == 4);
    return (pc != NULL &&
            (pc == d_r_debug_register[0] || pc == d_r_debug_register[1] ||
             pc == d_r_debug_register[2] || pc == d_r_debug_register[3]));
}
#endif

/* functions to build an operand */

/* not exported */
opnd_t
opnd_create_immed_float_for_opcode(uint opcode);

/* predicate functions */

/* Check if the operand kind and size fields are valid */
bool
opnd_is_valid(opnd_t opnd);

/* not exported */
int
opnd_get_reg_dcontext_offs(reg_id_t reg);

int
opnd_get_reg_mcontext_offs(reg_id_t reg);

/* internal version */
reg_t
reg_get_value_priv(reg_id_t reg, priv_mcontext_t *mc);

/* internal version */
void
reg_set_value_priv(reg_id_t reg, priv_mcontext_t *mc, reg_t value);

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

/* stack slot width */
#define XSP_SZ ((ssize_t)sizeof(reg_t))

/* This should be kept in sync w/ the defines in x86/x86.asm */
enum {
#ifdef X86
    DR_SYSNUM_REG = DR_REG_EAX,
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
    DR_SYSNUM_REG = DR_REG_R8,
    REGPARM_4 = DR_REG_R4,
    REGPARM_5 = DR_REG_R5,
    REGPARM_6 = DR_REG_R6,
    REGPARM_7 = DR_REG_R7,
    NUM_REGPARM = 8,
#    else
    DR_SYSNUM_REG = DR_REG_R7,
    NUM_REGPARM = 4,
#    endif /* 64/32 */
    REDZONE_SIZE = 0,
    REGPARM_MINSTACK = 0,
    REGPARM_END_ALIGN = 8,
#elif defined(RISCV64)
    DR_SYSNUM_REG = DR_REG_A7,
    REGPARM_0 = DR_REG_A0,
    REGPARM_1 = DR_REG_A1,
    REGPARM_2 = DR_REG_A2,
    REGPARM_3 = DR_REG_A3,
    REGPARM_4 = DR_REG_A4,
    REGPARM_5 = DR_REG_A5,
    NUM_REGPARM = 6,
    REDZONE_SIZE = 0,
    REGPARM_MINSTACK = 0,
    REGPARM_END_ALIGN = 8,
#endif
};

#ifdef X86
#    define MCXT_FLD_FIRST_REG xdi
#    define MCXT_FLD_SYSNUM_REG xax
#elif defined(AARCHXX)
#    define MCXT_FLD_FIRST_REG r0
#    ifdef X64
#        ifdef MACOS
#            define MCXT_FLD_SYSNUM_REG r16
#        else
#            define MCXT_FLD_SYSNUM_REG r8
#        endif
#    else
#        define MCXT_FLD_SYSNUM_REG r7
#    endif /* 64/32 */
#elif defined(RISCV64)
#    define MCXT_FLD_FIRST_REG x0
#    define MCXT_FLD_SYSNUM_REG a7
#endif
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
