/* **********************************************************
 * Copyright (c) 2011-2017 Google, Inc. All rights reserved.
 * Copyright (c) 2016-2018 ARM Limited. All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc. All rights reserved.
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
 * Used in an additional immediate source operand to a vector operation, denotes
 * half-precision floating point vector elements. See \ref sec_IR_AArch64.
 */
#define FSZ_HALF 1

/**
 * Operand indicating half-precision floating point vector elements for the
 * other operands of the containing instruction.
 */
#define OPND_CREATE_HALF() OPND_CREATE_INT8(FSZ_HALF)

/**
 * Used in an additional immediate source operand to a vector operation, denotes
 * single-precision floating point vector elements. See \ref sec_IR_AArch64.
 */
#define FSZ_SINGLE 2

/**
 * Operand indicating single-precision floating point vector elements for the
 * other operands of the containing instruction.
 */
#define OPND_CREATE_SINGLE() OPND_CREATE_INT8(FSZ_SINGLE)

/**
 * Used in an additional immediate source operand to a vector operation, denotes
 * double-precision floating point vector elements. See \ref sec_IR_AArch64.
 */
#define FSZ_DOUBLE 3

/**
 * Operand indicating double-precision floating point vector elements for the
 * other operands of the containing instruction.
 */
#define OPND_CREATE_DOUBLE() OPND_CREATE_INT8(FSZ_DOUBLE)


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

/** Create a zero register operand of the same size as reg. */
#define OPND_CREATE_ZR(reg) \
  opnd_create_reg(opnd_get_size(reg) == OPSZ_4 ? DR_REG_WZR : DR_REG_XZR)

/** Create an operand specifying LSL, the default shift type when there is no shift. */
#define OPND_CREATE_LSL() \
  opnd_add_flags(OPND_CREATE_INT(DR_SHIFT_LSL), DR_OPND_IS_SHIFT)

/****************************************************************************
 * Platform-independent INSTR_CREATE_* macros
 */
/** @name Platform-independent macros */
/* @{ */ /* doxygen start group */

/**
 * This platform-independent macro creates an instr_t for a debug trap
 * instruction, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define XINST_CREATE_debug_instr(dc) \
  INSTR_CREATE_brk((dc), OPND_CREATE_INT16(0))

/**
 * This platform-independent macro creates an instr_t for a 4-byte
 * or 8-byte memory load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param m   The source memory opnd.
 */
#define XINST_CREATE_load(dc, r, m) INSTR_CREATE_ldr((dc), (r), (m))

/**
 * This platform-independent macro creates an instr_t which loads 1 byte
 * from memory, zero-extends it to 4 bytes, and writes it to a 4 byte
 * destination register.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param m   The source memory opnd.
 */
#define XINST_CREATE_load_1byte_zext4(dc, r, m) INSTR_CREATE_ldrb(dc, r, m)

/**
 * This platform-independent macro creates an instr_t for a 1-byte
 * memory load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param m   The source memory opnd.
 */
#define XINST_CREATE_load_1byte(dc, r, m) INSTR_CREATE_ldrb(dc, r, m)

/**
 * This platform-independent macro creates an instr_t for a 2-byte
 * memory load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param m   The source memory opnd.
 */
#define XINST_CREATE_load_2bytes(dc, r, m) INSTR_CREATE_ldrh(dc, r, m)

/**
 * This platform-independent macro creates an instr_t for a 4-byte
 * or 8-byte memory store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r   The source register opnd.
 */
#define XINST_CREATE_store(dc, m, r) \
  (opnd_is_base_disp(m) && \
   opnd_get_disp(m) % opnd_size_in_bytes(opnd_get_size(m)) != 0 ? \
    INSTR_CREATE_stur(dc, m, \
      opnd_create_reg(reg_resize_to_opsz(opnd_get_reg(r), opnd_get_size(m)))) : \
    INSTR_CREATE_str(dc, m, \
      opnd_create_reg(reg_resize_to_opsz(opnd_get_reg(r), opnd_get_size(m)))))

/**
 * This platform-independent macro creates an instr_t for a 1-byte
 * memory store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r   The source register opnd.
 */
