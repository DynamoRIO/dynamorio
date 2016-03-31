/* **********************************************************
 * Copyright (c) 2002-2010 VMware, Inc. All rights reserved.
 * Copyright (c) 2011-2015 Google, Inc. All rights reserved.
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef INSTR_CREATE_H
#define INSTR_CREATE_H 1

#include "../instr_create_shared.h"
#include "instr.h"

/* DR_API EXPORT TOFILE dr_ir_macros_aarch64.h */
/* DR_API EXPORT BEGIN */
/**
 * @file dr_ir_macros_aarch64.h
 * @brief AArch64-specific instruction creation convenience macros.
 */

/**
 * Create an absolute address operand encoded as pc-relative.
 * Encoding will fail if addr is out of the maximum signed displacement
 * reach for the architecture.
 */
#define OPND_CREATE_ABSMEM(addr, size) \
  opnd_create_rel_addr(addr, size)

/**
 * Create an immediate integer operand. For AArch64 the size of an immediate
 * is ignored when encoding, so there is no need to specify the final size.
 */
#define OPND_CREATE_INT(val) OPND_CREATE_INTPTR(val)

/**
 * This platform-independent macro creates an instr_t for a debug trap
 * instruction, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define XINST_CREATE_debug_instr(dc) \
  INSTR_CREATE_brk((dc), OPND_CREATE_INT16(1))

/**
 * This platform-independent macro creates an instr_t for a 4-byte
 * or 8-byte memory load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param m   The source memory opnd.
 */
#define XINST_CREATE_load(dc, r, m) INSTR_CREATE_ldr((dc), (r), (m))

/**
 * This platform-independent macro creates an instr_t for a 4-byte
 * or 8-byte memory store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r   The source register opnd.
 */
#define XINST_CREATE_store(dc, m, r) INSTR_CREATE_str((dc), (m), (r))

/**
 * This platform-independent macro creates an instr_t for a 2-byte
 * memory store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r   The source register opnd.
 */
#define XINST_CREATE_store_2bytes(dc, m, r) INSTR_CREATE_strh((dc), (m), (r))

/**
 * This platform-independent macro creates an instr_t for a register
 * to register move instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d   The destination register opnd.
 * \param s   The source register opnd.
 */
#define XINST_CREATE_move(dc, d, s) INSTR_CREATE_mov((dc), (d), (s))

/**
 * This platform-independent macro creates an instr_t for a multimedia
 * register load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param m   The source memory opnd.
 */
#define XINST_CREATE_load_simd(dc, r, m) INSTR_CREATE_ldr((dc), (r), (m))

/**
 * This platform-independent macro creates an instr_t for a multimedia
 * register store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r   The source register opnd.
 */
#define XINST_CREATE_store_simd(dc, m, r) INSTR_CREATE_str((dc), (m), (r))

/**
 * This platform-independent macro creates an instr_t for an immediate
 * integer load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param i   The source immediate integer opnd.
 */
#define XINST_CREATE_load_int(dc, r, i) INSTR_CREATE_mov((dc), (r), (i))

/**
 * This platform-independent macro creates an instr_t for a return instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 */
#define XINST_CREATE_return(dc) \
    INSTR_CREATE_ret(dc, opnd_create_reg(DR_REG_X30))

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
#define XINST_CREATE_jump(dc, t) INSTR_CREATE_b((dc), (t))

/**
 * This platform-independent macro creates an instr_t for an indirect
 * jump instruction through a register.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The register opnd holding the target.
 */
#define XINST_CREATE_jump_reg(dc, r) INSTR_CREATE_br((dc), (r))

/**
 * This platform-independent macro creates an instr_t for an unconditional
 * branch instruction with the smallest available reach.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param t   The opnd_t target operand for the instruction, which can be
 * either a pc (opnd_create_pc)()) or an instr_t (opnd_create_instr()).
 * Be sure to ensure that the limited reach of this short branch will reach
 * the target (a pc operand is not suitable for most uses unless you know
 * precisely where this instruction will be encoded).
 */
#define XINST_CREATE_jump_short(dc, t) INSTR_CREATE_b((dc), (t))

