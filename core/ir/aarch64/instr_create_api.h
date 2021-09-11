/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc. All rights reserved.
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

#ifndef DR_IR_MACROS_AARCH64_H
#define DR_IR_MACROS_AARCH64_H 1

/**
 * Used in an additional immediate source operand to a vector operation, denotes
 * 8 bit vector element width. See \ref sec_IR_AArch64.
 */
#define VECTOR_ELEM_WIDTH_BYTE 0

/**
 * Used in an additional immediate source operand to a vector operation, denotes
 * 16 bit vector element width. See \ref sec_IR_AArch64.
 */
#define VECTOR_ELEM_WIDTH_HALF 1

/**
 * Used in an additional immediate source operand to a vector operation, denotes
 * 32 bit vector element width. See \ref sec_IR_AArch64.
 */
#define VECTOR_ELEM_WIDTH_SINGLE 2

/**
 * Used in an additional immediate source operand to a vector operation, denotes
 * 64 bit vector element width. See \ref sec_IR_AArch64.
 */
#define VECTOR_ELEM_WIDTH_DOUBLE 3

/**
 * Operand denoting 8 bit vector element width for the other operands of
 * the containing instruction.
 */
#define OPND_CREATE_BYTE() OPND_CREATE_INT8(VECTOR_ELEM_WIDTH_BYTE)

/**
 * Operand denoting 16 bit vector element width for the other operands of
 * the containing instruction.
 */
#define OPND_CREATE_HALF() OPND_CREATE_INT8(VECTOR_ELEM_WIDTH_HALF)

/**
 * Operand denoting 32 bit vector element width for the other operands of
 * the containing instruction.
 */
#define OPND_CREATE_SINGLE() OPND_CREATE_INT8(VECTOR_ELEM_WIDTH_SINGLE)

/**
 * Operand denoting 64 bit vector element width for the other operands of
 * the containing instruction.
 */
#define OPND_CREATE_DOUBLE() OPND_CREATE_INT8(VECTOR_ELEM_WIDTH_DOUBLE)

/**
 * @file dr_ir_macros_aarch64.h
 * @brief AArch64-specific instruction creation convenience macros.
 */

/**
 * Create an absolute address operand encoded as pc-relative.
 * Encoding will fail if addr is out of the maximum signed displacement
 * reach for the architecture.
 */
#define OPND_CREATE_ABSMEM(addr, size) opnd_create_rel_addr(addr, size)

/**
 * Create an immediate integer operand. For AArch64 the size of an immediate
 * is ignored when encoding, so there is no need to specify the final size.
 */
#define OPND_CREATE_INT(val) OPND_CREATE_INTPTR(val)

/** Create a zero register operand of the same size as reg. */
#define OPND_CREATE_ZR(reg) \
    opnd_create_reg(opnd_get_size(reg) == OPSZ_4 ? DR_REG_WZR : DR_REG_XZR)

/** Create an operand specifying LSL, the default shift type when there is no shift. */
#define OPND_CREATE_LSL() opnd_add_flags(OPND_CREATE_INT(DR_SHIFT_LSL), DR_OPND_IS_SHIFT)

/** INSTR_CREATE_sys() operations are specified by the following immediates. */
enum {
    DR_DC_ZVA = 0x1ba1,    /**< Zero dcache by address. */
    DR_DC_IVAC = 0x3b1,    /**< Invalidate dcache to Point of Coherency. */
    DR_DC_ISW = 0x3b2,     /**< Invalidate dcache by set/way. */
    DR_DC_CVAC = 0x1bd1,   /**< Clean dcache to point of coherency. */
    DR_DC_CSW = 0x3d2,     /**< Clean dcache by set/way. */
    DR_DC_CVAU = 0x1bd9,   /**< Clean dcache to point of unification. */
    DR_DC_CIVAC = 0x1bf1,  /**< Clean and invalidate dcache to point of coherency. */
    DR_DC_CISW = 0x3f2,    /**< Clean and invalidate dcache by set/way. */
    DR_IC_IALLUIS = 0x388, /**< Invalidate icaches in ISD to point of unification. */
    DR_IC_IALLU = 0x3a8,   /**< Invalidate icaches to point of unification. */
    DR_IC_IVAU = 0x1ba9 /**< Invalidate icache by address to point of unification. */,
};

/****************************************************************************
 * Platform-independent INSTR_CREATE_* macros
 */
/** @name Platform-independent macros */
/** @{ */ /* doxygen start group */

/**
 * This platform-independent macro creates an instr_t for a debug trap
 * instruction, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define XINST_CREATE_debug_instr(dc) INSTR_CREATE_brk((dc), OPND_CREATE_INT16(0))

/**
 * This platform-independent macro creates an instr_t for a 4-byte
 * or 8-byte memory load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param m   The source memory opnd.
 */
#define XINST_CREATE_load(dc, r, m)                                                    \
    ((opnd_is_base_disp(m) &&                                                          \
      (opnd_get_disp(m) < 0 ||                                                         \
       opnd_get_disp(m) % opnd_size_in_bytes(opnd_get_size(m)) != 0))                  \
         ? INSTR_CREATE_ldur(                                                          \
               dc,                                                                     \
               opnd_create_reg(reg_resize_to_opsz(opnd_get_reg(r), opnd_get_size(m))), \
               m)                                                                      \
         : INSTR_CREATE_ldr(                                                           \
               dc,                                                                     \
               opnd_create_reg(reg_resize_to_opsz(opnd_get_reg(r), opnd_get_size(m))), \
               m))

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
#define XINST_CREATE_store(dc, m, r)                                                   \
    ((opnd_is_base_disp(m) &&                                                          \
      (opnd_get_disp(m) < 0 ||                                                         \
       opnd_get_disp(m) % opnd_size_in_bytes(opnd_get_size(m)) != 0))                  \
         ? INSTR_CREATE_stur(                                                          \
               dc, m,                                                                  \
               opnd_create_reg(reg_resize_to_opsz(opnd_get_reg(r), opnd_get_size(m)))) \
         : INSTR_CREATE_str(                                                           \
               dc, m,                                                                  \
               opnd_create_reg(reg_resize_to_opsz(opnd_get_reg(r), opnd_get_size(m)))))

/**
 * This platform-independent macro creates an instr_t for a 1-byte
 * memory store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r   The source register opnd.
 */
#define XINST_CREATE_store_1byte(dc, m, r) \
    INSTR_CREATE_strb(dc, m, opnd_create_reg(reg_resize_to_opsz(opnd_get_reg(r), OPSZ_4)))

/**
 * This platform-independent macro creates an instr_t for a 2-byte
 * memory store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r   The source register opnd.
 */
#define XINST_CREATE_store_2bytes(dc, m, r) \
    INSTR_CREATE_strh(dc, m, opnd_create_reg(reg_resize_to_opsz(opnd_get_reg(r), OPSZ_4)))

/**
 * This AArchXX-platform-independent macro creates an instr_t for a 2-register
 * memory store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r1  The first register opnd.
 * \param r2  The second register opnd.
 */
#define XINST_CREATE_store_pair(dc, m, r1, r2) INSTR_CREATE_stp(dc, m, r1, r2)

/**
 * This AArchXX-platform-independent macro creates an instr_t for a 2-register
 * memory load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r1  The first register opnd.
 * \param r2  The second register opnd.
 * \param m   The source memory opnd.
 */
#define XINST_CREATE_load_pair(dc, r1, r2, m) INSTR_CREATE_ldp(dc, r1, r2, m)

/**
 * This platform-independent macro creates an instr_t for a register
 * to register move instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d   The destination register opnd.
 * \param s   The source register opnd.
 */
