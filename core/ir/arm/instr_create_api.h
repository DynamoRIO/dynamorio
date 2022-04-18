/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
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

#ifndef _DR_IR_MACROS_ARM_H_
#define _DR_IR_MACROS_ARM_H_ 1

/**
 * @file dr_ir_macros_arm.h
 * @brief ARM-specific instruction creation convenience macros.
 */

/**
 * Create an absolute address operand encoded as pc-relative.
 * Encoding will fail if addr is out of the maximum signed displacement
 * reach for the architecture and ISA mode.
 */
#define OPND_CREATE_ABSMEM(addr, size) opnd_create_rel_addr(addr, size)

/** Create a negated register operand. */
#define OPND_CREATE_NEG_REG(reg) opnd_create_reg_ex(reg, 0, DR_OPND_NEGATED)

/**
 * Create an immediate integer operand.  For ARM, the size of an immediate
 * is ignored when encoding, so there is no need to specify the final size.
 */
#define OPND_CREATE_INT(val) OPND_CREATE_INTPTR(val)

/** The immediate opnd_t for use with OP_msr to write the nzcvq status flags. */
#define OPND_CREATE_INT_MSR_NZCVQ() opnd_create_immed_int(EFLAGS_MSR_NZCVQ, OPSZ_4b)

/** The immediate opnd_t for use with OP_msr to write the apsr_g status flags. */
#define OPND_CREATE_INT_MSR_G() opnd_create_immed_int(EFLAGS_MSR_G, OPSZ_4b)

/** The immediate opnd_t for use with OP_msr to write the apsr_nzcvqg status flags. */
#define OPND_CREATE_INT_MSR_NZCVQG() opnd_create_immed_int(EFLAGS_MSR_NZCVQG, OPSZ_4b)

/** A memory opnd_t that auto-sizes at encode time to match a register list. */
#define OPND_CREATE_MEMLIST(base) \
    opnd_create_base_disp(base, DR_REG_NULL, 0, 0, OPSZ_VAR_REGLIST)

/** Immediate values for INSTR_CREATE_dmb(). */
enum {
    DR_DMB_OSHLD = 1, /**< DMB Outer Shareable - Loads. */
    DR_DMB_OSHST = 2, /**< DMB Outer Shareable - Stores. */
    DR_DMB_OSH = 3,   /**< DMB Outer Shareable - Loads and Stores. */
    DR_DMB_NSHLD = 5, /**< DMB Non Shareable - Loads. */
    DR_DMB_NSHST = 6, /**< DMB Non Shareable - Stores. */
    DR_DMB_NSH = 7,   /**< DMB Non Shareable - Loads and Stores. */
    DR_DMB_ISHLD = 9, /**< DMB Inner Shareable - Loads. */
    DR_DMB_ISHST = 10 /**< DMB Inner Shareable - Stores. */,
    DR_DMB_ISH = 11, /**< DMB Inner Shareable - Loads and Stores. */
    DR_DMB_LD = 13,  /**< DMB Full System - Loads. */
    DR_DMB_ST = 14,  /**< DMB Full System - Stores. */
    DR_DMB_SY = 15,  /**< DMB Full System - Loads and Stores. */
};

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

/****************************************************************************
 * Platform-independent XINST_CREATE_* macros
 */
/** @name Platform-independent macros */
/** @{ */ /* doxygen start group */

/**
 * This platform-independent macro creates an instr_t for a debug trap
 * instruction, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define XINST_CREATE_debug_instr(dc) INSTR_CREATE_bkpt((dc), OPND_CREATE_INT8(1))

/**
 * This platform-independent macro creates an instr_t for a 4-byte
 * or 8-byte (x64 only) memory load instruction.
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
#define XINST_CREATE_load_1byte_zext4(dc, r, m) INSTR_CREATE_ldrb((dc), (r), (m))

/**
 * This platform-independent macro creates an instr_t for a 1-byte
 * memory load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param m   The source memory opnd.
 */
#define XINST_CREATE_load_1byte(dc, r, m) INSTR_CREATE_ldrb((dc), (r), (m))

/**
 * This platform-independent macro creates an instr_t for a 2-byte
 * memory load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param m   The source memory opnd.
 */
#define XINST_CREATE_load_2bytes(dc, r, m) INSTR_CREATE_ldrh((dc), (r), (m))

/**
 * This platform-independent macro creates an instr_t for a 4-byte
 * or 8-byte (x64 only) memory store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r   The source register opnd.
 */
#define XINST_CREATE_store(dc, m, r) INSTR_CREATE_str((dc), (m), (r))

/**
 * This platform-independent macro creates an instr_t for a 1-byte
 * memory store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r   The source register opnd.
 */
#define XINST_CREATE_store_1byte(dc, m, r) INSTR_CREATE_strb((dc), (m), (r))

/**
 * This platform-independent macro creates an instr_t for a 2-byte
 * memory store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r   The source register opnd.
 */
#define XINST_CREATE_store_2bytes(dc, m, r) INSTR_CREATE_strh((dc), (m), (r))

/**
 * This AArchXX-platform-independent macro creates an instr_t for a 2-register
 * memory store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r1  The first register opnd.
 * \param r2  The second register opnd.
 */
#define XINST_CREATE_store_pair(dc, m, r1, r2) INSTR_CREATE_strd((dc), (m), (r1), (r2))

/**
 * This AArchXX-platform-independent macro creates an instr_t for a 2-register
 * memory store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r1  The first register opnd.
 * \param r2  The second register opnd.
 * \param m   The source memory opnd.
 */
#define XINST_CREATE_load_pair(dc, r1, r2, m) INSTR_CREATE_ldrd((dc), (r1), (r2), (m))

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
 *
 * \note Loading to 128-bit registers is not supported on 32-bit ARM.
 */
#define XINST_CREATE_load_simd(dc, r, m) INSTR_CREATE_vldr((dc), (r), (m))

/**
 * This platform-independent macro creates an instr_t for a multimedia
 * register store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r   The source register opnd.
 *
 * \note Storing from 128-bit registers is not supported on 32-bit ARM.
 */
#define XINST_CREATE_store_simd(dc, m, r) INSTR_CREATE_vstr((dc), (m), (r))

/**
 * This platform-independent macro creates an instr_t for an indirect
 * jump through memory instruction.  For AArch32, the memory address
 * must be aligned to 4.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The memory opnd holding the target.
 */
#define XINST_CREATE_jump_mem(dc, m) \
    INSTR_CREATE_ldr((dc), opnd_create_reg(DR_REG_PC), (m))

/**
 * This platform-independent macro creates an instr_t for an indirect
 * jump instruction through a register.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The register opnd holding the target.
 */
#define XINST_CREATE_jump_reg(dc, r) INSTR_CREATE_bx((dc), (r))

/**
 * This platform-independent macro creates an instr_t for an immediate
 * integer load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param i   The source immediate integer opnd.
 */
#define XINST_CREATE_load_int(dc, r, i)                                            \
    (opnd_get_immed_int(i) < 0                                                     \
         ? INSTR_CREATE_mvn((dc), (r), OPND_CREATE_INTPTR(-opnd_get_immed_int(i))) \
         : INSTR_CREATE_movw((dc), (r), (i)))

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
#define XINST_CREATE_jump(dc, t) INSTR_CREATE_b((dc), (t))

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
#define XINST_CREATE_call(dc, t) INSTR_CREATE_bl((dc), (t))

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
#define XINST_CREATE_jump_short(dc, t)                                         \
    (dr_get_isa_mode(dc) == DR_ISA_ARM_THUMB ? INSTR_CREATE_b_short((dc), (t)) \
                                             : INSTR_CREATE_b((dc), (t)))

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
    (INSTR_PRED(INSTR_CREATE_b((dc), (t)), (pred)))

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
 * \param s1  The opnd_t explicit first source operand for the instruction.  This
 * must be a register.
 * \param s2  The opnd_t explicit source operand for the instruction.  This
 * can be either a register or a 32-bit immediate integer on x86.
 */
#define XINST_CREATE_add_2src(dc, d, s1, s2) INSTR_CREATE_add((dc), (d), (s1), (s2))

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
    INSTR_CREATE_add_shimm((dc), (d), (s1), (s2_toshift),         \
                           OPND_CREATE_INT8(DR_SHIFT_LSL),        \
                           OPND_CREATE_INT8(shift_amount))

/**
 * This platform-independent macro creates an instr_t for an addition
 * instruction that does affect the status flags.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_add_s(dc, d, s) INSTR_CREATE_adds((dc), (d), (d), (s))

/**
 * This platform-independent macro creates an instr_t for a subtraction
 * instruction that does not affect the status flags.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_sub(dc, d, s) INSTR_CREATE_sub((dc), (d), (d), (s))

/**
 * This platform-independent macro creates an instr_t for a subtraction
 * instruction that does affect the status flags.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_sub_s(dc, d, s) INSTR_CREATE_subs((dc), (d), (d), (s))

/**
 * This platform-independent macro creates an instr_t for a bitwise and
 * instruction that does affect the status flags.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_and_s(dc, d, s) INSTR_CREATE_ands((dc), (d), (d), (s))

/**
 * This platform-independent macro creates an instr_t for a logical right shift
 * instruction that does affect the status flags.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_slr_s(dc, d, s) INSTR_CREATE_lsrs((dc), (d), (d), (s))

/**
 * This platform-independent macro creates an instr_t for a comparison
 * instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param s1  The opnd_t explicit source operand for the instruction.
 * \param s2  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_cmp(dc, s1, s2) INSTR_CREATE_cmp((dc), (s1), (s2))

/**
 * This platform-independent macro creates an instr_t for a software
 * interrupt instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param i   The source integer constant opnd_t operand.
 */
#define XINST_CREATE_interrupt(dc, i) INSTR_CREATE_svc(dc, (i))

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
#define XINST_CREATE_call_reg(dc, r) INSTR_CREATE_blx_ind(dc, r)
/** @} */ /* end doxygen group */

/****************************************************************************
 * Manually-added ARM-specific INSTR_CREATE_* macros
 */

/**
 * This macro creates an instr_t for a pop instruction into a single
 * register, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 */
#define INSTR_CREATE_pop(dc, Rd)                                          \
    INSTR_CREATE_ldr_wbimm((dc), (Rd), OPND_CREATE_MEMPTR(DR_REG_XSP, 0), \
                           OPND_CREATE_INT16(sizeof(void *)))

/**
 * This macro creates an instr_t for a pop instruction into a list of
 * registers, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param list_len The number of registers in the register list.
 * \param ... The register list as separate opnd_t arguments.
 */
#define INSTR_CREATE_pop_list(dc, list_len, ...) \
    INSTR_CREATE_ldm_wb((dc), OPND_CREATE_MEMLIST(DR_REG_XSP), list_len, __VA_ARGS__)

/**
 * This macro creates an instr_t for a push instruction of a single
 * register, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rt The destination register opnd_t operand.
 */
#define INSTR_CREATE_push(dc, Rt)                                                       \
    INSTR_CREATE_str_wbimm((dc), OPND_CREATE_MEMPTR(DR_REG_XSP, -sizeof(void *)), (Rt), \
                           OPND_CREATE_INT16(-sizeof(void *)))

/**
 * This macro creates an instr_t for a push instruction of a list of
 * registers, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param list_len The number of registers in the register list.
 * \param ... The register list as separate opnd_t arguments.
 */
#define INSTR_CREATE_push_list(dc, list_len, ...) \
    INSTR_CREATE_stmdb_wb((dc), OPND_CREATE_MEMLIST(DR_REG_XSP), list_len, __VA_ARGS__)

/**
 * This macro creates an instr_t for a negate instruction,
 automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rn The source register opnd_t operand.
 */
#define INSTR_CREATE_neg(dc, Rd, Rn) \
    INSTR_CREATE_rsb((dc), (Rd), (Rn), OPND_CREATE_INT16(0))

/**
 * This macro creats an instr_t for a jump instruction with a short
 * reach, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param pc The program counter constant opnd_t operand.
 */
#define INSTR_CREATE_b_short(dc, pc) instr_create_0dst_1src((dc), OP_b_short, (pc))

/* XXX i#1551: add macros for the other opcode aliases */

/****************************************************************************
 * Automatically-generated ARM-specific INSTR_CREATE_* macros.
 * These were generated from tools/arm_macros_gen.pl.
 * DO NOT EDIT THESE MANUALLY so we can continue using the script for easy
 * updates on table changes.
 */

/** @name Signature: () */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define INSTR_CREATE_clrex(dc) instr_create_0dst_0src((dc), OP_clrex)
#define INSTR_CREATE_dcps1(dc) instr_create_0dst_0src((dc), OP_dcps1)
#define INSTR_CREATE_dcps2(dc) instr_create_0dst_0src((dc), OP_dcps2)
#define INSTR_CREATE_dcps3(dc) instr_create_0dst_0src((dc), OP_dcps3)
#define INSTR_CREATE_enterx(dc) instr_create_0dst_0src((dc), OP_enterx)
#define INSTR_CREATE_eret(dc) \
    instr_create_0dst_1src((dc), OP_eret, opnd_create_reg(DR_REG_LR))
#define INSTR_CREATE_leavex(dc) instr_create_0dst_0src((dc), OP_leavex)
#define INSTR_CREATE_nop(dc) instr_create_0dst_0src((dc), OP_nop)
#define INSTR_CREATE_sev(dc) instr_create_0dst_0src((dc), OP_sev)
#define INSTR_CREATE_sevl(dc) instr_create_0dst_0src((dc), OP_sevl)
#define INSTR_CREATE_wfe(dc) instr_create_0dst_0src((dc), OP_wfe)
#define INSTR_CREATE_wfi(dc) instr_create_0dst_0src((dc), OP_wfi)
#define INSTR_CREATE_yield(dc) instr_create_0dst_0src((dc), OP_yield)
/** @} */ /* end doxygen group */

/** @name Signature: (Rd) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 */
#define INSTR_CREATE_vmrs(dc, Rd) \
    instr_create_1dst_1src((dc), OP_vmrs, (Rd), opnd_create_reg(DR_REG_FPSCR))
/** @} */ /* end doxygen group */

/** @name Signature: (Rm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rm The source register opnd_t operand.
 */
#define INSTR_CREATE_blx_ind(dc, Rm) \
    instr_create_1dst_1src((dc), OP_blx_ind, opnd_create_reg(DR_REG_LR), (Rm))
#define INSTR_CREATE_bx(dc, Rm) instr_create_0dst_1src((dc), OP_bx, (Rm))
#define INSTR_CREATE_bxj(dc, Rm) instr_create_0dst_1src((dc), OP_bxj, (Rm))
/** @} */ /* end doxygen group */

/** @name Signature: (Rt) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rt The source register opnd_t operand.
 */
#define INSTR_CREATE_vmsr(dc, Rt) \
    instr_create_1dst_1src((dc), OP_vmsr, opnd_create_reg(DR_REG_FPSCR), (Rt))
/** @} */ /* end doxygen group */

/** @name Signature: (pc) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param pc The program counter constant opnd_t operand.
 */
#define INSTR_CREATE_b(dc, pc) instr_create_0dst_1src((dc), OP_b, (pc))
#define INSTR_CREATE_b_short(dc, pc) instr_create_0dst_1src((dc), OP_b_short, (pc))
#define INSTR_CREATE_bl(dc, pc) \
    instr_create_1dst_1src((dc), OP_bl, opnd_create_reg(DR_REG_LR), (pc))
#define INSTR_CREATE_blx(dc, pc) \
    instr_create_1dst_1src((dc), OP_blx, opnd_create_reg(DR_REG_LR), (pc))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rm The source register opnd_t operand.
 */
#define INSTR_CREATE_clz(dc, Rd, Rm) instr_create_1dst_1src((dc), OP_clz, (Rd), (Rm))
#define INSTR_CREATE_rbit(dc, Rd, Rm) instr_create_1dst_1src((dc), OP_rbit, (Rd), (Rm))
#define INSTR_CREATE_rev(dc, Rd, Rm) instr_create_1dst_1src((dc), OP_rev, (Rd), (Rm))
#define INSTR_CREATE_rev16(dc, Rd, Rm) instr_create_1dst_1src((dc), OP_rev16, (Rd), (Rm))
#define INSTR_CREATE_revsh(dc, Rd, Rm) instr_create_1dst_1src((dc), OP_revsh, (Rd), (Rm))
#define INSTR_CREATE_rrx(dc, Rd, Rm) instr_create_1dst_1src((dc), OP_rrx, (Rd), (Rm))
#define INSTR_CREATE_rrxs(dc, Rd, Rm) instr_create_1dst_1src((dc), OP_rrxs, (Rd), (Rm))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rn) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rn The source register opnd_t operand.
 */
#define INSTR_CREATE_sxtb(dc, Rd, Rn) instr_create_1dst_1src((dc), OP_sxtb, (Rd), (Rn))
#define INSTR_CREATE_sxth(dc, Rd, Rn) instr_create_1dst_1src((dc), OP_sxth, (Rd), (Rn))
#define INSTR_CREATE_uxtb(dc, Rd, Rn) instr_create_1dst_1src((dc), OP_uxtb, (Rd), (Rn))
#define INSTR_CREATE_uxth(dc, Rd, Rn) instr_create_1dst_1src((dc), OP_uxth, (Rd), (Rn))
/** @} */ /* end doxygen group */

/** @name Signature: (pc, Rn) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param pc The program counter constant opnd_t operand.
 * \param Rn The source register opnd_t operand.
 */
#define INSTR_CREATE_cbnz(dc, pc, Rn) instr_create_0dst_2src((dc), OP_cbnz, (pc), (Rn))
#define INSTR_CREATE_cbz(dc, pc, Rn) instr_create_0dst_2src((dc), OP_cbz, (pc), (Rn))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, statreg) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param statreg The status register (usually DR_REG_CPSR) opnd_t operand.
 */
#define INSTR_CREATE_mrs(dc, Rd, statreg) \
    instr_create_1dst_1src((dc), OP_mrs, (Rd), (statreg))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rm, Rn) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rm The source register opnd_t operand.
 * \param Rn The source register opnd_t operand.
 */
#define INSTR_CREATE_qsub(dc, Rd, Rm, Rn) \
    instr_create_1dst_2src((dc), OP_qsub, (Rd), (Rm), (Rn))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rn, Rm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rn The source register opnd_t operand.
 * \param Rm The second source register opnd_t operand.
 */