#define XINST_CREATE_store_1byte(dc, m, r) INSTR_CREATE_strb(dc, m, \
   opnd_create_reg(reg_resize_to_opsz(opnd_get_reg(r), OPSZ_4)))

/**
 * This platform-independent macro creates an instr_t for a 2-byte
 * memory store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r   The source register opnd.
 */
#define XINST_CREATE_store_2bytes(dc, m, r) INSTR_CREATE_strh(dc, m, \
  opnd_create_reg(reg_resize_to_opsz(opnd_get_reg(r), OPSZ_4)))

/**
 * This platform-independent macro creates an instr_t for a register
 * to register move instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d   The destination register opnd.
 * \param s   The source register opnd.
 */
#define XINST_CREATE_move(dc, d, s) \
  ((opnd_get_reg(d) == DR_REG_XSP || opnd_get_reg(s) == DR_REG_XSP || \
    opnd_get_reg(d) == DR_REG_WSP || opnd_get_reg(s) == DR_REG_WSP) ? \
   instr_create_1dst_4src(dc, OP_add, d, s, OPND_CREATE_INT(0), \
                          OPND_CREATE_LSL(), OPND_CREATE_INT(0)) : \
   instr_create_1dst_4src(dc, OP_orr, d, OPND_CREATE_ZR(d), s, \
                          OPND_CREATE_LSL(), OPND_CREATE_INT(0)))

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
 * This platform-independent macro creates an instr_t for an indirect
 * jump instruction through a register.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The register opnd holding the target.
 */
#define XINST_CREATE_jump_reg(dc, r) INSTR_CREATE_br((dc), (r))

/**
 * This platform-independent macro creates an instr_t for an immediate
 * integer load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param i   The source immediate integer opnd.
 */
#define XINST_CREATE_load_int(dc, r, i) \
  (opnd_get_immed_int(i) < 0 ? \
   INSTR_CREATE_movn((dc), (r), OPND_CREATE_INT32(~opnd_get_immed_int(i)), \
                     OPND_CREATE_INT(0)) : \
   INSTR_CREATE_movz((dc), (r), (i), OPND_CREATE_INT(0)))

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
 * This platform-independent macro creates an instr_t for an unconditional
 * branch instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param t   The opnd_t target operand for the instruction, which can be
 * either a pc (opnd_create_pc)()) or an instr_t (opnd_create_instr()).
 * Be sure to ensure that the limited reach of this short branch will reach
 * the target (a pc operand is not suitable for most uses unless you know
 * precisely where this instruction will be encoded).
 */
#define XINST_CREATE_call(dc, t) INSTR_CREATE_bl(dc, t)

/**
 * This platform-independent macro creates an instr_t for a conditional
 * branch instruction that branches if the previously-set condition codes
 * indicate the condition indicated by \p pred.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param pred  The #dr_pred_type_t condition to match.
 * \param t   The opnd_t target operand for the instruction, which can be
 * either a pc (opnd_create_pc)()) or an instr_t (opnd_create_instr()).
 * Be sure to ensure that the limited reach of this short branch will reach
 * the target (a pc operand is not suitable for most uses unless you know
 * precisely where this instruction will be encoded).
 */
#define XINST_CREATE_jump_cond(dc, pred, t) \
    (INSTR_PRED(INSTR_CREATE_bcond((dc), (t)), (pred)))

/**
 * This platform-independent macro creates an instr_t for an addition
 * instruction that does not affect the status flags.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_add(dc, d, s) INSTR_CREATE_add(dc, d, d, s)

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
#define XINST_CREATE_add_2src(dc, d, s1, s2) INSTR_CREATE_add(dc, d, s1, s2)

/**
 * This platform-independent macro creates an instr_t for an addition
 * instruction that does not affect the status flags and takes two register sources
 * plus a destination, with one source being shifted logically left by
 * an immediate amount that is limited to either 0, 1, 2, or 3.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s1  The opnd_t explicit first source operand for the instruction.  This
 * must be a register.
 * \param s2_toshift  The opnd_t explicit source operand for the instruction.  This
 * must be a register.
 * \param shift_amount  An integer value that must be either 0, 1, 2, or 3.
 */