#define XINST_CREATE_move(dc, d, s)                                      \
    ((opnd_get_reg(d) == DR_REG_XSP || opnd_get_reg(s) == DR_REG_XSP ||  \
      opnd_get_reg(d) == DR_REG_WSP || opnd_get_reg(s) == DR_REG_WSP)    \
         ? instr_create_1dst_4src(dc, OP_add, d, s, OPND_CREATE_INT(0),  \
                                  OPND_CREATE_LSL(), OPND_CREATE_INT(0)) \
         : instr_create_1dst_4src(dc, OP_orr, d, OPND_CREATE_ZR(d), s,   \
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
#define XINST_CREATE_load_int(dc, r, i)                                            \
    (opnd_get_immed_int(i) < 0                                                     \
         ? INSTR_CREATE_movn((dc), (r), OPND_CREATE_INT32(~opnd_get_immed_int(i)), \
                             OPND_CREATE_INT(0))                                   \
         : INSTR_CREATE_movz((dc), (r), (i), OPND_CREATE_INT(0)))

/**
 * This platform-independent macro creates an instr_t for a return instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 */
#define XINST_CREATE_return(dc) INSTR_CREATE_ret(dc, opnd_create_reg(DR_REG_X30))

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
#define XINST_CREATE_add_sll(dc, d, s1, s2_toshift, shift_amount)            \
    INSTR_CREATE_add_shift((dc), (d), (s1), (s2_toshift), OPND_CREATE_LSL(), \
                           OPND_CREATE_INT8(shift_amount))

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
#define XINST_CREATE_slr_s(dc, d, rm_or_imm)                                          \
    (opnd_is_reg(rm_or_imm)                                                           \
         ? instr_create_1dst_2src(dc, OP_lsrv, d, d, rm_or_imm)                       \
         : instr_create_1dst_3src(dc, OP_ubfm, d, d, rm_or_imm,                       \
                                  reg_is_32bit(opnd_get_reg(d)) ? OPND_CREATE_INT(31) \
                                                                : OPND_CREATE_INT(63)))

/**
 * This platform-independent macro creates an instr_t for a nop instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 */
#define XINST_CREATE_nop(dc) INSTR_CREATE_nop(dc)

/**
 * This platform-independent macro creates an instr_t for an indirect call instr
 * through a register.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The opnd_t explicit source operand for the instruction. This should
 * be a reg_id_t operand with the address of the subroutine.
 */
#define XINST_CREATE_call_reg(dc, r) INSTR_CREATE_blr(dc, r)

/** @} */ /* end doxygen group */

/****************************************************************************
 * Manually-added ARM-specific INSTR_CREATE_* macros
 * FIXME i#4106: Add Doxygen headers.
 * Newer doxygens give warnings causing build errors on these so we remove
 * from the docs until headers are added.
 */

/** \cond disabled_until_i4106_is_fixed */
#define INSTR_CREATE_add(dc, rd, rn, rm_or_imm)                                         \
    opnd_is_reg(rm_or_imm)                                                              \
        ? /* _extend supports sp in rn, so prefer it, but it does not support imm. */   \
        INSTR_CREATE_add_extend(dc, rd, rn, rm_or_imm, OPND_CREATE_INT(DR_EXTEND_UXTX), \
                                OPND_CREATE_INT(0))                                     \
        : INSTR_CREATE_add_shift(dc, rd, rn, rm_or_imm, OPND_CREATE_LSL(),              \
                                 OPND_CREATE_INT(0))
#define INSTR_CREATE_add_extend(dc, rd, rn, rm, ext, exa)                             \
    instr_create_1dst_4src(dc, OP_add, rd, rn,                                        \
                           opnd_create_reg_ex(opnd_get_reg(rm), 0, DR_OPND_EXTENDED), \
                           opnd_add_flags(ext, DR_OPND_IS_EXTEND), exa)
#define INSTR_CREATE_add_shift(dc, rd, rn, rm_or_imm, sht, sha)                \
    opnd_is_reg(rm_or_imm)                                                     \
        ? instr_create_1dst_4src(                                              \
              (dc), OP_add, (rd), (rn),                                        \
              opnd_create_reg_ex(opnd_get_reg(rm_or_imm), 0, DR_OPND_SHIFTED), \
              opnd_add_flags((sht), DR_OPND_IS_SHIFT), (sha))                  \
        : instr_create_1dst_4src((dc), OP_add, (rd), (rn), (rm_or_imm), (sht), (sha))
#define INSTR_CREATE_adds(dc, rd, rn, rm_or_imm)                             \
    (opnd_is_reg(rm_or_imm)                                                  \
         ? INSTR_CREATE_adds_shift(dc, rd, rn, rm_or_imm, OPND_CREATE_LSL(), \
                                   OPND_CREATE_INT(0))                       \
         : INSTR_CREATE_adds_imm(dc, rd, rn, rm_or_imm, OPND_CREATE_INT(0)))
/** \endcond disabled_until_i4106_is_fixed */

/**
 * Creates an AND instruction with one output and two inputs.
 * \param dc   The void * dcontext used to allocate memory for the instr_t.
 * \param rd   The output register.
 * \param rn   The first input register.
 * \param rm_or_imm   The second input register or immediate.
 */
#define INSTR_CREATE_and(dc, rd, rn, rm_or_imm)                             \
    (opnd_is_immed(rm_or_imm)                                               \
         ? instr_create_1dst_2src((dc), OP_and, (rd), (rn), (rm_or_imm))    \
         : INSTR_CREATE_and_shift(dc, rd, rn, rm_or_imm, OPND_CREATE_LSL(), \
                                  OPND_CREATE_INT(0)))
/** \cond disabled_until_i4106_is_fixed */
#define INSTR_CREATE_and_shift(dc, rd, rn, rm, sht, sha)                             \
    instr_create_1dst_4src((dc), OP_and, (rd), (rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((sht), DR_OPND_IS_SHIFT), (sha))
/** \endcond disabled_until_i4106_is_fixed */

/**
 * Creates an ANDS instruction with one output and two inputs.
 * \param dc   The void * dcontext used to allocate memory for the instr_t.
 * \param rd   The output register.
 * \param rn   The first input register.
 * \param rm_or_imm   The second input register or immediate.
 */
#define INSTR_CREATE_ands(dc, rd, rn, rm_or_imm)                             \
    (opnd_is_immed(rm_or_imm)                                                \
         ? instr_create_1dst_2src((dc), OP_ands, (rd), (rn), (rm_or_imm))    \
         : INSTR_CREATE_ands_shift(dc, rd, rn, rm_or_imm, OPND_CREATE_LSL(), \
                                   OPND_CREATE_INT(0)))
/** \cond disabled_until_i4106_is_fixed */
#define INSTR_CREATE_ands_shift(dc, rd, rn, rm, sht, sha)                            \
    instr_create_1dst_4src((dc), OP_ands, (rd), (rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((sht), DR_OPND_IS_SHIFT), (sha))
#define INSTR_CREATE_b(dc, pc) instr_create_0dst_1src((dc), OP_b, (pc))
/** \endcond disabled_until_i4106_is_fixed */
/**
 * This macro creates an instr_t for a conditional branch instruction. The condition
 * can be set using INSTR_PRED macro.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param pc The opnd_t target operand containing the program counter to jump to.
 */
#define INSTR_CREATE_bcond(dc, pc) instr_create_0dst_1src((dc), OP_bcond, (pc))
/**
 * This macro creates an instr_t for a BL (branch and link) instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param pc The opnd_t target operand containing the program counter to jump to.
 */
#define INSTR_CREATE_bl(dc, pc) \
    instr_create_1dst_1src((dc), OP_bl, opnd_create_reg(DR_REG_X30), (pc))

/** \cond disabled_until_i4106_is_fixed */
#define INSTR_CREATE_adc(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_adc, (Rd), (Rn), (Rm))
#define INSTR_CREATE_adcs(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_adcs, (Rd), (Rn), (Rm))
#define INSTR_CREATE_adds_extend(dc, Rd, Rn, Rm, shift, imm3)                         \
    instr_create_1dst_4src((dc), OP_adds, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_EXTENDED), \
                           opnd_add_flags((shift), DR_OPND_IS_EXTEND), (imm3))
#define INSTR_CREATE_adds_imm(dc, Rd, Rn, imm12, shift_amt)                       \
    instr_create_1dst_4src((dc), OP_adds, (Rd), (Rn), (imm12), OPND_CREATE_LSL(), \
                           (shift_amt))
#define INSTR_CREATE_adds_shift(dc, Rd, Rn, Rm, shift, imm6)                         \
    instr_create_1dst_4src((dc), OP_adds, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm6))
#define INSTR_CREATE_br(dc, xn) instr_create_0dst_1src((dc), OP_br, (xn))
#define INSTR_CREATE_blr(dc, xn) \
    instr_create_1dst_1src((dc), OP_blr, opnd_create_reg(DR_REG_X30), (xn))
#define INSTR_CREATE_brk(dc, imm) instr_create_0dst_1src((dc), OP_brk, (imm))
#define INSTR_CREATE_cbnz(dc, pc, reg) instr_create_0dst_2src((dc), OP_cbnz, (pc), (reg))
#define INSTR_CREATE_cbz(dc, pc, reg) instr_create_0dst_2src((dc), OP_cbz, (pc), (reg))
#define INSTR_CREATE_tbz(dc, pc, reg, imm) \
    instr_create_0dst_3src((dc), OP_tbz, (pc), (reg), (imm))
#define INSTR_CREATE_tbnz(dc, pc, reg, imm) \
    instr_create_0dst_3src((dc), OP_tbnz, (pc), (reg), (imm))
#define INSTR_CREATE_cmp(dc, rn, rm_or_imm) \
    INSTR_CREATE_subs(dc, OPND_CREATE_ZR(rn), rn, rm_or_imm)
#define INSTR_CREATE_eor(dc, d, s)                                      \
    INSTR_CREATE_eor_shift(dc, d, d, s, OPND_CREATE_INT8(DR_SHIFT_LSL), \
                           OPND_CREATE_INT8(0))
#define INSTR_CREATE_eor_shift(dc, rd, rn, rm, sht, sha)                             \
    instr_create_1dst_4src(dc, OP_eor, rd, rn,                                       \
                           opnd_create_reg_ex(opnd_get_reg(rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags(sht, DR_OPND_IS_SHIFT), sha)

#define INSTR_CREATE_ldp(dc, rt1, rt2, mem) \
    instr_create_2dst_1src(dc, OP_ldp, rt1, rt2, mem)
#define INSTR_CREATE_ldr(dc, Rd, mem) instr_create_1dst_1src((dc), OP_ldr, (Rd), (mem))
#define INSTR_CREATE_ldrb(dc, Rd, mem) instr_create_1dst_1src(dc, OP_ldrb, Rd, mem)
#define INSTR_CREATE_ldrsb(dc, Rd, mem) \
    instr_create_1dst_1src((dc), OP_ldrsb, (Rd), (mem))
#define INSTR_CREATE_ldrh(dc, Rd, mem) instr_create_1dst_1src(dc, OP_ldrh, Rd, mem)
#define INSTR_CREATE_ldur(dc, rt, mem) instr_create_1dst_1src(dc, OP_ldur, rt, mem)
#define INSTR_CREATE_ldar(dc, Rt, mem) instr_create_1dst_1src((dc), OP_ldar, (Rt), (mem))
#define INSTR_CREATE_ldarb(dc, Rt, mem) \
    instr_create_1dst_1src((dc), OP_ldarb, (Rt), (mem))
#define INSTR_CREATE_ldarh(dc, Rt, mem) \
    instr_create_1dst_1src((dc), OP_ldarh, (Rt), (mem))
/* TODO i#4532: Remove these superfluous 0x1f non-operands. */
#define INSTR_CREATE_ldxr(dc, Rd, mem)                                        \
    instr_create_1dst_3src((dc), OP_ldxr, (Rd), (mem), OPND_CREATE_INT(0x1f), \
                           OPND_CREATE_INT(0x1f))
/* TODO i#4532: Remove these superfluous 0x1f non-operands. */
#define INSTR_CREATE_ldxrb(dc, Rd, mem)                                        \
    instr_create_1dst_3src((dc), OP_ldxrb, (Rd), (mem), OPND_CREATE_INT(0x1f), \
                           OPND_CREATE_INT(0x1f))
/* TODO i#4532: Remove these superfluous 0x1f non-operands. */
#define INSTR_CREATE_ldxrh(dc, Rd, mem)                                        \
    instr_create_1dst_3src((dc), OP_ldxrh, (Rd), (mem), OPND_CREATE_INT(0x1f), \
                           OPND_CREATE_INT(0x1f))
/* TODO i#4532: Remove these superfluous 0x1f non-operands. */
#define INSTR_CREATE_ldxp(dc, rt1, rt2, mem) \
    instr_create_2dst_2src((dc), OP_ldxp, rt1, rt2, (mem), OPND_CREATE_INT(0x1f))
/* TODO i#4532: Remove these superfluous 0x1f non-operands. */
#define INSTR_CREATE_ldaxr(dc, Rd, mem)                                        \
    instr_create_1dst_3src((dc), OP_ldaxr, (Rd), (mem), OPND_CREATE_INT(0x1f), \
                           OPND_CREATE_INT(0x1f))
/* TODO i#4532: Remove these superfluous 0x1f non-operands. */
#define INSTR_CREATE_ldaxrb(dc, Rd, mem)                                        \
    instr_create_1dst_3src((dc), OP_ldaxrb, (Rd), (mem), OPND_CREATE_INT(0x1f), \
                           OPND_CREATE_INT(0x1f))
/* TODO i#4532: Remove these superfluous 0x1f non-operands. */
#define INSTR_CREATE_ldaxrh(dc, Rd, mem)                                        \
    instr_create_1dst_3src((dc), OP_ldaxrh, (Rd), (mem), OPND_CREATE_INT(0x1f), \
                           OPND_CREATE_INT(0x1f))
/* TODO i#4532: Remove these superfluous 0x1f non-operands. */
#define INSTR_CREATE_ldaxp(dc, rt1, rt2, mem) \
    instr_create_2dst_2src((dc), OP_ldaxp, rt1, rt2, (mem), OPND_CREATE_INT(0x1f))
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
#define INSTR_CREATE_nop(dc) instr_create_0dst_0src((dc), OP_nop)
#define INSTR_CREATE_ret(dc, Rn) instr_create_0dst_1src((dc), OP_ret, (Rn))
#define INSTR_CREATE_stp(dc, mem, rt1, rt2) \
    instr_create_1dst_2src(dc, OP_stp, mem, rt1, rt2)
#define INSTR_CREATE_str(dc, mem, rt) instr_create_1dst_1src(dc, OP_str, mem, rt)
#define INSTR_CREATE_strb(dc, mem, rt) instr_create_1dst_1src(dc, OP_strb, mem, rt)
#define INSTR_CREATE_strh(dc, mem, rt) instr_create_1dst_1src(dc, OP_strh, mem, rt)
#define INSTR_CREATE_stur(dc, mem, rt) instr_create_1dst_1src(dc, OP_stur, mem, rt)
#define INSTR_CREATE_sturh(dc, mem, rt) instr_create_1dst_1src(dc, OP_sturh, mem, rt)
/* TODO i#4532: Remove these superfluous 0x1f non-operands. */
#define INST_CREATE_stlr(dc, mem, rt)                                   \
    instr_create_1dst_3src(dc, OP_stlr, mem, rt, OPND_CREATE_INT(0x1f), \
                           OPND_CREATE_INT(0x1f))
/* TODO i#4532: Remove these superfluous 0x1f non-operands. */
#define INSTR_CREATE_stxr(dc, mem, rs, rt) \
    instr_create_2dst_2src(dc, OP_stxr, mem, rs, rt, OPND_CREATE_INT(0x1f))
/* TODO i#4532: Remove these superfluous 0x1f non-operands. */
#define INSTR_CREATE_stxrb(dc, mem, rs, rt) \
    instr_create_2dst_2src(dc, OP_stxrb, mem, rs, rt, OPND_CREATE_INT(0x1f))
/* TODO i#4532: Remove these superfluous 0x1f non-operands. */
#define INSTR_CREATE_stxrh(dc, mem, rs, rt) \
    instr_create_2dst_2src(dc, OP_stxrh, mem, rs, rt, OPND_CREATE_INT(0x1f))
/* TODO i#4532: Remove these superfluous 0x1f non-operands. */
#define INSTR_CREATE_stxp(dc, mem, rs, rt1, rt2) \
    instr_create_2dst_2src(dc, OP_stxp, mem, rs, rt1, rt2)
/* TODO i#4532: Remove these superfluous 0x1f non-operands. */
#define INSTR_CREATE_stlxr(dc, mem, rs, rt) \
    instr_create_2dst_2src(dc, OP_stlxr, mem, rs, rt, OPND_CREATE_INT(0x1f))
/* TODO i#4532: Remove these superfluous 0x1f non-operands. */
#define INSTR_CREATE_stlxrb(dc, mem, rs, rt) \
    instr_create_2dst_2src(dc, OP_stlxrb, mem, rs, rt, OPND_CREATE_INT(0x1f))
/* TODO i#4532: Remove these superfluous 0x1f non-operands. */
#define INSTR_CREATE_stlxrh(dc, mem, rs, rt) \
    instr_create_2dst_2src(dc, OP_stlxrh, mem, rs, rt, OPND_CREATE_INT(0x1f))
/* TODO i#4532: Remove these superfluous 0x1f non-operands. */
#define INSTR_CREATE_stlxp(dc, mem, rs, rt1, rt2) \
    instr_create_2dst_2src(dc, OP_stlxp, mem, rs, rt1, rt2)
#define INSTR_CREATE_sub(dc, rd, rn, rm_or_imm)                                         \
    opnd_is_reg(rm_or_imm)                                                              \
        ? /* _extend supports sp in rn, so prefer it, but it does not support imm. */   \
        INSTR_CREATE_sub_extend(dc, rd, rn, rm_or_imm, OPND_CREATE_INT(DR_EXTEND_UXTX), \
                                OPND_CREATE_INT(0))                                     \
        : INSTR_CREATE_sub_shift(dc, rd, rn, rm_or_imm, OPND_CREATE_LSL(),              \
                                 OPND_CREATE_INT(0))
#define INSTR_CREATE_sub_extend(dc, rd, rn, rm, ext, exa)                             \
    instr_create_1dst_4src(dc, OP_sub, rd, rn,                                        \
                           opnd_create_reg_ex(opnd_get_reg(rm), 0, DR_OPND_EXTENDED), \
                           opnd_add_flags(ext, DR_OPND_IS_EXTEND), exa)
#define INSTR_CREATE_sub_shift(dc, rd, rn, rm_or_imm, sht, sha)                \
    opnd_is_reg(rm_or_imm)                                                     \
        ? instr_create_1dst_4src(                                              \
              (dc), OP_sub, (rd), (rn),                                        \
              opnd_create_reg_ex(opnd_get_reg(rm_or_imm), 0, DR_OPND_SHIFTED), \
              opnd_add_flags((sht), DR_OPND_IS_SHIFT), (sha))                  \
        : instr_create_1dst_4src((dc), OP_sub, (rd), (rn), (rm_or_imm), (sht), (sha))
#define INSTR_CREATE_subs(dc, rd, rn, rm_or_imm) \
    INSTR_CREATE_subs_shift(dc, rd, rn, rm_or_imm, OPND_CREATE_LSL(), OPND_CREATE_INT(0))
#define INSTR_CREATE_subs_extend(dc, rd, rn, rm, ext, exa)                            \
    instr_create_1dst_4src(dc, OP_subs, rd, rn,                                       \
                           opnd_create_reg_ex(opnd_get_reg(rm), 0, DR_OPND_EXTENDED), \
                           opnd_add_flags(ext, DR_OPND_IS_EXTEND), exa)
#define INSTR_CREATE_subs_shift(dc, rd, rn, rm_or_imm, sht, sha)               \
    opnd_is_reg(rm_or_imm)                                                     \
        ? instr_create_1dst_4src(                                              \
              (dc), OP_subs, (rd), (rn),                                       \
              opnd_create_reg_ex(opnd_get_reg(rm_or_imm), 0, DR_OPND_SHIFTED), \
              opnd_add_flags((sht), DR_OPND_IS_SHIFT), (sha))                  \
        : instr_create_1dst_4src((dc), OP_subs, (rd), (rn), (rm_or_imm), (sht), (sha))
#define INSTR_CREATE_svc(dc, imm) instr_create_0dst_1src((dc), OP_svc, (imm))
#define INSTR_CREATE_adr(dc, rt, imm) instr_create_1dst_1src(dc, OP_adr, rt, imm)
#define INSTR_CREATE_adrp(dc, rt, imm) instr_create_1dst_1src(dc, OP_adrp, rt, imm)

#define INSTR_CREATE_sys(dc, op, Rn) instr_create_0dst_2src(dc, OP_sys, op, Rn)

/**
 * Creates a CLREX instruction.
 * \param dc   The void * dcontext used to allocate memory for the instr_t.
 */
/* TODO i#4532: Remove this superfluous operand. */
#define INSTR_CREATE_clrex(dc) instr_create_0dst_1src(dc, OP_clrex, OPND_CREATE_INT(0))

/* FIXME i#1569: these two should perhaps not be provided */
#define INSTR_CREATE_add_shimm(dc, rd, rn, rm_or_imm, sht, sha) \
    INSTR_CREATE_add_shift(dc, rd, rn, rm_or_imm, sht, sha)
#define INSTR_CREATE_sub_shimm(dc, rd, rn, rm_or_imm, sht, sha) \
    INSTR_CREATE_sub_shift(dc, rd, rn, rm_or_imm, sht, sha)
/** \endcond disabled_until_i4106_is_fixed */

/**
 * Creates an FMOV instruction to move between GPRs and floating point registers.
 * \param dc   The void * dcontext used to allocate memory for the instr_t.
 * \param Rd   The output register.
 * \param Rn   The first input register.
 */
#define INSTR_CREATE_fmov_general(dc, Rd, Rn) instr_create_1dst_1src(dc, OP_fmov, Rd, Rn)

/* -------- Advanced SIMD three same including fp16 versions ----------------
 * Some macros are also used for
 *   SVE Integer Arithmetic - Unpredicated Group
 *   Advanced SIMD three same (FP16)
 */

/**
 * Creates a SHADD vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_shadd_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_shadd, Rd, Rm, Rn, width)

/**
 * Creates a SQADD vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_sqadd_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sqadd, Rd, Rm, Rn, width)

/**
 * Creates a SRHADD vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_srhadd_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_srhadd, Rd, Rm, Rn, width)

/**
 * Creates a SHSUB vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_shsub_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_shsub, Rd, Rm, Rn, width)

/**
 * Creates a SQSUB vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_sqsub_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sqsub, Rd, Rm, Rn, width)

/**
 * Creates a CMGT vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_cmgt_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_cmgt, Rd, Rm, Rn, width)

/**
 * Creates a CMGE vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_cmge_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_cmge, Rd, Rm, Rn, width)

/**
 * Creates a SSHL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_sshl_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sshl, Rd, Rm, Rn, width)

/**
 * Creates a SQSHL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_sqshl_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sqshl, Rd, Rm, Rn, width)

/**
 * Creates a SRSHL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_srshl_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_srshl, Rd, Rm, Rn, width)

/**
 * Creates a SQRSHL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_sqrshl_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sqrshl, Rd, Rm, Rn, width)

/**
 * Creates a SMAX vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_smax_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_smax, Rd, Rm, Rn, width)

/**
 * Creates a SMIN vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_smin_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_smin, Rd, Rm, Rn, width)

/**
 * Creates a SABD vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_sabd_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sabd, Rd, Rm, Rn, width)

/**
 * Creates a SABA vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_saba_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_saba, Rd, Rm, Rn, width)

/**
 * Creates a ADD vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_add_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_add, Rd, Rm, Rn, width)

/**
 * Creates a CMTST vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_cmtst_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_cmtst, Rd, Rm, Rn, width)

/**
 * Creates a MLA vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register. The instruction also reads this register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_mla_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_4src(dc, OP_mla, Rd, Rd, Rm, Rn, width)

/**
 * Creates a MUL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_mul_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_mul, Rd, Rm, Rn, width)

/**
 * Creates a SMAXP vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_smaxp_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_smaxp, Rd, Rm, Rn, width)

/**
 * Creates a SMINP vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_sminp_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sminp, Rd, Rm, Rn, width)

/**
 * Creates a SQDMULH vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF() or
 *                OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_sqdmulh_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sqdmulh, Rd, Rm, Rn, width)

/**
 * Creates a ADDP vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_addp_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_addp, Rd, Rm, Rn, width)

/**
 * Creates a FMAXNM vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fmaxnm_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fmaxnm, Rd, Rm, Rn, width)

/**
 * Creates a FMLA vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register. The instruction also reads this register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fmla_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_4src(dc, OP_fmla, Rd, Rd, Rm, Rn, width)

/**
 * Creates a FADD vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fadd_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fadd, Rd, Rm, Rn, width)

/**
 * Creates a FMULX vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fmulx_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fmulx, Rd, Rm, Rn, width)

/**
 * Creates a FCMEQ vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fcmeq_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fcmeq, Rd, Rm, Rn, width)

/**
 * Creates a FMLAL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register. The instruction also reads this register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fmlal_vector(dc, Rd, Rm, Rn) \
    instr_create_1dst_3src(dc, OP_fmlal, Rd, Rd, Rm, Rn)

/**
 * Creates a FMAX vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fmax_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fmax, Rd, Rm, Rn, width)

/**
 * Creates a FRECPS vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_frecps_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_frecps, Rd, Rm, Rn, width)

/**
 * Creates a AND vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_and_vector(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_and, Rd, Rm, Rn)

/**
 * Creates a BIC vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_bic_vector(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_bic, Rd, Rm, Rn)

/**
 * Creates a FMINNM vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fminnm_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fminnm, Rd, Rm, Rn, width)

/**
 * Creates a FMLS vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register. The instruction also reads this register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fmls_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_4src(dc, OP_fmls, Rd, Rd, Rm, Rn, width)

/**
 * Creates a FSUB vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fsub_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fsub, Rd, Rm, Rn, width)

/**
 * Creates a FMLSL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register. The instruction also reads this register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fmlsl_vector(dc, Rd, Rm, Rn) \
    instr_create_1dst_3src(dc, OP_fmlsl, Rd, Rd, Rm, Rn)

/**
 * Creates a FMIN vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fmin_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fmin, Rd, Rm, Rn, width)

/**
 * Creates a FRSQRTS vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_frsqrts_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_frsqrts, Rd, Rm, Rn, width)

/**
 * Creates a ORR vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_orr_vector(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_orr, Rd, Rm, Rn)

/**
 * Creates a ORN vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_orn_vector(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_orn, Rd, Rm, Rn)

/**
 * Creates a UHADD vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_uhadd_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_uhadd, Rd, Rm, Rn, width)

/**
 * Creates a UQADD vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_uqadd_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_uqadd, Rd, Rm, Rn, width)

/**
 * Creates a URHADD vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_urhadd_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_urhadd, Rd, Rm, Rn, width)

/**
 * Creates a UHSUB vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_uhsub_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_uhsub, Rd, Rm, Rn, width)

/**
 * Creates a UQSUB vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_uqsub_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_uqsub, Rd, Rm, Rn, width)

/**
 * Creates a CMHI vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_cmhi_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_cmhi, Rd, Rm, Rn, width)

/**
 * Creates a CMHS vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_cmhs_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_cmhs, Rd, Rm, Rn, width)

/**
 * Creates a USHL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_ushl_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_ushl, Rd, Rm, Rn, width)

/**
 * Creates a UQSHL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_uqshl_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_uqshl, Rd, Rm, Rn, width)

/**
 * Creates a URSHL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_urshl_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_urshl, Rd, Rm, Rn, width)

/**
 * Creates a UQRSHL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_uqrshl_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_uqrshl, Rd, Rm, Rn, width)

/**
 * Creates a UMAX vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_umax_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_umax, Rd, Rm, Rn, width)

/**
 * Creates a UMIN vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_umin_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_umin, Rd, Rm, Rn, width)

/**
 * Creates a UABD vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_uabd_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_uabd, Rd, Rm, Rn, width)

/**
 * Creates a UABA vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_uaba_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_uaba, Rd, Rm, Rn, width)

/**
 * Creates a SUB vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_sub_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sub, Rd, Rm, Rn, width)

/**
 * Creates a CMEQ vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_cmeq_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_cmeq, Rd, Rm, Rn, width)

/**
 * Creates a MLS vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register. The instruction also reads this register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_mls_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_4src(dc, OP_mls, Rd, Rd, Rm, Rn, width)

/**
 * Creates a PMUL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use OPND_CREATE_BYTE().
 */
#define INSTR_CREATE_pmul_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_pmul, Rd, Rm, Rn, width)

/**
 * Creates a UMAXP vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_umaxp_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_umaxp, Rd, Rm, Rn, width)

/**
 * Creates a UMINP vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_uminp_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_uminp, Rd, Rm, Rn, width)

/**
 * Creates a SQRDMULH vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF() or
 *                OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_sqrdmulh_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sqrdmulh, Rd, Rm, Rn, width)

/**
 * Creates a FMAXNMP vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fmaxnmp_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fmaxnmp, Rd, Rm, Rn, width)

/**
 * Creates a FMLAL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register. The instruction also reads this register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fmlal2_vector(dc, Rd, Rm, Rn) \
    instr_create_1dst_3src(dc, OP_fmlal2, Rd, Rd, Rm, Rn)

/**
 * Creates a FADDP vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_faddp_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_faddp, Rd, Rm, Rn, width)

/**
 * Creates a FMUL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fmul_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fmul, Rd, Rm, Rn, width)

/**
 * Creates a FCMGE vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fcmge_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fcmge, Rd, Rm, Rn, width)

/**
 * Creates a FACGE vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_facge_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_facge, Rd, Rm, Rn, width)

/**
 * Creates a FMAXP vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fmaxp_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fmaxp, Rd, Rm, Rn, width)

/**
 * Creates a FDIV vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fdiv_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fdiv, Rd, Rm, Rn, width)

/**
 * Creates a EOR vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_eor_vector(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_eor, Rd, Rm, Rn)

/**
 * Creates a BSL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_bsl_vector(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_bsl, Rd, Rm, Rn)

/**
 * Creates a FMINNMP vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fminnmp_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fminnmp, Rd, Rm, Rn, width)

/**
 * Creates a FMLSL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register. The instruction also reads this register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fmlsl2_vector(dc, Rd, Rm, Rn) \
    instr_create_1dst_3src(dc, OP_fmlsl2, Rd, Rd, Rm, Rn)

/**
 * Creates a FABD vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fabd_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fabd, Rd, Rm, Rn, width)

/**
 * Creates a FCMGT vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fcmgt_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fcmgt, Rd, Rm, Rn, width)

/**
 * Creates a FACGT vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_facgt_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_facgt, Rd, Rm, Rn, width)

/**
 * Creates a FMINP vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The vector element width. Use either OPND_CREATE_HALF(),
 *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fminp_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_fminp, Rd, Rm, Rn, width)

/**
 * Creates a BIT vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_bit_vector(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_bit, Rd, Rm, Rn)

/**
 * Creates a BIF vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_bif_vector(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_bif, Rd, Rm, Rn)

/**
 * Creates an FCVTAS vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      The output register.
 * \param Rm      The input vector register.
 * \param width   Immediate int of the vector element width. Must be #OPND_CREATE_SINGLE()
 * or #OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fcvtas_vector(dc, Rd, Rm, width) \
    instr_create_1dst_2src(dc, OP_fcvtas, Rd, Rm, width)

/**
 * Creates an FCVTNS vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param width   Immediate int of the vector element width. Must be #OPND_CREATE_SINGLE()
 * or #OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fcvtns_vector(dc, Rd, Rm, width) \
    instr_create_1dst_2src(dc, OP_fcvtns, Rd, Rm, width)

/**
 * Creates an FCVTPS vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param width   Immediate int of the vector element width. Must be #OPND_CREATE_SINGLE()
 * or #OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fcvtps_vector(dc, Rd, Rm, width) \
    instr_create_1dst_2src(dc, OP_fcvtps, Rd, Rm, width)

/**
 * Creates an FCVTPU vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param width   Immediate int of the vector element width. Must be #OPND_CREATE_SINGLE()
 * or #OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fcvtpu_vector(dc, Rd, Rm, width) \
    instr_create_1dst_2src(dc, OP_fcvtpu, Rd, Rm, width)

/**
 * Creates an FCVTZS vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param width   Immediate int of the vector element width. Must be #OPND_CREATE_SINGLE()
 * or #OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fcvtzs_vector(dc, Rd, Rm, width) \
    instr_create_1dst_2src(dc, OP_fcvtzs, Rd, Rm, width)

/**
 * Creates an FCVTZU vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param width   Immediate int of the vector element width. Must be #OPND_CREATE_SINGLE()
 * or #OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_fcvtzu_vector(dc, Rd, Rm, width) \
    instr_create_1dst_2src(dc, OP_fcvtzu, Rd, Rm, width)

/**
 * Creates an FCVTZU vector floating-point to fixed-point convert instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      The output register.
 * \param Rm      The input register.
 * \param width   The vector element width. Use either OPND_CREATE_SINGLE() or
 *                OPND_CREATE_DOUBLE().
 * \param fbits   The number of bits after the binary point in the fixed-point
 *                destination element.
 */
#define INSTR_CREATE_fcvtzu_vector_fixed(dc, Rd, Rm, width, fbits) \
    instr_create_1dst_3src(dc, OP_fcvtzu, Rd, Rm, width, fbits)

/**
 * Creates an SLI shift left and insert instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      The output register.
 * \param Rn      The input register.
 * \param width   The output vector element width. Use OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 * \param shift   The number of bits to shift the result by.
 */
#define INSTR_CREATE_sli_vector(dc, Rd, Rn, width, shift) \
    instr_create_1dst_3src(dc, OP_sli, Rd, Rn, width, shift)

/**
 * Creates an UQSHRN vector unsigned saturating shift right narrow (immediate)
 * instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      The output register.
 * \param Rn      The input register.
 * \param width   The output vector element width. Use OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 * \param shift   The number of bits to shift the result by.
 */
#define INSTR_CREATE_uqshrn_vector(dc, Rd, Rn, width, shift) \
    instr_create_1dst_3src(dc, OP_uqshrn, Rd, Rn, width, shift)

/**
 * Creates a UCVTF vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param width   Immediate int of the vector element width. Must be #OPND_CREATE_SINGLE()
 * or #OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_ucvtf_vector(dc, Rd, Rm, width) \
    instr_create_1dst_2src(dc, OP_ucvtf, Rd, Rm, width)

/**
 * Creates a UCVTF vector floating-point to fixed-point convert instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      The output register.
 * \param Rm      The input register.
 * \param width   The vector element width. Must be #OPND_CREATE_SINGLE() or
 *                #OPND_CREATE_DOUBLE().
 * \param fbits   The number of bits after the binary point in the fixed-point
 *                destination element.
 */
#define INSTR_CREATE_ucvtf_vector_fixed(dc, Rd, Rm, width, fbits) \
    instr_create_1dst_3src(dc, OP_ucvtf, Rd, Rm, width, fbits)

/**
 * Creates an SCVTF vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param width   Immediate int of the vector element width. Must be #OPND_CREATE_SINGLE()
 * or #OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_scvtf_vector(dc, Rd, Rm, width) \
    instr_create_1dst_2src(dc, OP_scvtf, Rd, Rm, width)

/**
 * Creates an SCVTF vector floating-point to fixed-point convert instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      The output register.
 * \param Rm      The input register.
 * \param width   The vector element width. Must be #OPND_CREATE_SINGLE() or
 *                #OPND_CREATE_DOUBLE().
 * \param fbits   The number of bits after the binary point in the fixed-point
 *                destination element.
 */
#define INSTR_CREATE_scvtf_vector_fixed(dc, Rd, Rm, width, fbits) \
    instr_create_1dst_3src(dc, OP_scvtf, Rd, Rm, width, fbits)

/* -------- Memory Touching instructions ------------------------------- */

/**
 * Creates an LDR immediate instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rt      The output register.
 * \param Xn      The input register or stack pointer
 * \param Rn      The input memory disposition.
 * \param imm     Immediate int of the input register offset
 */
#define INSTR_CREATE_ldr_imm(dc, Rt, Xn, Rn, imm) \
    instr_create_2dst_3src(dc, OP_ldr, Rt, Xn, Rn, Xn, imm)

/**
 * Creates a STR immediate instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rt      The output memory disposition.
 * \param Xt      The input register or stack pointer.
 * \param Xn      The output register
 * \param imm     Immediate int of the output register offset
 */
#define INSTR_CREATE_str_imm(dc, Rt, Xt, Xn, imm) \
    instr_create_2dst_3src(dc, OP_str, Rt, Xn, Xt, Xn, imm)

/* -------- Floating-point data-processing (1 source) ------------------ */

/**
 * Creates an FMOV floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_fmov_scalar(dc, Rd, Rm) instr_create_1dst_1src(dc, OP_fmov, Rd, Rm)

/**
 * Creates a FABS floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_fabs_scalar(dc, Rd, Rm) instr_create_1dst_1src(dc, OP_fabs, Rd, Rm)

/**
 * Creates a FNEG floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_fneg_scalar(dc, Rd, Rm) instr_create_1dst_1src(dc, OP_fneg, Rd, Rm)

/**
 * Creates a FSQRT floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_fsqrt_scalar(dc, Rd, Rm) instr_create_1dst_1src(dc, OP_fsqrt, Rd, Rm)

/**
 * Creates an FCVT floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      Floating-point or integer output register.
 * \param Rm      Floating-point input register.
 */
#define INSTR_CREATE_fcvt_scalar(dc, Rd, Rm) instr_create_1dst_1src(dc, OP_fcvt, Rd, Rm)

/**
 * Creates an FCVTAS floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      Floating-point or integer output register.
 * \param Rm      Floating-point input register.
 */
#define INSTR_CREATE_fcvtas_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_fcvtas, Rd, Rm)

/**
 * Creates an FCVTNS floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      Floating-point or integer output register.
 * \param Rm      Floating-point input register.
 */
#define INSTR_CREATE_fcvtns_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_fcvtns, Rd, Rm)

/**
 * Creates an FCVTPS floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      Floating-point or integer output register.
 * \param Rm      Floating-point input register.
 */
#define INSTR_CREATE_fcvtps_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_fcvtps, Rd, Rm)

/**
 * Creates an FCVTPU floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      Floating-point or integer output register.
 * \param Rm      Floating-point input register.
 */
#define INSTR_CREATE_fcvtpu_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_fcvtpu, Rd, Rm)

/**
 * Creates an FCVTZS floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      Floating-point or integer output register.
 * \param Rm      Floating-point input register.
 */
#define INSTR_CREATE_fcvtzs_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_fcvtzs, Rd, Rm)

/**
 * Creates an FCVTZS scalar floating-point to fixed-point convert instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      Floating-point or integer output register.
 * \param Rm      Floating-point input register.
 * \param fbits   The number of bits after the binary point in the fixed-point
 *                destination.
 */
#define INSTR_CREATE_fcvtzs_scalar_fixed(dc, Rd, Rm, fbits) \
    instr_create_1dst_2src(dc, OP_fcvtzs, Rd, Rm, fbits)

/**
 * Creates an FCVTZU floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      Floating-point or integer output register.
 * \param Rm      Floating-point input register.
 */
#define INSTR_CREATE_fcvtzu_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_fcvtzu, Rd, Rm)

/**
 * Creates an FCVTZU scalar floating-point to fixed-point convert instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      Floating-point or integer output register.
 * \param Rm      Floating-point input register.
 * \param fbits   The number of bits after the binary point in the fixed-point
 *                destination.
 */
#define INSTR_CREATE_fcvtzu_scalar_fixed(dc, Rd, Rm, fbits) \
    instr_create_1dst_2src(dc, OP_fcvtzu, Rd, Rm, fbits)

/**
 * Creates a UCVTF floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      Floating-point output register.
 * \param Rm      Integer input register.
 */
#define INSTR_CREATE_ucvtf_scalar(dc, Rd, Rm) instr_create_1dst_1src(dc, OP_ucvtf, Rd, Rm)

/**
 * Creates a UCVTF scalar floating-point to fixed-point convert instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      Floating-point output register.
 * \param Rm      Integer input register.
 * \param fbits   The number of bits after the binary point in the fixed-point
 *                input.
 */
#define INSTR_CREATE_ucvtf_scalar_fixed(dc, Rd, Rm, fbits) \
    instr_create_1dst_2src(dc, OP_ucvtf, Rd, Rm, fbits)

/**
 * Creates an SCVTF floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      Floating-point output register.
 * \param Rm      Integer input register.
 */
#define INSTR_CREATE_scvtf_scalar(dc, Rd, Rm) instr_create_1dst_1src(dc, OP_scvtf, Rd, Rm)

/**
 * Creates an SCVTF scalar floating-point to fixed-point convert instruction.
 * \param dc      The void * dcontext used to allocate memory for the #instr_t.
 * \param Rd      Floating-point output register.
 * \param Rm      Integer input register.
 * \param fbits   The number of bits after the binary point in the fixed-point
 *                input.
 */
#define INSTR_CREATE_scvtf_scalar_fixed(dc, Rd, Rm, fbits) \
    instr_create_1dst_2src(dc, OP_scvtf, Rd, Rm, fbits)

/**
 * Creates a FRINTN floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_frintn_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_frintn, Rd, Rm)

/**
 * Creates a FRINTP floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_frintp_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_frintp, Rd, Rm)

/**
 * Creates a FRINTM floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_frintm_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_frintm, Rd, Rm)

/**
 * Creates a FRINTZ floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_frintz_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_frintz, Rd, Rm)

/**
 * Creates a FRINTA floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_frinta_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_frinta, Rd, Rm)

/**
 * Creates a FRINTX floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_frintx_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_frintx, Rd, Rm)

/**
 * Creates a FRINTI floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 */
#define INSTR_CREATE_frinti_scalar(dc, Rd, Rm) \
    instr_create_1dst_1src(dc, OP_frinti, Rd, Rm)

/**
 * Creates a LDPSW floating point instruction.
 * \param dc    The void * dcontext used to allocate memory for the instr_t.
 * \param Xt1   The first GPR output register.
 * \param Xt2   The second GPR output register.
 * \param Xn    The input Stack-pointer or GPR register.
 * \param Xr    The disposition of the input Stack-pointer or GPR register.
 * \param imm   The immediate integer offset.
 */

#define INSTR_CREATE_ldpsw(dc, Xt1, Xt2, Xn, Xr, imm) \
    instr_create_3dst_3src(dc, OP_ldpsw, Xt1, Xt2, Xn, Xr, Xn, imm)

/**
 * Creates a LDPSW floating point instruction.
 * \param dc    dc
 * \param Xt1   The first GPR output register.
 * \param Xt2   The second GPR output register.
 * \param Xn    The disposition of the input register.
 */
#define INSTR_CREATE_ldpsw_2(dc, Xt1, Xt2, Xn) \
    instr_create_2dst_1src(dc, OP_ldpsw, Xt1, Xt2, Xn)

/* -------- Floating-point data-processing (2 source) ------------------ */

/**
 * Creates a FMUL floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fmul_scalar(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_fmul, Rd, Rm, Rn)

/**
 * Creates a FDIV floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fdiv_scalar(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_fdiv, Rd, Rm, Rn)

/**
 * Creates a FADD floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fadd_scalar(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_fadd, Rd, Rm, Rn)

/**
 * Creates a FSUB floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fsub_scalar(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_fsub, Rd, Rm, Rn)

/**
 * Creates a FMAX floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fmax_scalar(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_fmax, Rd, Rm, Rn)

/**
 * Creates a FMIN floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fmin_scalar(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_fmin, Rd, Rm, Rn)

/**
 * Creates a FMAXNM floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fmaxnm_scalar(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_fmaxnm, Rd, Rm, Rn)

/**
 * Creates a FMINNM floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fminnm_scalar(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_fminnm, Rd, Rm, Rn)

/**
 * Creates a FNMUL floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 */
#define INSTR_CREATE_fnmul_scalar(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src(dc, OP_fnmul, Rd, Rm, Rn)

/* -------- Floating-point data-processing (3 source) ------------------ */

/**
 * Creates a FMADD floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param Ra      The third input register.
 */
#define INSTR_CREATE_fmadd_scalar(dc, Rd, Rm, Rn, Ra) \
    instr_create_1dst_3src(dc, OP_fmadd, Rd, Rm, Rn, Ra)

/**
 * Creates a FMSUB floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param Ra      The third input register.
 */
#define INSTR_CREATE_fmsub_scalar(dc, Rd, Rm, Rn, Ra) \
    instr_create_1dst_3src(dc, OP_fmsub, Rd, Rm, Rn, Ra)

/**
 * Creates a FNMADD floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param Ra      The third input register.
 */
#define INSTR_CREATE_fnmadd_scalar(dc, Rd, Rm, Rn, Ra) \
    instr_create_1dst_3src(dc, OP_fnmadd, Rd, Rm, Rn, Ra)

/**
 * Creates a FNMSUB floating point instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param Ra      The third input register.
 */
#define INSTR_CREATE_fnmsub_scalar(dc, Rd, Rm, Rn, Ra) \
    instr_create_1dst_3src(dc, OP_fnmsub, Rd, Rm, Rn, Ra)

/* Advanced SIMD (NEON) memory instructions */

#define INSTR_CREATE_ld2_multi(dc, Vt1, Vt2, Xn, index) \
    instr_create_2dst_2src(dc, OP_ld2, Vt1, Vt2, Xn, index)

/**
 * Creates an Advanced SIMD (NEON) LD2 instruction to load multiple 2-element
 * structures to two vector registers with post-indexing, e.g. LD2 {V0.4H, V1.4H}, [X0],
 * #32. \param dc      The void * dcontext used to allocate memory for the instr_t. \param
 * Vt1     The destination vector register operand. \param Vt2     The second destination
 * vector register operand. \param Xn      The stack-pointer or GPR to load into Vt1 and
 * Vt2. \param disp    The disposition of Xn. \param index   The element index of the
 * vectors. \param offset  The post-index offset.
 */
#define INSTR_CREATE_ld2_multi_2(dc, Vt1, Vt2, Xn, disp, index, offset) \
    instr_create_3dst_4src(dc, OP_ld2, Vt1, Vt2, Xn, disp, index, Xn, offset)

/**
 * Creates an Advanced SIMD (NEON) LD2 instruction to load a 2-element
 * structure to the index of two vector registers, e.g. LD2 {V0.4H, V1.4H}[5], [X0].
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Vt1     The first destination vector register operand.
 * \param Vt2     The second destination vector register operand.
 * \param Xn      The stack-pointer or GPR to load into Vt1 and Vt2.
 * \param index   The vector element index.
 */
#define INSTR_CREATE_ld2(dc, Vt1, Vt2, Xn, index) \
    instr_create_2dst_2src(dc, OP_ld2, Vt1, Vt2, Xn, index)

/**
 * Creates an Advanced SIMD (NEON) LD2 instruction to load a 2-element
 * structure to the index of two vector registers with post-indexing,
 * e.g. LD2 {V0.4H, V1.4H}[5], [X0], X1.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Vt1     The first destination vector register operand.
 * \param Vt2     The second destination vector register operand.
 * \param Xn      The stack-pointer or register to load into Vt and Vt2.
 * \param Xnd     The disposition of Xn.
 * \param index   The index of the destination vectors.
 * \param offset  The post-index offset.
 */
#define INSTR_CREATE_ld2_2(dc, Vt1, Vt2, Xn, Xnd, index, offset) \
    instr_create_3dst_6src(dc, OP_ld2, Vt1, Vt2, Xn, Vt1, Vt2, Xnd, index, Xn, offset)

/**
 * Creates an Advanced SIMD (NEON) LD2R instruction to load and replicate a
 * single 2-element structure to all lanes of two vector registers,
 * e.g. LD2R {V0.4H, V1.4H}, [X0].
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Vt1     The first destination vector register operand.
 * \param Vt2     The second destination vector register operand.
 * \param Xn      The stack-pointer or GPR to load into Vt1 and Vt2.
 */
#define INSTR_CREATE_ld2r(dc, Vt1, Vt2, Xn) \
    instr_create_2dst_1src(dc, OP_ld2r, Vt1, Vt2, Xn)

/**
 * Creates an Advanced SIMD (NEON) LD2R instruction to load and replicate a
 * single 2-element structure to all lanes of two vector registers with post-indexing
 * , e.g. LD2R {V0.4H, V1.4H}, [X0], X1.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Vt1     The destination vector register operand.
 * \param Vt2     The second destination vector register operand.
 * \param Xn      The stack-pointer or GPR to load into Vt and Vt2.
 * \param Xnd     Disposition of Xn.
 * \param Xm      The post-index offset.
 */
#define INSTR_CREATE_ld2r_2(dc, Vt1, Vt2, Xn, Xnd, Xm) \
    instr_create_3dst_3src(dc, OP_ld2r, Vt1, Vt2, Xn, Xnd, Xn, Xm)

/**
 * Creates an Advanced SIMD (NEON) LD3 instruction to load multiple 3-element
 * structures from memory to three vector register,
 * e.g. LD3 {V0.4H, V1.4H, V2.4H}, [X0].
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Vt1     The first destination vector register operand.
 * \param Vt2     The second destination vector register operand.
 * \param Vt3     The third destination vector register operand.
 * \param Xn      The stack-pointer or GPR to load into Vt1, Vt2 and Vt3.
 * \param index   The index of the vectors.
 */
#define INSTR_CREATE_ld3_multi(dc, Vt1, Vt2, Vt3, Xn, index) \
    instr_create_3dst_2src(dc, OP_ld3, Vt1, Vt2, Vt3, Xn, index)

/**
 * Creates an Advanced SIMD (NEON) LD3 instruction to load multiple 3-element
 * structures from memory to the index of three vector registers with
 * post-index offset, e.g. LD3 {V0.4H, V1.4H, V2.4H}, [X0], X1.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Vt1     The first destination vector register operand.
 * \param Vt2     The second destination vector register operand.
 * \param Vt3     The third destination vector register operand.
 * \param Xn      The stack-pointer or GPR to load into Vt1, Vt2 and Vt3.
 * \param Xnd     The disposition of Xn.
 * \param index   The index of the vectors.
 * \param Xm      The post-index offset.
 */
#define INSTR_CREATE_ld3_multi_2(dc, Vt1, Vt2, Vt3, Xn, Xnd, index, Xm) \
    instr_create_4dst_4src(dc, OP_ld3, Vt1, Vt2, Vt3, Xn, Xnd, index, Xn, Xm)

/**
 * Creates an Advanced SIMD (NEON) LD3 instruction to load a single 3-element
 * structure to the index of three vector registers, e.g. LD3 {V0.4H, V1.4H, V2.4H}[15],
 * [X0]. \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Vt1     The first destination vector register operand.
 * \param Vt2     The second destination vector register operand.
 * \param Vt3     The third destination vector register operand.
 * \param Xn      The GPR to load into Vt1, Vt2 and Vt3.
 * \param index   The index of the vectors.
 */
#define INSTR_CREATE_ld3(dc, Vt1, Vt2, Vt3, Xn, index) \
    instr_create_3dst_2src(dc, OP_ld3, Vt1, Vt2, Vt3, Xn, index)

/**
 * Creates an Advanced SIMD (NEON) LD3 instruction to load a single 3-element
 * structure to the index of three vector registers with post-index offset,
 * e.g. LD3 {V0.4H, V1.4H, V2.4H}[15], [X0], X1.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Vt1     The first destination vector register operand.
 * \param Vt2     The second destination vector register operand.
 * \param Vt3     The third destination vector register operand.
 * \param Xn      The register to load into Vt, Vt2 and Vt3.
 * \param Xnd     The disposition of Xn.
 * \param index   The index of the vectors.
 * \param offset  The immediate or GPR post-index offset.
 */
#define INSTR_CREATE_ld3_2(dc, Vt1, Vt2, Vt3, Xn, Xnd, index, offset)                    \
    instr_create_4dst_7src(dc, OP_ld3, Vt1, Vt2, Vt3, Xn, Vt1, Vt2, Vt3, Xnd, index, Xn, \
                           offset)

/**
 * Creates an Advanced SIMD (NEON) LD3 instruction to load and replicate a single
 * 3-element structure to the index of three vector registers, e.g. LD3 {V0.4H, V1.4H,
 * V2.4H}[15], [X0]. \param dc      The void * dcontext used to allocate memory for the
 * instr_t. \param Vt1     The first destination vector register operand. \param Vt2 The
 * second destination vector register operand. \param Vt3     The third destination vector
 * register operand. \param Xn      The stack-pointer or GPR to load into Vt1, Vt2 and
 * Vt3.
 */
#define INSTR_CREATE_ld3r(dc, Vt1, Vt2, Vt3, Xn) \
    instr_create_3dst_1src(dc, OP_ld3r, Vt1, Vt2, Vt3, Xn)

/**
 * Creates an Advanced SIMD (NEON) LD3 instruction to load and replicate a single
 * 3-element structure to the index of three vector registers with post-index offset, e.g.
 * LD3 {V0.4H, V1.4H, V2.4H}[15], [X0], X1. \param dc      The void * dcontext used to
 * allocate memory for the instr_t. \param Vt1     The first destination vector register
 * operand. \param Vt2     The second destination vector register operand. \param Vt3 The
 * third destination vector register operand. \param Xn      The stack-pointer or GPR to
 * load into Vt1, Vt2 and Vt3. \param Xnd     The disposition of Xn. \param offset  The
 * immediate or GPR post-index offset
 */
#define INSTR_CREATE_ld3r_2(dc, Vt1, Vt2, Vt3, Xn, Xnd, offset) \
    instr_create_4dst_3src(dc, OP_ld3r, Vt1, Vt2, Vt3, Xn, Xnd, Xn, offset)

/**
 * Creates an Advanced SIMD (NEON) LD4 instruction to load single or multiple 4-element
 * structures to four vector registers, e.g. LD4 {V0.4H, V1.4H, V2.4H, V3.4H}, [X0].
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Vt1     The first destination vector register operand.
 * \param Vt2     The second destination vector register operand.
 * \param Vt3     The third destination vector register operand.
 * \param Vt4     The fourth destination vector register operand.
 * \param Xn      The stack-pointer or register to load into the destination vectors.
 * \param index   The immediate or GPR post-index offset.
 */
#define INSTR_CREATE_ld4_multi(dc, Vt1, Vt2, Vt3, Vt4, Xn, index) \
    instr_create_4dst_2src(dc, OP_ld4, Vt1, Vt2, Vt3, Vt4, Xn, index)

/**
 * Creates an Advanced SIMD (NEON) LD4 instruction to load multiple 4-element
 * structures to four vector registers with post-index,
 * e.g. LD4 {V0.4H, V1.4H, V2.4H, V3.4H}, [X0], X1.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Vt1     The first destination vector register operand.
 * \param Vt2     The second destination vector register operand.
 * \param Vt3     The third destination vector register operand.
 * \param Vt4     The fourth destination vector register operand.
 * \param Xn      The stack-pointer or GPR to load into the destination vectors
 * \param Xnd     The disposition of Xn
 * \param index   The index of the vectors.
 * \param offset  The post-index offset.
 */
#define INSTR_CREATE_ld4_multi_2(dc, Vt1, Vt2, Vt3, Vt4, Xn, Xnd, index, offset) \
    instr_create_5dst_4src(dc, OP_ld4, Vt1, Vt2, Vt3, Vt4, Xn, Xnd, index, Xn, offset)

/**
 * Creates an Advanced SIMD (NEON) LD4 instruction to load single or multiple 4-element
 * structures to four vector registers, e.g. LD4 {V0.4H, V1.4H, V2.4H, V3.4H}, [X0].
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Vt1     The first destination vector register operand.
 * \param Vt2     The second destination vector register operand.
 * \param Vt3     The third destination vector register operand.
 * \param Vt4     The fourth destination vector register operand.
 * \param Xn      The stack-pointer or register to load into the destination vectors.
 * \param index   The immediate or GPR post-index offset.
 */
#define INSTR_CREATE_ld4(dc, Vt1, Vt2, Vt3, Vt4, Xn, index) \
    instr_create_4dst_2src(dc, OP_ld4, Vt1, Vt2, Vt3, Vt4, Xn, index)

/**
 * Creates an Advanced SIMD (NEON) LD4 instruction to load a single 4-element
 * structures to four vector registers with post-index,
 * e.g. LD4 {V0.4H, V1.4H, V2.4H, V3.4H}, [X0], X1.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Vt1     The first destination vector register operand.
 * \param Vt2     The second destination vector register operand.
 * \param Vt3     The third destination vector register operand.
 * \param Vt4     The fourth destination vector register operand.
 * \param Xn      The stack-pointer or GPR to load into the destination vectors.
 * \param Xnd     The disposition of Xn.
 * \param index   The index of the vectors
 * \param offset  The post-index offset.
 */
#define INSTR_CREATE_ld4_2(dc, Vt1, Vt2, Vt3, Vt4, Xn, Xnd, index, offset)              \
    instr_create_5dst_8src(dc, OP_ld4, Vt1, Vt2, Vt3, Vt4, Xn, Vt1, Vt2, Vt3, Vt4, Xnd, \
                           index, Xn, offset)

/**
 * Creates an Advanced SIMD (NEON) LD4R instruction to load
 * and replicate a single 4-element structure to four vector registers,
 * e.g. LD4R {V0.4H, V1.4H, V2.4H, V3.4H}, [X0].
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Vt1     The first destination vector register operand.
 * \param Vt2     The second destination vector register operand.
 * \param Vt3     The third destination vector register operand.
 * \param Vt4     The fourth destination vector register operand.
 * \param Xn      The stack-pointer or GPR to load into the destination vectors.
 */
#define INSTR_CREATE_ld4r(dc, Vt1, Vt2, Vt3, Vt4, Xn) \
    instr_create_4dst_1src(dc, OP_ld4r, Vt1, Vt2, Vt3, Vt4, Xn)

/**
 * Creates an Advanced SIMD (NEON) LD4R instruction to load and
 * replicate a single 4-element structure to four vector registers with post-indexing,
 * e.g. LD4 {V0.4H, V1.4H, V2.4H, V3.4H}, [X0], X1.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Vt1     The first destination vector register operand.
 * \param Vt2     The second destination vector register operand.
 * \param Vt3     The third destination vector register operand.
 * \param Vt4     The fourth destination vector register operand.
 * \param Xn      The stack-pointer or register to load into the destination vectors.
 * \param Xnd     The disposition of Xn.
 * \param offset  The post-index offset.
 */
#define INSTR_CREATE_ld4r_2(dc, Vt1, Vt2, Vt3, Vt4, Xn, Xnd, offset) \
    instr_create_5dst_3src(dc, OP_ld4r, Vt1, Vt2, Vt3, Vt4, Xn, Xnd, Xn, offset)

/**
 * Creates an Advanced SIMD (NEON) LD1 instruction to load multiple
 * single element structures to one vector register, e.g. LD1 {V0.4H},[X0].
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param q       The destination vector register operand.
 * \param r       The source memory operand.
 * \param s       The size of the vector element.
 */
#define INSTR_CREATE_ld1_multi_1(dc, q, r, s) instr_create_1dst_2src(dc, OP_ld1, q, r, s)

/**
 * Creates an Advanced SIMD (NEON) ST1 instruction to store multiple
 * single element structures from one vector register, e.g. ST1 {V1.2S},[X1].
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param r       The destination memory operand.
 * \param q       The source vector register operand.
 * \param s       The size of the vector element.
 */
#define INSTR_CREATE_st1_multi_1(dc, r, q, s) instr_create_1dst_2src(dc, OP_st1, r, q, s)

/* -------- SVE bitwise logical operations (predicated) ---------------- */

/**
 * Creates an ORR scalable vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Zd      The output SVE vector register.
 * \param Pg      Predicate register for predicated instruction, P0-P7.
 * \param Zd_     The first input SVE vector register. Must match Zd.
 * \param Zm      The second input SVE vector register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_orr_sve_pred(dc, Zd, Pg, Zd_, Zm, width) \
    instr_create_1dst_4src(dc, OP_orr, Zd, Pg, Zd_, Zm, width)

/**
 * Creates an EOR scalable vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Zd      The output SVE vector register.
 * \param Pg      Predicate register for predicated instruction, P0-P7.
 * \param Zd_     The first input SVE vector register. Must match Zd.
 * \param Zm      The second input SVE vector register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_eor_sve_pred(dc, Zd, Pg, Zd_, Zm, width) \
    instr_create_1dst_4src(dc, OP_eor, Zd, Pg, Zd_, Zm, width)

/**
 * Creates an AND scalable vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Zd      The output SVE vector register.
 * \param Pg      Predicate register for predicated instruction, P0-P7.
 * \param Zd_     The first input SVE vector register. Must match Zd.
 * \param Zm      The second input SVE vector register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_and_sve_pred(dc, Zd, Pg, Zd_, Zm, width) \
    instr_create_1dst_4src(dc, OP_and, Zd, Pg, Zd_, Zm, width)

/**
 * Creates a BIC scalable vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Zd      The output SVE vector register.
 * \param Pg      Predicate register for predicated instruction, P0-P7.
 * \param Zd_     The first input SVE vector register. Must match Zd.
 * \param Zm      The second input SVE vector register.
 * \param width   The vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF(), OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_bic_sve_pred(dc, Zd, Pg, Zd_, Zm, width) \
    instr_create_1dst_4src(dc, OP_bic, Zd, Pg, Zd_, Zm, width)

/* -------- Advanced SIMD three different ------------------------------ */

/**
 * Creates a SADDL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_saddl_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_saddl, Rd, Rm, Rn, width)

/**
 * Creates a SADDL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_saddl2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_saddl2, Rd, Rm, Rn, width)

/**
 * Creates a SADDW vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_saddw_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_saddw, Rd, Rm, Rn, width)

/**
 * Creates a SADDW2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_saddw2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_saddw2, Rd, Rm, Rn, width)

/**
 * Creates a SSUBL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_ssubl_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_ssubl, Rd, Rm, Rn, width)

/**
 * Creates a SSUBL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_ssubl2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_ssubl2, Rd, Rm, Rn, width)

/**
 * Creates a SSUBW vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_ssubw_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_ssubw, Rd, Rm, Rn, width)

/**
 * Creates a SSUBW2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_ssubw2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_ssubw2, Rd, Rm, Rn, width)

/**
 * Creates a ADDHN vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_addhn_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_addhn, Rd, Rm, Rn, width)

/**
 * Creates a ADDHN2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_addhn2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_addhn2, Rd, Rm, Rn, width)

/**
 * Creates a SABAL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_sabal_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sabal, Rd, Rm, Rn, width)

/**
 * Creates a SABAL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_sabal2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sabal2, Rd, Rm, Rn, width)

/**
 * Creates a SUBHN vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_subhn_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_subhn, Rd, Rm, Rn, width)

/**
 * Creates a SUBHN2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_subhn2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_subhn2, Rd, Rm, Rn, width)

/**
 * Creates a SABDL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_sabdl_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sabdl, Rd, Rm, Rn, width)

/**
 * Creates a SABDL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_sabdl2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sabdl2, Rd, Rm, Rn, width)

/**
 * Creates a SMLAL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_smlal_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_smlal, Rd, Rm, Rn, width)

/**
 * Creates a SMLAL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_smlal2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_smlal2, Rd, Rm, Rn, width)

/**
 * Creates a SQDMLAL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_HALF() or
 *                OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_sqdmlal_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sqdmlal, Rd, Rm, Rn, width)

/**
 * Creates a SQDMLAL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_HALF() or
 *                OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_sqdmlal2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sqdmlal2, Rd, Rm, Rn, width)

/**
 * Creates a SMLSL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_smlsl_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_smlsl, Rd, Rm, Rn, width)

/**
 * Creates a SMLSL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_smlsl2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_smlsl2, Rd, Rm, Rn, width)

/**
 * Creates a SQDMLSL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_HALF() or
 *                OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_sqdmlsl_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sqdmlsl, Rd, Rm, Rn, width)

/**
 * Creates a SQDMLSL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_HALF() or
 *                OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_sqdmlsl2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sqdmlsl2, Rd, Rm, Rn, width)

/**
 * Creates a SMULL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_smull_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_smull, Rd, Rm, Rn, width)

/**
 * Creates a SMULL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_smull2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_smull2, Rd, Rm, Rn, width)

/**
 * Creates a SQDMULL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_HALF() or
 *                OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_sqdmull_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sqdmull, Rd, Rm, Rn, width)

/**
 * Creates a SQDMULL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_HALF() or
 *                OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_sqdmull2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_sqdmull2, Rd, Rm, Rn, width)

/**
 * Creates a PMULL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(), or
 *                OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_pmull_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_pmull, Rd, Rm, Rn, width)

/**
 * Creates a PMULL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(), or
 *                OPND_CREATE_DOUBLE().
 */
#define INSTR_CREATE_pmull2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_pmull2, Rd, Rm, Rn, width)

/**
 * Creates a UADDL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_uaddl_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_uaddl, Rd, Rm, Rn, width)

/**
 * Creates a UADDL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_uaddl2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_uaddl2, Rd, Rm, Rn, width)

/**
 * Creates a UADDW vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_uaddw_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_uaddw, Rd, Rm, Rn, width)

/**
 * Creates a UADDW2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_uaddw2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_uaddw2, Rd, Rm, Rn, width)

/**
 * Creates a USUBL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_usubl_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_usubl, Rd, Rm, Rn, width)

/**
 * Creates a USUBL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_usubl2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_usubl2, Rd, Rm, Rn, width)

/**
 * Creates a USUBW vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_usubw_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_usubw, Rd, Rm, Rn, width)

/**
 * Creates a USUBW2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_usubw2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_usubw2, Rd, Rm, Rn, width)

/**
 * Creates a RADDHN vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_raddhn_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_raddhn, Rd, Rm, Rn, width)

/**
 * Creates a RADDHN2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_raddhn2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_raddhn2, Rd, Rm, Rn, width)

/**
 * Creates a UABAL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_uabal_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_uabal, Rd, Rm, Rn, width)

/**
 * Creates a UABAL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_uabal2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_uabal2, Rd, Rm, Rn, width)

/**
 * Creates a RSUBHN vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_rsubhn_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_rsubhn, Rd, Rm, Rn, width)

/**
 * Creates a RSUBHN2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_rsubhn2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_rsubhn2, Rd, Rm, Rn, width)

/**
 * Creates a UABDL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_uabdl_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_uabdl, Rd, Rm, Rn, width)

/**
 * Creates a UABDL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_uabdl2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_uabdl2, Rd, Rm, Rn, width)

/**
 * Creates a UMLAL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_umlal_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_umlal, Rd, Rm, Rn, width)

/**
 * Creates a UMLAL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_umlal2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_umlal2, Rd, Rm, Rn, width)

/**
 * Creates a UMLSL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_umlsl_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_umlsl, Rd, Rm, Rn, width)

/**
 * Creates a UMLSL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_umlsl2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_umlsl2, Rd, Rm, Rn, width)

/**
 * Creates a UMULL vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_umull_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_umull, Rd, Rm, Rn, width)

/**
 * Creates a UMULL2 vector instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output register.
 * \param Rm      The first input register.
 * \param Rn      The second input register.
 * \param width   The input vector element width. Use either OPND_CREATE_BYTE(),
 *                OPND_CREATE_HALF() or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_umull2_vector(dc, Rd, Rm, Rn, width) \
    instr_create_1dst_3src(dc, OP_umull2, Rd, Rm, Rn, width)

/**
 * Creates an FMOV immediate to vector floating point move instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output vector register.
 * \param f       The source immediate floating point opnd.
 * \param width   The output vector element width. Use either OPND_CREATE_HALF()
 *                or OPND_CREATE_SINGLE().
 */
#define INSTR_CREATE_fmov_vector_imm(dc, Rd, f, width) \
    instr_create_1dst_2src(dc, OP_fmov, Rd, f, width)

/**
 * Creates an FMOV immediate to scalar floating point move instruction.
 * \param dc      The void * dcontext used to allocate memory for the instr_t.
 * \param Rd      The output scalar register.
 * \param f       The source immediate floating point opnd.
 */
#define INSTR_CREATE_fmov_scalar_imm(dc, Rd, f) instr_create_1dst_1src(dc, OP_fmov, Rd, f)

#endif /* DR_IR_MACROS_AARCH64_H */