#define INSTR_CREATE_crc32b(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_crc32b, (Rd), (Rn), (Rm))
#define INSTR_CREATE_crc32cb(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_crc32cb, (Rd), (Rn), (Rm))
#define INSTR_CREATE_crc32ch(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_crc32ch, (Rd), (Rn), (Rm))
#define INSTR_CREATE_crc32cw(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_crc32cw, (Rd), (Rn), (Rm))
#define INSTR_CREATE_crc32h(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_crc32h, (Rd), (Rn), (Rm))
#define INSTR_CREATE_crc32w(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_crc32w, (Rd), (Rn), (Rm))
#define INSTR_CREATE_mul(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_mul, (Rd), (Rn), (Rm))
#define INSTR_CREATE_muls(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_muls, (Rd), (Rn), (Rm))
#define INSTR_CREATE_qadd(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_qadd, (Rd), (Rn), (Rm))
#define INSTR_CREATE_qadd16(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_qadd16, (Rd), (Rn), (Rm))
#define INSTR_CREATE_qadd8(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_qadd8, (Rd), (Rn), (Rm))
#define INSTR_CREATE_qasx(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_qasx, (Rd), (Rn), (Rm))
#define INSTR_CREATE_qdadd(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_qdadd, (Rd), (Rn), (Rm))
#define INSTR_CREATE_qdsub(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_qdsub, (Rd), (Rn), (Rm))
#define INSTR_CREATE_qsax(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_qsax, (Rd), (Rn), (Rm))
#define INSTR_CREATE_qsub16(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_qsub16, (Rd), (Rn), (Rm))
#define INSTR_CREATE_qsub8(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_qsub8, (Rd), (Rn), (Rm))
#define INSTR_CREATE_sadd16(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_sadd16, (Rd), (Rn), (Rm))
#define INSTR_CREATE_sadd8(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_sadd8, (Rd), (Rn), (Rm))
#define INSTR_CREATE_sasx(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_sasx, (Rd), (Rn), (Rm))
#define INSTR_CREATE_sdiv(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_sdiv, (Rd), (Rn), (Rm))
#define INSTR_CREATE_sel(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_sel, (Rd), (Rn), (Rm))
#define INSTR_CREATE_shadd16(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_shadd16, (Rd), (Rn), (Rm))
#define INSTR_CREATE_shadd8(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_shadd8, (Rd), (Rn), (Rm))
#define INSTR_CREATE_shasx(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_shasx, (Rd), (Rn), (Rm))
#define INSTR_CREATE_shsax(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_shsax, (Rd), (Rn), (Rm))
#define INSTR_CREATE_shsub16(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_shsub16, (Rd), (Rn), (Rm))
#define INSTR_CREATE_shsub8(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_shsub8, (Rd), (Rn), (Rm))
#define INSTR_CREATE_smmul(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_smmul, (Rd), (Rn), (Rm))
#define INSTR_CREATE_smmulr(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_smmulr, (Rd), (Rn), (Rm))
#define INSTR_CREATE_smuad(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_smuad, (Rd), (Rn), (Rm))
#define INSTR_CREATE_smuadx(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_smuadx, (Rd), (Rn), (Rm))
#define INSTR_CREATE_smulbb(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_smulbb, (Rd), (Rn), (Rm))
#define INSTR_CREATE_smulbt(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_smulbt, (Rd), (Rn), (Rm))
#define INSTR_CREATE_smultb(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_smultb, (Rd), (Rn), (Rm))
#define INSTR_CREATE_smultt(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_smultt, (Rd), (Rn), (Rm))
#define INSTR_CREATE_smulwb(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_smulwb, (Rd), (Rn), (Rm))
#define INSTR_CREATE_smulwt(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_smulwt, (Rd), (Rn), (Rm))
#define INSTR_CREATE_smusd(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_smusd, (Rd), (Rn), (Rm))
#define INSTR_CREATE_smusdx(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_smusdx, (Rd), (Rn), (Rm))
#define INSTR_CREATE_ssax(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_ssax, (Rd), (Rn), (Rm))
#define INSTR_CREATE_ssub16(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_ssub16, (Rd), (Rn), (Rm))
#define INSTR_CREATE_ssub8(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_ssub8, (Rd), (Rn), (Rm))
#define INSTR_CREATE_uadd16(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_uadd16, (Rd), (Rn), (Rm))
#define INSTR_CREATE_uadd8(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_uadd8, (Rd), (Rn), (Rm))
#define INSTR_CREATE_uasx(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_uasx, (Rd), (Rn), (Rm))
#define INSTR_CREATE_udiv(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_udiv, (Rd), (Rn), (Rm))
#define INSTR_CREATE_uhadd16(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_uhadd16, (Rd), (Rn), (Rm))
#define INSTR_CREATE_uhadd8(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_uhadd8, (Rd), (Rn), (Rm))
#define INSTR_CREATE_uhasx(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_uhasx, (Rd), (Rn), (Rm))
#define INSTR_CREATE_uhsax(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_uhsax, (Rd), (Rn), (Rm))
#define INSTR_CREATE_uhsub16(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_uhsub16, (Rd), (Rn), (Rm))
#define INSTR_CREATE_uhsub8(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_uhsub8, (Rd), (Rn), (Rm))
#define INSTR_CREATE_uqadd16(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_uqadd16, (Rd), (Rn), (Rm))
#define INSTR_CREATE_uqadd8(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_uqadd8, (Rd), (Rn), (Rm))
#define INSTR_CREATE_uqasx(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_uqasx, (Rd), (Rn), (Rm))
#define INSTR_CREATE_uqsax(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_uqsax, (Rd), (Rn), (Rm))
#define INSTR_CREATE_uqsub16(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_uqsub16, (Rd), (Rn), (Rm))
#define INSTR_CREATE_uqsub8(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_uqsub8, (Rd), (Rn), (Rm))
#define INSTR_CREATE_usad8(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_usad8, (Rd), (Rn), (Rm))
#define INSTR_CREATE_usax(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_usax, (Rd), (Rn), (Rm))
#define INSTR_CREATE_usub16(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_usub16, (Rd), (Rn), (Rm))
#define INSTR_CREATE_usub8(dc, Rd, Rn, Rm) \
    instr_create_1dst_2src((dc), OP_usub8, (Rd), (Rn), (Rm))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rn, Rm, Ra) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rn The source register opnd_t operand.
 * \param Rm The second source register opnd_t operand.
 * \param Ra The third source register opnd_t operand.
 */
#define INSTR_CREATE_mla(dc, Rd, Rn, Rm, Ra) \
    instr_create_1dst_3src((dc), OP_mla, (Rd), (Rn), (Rm), (Ra))
#define INSTR_CREATE_mlas(dc, Rd, Rn, Rm, Ra) \
    instr_create_1dst_3src((dc), OP_mlas, (Rd), (Rn), (Rm), (Ra))
#define INSTR_CREATE_mls(dc, Rd, Rn, Rm, Ra) \
    instr_create_1dst_3src((dc), OP_mls, (Rd), (Rn), (Rm), (Ra))
#define INSTR_CREATE_smlabb(dc, Rd, Rn, Rm, Ra) \
    instr_create_1dst_3src((dc), OP_smlabb, (Rd), (Rn), (Rm), (Ra))
#define INSTR_CREATE_smlabt(dc, Rd, Rn, Rm, Ra) \
    instr_create_1dst_3src((dc), OP_smlabt, (Rd), (Rn), (Rm), (Ra))
#define INSTR_CREATE_smlad(dc, Rd, Rn, Rm, Ra) \
    instr_create_1dst_3src((dc), OP_smlad, (Rd), (Rn), (Rm), (Ra))
#define INSTR_CREATE_smladx(dc, Rd, Rn, Rm, Ra) \
    instr_create_1dst_3src((dc), OP_smladx, (Rd), (Rn), (Rm), (Ra))
#define INSTR_CREATE_smlatb(dc, Rd, Rn, Rm, Ra) \
    instr_create_1dst_3src((dc), OP_smlatb, (Rd), (Rn), (Rm), (Ra))
#define INSTR_CREATE_smlatt(dc, Rd, Rn, Rm, Ra) \
    instr_create_1dst_3src((dc), OP_smlatt, (Rd), (Rn), (Rm), (Ra))
#define INSTR_CREATE_smlawb(dc, Rd, Rn, Rm, Ra) \
    instr_create_1dst_3src((dc), OP_smlawb, (Rd), (Rn), (Rm), (Ra))
#define INSTR_CREATE_smlawt(dc, Rd, Rn, Rm, Ra) \
    instr_create_1dst_3src((dc), OP_smlawt, (Rd), (Rn), (Rm), (Ra))
#define INSTR_CREATE_smlsd(dc, Rd, Rn, Rm, Ra) \
    instr_create_1dst_3src((dc), OP_smlsd, (Rd), (Rn), (Rm), (Ra))
#define INSTR_CREATE_smlsdx(dc, Rd, Rn, Rm, Ra) \
    instr_create_1dst_3src((dc), OP_smlsdx, (Rd), (Rn), (Rm), (Ra))
#define INSTR_CREATE_smmla(dc, Rd, Rn, Rm, Ra) \
    instr_create_1dst_3src((dc), OP_smmla, (Rd), (Rn), (Rm), (Ra))
#define INSTR_CREATE_smmlar(dc, Rd, Rn, Rm, Ra) \
    instr_create_1dst_3src((dc), OP_smmlar, (Rd), (Rn), (Rm), (Ra))
#define INSTR_CREATE_smmls(dc, Rd, Rn, Rm, Ra) \
    instr_create_1dst_3src((dc), OP_smmls, (Rd), (Rn), (Rm), (Ra))
#define INSTR_CREATE_smmlsr(dc, Rd, Rn, Rm, Ra) \
    instr_create_1dst_3src((dc), OP_smmlsr, (Rd), (Rn), (Rm), (Ra))
#define INSTR_CREATE_usada8(dc, Rd, Rn, Rm, Ra) \
    instr_create_1dst_3src((dc), OP_usada8, (Rd), (Rn), (Rm), (Ra))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rd2, Rn, Rm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rd2 The second destination register opnd_t operand.
 * \param Rn The source register opnd_t operand.
 * \param Rm The second source register opnd_t operand.
 */
#define INSTR_CREATE_smlal(dc, Rd, Rd2, Rn, Rm) \
    instr_create_2dst_4src((dc), OP_smlal, (Rd), (Rd2), (Rd), (Rd2), (Rn), (Rm))
#define INSTR_CREATE_smlalbb(dc, Rd, Rd2, Rn, Rm) \
    instr_create_2dst_4src((dc), OP_smlalbb, (Rd), (Rd2), (Rd), (Rd2), (Rn), (Rm))
#define INSTR_CREATE_smlalbt(dc, Rd, Rd2, Rn, Rm) \
    instr_create_2dst_4src((dc), OP_smlalbt, (Rd), (Rd2), (Rd), (Rd2), (Rn), (Rm))
#define INSTR_CREATE_smlald(dc, Rd, Rd2, Rn, Rm) \
    instr_create_2dst_4src((dc), OP_smlald, (Rd), (Rd2), (Rd), (Rd2), (Rn), (Rm))
#define INSTR_CREATE_smlaldx(dc, Rd, Rd2, Rn, Rm) \
    instr_create_2dst_4src((dc), OP_smlaldx, (Rd), (Rd2), (Rd), (Rd2), (Rn), (Rm))
#define INSTR_CREATE_smlals(dc, Rd, Rd2, Rn, Rm) \
    instr_create_2dst_4src((dc), OP_smlals, (Rd), (Rd2), (Rd), (Rd2), (Rn), (Rm))
#define INSTR_CREATE_smlaltb(dc, Rd, Rd2, Rn, Rm) \
    instr_create_2dst_4src((dc), OP_smlaltb, (Rd), (Rd2), (Rd), (Rd2), (Rn), (Rm))
#define INSTR_CREATE_smlaltt(dc, Rd, Rd2, Rn, Rm) \
    instr_create_2dst_4src((dc), OP_smlaltt, (Rd), (Rd2), (Rd), (Rd2), (Rn), (Rm))
#define INSTR_CREATE_smlsld(dc, Rd, Rd2, Rn, Rm) \
    instr_create_2dst_4src((dc), OP_smlsld, (Rd), (Rd2), (Rd), (Rd2), (Rn), (Rm))
#define INSTR_CREATE_smlsldx(dc, Rd, Rd2, Rn, Rm) \
    instr_create_2dst_4src((dc), OP_smlsldx, (Rd), (Rd2), (Rd), (Rd2), (Rn), (Rm))
#define INSTR_CREATE_smull(dc, Rd, Rd2, Rn, Rm) \
    instr_create_2dst_2src((dc), OP_smull, (Rd), (Rd2), (Rn), (Rm))
#define INSTR_CREATE_smulls(dc, Rd, Rd2, Rn, Rm) \
    instr_create_2dst_2src((dc), OP_smulls, (Rd), (Rd2), (Rn), (Rm))
#define INSTR_CREATE_umaal(dc, Rd, Rd2, Rn, Rm) \
    instr_create_2dst_4src((dc), OP_umaal, (Rd), (Rd2), (Rd), (Rd2), (Rn), (Rm))
#define INSTR_CREATE_umlal(dc, Rd, Rd2, Rn, Rm) \
    instr_create_2dst_4src((dc), OP_umlal, (Rd), (Rd2), (Rd), (Rd2), (Rn), (Rm))
#define INSTR_CREATE_umlals(dc, Rd, Rd2, Rn, Rm) \
    instr_create_2dst_4src((dc), OP_umlals, (Rd), (Rd2), (Rd), (Rd2), (Rn), (Rm))
#define INSTR_CREATE_umull(dc, Rd, Rd2, Rn, Rm) \
    instr_create_2dst_2src((dc), OP_umull, (Rd), (Rd2), (Rn), (Rm))
#define INSTR_CREATE_umulls(dc, Rd, Rd2, Rn, Rm) \
    instr_create_2dst_2src((dc), OP_umulls, (Rd), (Rd2), (Rn), (Rm))
/** @} */ /* end doxygen group */

/** @name Signature: (imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_bkpt(dc, imm) instr_create_0dst_1src((dc), OP_bkpt, (imm))
#define INSTR_CREATE_cps(dc, imm) instr_create_0dst_1src((dc), OP_cps, (imm))
#define INSTR_CREATE_cpsid(dc, imm) instr_create_0dst_1src((dc), OP_cpsid, (imm))
#define INSTR_CREATE_cpsie(dc, imm) instr_create_0dst_1src((dc), OP_cpsie, (imm))
#define INSTR_CREATE_dbg(dc, imm) instr_create_0dst_1src((dc), OP_dbg, (imm))
#define INSTR_CREATE_dmb(dc, imm) instr_create_0dst_1src((dc), OP_dmb, (imm))
#define INSTR_CREATE_dsb(dc, imm) instr_create_0dst_1src((dc), OP_dsb, (imm))
#define INSTR_CREATE_eret_imm(dc, imm) \
    instr_create_0dst_2src((dc), OP_eret, opnd_create_reg(DR_REG_LR), (imm))
#define dr_ir_macros_arm_hlt(dc, imm) instr_create_0dst_1src((dc), OP_hlt, (imm))
#define dr_ir_macros_arm_hvc(dc, imm) instr_create_0dst_1src((dc), OP_hvc, (imm))
#define INSTR_CREATE_isb(dc, imm) instr_create_0dst_1src((dc), OP_isb, (imm))
#define INSTR_CREATE_setend(dc, imm) instr_create_0dst_1src((dc), OP_setend, (imm))
#define INSTR_CREATE_smc(dc, imm) instr_create_0dst_1src((dc), OP_smc, (imm))
#define INSTR_CREATE_svc(dc, imm) instr_create_0dst_1src((dc), OP_svc, (imm))
#define INSTR_CREATE_udf(dc, imm) instr_create_0dst_1src((dc), OP_udf, (imm))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_movt(dc, Rd, imm) instr_create_1dst_1src((dc), OP_movt, (Rd), (imm))
#define INSTR_CREATE_movw(dc, Rd, imm) instr_create_1dst_1src((dc), OP_movw, (Rd), (imm))
#define INSTR_CREATE_mrs_priv(dc, Rd, imm) \
    instr_create_1dst_1src((dc), OP_mrs_priv, (Rd), (imm))
#define INSTR_CREATE_vmrs_imm(dc, Rd, imm) \
    instr_create_1dst_1src((dc), OP_vmrs, (Rd), (imm))
/** @} */ /* end doxygen group */

/** @name Signature: (Rt, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rt The source register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_vmsr_imm(dc, Rt, imm) \
    instr_create_0dst_2src((dc), OP_vmsr, (Rt), (imm))
/** @} */ /* end doxygen group */

/** @name Signature: (imm, Rm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param imm The integer constant opnd_t operand.
 * \param Rm The source register opnd_t operand.
 */
#define INSTR_CREATE_msr_priv(dc, imm, Rm) \
    instr_create_0dst_2src((dc), OP_msr_priv, (imm), (Rm))
/** @} */ /* end doxygen group */

/** @name Signature: (imm, imm2) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param imm The integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 */
#define INSTR_CREATE_cpsid_noflags(dc, imm, imm2) \
    instr_create_0dst_2src((dc), OP_cpsid, (imm), (imm2))
#define INSTR_CREATE_cpsie_noflags(dc, imm, imm2) \
    instr_create_0dst_2src((dc), OP_cpsie, (imm), (imm2))
#define INSTR_CREATE_it(dc, imm, imm2) instr_create_0dst_2src((dc), OP_it, (imm), (imm2))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rm_or_imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rm_or_imm The source register, or integer constant, opnd_t operand.
 */
#define INSTR_CREATE_mov(dc, Rd, Rm_or_imm) \
    instr_create_1dst_1src((dc), OP_mov, (Rd), (Rm_or_imm))
#define INSTR_CREATE_movs(dc, Rd, Rm_or_imm) \
    instr_create_1dst_1src((dc), OP_movs, (Rd), (Rm_or_imm))