#define XINST_CREATE_add_sll(dc, d, s1, s2_toshift, shift_amount) \
  INSTR_CREATE_add_shift((dc), (d), (s1), (s2_toshift), \
    OPND_CREATE_LSL(), OPND_CREATE_INT8(shift_amount))

/**
 * This platform-independent macro creates an instr_t for an addition
 * instruction that does affect the status flags.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_add_s(dc, d, s) INSTR_CREATE_adds(dc, d, d, s)

/**
 * This platform-independent macro creates an instr_t for a subtraction
 * instruction that does not affect the status flags.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_sub(dc, d, s) INSTR_CREATE_sub(dc, d, d, s)

/**
 * This platform-independent macro creates an instr_t for a subtraction
 * instruction that does affect the status flags.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_sub_s(dc, d, s) INSTR_CREATE_subs(dc, d, d, s)

/**
 * This platform-independent macro creates an instr_t for a bitwise and
 * instruction that does affect the status flags.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_and_s(dc, d, s) INSTR_CREATE_ands(dc, d, d, s)

/**
 * This platform-independent macro creates an instr_t for a comparison
 * instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param s1  The opnd_t explicit source operand for the instruction.
 * \param s2  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_cmp(dc, s1, s2) INSTR_CREATE_cmp(dc, s1, s2)

/**
 * This platform-independent macro creates an instr_t for a software
 * interrupt instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param i   The source integer constant opnd_t operand.
 */
#define XINST_CREATE_interrupt(dc, i) INSTR_CREATE_svc(dc, (i))

/**
 * This platform-independent macro creates an instr_t for a logical right shift
 * instruction that does affect the status flags.
 * \param dc         The void * dcontext used to allocate memory for the instr_t.
 * \param d          The opnd_t explicit destination operand for the instruction.
 * \param rm_or_imm  The opnd_t explicit source operand for the instruction.
 */
/* FIXME i#2440: I'm not sure this is correct.  Use INSTR_CREATE_lsr once available!
 * Also, what about writing the flags?  Most users don't want to read the flag results,
 * they just need to know whether they need to preserve the app's flags, so maybe
 * we can just document that this may not write them.
 */
#define XINST_CREATE_slr_s(dc, d, rm_or_imm) \
  (opnd_is_reg(rm_or_imm) ? \
    instr_create_1dst_2src(dc, OP_lsrv, d, d, rm_or_imm) : \
    instr_create_1dst_3src(dc, OP_ubfm, d, d, rm_or_imm, \
                           reg_is_32bit(opnd_get_reg(d)) ? OPND_CREATE_INT(31) : \
                                                           OPND_CREATE_INT(63)))

/**
 * This platform-independent macro creates an instr_t for a nop instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 */
#define XINST_CREATE_nop(dc) INSTR_CREATE_nop(dc)

/* @} */ /* end doxygen group */

/****************************************************************************
 * Manually-added ARM-specific INSTR_CREATE_* macros
 * FIXME i#1569: Add Doxygen headers.
 */

#define INSTR_CREATE_add(dc, rd, rn, rm_or_imm) \
  INSTR_CREATE_add_shift(dc, rd, rn, rm_or_imm, OPND_CREATE_LSL(), OPND_CREATE_INT(0))
#define INSTR_CREATE_add_extend(dc, rd, rn, rm, ext, exa) \
  instr_create_1dst_4src(dc, OP_add, rd, rn, \
    opnd_create_reg_ex(opnd_get_reg(rm), 0, DR_OPND_EXTENDED), \
    opnd_add_flags(ext, DR_OPND_IS_EXTEND), \
    exa)
#define INSTR_CREATE_add_shift(dc, rd, rn, rm_or_imm, sht, sha) \
  opnd_is_reg(rm_or_imm) ? \
  instr_create_1dst_4src((dc), OP_add, (rd), (rn), \
    opnd_create_reg_ex(opnd_get_reg(rm_or_imm), 0, DR_OPND_SHIFTED), \
    opnd_add_flags((sht), DR_OPND_IS_SHIFT), (sha)) : \
  instr_create_1dst_4src((dc), OP_add, (rd), (rn), (rm_or_imm), (sht), (sha))