/**
 * This platform-independent macro creates an instr_t for an addition
 * instruction that does not affect the status flags.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_add(dc, d, s) INSTR_CREATE_add((dc), (d), (d), (s))

/**
 * This platform-independent macro creates an instr_t for an addition
 * instruction that does not affect the status flags and takes two sources
 * plus a destination.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s1  The opnd_t explicit first source operand for the instruction. This
 * must be a register.
 * \param s2  The opnd_t explicit source operand for the instruction. This
 * can be either a register or an immediate integer.
 */
#define XINST_CREATE_add_2src(dc, d, s1, s2) \
  INSTR_CREATE_add((dc), (d), (s1), (s2))

/**
 * This platform-independent macro creates an instr_t for a subtraction
 * instruction that does not affect the status flags.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_sub(dc, d, s) INSTR_CREATE_sub((dc), (d), (d), (s))

/**
 * This platform-independent macro creates an instr_t for a nop instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 */
#define XINST_CREATE_nop(dc) INSTR_CREATE_nop(dc)

/****************************************************************************
 * Manually-added ARM-specific INSTR_CREATE_* macros
 */

#define INSTR_CREATE_add(dc, Rd, Rn, Rm_or_imm) \
  (opnd_is_reg(Rm_or_imm) ? \
   INSTR_CREATE_add_shimm((dc), (Rd), (Rn), (Rm_or_imm), \
     OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) : \
   instr_create_1dst_2src((dc), OP_add, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_b(dc, pc) \
  instr_create_0dst_1src((dc), OP_b, (pc))
#define INSTR_CREATE_br(dc, Xn) \
  instr_create_0dst_1src((dc), OP_br, (Xn))
#define INSTR_CREATE_brk(dc, imm) \
  instr_create_0dst_1src((dc), OP_brk, (imm))
#define INSTR_CREATE_ldr(dc, Rd, mem) \
  instr_create_1dst_1src((dc), OP_ldr, (Rd), (mem))
#define INSTR_CREATE_mov(dc, Rd, Rm_or_imm) \
  instr_create_1dst_1src((dc), OP_mov, (Rd), (Rm_or_imm))
#define INSTR_CREATE_mrs(dc, Xt, sysreg) \
  instr_create_1dst_1src((dc), OP_mrs, (Xt), (sysreg))
#define INSTR_CREATE_msr(dc, sysreg, Xt) \
  instr_create_1dst_1src((dc), OP_msr, (sysreg), (Xt))
#define INSTR_CREATE_nop(dc) \
  instr_create_0dst_0src((dc), OP_nop)
#define INSTR_CREATE_ret(dc, Rn) \
  instr_create_0dst_1src((dc), OP_ret, (Rn))
#define INSTR_CREATE_str(dc, mem, Rt) \
  instr_create_1dst_1src((dc), OP_str, (mem), (Rt))
#define INSTR_CREATE_strh(dc, mem, Rt) \
  instr_create_1dst_1src((dc), OP_strh, (mem), (Rt))
#define INSTR_CREATE_sub(dc, Rd, Rn, Rm_or_imm) \
  (opnd_is_reg(Rm_or_imm) ? \
   INSTR_CREATE_sub_shimm((dc), (Rd), (Rn), (Rm_or_imm), \
     OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) : \
   instr_create_1dst_2src((dc), OP_sub, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_svc(dc, imm) \
  instr_create_0dst_1src((dc), OP_svc, (imm))

#define INSTR_CREATE_add_shimm(dc, Rd, Rn, Rm, shift, imm) \
  instr_create_1dst_4src((dc), OP_add, (Rd), (Rn), \
  opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
  opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_sub_shimm(dc, Rd, Rn, Rm, shift, imm) \
  instr_create_1dst_4src((dc), OP_sub, (Rd), (Rn), \
  opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
  opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))

/* FIXME i#1569: placeholder for undecoded instructions */
#define INSTR_CREATE_xx(dc, enc) \
  instr_create_0dst_1src((dc), OP_xx, OPND_CREATE_INT32(enc))

/* DR_API EXPORT END */

#endif /* INSTR_CREATE_H */
