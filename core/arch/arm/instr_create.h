/* **********************************************************
 * Copyright (c) 2011-2014 Google, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2002-2003 Massachusetts Institute of Technology */

#ifndef _INSTR_CREATE_H_
#define _INSTR_CREATE_H_ 1

#include "../instr_create_shared.h"

/* DR_API EXPORT TOFILE dr_ir_macros_arm.h */
/* DR_API EXPORT BEGIN */

/**
 * Create an absolute address operand encoded as pc-relative.
 * Encoding will fail if addr is out of the maximum signed displacement
 * reach for the architecture and ISA mode.
 */
#define OPND_CREATE_ABSMEM(addr, size) \
  opnd_create_rel_addr(addr, size)

/* Macros for building instructions, one for each opcode.
 * Each INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The macro parameter types, encoded by name, are:
 *   dc = DR Context*
 *   op = uint = opcode
 *   s  = opnd_t = source operand
 *   i  = opnd_t = source operand that is an immediate
 *   ri = opnd_t = source operand that can be a register or an immediate
 *   t  = opnd_t = source operand that is a jump target
 *   m  = opnd_t = source operand that can only reference memory
 *   f  = opnd_t = floating point register operand
 *   d  = opnd_t = destination operand
 */

/* platform-independent INSTR_CREATE_* macros */
/** @name Platform-independent macros */
/* @{ */ /* doxygen start group */

/**
 * This platform-independent macro creates an instr_t for a debug trap
 * instruction, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define XINST_CREATE_debug_instr(dc) INSTR_CREATE_bkpt(dc, OPND_CREATE_INT8(1))

/**
 * This platform-independent creates an instr_t for a memory load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param m   The source memory opnd.
 */
#define XINST_CREATE_load(dc, r, m)  INSTR_CREATE_ldr(dc, r, m)

/**
 * This platform-independent macro creates an instr_t for a memory store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r   The source register opnd.
 */
#define XINST_CREATE_store(dc, m, r)  INSTR_CREATE_str(dc, m, r)

/**
 * This platform-independent macro creates an instr_t for a register
 * to register move instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d   The destination register opnd.
 * \param s   The source register opnd.
 */
#define XINST_CREATE_move(dc, d, s) \
    instr_create_1dst_1src((dc), OP_mov, (d), (s))

/**
 * This platform-independent macro creates an instr_t for a multimedia
 * register load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param m   The source memory opnd.
 *
 * \note Loading to 128-bit registers is not supported on 32-bit ARM.
 */
#define XINST_CREATE_load_simd(dc, r, m) INSTR_CREATE_vldr(dc, r, m)

/**
 * This platform-independent macro creates an instr_t for a multimedia
 * register store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r   The source register opnd.
 *
 * \note Storing from 128-bit registers is not supported on 32-bit ARM.
 */
#define XINST_CREATE_store_simd(dc, m, r) INSTR_CREATE_vstr(dc, m, r)

/**
 * This platform-independent macro creates an instr_t for an indirect
 * jump through memory instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The memory opnd holding the target.
 */
#define XINST_CREATE_jmp_ind_mem(dc, m) \
    INSTR_CREATE_ldr(dc, opnd_create_reg(DR_REG_PC), m)

/**
 * This platform-independent macro creates an instr_t for an immediate
 * integer load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param i   The source immediate integer opnd.
 */
#define XINST_CREATE_load_int(dc, r, i) XINST_CREATE_move(dc, r, i)

/**
 * This platform-independent macro creates an instr_t for a return instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 */
#define XINST_CREATE_return(dc) INSTR_CREATE_pop(dc, opnd_create_reg(DR_REG_PC))

/**
 * This platform-independent macro creates an instr_t for an unconditional
 * branch instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param t   The opnd_t target operand for the instruction, which can be
 * either a pc (opnd_create_pc)()) or an instr_t (opnd_create_instr()).
 * Be sure to ensure that the limited reach of this short branch will reach
 * the target (a pc operand is not suitable for most uses unless you know
 * precisely where this instruction will be encoded).
 */
#define XINST_CREATE_jmp(dc, t) instr_create_0dst_1src((dc), OP_b, (t))

/* @} */ /* end doxygen group */

/****************************************************************************/
/* ARM-specific INSTR_CREATE_* macros */

/** @name 1 destination, 1 source */
/* @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/* FIXME i#1551: provide better documentation for INSTR_CREATE_* macros. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 */
#define INSTR_CREATE_ldr(dc, d, s) \
  instr_create_1dst_1src((dc), OP_ldr, (d), (s))
#define INSTR_CREATE_str(dc, d, s) \
  instr_create_1dst_1src((dc), OP_str, (d), (s))
#define INSTR_CREATE_mrs(dc, d, s) \
  instr_create_1dst_1src((dc), OP_mrs, (d), (s))
#define INSTR_CREATE_msr(dc, d, s) \
  instr_create_1dst_1src((dc), OP_mrs, (d), (s))
#define INSTR_CREATE_vldr(dc, d, s) \
  instr_create_1dst_1src((dc), OP_vldr, (d), (s))
#define INSTR_CREATE_vstr(dc, d, s) \
  instr_create_1dst_1src((dc), OP_vstr, (d), (s))
#define INSTR_CREATE_pop(dc, d) \
  instr_create_2dst_2src((dc), OP_pop, (d), opnd_create_reg(REG_XSP), \
    opnd_create_reg(REG_XSP),                                         \
    opnd_create_base_disp(REG_XSP, REG_NULL, 0, 0, OPSZ_VARSTACK))

/* FIXME i#1551: provide cross-platform INSTR_CREATE_* macros.
 * The arithmetic operations are different between ARM and X86 in several ways, e.g.,
 * arith-flag update and destructive source opnd, so we need a way to provide
 * cross-platform INSTR_CREATE_* macros to avoid confusion.
 */
#define INSTR_CREATE_add(dc, d, s) \
  instr_create_1dst_2src((dc), OP_add, (d), (s), (d))
#define INSTR_CREATE_add_noflags(dc, d, s) INSTR_CREATE_add(dc, d, s)
#define INSTR_CREATE_sub(dc, d, s) \
  instr_create_1dst_2src((dc), OP_sub, (d), (s), (d))
/* @} */ /* end doxygen group */

/** @name no destination, 1 source */
/* @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_bkpt macro creates an instr_t with opcode OP_bkpt and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param i  The opnd_t explicit source operand for the instruction, which
 * must be a 1-byte immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_bkpt(dc, i) \
    instr_create_0dst_1src((dc), OP_bkpt, (i))
/* @} */ /* end doxygen group */

/* DR_API EXPORT END */

#endif /* _INSTR_CREATE_H_ */