#define INSTR_CREATE_adds(dc, rd, rn, rm_or_imm) \
  (opnd_is_reg(rm_or_imm) ? \
    INSTR_CREATE_adds_shift(dc, rd, rn, rm_or_imm, OPND_CREATE_LSL(), OPND_CREATE_INT(0)) : \
    INSTR_CREATE_adds_imm(dc, rd, rn, rm_or_imm, OPND_CREATE_INT(0)))
#define INSTR_CREATE_and(dc, rd, rn, rm_or_imm) \
  INSTR_CREATE_and_shift(dc, rd, rn, rm_or_imm, OPND_CREATE_LSL(), OPND_CREATE_INT(0))
#define INSTR_CREATE_and_shift(dc, rd, rn, rm, sht, sha) \
  instr_create_1dst_4src((dc), OP_and, (rd), (rn), \
    opnd_create_reg_ex(opnd_get_reg(rm), 0, DR_OPND_SHIFTED), \
    opnd_add_flags((sht), DR_OPND_IS_SHIFT), (sha))
#define INSTR_CREATE_ands(dc, rd, rn, rm_or_imm) \
  INSTR_CREATE_ands_shift(dc, rd, rn, rm_or_imm, OPND_CREATE_LSL(), OPND_CREATE_INT(0))
#define INSTR_CREATE_ands_shift(dc, rd, rn, rm, sht, sha) \
  instr_create_1dst_4src((dc), OP_ands, (rd), (rn), \
    opnd_create_reg_ex(opnd_get_reg(rm), 0, DR_OPND_SHIFTED), \
    opnd_add_flags((sht), DR_OPND_IS_SHIFT), (sha))
#define INSTR_CREATE_b(dc, pc) \
  instr_create_0dst_1src((dc), OP_b, (pc))
/**
 * This macro creates an instr_t for a conditional branch instruction. The condition
 * can be set using INSTR_PRED macro.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param pc The opnd_t target operand containing the program counter to jump to.
 */
#define INSTR_CREATE_bcond(dc, pc) \
  instr_create_0dst_1src((dc), OP_bcond, (pc))
/**
 * This macro creates an instr_t for a BL (branch and link) instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param pc The opnd_t target operand containing the program counter to jump to.
 */
#define INSTR_CREATE_bl(dc, pc) \
  instr_create_1dst_1src((dc), OP_bl, opnd_create_reg(DR_REG_X30), (pc))

#define INSTR_CREATE_adc(dc, Rd, Rn, Rm) \
  instr_create_1dst_2src((dc), OP_adc, (Rd), (Rn), (Rm))
#define INSTR_CREATE_adcs(dc, Rd, Rn, Rm) \
  instr_create_1dst_2src((dc), OP_adcs, (Rd), (Rn), (Rm))
#define INSTR_CREATE_adds_extend(dc, Rd, Rn, Rm, shift, imm3) \
  instr_create_1dst_4src((dc), OP_adds, (Rd), (Rn), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_EXTENDED), opnd_add_flags((shift), DR_OPND_IS_EXTEND), (imm3))
#define INSTR_CREATE_adds_imm(dc, Rd, Rn, imm12, shift_amt) \
  instr_create_1dst_4src((dc), OP_adds, (Rd), (Rn), (imm12), OPND_CREATE_LSL(), (shift_amt))
#define INSTR_CREATE_adds_shift(dc, Rd, Rn, Rm, shift, imm6) \
  instr_create_1dst_4src((dc), OP_adds, (Rd), (Rn), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm6))
#define INSTR_CREATE_br(dc, xn) \
  instr_create_0dst_1src((dc), OP_br, (xn))
#define INSTR_CREATE_blr(dc, xn) \
  instr_create_1dst_1src((dc), OP_blr, opnd_create_reg(DR_REG_X30), (xn))
#define INSTR_CREATE_brk(dc, imm) \
  instr_create_0dst_1src((dc), OP_brk, (imm))
#define INSTR_CREATE_cbnz(dc, pc, reg) \
  instr_create_0dst_2src((dc), OP_cbnz, (pc), (reg))
#define INSTR_CREATE_cbz(dc, pc, reg) \
  instr_create_0dst_2src((dc), OP_cbz, (pc), (reg))