#define INSTR_CREATE_mvn(dc, Rd, Rm_or_imm)                                             \
    (opnd_is_reg(Rm_or_imm)                                                             \
         ? INSTR_CREATE_mvn_shimm((dc), (Rd), (Rm_or_imm),                              \
                                  OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_1src((dc), OP_mvn, (Rd), (Rm_or_imm)))
#define INSTR_CREATE_mvns(dc, Rd, Rm_or_imm)                                             \
    (opnd_is_reg(Rm_or_imm)                                                              \
         ? INSTR_CREATE_mvns_shimm((dc), (Rd), (Rm_or_imm),                              \
                                   OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_1src((dc), OP_mvns, (Rd), (Rm_or_imm)))
/** @} */ /* end doxygen group */

/** @name Signature: (Rn, Rm_or_imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rn The source register opnd_t operand.
 * \param Rm_or_imm The second source register, or integer constant, opnd_t operand.
 */
#define INSTR_CREATE_cmn(dc, Rn, Rm_or_imm)                                             \
    (opnd_is_reg(Rm_or_imm)                                                             \
         ? INSTR_CREATE_cmn_shimm((dc), (Rn), (Rm_or_imm),                              \
                                  OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_0dst_2src((dc), OP_cmn, (Rn), (Rm_or_imm)))
#define INSTR_CREATE_cmp(dc, Rn, Rm_or_imm)                                             \
    (opnd_is_reg(Rm_or_imm)                                                             \
         ? INSTR_CREATE_cmp_shimm((dc), (Rn), (Rm_or_imm),                              \
                                  OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_0dst_2src((dc), OP_cmp, (Rn), (Rm_or_imm)))
#define INSTR_CREATE_teq(dc, Rn, Rm_or_imm)                                             \
    (opnd_is_reg(Rm_or_imm)                                                             \
         ? INSTR_CREATE_teq_shimm((dc), (Rn), (Rm_or_imm),                              \
                                  OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_0dst_2src((dc), OP_teq, (Rn), (Rm_or_imm)))
#define INSTR_CREATE_tst(dc, Rn, Rm_or_imm)                                             \
    (opnd_is_reg(Rm_or_imm)                                                             \
         ? INSTR_CREATE_tst_shimm((dc), (Rn), (Rm_or_imm),                              \
                                  OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_0dst_2src((dc), OP_tst, (Rn), (Rm_or_imm)))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rm, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rm The source register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_sxtb16(dc, Rd, Rm, imm) \
    instr_create_1dst_2src((dc), OP_sxtb16, (Rd), (Rm), (imm))
#define INSTR_CREATE_sxtb_imm(dc, Rd, Rm, imm) \
    instr_create_1dst_2src((dc), OP_sxtb, (Rd), (Rm), (imm))
#define INSTR_CREATE_sxth_imm(dc, Rd, Rm, imm) \
    instr_create_1dst_2src((dc), OP_sxth, (Rd), (Rm), (imm))
#define INSTR_CREATE_uxtb16(dc, Rd, Rm, imm) \
    instr_create_1dst_2src((dc), OP_uxtb16, (Rd), (Rm), (imm))
#define INSTR_CREATE_uxtb_imm(dc, Rd, Rm, imm) \
    instr_create_1dst_2src((dc), OP_uxtb, (Rd), (Rm), (imm))
#define INSTR_CREATE_uxth_imm(dc, Rd, Rm, imm) \
    instr_create_1dst_2src((dc), OP_uxth, (Rd), (Rm), (imm))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rn, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rn The source register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_addw(dc, Rd, Rn, imm) \
    instr_create_1dst_2src((dc), OP_addw, (Rd), (Rn), (imm))
#define INSTR_CREATE_subw(dc, Rd, Rn, imm) \
    instr_create_1dst_2src((dc), OP_subw, (Rd), (Rn), (imm))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, imm, Rm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param Rm The source register opnd_t operand.
 */
#define INSTR_CREATE_ssat16(dc, Rd, imm, Rm) \
    instr_create_1dst_2src((dc), OP_ssat16, (Rd), (imm), (Rm))
#define INSTR_CREATE_usat16(dc, Rd, imm, Rm) \
    instr_create_1dst_2src((dc), OP_usat16, (Rd), (imm), (Rm))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, imm, imm2) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 */
#define INSTR_CREATE_bfc(dc, Rd, imm, imm2) \
    instr_create_1dst_3src((dc), OP_bfc, (Rd), (imm), (imm2), (Rd))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rn, Rm_or_imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rn The source register opnd_t operand.
 * \param Rm_or_imm The second source register, or integer constant, opnd_t operand.
 */
#define INSTR_CREATE_adc(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                             \
         ? INSTR_CREATE_adc_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                  OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_adc, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_adcs(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                              \
         ? INSTR_CREATE_adcs_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                   OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_adcs, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_add(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                             \
         ? INSTR_CREATE_add_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                  OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_add, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_adds(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                              \
         ? INSTR_CREATE_adds_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                   OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_adds, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_and(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                             \
         ? INSTR_CREATE_and_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                  OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_and, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_ands(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                              \
         ? INSTR_CREATE_ands_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                   OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_ands, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_asr(dc, Rd, Rn, Rm_or_imm) \
    instr_create_1dst_2src((dc), OP_asr, (Rd), (Rn), (Rm_or_imm))
#define INSTR_CREATE_asrs(dc, Rd, Rn, Rm_or_imm) \
    instr_create_1dst_2src((dc), OP_asrs, (Rd), (Rn), (Rm_or_imm))
#define INSTR_CREATE_bic(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                             \
         ? INSTR_CREATE_bic_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                  OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_bic, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_bics(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                              \
         ? INSTR_CREATE_bics_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                   OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_bics, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_eor(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                             \
         ? INSTR_CREATE_eor_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                  OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_eor, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_eors(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                              \
         ? INSTR_CREATE_eors_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                   OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_eors, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_lsl(dc, Rd, Rn, Rm_or_imm) \
    instr_create_1dst_2src((dc), OP_lsl, (Rd), (Rn), (Rm_or_imm))
#define INSTR_CREATE_lsls(dc, Rd, Rn, Rm_or_imm) \
    instr_create_1dst_2src((dc), OP_lsls, (Rd), (Rn), (Rm_or_imm))
#define INSTR_CREATE_lsr(dc, Rd, Rn, Rm_or_imm) \
    instr_create_1dst_2src((dc), OP_lsr, (Rd), (Rn), (Rm_or_imm))
#define INSTR_CREATE_lsrs(dc, Rd, Rn, Rm_or_imm) \
    instr_create_1dst_2src((dc), OP_lsrs, (Rd), (Rn), (Rm_or_imm))
#define INSTR_CREATE_orn(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                             \
         ? INSTR_CREATE_orn_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                  OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_orn, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_orns(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                              \
         ? INSTR_CREATE_orns_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                   OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_orns, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_orr(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                             \
         ? INSTR_CREATE_orr_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                  OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_orr, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_orrs(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                              \
         ? INSTR_CREATE_orrs_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                   OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_orrs, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_ror(dc, Rd, Rn, Rm_or_imm) \
    instr_create_1dst_2src((dc), OP_ror, (Rd), (Rn), (Rm_or_imm))
#define INSTR_CREATE_rors(dc, Rd, Rn, Rm_or_imm) \
    instr_create_1dst_2src((dc), OP_rors, (Rd), (Rn), (Rm_or_imm))
#define INSTR_CREATE_rsb(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                             \
         ? INSTR_CREATE_rsb_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                  OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_rsb, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_rsbs(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                              \
         ? INSTR_CREATE_rsbs_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                   OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_rsbs, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_rsc(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                             \
         ? INSTR_CREATE_rsc_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                  OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_rsc, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_rscs(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                              \
         ? INSTR_CREATE_rscs_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                   OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_rscs, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_sbc(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                             \
         ? INSTR_CREATE_sbc_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                  OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_sbc, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_sbcs(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                              \
         ? INSTR_CREATE_sbcs_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                   OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_sbcs, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_sub(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                             \
         ? INSTR_CREATE_sub_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                  OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_sub, (Rd), (Rn), (Rm_or_imm)))
#define INSTR_CREATE_subs(dc, Rd, Rn, Rm_or_imm)                                         \
    (opnd_is_reg(Rm_or_imm)                                                              \
         ? INSTR_CREATE_subs_shimm((dc), (Rd), (Rn), (Rm_or_imm),                        \
                                   OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) \
         : instr_create_1dst_2src((dc), OP_subs, (Rd), (Rn), (Rm_or_imm)))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, statreg, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param statreg The status register (usually DR_REG_CPSR) opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_mrs_priv_spsr(dc, Rd, statreg, imm) \
    instr_create_1dst_2src((dc), OP_mrs_priv, (Rd), (statreg), (imm))
/** @} */ /* end doxygen group */

/** @name Signature: (statreg, imm, Rm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param statreg The status register (usually DR_REG_CPSR) opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param Rm The source register opnd_t operand.
 */
#define INSTR_CREATE_msr_priv_spsr(dc, statreg, imm, Rm) \
    instr_create_1dst_2src((dc), OP_msr_priv, (statreg), (imm), (Rm))
/** @} */ /* end doxygen group */

/** @name Signature: (statreg, imm, imm2) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param statreg The status register (usually DR_REG_CPSR) opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 */
#define INSTR_CREATE_msr_imm(dc, statreg, imm, imm2) \
    instr_create_1dst_2src((dc), OP_msr, (statreg), (imm), (imm2))
/** @} */ /* end doxygen group */

/** @name Signature: (statreg, imm_msr, Rm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param statreg The status register (usually DR_REG_CPSR) opnd_t operand.
 * \param imm_msr The integer constant (typically from OPND_CREATE_INT_MSR*) opnd_t
 * operand. \param Rm The source register opnd_t operand.
 */
#define INSTR_CREATE_msr(dc, statreg, imm_msr, Rm) \
    instr_create_1dst_2src((dc), OP_msr, (statreg), (imm_msr), (Rm))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rn, Rm, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rn The source register opnd_t operand.
 * \param Rm The second source register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_sxtab(dc, Rd, Rn, Rm, imm) \
    instr_create_1dst_3src((dc), OP_sxtab, (Rd), (Rn), (Rm), (imm))
#define INSTR_CREATE_sxtab16(dc, Rd, Rn, Rm, imm) \
    instr_create_1dst_3src((dc), OP_sxtab16, (Rd), (Rn), (Rm), (imm))
#define INSTR_CREATE_sxtah(dc, Rd, Rn, Rm, imm) \
    instr_create_1dst_3src((dc), OP_sxtah, (Rd), (Rn), (Rm), (imm))
#define INSTR_CREATE_uxtab(dc, Rd, Rn, Rm, imm) \
    instr_create_1dst_3src((dc), OP_uxtab, (Rd), (Rn), (Rm), (imm))
#define INSTR_CREATE_uxtab16(dc, Rd, Rn, Rm, imm) \
    instr_create_1dst_3src((dc), OP_uxtab16, (Rd), (Rn), (Rm), (imm))
#define INSTR_CREATE_uxtah(dc, Rd, Rn, Rm, imm) \
    instr_create_1dst_3src((dc), OP_uxtah, (Rd), (Rn), (Rm), (imm))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rm, imm, imm2) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rm The source register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 */
#define INSTR_CREATE_bfi(dc, Rd, Rm, imm, imm2) \
    instr_create_1dst_4src((dc), OP_bfi, (Rd), (Rm), (imm), (imm2), (Rd))
#define INSTR_CREATE_sbfx(dc, Rd, Rm, imm, imm2) \
    instr_create_1dst_3src((dc), OP_sbfx, (Rd), (Rm), (imm), (imm2))
#define INSTR_CREATE_ubfx(dc, Rd, Rm, imm, imm2) \
    instr_create_1dst_3src((dc), OP_ubfx, (Rd), (Rm), (imm), (imm2))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rm, shift, Rs) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rm The source register opnd_t operand.
 * \param shift The dr_shift_type_t integer constant opnd_t operand.
 * \param Rs The third source register opnd_t operand.
 */
#define INSTR_CREATE_mvn_shreg(dc, Rd, Rm, shift, Rs)                                \
    instr_create_1dst_3src((dc), OP_mvn, (Rd),                                       \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_mvns_shreg(dc, Rd, Rm, shift, Rs)                               \
    instr_create_1dst_3src((dc), OP_mvns, (Rd),                                      \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
/** @} */ /* end doxygen group */

/** @name Signature: (Rn, Rm, shift, Rs) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rn The source register opnd_t operand.
 * \param Rm The second source register opnd_t operand.
 * \param shift The dr_shift_type_t integer constant opnd_t operand.
 * \param Rs The third source register opnd_t operand.
 */
#define INSTR_CREATE_cmn_shreg(dc, Rn, Rm, shift, Rs)                                \
    instr_create_0dst_4src((dc), OP_cmn, (Rn),                                       \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_cmp_shreg(dc, Rn, Rm, shift, Rs)                                \
    instr_create_0dst_4src((dc), OP_cmp, (Rn),                                       \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_teq_shreg(dc, Rn, Rm, shift, Rs)                                \
    instr_create_0dst_4src((dc), OP_teq, (Rn),                                       \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_tst_shreg(dc, Rn, Rm, shift, Rs)                                \
    instr_create_0dst_4src((dc), OP_tst, (Rn),                                       \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rn, Rm, shift, Rs) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rn The source register opnd_t operand.
 * \param Rm The second source register opnd_t operand.
 * \param shift The dr_shift_type_t integer constant opnd_t operand.
 * \param Rs The third source register opnd_t operand.
 */
#define INSTR_CREATE_adc_shreg(dc, Rd, Rn, Rm, shift, Rs)                            \
    instr_create_1dst_4src((dc), OP_adc, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_adcs_shreg(dc, Rd, Rn, Rm, shift, Rs)                           \
    instr_create_1dst_4src((dc), OP_adcs, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_add_shreg(dc, Rd, Rn, Rm, shift, Rs)                            \
    instr_create_1dst_4src((dc), OP_add, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_adds_shreg(dc, Rd, Rn, Rm, shift, Rs)                           \
    instr_create_1dst_4src((dc), OP_adds, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_and_shreg(dc, Rd, Rn, Rm, shift, Rs)                            \
    instr_create_1dst_4src((dc), OP_and, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_ands_shreg(dc, Rd, Rn, Rm, shift, Rs)                           \
    instr_create_1dst_4src((dc), OP_ands, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_bic_shreg(dc, Rd, Rn, Rm, shift, Rs)                            \
    instr_create_1dst_4src((dc), OP_bic, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_bics_shreg(dc, Rd, Rn, Rm, shift, Rs)                           \
    instr_create_1dst_4src((dc), OP_bics, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_eor_shreg(dc, Rd, Rn, Rm, shift, Rs)                            \
    instr_create_1dst_4src((dc), OP_eor, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_eors_shreg(dc, Rd, Rn, Rm, shift, Rs)                           \
    instr_create_1dst_4src((dc), OP_eors, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_orr_shreg(dc, Rd, Rn, Rm, shift, Rs)                            \
    instr_create_1dst_4src((dc), OP_orr, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_orrs_shreg(dc, Rd, Rn, Rm, shift, Rs)                           \
    instr_create_1dst_4src((dc), OP_orrs, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_rsb_shreg(dc, Rd, Rn, Rm, shift, Rs)                            \
    instr_create_1dst_4src((dc), OP_rsb, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_rsbs_shreg(dc, Rd, Rn, Rm, shift, Rs)                           \
    instr_create_1dst_4src((dc), OP_rsbs, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_rsc_shreg(dc, Rd, Rn, Rm, shift, Rs)                            \
    instr_create_1dst_4src((dc), OP_rsc, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_rscs_shreg(dc, Rd, Rn, Rm, shift, Rs)                           \
    instr_create_1dst_4src((dc), OP_rscs, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_sbc_shreg(dc, Rd, Rn, Rm, shift, Rs)                            \
    instr_create_1dst_4src((dc), OP_sbc, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_sbcs_shreg(dc, Rd, Rn, Rm, shift, Rs)                           \
    instr_create_1dst_4src((dc), OP_sbcs, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_sub_shreg(dc, Rd, Rn, Rm, shift, Rs)                            \
    instr_create_1dst_4src((dc), OP_sub, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
#define INSTR_CREATE_subs_shreg(dc, Rd, Rn, Rm, shift, Rs)                           \
    instr_create_1dst_4src((dc), OP_subs, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (Rs))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rm, shift, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rm The source register opnd_t operand.
 * \param shift The dr_shift_type_t integer constant opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_mvn_shimm(dc, Rd, Rm, shift, imm)                               \
    instr_create_1dst_3src((dc), OP_mvn, (Rd),                                       \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_mvns_shimm(dc, Rd, Rm, shift, imm)                              \
    instr_create_1dst_3src((dc), OP_mvns, (Rd),                                      \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
/** @} */ /* end doxygen group */

/** @name Signature: (Rn, Rm, shift, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rn The source register opnd_t operand.
 * \param Rm The second source register opnd_t operand.
 * \param shift The dr_shift_type_t integer constant opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_cmn_shimm(dc, Rn, Rm, shift, imm)                               \
    instr_create_0dst_4src((dc), OP_cmn, (Rn),                                       \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_cmp_shimm(dc, Rn, Rm, shift, imm)                               \
    instr_create_0dst_4src((dc), OP_cmp, (Rn),                                       \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_teq_shimm(dc, Rn, Rm, shift, imm)                               \
    instr_create_0dst_4src((dc), OP_teq, (Rn),                                       \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_tst_shimm(dc, Rn, Rm, shift, imm)                               \
    instr_create_0dst_4src((dc), OP_tst, (Rn),                                       \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rn, Rm, shift, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rn The source register opnd_t operand.
 * \param Rm The second source register opnd_t operand.
 * \param shift The dr_shift_type_t integer constant opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_adc_shimm(dc, Rd, Rn, Rm, shift, imm)                           \
    instr_create_1dst_4src((dc), OP_adc, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_adcs_shimm(dc, Rd, Rn, Rm, shift, imm)                          \
    instr_create_1dst_4src((dc), OP_adcs, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_add_shimm(dc, Rd, Rn, Rm, shift, imm)                           \
    instr_create_1dst_4src((dc), OP_add, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_adds_shimm(dc, Rd, Rn, Rm, shift, imm)                          \
    instr_create_1dst_4src((dc), OP_adds, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_and_shimm(dc, Rd, Rn, Rm, shift, imm)                           \
    instr_create_1dst_4src((dc), OP_and, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_ands_shimm(dc, Rd, Rn, Rm, shift, imm)                          \
    instr_create_1dst_4src((dc), OP_ands, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_bic_shimm(dc, Rd, Rn, Rm, shift, imm)                           \
    instr_create_1dst_4src((dc), OP_bic, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_bics_shimm(dc, Rd, Rn, Rm, shift, imm)                          \
    instr_create_1dst_4src((dc), OP_bics, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_eor_shimm(dc, Rd, Rn, Rm, shift, imm)                           \
    instr_create_1dst_4src((dc), OP_eor, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_eors_shimm(dc, Rd, Rn, Rm, shift, imm)                          \
    instr_create_1dst_4src((dc), OP_eors, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_orn_shimm(dc, Rd, Rn, Rm, shift, imm)                           \
    instr_create_1dst_4src((dc), OP_orn, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_orns_shimm(dc, Rd, Rn, Rm, shift, imm)                          \
    instr_create_1dst_4src((dc), OP_orns, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_orr_shimm(dc, Rd, Rn, Rm, shift, imm)                           \
    instr_create_1dst_4src((dc), OP_orr, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_orrs_shimm(dc, Rd, Rn, Rm, shift, imm)                          \
    instr_create_1dst_4src((dc), OP_orrs, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_pkhbt_shimm(dc, Rd, Rn, Rm, shift, imm) \
    instr_create_1dst_4src((dc), OP_pkhbt, (Rd), (Rn), (Rm), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_pkhtb_shimm(dc, Rd, Rn, Rm, shift, imm) \
    instr_create_1dst_4src((dc), OP_pkhtb, (Rd), (Rn), (Rm), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_rsb_shimm(dc, Rd, Rn, Rm, shift, imm)                           \
    instr_create_1dst_4src((dc), OP_rsb, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_rsbs_shimm(dc, Rd, Rn, Rm, shift, imm)                          \
    instr_create_1dst_4src((dc), OP_rsbs, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_rsc_shimm(dc, Rd, Rn, Rm, shift, imm)                           \
    instr_create_1dst_4src((dc), OP_rsc, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_rscs_shimm(dc, Rd, Rn, Rm, shift, imm)                          \
    instr_create_1dst_4src((dc), OP_rscs, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_sbc_shimm(dc, Rd, Rn, Rm, shift, imm)                           \
    instr_create_1dst_4src((dc), OP_sbc, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_sbcs_shimm(dc, Rd, Rn, Rm, shift, imm)                          \
    instr_create_1dst_4src((dc), OP_sbcs, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_sub_shimm(dc, Rd, Rn, Rm, shift, imm)                           \
    instr_create_1dst_4src((dc), OP_sub, (Rd), (Rn),                                 \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
#define INSTR_CREATE_subs_shimm(dc, Rd, Rn, Rm, shift, imm)                          \
    instr_create_1dst_4src((dc), OP_subs, (Rd), (Rn),                                \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, imm, Rm, shift, imm2) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param Rm The source register opnd_t operand.
 * \param shift The dr_shift_type_t integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 */
#define INSTR_CREATE_ssat_shimm(dc, Rd, imm, Rm, shift, imm2)                        \
    instr_create_1dst_4src((dc), OP_ssat, (Rd), (imm),                               \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm2))
#define INSTR_CREATE_usat_shimm(dc, Rd, imm, Rm, shift, imm2)                        \
    instr_create_1dst_4src((dc), OP_usat, (Rd), (imm),                               \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm2))
/** @} */ /* end doxygen group */

/** @name Signature: (mem) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 */
#define INSTR_CREATE_pld(dc, mem) instr_create_0dst_1src((dc), OP_pld, (mem))
#define INSTR_CREATE_pldw(dc, mem) instr_create_0dst_1src((dc), OP_pldw, (mem))
#define INSTR_CREATE_pli(dc, mem) instr_create_0dst_1src((dc), OP_pli, (mem))
#define INSTR_CREATE_tbb(dc, mem) instr_create_0dst_1src((dc), OP_tbb, (mem))
#define INSTR_CREATE_tbh(dc, mem) instr_create_0dst_1src((dc), OP_tbh, (mem))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, mem) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param mem The memory opnd_t operand.
 */
#define INSTR_CREATE_lda(dc, Rd, mem) instr_create_1dst_1src((dc), OP_lda, (Rd), (mem))
#define INSTR_CREATE_ldab(dc, Rd, mem) instr_create_1dst_1src((dc), OP_ldab, (Rd), (mem))
#define INSTR_CREATE_ldaex(dc, Rd, mem) \
    instr_create_1dst_1src((dc), OP_ldaex, (Rd), (mem))
#define INSTR_CREATE_ldaexb(dc, Rd, mem) \
    instr_create_1dst_1src((dc), OP_ldaexb, (Rd), (mem))
#define INSTR_CREATE_ldaexh(dc, Rd, mem) \
    instr_create_1dst_1src((dc), OP_ldaexh, (Rd), (mem))
#define INSTR_CREATE_ldah(dc, Rd, mem) instr_create_1dst_1src((dc), OP_ldah, (Rd), (mem))
#define INSTR_CREATE_ldr(dc, Rd, mem) instr_create_1dst_1src((dc), OP_ldr, (Rd), (mem))
#define INSTR_CREATE_ldrb(dc, Rd, mem) instr_create_1dst_1src((dc), OP_ldrb, (Rd), (mem))
#define INSTR_CREATE_ldrbt(dc, Rd, mem) \
    instr_create_1dst_1src((dc), OP_ldrbt, (Rd), (mem))
#define INSTR_CREATE_ldrex(dc, Rd, mem) \
    instr_create_1dst_1src((dc), OP_ldrex, (Rd), (mem))
#define INSTR_CREATE_ldrexb(dc, Rd, mem) \
    instr_create_1dst_1src((dc), OP_ldrexb, (Rd), (mem))
#define INSTR_CREATE_ldrexh(dc, Rd, mem) \
    instr_create_1dst_1src((dc), OP_ldrexh, (Rd), (mem))
#define INSTR_CREATE_ldrh(dc, Rd, mem) instr_create_1dst_1src((dc), OP_ldrh, (Rd), (mem))
#define INSTR_CREATE_ldrht(dc, Rd, mem) \
    instr_create_1dst_1src((dc), OP_ldrht, (Rd), (mem))
#define INSTR_CREATE_ldrsb(dc, Rd, mem) \
    instr_create_1dst_1src((dc), OP_ldrsb, (Rd), (mem))
#define INSTR_CREATE_ldrsbt(dc, Rd, mem) \
    instr_create_1dst_1src((dc), OP_ldrsbt, (Rd), (mem))
#define INSTR_CREATE_ldrsh(dc, Rd, mem) \
    instr_create_1dst_1src((dc), OP_ldrsh, (Rd), (mem))
#define INSTR_CREATE_ldrsht(dc, Rd, mem) \
    instr_create_1dst_1src((dc), OP_ldrsht, (Rd), (mem))
#define INSTR_CREATE_ldrt(dc, Rd, mem) instr_create_1dst_1src((dc), OP_ldrt, (Rd), (mem))
/** @} */ /* end doxygen group */

/** @name Signature: (mem, Rm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param Rm The source register opnd_t operand.
 */
#define INSTR_CREATE_stl(dc, mem, Rm) instr_create_1dst_1src((dc), OP_stl, (mem), (Rm))
#define INSTR_CREATE_stlb(dc, mem, Rm) instr_create_1dst_1src((dc), OP_stlb, (mem), (Rm))
#define INSTR_CREATE_stlh(dc, mem, Rm) instr_create_1dst_1src((dc), OP_stlh, (mem), (Rm))
/** @} */ /* end doxygen group */

/** @name Signature: (mem, Rt) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param Rt The source register opnd_t operand.
 */
#define INSTR_CREATE_str(dc, mem, Rt) instr_create_1dst_1src((dc), OP_str, (mem), (Rt))
#define INSTR_CREATE_strb(dc, mem, Rt) instr_create_1dst_1src((dc), OP_strb, (mem), (Rt))
#define INSTR_CREATE_strbt(dc, mem, Rt) \
    instr_create_1dst_1src((dc), OP_strbt, (mem), (Rt))
#define INSTR_CREATE_strh(dc, mem, Rt) instr_create_1dst_1src((dc), OP_strh, (mem), (Rt))
#define INSTR_CREATE_strht(dc, mem, Rt) \
    instr_create_1dst_1src((dc), OP_strht, (mem), (Rt))
#define INSTR_CREATE_strt(dc, mem, Rt) instr_create_1dst_1src((dc), OP_strt, (mem), (Rt))
/** @} */ /* end doxygen group */

/** @name Signature: (statreg, mem) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param statreg The status register (usually DR_REG_CPSR) opnd_t operand.
 * \param mem The memory opnd_t operand.
 */
#define INSTR_CREATE_rfe(dc, statreg, mem) \
    instr_create_1dst_1src((dc), OP_rfe, (statreg), (mem))
#define INSTR_CREATE_rfe_wb(dc, statreg, mem)                                            \
    instr_create_2dst_2src((dc), OP_rfe, opnd_create_reg(opnd_get_base(mem)), (statreg), \
                           (mem), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_rfeda(dc, statreg, mem) \
    instr_create_1dst_1src((dc), OP_rfeda, (statreg), (mem))
#define INSTR_CREATE_rfeda_wb(dc, statreg, mem)                                 \
    instr_create_2dst_2src((dc), OP_rfeda, opnd_create_reg(opnd_get_base(mem)), \
                           (statreg), (mem), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_rfedb(dc, statreg, mem) \
    instr_create_1dst_1src((dc), OP_rfedb, (statreg), (mem))
#define INSTR_CREATE_rfedb_wb(dc, statreg, mem)                                 \
    instr_create_2dst_2src((dc), OP_rfedb, opnd_create_reg(opnd_get_base(mem)), \
                           (statreg), (mem), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_rfeib(dc, statreg, mem) \
    instr_create_1dst_1src((dc), OP_rfeib, (statreg), (mem))
#define INSTR_CREATE_rfeib_wb(dc, statreg, mem)                                 \
    instr_create_2dst_2src((dc), OP_rfeib, opnd_create_reg(opnd_get_base(mem)), \
                           (statreg), (mem), opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rd2, mem) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rd2 The second destination register opnd_t operand.
 * \param mem The memory opnd_t operand.
 */
#define INSTR_CREATE_ldaexd(dc, Rd, Rd2, mem) \
    instr_create_2dst_1src((dc), OP_ldaexd, (Rd), (Rd2), (mem))
#define INSTR_CREATE_ldrd(dc, Rd, Rd2, mem) \
    instr_create_2dst_1src((dc), OP_ldrd, (Rd), (Rd2), (mem))
#define INSTR_CREATE_ldrexd(dc, Rd, Rd2, mem) \
    instr_create_2dst_1src((dc), OP_ldrexd, (Rd), (Rd2), (mem))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, mem, Rm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param mem The memory opnd_t operand.
 * \param Rm The source register opnd_t operand.
 */
#define INSTR_CREATE_ldrh_wbreg(dc, Rd, mem, Rm)                                     \
    instr_create_2dst_3src((dc), OP_ldrh, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem),                                                    \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldrht_wbreg(dc, Rd, mem, Rm)                                     \
    instr_create_2dst_3src((dc), OP_ldrht, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem),                                                     \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),  \
                           opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldrsb_wbreg(dc, Rd, mem, Rm)                                     \
    instr_create_2dst_3src((dc), OP_ldrsb, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem),                                                     \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),  \
                           opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldrsbt_wbreg(dc, Rd, mem, Rm)                                     \
    instr_create_2dst_3src((dc), OP_ldrsbt, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem),                                                      \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),   \
                           opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldrsh_wbreg(dc, Rd, mem, Rm)                                     \
    instr_create_2dst_3src((dc), OP_ldrsh, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem),                                                     \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),  \
                           opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldrsht_wbreg(dc, Rd, mem, Rm)                                     \
    instr_create_2dst_3src((dc), OP_ldrsht, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem),                                                      \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),   \
                           opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_swp(dc, Rd, mem, Rm) \
    instr_create_2dst_2src((dc), OP_swp, (mem), (Rd), (mem), (Rm))
#define INSTR_CREATE_swpb(dc, Rd, mem, Rm) \
    instr_create_2dst_2src((dc), OP_swpb, (mem), (Rd), (mem), (Rm))
/** @} */ /* end doxygen group */

/** @name Signature: (mem, Rd, Rm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param Rd The destination register opnd_t operand.
 * \param Rm The source register opnd_t operand.
 */
#define INSTR_CREATE_stlex(dc, mem, Rd, Rm) \
    instr_create_2dst_1src((dc), OP_stlex, (mem), (Rd), (Rm))
#define INSTR_CREATE_stlexb(dc, mem, Rd, Rm) \
    instr_create_2dst_1src((dc), OP_stlexb, (mem), (Rd), (Rm))
#define INSTR_CREATE_stlexh(dc, mem, Rd, Rm) \
    instr_create_2dst_1src((dc), OP_stlexh, (mem), (Rd), (Rm))
#define INSTR_CREATE_strex(dc, mem, Rd, Rm) \
    instr_create_2dst_1src((dc), OP_strex, (mem), (Rd), (Rm))
#define INSTR_CREATE_strexb(dc, mem, Rd, Rm) \
    instr_create_2dst_1src((dc), OP_strexb, (mem), (Rd), (Rm))
#define INSTR_CREATE_strexh(dc, mem, Rd, Rm) \
    instr_create_2dst_1src((dc), OP_strexh, (mem), (Rd), (Rm))
/** @} */ /* end doxygen group */

/** @name Signature: (mem, Rt, Rm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param Rt The source register opnd_t operand.
 * \param Rm The source register opnd_t operand.
 */
#define INSTR_CREATE_strh_wbreg(dc, mem, Rt, Rm)                                      \
    instr_create_2dst_3src((dc), OP_strh, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (Rt),                                                      \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),  \
                           opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_strht_wbreg(dc, mem, Rt, Rm)                                      \
    instr_create_2dst_3src((dc), OP_strht, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (Rt),                                                       \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),   \
                           opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (mem, Rt, Rt2) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param Rt The source register opnd_t operand.
 * \param Rt2 The second source register opnd_t operand.
 */
#define INSTR_CREATE_strd(dc, mem, Rt, Rt2) \
    instr_create_1dst_2src((dc), OP_strd, (mem), (Rt), (Rt2))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rd2, mem, Rm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rd2 The second destination register opnd_t operand.
 * \param mem The memory opnd_t operand.
 * \param Rm The source register opnd_t operand.
 */
#define INSTR_CREATE_ldrd_wbreg(dc, Rd, Rd2, mem, Rm)                                \
    instr_create_3dst_3src((dc), OP_ldrd, (Rd), (Rd2),                               \
                           opnd_create_reg(opnd_get_base(mem)), (mem),               \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (mem, Rd, Rt, Rt2) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param Rd The destination register opnd_t operand.
 * \param Rt The source register opnd_t operand.
 * \param Rt2 The second source register opnd_t operand.
 */
#define INSTR_CREATE_stlexd(dc, mem, Rd, Rt, Rt2) \
    instr_create_2dst_2src((dc), OP_stlexd, (mem), (Rd), (Rt), (Rt2))
#define INSTR_CREATE_strexd(dc, mem, Rd, Rt, Rt2) \
    instr_create_2dst_2src((dc), OP_strexd, (mem), (Rd), (Rt), (Rt2))
/** @} */ /* end doxygen group */

/** @name Signature: (mem, Rt, Rt2, Rm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param Rt The source register opnd_t operand.
 * \param Rt2 The second source register opnd_t operand.
 * \param Rm The source register opnd_t operand.
 */
#define INSTR_CREATE_strd_wbreg(dc, mem, Rt, Rt2, Rm)                                 \
    instr_create_2dst_4src((dc), OP_strd, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (Rt), (Rt2),                                               \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),  \
                           opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, mem, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param mem The memory opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_ldr_wbimm(dc, Rd, mem, imm)                                    \
    instr_create_2dst_3src((dc), OP_ldr, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem), (imm), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldrb_wbimm(dc, Rd, mem, imm)                                    \
    instr_create_2dst_3src((dc), OP_ldrb, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem), (imm), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldrbt_wbimm(dc, Rd, mem, imm)                                    \
    instr_create_2dst_3src((dc), OP_ldrbt, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem), (imm), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldrh_wbimm(dc, Rd, mem, imm)                                    \
    instr_create_2dst_3src((dc), OP_ldrh, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem), (imm), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldrht_wbimm(dc, Rd, mem, imm)                                    \
    instr_create_2dst_3src((dc), OP_ldrht, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem), (imm), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldrsb_wbimm(dc, Rd, mem, imm)                                    \
    instr_create_2dst_3src((dc), OP_ldrsb, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem), (imm), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldrsbt_wbimm(dc, Rd, mem, imm)                                    \
    instr_create_2dst_3src((dc), OP_ldrsbt, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem), (imm), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldrsh_wbimm(dc, Rd, mem, imm)                                    \
    instr_create_2dst_3src((dc), OP_ldrsh, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem), (imm), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldrsht_wbimm(dc, Rd, mem, imm)                                    \
    instr_create_2dst_3src((dc), OP_ldrsht, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem), (imm), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldrt_wbimm(dc, Rd, mem, imm)                                    \
    instr_create_2dst_3src((dc), OP_ldrt, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem), (imm), opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (mem, Rt, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param Rt The source register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_str_wbimm(dc, mem, Rt, imm)                                     \
    instr_create_2dst_3src((dc), OP_str, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (Rt), (imm), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_strb_wbimm(dc, mem, Rt, imm)                                     \
    instr_create_2dst_3src((dc), OP_strb, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (Rt), (imm), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_strbt_wbimm(dc, mem, Rt, imm)                                     \
    instr_create_2dst_3src((dc), OP_strbt, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (Rt), (imm), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_strh_wbimm(dc, mem, Rt, imm)                                     \
    instr_create_2dst_3src((dc), OP_strh, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (Rt), (imm), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_strht_wbimm(dc, mem, Rt, imm)                                     \
    instr_create_2dst_3src((dc), OP_strht, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (Rt), (imm), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_strt_wbimm(dc, mem, Rt, imm)                                     \
    instr_create_2dst_3src((dc), OP_strt, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (Rt), (imm), opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (mem, imm, statreg) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param statreg The status register (usually DR_REG_CPSR) opnd_t operand.
 */
#define INSTR_CREATE_srs(dc, mem, imm, statreg)                                    \
    instr_create_1dst_3src((dc), OP_srs, (mem), (imm), opnd_create_reg(DR_REG_LR), \
                           (statreg))
#define INSTR_CREATE_srs_wbimm(dc, mem, imm, statreg)                                \
    instr_create_2dst_4src((dc), OP_srs, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (imm), opnd_create_reg(opnd_get_base(mem)),               \
                           opnd_create_reg(DR_REG_LR), (statreg))
#define INSTR_CREATE_srsda(dc, mem, imm, statreg)                                    \
    instr_create_1dst_3src((dc), OP_srsda, (mem), (imm), opnd_create_reg(DR_REG_LR), \
                           (statreg))
#define INSTR_CREATE_srsda_wbimm(dc, mem, imm, statreg)                                \
    instr_create_2dst_4src((dc), OP_srsda, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (imm), opnd_create_reg(opnd_get_base(mem)),                 \
                           opnd_create_reg(DR_REG_LR), (statreg))
#define INSTR_CREATE_srsdb(dc, mem, imm, statreg)                                    \
    instr_create_1dst_3src((dc), OP_srsdb, (mem), (imm), opnd_create_reg(DR_REG_LR), \
                           (statreg))
#define INSTR_CREATE_srsdb_wbimm(dc, mem, imm, statreg)                                \
    instr_create_2dst_4src((dc), OP_srsdb, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (imm), opnd_create_reg(opnd_get_base(mem)),                 \
                           opnd_create_reg(DR_REG_LR), (statreg))
#define INSTR_CREATE_srsib(dc, mem, imm, statreg)                                    \
    instr_create_1dst_3src((dc), OP_srsib, (mem), (imm), opnd_create_reg(DR_REG_LR), \
                           (statreg))
#define INSTR_CREATE_srsib_wbimm(dc, mem, imm, statreg)                                \
    instr_create_2dst_4src((dc), OP_srsib, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (imm), opnd_create_reg(opnd_get_base(mem)),                 \
                           opnd_create_reg(DR_REG_LR), (statreg))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rd2, mem, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rd2 The second destination register opnd_t operand.
 * \param mem The memory opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_ldrd_wbimm(dc, Rd, Rd2, mem, imm)                        \
    instr_create_3dst_3src((dc), OP_ldrd, (Rd), (Rd2),                        \
                           opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                           opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (mem, Rt, Rt2, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param Rt The source register opnd_t operand.
 * \param Rt2 The second source register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_strd_wbimm(dc, mem, Rt, Rt2, imm)                                \
    instr_create_2dst_4src((dc), OP_strd, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (Rt), (Rt2), (imm), opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, mem, Rm, shift, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param mem The memory opnd_t operand.
 * \param Rm The source register opnd_t operand.
 * \param shift The dr_shift_type_t integer constant opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_ldr_wbreg(dc, Rd, mem, Rm, shift, imm)                          \
    instr_create_2dst_5src((dc), OP_ldr, (Rd), opnd_create_reg(opnd_get_base(mem)),  \
                           (mem),                                                    \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm),         \
                           opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldrb_wbreg(dc, Rd, mem, Rm, shift, imm)                         \
    instr_create_2dst_5src((dc), OP_ldrb, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem),                                                    \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm),         \
                           opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldrbt_wbreg(dc, Rd, mem, Rm, shift, imm)                         \
    instr_create_2dst_5src((dc), OP_ldrbt, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem),                                                     \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),  \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm),          \
                           opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldrt_wbreg(dc, Rd, mem, Rm, shift, imm)                         \
    instr_create_2dst_5src((dc), OP_ldrt, (Rd), opnd_create_reg(opnd_get_base(mem)), \
                           (mem),                                                    \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm),         \
                           opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (mem, Rt, Rm, shift, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param Rt The source register opnd_t operand.
 * \param Rm The source register opnd_t operand.
 * \param shift The dr_shift_type_t integer constant opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_str_wbreg(dc, mem, Rt, Rm, shift, imm)                          \
    instr_create_2dst_5src((dc), OP_str, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (Rt),                                                     \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm),         \
                           opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_strb_wbreg(dc, mem, Rt, Rm, shift, imm)                          \
    instr_create_2dst_5src((dc), OP_strb, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (Rt),                                                      \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),  \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm),          \
                           opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_strbt_wbreg(dc, mem, Rt, Rm, shift, imm)                          \
    instr_create_2dst_5src((dc), OP_strbt, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (Rt),                                                       \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),   \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm),           \
                           opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_strt_wbreg(dc, mem, Rt, Rm, shift, imm)                          \
    instr_create_2dst_5src((dc), OP_strt, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (Rt),                                                      \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),  \
                           opnd_add_flags((shift), DR_OPND_IS_SHIFT), (imm),          \
                           opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (mem, list_len, ...) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param list_len The number of registers in the register list.
 * \param ... The register list as separate opnd_t arguments.
 * The registers in the list must be in increasing order.
 */
#define INSTR_CREATE_ldm(dc, mem, list_len, ...) \
    instr_create_Ndst_Msrc_vardst((dc), OP_ldm, 0, 1, list_len, 0, (mem), __VA_ARGS__)
#define INSTR_CREATE_ldm_priv(dc, mem, list_len, ...)                          \
    instr_create_Ndst_Msrc_vardst((dc), OP_ldm_priv, 0, 1, list_len, 0, (mem), \
                                  __VA_ARGS__)
#define INSTR_CREATE_ldm_priv_wb(dc, mem, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst((dc), OP_ldm_priv, 1, 2, list_len, 0,       \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_ldm_wb(dc, mem, list_len, ...)                           \
    instr_create_Ndst_Msrc_vardst((dc), OP_ldm, 1, 2, list_len, 0,            \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_ldmda(dc, mem, list_len, ...) \
    instr_create_Ndst_Msrc_vardst((dc), OP_ldmda, 0, 1, list_len, 0, (mem), __VA_ARGS__)
#define INSTR_CREATE_ldmda_priv(dc, mem, list_len, ...)                          \
    instr_create_Ndst_Msrc_vardst((dc), OP_ldmda_priv, 0, 1, list_len, 0, (mem), \
                                  __VA_ARGS__)
#define INSTR_CREATE_ldmda_priv_wb(dc, mem, list_len, ...)                    \
    instr_create_Ndst_Msrc_vardst((dc), OP_ldmda_priv, 1, 2, list_len, 0,     \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_ldmda_wb(dc, mem, list_len, ...)                         \
    instr_create_Ndst_Msrc_vardst((dc), OP_ldmda, 1, 2, list_len, 0,          \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_ldmdb(dc, mem, list_len, ...) \
    instr_create_Ndst_Msrc_vardst((dc), OP_ldmdb, 0, 1, list_len, 0, (mem), __VA_ARGS__)
#define INSTR_CREATE_ldmdb_priv(dc, mem, list_len, ...)                          \
    instr_create_Ndst_Msrc_vardst((dc), OP_ldmdb_priv, 0, 1, list_len, 0, (mem), \
                                  __VA_ARGS__)
#define INSTR_CREATE_ldmdb_priv_wb(dc, mem, list_len, ...)                    \
    instr_create_Ndst_Msrc_vardst((dc), OP_ldmdb_priv, 1, 2, list_len, 0,     \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_ldmdb_wb(dc, mem, list_len, ...)                         \
    instr_create_Ndst_Msrc_vardst((dc), OP_ldmdb, 1, 2, list_len, 0,          \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_ldmib(dc, mem, list_len, ...) \
    instr_create_Ndst_Msrc_vardst((dc), OP_ldmib, 0, 1, list_len, 0, (mem), __VA_ARGS__)
#define INSTR_CREATE_ldmib_priv(dc, mem, list_len, ...)                          \
    instr_create_Ndst_Msrc_vardst((dc), OP_ldmib_priv, 0, 1, list_len, 0, (mem), \
                                  __VA_ARGS__)
#define INSTR_CREATE_ldmib_wb(dc, mem, list_len, ...)                         \
    instr_create_Ndst_Msrc_vardst((dc), OP_ldmib, 1, 2, list_len, 0,          \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_stm(dc, mem, list_len, ...) \
    instr_create_Ndst_Msrc_varsrc((dc), OP_stm, 1, 0, list_len, 0, (mem), __VA_ARGS__)
#define INSTR_CREATE_stm_priv(dc, mem, list_len, ...)                          \
    instr_create_Ndst_Msrc_varsrc((dc), OP_stm_priv, 1, 0, list_len, 0, (mem), \
                                  __VA_ARGS__)
#define INSTR_CREATE_stm_wb(dc, mem, list_len, ...)                       \
    instr_create_Ndst_Msrc_varsrc((dc), OP_stm, 2, 1, list_len, 0, (mem), \
                                  opnd_create_reg(opnd_get_base(mem)),    \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_stmda(dc, mem, list_len, ...) \
    instr_create_Ndst_Msrc_varsrc((dc), OP_stmda, 1, 0, list_len, 0, (mem), __VA_ARGS__)
#define INSTR_CREATE_stmda_priv(dc, mem, list_len, ...)                          \
    instr_create_Ndst_Msrc_varsrc((dc), OP_stmda_priv, 1, 0, list_len, 0, (mem), \
                                  __VA_ARGS__)
#define INSTR_CREATE_stmda_wb(dc, mem, list_len, ...)                       \
    instr_create_Ndst_Msrc_varsrc((dc), OP_stmda, 2, 1, list_len, 0, (mem), \
                                  opnd_create_reg(opnd_get_base(mem)),      \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_stmdb(dc, mem, list_len, ...) \
    instr_create_Ndst_Msrc_varsrc((dc), OP_stmdb, 1, 0, list_len, 0, (mem), __VA_ARGS__)
#define INSTR_CREATE_stmdb_priv(dc, mem, list_len, ...)                          \
    instr_create_Ndst_Msrc_varsrc((dc), OP_stmdb_priv, 1, 0, list_len, 0, (mem), \
                                  __VA_ARGS__)
#define INSTR_CREATE_stmdb_wb(dc, mem, list_len, ...)                       \
    instr_create_Ndst_Msrc_varsrc((dc), OP_stmdb, 2, 1, list_len, 0, (mem), \
                                  opnd_create_reg(opnd_get_base(mem)),      \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_stmib(dc, mem, list_len, ...) \
    instr_create_Ndst_Msrc_varsrc((dc), OP_stmib, 1, 0, list_len, 0, (mem), __VA_ARGS__)
#define INSTR_CREATE_stmib_priv(dc, mem, list_len, ...)                          \
    instr_create_Ndst_Msrc_varsrc((dc), OP_stmib_priv, 1, 0, list_len, 0, (mem), \
                                  __VA_ARGS__)
#define INSTR_CREATE_stmib_wb(dc, mem, list_len, ...)                       \
    instr_create_Ndst_Msrc_varsrc((dc), OP_stmib, 2, 1, list_len, 0, (mem), \
                                  opnd_create_reg(opnd_get_base(mem)),      \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld1_dup_8(dc, mem, list_len, ...)                          \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld1_dup_8, 0, 1, list_len, 0, (mem), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld1_dup_8_wb(dc, mem, list_len, ...)                    \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld1_dup_8, 1, 2, list_len, 0,     \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld2_dup_8(dc, mem, list_len, ...)                          \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld2_dup_8, 0, 1, list_len, 0, (mem), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld2_dup_8_wb(dc, mem, list_len, ...)                    \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld2_dup_8, 1, 2, list_len, 0,     \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld3_dup_8(dc, mem, list_len, ...)                          \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld3_dup_8, 0, 1, list_len, 0, (mem), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld3_dup_8_wb(dc, mem, list_len, ...)                    \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld3_dup_8, 1, 2, list_len, 0,     \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld4_dup_8(dc, mem, list_len, ...)                          \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld4_dup_8, 0, 1, list_len, 0, (mem), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld4_dup_8_wb(dc, mem, list_len, ...)                    \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld4_dup_8, 1, 2, list_len, 0,     \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vldm(dc, mem, list_len, ...) \
    instr_create_Ndst_Msrc_vardst((dc), OP_vldm, 0, 1, list_len, 0, (mem), __VA_ARGS__)
#define INSTR_CREATE_vldm_wb(dc, mem, list_len, ...)                          \
    instr_create_Ndst_Msrc_vardst((dc), OP_vldm, 1, 2, list_len, 0,           \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vldmdb(dc, mem, list_len, ...)                           \
    instr_create_Ndst_Msrc_vardst((dc), OP_vldmdb, 1, 2, list_len, 0,         \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vstm(dc, mem, list_len, ...) \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vstm, 1, 0, list_len, 0, (mem), __VA_ARGS__)
#define INSTR_CREATE_vstm_wb(dc, mem, list_len, ...)                       \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vstm, 2, 1, list_len, 0, (mem), \
                                  opnd_create_reg(opnd_get_base(mem)),     \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vstmdb(dc, mem, list_len, ...)                          \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vstmdb, 2, 1, list_len, 0, (mem), \
                                  opnd_create_reg(opnd_get_base(mem)),       \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
/** @} */ /* end doxygen group */

/** @name Signature: (mem, Rm, list_len, ...) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param Rm The source register opnd_t operand.
 * \param list_len The number of registers in the register list.
 * \param ... The register list as separate opnd_t arguments.
 * The registers in the list must be in increasing order.
 */
#define INSTR_CREATE_vld1_dup_8_wbreg(dc, mem, Rm, list_len, ...)                    \
    instr_create_Ndst_Msrc_vardst(                                                   \
        (dc), OP_vld1_dup_8, 1, 3, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),             \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld2_dup_8_wbreg(dc, mem, Rm, list_len, ...)                    \
    instr_create_Ndst_Msrc_vardst(                                                   \
        (dc), OP_vld2_dup_8, 1, 3, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),             \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld3_dup_8_wbreg(dc, mem, Rm, list_len, ...)                    \
    instr_create_Ndst_Msrc_vardst(                                                   \
        (dc), OP_vld3_dup_8, 1, 3, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),             \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld4_dup_8_wbreg(dc, mem, Rm, list_len, ...)                    \
    instr_create_Ndst_Msrc_vardst(                                                   \
        (dc), OP_vld4_dup_8, 1, 3, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),             \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
/** @} */ /* end doxygen group */

/** @name Signature: (mem, imm, list_len, ...) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param list_len The number of registers in the register list.
 * \param ... The register list as separate opnd_t arguments.
 * The registers in the list must be in increasing order.
 */
#define INSTR_CREATE_vld1_16(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld1_16, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld1_16_wbimm(dc, mem, imm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld1_16, 1, 3, list_len, 0,               \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld1_32(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld1_32, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld1_32_wbimm(dc, mem, imm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld1_32, 1, 3, list_len, 0,               \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld1_64(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld1_64, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld1_64_wbimm(dc, mem, imm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld1_64, 1, 3, list_len, 0,               \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld1_8(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld1_8, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld1_8_wbimm(dc, mem, imm, list_len, ...)                       \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld1_8, 1, 3, list_len, 0,                \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld1_dup_16(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld1_dup_16, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld1_dup_16_wbimm(dc, mem, imm, list_len, ...)                  \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld1_dup_16, 1, 3, list_len, 0,           \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld1_dup_32(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld1_dup_32, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld1_dup_32_wbimm(dc, mem, imm, list_len, ...)                  \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld1_dup_32, 1, 3, list_len, 0,           \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld2_16(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld2_16, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld2_16_wbimm(dc, mem, imm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld2_16, 1, 3, list_len, 0,               \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld2_32(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld2_32, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld2_32_wbimm(dc, mem, imm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld2_32, 1, 3, list_len, 0,               \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld2_8(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld2_8, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld2_8_wbimm(dc, mem, imm, list_len, ...)                       \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld2_8, 1, 3, list_len, 0,                \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld2_dup_16(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld2_dup_16, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld2_dup_16_wbimm(dc, mem, imm, list_len, ...)                  \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld2_dup_16, 1, 3, list_len, 0,           \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld2_dup_32(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld2_dup_32, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld2_dup_32_wbimm(dc, mem, imm, list_len, ...)                  \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld2_dup_32, 1, 3, list_len, 0,           \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld2_lane_8(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld2_lane_8, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld2_lane_8_wbimm(dc, mem, imm, list_len, ...)                  \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld2_lane_8, 1, 3, list_len, 0,           \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld3_16(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld3_16, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld3_16_wbimm(dc, mem, imm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld3_16, 1, 3, list_len, 0,               \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld3_32(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld3_32, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld3_32_wbimm(dc, mem, imm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld3_32, 1, 3, list_len, 0,               \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld3_8(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld3_8, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld3_8_wbimm(dc, mem, imm, list_len, ...)                       \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld3_8, 1, 3, list_len, 0,                \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld3_dup_16(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld3_dup_16, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld3_dup_16_wbimm(dc, mem, imm, list_len, ...)                  \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld3_dup_16, 1, 3, list_len, 0,           \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld3_dup_32(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld3_dup_32, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld3_dup_32_wbimm(dc, mem, imm, list_len, ...)                  \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld3_dup_32, 1, 3, list_len, 0,           \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld3_lane_8(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld3_lane_8, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld3_lane_8_wbimm(dc, mem, imm, list_len, ...)                  \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld3_lane_8, 1, 3, list_len, 0,           \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld4_16(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld4_16, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld4_16_wbimm(dc, mem, imm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld4_16, 1, 3, list_len, 0,               \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld4_32(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld4_32, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld4_32_wbimm(dc, mem, imm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld4_32, 1, 3, list_len, 0,               \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld4_8(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld4_8, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld4_8_wbimm(dc, mem, imm, list_len, ...)                       \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld4_8, 1, 3, list_len, 0,                \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld4_dup_16(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld4_dup_16, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld4_dup_16_wbimm(dc, mem, imm, list_len, ...)                  \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld4_dup_16, 1, 3, list_len, 0,           \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld4_dup_32(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld4_dup_32, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld4_dup_32_wbimm(dc, mem, imm, list_len, ...)                  \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld4_dup_32, 1, 3, list_len, 0,           \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld4_lane_8(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld4_lane_8, 0, 2, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vld4_lane_8_wbimm(dc, mem, imm, list_len, ...)                  \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld4_lane_8, 1, 3, list_len, 0,           \
                                  opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst1_16(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst1_16, 1, 1, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vst1_16_wbimm(dc, mem, imm, list_len, ...)               \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst1_16, 2, 2, list_len, 0, (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst1_32(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst1_32, 1, 1, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vst1_32_wbimm(dc, mem, imm, list_len, ...)               \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst1_32, 2, 2, list_len, 0, (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst1_64(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst1_64, 1, 1, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vst1_64_wbimm(dc, mem, imm, list_len, ...)               \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst1_64, 2, 2, list_len, 0, (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst1_8(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst1_8, 1, 1, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vst1_8_wbimm(dc, mem, imm, list_len, ...)                \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst1_8, 2, 2, list_len, 0, (mem),  \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst2_16(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst2_16, 1, 1, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vst2_16_wbimm(dc, mem, imm, list_len, ...)               \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst2_16, 2, 2, list_len, 0, (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst2_32(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst2_32, 1, 1, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vst2_32_wbimm(dc, mem, imm, list_len, ...)               \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst2_32, 2, 2, list_len, 0, (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst2_8(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst2_8, 1, 1, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vst2_8_wbimm(dc, mem, imm, list_len, ...)                \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst2_8, 2, 2, list_len, 0, (mem),  \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst3_16(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst3_16, 1, 1, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vst3_16_wbimm(dc, mem, imm, list_len, ...)               \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst3_16, 2, 2, list_len, 0, (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst3_32(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst3_32, 1, 1, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vst3_32_wbimm(dc, mem, imm, list_len, ...)               \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst3_32, 2, 2, list_len, 0, (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst3_8(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst3_8, 1, 1, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vst3_8_wbimm(dc, mem, imm, list_len, ...)                \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst3_8, 2, 2, list_len, 0, (mem),  \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst3_lane_16(dc, mem, imm, list_len, ...)                     \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst3_lane_16, 1, 1, list_len, 0, (mem), \
                                  (imm), __VA_ARGS__)
#define INSTR_CREATE_vst3_lane_16_wbimm(dc, mem, imm, list_len, ...)               \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst3_lane_16, 2, 2, list_len, 0, (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), (imm),      \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst3_lane_32(dc, mem, imm, list_len, ...)                     \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst3_lane_32, 1, 1, list_len, 0, (mem), \
                                  (imm), __VA_ARGS__)
#define INSTR_CREATE_vst3_lane_32_wbimm(dc, mem, imm, list_len, ...)               \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst3_lane_32, 2, 2, list_len, 0, (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), (imm),      \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst3_lane_8(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst3_lane_8, 1, 1, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vst3_lane_8_wbimm(dc, mem, imm, list_len, ...)               \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst3_lane_8, 2, 2, list_len, 0, (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), (imm),     \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst4_16(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst4_16, 1, 1, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vst4_16_wbimm(dc, mem, imm, list_len, ...)               \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst4_16, 2, 2, list_len, 0, (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst4_32(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst4_32, 1, 1, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vst4_32_wbimm(dc, mem, imm, list_len, ...)               \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst4_32, 2, 2, list_len, 0, (mem), \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst4_8(dc, mem, imm, list_len, ...)                            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst4_8, 1, 1, list_len, 0, (mem), (imm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vst4_8_wbimm(dc, mem, imm, list_len, ...)                \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst4_8, 2, 2, list_len, 0, (mem),  \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
/** @} */ /* end doxygen group */

/** @name Signature: (mem, imm, Rm, list_len, ...) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param Rm The source register opnd_t operand.
 * \param list_len The number of registers in the register list.
 * \param ... The register list as separate opnd_t arguments.
 * The registers in the list must be in increasing order.
 */
#define INSTR_CREATE_vld1_16_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst(                                                       \
        (dc), OP_vld1_16, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), (mem), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                 \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld1_32_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst(                                                       \
        (dc), OP_vld1_32, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), (mem), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                 \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld1_64_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst(                                                       \
        (dc), OP_vld1_64, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), (mem), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                 \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld1_8_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst(                                                      \
        (dc), OP_vld1_8, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), (mem), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld1_dup_16_wbreg(dc, mem, imm, Rm, list_len, ...)               \
    instr_create_Ndst_Msrc_vardst(                                                    \
        (dc), OP_vld1_dup_16, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),       \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld1_dup_32_wbreg(dc, mem, imm, Rm, list_len, ...)               \
    instr_create_Ndst_Msrc_vardst(                                                    \
        (dc), OP_vld1_dup_32, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),       \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld2_16_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst(                                                       \
        (dc), OP_vld2_16, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), (mem), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                 \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld2_32_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst(                                                       \
        (dc), OP_vld2_32, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), (mem), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                 \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld2_8_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst(                                                      \
        (dc), OP_vld2_8, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), (mem), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld2_dup_16_wbreg(dc, mem, imm, Rm, list_len, ...)               \
    instr_create_Ndst_Msrc_vardst(                                                    \
        (dc), OP_vld2_dup_16, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),       \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld2_dup_32_wbreg(dc, mem, imm, Rm, list_len, ...)               \
    instr_create_Ndst_Msrc_vardst(                                                    \
        (dc), OP_vld2_dup_32, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),       \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld2_lane_8_wbreg(dc, mem, imm, Rm, list_len, ...)               \
    instr_create_Ndst_Msrc_vardst(                                                    \
        (dc), OP_vld2_lane_8, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),       \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld3_16_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst(                                                       \
        (dc), OP_vld3_16, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), (mem), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                 \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld3_32_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst(                                                       \
        (dc), OP_vld3_32, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), (mem), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                 \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld3_8_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst(                                                      \
        (dc), OP_vld3_8, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), (mem), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld3_dup_16_wbreg(dc, mem, imm, Rm, list_len, ...)               \
    instr_create_Ndst_Msrc_vardst(                                                    \
        (dc), OP_vld3_dup_16, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),       \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld3_dup_32_wbreg(dc, mem, imm, Rm, list_len, ...)               \
    instr_create_Ndst_Msrc_vardst(                                                    \
        (dc), OP_vld3_dup_32, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),       \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld3_lane_8_wbreg(dc, mem, imm, Rm, list_len, ...)               \
    instr_create_Ndst_Msrc_vardst(                                                    \
        (dc), OP_vld3_lane_8, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),       \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld4_16_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst(                                                       \
        (dc), OP_vld4_16, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), (mem), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                 \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld4_32_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst(                                                       \
        (dc), OP_vld4_32, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), (mem), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                 \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld4_8_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_vardst(                                                      \
        (dc), OP_vld4_8, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), (mem), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld4_dup_16_wbreg(dc, mem, imm, Rm, list_len, ...)               \
    instr_create_Ndst_Msrc_vardst(                                                    \
        (dc), OP_vld4_dup_16, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),       \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld4_dup_32_wbreg(dc, mem, imm, Rm, list_len, ...)               \
    instr_create_Ndst_Msrc_vardst(                                                    \
        (dc), OP_vld4_dup_32, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),       \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld4_lane_8_wbreg(dc, mem, imm, Rm, list_len, ...)               \
    instr_create_Ndst_Msrc_vardst(                                                    \
        (dc), OP_vld4_lane_8, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),       \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst1_16_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_varsrc(                                                       \
        (dc), OP_vst1_16, 2, 3, list_len, 0, (mem), opnd_create_reg(opnd_get_base(mem)), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                 \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst1_32_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_varsrc(                                                       \
        (dc), OP_vst1_32, 2, 3, list_len, 0, (mem), opnd_create_reg(opnd_get_base(mem)), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                 \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst1_64_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_varsrc(                                                       \
        (dc), OP_vst1_64, 2, 3, list_len, 0, (mem), opnd_create_reg(opnd_get_base(mem)), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                 \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst1_8_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_varsrc(                                                      \
        (dc), OP_vst1_8, 2, 3, list_len, 0, (mem), opnd_create_reg(opnd_get_base(mem)), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst2_16_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_varsrc(                                                       \
        (dc), OP_vst2_16, 2, 3, list_len, 0, (mem), opnd_create_reg(opnd_get_base(mem)), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                 \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst2_32_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_varsrc(                                                       \
        (dc), OP_vst2_32, 2, 3, list_len, 0, (mem), opnd_create_reg(opnd_get_base(mem)), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                 \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst2_8_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_varsrc(                                                      \
        (dc), OP_vst2_8, 2, 3, list_len, 0, (mem), opnd_create_reg(opnd_get_base(mem)), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst3_16_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_varsrc(                                                       \
        (dc), OP_vst3_16, 2, 3, list_len, 0, (mem), opnd_create_reg(opnd_get_base(mem)), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                 \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst3_32_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_varsrc(                                                       \
        (dc), OP_vst3_32, 2, 3, list_len, 0, (mem), opnd_create_reg(opnd_get_base(mem)), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                 \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst3_8_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_varsrc(                                                      \
        (dc), OP_vst3_8, 2, 3, list_len, 0, (mem), opnd_create_reg(opnd_get_base(mem)), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst3_lane_16_wbreg(dc, mem, imm, Rm, list_len, ...) \
    instr_create_Ndst_Msrc_varsrc(                                       \
        (dc), OP_vst3_lane_16, 2, 3, list_len, 0, (mem),                 \
        opnd_create_reg(opnd_get_base(mem)), (imm),                      \
        opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),        \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst3_lane_32_wbreg(dc, mem, imm, Rm, list_len, ...) \
    instr_create_Ndst_Msrc_varsrc(                                       \
        (dc), OP_vst3_lane_32, 2, 3, list_len, 0, (mem),                 \
        opnd_create_reg(opnd_get_base(mem)), (imm),                      \
        opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),        \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst3_lane_8_wbreg(dc, mem, imm, Rm, list_len, ...) \
    instr_create_Ndst_Msrc_varsrc(                                      \
        (dc), OP_vst3_lane_8, 2, 3, list_len, 0, (mem),                 \
        opnd_create_reg(opnd_get_base(mem)), (imm),                     \
        opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),       \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst4_16_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_varsrc(                                                       \
        (dc), OP_vst4_16, 2, 3, list_len, 0, (mem), opnd_create_reg(opnd_get_base(mem)), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                 \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst4_32_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_varsrc(                                                       \
        (dc), OP_vst4_32, 2, 3, list_len, 0, (mem), opnd_create_reg(opnd_get_base(mem)), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                 \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst4_8_wbreg(dc, mem, imm, Rm, list_len, ...)                      \
    instr_create_Ndst_Msrc_varsrc(                                                      \
        (dc), OP_vst4_8, 2, 3, list_len, 0, (mem), opnd_create_reg(opnd_get_base(mem)), \
        (imm), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),                \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
/** @} */ /* end doxygen group */

/** @name Signature: (mem, imm, imm2, list_len, ...) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 * \param list_len The number of registers in the register list.
 * \param ... The register list as separate opnd_t arguments.
 * The registers in the list must be in increasing order.
 */
#define INSTR_CREATE_vld2_lane_16(dc, mem, imm, imm2, list_len, ...)               \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld2_lane_16, 0, 3, list_len, 0, (mem), \
                                  (imm), (imm2), __VA_ARGS__)
#define INSTR_CREATE_vld2_lane_16_wbimm(dc, mem, imm, imm2, list_len, ...)             \
    instr_create_Ndst_Msrc_vardst(                                                     \
        (dc), OP_vld2_lane_16, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), (imm), (imm2), opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld2_lane_32(dc, mem, imm, imm2, list_len, ...)               \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld2_lane_32, 0, 3, list_len, 0, (mem), \
                                  (imm), (imm2), __VA_ARGS__)
#define INSTR_CREATE_vld2_lane_32_wbimm(dc, mem, imm, imm2, list_len, ...)             \
    instr_create_Ndst_Msrc_vardst(                                                     \
        (dc), OP_vld2_lane_32, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), (imm), (imm2), opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld3_lane_16(dc, mem, imm, imm2, list_len, ...)               \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld3_lane_16, 0, 3, list_len, 0, (mem), \
                                  (imm), (imm2), __VA_ARGS__)
#define INSTR_CREATE_vld3_lane_16_wbimm(dc, mem, imm, imm2, list_len, ...)             \
    instr_create_Ndst_Msrc_vardst(                                                     \
        (dc), OP_vld3_lane_16, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), (imm), (imm2), opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld3_lane_32(dc, mem, imm, imm2, list_len, ...)               \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld3_lane_32, 0, 3, list_len, 0, (mem), \
                                  (imm), (imm2), __VA_ARGS__)
#define INSTR_CREATE_vld3_lane_32_wbimm(dc, mem, imm, imm2, list_len, ...)             \
    instr_create_Ndst_Msrc_vardst(                                                     \
        (dc), OP_vld3_lane_32, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), (imm), (imm2), opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld4_lane_16(dc, mem, imm, imm2, list_len, ...)               \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld4_lane_16, 0, 3, list_len, 0, (mem), \
                                  (imm), (imm2), __VA_ARGS__)
#define INSTR_CREATE_vld4_lane_16_wbimm(dc, mem, imm, imm2, list_len, ...)             \
    instr_create_Ndst_Msrc_vardst(                                                     \
        (dc), OP_vld4_lane_16, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), (imm), (imm2), opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld4_lane_32(dc, mem, imm, imm2, list_len, ...)               \
    instr_create_Ndst_Msrc_vardst((dc), OP_vld4_lane_32, 0, 3, list_len, 0, (mem), \
                                  (imm), (imm2), __VA_ARGS__)
#define INSTR_CREATE_vld4_lane_32_wbimm(dc, mem, imm, imm2, list_len, ...)             \
    instr_create_Ndst_Msrc_vardst(                                                     \
        (dc), OP_vld4_lane_32, 1, 4, list_len, 0, opnd_create_reg(opnd_get_base(mem)), \
        (mem), (imm), (imm2), opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst2_lane_16(dc, mem, imm, imm2, list_len, ...)               \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst2_lane_16, 1, 2, list_len, 0, (mem), \
                                  (imm), (imm2), __VA_ARGS__)
#define INSTR_CREATE_vst2_lane_16_wbimm(dc, mem, imm, imm2, list_len, ...)            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst2_lane_16, 2, 3, list_len, 0, (mem),    \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), (imm2), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst2_lane_32(dc, mem, imm, imm2, list_len, ...)               \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst2_lane_32, 1, 2, list_len, 0, (mem), \
                                  (imm), (imm2), __VA_ARGS__)
#define INSTR_CREATE_vst2_lane_32_wbimm(dc, mem, imm, imm2, list_len, ...)            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst2_lane_32, 2, 3, list_len, 0, (mem),    \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), (imm2), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst2_lane_8(dc, mem, imm, imm2, list_len, ...)                      \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst2_lane_8, 1, 2, list_len, 0, (mem), (imm), \
                                  (imm2), __VA_ARGS__)
#define INSTR_CREATE_vst2_lane_8_wbimm(dc, mem, imm, imm2, list_len, ...)             \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst2_lane_8, 2, 3, list_len, 0, (mem),     \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), (imm2), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst4_lane_16(dc, mem, imm, imm2, list_len, ...)               \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst4_lane_16, 1, 2, list_len, 0, (mem), \
                                  (imm), (imm2), __VA_ARGS__)
#define INSTR_CREATE_vst4_lane_16_wbimm(dc, mem, imm, imm2, list_len, ...)            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst4_lane_16, 2, 3, list_len, 0, (mem),    \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), (imm2), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst4_lane_32(dc, mem, imm, imm2, list_len, ...)               \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst4_lane_32, 1, 2, list_len, 0, (mem), \
                                  (imm), (imm2), __VA_ARGS__)
#define INSTR_CREATE_vst4_lane_32_wbimm(dc, mem, imm, imm2, list_len, ...)            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst4_lane_32, 2, 3, list_len, 0, (mem),    \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), (imm2), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst4_lane_8(dc, mem, imm, imm2, list_len, ...)                      \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst4_lane_8, 1, 2, list_len, 0, (mem), (imm), \
                                  (imm2), __VA_ARGS__)
#define INSTR_CREATE_vst4_lane_8_wbimm(dc, mem, imm, imm2, list_len, ...)             \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vst4_lane_8, 2, 3, list_len, 0, (mem),     \
                                  opnd_create_reg(opnd_get_base(mem)), (imm), (imm2), \
                                  opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
/** @} */ /* end doxygen group */

/** @name Signature: (mem, imm, imm2, Rm, list_len, ...) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 * \param Rm The source register opnd_t operand.
 * \param list_len The number of registers in the register list.
 * \param ... The register list as separate opnd_t arguments.
 * The registers in the list must be in increasing order.
 */
#define INSTR_CREATE_vld2_lane_16_wbreg(dc, mem, imm, imm2, Rm, list_len, ...)          \
    instr_create_Ndst_Msrc_vardst(                                                      \
        (dc), OP_vld2_lane_16, 1, 5, list_len, 0, opnd_create_reg(opnd_get_base(mem)),  \
        (mem), (imm), (imm2), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld2_lane_32_wbreg(dc, mem, imm, imm2, Rm, list_len, ...)          \
    instr_create_Ndst_Msrc_vardst(                                                      \
        (dc), OP_vld2_lane_32, 1, 5, list_len, 0, opnd_create_reg(opnd_get_base(mem)),  \
        (mem), (imm), (imm2), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld3_lane_16_wbreg(dc, mem, imm, imm2, Rm, list_len, ...)          \
    instr_create_Ndst_Msrc_vardst(                                                      \
        (dc), OP_vld3_lane_16, 1, 5, list_len, 0, opnd_create_reg(opnd_get_base(mem)),  \
        (mem), (imm), (imm2), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld3_lane_32_wbreg(dc, mem, imm, imm2, Rm, list_len, ...)          \
    instr_create_Ndst_Msrc_vardst(                                                      \
        (dc), OP_vld3_lane_32, 1, 5, list_len, 0, opnd_create_reg(opnd_get_base(mem)),  \
        (mem), (imm), (imm2), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld4_lane_16_wbreg(dc, mem, imm, imm2, Rm, list_len, ...)          \
    instr_create_Ndst_Msrc_vardst(                                                      \
        (dc), OP_vld4_lane_16, 1, 5, list_len, 0, opnd_create_reg(opnd_get_base(mem)),  \
        (mem), (imm), (imm2), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vld4_lane_32_wbreg(dc, mem, imm, imm2, Rm, list_len, ...)          \
    instr_create_Ndst_Msrc_vardst(                                                      \
        (dc), OP_vld4_lane_32, 1, 5, list_len, 0, opnd_create_reg(opnd_get_base(mem)),  \
        (mem), (imm), (imm2), opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst2_lane_16_wbreg(dc, mem, imm, imm2, Rm, list_len, ...) \
    instr_create_Ndst_Msrc_varsrc(                                             \
        (dc), OP_vst2_lane_16, 2, 4, list_len, 0, (mem),                       \
        opnd_create_reg(opnd_get_base(mem)), (imm), (imm2),                    \
        opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),              \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst2_lane_32_wbreg(dc, mem, imm, imm2, Rm, list_len, ...) \
    instr_create_Ndst_Msrc_varsrc(                                             \
        (dc), OP_vst2_lane_32, 2, 4, list_len, 0, (mem),                       \
        opnd_create_reg(opnd_get_base(mem)), (imm), (imm2),                    \
        opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),              \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst2_lane_8_wbreg(dc, mem, imm, imm2, Rm, list_len, ...) \
    instr_create_Ndst_Msrc_varsrc(                                            \
        (dc), OP_vst2_lane_8, 2, 4, list_len, 0, (mem),                       \
        opnd_create_reg(opnd_get_base(mem)), (imm), (imm2),                   \
        opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),             \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst4_lane_16_wbreg(dc, mem, imm, imm2, Rm, list_len, ...) \
    instr_create_Ndst_Msrc_varsrc(                                             \
        (dc), OP_vst4_lane_16, 2, 4, list_len, 0, (mem),                       \
        opnd_create_reg(opnd_get_base(mem)), (imm), (imm2),                    \
        opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),              \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst4_lane_32_wbreg(dc, mem, imm, imm2, Rm, list_len, ...) \
    instr_create_Ndst_Msrc_varsrc(                                             \
        (dc), OP_vst4_lane_32, 2, 4, list_len, 0, (mem),                       \
        opnd_create_reg(opnd_get_base(mem)), (imm), (imm2),                    \
        opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),              \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
#define INSTR_CREATE_vst4_lane_8_wbreg(dc, mem, imm, imm2, Rm, list_len, ...) \
    instr_create_Ndst_Msrc_varsrc(                                            \
        (dc), OP_vst4_lane_8, 2, 4, list_len, 0, (mem),                       \
        opnd_create_reg(opnd_get_base(mem)), (imm), (imm2),                   \
        opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),             \
        opnd_create_reg(opnd_get_base(mem)), __VA_ARGS__)
/** @} */ /* end doxygen group */

/** @name Signature: (Ra, Rd, imm, imm2, cpreg) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Ra The third source register opnd_t operand.
 * \param Rd The destination register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 * \param cpreg The coprocessor register opnd_t operand.
 */
#define INSTR_CREATE_mrrc(dc, Ra, Rd, imm, imm2, cpreg) \
    instr_create_2dst_3src((dc), OP_mrrc, (Ra), (Rd), (imm), (imm2), (cpreg))
#define INSTR_CREATE_mrrc2(dc, Ra, Rd, imm, imm2, cpreg) \
    instr_create_2dst_3src((dc), OP_mrrc2, (Ra), (Rd), (imm), (imm2), (cpreg))
/** @} */ /* end doxygen group */

/** @name Signature: (cpreg, Rn, Rt, imm, imm2) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param cpreg The coprocessor register opnd_t operand.
 * \param Rn The source register opnd_t operand.
 * \param Rt The source register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 */
#define INSTR_CREATE_mcrr(dc, cpreg, Rn, Rt, imm, imm2) \
    instr_create_1dst_4src((dc), OP_mcrr, (cpreg), (Rn), (Rt), (imm), (imm2))
#define INSTR_CREATE_mcrr2(dc, cpreg, Rn, Rt, imm, imm2) \
    instr_create_1dst_4src((dc), OP_mcrr2, (cpreg), (Rn), (Rt), (imm), (imm2))
/** @} */ /* end doxygen group */

/** @name Signature: (cpreg, cpreg2, imm, imm2, Rt) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param cpreg The coprocessor register opnd_t operand.
 * \param cpreg2 The second coprocessor register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 * \param Rt The source register opnd_t operand.
 */
#define INSTR_CREATE_mcr2(dc, cpreg, cpreg2, imm, imm2, Rt) \
    instr_create_2dst_3src((dc), OP_mcr2, (cpreg), (cpreg2), (imm), (imm2), (Rt))
/** @} */ /* end doxygen group */

/** @name Signature: (cpreg, imm, imm2, cpreg2, cpreg3) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param cpreg The coprocessor register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 * \param cpreg2 The second coprocessor register opnd_t operand.
 * \param cpreg3 The third coprocessor register opnd_t operand.
 */
#define INSTR_CREATE_cdp2(dc, cpreg, imm, imm2, cpreg2, cpreg3) \
    instr_create_1dst_4src((dc), OP_cdp2, (cpreg), (imm), (imm2), (cpreg2), (cpreg3))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, imm, imm2, cpreg, cpreg2, imm3) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 * \param cpreg The coprocessor register opnd_t operand.
 * \param cpreg2 The second coprocessor register opnd_t operand.
 * \param imm3 The third integer constant opnd_t operand.
 */
#define INSTR_CREATE_mrc(dc, Rd, imm, imm2, cpreg, cpreg2, imm3) \
    instr_create_1dst_5src((dc), OP_mrc, (Rd), (imm), (imm2), (cpreg), (cpreg2), (imm3))
#define INSTR_CREATE_mrc2(dc, Rd, imm, imm2, cpreg, cpreg2, imm3) \
    instr_create_1dst_5src((dc), OP_mrc2, (Rd), (imm), (imm2), (cpreg), (cpreg2), (imm3))
/** @} */ /* end doxygen group */

/** @name Signature: (cpreg, cpreg2, imm, imm2, Rt, imm3) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param cpreg The coprocessor register opnd_t operand.
 * \param cpreg2 The second coprocessor register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 * \param Rt The source register opnd_t operand.
 * \param imm3 The third integer constant opnd_t operand.
 */
#define INSTR_CREATE_mcr(dc, cpreg, cpreg2, imm, imm2, Rt, imm3) \
    instr_create_2dst_4src((dc), OP_mcr, (cpreg), (cpreg2), (imm), (imm2), (Rt), (imm3))
/** @} */ /* end doxygen group */

/** @name Signature: (cpreg, imm, imm2, cpreg2, cpreg3, imm3) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param cpreg The coprocessor register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 * \param cpreg2 The second coprocessor register opnd_t operand.
 * \param cpreg3 The third coprocessor register opnd_t operand.
 * \param imm3 The third integer constant opnd_t operand.
 */
#define INSTR_CREATE_cdp(dc, cpreg, imm, imm2, cpreg2, cpreg3, imm3)                 \
    instr_create_1dst_5src((dc), OP_cdp, (cpreg), (imm), (imm2), (cpreg2), (cpreg3), \
                           (imm3))
/** @} */ /* end doxygen group */

/** @name Signature: (cpreg, mem, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param cpreg The coprocessor register opnd_t operand.
 * \param mem The memory opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_ldc(dc, cpreg, mem, imm) \
    instr_create_1dst_2src((dc), OP_ldc, (cpreg), (mem), (imm))
#define INSTR_CREATE_ldcl(dc, cpreg, mem, imm) \
    instr_create_1dst_2src((dc), OP_ldcl, (cpreg), (mem), (imm))
/** @} */ /* end doxygen group */

/** @name Signature: (mem, imm, cpreg, imm2) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param cpreg The coprocessor register opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 */
#define INSTR_CREATE_stc(dc, mem, imm, cpreg, imm2) \
    instr_create_1dst_3src((dc), OP_stc, (mem), (imm), (cpreg), (imm2))
#define INSTR_CREATE_stc2(dc, mem, imm, cpreg, imm2) \
    instr_create_1dst_3src((dc), OP_stc2, (mem), (imm), (cpreg), (imm2))
#define INSTR_CREATE_stc2_wbimm(dc, mem, imm, cpreg, imm2)                            \
    instr_create_2dst_4src((dc), OP_stc2, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (imm), (cpreg), (imm2), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_stc2l(dc, mem, imm, cpreg, imm2) \
    instr_create_1dst_3src((dc), OP_stc2l, (mem), (imm), (cpreg), (imm2))
#define INSTR_CREATE_stc2l_wbimm(dc, mem, imm, cpreg, imm2)                            \
    instr_create_2dst_4src((dc), OP_stc2l, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (imm), (cpreg), (imm2), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_stc_wbimm(dc, mem, imm, cpreg, imm2)                            \
    instr_create_2dst_4src((dc), OP_stc, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (imm), (cpreg), (imm2), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_stcl(dc, mem, imm, cpreg, imm2) \
    instr_create_1dst_3src((dc), OP_stcl, (mem), (imm), (cpreg), (imm2))
#define INSTR_CREATE_stcl_wbimm(dc, mem, imm, cpreg, imm2)                            \
    instr_create_2dst_4src((dc), OP_stcl, (mem), opnd_create_reg(opnd_get_base(mem)), \
                           (imm), (cpreg), (imm2), opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (cpreg, mem, imm, imm2) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param cpreg The coprocessor register opnd_t operand.
 * \param mem The memory opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 */
#define INSTR_CREATE_ldc2_option(dc, cpreg, mem, imm, imm2) \
    instr_create_1dst_3src((dc), OP_ldc2, (cpreg), (mem), (imm), (imm2))
#define INSTR_CREATE_ldc2_wbimm(dc, cpreg, mem, imm, imm2)                              \
    instr_create_2dst_4src((dc), OP_ldc2, (cpreg), opnd_create_reg(opnd_get_base(mem)), \
                           (mem), (imm), (imm2), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldc2l_option(dc, cpreg, mem, imm, imm2) \
    instr_create_1dst_3src((dc), OP_ldc2l, (cpreg), (mem), (imm), (imm2))
#define INSTR_CREATE_ldc2l_wbimm(dc, cpreg, mem, imm, imm2)                              \
    instr_create_2dst_4src((dc), OP_ldc2l, (cpreg), opnd_create_reg(opnd_get_base(mem)), \
                           (mem), (imm), (imm2), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldc_option(dc, cpreg, mem, imm, imm2) \
    instr_create_1dst_3src((dc), OP_ldc, (cpreg), (mem), (imm), (imm2))
#define INSTR_CREATE_ldc_wbimm(dc, cpreg, mem, imm, imm2)                              \
    instr_create_2dst_4src((dc), OP_ldc, (cpreg), opnd_create_reg(opnd_get_base(mem)), \
                           (mem), (imm), (imm2), opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_ldcl_option(dc, cpreg, mem, imm, imm2) \
    instr_create_1dst_3src((dc), OP_ldcl, (cpreg), (mem), (imm), (imm2))
#define INSTR_CREATE_ldcl_wbimm(dc, cpreg, mem, imm, imm2)                              \
    instr_create_2dst_4src((dc), OP_ldcl, (cpreg), opnd_create_reg(opnd_get_base(mem)), \
                           (mem), (imm), (imm2), opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Vn) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Vn The source SIMD register opnd_t operand.
 */
#define INSTR_CREATE_vmov_s2g(dc, Rd, Vn) \
    instr_create_1dst_1src((dc), OP_vmov, (Rd), (Vn))
/** @} */ /* end doxygen group */

/** @name Signature: (Vd, Vm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vd The destination SIMD register opnd_t operand.
 * \param Vm The source SIMD register opnd_t operand.
 */
#define INSTR_CREATE_aesd_8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_aesd_8, (Vd), (Vm))
#define INSTR_CREATE_aese_8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_aese_8, (Vd), (Vm))
#define INSTR_CREATE_aesimc_8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_aesimc_8, (Vd), (Vm))
#define INSTR_CREATE_aesmc_8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_aesmc_8, (Vd), (Vm))
#define INSTR_CREATE_sha1h_32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_sha1h_32, (Vd), (Vm))
#define INSTR_CREATE_sha1su1_32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_sha1su1_32, (Vd), (Vm))
#define INSTR_CREATE_sha256su0_32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_sha256su0_32, (Vd), (Vm))
#define INSTR_CREATE_vabs_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vabs_f32, (Vd), (Vm))
#define INSTR_CREATE_vabs_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vabs_f64, (Vd), (Vm))
#define INSTR_CREATE_vabs_s16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vabs_s16, (Vd), (Vm))
#define INSTR_CREATE_vabs_s32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vabs_s32, (Vd), (Vm))
#define INSTR_CREATE_vabs_s8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vabs_s8, (Vd), (Vm))
#define INSTR_CREATE_vcls_s16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcls_s16, (Vd), (Vm))
#define INSTR_CREATE_vcls_s32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcls_s32, (Vd), (Vm))
#define INSTR_CREATE_vcls_s8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcls_s8, (Vd), (Vm))
#define INSTR_CREATE_vclz_i16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vclz_i16, (Vd), (Vm))
#define INSTR_CREATE_vclz_i32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vclz_i32, (Vd), (Vm))
#define INSTR_CREATE_vclz_i8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vclz_i8, (Vd), (Vm))
#define INSTR_CREATE_vcnt_8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcnt_8, (Vd), (Vm))
#define INSTR_CREATE_vcvt_f16_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvt_f16_f32, (Vd), (Vm))
#define INSTR_CREATE_vcvt_f32_f16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvt_f32_f16, (Vd), (Vm))
#define INSTR_CREATE_vcvt_f32_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvt_f32_f64, (Vd), (Vm))
#define INSTR_CREATE_vcvt_f32_s32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvt_f32_s32, (Vd), (Vm))
#define INSTR_CREATE_vcvt_f32_u32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvt_f32_u32, (Vd), (Vm))
#define INSTR_CREATE_vcvt_f64_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvt_f64_f32, (Vd), (Vm))
#define INSTR_CREATE_vcvt_f64_s32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvt_f64_s32, (Vd), (Vm))
#define INSTR_CREATE_vcvt_f64_u32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvt_f64_u32, (Vd), (Vm))
#define INSTR_CREATE_vcvt_s32_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvt_s32_f32, (Vd), (Vm))
#define INSTR_CREATE_vcvt_s32_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvt_s32_f64, (Vd), (Vm))
#define INSTR_CREATE_vcvt_u32_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvt_u32_f32, (Vd), (Vm))
#define INSTR_CREATE_vcvt_u32_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvt_u32_f64, (Vd), (Vm))
#define INSTR_CREATE_vcvta_s32_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvta_s32_f32, (Vd), (Vm))
#define INSTR_CREATE_vcvta_s32_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvta_s32_f64, (Vd), (Vm))
#define INSTR_CREATE_vcvta_u32_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvta_u32_f32, (Vd), (Vm))
#define INSTR_CREATE_vcvta_u32_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvta_u32_f64, (Vd), (Vm))
#define INSTR_CREATE_vcvtb_f16_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtb_f16_f32, (Vd), (Vm))
#define INSTR_CREATE_vcvtb_f16_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtb_f16_f64, (Vd), (Vm))
#define INSTR_CREATE_vcvtb_f32_f16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtb_f32_f16, (Vd), (Vm))
#define INSTR_CREATE_vcvtb_f64_f16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtb_f64_f16, (Vd), (Vm))
#define INSTR_CREATE_vcvtm_s32_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtm_s32_f32, (Vd), (Vm))
#define INSTR_CREATE_vcvtm_s32_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtm_s32_f64, (Vd), (Vm))
#define INSTR_CREATE_vcvtm_u32_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtm_u32_f32, (Vd), (Vm))
#define INSTR_CREATE_vcvtm_u32_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtm_u32_f64, (Vd), (Vm))
#define INSTR_CREATE_vcvtn_s32_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtn_s32_f32, (Vd), (Vm))
#define INSTR_CREATE_vcvtn_s32_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtn_s32_f64, (Vd), (Vm))
#define INSTR_CREATE_vcvtn_u32_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtn_u32_f32, (Vd), (Vm))
#define INSTR_CREATE_vcvtn_u32_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtn_u32_f64, (Vd), (Vm))
#define INSTR_CREATE_vcvtp_s32_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtp_s32_f32, (Vd), (Vm))
#define INSTR_CREATE_vcvtp_s32_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtp_s32_f64, (Vd), (Vm))
#define INSTR_CREATE_vcvtp_u32_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtp_u32_f32, (Vd), (Vm))
#define INSTR_CREATE_vcvtp_u32_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtp_u32_f64, (Vd), (Vm))
#define INSTR_CREATE_vcvtr_s32_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtr_s32_f32, (Vd), (Vm))
#define INSTR_CREATE_vcvtr_s32_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtr_s32_f64, (Vd), (Vm))
#define INSTR_CREATE_vcvtr_u32_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtr_u32_f32, (Vd), (Vm))
#define INSTR_CREATE_vcvtr_u32_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtr_u32_f64, (Vd), (Vm))
#define INSTR_CREATE_vcvtt_f16_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtt_f16_f32, (Vd), (Vm))
#define INSTR_CREATE_vcvtt_f16_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtt_f16_f64, (Vd), (Vm))
#define INSTR_CREATE_vcvtt_f32_f16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtt_f32_f16, (Vd), (Vm))
#define INSTR_CREATE_vcvtt_f64_f16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vcvtt_f64_f16, (Vd), (Vm))
#define INSTR_CREATE_vmovl_s16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vmovl_s16, (Vd), (Vm))
#define INSTR_CREATE_vmovl_s32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vmovl_s32, (Vd), (Vm))
#define INSTR_CREATE_vmovl_s8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vmovl_s8, (Vd), (Vm))
#define INSTR_CREATE_vmovl_u16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vmovl_u16, (Vd), (Vm))
#define INSTR_CREATE_vmovl_u32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vmovl_u32, (Vd), (Vm))
#define INSTR_CREATE_vmovl_u8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vmovl_u8, (Vd), (Vm))
#define INSTR_CREATE_vmovn_i16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vmovn_i16, (Vd), (Vm))
#define INSTR_CREATE_vmovn_i32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vmovn_i32, (Vd), (Vm))
#define INSTR_CREATE_vmovn_i64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vmovn_i64, (Vd), (Vm))
#define INSTR_CREATE_vmvn(dc, Vd, Vm) instr_create_1dst_1src((dc), OP_vmvn, (Vd), (Vm))
#define INSTR_CREATE_vneg_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vneg_f32, (Vd), (Vm))
#define INSTR_CREATE_vneg_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vneg_f64, (Vd), (Vm))
#define INSTR_CREATE_vneg_s16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vneg_s16, (Vd), (Vm))
#define INSTR_CREATE_vneg_s32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vneg_s32, (Vd), (Vm))
#define INSTR_CREATE_vneg_s8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vneg_s8, (Vd), (Vm))
#define INSTR_CREATE_vpadal_s16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vpadal_s16, (Vd), (Vm))
#define INSTR_CREATE_vpadal_s32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vpadal_s32, (Vd), (Vm))
#define INSTR_CREATE_vpadal_s8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vpadal_s8, (Vd), (Vm))
#define INSTR_CREATE_vpadal_u16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vpadal_u16, (Vd), (Vm))
#define INSTR_CREATE_vpadal_u32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vpadal_u32, (Vd), (Vm))
#define INSTR_CREATE_vpadal_u8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vpadal_u8, (Vd), (Vm))
#define INSTR_CREATE_vpaddl_s16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vpaddl_s16, (Vd), (Vm))
#define INSTR_CREATE_vpaddl_s32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vpaddl_s32, (Vd), (Vm))
#define INSTR_CREATE_vpaddl_s8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vpaddl_s8, (Vd), (Vm))
#define INSTR_CREATE_vpaddl_u16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vpaddl_u16, (Vd), (Vm))
#define INSTR_CREATE_vpaddl_u32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vpaddl_u32, (Vd), (Vm))
#define INSTR_CREATE_vpaddl_u8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vpaddl_u8, (Vd), (Vm))
#define INSTR_CREATE_vqabs_s16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vqabs_s16, (Vd), (Vm))
#define INSTR_CREATE_vqabs_s32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vqabs_s32, (Vd), (Vm))
#define INSTR_CREATE_vqabs_s8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vqabs_s8, (Vd), (Vm))
#define INSTR_CREATE_vqmovn_s16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vqmovn_s16, (Vd), (Vm))
#define INSTR_CREATE_vqmovn_s32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vqmovn_s32, (Vd), (Vm))
#define INSTR_CREATE_vqmovn_s64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vqmovn_s64, (Vd), (Vm))
#define INSTR_CREATE_vqmovn_u16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vqmovn_u16, (Vd), (Vm))
#define INSTR_CREATE_vqmovn_u32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vqmovn_u32, (Vd), (Vm))
#define INSTR_CREATE_vqmovn_u64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vqmovn_u64, (Vd), (Vm))
#define INSTR_CREATE_vqmovun_s16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vqmovun_s16, (Vd), (Vm))
#define INSTR_CREATE_vqmovun_s32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vqmovun_s32, (Vd), (Vm))
#define INSTR_CREATE_vqmovun_s64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vqmovun_s64, (Vd), (Vm))
#define INSTR_CREATE_vqneg_s16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vqneg_s16, (Vd), (Vm))
#define INSTR_CREATE_vqneg_s32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vqneg_s32, (Vd), (Vm))
#define INSTR_CREATE_vqneg_s8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vqneg_s8, (Vd), (Vm))
#define INSTR_CREATE_vrecpe_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrecpe_f32, (Vd), (Vm))
#define INSTR_CREATE_vrecpe_u32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrecpe_u32, (Vd), (Vm))
#define INSTR_CREATE_vrev16_16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrev16_16, (Vd), (Vm))
#define INSTR_CREATE_vrev16_8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrev16_8, (Vd), (Vm))
#define INSTR_CREATE_vrev32_16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrev32_16, (Vd), (Vm))
#define INSTR_CREATE_vrev32_32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrev32_32, (Vd), (Vm))
#define INSTR_CREATE_vrev32_8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrev32_8, (Vd), (Vm))
#define INSTR_CREATE_vrev64_16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrev64_16, (Vd), (Vm))
#define INSTR_CREATE_vrev64_32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrev64_32, (Vd), (Vm))
#define INSTR_CREATE_vrev64_8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrev64_8, (Vd), (Vm))
#define INSTR_CREATE_vrinta_f32_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrinta_f32_f32, (Vd), (Vm))
#define INSTR_CREATE_vrinta_f64_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrinta_f64_f64, (Vd), (Vm))
#define INSTR_CREATE_vrintm_f32_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrintm_f32_f32, (Vd), (Vm))
#define INSTR_CREATE_vrintm_f64_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrintm_f64_f64, (Vd), (Vm))
#define INSTR_CREATE_vrintn_f32_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrintn_f32_f32, (Vd), (Vm))
#define INSTR_CREATE_vrintn_f64_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrintn_f64_f64, (Vd), (Vm))
#define INSTR_CREATE_vrintp_f32_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrintp_f32_f32, (Vd), (Vm))
#define INSTR_CREATE_vrintp_f64_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrintp_f64_f64, (Vd), (Vm))
#define INSTR_CREATE_vrintr_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrintr_f32, (Vd), (Vm))
#define INSTR_CREATE_vrintr_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrintr_f64, (Vd), (Vm))
#define INSTR_CREATE_vrintx_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrintx_f32, (Vd), (Vm))
#define INSTR_CREATE_vrintx_f32_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrintx_f32_f32, (Vd), (Vm))
#define INSTR_CREATE_vrintx_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrintx_f64, (Vd), (Vm))
#define INSTR_CREATE_vrintz_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrintz_f32, (Vd), (Vm))
#define INSTR_CREATE_vrintz_f32_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrintz_f32_f32, (Vd), (Vm))
#define INSTR_CREATE_vrintz_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrintz_f64, (Vd), (Vm))
#define INSTR_CREATE_vrsqrte_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrsqrte_f32, (Vd), (Vm))
#define INSTR_CREATE_vrsqrte_u32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vrsqrte_u32, (Vd), (Vm))
#define INSTR_CREATE_vsqrt_f32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vsqrt_f32, (Vd), (Vm))
#define INSTR_CREATE_vsqrt_f64(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vsqrt_f64, (Vd), (Vm))
#define INSTR_CREATE_vswp(dc, Vd, Vm) instr_create_1dst_1src((dc), OP_vswp, (Vd), (Vm))
#define INSTR_CREATE_vtrn_16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vtrn_16, (Vd), (Vm))
#define INSTR_CREATE_vtrn_32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vtrn_32, (Vd), (Vm))
#define INSTR_CREATE_vtrn_8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vtrn_8, (Vd), (Vm))
#define INSTR_CREATE_vuzp_16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vuzp_16, (Vd), (Vm))
#define INSTR_CREATE_vuzp_32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vuzp_32, (Vd), (Vm))
#define INSTR_CREATE_vuzp_8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vuzp_8, (Vd), (Vm))
#define INSTR_CREATE_vzip_16(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vzip_16, (Vd), (Vm))
#define INSTR_CREATE_vzip_32(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vzip_32, (Vd), (Vm))
#define INSTR_CREATE_vzip_8(dc, Vd, Vm) \
    instr_create_1dst_1src((dc), OP_vzip_8, (Vd), (Vm))
/** @} */ /* end doxygen group */

/** @name Signature: (Vd, Rt) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vd The destination SIMD register opnd_t operand.
 * \param Rt The source register opnd_t operand.
 */
#define INSTR_CREATE_vdup_16(dc, Vd, Rt) \
    instr_create_1dst_1src((dc), OP_vdup_16, (Vd), (Rt))
#define INSTR_CREATE_vdup_32(dc, Vd, Rt) \
    instr_create_1dst_1src((dc), OP_vdup_32, (Vd), (Rt))
#define INSTR_CREATE_vdup_8(dc, Vd, Rt) \
    instr_create_1dst_1src((dc), OP_vdup_8, (Vd), (Rt))
#define INSTR_CREATE_vmov_g2s(dc, Vd, Rt) \
    instr_create_1dst_1src((dc), OP_vmov, (Vd), (Rt))
/** @} */ /* end doxygen group */

/** @name Signature: (Ra, Rd, Vm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Ra The third source register opnd_t operand.
 * \param Rd The destination register opnd_t operand.
 * \param Vm The source SIMD register opnd_t operand.
 */
#define INSTR_CREATE_vmov_s2gg(dc, Ra, Rd, Vm) \
    instr_create_2dst_1src((dc), OP_vmov, (Ra), (Rd), (Vm))
/** @} */ /* end doxygen group */

/** @name Signature: (Vd, Vn, Vm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vd The destination SIMD register opnd_t operand.
 * \param Vn The source SIMD register opnd_t operand.
 * \param Vm The second source SIMD register opnd_t operand.
 */
#define INSTR_CREATE_sha1c_32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_sha1c_32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_sha1m_32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_sha1m_32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_sha1p_32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_sha1p_32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_sha1su0_32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_sha1su0_32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_sha256h2_32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_sha256h2_32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_sha256h_32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_sha256h_32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_sha256su1_32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_sha256su1_32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaba_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaba_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaba_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaba_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaba_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaba_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaba_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaba_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaba_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaba_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaba_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaba_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vabal_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vabal_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vabal_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vabal_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vabal_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vabal_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vabal_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vabal_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vabal_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vabal_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vabal_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vabal_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vabd_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vabd_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vabd_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vabd_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vabd_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vabd_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vabd_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vabd_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vabd_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vabd_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vabd_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vabd_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vabdl_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vabdl_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vabdl_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vabdl_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vabdl_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vabdl_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vabdl_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vabdl_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vabdl_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vabdl_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vabdl_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vabdl_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vacge_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vacge_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vacgt_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vacgt_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vadd_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vadd_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vadd_f64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vadd_f64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vadd_i16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vadd_i16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vadd_i32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vadd_i32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vadd_i64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vadd_i64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vadd_i8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vadd_i8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaddhn_i16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaddhn_i16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaddhn_i32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaddhn_i32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaddhn_i64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaddhn_i64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaddl_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaddl_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaddl_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaddl_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaddl_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaddl_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaddl_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaddl_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaddl_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaddl_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaddl_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaddl_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaddw_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaddw_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaddw_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaddw_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaddw_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaddw_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaddw_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaddw_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaddw_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaddw_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vaddw_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vaddw_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vand(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vand, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vbic(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vbic, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vbif(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vbif, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vbit(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vbit, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vbsl(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vbsl, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vcge_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vcge_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vcge_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vcge_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vcge_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vcge_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vcgt_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vcgt_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vcgt_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vcgt_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vcgt_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vcgt_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vdiv_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vdiv_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vdiv_f64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vdiv_f64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_veor(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_veor, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vfma_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vfma_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vfma_f64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vfma_f64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vfms_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vfms_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vfms_f64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vfms_f64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vfnma_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vfnma_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vfnma_f64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vfnma_f64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vfnms_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vfnms_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vfnms_f64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vfnms_f64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vhadd_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vhadd_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vhadd_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vhadd_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vhadd_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vhadd_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vhadd_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vhadd_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vhadd_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vhadd_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vhadd_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vhadd_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vhsub_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vhsub_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vhsub_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vhsub_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vhsub_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vhsub_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vhsub_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vhsub_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vhsub_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vhsub_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vhsub_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vhsub_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmax_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmax_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmax_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmax_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmax_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmax_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmax_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmax_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmax_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmax_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmax_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmax_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmax_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmax_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmaxnm_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmaxnm_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmaxnm_f64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmaxnm_f64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmin_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmin_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmin_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmin_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmin_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmin_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmin_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmin_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmin_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmin_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmin_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmin_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmin_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmin_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vminnm_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vminnm_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vminnm_f64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vminnm_f64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmla_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmla_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmla_f64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmla_f64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmla_i16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmla_i16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmla_i32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmla_i32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmla_i8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmla_i8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmlal_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmlal_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmlal_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmlal_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmlal_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmlal_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmlal_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmlal_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmlal_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmlal_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmlal_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmlal_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmls_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmls_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmls_f64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmls_f64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmls_i16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmls_i16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmls_i32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmls_i32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmls_i8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmls_i8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmlsl_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmlsl_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmlsl_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmlsl_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmlsl_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmlsl_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmlsl_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmlsl_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmlsl_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmlsl_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmlsl_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmlsl_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmul_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmul_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmul_f64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmul_f64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmul_i16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmul_i16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmul_i32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmul_i32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmul_i8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmul_i8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmul_p32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmul_p32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmul_p8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmul_p8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmull_p32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmull_p32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmull_p8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmull_p8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmull_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmull_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmull_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmull_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmull_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmull_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmull_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmull_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmull_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmull_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vmull_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vmull_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vnmla_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vnmla_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vnmla_f64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vnmla_f64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vnmls_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vnmls_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vnmls_f64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vnmls_f64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vnmul_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vnmul_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vnmul_f64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vnmul_f64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vorn(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vorn, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vorr(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vorr, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vpadd_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vpadd_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vpadd_i16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vpadd_i16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vpadd_i32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vpadd_i32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vpadd_i8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vpadd_i8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vpmax_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vpmax_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vpmax_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vpmax_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vpmax_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vpmax_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vpmax_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vpmax_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vpmax_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vpmax_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vpmax_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vpmax_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vpmax_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vpmax_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vpmin_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vpmin_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vpmin_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vpmin_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vpmin_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vpmin_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vpmin_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vpmin_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vpmin_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vpmin_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vpmin_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vpmin_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vpmin_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vpmin_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqadd_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqadd_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqadd_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqadd_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqadd_s64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqadd_s64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqadd_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqadd_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqadd_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqadd_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqadd_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqadd_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqadd_u64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqadd_u64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqadd_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqadd_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqdmlal_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqdmlal_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqdmlal_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqdmlal_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqdmlsl_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqdmlsl_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqdmlsl_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqdmlsl_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqdmulh_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqdmulh_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqdmulh_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqdmulh_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqdmull_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqdmull_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqdmull_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqdmull_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqrdmulh_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqrdmulh_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqrdmulh_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqrdmulh_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqrshl_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqrshl_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqrshl_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqrshl_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqrshl_s64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqrshl_s64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqrshl_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqrshl_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqrshl_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqrshl_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqrshl_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqrshl_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqrshl_u64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqrshl_u64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqrshl_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqrshl_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqsub_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqsub_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqsub_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqsub_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqsub_s64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqsub_s64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqsub_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqsub_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqsub_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqsub_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqsub_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqsub_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqsub_u64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqsub_u64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vqsub_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vqsub_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vraddhn_i16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vraddhn_i16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vraddhn_i32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vraddhn_i32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vraddhn_i64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vraddhn_i64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrecps_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrecps_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrhadd_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrhadd_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrhadd_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrhadd_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrhadd_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrhadd_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrhadd_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrhadd_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrhadd_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrhadd_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrhadd_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrhadd_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrshl_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrshl_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrshl_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrshl_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrshl_s64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrshl_s64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrshl_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrshl_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrshl_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrshl_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrshl_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrshl_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrshl_u64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrshl_u64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrshl_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrshl_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrsqrts_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrsqrts_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrsubhn_i16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrsubhn_i16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrsubhn_i32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrsubhn_i32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vrsubhn_i64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vrsubhn_i64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vshl_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vshl_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vshl_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vshl_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vshl_s64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vshl_s64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vshl_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vshl_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vshl_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vshl_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vshl_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vshl_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vshl_u64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vshl_u64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vshl_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vshl_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsub_f32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsub_f32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsub_f64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsub_f64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsub_i16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsub_i16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsub_i32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsub_i32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsub_i64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsub_i64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsub_i8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsub_i8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsubhn_i16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsubhn_i16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsubhn_i32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsubhn_i32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsubhn_i64(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsubhn_i64, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsubl_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsubl_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsubl_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsubl_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsubl_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsubl_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsubl_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsubl_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsubl_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsubl_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsubl_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsubl_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsubw_s16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsubw_s16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsubw_s32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsubw_s32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsubw_s8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsubw_s8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsubw_u16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsubw_u16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsubw_u32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsubw_u32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vsubw_u8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vsubw_u8, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vtst_16(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vtst_16, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vtst_32(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vtst_32, (Vd), (Vn), (Vm))
#define INSTR_CREATE_vtst_8(dc, Vd, Vn, Vm) \
    instr_create_1dst_2src((dc), OP_vtst_8, (Vd), (Vn), (Vm))
/** @} */ /* end doxygen group */

/** @name Signature: (Vd, Rt, Rt2) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vd The destination SIMD register opnd_t operand.
 * \param Rt The source register opnd_t operand.
 * \param Rt2 The second source register opnd_t operand.
 */
#define INSTR_CREATE_vmov_gg2s(dc, Vd, Rt, Rt2) \
    instr_create_1dst_2src((dc), OP_vmov, (Vd), (Rt), (Rt2))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Rd2, Vt, Vt2) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Rd2 The second destination register opnd_t operand.
 * \param Vt The source SIMD register opnd_t operand.
 * \param Vt2 The second source SIMD register opnd_t operand.
 */
#define INSTR_CREATE_vmov_ss2gg(dc, Rd, Rd2, Vt, Vt2) \
    instr_create_2dst_2src((dc), OP_vmov, (Rd), (Rd2), (Vt), (Vt2))
/** @} */ /* end doxygen group */

/** @name Signature: (Vd, Vd2, Rt, Rt2) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vd The destination SIMD register opnd_t operand.
 * \param Vd2 The second destination register opnd_t operand.
 * \param Rt The source register opnd_t operand.
 * \param Rt2 The second source register opnd_t operand.
 */
#define INSTR_CREATE_vmov_gg2ss(dc, Vd, Vd2, Rt, Rt2) \
    instr_create_2dst_2src((dc), OP_vmov, (Vd), (Vd2), (Rt), (Rt2))
/** @} */ /* end doxygen group */

/** @name Signature: (Vd, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vd The destination SIMD register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_vbic_i16(dc, Vd, imm) \
    instr_create_1dst_1src((dc), OP_vbic_i16, (Vd), (imm))
#define INSTR_CREATE_vbic_i32(dc, Vd, imm) \
    instr_create_1dst_1src((dc), OP_vbic_i32, (Vd), (imm))
#define INSTR_CREATE_vmov_i16(dc, Vd, imm) \
    instr_create_1dst_1src((dc), OP_vmov_i16, (Vd), (imm))
#define INSTR_CREATE_vmov_i32(dc, Vd, imm) \
    instr_create_1dst_1src((dc), OP_vmov_i32, (Vd), (imm))
#define INSTR_CREATE_vmov_i64(dc, Vd, imm) \
    instr_create_1dst_1src((dc), OP_vmov_i64, (Vd), (imm))
#define INSTR_CREATE_vmov_i8(dc, Vd, imm) \
    instr_create_1dst_1src((dc), OP_vmov_i8, (Vd), (imm))
#define INSTR_CREATE_vmvn_i16(dc, Vd, imm) \
    instr_create_1dst_1src((dc), OP_vmvn_i16, (Vd), (imm))
#define INSTR_CREATE_vmvn_i32(dc, Vd, imm) \
    instr_create_1dst_1src((dc), OP_vmvn_i32, (Vd), (imm))
#define INSTR_CREATE_vorr_i16(dc, Vd, imm) \
    instr_create_1dst_1src((dc), OP_vorr_i16, (Vd), (imm))
#define INSTR_CREATE_vorr_i32(dc, Vd, imm) \
    instr_create_1dst_1src((dc), OP_vorr_i32, (Vd), (imm))
/** @} */ /* end doxygen group */

/** @name Signature: (Vd, Vm_or_imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vd The destination SIMD register opnd_t operand.
 * \param Vm_or_imm The source SIMD register, or integer constant, opnd_t operand.
 */
#define INSTR_CREATE_vmov_f32(dc, Vd, Vm_or_imm) \
    instr_create_1dst_1src((dc), OP_vmov_f32, (Vd), (Vm_or_imm))
#define INSTR_CREATE_vmov_f64(dc, Vd, Vm_or_imm) \
    instr_create_1dst_1src((dc), OP_vmov_f64, (Vd), (Vm_or_imm))
/** @} */ /* end doxygen group */

/** @name Signature: (Vt, Vm_or_imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vt The source SIMD register opnd_t operand.
 * \param Vm_or_imm The source SIMD register, or integer constant, opnd_t operand.
 */
#define INSTR_CREATE_vcmp_f32(dc, Vt, Vm_or_imm)                                   \
    instr_create_1dst_2src((dc), OP_vcmp_f32, opnd_create_reg(DR_REG_FPSCR), (Vt), \
                           (Vm_or_imm))
#define INSTR_CREATE_vcmp_f64(dc, Vt, Vm_or_imm)                                   \
    instr_create_1dst_2src((dc), OP_vcmp_f64, opnd_create_reg(DR_REG_FPSCR), (Vt), \
                           (Vm_or_imm))
#define INSTR_CREATE_vcmpe_f32(dc, Vt, Vm_or_imm)                                   \
    instr_create_1dst_2src((dc), OP_vcmpe_f32, opnd_create_reg(DR_REG_FPSCR), (Vt), \
                           (Vm_or_imm))
#define INSTR_CREATE_vcmpe_f64(dc, Vt, Vm_or_imm)                                   \
    instr_create_1dst_2src((dc), OP_vcmpe_f64, opnd_create_reg(DR_REG_FPSCR), (Vt), \
                           (Vm_or_imm))
/** @} */ /* end doxygen group */

/** @name Signature: (Rd, Vn, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Rd The destination register opnd_t operand.
 * \param Vn The source SIMD register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_vmov_32_s2g(dc, Rd, Vn, imm) \
    instr_create_1dst_2src((dc), OP_vmov_32, (Rd), (Vn), (imm))
#define INSTR_CREATE_vmov_s16(dc, Rd, Vn, imm) \
    instr_create_1dst_2src((dc), OP_vmov_s16, (Rd), (Vn), (imm))
#define INSTR_CREATE_vmov_s8(dc, Rd, Vn, imm) \
    instr_create_1dst_2src((dc), OP_vmov_s8, (Rd), (Vn), (imm))
#define INSTR_CREATE_vmov_u16(dc, Rd, Vn, imm) \
    instr_create_1dst_2src((dc), OP_vmov_u16, (Rd), (Vn), (imm))
#define INSTR_CREATE_vmov_u8(dc, Rd, Vn, imm) \
    instr_create_1dst_2src((dc), OP_vmov_u8, (Rd), (Vn), (imm))
/** @} */ /* end doxygen group */

/** @name Signature: (Vd, Vm, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vd The destination SIMD register opnd_t operand.
 * \param Vm The source SIMD register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_vcle_f32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcle_f32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcle_s16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcle_s16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcle_s32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcle_s32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcle_s8(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcle_s8, (Vd), (Vm), (imm))
#define INSTR_CREATE_vclt_f32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vclt_f32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vclt_s16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vclt_s16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vclt_s32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vclt_s32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vclt_s8(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vclt_s8, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcvt_f32_s16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcvt_f32_s16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcvt_f32_s32_imm(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcvt_f32_s32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcvt_f32_u16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcvt_f32_u16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcvt_f32_u32_imm(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcvt_f32_u32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcvt_f64_s16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcvt_f64_s16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcvt_f64_s32_imm(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcvt_f64_s32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcvt_f64_u16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcvt_f64_u16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcvt_f64_u32_imm(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcvt_f64_u32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcvt_s16_f32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcvt_s16_f32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcvt_s16_f64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcvt_s16_f64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcvt_s32_f32_imm(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcvt_s32_f32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcvt_s32_f64_imm(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcvt_s32_f64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcvt_u16_f32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcvt_u16_f32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcvt_u16_f64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcvt_u16_f64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcvt_u32_f32_imm(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcvt_u32_f32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vcvt_u32_f64_imm(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vcvt_u32_f64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vdup_16_imm(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vdup_16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vdup_32_imm(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vdup_32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vdup_8_imm(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vdup_8, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqrshrn_s16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqrshrn_s16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqrshrn_s32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqrshrn_s32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqrshrn_s64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqrshrn_s64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqrshrn_u16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqrshrn_u16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqrshrn_u32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqrshrn_u32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqrshrn_u64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqrshrn_u64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqrshrun_s16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqrshrun_s16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqrshrun_s32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqrshrun_s32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqrshrun_s64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqrshrun_s64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqshlu_s16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqshlu_s16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqshlu_s32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqshlu_s32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqshlu_s64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqshlu_s64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqshlu_s8(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqshlu_s8, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqshrn_s16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqshrn_s16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqshrn_s32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqshrn_s32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqshrn_s64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqshrn_s64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqshrn_u16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqshrn_u16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqshrn_u32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqshrn_u32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqshrn_u64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqshrn_u64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqshrun_s16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqshrun_s16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqshrun_s32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqshrun_s32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vqshrun_s64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vqshrun_s64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrshr_s16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrshr_s16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrshr_s32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrshr_s32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrshr_s64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrshr_s64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrshr_s8(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrshr_s8, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrshr_u16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrshr_u16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrshr_u32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrshr_u32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrshr_u64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrshr_u64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrshr_u8(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrshr_u8, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrshrn_i16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrshrn_i16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrshrn_i32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrshrn_i32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrshrn_i64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrshrn_i64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrsra_s16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrsra_s16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrsra_s32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrsra_s32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrsra_s64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrsra_s64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrsra_s8(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrsra_s8, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrsra_u16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrsra_u16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrsra_u32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrsra_u32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrsra_u64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrsra_u64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vrsra_u8(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vrsra_u8, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshl_i16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshl_i16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshl_i32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshl_i32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshl_i64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshl_i64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshl_i8(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshl_i8, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshll_i16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshll_i16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshll_i32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshll_i32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshll_i8(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshll_i8, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshll_s16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshll_s16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshll_s32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshll_s32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshll_s8(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshll_s8, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshll_u16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshll_u16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshll_u32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshll_u32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshll_u8(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshll_u8, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshr_s16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshr_s16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshr_s32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshr_s32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshr_s64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshr_s64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshr_s8(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshr_s8, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshr_u16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshr_u16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshr_u32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshr_u32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshr_u64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshr_u64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshr_u8(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshr_u8, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshrn_i16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshrn_i16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshrn_i32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshrn_i32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vshrn_i64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vshrn_i64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vsli_16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vsli_16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vsli_32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vsli_32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vsli_64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vsli_64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vsli_8(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vsli_8, (Vd), (Vm), (imm))
#define INSTR_CREATE_vsra_s16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vsra_s16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vsra_s32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vsra_s32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vsra_s64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vsra_s64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vsra_s8(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vsra_s8, (Vd), (Vm), (imm))
#define INSTR_CREATE_vsra_u16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vsra_u16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vsra_u32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vsra_u32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vsra_u64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vsra_u64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vsra_u8(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vsra_u8, (Vd), (Vm), (imm))
#define INSTR_CREATE_vsri_16(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vsri_16, (Vd), (Vm), (imm))
#define INSTR_CREATE_vsri_32(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vsri_32, (Vd), (Vm), (imm))
#define INSTR_CREATE_vsri_64(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vsri_64, (Vd), (Vm), (imm))
#define INSTR_CREATE_vsri_8(dc, Vd, Vm, imm) \
    instr_create_1dst_2src((dc), OP_vsri_8, (Vd), (Vm), (imm))
/** @} */ /* end doxygen group */

/** @name Signature: (Vd, Rt, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vd The destination SIMD register opnd_t operand.
 * \param Rt The source register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_vmov_16(dc, Vd, Rt, imm) \
    instr_create_1dst_2src((dc), OP_vmov_16, (Vd), (Rt), (imm))
#define INSTR_CREATE_vmov_32_g2s(dc, Vd, Rt, imm) \
    instr_create_1dst_2src((dc), OP_vmov_32, (Vd), (Rt), (imm))
#define INSTR_CREATE_vmov_8(dc, Vd, Rt, imm) \
    instr_create_1dst_2src((dc), OP_vmov_8, (Vd), (Rt), (imm))
/** @} */ /* end doxygen group */

/** @name Signature: (Vd, Vn, Vm_or_imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vd The destination SIMD register opnd_t operand.
 * \param Vn The source SIMD register opnd_t operand.
 * \param Vm_or_imm The source SIMD register, or integer constant, opnd_t operand.
 */
#define INSTR_CREATE_vceq_f32(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vceq_f32, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vceq_i16(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vceq_i16, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vceq_i32(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vceq_i32, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vceq_i8(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vceq_i8, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vcge_f32(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vcge_f32, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vcge_s16(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vcge_s16, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vcge_s32(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vcge_s32, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vcge_s8(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vcge_s8, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vcgt_f32(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vcgt_f32, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vcgt_s16(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vcgt_s16, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vcgt_s32(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vcgt_s32, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vcgt_s8(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vcgt_s8, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vqshl_s16(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vqshl_s16, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vqshl_s32(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vqshl_s32, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vqshl_s64(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vqshl_s64, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vqshl_s8(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vqshl_s8, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vqshl_u16(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vqshl_u16, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vqshl_u32(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vqshl_u32, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vqshl_u64(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vqshl_u64, (Vd), (Vn), (Vm_or_imm))
#define INSTR_CREATE_vqshl_u8(dc, Vd, Vn, Vm_or_imm) \
    instr_create_1dst_2src((dc), OP_vqshl_u8, (Vd), (Vn), (Vm_or_imm))
/** @} */ /* end doxygen group */

/** @name Signature: (Vd, Vn, Vm, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vd The destination SIMD register opnd_t operand.
 * \param Vn The source SIMD register opnd_t operand.
 * \param Vm The second source SIMD register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_vext(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vext, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmla_f32_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmla_f32, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmla_i16_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmla_i16, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmla_i32_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmla_i32, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmlal_s16_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmlal_s16, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmlal_s32_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmlal_s32, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmlal_u16_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmlal_u16, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmlal_u32_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmlal_u32, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmls_f32_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmls_f32, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmls_i16_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmls_i16, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmls_i32_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmls_i32, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmlsl_s16_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmlsl_s16, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmlsl_s32_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmlsl_s32, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmlsl_u16_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmlsl_u16, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmlsl_u32_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmlsl_u32, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmul_f32_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmul_f32, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmul_i16_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmul_i16, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmul_i32_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmul_i32, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmull_s16_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmull_s16, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmull_s32_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmull_s32, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmull_u16_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmull_u16, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vmull_u32_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vmull_u32, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vqdmlal_s16_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vqdmlal_s16, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vqdmlal_s32_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vqdmlal_s32, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vqdmlsl_s16_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vqdmlsl_s16, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vqdmlsl_s32_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vqdmlsl_s32, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vqdmulh_s16_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vqdmulh_s16, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vqdmulh_s32_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vqdmulh_s32, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vqdmull_s16_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vqdmull_s16, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vqdmull_s32_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vqdmull_s32, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vqrdmulh_s16_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vqrdmulh_s16, (Vd), (Vn), (Vm), (imm))
#define INSTR_CREATE_vqrdmulh_s32_imm(dc, Vd, Vn, Vm, imm) \
    instr_create_1dst_3src((dc), OP_vqrdmulh_s32, (Vd), (Vn), (Vm), (imm))
/** @} */ /* end doxygen group */

/** @name Signature: (Vd, imm, Vn, Vm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vd The destination SIMD register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param Vn The source SIMD register opnd_t operand.
 * \param Vm The second source SIMD register opnd_t operand.
 */
#define INSTR_CREATE_vsel_eq_f32(dc, Vd, imm, Vn, Vm) \
    instr_create_1dst_3src((dc), OP_vsel_eq_f32, (Vd), (imm), (Vn), (Vm))
#define INSTR_CREATE_vsel_eq_f64(dc, Vd, imm, Vn, Vm) \
    instr_create_1dst_3src((dc), OP_vsel_eq_f64, (Vd), (imm), (Vn), (Vm))
#define INSTR_CREATE_vsel_ge_f32(dc, Vd, imm, Vn, Vm) \
    instr_create_1dst_3src((dc), OP_vsel_ge_f32, (Vd), (imm), (Vn), (Vm))
#define INSTR_CREATE_vsel_ge_f64(dc, Vd, imm, Vn, Vm) \
    instr_create_1dst_3src((dc), OP_vsel_ge_f64, (Vd), (imm), (Vn), (Vm))
#define INSTR_CREATE_vsel_gt_f32(dc, Vd, imm, Vn, Vm) \
    instr_create_1dst_3src((dc), OP_vsel_gt_f32, (Vd), (imm), (Vn), (Vm))
#define INSTR_CREATE_vsel_gt_f64(dc, Vd, imm, Vn, Vm) \
    instr_create_1dst_3src((dc), OP_vsel_gt_f64, (Vd), (imm), (Vn), (Vm))
#define INSTR_CREATE_vsel_vs_f32(dc, Vd, imm, Vn, Vm) \
    instr_create_1dst_3src((dc), OP_vsel_vs_f32, (Vd), (imm), (Vn), (Vm))
#define INSTR_CREATE_vsel_vs_f64(dc, Vd, imm, Vn, Vm) \
    instr_create_1dst_3src((dc), OP_vsel_vs_f64, (Vd), (imm), (Vn), (Vm))
/** @} */ /* end doxygen group */

/** @name Signature: (Vd, mem) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vd The destination SIMD register opnd_t operand.
 * \param mem The memory opnd_t operand.
 */
#define INSTR_CREATE_vldr(dc, Vd, mem) instr_create_1dst_1src((dc), OP_vldr, (Vd), (mem))
/** @} */ /* end doxygen group */

/** @name Signature: (mem, Vt) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param Vt The source SIMD register opnd_t operand.
 */
#define INSTR_CREATE_vstr(dc, mem, Vt) instr_create_1dst_1src((dc), OP_vstr, (mem), (Vt))
/** @} */ /* end doxygen group */

/** @name Signature: (Vd, mem, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vd The destination SIMD register opnd_t operand.
 * \param mem The memory opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_vld1_lane_8(dc, Vd, mem, imm) \
    instr_create_1dst_2src((dc), OP_vld1_lane_8, (Vd), (mem), (imm))
#define INSTR_CREATE_vld1_lane_8_wbimm(dc, Vd, mem, imm)                      \
    instr_create_2dst_3src((dc), OP_vld1_lane_8, (Vd),                        \
                           opnd_create_reg(opnd_get_base(mem)), (mem), (imm), \
                           opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (mem, Vt, imm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param Vt The source SIMD register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 */
#define INSTR_CREATE_vst1_lane_8(dc, mem, Vt, imm) \
    instr_create_1dst_2src((dc), OP_vst1_lane_8, (mem), (Vt), (imm))
#define INSTR_CREATE_vst1_lane_8_wbimm(dc, mem, Vt, imm)                     \
    instr_create_2dst_3src((dc), OP_vst1_lane_8, (mem),                      \
                           opnd_create_reg(opnd_get_base(mem)), (Vt), (imm), \
                           opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (Vd, mem, imm, Rm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vd The destination SIMD register opnd_t operand.
 * \param mem The memory opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param Rm The source register opnd_t operand.
 */
#define INSTR_CREATE_vld1_lane_8_wbreg(dc, Vd, mem, imm, Rm)                         \
    instr_create_2dst_4src((dc), OP_vld1_lane_8, (Vd),                               \
                           opnd_create_reg(opnd_get_base(mem)), (mem), (imm),        \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (Vd, mem, imm, imm2) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vd The destination SIMD register opnd_t operand.
 * \param mem The memory opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 */
#define INSTR_CREATE_vld1_lane_16(dc, Vd, mem, imm, imm2) \
    instr_create_1dst_3src((dc), OP_vld1_lane_16, (Vd), (mem), (imm), (imm2))
#define INSTR_CREATE_vld1_lane_16_wbimm(dc, Vd, mem, imm, imm2)                       \
    instr_create_2dst_4src((dc), OP_vld1_lane_16, (Vd),                               \
                           opnd_create_reg(opnd_get_base(mem)), (mem), (imm), (imm2), \
                           opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_vld1_lane_32(dc, Vd, mem, imm, imm2) \
    instr_create_1dst_3src((dc), OP_vld1_lane_32, (Vd), (mem), (imm), (imm2))
#define INSTR_CREATE_vld1_lane_32_wbimm(dc, Vd, mem, imm, imm2)                       \
    instr_create_2dst_4src((dc), OP_vld1_lane_32, (Vd),                               \
                           opnd_create_reg(opnd_get_base(mem)), (mem), (imm), (imm2), \
                           opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (mem, Vt, imm, Rm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param Vt The source SIMD register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param Rm The source register opnd_t operand.
 */
#define INSTR_CREATE_vst1_lane_8_wbreg(dc, mem, Vt, imm, Rm)                         \
    instr_create_2dst_4src((dc), OP_vst1_lane_8, (mem),                              \
                           opnd_create_reg(opnd_get_base(mem)), (Vt), (imm),         \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (mem, Vt, imm, imm2) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param Vt The source SIMD register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 */
#define INSTR_CREATE_vst1_lane_16(dc, mem, Vt, imm, imm2) \
    instr_create_1dst_3src((dc), OP_vst1_lane_16, (mem), (Vt), (imm), (imm2))
#define INSTR_CREATE_vst1_lane_16_wbimm(dc, mem, Vt, imm, imm2)                      \
    instr_create_2dst_4src((dc), OP_vst1_lane_16, (mem),                             \
                           opnd_create_reg(opnd_get_base(mem)), (Vt), (imm), (imm2), \
                           opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_vst1_lane_32(dc, mem, Vt, imm, imm2) \
    instr_create_1dst_3src((dc), OP_vst1_lane_32, (mem), (Vt), (imm), (imm2))
#define INSTR_CREATE_vst1_lane_32_wbimm(dc, mem, Vt, imm, imm2)                      \
    instr_create_2dst_4src((dc), OP_vst1_lane_32, (mem),                             \
                           opnd_create_reg(opnd_get_base(mem)), (Vt), (imm), (imm2), \
                           opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (Vd, mem, imm, imm2, Rm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vd The destination SIMD register opnd_t operand.
 * \param mem The memory opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 * \param Rm The source register opnd_t operand.
 */
#define INSTR_CREATE_vld1_lane_16_wbreg(dc, Vd, mem, imm, imm2, Rm)                   \
    instr_create_2dst_5src((dc), OP_vld1_lane_16, (Vd),                               \
                           opnd_create_reg(opnd_get_base(mem)), (mem), (imm), (imm2), \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),  \
                           opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_vld1_lane_32_wbreg(dc, Vd, mem, imm, imm2, Rm)                   \
    instr_create_2dst_5src((dc), OP_vld1_lane_32, (Vd),                               \
                           opnd_create_reg(opnd_get_base(mem)), (mem), (imm), (imm2), \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED),  \
                           opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (mem, Vt, imm, imm2, Rm) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param mem The memory opnd_t operand.
 * \param Vt The source SIMD register opnd_t operand.
 * \param imm The integer constant opnd_t operand.
 * \param imm2 The second integer constant opnd_t operand.
 * \param Rm The source register opnd_t operand.
 */
#define INSTR_CREATE_vst1_lane_16_wbreg(dc, mem, Vt, imm, imm2, Rm)                  \
    instr_create_2dst_5src((dc), OP_vst1_lane_16, (mem),                             \
                           opnd_create_reg(opnd_get_base(mem)), (Vt), (imm), (imm2), \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_create_reg(opnd_get_base(mem)))
#define INSTR_CREATE_vst1_lane_32_wbreg(dc, mem, Vt, imm, imm2, Rm)                  \
    instr_create_2dst_5src((dc), OP_vst1_lane_32, (mem),                             \
                           opnd_create_reg(opnd_get_base(mem)), (Vt), (imm), (imm2), \
                           opnd_create_reg_ex(opnd_get_reg(Rm), 0, DR_OPND_SHIFTED), \
                           opnd_create_reg(opnd_get_base(mem)))
/** @} */ /* end doxygen group */

/** @name Signature: (Vd, Vm, list_len, ...) */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param Vd The destination SIMD register opnd_t operand.
 * \param Vm The source SIMD register opnd_t operand.
 * \param list_len The number of registers in the register list.
 * \param ... The register list as separate opnd_t arguments.
 * The registers in the list must be in increasing order.
 */
#define INSTR_CREATE_vtbl_8(dc, Vd, Vm, list_len, ...)                            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vtbl_8, 1, 1, list_len, 0, (Vd), (Vm), \
                                  __VA_ARGS__)
#define INSTR_CREATE_vtbx_8(dc, Vd, Vm, list_len, ...)                            \
    instr_create_Ndst_Msrc_varsrc((dc), OP_vtbx_8, 1, 1, list_len, 0, (Vd), (Vm), \
                                  __VA_ARGS__)
/** @} */ /* end doxygen group */

#endif /* _DR_IR_MACROS_ARM_H_ */