#define INSTR_CREATE_cmp(dc, rn, rm_or_imm) \
  INSTR_CREATE_subs(dc, OPND_CREATE_ZR(rn), rn, rm_or_imm)
#define INSTR_CREATE_ldp(dc, rt1, rt2, mem) \
  instr_create_2dst_1src(dc, OP_ldp, rt1, rt2, mem)
#define INSTR_CREATE_ldr(dc, Rd, mem) \
  instr_create_1dst_1src((dc), OP_ldr, (Rd), (mem))
#define INSTR_CREATE_ldrb(dc, Rd, mem) \
  instr_create_1dst_1src(dc, OP_ldrb, Rd, mem)
#define INSTR_CREATE_ldrh(dc, Rd, mem) \
  instr_create_1dst_1src(dc, OP_ldrh, Rd, mem)
#define INSTR_CREATE_ldar(dc, Rt, mem) \
  instr_create_1dst_1src((dc), OP_ldar, (Rt), (mem))
#define INSTR_CREATE_ldarb(dc, Rt, mem) \
  instr_create_1dst_1src((dc), OP_ldarb, (Rt), (mem))
#define INSTR_CREATE_ldarh(dc, Rt, mem) \
  instr_create_1dst_1src((dc), OP_ldarh, (Rt), (mem))
#define INSTR_CREATE_movk(dc, rt, imm16, lsl) \
  instr_create_1dst_4src(dc, OP_movk, rt, rt, imm16, OPND_CREATE_LSL(), lsl)
#define INSTR_CREATE_movn(dc, rt, imm16, lsl) \
  instr_create_1dst_3src(dc, OP_movn, rt, imm16, OPND_CREATE_LSL(), lsl)
#define INSTR_CREATE_movz(dc, rt, imm16, lsl) \
  instr_create_1dst_3src(dc, OP_movz, rt, imm16, OPND_CREATE_LSL(), lsl)
#define INSTR_CREATE_mrs(dc, Xt, sysreg) \
  instr_create_1dst_1src((dc), OP_mrs, (Xt), (sysreg))
#define INSTR_CREATE_msr(dc, sysreg, Xt) \
  instr_create_1dst_1src((dc), OP_msr, (sysreg), (Xt))
#define INSTR_CREATE_nop(dc) \
  instr_create_0dst_0src((dc), OP_nop)
#define INSTR_CREATE_ret(dc, Rn) \
  instr_create_0dst_1src((dc), OP_ret, (Rn))
#define INSTR_CREATE_stp(dc, mem, rt1, rt2) \
  instr_create_1dst_2src(dc, OP_stp, mem, rt1, rt2)
#define INSTR_CREATE_str(dc, mem, rt) \
  instr_create_1dst_1src(dc, OP_str, mem, rt)
#define INSTR_CREATE_strb(dc, mem, rt) \
  instr_create_1dst_1src(dc, OP_strb, mem, rt)
#define INSTR_CREATE_strh(dc, mem, rt) \
  instr_create_1dst_1src(dc, OP_strh, mem, rt)
#define INSTR_CREATE_stur(dc, mem, rt) \
  instr_create_1dst_1src(dc, OP_stur, mem, rt)
#define INSTR_CREATE_sturh(dc, mem, rt) \
  instr_create_1dst_1src(dc, OP_sturh, mem, rt)
#define INSTR_CREATE_sub(dc, rd, rn, rm_or_imm) \
  INSTR_CREATE_sub_shift(dc, rd, rn, rm_or_imm, OPND_CREATE_LSL(), OPND_CREATE_INT(0))
#define INSTR_CREATE_sub_extend(dc, rd, rn, rm, ext, exa) \
  instr_create_1dst_4src(dc, OP_sub, rd, rn, \
    opnd_create_reg_ex(opnd_get_reg(rm), 0, DR_OPND_EXTENDED), \
    opnd_add_flags(ext, DR_OPND_IS_EXTEND), \
    exa)
#define INSTR_CREATE_sub_shift(dc, rd, rn, rm_or_imm, sht, sha) \
  opnd_is_reg(rm_or_imm) ? \
  instr_create_1dst_4src((dc), OP_sub, (rd), (rn), \
    opnd_create_reg_ex(opnd_get_reg(rm_or_imm), 0, DR_OPND_SHIFTED), \
    opnd_add_flags((sht), DR_OPND_IS_SHIFT), (sha)) : \
  instr_create_1dst_4src((dc), OP_sub, (rd), (rn), (rm_or_imm), (sht), (sha))
#define INSTR_CREATE_subs(dc, rd, rn, rm_or_imm) \
  INSTR_CREATE_subs_shift(dc, rd, rn, rm_or_imm, OPND_CREATE_LSL(), OPND_CREATE_INT(0))
#define INSTR_CREATE_subs_extend(dc, rd, rn, rm, ext, exa) \
  instr_create_1dst_4src(dc, OP_subs, rd, rn, \
    opnd_create_reg_ex(opnd_get_reg(rm), 0, DR_OPND_EXTENDED), \
    opnd_add_flags(ext, DR_OPND_IS_EXTEND), \
    exa)
#define INSTR_CREATE_subs_shift(dc, rd, rn, rm_or_imm, sht, sha) \
  opnd_is_reg(rm_or_imm) ? \
  instr_create_1dst_4src((dc), OP_subs, (rd), (rn), \
    opnd_create_reg_ex(opnd_get_reg(rm_or_imm), 0, DR_OPND_SHIFTED), \
    opnd_add_flags((sht), DR_OPND_IS_SHIFT), (sha)) : \
  instr_create_1dst_4src((dc), OP_subs, (rd), (rn), (rm_or_imm), (sht), (sha))
#define INSTR_CREATE_svc(dc, imm) \
  instr_create_0dst_1src((dc), OP_svc, (imm))
#define INSTR_CREATE_adr(dc, rt, imm) \
  instr_create_1dst_1src(dc, OP_adr, rt, imm)
#define INSTR_CREATE_adrp(dc, rt, imm) \
  instr_create_1dst_1src(dc, OP_adrp, rt, imm)

/* FIXME i#1569: these two should perhaps not be provided */
#define INSTR_CREATE_add_shimm(dc, rd, rn, rm_or_imm, sht, sha) \
  INSTR_CREATE_add_shift(dc, rd, rn, rm_or_imm, sht, sha)
#define INSTR_CREATE_sub_shimm(dc, rd, rn, rm_or_imm, sht, sha) \
  INSTR_CREATE_sub_shift(dc, rd, rn, rm_or_imm, sht, sha)

/**
 * Creates a FABD vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fabd_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fabd, Rd, Rm, Rn, width)

/**
 * Creates a FABS floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_fabs_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_fabs, Rd, Rm)

/**
 * Creates a FACGE vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_facge_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_facge, Rd, Rm, Rn, width)

/**
 * Creates a FACGT vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_facgt_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_facgt, Rd, Rm, Rn, width)

/**
 * Creates a FADD vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fadd_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fadd, Rd, Rm, Rn, width)

/**
 * Creates a FADD floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fadd_scalar(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_fadd, Rd, Rm, Rn)

/**
 * Creates a FADDP vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_faddp_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_faddp, Rd, Rm, Rn, width)

/**
 * Creates a FCMEQ vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fcmeq_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fcmeq, Rd, Rm, Rn, width)

/**
 * Creates a FCMGE vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fcmge_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fcmge, Rd, Rm, Rn, width)

/**
 * Creates a FCMGT vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fcmgt_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fcmgt, Rd, Rm, Rn, width)

/**
 * Creates a FDIV vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fdiv_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fdiv, Rd, Rm, Rn, width)

/**
 * Creates a FDIV floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fdiv_scalar(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_fdiv, Rd, Rm, Rn)

/**
 * Creates a FMADD floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param Ra      The third input register.
 */
#define INSTR_CREATE_fmadd_scalar(dc, Rd, Rm, Rn, Ra) \
    instr_create_1dst_3src(dc, OP_fmadd, Rd, Rm, Rn, Ra)

/**
 * Creates a FMAX vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fmax_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fmax, Rd, Rm, Rn, width)

/**
 * Creates a FMAX floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fmax_scalar(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_fmax, Rd, Rm, Rn)

/**
 * Creates a FMAXNM vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fmaxnm_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fmaxnm, Rd, Rm, Rn, width)

/**
 * Creates a FMAXNM floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fmaxnm_scalar(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_fmaxnm, Rd, Rm, Rn)

/**
 * Creates a FMAXNMP vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fmaxnmp_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fmaxnmp, Rd, Rm, Rn, width)

/**
 * Creates a FMAXP vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fmaxp_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fmaxp, Rd, Rm, Rn, width)

/**
 * Creates a FMIN vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fmin_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fmin, Rd, Rm, Rn, width)

/**
 * Creates a FMIN floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fmin_scalar(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_fmin, Rd, Rm, Rn)

/**
 * Creates a FMINNM vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fminnm_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fminnm, Rd, Rm, Rn, width)

/**
 * Creates a FMINNM floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fminnm_scalar(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_fminnm, Rd, Rm, Rn)

/**
 * Creates a FMINNMP vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fminnmp_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fminnmp, Rd, Rm, Rn, width)

/**
 * Creates a FMINP vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fminp_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fminp, Rd, Rm, Rn, width)

/**
 * Creates a FMLA vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fmla_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fmla, Rd, Rm, Rn, width)

/**
 * Creates a FMLS vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fmls_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fmls, Rd, Rm, Rn, width)

/**
 * Creates a FMOV floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_fmov_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_fmov, Rd, Rm)

/**
 * Creates a FMSUB floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param Ra      The third input register.
 */
#define INSTR_CREATE_fmsub_scalar(dc, Rd, Rm, Rn, Ra) \
    instr_create_1dst_3src(dc, OP_fmsub, Rd, Rm, Rn, Ra)

/**
 * Creates a FMUL vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fmul_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fmul, Rd, Rm, Rn, width)

/**
 * Creates a FMUL floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fmul_scalar(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_fmul, Rd, Rm, Rn)

/**
 * Creates a FMULX vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fmulx_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fmulx, Rd, Rm, Rn, width)

/**
 * Creates a FNEG floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_fneg_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_fneg, Rd, Rm)

/**
 * Creates a FNMADD floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param Ra      The third input register.
 */
#define INSTR_CREATE_fnmadd_scalar(dc, Rd, Rm, Rn, Ra) \
    instr_create_1dst_3src(dc, OP_fnmadd, Rd, Rm, Rn, Ra)

/**
 * Creates a FNMSUB floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param Ra      The third input register.
 */
#define INSTR_CREATE_fnmsub_scalar(dc, Rd, Rm, Rn, Ra) \
    instr_create_1dst_3src(dc, OP_fnmsub, Rd, Rm, Rn, Ra)

/**
 * Creates a FNMUL floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fnmul_scalar(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_fnmul, Rd, Rm, Rn)

/**
 * Creates a FRECPS vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_frecps_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_frecps, Rd, Rm, Rn, width)

/**
 * Creates a FRINTA floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_frinta_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_frinta, Rd, Rm)

/**
 * Creates a FRINTI floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_frinti_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_frinti, Rd, Rm)

/**
 * Creates a FRINTM floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_frintm_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_frintm, Rd, Rm)

/**
 * Creates a FRINTN floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_frintn_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_frintn, Rd, Rm)

/**
 * Creates a FRINTP floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_frintp_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_frintp, Rd, Rm)

/**
 * Creates a FRINTX floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_frintx_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_frintx, Rd, Rm)

/**
 * Creates a FRINTZ floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_frintz_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_frintz, Rd, Rm)

/**
 * Creates a FRSQRTS vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_frsqrts_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_frsqrts, Rd, Rm, Rn, width)

/**
 * Creates a FSQRT floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_fsqrt_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_fsqrt, Rd, Rm)

/**
 * Creates a FSUB vector instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fsub_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fsub, Rd, Rm, Rn, width)

/**
 * Creates a FSUB floating point instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fsub_scalar(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_fsub, Rd, Rm, Rn)

/* DR_API EXPORT END */

#endif /* INSTR_CREATE_H */
