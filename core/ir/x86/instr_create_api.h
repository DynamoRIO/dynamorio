/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
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

#ifndef _DR_IR_MACROS_X86_H_
#define _DR_IR_MACROS_X86_H_ 1

/**
 * @file dr_ir_macros_x86.h
 * @brief AMD64/IA-32 instruction creation convenience macros.
 *
 * All macros assume default data and address sizes.  For the most part these
 * macros do not support building non-default address or data size
 * versions; for that, simply duplicate the macro's body, replacing the
 * size and/or hardcoded registers with smaller versions (the IR does
 * not support cs segments with non-default sizes where the default
 * size requires instruction prefixes).  For shrinking data sizes, see
 * the instr_shrink_to_16_bits() routine.
 */

#include <math.h> /* for floating-point math constants */

/* instruction modification convenience routines */
/**
 * Add the lock prefix to an instruction. For example:
 * instr_t *lock_inc_instr = LOCK(INSTR_CREATE_inc(....));
 */
#define LOCK(instr_ptr) instr_set_prefix_flag((instr_ptr), PREFIX_LOCK)

#ifdef X64
/**
 * Create an absolute address operand encoded as pc-relative.
 * Encoding will fail if addr is out of 32-bit-signed-displacement reach.
 */
#    define OPND_CREATE_ABSMEM(addr, size) opnd_create_rel_addr(addr, size)
#else
/** Create an absolute address operand. */
#    define OPND_CREATE_ABSMEM(addr, size) opnd_create_abs_addr(addr, size)
#endif

/* operand convenience routines for specific opcodes with odd sizes */
/** Create a memory reference operand appropriately sized for OP_lea. */
#define OPND_CREATE_MEM_lea(base, index, scale, disp) \
    opnd_create_base_disp(base, index, scale, disp, OPSZ_lea)
/** Create a memory reference operand appropriately sized for OP_invlpg. */
#define OPND_CREATE_MEM_invlpg(base, index, scale, disp) \
    opnd_create_base_disp(base, index, scale, disp, OPSZ_invlpg)
/** Create a memory reference operand appropriately sized for OP_clflush. */
#define OPND_CREATE_MEM_clflush(base, index, scale, disp) \
    opnd_create_base_disp(base, index, scale, disp, OPSZ_clflush)
/** Create a memory reference operand appropriately sized for OP_prefetch*. */
#define OPND_CREATE_MEM_prefetch(base, index, scale, disp) \
    opnd_create_base_disp(base, index, scale, disp, OPSZ_prefetch)
/** Create a memory reference operand appropriately sized for OP_lgdt. */
#define OPND_CREATE_MEM_lgdt(base, index, scale, disp) \
    opnd_create_base_disp(base, index, scale, disp, OPSZ_lgdt)
/** Create a memory reference operand appropriately sized for OP_sgdt. */
#define OPND_CREATE_MEM_sgdt(base, index, scale, disp) \
    opnd_create_base_disp(base, index, scale, disp, OPSZ_sgdt)
/** Create a memory reference operand appropriately sized for OP_lidt. */
#define OPND_CREATE_MEM_lidt(base, index, scale, disp) \
    opnd_create_base_disp(base, index, scale, disp, OPSZ_lidt)
/** Create a memory reference operand appropriately sized for OP_sidt. */
#define OPND_CREATE_MEM_sidt(base, index, scale, disp) \
    opnd_create_base_disp(base, index, scale, disp, OPSZ_sidt)
/** Create a memory reference operand appropriately sized for OP_bound. */
#define OPND_CREATE_MEM_bound(base, index, scale, disp) \
    opnd_create_base_disp(base, index, scale, disp, OPSZ_bound)
/** Create a memory reference operand appropriately sized for OP_fldenv. */
#define OPND_CREATE_MEM_fldenv(base, index, scale, disp) \
    opnd_create_base_disp(base, index, scale, disp, OPSZ_fldenv)
/** Create a memory reference operand appropriately sized for OP_fnstenv. */
#define OPND_CREATE_MEM_fnstenv(base, index, scale, disp) \
    opnd_create_base_disp(base, index, scale, disp, OPSZ_fnstenv)
/** Create a memory reference operand appropriately sized for OP_fnsave. */
#define OPND_CREATE_MEM_fnsave(base, index, scale, disp) \
    opnd_create_base_disp(base, index, scale, disp, OPSZ_fnsave)
/** Create a memory reference operand appropriately sized for OP_frstor. */
#define OPND_CREATE_MEM_frstor(base, index, scale, disp) \
    opnd_create_base_disp(base, index, scale, disp, OPSZ_frstor)
/** Create a memory reference operand appropriately sized for OP_fxsave32/OP_fxsave64. */
#define OPND_CREATE_MEM_fxsave(base, index, scale, disp) \
    opnd_create_base_disp(base, index, scale, disp, OPSZ_fxsave)
/** Create a memory reference operand appropriately sized for OP_fxrstor32/OP_fxrstor64.*/
#define OPND_CREATE_MEM_fxrstor(base, index, scale, disp) \
    opnd_create_base_disp(base, index, scale, disp, OPSZ_fxrstor)
/** Create a memory reference operand appropriately sized for OP_ptwrite.*/
#define OPND_CREATE_MEM_ptwrite(base, index, scale, disp) \
    opnd_create_base_disp(base, index, scale, disp, OPSZ_ptwrite)
/**
 * Create a memory reference operand appropriately sized for OP_xsave32,
 * OP_xsave64, OP_xsaveopt32, OP_xsaveopt64, OP_xsavec32, OP_xsavec64,
 * OP_xrstor32, or OP_xrstor64.
 */
#define OPND_CREATE_MEM_xsave(base, index, scale, disp) \
    opnd_create_base_disp(base, index, scale, disp, OPSZ_xsave)

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
 * Platform-independent INSTR_CREATE_* macros
 */
/** @name Platform-independent macros */
/** @{ */ /* doxygen start group */

/**
 * This platform-independent macro creates an instr_t for a debug trap
 * instruction, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define XINST_CREATE_debug_instr(dc) INSTR_CREATE_int3(dc)

/**
 * This platform-independent macro creates an instr_t for a 4-byte
 * or 8-byte (x64 only) memory load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param m   The source memory opnd.
 */
#define XINST_CREATE_load(dc, r, m) INSTR_CREATE_mov_ld(dc, r, m)

/**
 * This platform-independent macro creates an instr_t which loads 1 byte
 * from memory, zero-extends it to 4 bytes, and writes it to a 4 byte
 * destination register.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param m   The source memory opnd.
 */
#define XINST_CREATE_load_1byte_zext4(dc, r, m) INSTR_CREATE_movzx(dc, r, m)

/**
 * This platform-independent macro creates an instr_t for a 1-byte
 * memory load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param m   The source memory opnd.
 */
#define XINST_CREATE_load_1byte(dc, r, m) INSTR_CREATE_mov_ld(dc, r, m)

/**
 * This platform-independent macro creates an instr_t for a 2-byte
 * memory load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param m   The source memory opnd.
 */
#define XINST_CREATE_load_2bytes(dc, r, m) INSTR_CREATE_mov_ld(dc, r, m)

/**
 * This platform-independent macro creates an instr_t for a 4-byte
 * or 8-byte (x64 only) memory store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r   The source register opnd.
 */
#define XINST_CREATE_store(dc, m, r) INSTR_CREATE_mov_st(dc, m, r)

/**
 * This platform-independent macro creates an instr_t for a 1-byte
 * memory store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r   The source register opnd.
 */
#define XINST_CREATE_store_1byte(dc, m, r) INSTR_CREATE_mov_st(dc, m, r)

/**
 * This platform-independent macro creates an instr_t for a 2-byte
 * memory store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r   The source register opnd.
 */
#define XINST_CREATE_store_2bytes(dc, m, r) INSTR_CREATE_mov_st(dc, m, r)

/**
 * This platform-independent macro creates an instr_t for a register
 * to register move instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d   The destination register opnd.
 * \param s   The source register opnd.
 */
#define XINST_CREATE_move(dc, d, s) INSTR_CREATE_mov_ld(dc, d, s)

/**
 * This platform-independent macro creates an instr_t for a multimedia
 * register load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param m   The source memory opnd.
 */
#define XINST_CREATE_load_simd(dc, r, m) INSTR_CREATE_movd(dc, r, m)

/**
 * This platform-independent macro creates an instr_t for a multimedia
 * register store instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The destination memory opnd.
 * \param r   The source register opnd.
 */
#define XINST_CREATE_store_simd(dc, m, r) INSTR_CREATE_movd(dc, m, r)

/**
 * This platform-independent macro creates an instr_t for an indirect
 * jump through memory instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param m   The memory opnd holding the target.
 */
#define XINST_CREATE_jump_mem(dc, m) INSTR_CREATE_jmp_ind(dc, m)

/**
 * This platform-independent macro creates an instr_t for an indirect
 * jump instruction through a register.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The register opnd holding the target.
 */
#define XINST_CREATE_jump_reg(dc, r) INSTR_CREATE_jmp_ind(dc, r)

/**
 * This platform-independent macro creates an instr_t for an immediate
 * integer load instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param r   The destination register opnd.
 * \param i   The source immediate integer opnd.
 */
#define XINST_CREATE_load_int(dc, r, i) INSTR_CREATE_mov_imm(dc, r, i)

/**
 * This platform-independent macro creates an instr_t for a return instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 */
#define XINST_CREATE_return(dc) INSTR_CREATE_ret(dc)

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
#define XINST_CREATE_jump(dc, t) INSTR_CREATE_jmp((dc), (t))

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
#define XINST_CREATE_jump_short(dc, t) INSTR_CREATE_jmp_short((dc), (t))

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
    (INSTR_CREATE_jcc((dc), (pred)-DR_PRED_O + OP_jo, (t)))

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
#define XINST_CREATE_call(dc, t) INSTR_CREATE_call((dc), (t))

/**
 * This platform-independent macro creates an instr_t for an addition
 * instruction that does not affect the status flags.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.  This
 * can be either a register or a 32-bit immediate integer on x86.
 */
#define XINST_CREATE_add(dc, d, s)                                           \
    INSTR_CREATE_lea(                                                        \
        (dc), (d),                                                           \
        OPND_CREATE_MEM_lea(                                                 \
            opnd_get_reg(d), opnd_is_reg(s) ? opnd_get_reg(s) : DR_REG_NULL, \
            opnd_is_reg(s) ? 1 : 0, opnd_is_reg(s) ? 0 : (int)opnd_get_immed_int(s)))

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
#define XINST_CREATE_add_2src(dc, d, s1, s2)                                    \
    INSTR_CREATE_lea(                                                           \
        (dc), (d),                                                              \
        OPND_CREATE_MEM_lea(                                                    \
            opnd_get_reg(s1), opnd_is_reg(s2) ? opnd_get_reg(s2) : DR_REG_NULL, \
            opnd_is_reg(s2) ? 1 : 0, opnd_is_reg(s2) ? 0 : (int)opnd_get_immed_int(s2)))

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
#define XINST_CREATE_add_sll(dc, d, s1, s2_toshift, shift_amount)                      \
    INSTR_CREATE_lea(                                                                  \
        (dc), (d),                                                                     \
        OPND_CREATE_MEM_lea(                                                           \
            opnd_get_reg(s1), opnd_get_reg(s2_toshift),                                \
            ((shift_amount) == 0                                                       \
                 ? 1                                                                   \
                 : ((shift_amount) == 1                                                \
                        ? 2                                                            \
                        : ((shift_amount) == 2                                         \
                               ? 4                                                     \
                               : ((shift_amount) == 3                                  \
                                      ? 8                                              \
                                      : (DR_ASSERT_MSG(false, "invalid shift amount"), \
                                         0))))),                                       \
            0))

/**
 * This platform-independent macro creates an instr_t for an addition
 * instruction that does affect the status flags.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_add_s(dc, d, s) INSTR_CREATE_add((dc), (d), (s))

/**
 * This platform-independent macro creates an instr_t for a subtraction
 * instruction that does not affect the status flags.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 * The source must be an immediate integer on x86.
 */
#define XINST_CREATE_sub(dc, d, s)                                        \
    INSTR_CREATE_lea((dc), (d),                                           \
                     OPND_CREATE_MEM_lea(opnd_get_reg(d), DR_REG_NULL, 0, \
                                         -(int)opnd_get_immed_int(s)))

/**
 * This platform-independent macro creates an instr_t for a subtraction
 * instruction that does affect the status flags.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_sub_s(dc, d, s) INSTR_CREATE_sub((dc), (d), (s))

/**
 * This platform-independent macro creates an instr_t for a bitwise and
 * instruction that does affect the status flags.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_and_s(dc, d, s) INSTR_CREATE_and((dc), (d), (s))

/**
 * This platform-independent macro creates an instr_t for a logical right shift
 * instruction that does affect the status flags.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param d  The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_slr_s(dc, d, s) INSTR_CREATE_shr((dc), (d), (s))

/**
 * This platform-independent macro creates an instr_t for a comparison
 * instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param s1  The opnd_t explicit destination operand for the instruction.
 * \param s2  The opnd_t explicit source operand for the instruction.
 */
#define XINST_CREATE_cmp(dc, s1, s2) instr_create_0dst_2src((dc), OP_cmp, (s1), (s2))

/**
 * This platform-independent macro creates an instr_t for a software
 * interrupt instruction.
 * \param dc  The void * dcontext used to allocate memory for the instr_t.
 * \param i   The source integer constant opnd_t operand.
 */
#define XINST_CREATE_interrupt(dc, i) INSTR_CREATE_int(dc, i)

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
#define XINST_CREATE_call_reg(dc, r) INSTR_CREATE_call_ind(dc, r)
/** @} */ /* end doxygen group */

/****************************************************************************
 * x86-specific INSTR_CREATE_* macros
 */

/* no-operand instructions */
/** @name No-operand instructions */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx, automatically
 * supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define INSTR_CREATE_fwait(dc) instr_create_0dst_0src((dc), OP_fwait)
#define INSTR_CREATE_hlt(dc) instr_create_0dst_0src((dc), OP_hlt)
#define INSTR_CREATE_cmc(dc) instr_create_0dst_0src((dc), OP_cmc)
#define INSTR_CREATE_clc(dc) instr_create_0dst_0src((dc), OP_clc)
#define INSTR_CREATE_stc(dc) instr_create_0dst_0src((dc), OP_stc)
#define INSTR_CREATE_cli(dc) instr_create_0dst_0src((dc), OP_cli)
#define INSTR_CREATE_sti(dc) instr_create_0dst_0src((dc), OP_sti)
#define INSTR_CREATE_cld(dc) instr_create_0dst_0src((dc), OP_cld)
#define INSTR_CREATE_std(dc) instr_create_0dst_0src((dc), OP_std)
#define INSTR_CREATE_clts(dc) instr_create_0dst_0src((dc), OP_clts)
#define INSTR_CREATE_invd(dc) instr_create_0dst_0src((dc), OP_invd)
#define INSTR_CREATE_wbinvd(dc) instr_create_0dst_0src((dc), OP_wbinvd)
#define INSTR_CREATE_ud2(dc) instr_create_0dst_0src((dc), OP_ud2)
#define INSTR_CREATE_ud2a(dc) INSTR_CREATE_ud2((dc))
#define INSTR_CREATE_emms(dc) instr_create_0dst_0src((dc), OP_emms)
#define INSTR_CREATE_rsm(dc) instr_create_0dst_0src((dc), OP_rsm)
#define INSTR_CREATE_lfence(dc) instr_create_0dst_0src((dc), OP_lfence)
#define INSTR_CREATE_mfence(dc) instr_create_0dst_0src((dc), OP_mfence)
#define INSTR_CREATE_sfence(dc) instr_create_0dst_0src((dc), OP_sfence)
#define INSTR_CREATE_nop(dc) instr_create_0dst_0src((dc), OP_nop)
#define INSTR_CREATE_pause(dc) instr_create_0dst_0src((dc), OP_pause)
#define INSTR_CREATE_fnop(dc) instr_create_0dst_0src((dc), OP_fnop)
#define INSTR_CREATE_fdecstp(dc) instr_create_0dst_0src((dc), OP_fdecstp)
#define INSTR_CREATE_fincstp(dc) instr_create_0dst_0src((dc), OP_fincstp)
#define INSTR_CREATE_fnclex(dc) instr_create_0dst_0src((dc), OP_fnclex)
#define INSTR_CREATE_fninit(dc) instr_create_0dst_0src((dc), OP_fninit)
#define INSTR_CREATE_femms(dc) instr_create_0dst_0src((dc), OP_femms)
#define INSTR_CREATE_swapgs(dc) instr_create_0dst_0src((dc), OP_swapgs)
#define INSTR_CREATE_vmcall(dc) instr_create_0dst_0src((dc), OP_vmcall)
#define INSTR_CREATE_vmlaunch(dc) instr_create_0dst_0src((dc), OP_vmlaunch)
#define INSTR_CREATE_vmresume(dc) instr_create_0dst_0src((dc), OP_vmresume)
#define INSTR_CREATE_vmxoff(dc) instr_create_0dst_0src((dc), OP_vmxoff)
#define INSTR_CREATE_vmmcall(dc) instr_create_0dst_0src((dc), OP_vmmcall)
#define INSTR_CREATE_vmfunc(dc) instr_create_0dst_0src((dc), OP_vmfunc)
#define INSTR_CREATE_stgi(dc) instr_create_0dst_0src((dc), OP_stgi)
#define INSTR_CREATE_clgi(dc) instr_create_0dst_0src((dc), OP_clgi)
#define INSTR_CREATE_int3(dc) instr_create_0dst_0src((dc), OP_int3)
#define INSTR_CREATE_into(dc) instr_create_0dst_0src((dc), OP_into)
#define INSTR_CREATE_int1(dc) instr_create_0dst_0src((dc), OP_int1)
#define INSTR_CREATE_vzeroupper(dc) instr_create_0dst_0src((dc), OP_vzeroupper)
#define INSTR_CREATE_vzeroall(dc) instr_create_0dst_0src((dc), OP_vzeroall)
#define INSTR_CREATE_xtest(dc) instr_create_0dst_0src((dc), OP_xtest)
/** @} */ /* end doxygen group */

/* no destination, 1 source */
/**
 * Creates an instr_t for a short conditional branch instruction with the given
 * opcode and target operand.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param op The OP_xxx opcode for the conditional branch, which should be
 * in the range [OP_jo_short, OP_jnle_short].
 * \param t The opnd_t target operand for the instruction, which can be either
 * a pc (opnd_create_pc()) or an instr_t (opnd_create_instr()).  Be sure to
 * ensure that the limited reach of this short branch will reach the target
 * (a pc operand is not suitable for most uses unless you know precisely where
 * this instruction will be encoded).
 */
#define INSTR_CREATE_jcc_short(dc, op, t) instr_create_0dst_1src((dc), (op), (t))
/**
 * Creates an instr_t for a conditional branch instruction with the given opcode
 * and target operand.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param op The OP_xxx opcode for the conditional branch, which should be
 * in the range [OP_jo, OP_jnle].
 * \param t The opnd_t target operand for the instruction, which can be either
 * a pc (opnd_create_pc()) or an instr_t (opnd_create_instr()).
 */
#define INSTR_CREATE_jcc(dc, op, t) instr_create_0dst_1src((dc), (op), (t))
/** @name Direct unconditional jump */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param t The opnd_t target operand for the instruction, which can be either
 * a pc (opnd_create_pc()) or an instr_t (opnd_create_instr()).
 */
#define INSTR_CREATE_jmp(dc, t) instr_create_0dst_1src((dc), OP_jmp, (t))
#define INSTR_CREATE_jmp_short(dc, t) instr_create_0dst_1src((dc), OP_jmp_short, (t))
#define INSTR_CREATE_xbegin(dc, t) instr_create_0dst_1src((dc), OP_xbegin, (t))
/** @} */ /* end doxygen group */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param t The opnd_t target operand for the instruction, which should be
 * a memory reference created with opnd_create_base_disp().
 */
#define INSTR_CREATE_jmp_ind(dc, t) instr_create_0dst_1src((dc), OP_jmp_ind, (t))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param t The opnd_t target operand for the instruction, which should be
 * a far pc operand created with opnd_create_far_pc().
 */
#define INSTR_CREATE_jmp_far(dc, t) instr_create_0dst_1src((dc), OP_jmp_far, (t))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param t The opnd_t target operand for the instruction, which should be
 * a far memory reference created with opnd_create_far_base_disp().
 */
#define INSTR_CREATE_jmp_far_ind(dc, t) instr_create_0dst_1src((dc), OP_jmp_far_ind, (t))
/** @name One explicit source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s The opnd_t explicit source operand for the instruction.
 */
#define INSTR_CREATE_lldt(dc, s) instr_create_0dst_1src((dc), OP_lldt, (s))
#define INSTR_CREATE_ltr(dc, s) instr_create_0dst_1src((dc), OP_ltr, (s))
#define INSTR_CREATE_verr(dc, s) instr_create_0dst_1src((dc), OP_verr, (s))
#define INSTR_CREATE_verw(dc, s) instr_create_0dst_1src((dc), OP_verw, (s))
#define INSTR_CREATE_vmptrld(dc, s) instr_create_0dst_1src((dc), OP_vmptrld, (s))
#define INSTR_CREATE_vmxon(dc, s) instr_create_0dst_1src((dc), OP_vmxon, (s))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s The opnd_t explicit second source operand for the instruction, which
 * must be a general-purpose register.
 */
#define INSTR_CREATE_wrfsbase(dc, s) instr_create_0dst_1src((dc), OP_wrfsbase, (s))
#define INSTR_CREATE_wrgsbase(dc, s) instr_create_0dst_1src((dc), OP_wrgsbase, (s))
#define INSTR_CREATE_llwpcb(dc, s) instr_create_0dst_1src((dc), OP_llwpcb, (s))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s The opnd_t explicit source operand for the instruction, which can be
 * created with OPND_CREATE_MEM_lgdt() to get the appropriate operand size.
 */
#define INSTR_CREATE_lgdt(dc, s) instr_create_0dst_1src((dc), OP_lgdt, (s))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s The opnd_t explicit source operand for the instruction, which can be
 * created with OPND_CREATE_MEM_lidt() to get the appropriate operand size.
 */
#define INSTR_CREATE_lidt(dc, s) instr_create_0dst_1src((dc), OP_lidt, (s))
#define INSTR_CREATE_lmsw(dc, s) instr_create_0dst_1src((dc), OP_lmsw, (s))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s The opnd_t explicit source operand for the instruction, which can be
 * created with OPND_CREATE_MEM_invlpg() to get the appropriate operand size.
 */
#define INSTR_CREATE_invlpg(dc, s) instr_create_0dst_1src((dc), OP_invlpg, (s))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s The opnd_t explicit source operand for the instruction, which can be
 * created with OPND_CREATE_MEM_fxrstor() to get the appropriate operand size.
 */
#define INSTR_CREATE_fxrstor32(dc, s) instr_create_0dst_1src((dc), OP_fxrstor32, (s))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s The opnd_t explicit source operand for the instruction, which can be
 * created with OPND_CREATE_MEM_fxrstor() to get the appropriate operand size.
 */
#define INSTR_CREATE_fxrstor64(dc, s) instr_create_0dst_1src((dc), OP_fxrstor64, (s))
#define INSTR_CREATE_ldmxcsr(dc, s) instr_create_0dst_1src((dc), OP_ldmxcsr, (s))
#define INSTR_CREATE_vldmxcsr(dc, s) instr_create_0dst_1src((dc), OP_vldmxcsr, (s))
#define INSTR_CREATE_nop_modrm(dc, s) instr_create_0dst_1src((dc), OP_nop_modrm, (s))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s The opnd_t explicit source operand for the instruction, which can be
 * a general-purpose registers or a memory reference. The latter can be created with
 * OPND_CREATE_MEM_ptwrite() to get the appropriate operand size.
 */
#define INSTR_CREATE_ptwrite(dc, s) instr_create_0dst_1src((dc), OP_ptwrite, (s))
/** @} */ /* end doxygen group */
/** @name Prefetch */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s The opnd_t explicit source operand for the instruction, which can be
 * created with OPND_CREATE_MEM_prefetch() to get the appropriate operand size.
 */
#define INSTR_CREATE_prefetchnta(dc, s) instr_create_0dst_1src((dc), OP_prefetchnta, (s))
#define INSTR_CREATE_prefetcht0(dc, s) instr_create_0dst_1src((dc), OP_prefetcht0, (s))
#define INSTR_CREATE_prefetcht1(dc, s) instr_create_0dst_1src((dc), OP_prefetcht1, (s))
#define INSTR_CREATE_prefetcht2(dc, s) instr_create_0dst_1src((dc), OP_prefetcht2, (s))
#define INSTR_CREATE_prefetch(dc, s) instr_create_0dst_1src((dc), OP_prefetch, (s))
#define INSTR_CREATE_prefetchw(dc, s) instr_create_0dst_1src((dc), OP_prefetchw, (s))
/** @} */ /* end doxygen group */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s The opnd_t explicit source operand for the instruction, which can be
 * created with OPND_CREATE_MEM_clflush() to get the appropriate operand size.
 */
#define INSTR_CREATE_clflush(dc, s) instr_create_0dst_1src((dc), OP_clflush, (s))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param i The opnd_t explicit second source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_int(dc, i) instr_create_0dst_1src((dc), OP_int, (i))

/* floating-point */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param m The opnd_t explicit destination operand for the instruction, which can be
 * created with OPND_CREATE_MEM_fldenv() to get the appropriate operand size.
 */
#define INSTR_CREATE_fldenv(dc, m) instr_create_0dst_1src((dc), OP_fldenv, (m))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param m The opnd_t explicit destination operand for the instruction, which must
 * be a memory reference (opnd_create_base_disp() or opnd_create_far_base_disp()).
 */
#define INSTR_CREATE_fldcw(dc, m) instr_create_0dst_1src((dc), OP_fldcw, (m))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param m The opnd_t explicit destination operand for the instruction, which can be
 * created with OPND_CREATE_MEM_frstor() to get the appropriate operand size.
 */
#define INSTR_CREATE_frstor(dc, m) instr_create_0dst_1src((dc), OP_frstor, (m))

/* no destination, 1 implicit source */
/** @name One implicit source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx, automatically
 * supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define INSTR_CREATE_fxam(dc) \
    instr_create_0dst_1src((dc), OP_fxam, opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_sahf(dc) \
    instr_create_0dst_1src((dc), OP_sahf, opnd_create_reg(DR_REG_AH))
#define INSTR_CREATE_vmrun(dc) \
    instr_create_0dst_1src((dc), OP_vmrun, opnd_create_reg(DR_REG_XAX))
#define INSTR_CREATE_vmload(dc) \
    instr_create_0dst_1src((dc), OP_vmload, opnd_create_reg(DR_REG_XAX))
#define INSTR_CREATE_vmsave(dc) \
    instr_create_0dst_1src((dc), OP_vmsave, opnd_create_reg(DR_REG_XAX))
#define INSTR_CREATE_skinit(dc) \
    instr_create_0dst_1src((dc), OP_skinit, opnd_create_reg(DR_REG_EAX))
#ifndef X64
#    define INSTR_CREATE_sysret(dc) \
        instr_create_0dst_1src((dc), OP_sysret, opnd_create_reg(DR_REG_XCX))
#endif
/** @} */ /* end doxygen group */

/* no destination, 2 explicit sources */
/** @name No destination, 2 explicit sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s1 The opnd_t first source operand for the instruction.
 * \param s2 The opnd_t second source operand for the instruction.
 */
#define INSTR_CREATE_cmp(dc, s1, s2) instr_create_0dst_2src((dc), OP_cmp, (s1), (s2))
#define INSTR_CREATE_test(dc, s1, s2) instr_create_0dst_2src((dc), OP_test, (s1), (s2))
#define INSTR_CREATE_ptest(dc, s1, s2) instr_create_0dst_2src((dc), OP_ptest, (s1), (s2))
#define INSTR_CREATE_ud1(dc, s1, s2) instr_create_0dst_2src((dc), OP_ud1, (s1), (s2))
#define INSTR_CREATE_ud2b(dc, s1, s2) INSTR_CREATE_ud1((dc), (s1), (s2))
/* AVX */
#define INSTR_CREATE_vucomiss(dc, s1, s2) \
    instr_create_0dst_2src((dc), OP_vucomiss, (s1), (s2))
#define INSTR_CREATE_vucomisd(dc, s1, s2) \
    instr_create_0dst_2src((dc), OP_vucomisd, (s1), (s2))
#define INSTR_CREATE_vcomiss(dc, s1, s2) \
    instr_create_0dst_2src((dc), OP_vcomiss, (s1), (s2))
#define INSTR_CREATE_vcomisd(dc, s1, s2) \
    instr_create_0dst_2src((dc), OP_vcomisd, (s1), (s2))
#define INSTR_CREATE_vptest(dc, s1, s2) \
    instr_create_0dst_2src((dc), OP_vptest, (s1), (s2))
#define INSTR_CREATE_vtestps(dc, s1, s2) \
    instr_create_0dst_2src((dc), OP_vtestps, (s1), (s2))
#define INSTR_CREATE_vtestpd(dc, s1, s2) \
    instr_create_0dst_2src((dc), OP_vtestpd, (s1), (s2))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s1 The opnd_t first source operand for the instruction.
 * \param s2 The opnd_t second source operand for the instruction, which can
 * be created with OPND_CREATE_MEM_bound() to get the appropriate operand size.
 */
#define INSTR_CREATE_bound(dc, s1, s2) instr_create_0dst_2src((dc), OP_bound, (s1), (s2))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s The opnd_t first source operand for the instruction.
 * \param ri The opnd_t second source operand for the instruction, which can
 * be either a register or an immediate integer.
 */
#define INSTR_CREATE_bt(dc, s, ri) instr_create_0dst_2src((dc), OP_bt, (s), (ri))
#define INSTR_CREATE_ucomiss(dc, s1, s2) \
    instr_create_0dst_2src((dc), OP_ucomiss, (s1), (s2))
#define INSTR_CREATE_ucomisd(dc, s1, s2) \
    instr_create_0dst_2src((dc), OP_ucomisd, (s1), (s2))
#define INSTR_CREATE_comiss(dc, s1, s2) \
    instr_create_0dst_2src((dc), OP_comiss, (s1), (s2))
#define INSTR_CREATE_comisd(dc, s1, s2) \
    instr_create_0dst_2src((dc), OP_comisd, (s1), (s2))
#define INSTR_CREATE_invept(dc, s1, s2) \
    instr_create_0dst_2src((dc), OP_invept, (s1), (s2))
#define INSTR_CREATE_invvpid(dc, s1, s2) \
    instr_create_0dst_2src((dc), OP_invvpid, (s1), (s2))
#define INSTR_CREATE_invpcid(dc, s1, s2) \
    instr_create_0dst_2src((dc), OP_invpcid, (s1), (s2))
/** @} */ /* end doxygen group */

/* no destination, 1 mask, and 1 explicit source */
/** @name No destination, 1 mask, and 1 explicit source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param k The opnd_t first source operand for the instruction.
 * \param s The opnd_t second source operand for the instruction.
 */
/* AVX-512 EVEX */
#define INSTR_CREATE_vgatherpf0dps_mask(dc, k, s) \
    instr_create_0dst_2src((dc), OP_vgatherpf0dps, (k), (s))
#define INSTR_CREATE_vgatherpf0dpd_mask(dc, k, s) \
    instr_create_0dst_2src((dc), OP_vgatherpf0dpd, (k), (s))
#define INSTR_CREATE_vgatherpf0qps_mask(dc, k, s) \
    instr_create_0dst_2src((dc), OP_vgatherpf0qps, (k), (s))
#define INSTR_CREATE_vgatherpf0qpd_mask(dc, k, s) \
    instr_create_0dst_2src((dc), OP_vgatherpf0qpd, (k), (s))
#define INSTR_CREATE_vgatherpf1dps_mask(dc, k, s) \
    instr_create_0dst_2src((dc), OP_vgatherpf1dps, (k), (s))
#define INSTR_CREATE_vgatherpf1dpd_mask(dc, k, s) \
    instr_create_0dst_2src((dc), OP_vgatherpf1dpd, (k), (s))
#define INSTR_CREATE_vgatherpf1qps_mask(dc, k, s) \
    instr_create_0dst_2src((dc), OP_vgatherpf1qps, (k), (s))
#define INSTR_CREATE_vgatherpf1qpd_mask(dc, k, s) \
    instr_create_0dst_2src((dc), OP_vgatherpf1qpd, (k), (s))
#define INSTR_CREATE_vscatterpf0dps_mask(dc, k, s) \
    instr_create_0dst_2src((dc), OP_vscatterpf0dps, (k), (s))
#define INSTR_CREATE_vscatterpf0dpd_mask(dc, k, s) \
    instr_create_0dst_2src((dc), OP_vscatterpf0dpd, (k), (s))
#define INSTR_CREATE_vscatterpf0qps_mask(dc, k, s) \
    instr_create_0dst_2src((dc), OP_vscatterpf0qps, (k), (s))
#define INSTR_CREATE_vscatterpf0qpd_mask(dc, k, s) \
    instr_create_0dst_2src((dc), OP_vscatterpf0qpd, (k), (s))
#define INSTR_CREATE_vscatterpf1dps_mask(dc, k, s) \
    instr_create_0dst_2src((dc), OP_vscatterpf1dps, (k), (s))
#define INSTR_CREATE_vscatterpf1dpd_mask(dc, k, s) \
    instr_create_0dst_2src((dc), OP_vscatterpf1dpd, (k), (s))
#define INSTR_CREATE_vscatterpf1qps_mask(dc, k, s) \
    instr_create_0dst_2src((dc), OP_vscatterpf1qps, (k), (s))
#define INSTR_CREATE_vscatterpf1qpd_mask(dc, k, s) \
    instr_create_0dst_2src((dc), OP_vscatterpf1qpd, (k), (s))
/** @} */ /* end doxygen group */

/* no destination, 2 sources: 1 implicit */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param t The opnd_t target operand for the instruction, which can be either
 * a pc (opnd_create_pc()) or an instr_t (opnd_create_instr()).
 */
#define INSTR_CREATE_jecxz(dc, t) \
    instr_create_0dst_2src((dc), OP_jecxz, (t), opnd_create_reg(DR_REG_XCX))
/**
 * Creates an instr_t for an OP_jecxz instruction that uses cx instead of ecx
 * (there is no separate OP_jcxz).
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param t The opnd_t target operand for the instruction, which can be either
 * a pc (opnd_create_pc()) or an instr_t (opnd_create_instr()).
 */
#define INSTR_CREATE_jcxz(dc, t) \
    instr_create_0dst_2src((dc), OP_jecxz, (t), opnd_create_reg(DR_REG_CX))

/* no destination, 2 sources */
/** @name No destination, 2 sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * Creates an instr_t for an OP_out instruction with a source of al
 * (INSTR_CREATE_out_1()) or eax (INSTR_CREATE_out_4()) and dx.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define INSTR_CREATE_out_1(dc)                                       \
    instr_create_0dst_2src((dc), OP_out, opnd_create_reg(DR_REG_AL), \
                           opnd_create_reg(DR_REG_DX))
#define INSTR_CREATE_out_4(dc)                                        \
    instr_create_0dst_2src((dc), OP_out, opnd_create_reg(DR_REG_EAX), \
                           opnd_create_reg(DR_REG_DX))
/** @} */ /* end doxygen group */
/** @name No destination, explicit immed source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * Creates an instr_t for an OP_out instruction with a source of al
 * (INSTR_CREATE_out_1_imm()) or eax (INSTR_CREATE_out_4_imm()) and an immediate.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param i The opnd_t explicit source operand for the instruction, which must be an
 * immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_out_1_imm(dc, i) \
    instr_create_0dst_2src((dc), OP_out, (i), opnd_create_reg(DR_REG_AL))
#define INSTR_CREATE_out_4_imm(dc, i) \
    instr_create_0dst_2src((dc), OP_out, (i), opnd_create_reg(DR_REG_EAX))
/** @} */ /* end doxygen group */

/** @name No destination, 2 implicit sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx, automatically
 * supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
/* no destination, 2 implicit sources */
#define INSTR_CREATE_mwait(dc)                                          \
    instr_create_0dst_2src((dc), OP_mwait, opnd_create_reg(DR_REG_EAX), \
                           opnd_create_reg(DR_REG_ECX))
#define INSTR_CREATE_mwaitx(dc)                                          \
    instr_create_0dst_2src((dc), OP_mwaitx, opnd_create_reg(DR_REG_EAX), \
                           opnd_create_reg(DR_REG_ECX))
#define INSTR_CREATE_invlpga(dc)                                          \
    instr_create_0dst_2src((dc), OP_invlpga, opnd_create_reg(DR_REG_XAX), \
                           opnd_create_reg(DR_REG_ECX))
#ifdef X64
#    define INSTR_CREATE_sysret(dc)                                          \
        instr_create_0dst_2src((dc), OP_sysret, opnd_create_reg(DR_REG_XCX), \
                               opnd_create_reg(DR_REG_R11))
#endif
/* no destination, 3 implicit sources */
#define INSTR_CREATE_wrmsr(dc)                                          \
    instr_create_0dst_3src((dc), OP_wrmsr, opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_ECX))
#define INSTR_CREATE_monitor(dc)                                          \
    instr_create_0dst_3src((dc), OP_monitor, opnd_create_reg(DR_REG_XAX), \
                           opnd_create_reg(DR_REG_ECX), opnd_create_reg(DR_REG_EDX))
#define INSTR_CREATE_monitorx(dc)                                          \
    instr_create_0dst_3src((dc), OP_monitorx, opnd_create_reg(DR_REG_XAX), \
                           opnd_create_reg(DR_REG_ECX), opnd_create_reg(DR_REG_EDX))
#define INSTR_CREATE_xsetbv(dc)                                          \
    instr_create_0dst_3src((dc), OP_xsetbv, opnd_create_reg(DR_REG_ECX), \
                           opnd_create_reg(DR_REG_EDX), opnd_create_reg(DR_REG_EAX))
#define INSTR_CREATE_wrpkru(dc)                                          \
    instr_create_0dst_3src((dc), OP_wrpkru, opnd_create_reg(DR_REG_ECX), \
                           opnd_create_reg(DR_REG_EDX), opnd_create_reg(DR_REG_EAX))
/** @} */ /* end doxygen group */

/** @name No destination, 3 sources: 1 implicit */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx, automatically
 * supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s The opnd_t explicit source operand for the instruction, which can be
 * created with OPND_CREATE_MEM_xsave() to get the appropriate operand size.
 */
#define INSTR_CREATE_xrstor32(dc, s)                                            \
    instr_create_0dst_3src((dc), OP_xrstor32, (s), opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx, automatically
 * supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s The opnd_t explicit source operand for the instruction, which can be
 * created with OPND_CREATE_MEM_xsave() to get the appropriate operand size.
 */
#define INSTR_CREATE_xrstor64(dc, s)                                            \
    instr_create_0dst_3src((dc), OP_xrstor64, (s), opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX))
/** @} */ /* end doxygen group */

/** @name No destination, 3 sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx, automatically
 * supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s1 The opnd_t first source operand for the instruction, which must be
 * a general-purpose register.
 * \param s2 The opnd_t second source operand for the instruction
 * \param i The opnd_t third source operand for the instruction, which must be
 * an immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_lwpins(dc, s1, s2, i) \
    instr_create_0dst_3src((dc), OP_lwpins, (s1), (s2), (i))
#define INSTR_CREATE_lwpval(dc, s1, s2, i) \
    instr_create_0dst_3src((dc), OP_lwpval, (s1), (s2), (i))
/** @} */ /* end doxygen group */

/* floating-point */
/** @name Floating-point with source of memory or fp register */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s The opnd_t explicit source operand for the instruction, which must
 * be one of the following:
 * -# A floating point register (opnd_create_reg()).
 * -# A memory reference (opnd_create_base_disp() or opnd_create_far_base_disp()).
 * The other (implicit) source operand is #DR_REG_ST0.
 */
#define INSTR_CREATE_fcom(dc, s) \
    instr_create_0dst_2src((dc), OP_fcom, (s), opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_fcomp(dc, s) \
    instr_create_0dst_2src((dc), OP_fcomp, (s), opnd_create_reg(DR_REG_ST0))
/** @} */ /* end doxygen group */
/** @name Floating-point with fp register source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param f The opnd_t explicit source operand for the instruction, which must
 * be a floating point register (opnd_create_reg()).
 * The other (implicit) source operand is #DR_REG_ST0.
 */
#define INSTR_CREATE_fcomi(dc, f) \
    instr_create_0dst_2src((dc), OP_fcomi, opnd_create_reg(DR_REG_ST0), (f))
#define INSTR_CREATE_fcomip(dc, f) \
    instr_create_0dst_2src((dc), OP_fcomip, opnd_create_reg(DR_REG_ST0), (f))
#define INSTR_CREATE_fucomi(dc, f) \
    instr_create_0dst_2src((dc), OP_fucomi, opnd_create_reg(DR_REG_ST0), (f))
#define INSTR_CREATE_fucomip(dc, f) \
    instr_create_0dst_2src((dc), OP_fucomip, opnd_create_reg(DR_REG_ST0), (f))
#define INSTR_CREATE_fucom(dc, f) \
    instr_create_0dst_2src((dc), OP_fucom, opnd_create_reg(DR_REG_ST0), (f))
#define INSTR_CREATE_fucomp(dc, f) \
    instr_create_0dst_2src((dc), OP_fucomp, opnd_create_reg(DR_REG_ST0), (f))
/** @} */ /* end doxygen group */
/** @name Floating-point with no explicit sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx,
 * automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define INSTR_CREATE_fucompp(dc)                                          \
    instr_create_0dst_2src((dc), OP_fucompp, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_reg(DR_REG_ST1))
#define INSTR_CREATE_fcompp(dc)                                          \
    instr_create_0dst_2src((dc), OP_fcompp, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_reg(DR_REG_ST1))
/** @} */ /* end doxygen group */

/* 1 destination, no sources */
/**
 * Creates an instr_t for a conditional set instruction with the given opcode
 * and destination operand.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param op The OP_xxx opcode for the instruction, which should be in the range
 * [OP_seto, OP_setnle].
 * \param d The opnd_t destination operand for the instruction.
 */
#define INSTR_CREATE_setcc(dc, op, d) instr_create_1dst_0src((dc), (op), (d))
/** @name 1 explicit destination, no sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 */
#define INSTR_CREATE_sldt(dc, d) instr_create_1dst_0src((dc), OP_sldt, (d))
#define INSTR_CREATE_str(dc, d) instr_create_1dst_0src((dc), OP_str, (d))
#define INSTR_CREATE_vmptrst(dc, d) instr_create_1dst_0src((dc), OP_vmptrst, (d))
#define INSTR_CREATE_vmclear(dc, d) instr_create_1dst_0src((dc), OP_vmclear, (d))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction, which
 * must be a general-purpose register.
 */
#define INSTR_CREATE_rdrand(dc, d) instr_create_1dst_0src((dc), OP_rdrand, (d))
#define INSTR_CREATE_rdseed(dc, d) instr_create_1dst_0src((dc), OP_rdseed, (d))
#define INSTR_CREATE_rdfsbase(dc, d) instr_create_1dst_0src((dc), OP_rdfsbase, (d))
#define INSTR_CREATE_rdgsbase(dc, d) instr_create_1dst_0src((dc), OP_rdgsbase, (d))
#define INSTR_CREATE_slwpcb(dc, d) instr_create_1dst_0src((dc), OP_slwpcb, (d))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction, which can
 * be created with OPND_CREATE_MEM_sgdt() to get the appropriate operand size.
 */
#define INSTR_CREATE_sgdt(dc, d) instr_create_1dst_0src((dc), OP_sgdt, (d))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction, which can
 * be created with OPND_CREATE_MEM_sidt() to get the appropriate operand size.
 */
#define INSTR_CREATE_sidt(dc, d) instr_create_1dst_0src((dc), OP_sidt, (d))
#define INSTR_CREATE_smsw(dc, d) instr_create_1dst_0src((dc), OP_smsw, (d))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction, which can
 * be created with OPND_CREATE_MEM_fxsave() to get the appropriate operand size.
 */
#define INSTR_CREATE_fxsave32(dc, d) instr_create_1dst_0src((dc), OP_fxsave32, (d))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction, which can
 * be created with OPND_CREATE_MEM_fxsave() to get the appropriate operand size.
 */
#define INSTR_CREATE_fxsave64(dc, d) instr_create_1dst_0src((dc), OP_fxsave64, (d))
#define INSTR_CREATE_stmxcsr(dc, d) instr_create_1dst_0src((dc), OP_stmxcsr, (d))
#define INSTR_CREATE_vstmxcsr(dc, d) instr_create_1dst_0src((dc), OP_vstmxcsr, (d))
/** @} */ /* end doxygen group */

/* floating-point */
/** @name Floating-point with memory destination */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param m The opnd_t explicit destination operand for the instruction, which must
 * be a memory reference (opnd_create_base_disp() or opnd_create_far_base_disp()).
 */
#define INSTR_CREATE_fnstcw(dc, m) instr_create_1dst_0src((dc), OP_fnstcw, (m))
#define INSTR_CREATE_fnstsw(dc, m) instr_create_1dst_0src((dc), OP_fnstsw, (m))
/** @} */ /* end doxygen group */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param m The opnd_t explicit destination operand for the instruction, which can
 * be created with OPND_CREATE_MEM_fnstenv() to get the appropriate operand size.
 */
#define INSTR_CREATE_fnstenv(dc, m) instr_create_1dst_0src((dc), OP_fnstenv, (m))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param m The opnd_t explicit destination operand for the instruction, which can
 * be created with OPND_CREATE_MEM_fnsave() to get the appropriate operand size.
 */
#define INSTR_CREATE_fnsave(dc, m) instr_create_1dst_0src((dc), OP_fnsave, (m))

/** @name Floating-point with float register destination, no sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param f The opnd_t explicit destination operand for the instruction, which must
 * be a floating point register (opnd_create_reg()).
 */
#define INSTR_CREATE_ffree(dc, f) instr_create_1dst_0src((dc), OP_ffree, (f))
#define INSTR_CREATE_ffreep(dc, f) instr_create_1dst_0src((dc), OP_ffreep, (f))
/** @} */ /* end doxygen group */

/* 1 implicit destination, no sources */
/** @name 1 implicit destination, no sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx, automatically
 * supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define INSTR_CREATE_lahf(dc) \
    instr_create_1dst_0src((dc), OP_lahf, opnd_create_reg(DR_REG_AH))
#define INSTR_CREATE_sysenter(dc) \
    instr_create_1dst_0src((dc), OP_sysenter, opnd_create_reg(DR_REG_XSP))
#ifndef X64
#    define INSTR_CREATE_syscall(dc) \
        instr_create_1dst_0src((dc), OP_syscall, opnd_create_reg(DR_REG_XCX))
#endif
#define INSTR_CREATE_salc(dc) \
    instr_create_1dst_0src((dc), OP_salc, opnd_create_reg(DR_REG_AL))
/** @} */ /* end doxygen group */

/* 1 destination, 1 source */
/** @name 1 destination, 1 source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s The opnd_t explicit source operand for the instruction.
 */
#define INSTR_CREATE_arpl(dc, d, s) instr_create_1dst_1src((dc), OP_arpl, (d), (s))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s The opnd_t explicit source operand for the instruction, which can be
 * created with OPND_CREATE_MEM_lea() to get the appropriate operand size.
 */
#define INSTR_CREATE_lea(dc, d, s) instr_create_1dst_1src((dc), OP_lea, (d), (s))
#define INSTR_CREATE_mov_ld(dc, d, s) instr_create_1dst_1src((dc), OP_mov_ld, (d), (s))
#define INSTR_CREATE_mov_st(dc, d, s) instr_create_1dst_1src((dc), OP_mov_st, (d), (s))
#define INSTR_CREATE_mov_imm(dc, d, s) instr_create_1dst_1src((dc), OP_mov_imm, (d), (s))
#define INSTR_CREATE_mov_seg(dc, d, s) instr_create_1dst_1src((dc), OP_mov_seg, (d), (s))
#define INSTR_CREATE_mov_priv(dc, d, s) \
    instr_create_1dst_1src((dc), OP_mov_priv, (d), (s))
#define INSTR_CREATE_lar(dc, d, s) instr_create_1dst_1src((dc), OP_lar, (d), (s))
#define INSTR_CREATE_lsl(dc, d, s) instr_create_1dst_1src((dc), OP_lsl, (d), (s))
#define INSTR_CREATE_movntps(dc, d, s) instr_create_1dst_1src((dc), OP_movntps, (d), (s))
#define INSTR_CREATE_movntpd(dc, d, s) instr_create_1dst_1src((dc), OP_movntpd, (d), (s))
#define INSTR_CREATE_movd(dc, d, s) instr_create_1dst_1src((dc), OP_movd, (d), (s))
#define INSTR_CREATE_movq(dc, d, s) instr_create_1dst_1src((dc), OP_movq, (d), (s))
#define INSTR_CREATE_movdqu(dc, d, s) instr_create_1dst_1src((dc), OP_movdqu, (d), (s))
#define INSTR_CREATE_movdqa(dc, d, s) instr_create_1dst_1src((dc), OP_movdqa, (d), (s))
#define INSTR_CREATE_movzx(dc, d, s) instr_create_1dst_1src((dc), OP_movzx, (d), (s))
#define INSTR_CREATE_movsx(dc, d, s) instr_create_1dst_1src((dc), OP_movsx, (d), (s))
#define INSTR_CREATE_bsf(dc, d, s) \
    INSTR_PRED(instr_create_1dst_1src((dc), OP_bsf, (d), (s)), DR_PRED_COMPLEX)
#define INSTR_CREATE_bsr(dc, d, s) \
    INSTR_PRED(instr_create_1dst_1src((dc), OP_bsr, (d), (s)), DR_PRED_COMPLEX)
#define INSTR_CREATE_pmovmskb(dc, d, s) \
    instr_create_1dst_1src((dc), OP_pmovmskb, (d), (s))
#define INSTR_CREATE_movups(dc, d, s) instr_create_1dst_1src((dc), OP_movups, (d), (s))
#define INSTR_CREATE_movss(dc, d, s) instr_create_1dst_1src((dc), OP_movss, (d), (s))
#define INSTR_CREATE_movupd(dc, d, s) instr_create_1dst_1src((dc), OP_movupd, (d), (s))
#define INSTR_CREATE_movsd(dc, d, s) instr_create_1dst_1src((dc), OP_movsd, (d), (s))
#define INSTR_CREATE_movlps(dc, d, s) instr_create_1dst_1src((dc), OP_movlps, (d), (s))
#define INSTR_CREATE_movlpd(dc, d, s) instr_create_1dst_1src((dc), OP_movlpd, (d), (s))
#define INSTR_CREATE_movhps(dc, d, s) instr_create_1dst_1src((dc), OP_movhps, (d), (s))
#define INSTR_CREATE_movhpd(dc, d, s) instr_create_1dst_1src((dc), OP_movhpd, (d), (s))
#define INSTR_CREATE_movaps(dc, d, s) instr_create_1dst_1src((dc), OP_movaps, (d), (s))
#define INSTR_CREATE_movapd(dc, d, s) instr_create_1dst_1src((dc), OP_movapd, (d), (s))
#define INSTR_CREATE_cvtpi2ps(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvtpi2ps, (d), (s))
#define INSTR_CREATE_cvtsi2ss(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvtsi2ss, (d), (s))
#define INSTR_CREATE_cvtpi2pd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvtpi2pd, (d), (s))
#define INSTR_CREATE_cvtsi2sd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvtsi2sd, (d), (s))
#define INSTR_CREATE_cvttps2pi(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvttps2pi, (d), (s))
#define INSTR_CREATE_cvttss2si(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvttss2si, (d), (s))
#define INSTR_CREATE_cvttpd2pi(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvttpd2pi, (d), (s))
#define INSTR_CREATE_cvttsd2si(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvttsd2si, (d), (s))
#define INSTR_CREATE_cvtps2pi(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvtps2pi, (d), (s))
#define INSTR_CREATE_cvtss2si(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvtss2si, (d), (s))
#define INSTR_CREATE_cvtpd2pi(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvtpd2pi, (d), (s))
#define INSTR_CREATE_cvtsd2si(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvtsd2si, (d), (s))
#define INSTR_CREATE_cvtps2pd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvtps2pd, (d), (s))
#define INSTR_CREATE_cvtss2sd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvtss2sd, (d), (s))
#define INSTR_CREATE_cvtpd2ps(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvtpd2ps, (d), (s))
#define INSTR_CREATE_cvtsd2ss(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvtsd2ss, (d), (s))
#define INSTR_CREATE_cvtdq2ps(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvtdq2ps, (d), (s))
#define INSTR_CREATE_cvttps2dq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvttps2dq, (d), (s))
#define INSTR_CREATE_cvtps2dq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvtps2dq, (d), (s))
#define INSTR_CREATE_cvtdq2pd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvtdq2pd, (d), (s))
#define INSTR_CREATE_cvttpd2dq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvttpd2dq, (d), (s))
#define INSTR_CREATE_cvtpd2dq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_cvtpd2dq, (d), (s))
#define INSTR_CREATE_movmskps(dc, d, s) \
    instr_create_1dst_1src((dc), OP_movmskps, (d), (s))
#define INSTR_CREATE_movmskpd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_movmskpd, (d), (s))
#define INSTR_CREATE_sqrtps(dc, d, s) instr_create_1dst_1src((dc), OP_sqrtps, (d), (s))
#define INSTR_CREATE_sqrtss(dc, d, s) instr_create_1dst_1src((dc), OP_sqrtss, (d), (s))
#define INSTR_CREATE_sqrtpd(dc, d, s) instr_create_1dst_1src((dc), OP_sqrtpd, (d), (s))
#define INSTR_CREATE_sqrtsd(dc, d, s) instr_create_1dst_1src((dc), OP_sqrtsd, (d), (s))
#define INSTR_CREATE_rsqrtps(dc, d, s) instr_create_1dst_1src((dc), OP_rsqrtps, (d), (s))
#define INSTR_CREATE_rsqrtss(dc, d, s) instr_create_1dst_1src((dc), OP_rsqrtss, (d), (s))
#define INSTR_CREATE_rcpps(dc, d, s) instr_create_1dst_1src((dc), OP_rcpps, (d), (s))
#define INSTR_CREATE_rcpss(dc, d, s) instr_create_1dst_1src((dc), OP_rcpss, (d), (s))
#define INSTR_CREATE_lddqu(dc, d, s) instr_create_1dst_1src((dc), OP_lddqu, (d), (s))
#define INSTR_CREATE_movsldup(dc, d, s) \
    instr_create_1dst_1src((dc), OP_movsldup, (d), (s))
#define INSTR_CREATE_movshdup(dc, d, s) \
    instr_create_1dst_1src((dc), OP_movshdup, (d), (s))
#define INSTR_CREATE_movddup(dc, d, s) instr_create_1dst_1src((dc), OP_movddup, (d), (s))
#define INSTR_CREATE_popcnt(dc, d, s) instr_create_1dst_1src((dc), OP_popcnt, (d), (s))
#define INSTR_CREATE_movntss(dc, d, s) instr_create_1dst_1src((dc), OP_movntss, (d), (s))
#define INSTR_CREATE_movntsd(dc, d, s) instr_create_1dst_1src((dc), OP_movntsd, (d), (s))
#define INSTR_CREATE_movntq(dc, d, s) instr_create_1dst_1src((dc), OP_movntq, (d), (s))
#define INSTR_CREATE_movntdq(dc, d, s) instr_create_1dst_1src((dc), OP_movntdq, (d), (s))
#define INSTR_CREATE_movnti(dc, d, s) instr_create_1dst_1src((dc), OP_movnti, (d), (s))
#define INSTR_CREATE_lzcnt(dc, d, s) instr_create_1dst_1src((dc), OP_lzcnt, (d), (s))
#define INSTR_CREATE_pmovsxbw(dc, d, s) \
    instr_create_1dst_1src((dc), OP_pmovsxbw, (d), (s))
#define INSTR_CREATE_pmovsxbd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_pmovsxbd, (d), (s))
#define INSTR_CREATE_pmovsxbq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_pmovsxbq, (d), (s))
#define INSTR_CREATE_pmovsxwd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_pmovsxwd, (d), (s))
#define INSTR_CREATE_pmovsxwq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_pmovsxwq, (d), (s))
#define INSTR_CREATE_pmovsxdq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_pmovsxdq, (d), (s))
#define INSTR_CREATE_movntdqa(dc, d, s) \
    instr_create_1dst_1src((dc), OP_movntdqa, (d), (s))
#define INSTR_CREATE_pmovzxbw(dc, d, s) \
    instr_create_1dst_1src((dc), OP_pmovzxbw, (d), (s))
#define INSTR_CREATE_pmovzxbd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_pmovzxbd, (d), (s))
#define INSTR_CREATE_pmovzxbq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_pmovzxbq, (d), (s))
#define INSTR_CREATE_pmovzxwd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_pmovzxwd, (d), (s))
#define INSTR_CREATE_pmovzxwq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_pmovzxwq, (d), (s))
#define INSTR_CREATE_pmovzxdq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_pmovzxdq, (d), (s))
#define INSTR_CREATE_phminposuw(dc, d, s) \
    instr_create_1dst_1src((dc), OP_phminposuw, (d), (s))
#define INSTR_CREATE_vmread(dc, d, s) instr_create_1dst_1src((dc), OP_vmread, (d), (s))
#define INSTR_CREATE_vmwrite(dc, d, s) instr_create_1dst_1src((dc), OP_vmwrite, (d), (s))
#define INSTR_CREATE_movsxd(dc, d, s) instr_create_1dst_1src((dc), OP_movsxd, (d), (s))
#define INSTR_CREATE_movbe(dc, d, s) instr_create_1dst_1src((dc), OP_movbe, (d), (s))
#define INSTR_CREATE_aesimc(dc, d, s) instr_create_1dst_1src((dc), OP_aesimc, (d), (s))
#define INSTR_CREATE_pabsb(dc, d, s) instr_create_1dst_1src((dc), OP_pabsb, (d), (s))
#define INSTR_CREATE_pabsw(dc, d, s) instr_create_1dst_1src((dc), OP_pabsw, (d), (s))
#define INSTR_CREATE_pabsd(dc, d, s) instr_create_1dst_1src((dc), OP_pabsd, (d), (s))
/* AVX */
#define INSTR_CREATE_vmovups(dc, d, s) instr_create_1dst_1src((dc), OP_vmovups, (d), (s))
#define INSTR_CREATE_vmovupd(dc, d, s) instr_create_1dst_1src((dc), OP_vmovupd, (d), (s))
#define INSTR_CREATE_vmovsldup(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vmovsldup, (d), (s))
#define INSTR_CREATE_vmovddup(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vmovddup, (d), (s))
#define INSTR_CREATE_vmovlps(dc, d, s) instr_create_1dst_1src((dc), OP_vmovlps, (d), (s))
#define INSTR_CREATE_vmovlpd(dc, d, s) instr_create_1dst_1src((dc), OP_vmovlpd, (d), (s))
#define INSTR_CREATE_vmovshdup(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vmovshdup, (d), (s))
#define INSTR_CREATE_vmovhps(dc, d, s) instr_create_1dst_1src((dc), OP_vmovhps, (d), (s))
#define INSTR_CREATE_vmovhpd(dc, d, s) instr_create_1dst_1src((dc), OP_vmovhpd, (d), (s))
#define INSTR_CREATE_vmovaps(dc, d, s) instr_create_1dst_1src((dc), OP_vmovaps, (d), (s))
#define INSTR_CREATE_vmovapd(dc, d, s) instr_create_1dst_1src((dc), OP_vmovapd, (d), (s))
#define INSTR_CREATE_vmovntps(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vmovntps, (d), (s))
#define INSTR_CREATE_vmovntpd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vmovntpd, (d), (s))
#define INSTR_CREATE_vcvttss2si(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vcvttss2si, (d), (s))
#define INSTR_CREATE_vcvttsd2si(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vcvttsd2si, (d), (s))
#define INSTR_CREATE_vcvtss2si(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vcvtss2si, (d), (s))
#define INSTR_CREATE_vcvtsd2si(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vcvtsd2si, (d), (s))
#define INSTR_CREATE_vcvtss2usi(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vcvtss2usi, (d), (s))
#define INSTR_CREATE_vcvtsd2usi(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vcvtsd2usi, (d), (s))
#define INSTR_CREATE_vcvttss2usi(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vcvttss2usi, (d), (s))
#define INSTR_CREATE_vcvttsd2usi(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vcvttsd2usi, (d), (s))
#define INSTR_CREATE_vmovmskps(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vmovmskps, (d), (s))
#define INSTR_CREATE_vmovmskpd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vmovmskpd, (d), (s))
#define INSTR_CREATE_vsqrtps(dc, d, s) instr_create_1dst_1src((dc), OP_vsqrtps, (d), (s))
#define INSTR_CREATE_vsqrtpd(dc, d, s) instr_create_1dst_1src((dc), OP_vsqrtpd, (d), (s))
#define INSTR_CREATE_vrsqrtps(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vrsqrtps, (d), (s))
#define INSTR_CREATE_vrcpps(dc, d, s) instr_create_1dst_1src((dc), OP_vrcpps, (d), (s))
#define INSTR_CREATE_vcvtps2pd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vcvtps2pd, (d), (s))
#define INSTR_CREATE_vcvtpd2ps(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vcvtpd2ps, (d), (s))
#define INSTR_CREATE_vcvtdq2ps(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vcvtdq2ps, (d), (s))
#define INSTR_CREATE_vcvttps2dq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vcvttps2dq, (d), (s))
#define INSTR_CREATE_vcvtps2dq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vcvtps2dq, (d), (s))
#define INSTR_CREATE_vmovd(dc, d, s) instr_create_1dst_1src((dc), OP_vmovd, (d), (s))
#define INSTR_CREATE_vmovq(dc, d, s) instr_create_1dst_1src((dc), OP_vmovq, (d), (s))
#define INSTR_CREATE_vpmovmskb(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovmskb, (d), (s))
#define INSTR_CREATE_vcvtdq2pd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vcvtdq2pd, (d), (s))
#define INSTR_CREATE_vcvttpd2dq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vcvttpd2dq, (d), (s))
#define INSTR_CREATE_vcvtpd2dq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vcvtpd2dq, (d), (s))
#define INSTR_CREATE_vmovntdq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vmovntdq, (d), (s))
#define INSTR_CREATE_vmovdqu(dc, d, s) instr_create_1dst_1src((dc), OP_vmovdqu, (d), (s))
#define INSTR_CREATE_vmovdqa(dc, d, s) instr_create_1dst_1src((dc), OP_vmovdqa, (d), (s))
#define INSTR_CREATE_vlddqu(dc, d, s) instr_create_1dst_1src((dc), OP_vlddqu, (d), (s))
#define INSTR_CREATE_vpmovsxbw(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovsxbw, (d), (s))
#define INSTR_CREATE_vpmovsxbd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovsxbd, (d), (s))
#define INSTR_CREATE_vpmovsxbq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovsxbq, (d), (s))
#define INSTR_CREATE_vpmovsxwd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovsxwd, (d), (s))
#define INSTR_CREATE_vpmovsxwq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovsxwq, (d), (s))
#define INSTR_CREATE_vpmovsxdq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovsxdq, (d), (s))
#define INSTR_CREATE_vmovntdqa(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vmovntdqa, (d), (s))
#define INSTR_CREATE_vpmovzxbw(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovzxbw, (d), (s))
#define INSTR_CREATE_vpmovzxbd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovzxbd, (d), (s))
#define INSTR_CREATE_vpmovzxbq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovzxbq, (d), (s))
#define INSTR_CREATE_vpmovzxwd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovzxwd, (d), (s))
#define INSTR_CREATE_vpmovzxwq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovzxwq, (d), (s))
#define INSTR_CREATE_vpmovzxdq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovzxdq, (d), (s))
#define INSTR_CREATE_vphminposuw(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vphminposuw, (d), (s))
#define INSTR_CREATE_vaesimc(dc, d, s) instr_create_1dst_1src((dc), OP_vaesimc, (d), (s))
#define INSTR_CREATE_vmovss(dc, d, s) instr_create_1dst_1src((dc), OP_vmovss, (d), (s))
#define INSTR_CREATE_vmovsd(dc, d, s) instr_create_1dst_1src((dc), OP_vmovsd, (d), (s))
#define INSTR_CREATE_vcvtph2ps(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vcvtph2ps, (d), (s))
#define INSTR_CREATE_vbroadcastss(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vbroadcastss, (d), (s))
#define INSTR_CREATE_vbroadcastsd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vbroadcastsd, (d), (s))
#define INSTR_CREATE_vbroadcastf128(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vbroadcastf128, (d), (s))
#define INSTR_CREATE_movq2dq(dc, d, s) instr_create_1dst_1src((dc), OP_movq2dq, (d), (s))
#define INSTR_CREATE_movdq2q(dc, d, s) instr_create_1dst_1src((dc), OP_movdq2q, (d), (s))
#define INSTR_CREATE_vpabsb(dc, d, s) instr_create_1dst_1src((dc), OP_vpabsb, (d), (s))
#define INSTR_CREATE_vpabsw(dc, d, s) instr_create_1dst_1src((dc), OP_vpabsw, (d), (s))
#define INSTR_CREATE_vpabsd(dc, d, s) instr_create_1dst_1src((dc), OP_vpabsd, (d), (s))
/* XOP */
#define INSTR_CREATE_vfrczps(dc, d, s) instr_create_1dst_1src((dc), OP_vfrczps, (d), (s))
#define INSTR_CREATE_vfrczpd(dc, d, s) instr_create_1dst_1src((dc), OP_vfrczpd, (d), (s))
#define INSTR_CREATE_vfrczss(dc, d, s) instr_create_1dst_1src((dc), OP_vfrczss, (d), (s))
#define INSTR_CREATE_vfrczsd(dc, d, s) instr_create_1dst_1src((dc), OP_vfrczsd, (d), (s))
#define INSTR_CREATE_vphaddbw(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vphaddbw, (d), (s))
#define INSTR_CREATE_vphaddbd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vphaddbd, (d), (s))
#define INSTR_CREATE_vphaddbq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vphaddbq, (d), (s))
#define INSTR_CREATE_vphaddwd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vphaddwd, (d), (s))
#define INSTR_CREATE_vphaddwq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vphaddwq, (d), (s))
#define INSTR_CREATE_vphadddq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vphadddq, (d), (s))
#define INSTR_CREATE_vphaddubw(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vphaddubw, (d), (s))
#define INSTR_CREATE_vphaddubd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vphaddubd, (d), (s))
#define INSTR_CREATE_vphaddubq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vphaddubq, (d), (s))
#define INSTR_CREATE_vphadduwd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vphadduwd, (d), (s))
#define INSTR_CREATE_vphadduwq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vphadduwq, (d), (s))
#define INSTR_CREATE_vphaddudq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vphaddudq, (d), (s))
#define INSTR_CREATE_vphsubbw(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vphsubbw, (d), (s))
#define INSTR_CREATE_vphsubwd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vphsubwd, (d), (s))
#define INSTR_CREATE_vphsubdq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vphsubdq, (d), (s))
/* TBM */
#define INSTR_CREATE_blcfill(dc, d, s) instr_create_1dst_1src((dc), OP_blcfill, (d), (s))
#define INSTR_CREATE_blci(dc, d, s) instr_create_1dst_1src((dc), OP_blci, (d), (s))
#define INSTR_CREATE_blcic(dc, d, s) instr_create_1dst_1src((dc), OP_blcic, (d), (s))
#define INSTR_CREATE_blcmsk(dc, d, s) instr_create_1dst_1src((dc), OP_blcmsk, (d), (s))
#define INSTR_CREATE_blcs(dc, d, s) instr_create_1dst_1src((dc), OP_blcs, (d), (s))
#define INSTR_CREATE_blsfill(dc, d, s) instr_create_1dst_1src((dc), OP_blsfill, (d), (s))
#define INSTR_CREATE_blsic(dc, d, s) instr_create_1dst_1src((dc), OP_blsic, (d), (s))
#define INSTR_CREATE_t1mskc(dc, d, s) instr_create_1dst_1src((dc), OP_t1mskc, (d), (s))
#define INSTR_CREATE_tzmsk(dc, d, s) instr_create_1dst_1src((dc), OP_tzmsk, (d), (s))
/* BL1 */
#define INSTR_CREATE_blsr(dc, d, s) instr_create_1dst_1src((dc), OP_blsr, (d), (s))
#define INSTR_CREATE_blsmsk(dc, d, s) instr_create_1dst_1src((dc), OP_blsmsk, (d), (s))
#define INSTR_CREATE_blsi(dc, d, s) instr_create_1dst_1src((dc), OP_blsi, (d), (s))
#define INSTR_CREATE_tzcnt(dc, d, s) instr_create_1dst_1src((dc), OP_tzcnt, (d), (s))
/* AVX2 */
#define INSTR_CREATE_vbroadcasti128(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vbroadcasti128, (d), (s))
#define INSTR_CREATE_vpbroadcastb(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpbroadcastb, (d), (s))
#define INSTR_CREATE_vpbroadcastw(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpbroadcastw, (d), (s))
#define INSTR_CREATE_vpbroadcastd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpbroadcastd, (d), (s))
#define INSTR_CREATE_vpbroadcastq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpbroadcastq, (d), (s))
/* AVX-512 VEX */
#define INSTR_CREATE_kmovw(dc, d, s) instr_create_1dst_1src((dc), OP_kmovw, (d), (s))
#define INSTR_CREATE_kmovb(dc, d, s) instr_create_1dst_1src((dc), OP_kmovb, (d), (s))
#define INSTR_CREATE_kmovq(dc, d, s) instr_create_1dst_1src((dc), OP_kmovq, (d), (s))
#define INSTR_CREATE_kmovd(dc, d, s) instr_create_1dst_1src((dc), OP_kmovd, (d), (s))
#define INSTR_CREATE_knotw(dc, d, s) instr_create_1dst_1src((dc), OP_knotw, (d), (s))
#define INSTR_CREATE_knotb(dc, d, s) instr_create_1dst_1src((dc), OP_knotb, (d), (s))
#define INSTR_CREATE_knotq(dc, d, s) instr_create_1dst_1src((dc), OP_knotq, (d), (s))
#define INSTR_CREATE_knotd(dc, d, s) instr_create_1dst_1src((dc), OP_knotd, (d), (s))
#define INSTR_CREATE_kortestw(dc, d, s) \
    instr_create_1dst_1src((dc), OP_kortestw, (d), (s))
#define INSTR_CREATE_kortestb(dc, d, s) \
    instr_create_1dst_1src((dc), OP_kortestb, (d), (s))
#define INSTR_CREATE_kortestq(dc, d, s) \
    instr_create_1dst_1src((dc), OP_kortestq, (d), (s))
#define INSTR_CREATE_kortestd(dc, d, s) \
    instr_create_1dst_1src((dc), OP_kortestd, (d), (s))
#define INSTR_CREATE_ktestw(dc, d, s) instr_create_1dst_1src((dc), OP_ktestw, (d), (s))
#define INSTR_CREATE_ktestb(dc, d, s) instr_create_1dst_1src((dc), OP_ktestb, (d), (s))
#define INSTR_CREATE_ktestq(dc, d, s) instr_create_1dst_1src((dc), OP_ktestq, (d), (s))
#define INSTR_CREATE_ktestd(dc, d, s) instr_create_1dst_1src((dc), OP_ktestd, (d), (s))
/* AVX-512 EVEX */
#define INSTR_CREATE_vmovd_mask(dc, d, s) instr_create_1dst_1src((dc), OP_vmovd, (d), (s))
#define INSTR_CREATE_vpmovm2b(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovm2b, (d), (s))
#define INSTR_CREATE_vpmovm2w(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovm2w, (d), (s))
#define INSTR_CREATE_vpmovm2d(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovm2d, (d), (s))
#define INSTR_CREATE_vpmovm2q(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovm2q, (d), (s))
#define INSTR_CREATE_vpmovb2m(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovb2m, (d), (s))
#define INSTR_CREATE_vpmovw2m(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovw2m, (d), (s))
#define INSTR_CREATE_vpmovd2m(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovd2m, (d), (s))
#define INSTR_CREATE_vpmovq2m(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpmovq2m, (d), (s))
#define INSTR_CREATE_vpbroadcastmb2q(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpbroadcastmb2q, (d), (s))
#define INSTR_CREATE_vpbroadcastmw2d(dc, d, s) \
    instr_create_1dst_1src((dc), OP_vpbroadcastmw2d, (d), (s))
/* MPX */
#define INSTR_CREATE_bndmov(dc, d, s) instr_create_1dst_1src((dc), OP_bndmov, (d), (s))
#define INSTR_CREATE_bndcl(dc, d, s) instr_create_1dst_1src((dc), OP_bndcl, (d), (s))
#define INSTR_CREATE_bndcu(dc, d, s) instr_create_1dst_1src((dc), OP_bndcu, (d), (s))
#define INSTR_CREATE_bndcn(dc, d, s) instr_create_1dst_1src((dc), OP_bndcn, (d), (s))
#define INSTR_CREATE_bndmk(dc, d, s) instr_create_1dst_1src((dc), OP_bndmk, (d), (s))
#define INSTR_CREATE_bndldx(dc, d, s) instr_create_1dst_1src((dc), OP_bndldx, (d), (s))
#define INSTR_CREATE_bndstx(dc, d, s) instr_create_1dst_1src((dc), OP_bndstx, (d), (s))
/** @} */ /* end doxygen group */

/* 1 destination, 1 implicit source */
/** @name 1 destination, 1 implicit source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 */
#define INSTR_CREATE_inc(dc, d) instr_create_1dst_1src((dc), OP_inc, (d), (d))
#define INSTR_CREATE_dec(dc, d) instr_create_1dst_1src((dc), OP_dec, (d), (d))
/* FIXME: check that d is a 32-bit reg? */
#define INSTR_CREATE_bswap(dc, d) instr_create_1dst_1src((dc), OP_bswap, (d), (d))
#define INSTR_CREATE_not(dc, d) instr_create_1dst_1src((dc), OP_not, (d), (d))
#define INSTR_CREATE_neg(dc, d) instr_create_1dst_1src((dc), OP_neg, (d), (d))
/** @} */ /* end doxygen group */

/* 1 implicit destination, 1 implicit source */
/** @name 1 implicit destination, 1 implicit source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx, automatically
 * supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define INSTR_CREATE_cdq(dc)                                          \
    instr_create_1dst_1src((dc), OP_cdq, opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX))
#define INSTR_CREATE_daa(dc)                                         \
    instr_create_1dst_1src((dc), OP_daa, opnd_create_reg(DR_REG_AL), \
                           opnd_create_reg(DR_REG_AL))
#define INSTR_CREATE_das(dc)                                         \
    instr_create_1dst_1src((dc), OP_das, opnd_create_reg(DR_REG_AL), \
                           opnd_create_reg(DR_REG_AL))
#define INSTR_CREATE_aaa(dc)                                         \
    instr_create_1dst_1src((dc), OP_aaa, opnd_create_reg(DR_REG_AX), \
                           opnd_create_reg(DR_REG_AX))
#define INSTR_CREATE_aas(dc)                                         \
    instr_create_1dst_1src((dc), OP_aas, opnd_create_reg(DR_REG_AX), \
                           opnd_create_reg(DR_REG_AX))
#define INSTR_CREATE_cwde(dc)                                          \
    instr_create_1dst_1src((dc), OP_cwde, opnd_create_reg(DR_REG_EAX), \
                           opnd_create_reg(DR_REG_AX))
#define INSTR_CREATE_xlat(dc)                      \
    instr_create_1dst_1src(                        \
        (dc), OP_xlat, opnd_create_reg(DR_REG_AL), \
        opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XBX, DR_REG_AL, 1, 0, OPSZ_xlat))
#define INSTR_CREATE_xend(dc)                                                      \
    INSTR_PRED(instr_create_1dst_0src((dc), OP_xend, opnd_create_reg(DR_REG_EAX)), \
               DR_PRED_COMPLEX)
#define INSTR_CREATE_sysexit(dc)                                          \
    instr_create_1dst_1src((dc), OP_sysexit, opnd_create_reg(DR_REG_XSP), \
                           opnd_create_reg(DR_REG_XCX))
/** @} */ /* end doxygen group */

/** @name In with no explicit sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * Creates an instr_t for an OP_in instruction with a source of al
 * (INSTR_CREATE_in_1()) or eax (INSTR_CREATE_in_4()) and dx.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define INSTR_CREATE_in_1(dc)                                       \
    instr_create_1dst_1src((dc), OP_in, opnd_create_reg(DR_REG_AL), \
                           opnd_create_reg(DR_REG_DX))
#define INSTR_CREATE_in_4(dc)                                        \
    instr_create_1dst_1src((dc), OP_in, opnd_create_reg(DR_REG_EAX), \
                           opnd_create_reg(DR_REG_DX))
/** @} */ /* end doxygen group */
/** @name In with explicit source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * Creates an instr_t for an OP_in instruction with a source of al
 * (INSTR_CREATE_in_1_imm()) or eax (INSTR_CREATE_in_4_imm()) and an immediate.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param i The opnd_t explicit source operand for the instruction, which must be an
 * immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_in_1_imm(dc, i) \
    instr_create_1dst_1src((dc), OP_in, opnd_create_reg(DR_REG_AL), (i))
#define INSTR_CREATE_in_4_imm(dc, i) \
    instr_create_1dst_1src((dc), OP_in, opnd_create_reg(DR_REG_EAX), (i))
/** @} */ /* end doxygen group */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param i The opnd_t explicit source operand for the instruction, which must be an
 * immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_xabort(dc, i) \
    instr_create_1dst_1src((dc), OP_xabort, opnd_create_reg(DR_REG_EAX), (i))

/* floating-point */
/**
 * Creates an instr_t for a conditional move instruction with the given opcode
 * and destination operand.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param op The OP_xxx opcode for the instruction, which should be in the range
 * [OP_fcmovb, OP_fcmovnu], excluding OP_fucompp.
 * \param f The opnd_t explicit source operand for the instruction, which must
 * be a floating point register (opnd_create_reg()).
 */
#define INSTR_CREATE_fcmovcc(dc, op, f)                                              \
    INSTR_PRED(instr_create_1dst_1src((dc), (op), opnd_create_reg(DR_REG_ST0), (f)), \
               DR_PRED_O + instr_cmovcc_to_jcc(op) - OP_jo)
/** @name Floating point with destination that is memory or fp register */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction, which must
 * be one of the following:
 * -# A floating point register (opnd_create_reg()).
 * -# A memory reference (opnd_create_base_disp() or opnd_create_far_base_disp()).
 */
#define INSTR_CREATE_fst(dc, d) \
    instr_create_1dst_1src((dc), OP_fst, (d), opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_fstp(dc, d) \
    instr_create_1dst_1src((dc), OP_fstp, (d), opnd_create_reg(DR_REG_ST0))
/** @} */ /* end doxygen group */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s The opnd_t explicit source operand for the instruction, which must
 * be one of the following:
 * -# A floating point register (opnd_create_reg()).
 * -# A memory reference (opnd_create_base_disp() or opnd_create_far_base_disp()).
 */
#define INSTR_CREATE_fld(dc, s) \
    instr_create_1dst_1src((dc), OP_fld, opnd_create_reg(DR_REG_ST0), (s))
/** @name Floating-point with memory destination and implicit source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx_mem macro creates an instr_t with opcode OP_xxx and
 * the given explicit memory operand, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param m The opnd_t explicit destination operand for the instruction, which must be
 * a memory reference (opnd_create_base_disp() or opnd_create_far_base_disp()).
 */
#define INSTR_CREATE_fist(dc, m) \
    instr_create_1dst_1src((dc), OP_fist, (m), opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_fistp(dc, m) \
    instr_create_1dst_1src((dc), OP_fistp, (m), opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_fisttp(dc, m) \
    instr_create_1dst_1src((dc), OP_fisttp, (m), opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_fbstp(dc, m) \
    instr_create_1dst_1src((dc), OP_fbstp, (m), opnd_create_reg(DR_REG_ST0))
/** @} */ /* end doxygen group */
/** @name Floating-point with memory source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx_mem macro creates an instr_t with opcode OP_xxx and
 * the given explicit memory operand, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param m The opnd_t explicit source operand for the instruction, which must be
 * a memory reference (opnd_create_base_disp() or opnd_create_far_base_disp()).
 */
#define INSTR_CREATE_fild(dc, m) \
    instr_create_1dst_1src((dc), OP_fild, opnd_create_reg(DR_REG_ST0), (m))
#define INSTR_CREATE_fbld(dc, m) \
    instr_create_1dst_1src((dc), OP_fbld, opnd_create_reg(DR_REG_ST0), (m))
/** @} */ /* end doxygen group */
/** @name Floating-point implicit destination and implicit source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx, automatically
 * supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define INSTR_CREATE_fchs(dc)                                          \
    instr_create_1dst_1src((dc), OP_fchs, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_fabs(dc)                                          \
    instr_create_1dst_1src((dc), OP_fabs, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_ftst(dc)                                          \
    instr_create_1dst_1src((dc), OP_ftst, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_immed_float(0.0f))
#define INSTR_CREATE_fld1(dc)                                          \
    instr_create_1dst_1src((dc), OP_fld1, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_immed_float(1.0f))
/* FIXME: do we really want these constants here? Should they be floats or doubles? */
#define INSTR_CREATE_fldl2t(dc)                                          \
    instr_create_1dst_1src((dc), OP_fldl2t, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_immed_float((float)M_LN10 / (float)M_LN2))
#define INSTR_CREATE_fldl2e(dc)                                          \
    instr_create_1dst_1src((dc), OP_fldl2e, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_immed_float(1.0f / (float)M_LN2))
#define INSTR_CREATE_fldpi(dc)                                          \
    instr_create_1dst_1src((dc), OP_fldpi, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_immed_float((float)M_PI))
#define INSTR_CREATE_fldlg2(dc)                                          \
    instr_create_1dst_1src((dc), OP_fldlg2, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_immed_float((float)M_LN2 / (float)M_LN10))
#define INSTR_CREATE_fldln2(dc)                                          \
    instr_create_1dst_1src((dc), OP_fldln2, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_immed_float((float)M_LN2))
#define INSTR_CREATE_fldz(dc)                                          \
    instr_create_1dst_1src((dc), OP_fldz, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_immed_float(0.0f))
#define INSTR_CREATE_f2xm1(dc)                                          \
    instr_create_1dst_1src((dc), OP_f2xm1, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_fptan(dc)                                          \
    instr_create_1dst_1src((dc), OP_fptan, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_fxtract(dc)                                          \
    instr_create_1dst_1src((dc), OP_fxtract, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_fsqrt(dc)                                          \
    instr_create_1dst_1src((dc), OP_fsqrt, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_fsincos(dc)                                          \
    instr_create_1dst_1src((dc), OP_fsincos, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_frndint(dc)                                          \
    instr_create_1dst_1src((dc), OP_frndint, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_fsin(dc)                                          \
    instr_create_1dst_1src((dc), OP_fsin, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_fcos(dc)                                          \
    instr_create_1dst_1src((dc), OP_fcos, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_reg(DR_REG_ST0))

#define INSTR_CREATE_fscale(dc)                                          \
    instr_create_1dst_2src((dc), OP_fscale, opnd_create_reg(DR_REG_ST0), \
                           opnd_create_reg(DR_REG_ST1), opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_fyl2x(dc)                                                       \
    instr_create_2dst_2src((dc), OP_fyl2x, opnd_create_reg(DR_REG_ST0),              \
                           opnd_create_reg(DR_REG_ST1), opnd_create_reg(DR_REG_ST0), \
                           opnd_create_reg(DR_REG_ST1))
#define INSTR_CREATE_fyl2xp1(dc)                                                     \
    instr_create_2dst_2src((dc), OP_fyl2xp1, opnd_create_reg(DR_REG_ST0),            \
                           opnd_create_reg(DR_REG_ST1), opnd_create_reg(DR_REG_ST0), \
                           opnd_create_reg(DR_REG_ST1))
#define INSTR_CREATE_fpatan(dc)                                                      \
    instr_create_2dst_2src((dc), OP_fpatan, opnd_create_reg(DR_REG_ST0),             \
                           opnd_create_reg(DR_REG_ST1), opnd_create_reg(DR_REG_ST0), \
                           opnd_create_reg(DR_REG_ST1))
#define INSTR_CREATE_fprem(dc)                                                       \
    instr_create_2dst_2src((dc), OP_fprem, opnd_create_reg(DR_REG_ST0),              \
                           opnd_create_reg(DR_REG_ST1), opnd_create_reg(DR_REG_ST0), \
                           opnd_create_reg(DR_REG_ST1))
#define INSTR_CREATE_fprem1(dc)                                                      \
    instr_create_2dst_2src((dc), OP_fprem1, opnd_create_reg(DR_REG_ST0),             \
                           opnd_create_reg(DR_REG_ST1), opnd_create_reg(DR_REG_ST0), \
                           opnd_create_reg(DR_REG_ST1))
/** @} */ /* end doxygen group */

/* 1 destination, 2 sources */
/** @name 1 destination, 2 sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s The opnd_t explicit source operand for the instruction.
 * \param i The opnd_t explicit second source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_pshufw(dc, d, s, i) \
    instr_create_1dst_2src((dc), OP_pshufw, (d), (s), (i))
#define INSTR_CREATE_pshufd(dc, d, s, i) \
    instr_create_1dst_2src((dc), OP_pshufd, (d), (s), (i))
#define INSTR_CREATE_pshufhw(dc, d, s, i) \
    instr_create_1dst_2src((dc), OP_pshufhw, (d), (s), (i))
#define INSTR_CREATE_pshuflw(dc, d, s, i) \
    instr_create_1dst_2src((dc), OP_pshuflw, (d), (s), (i))
#define INSTR_CREATE_pinsrw(dc, d, s, i) \
    instr_create_1dst_2src((dc), OP_pinsrw, (d), (s), (i))
#define INSTR_CREATE_pextrw(dc, d, s, i) \
    instr_create_1dst_2src((dc), OP_pextrw, (d), (s), (i))
/* SSE4 */
#define INSTR_CREATE_pextrb(dc, d, s, i) \
    instr_create_1dst_2src((dc), OP_pextrb, (d), (s), (i))
#define INSTR_CREATE_pextrd(dc, d, s, i) \
    instr_create_1dst_2src((dc), OP_pextrd, (d), (s), (i))
#define INSTR_CREATE_extractps(dc, d, s, i) \
    instr_create_1dst_2src((dc), OP_extractps, (d), (s), (i))
#define INSTR_CREATE_roundps(dc, d, s, i) \
    instr_create_1dst_2src((dc), OP_roundps, (d), (s), (i))
#define INSTR_CREATE_roundpd(dc, d, s, i) \
    instr_create_1dst_2src((dc), OP_roundpd, (d), (s), (i))
#define INSTR_CREATE_roundss(dc, d, s, i) \
    instr_create_1dst_2src((dc), OP_roundss, (d), (s), (i))
#define INSTR_CREATE_roundsd(dc, d, s, i) \
    instr_create_1dst_2src((dc), OP_roundsd, (d), (s), (i))
#define INSTR_CREATE_pinsrb(dc, d, s, i) \
    instr_create_1dst_2src((dc), OP_pinsrb, (d), (s), (i))
#define INSTR_CREATE_insertps(dc, d, s, i) \
    instr_create_1dst_2src((dc), OP_insertps, (d), (s), (i))
#define INSTR_CREATE_pinsrd(dc, d, s, i) \
    instr_create_1dst_2src((dc), OP_pinsrd, (d), (s), (i))
#define INSTR_CREATE_aeskeygenassist(dc, d, s, i) \
    instr_create_1dst_2src((dc), OP_aeskeygenassist, (d), (s), (i))
/** @} */ /* end doxygen group */

/* 1 destination, 2 sources */
/** @name 1 destination, 1 mask, and 2 sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param k The opnd_t explicit mask source operand for the instruction.
 * \param i The opnd_t explicit second source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()).
 * \param s The opnd_t explicit source operand for the instruction.
 */
/* AVX-512 EVEX */
#define INSTR_CREATE_vpshufhw_mask(dc, d, k, i, s) \
    instr_create_1dst_3src((dc), OP_vpshufhw, (d), (k), (i), (s))
#define INSTR_CREATE_vpshufd_mask(dc, d, k, i, s) \
    instr_create_1dst_3src((dc), OP_vpshufd, (d), (k), (i), (s))
#define INSTR_CREATE_vpshuflw_mask(dc, d, k, i, s) \
    instr_create_1dst_3src((dc), OP_vpshuflw, (d), (k), (i), (s))
#define INSTR_CREATE_vgetmantps_mask(dc, d, k, i, s) \
    instr_create_1dst_3src((dc), OP_vgetmantps, (d), (k), (i), (s))
#define INSTR_CREATE_vgetmantpd_mask(dc, d, k, i, s) \
    instr_create_1dst_3src((dc), OP_vgetmantpd, (d), (k), (i), (s))
#define INSTR_CREATE_vreduceps_mask(dc, d, k, i, s) \
    instr_create_1dst_3src((dc), OP_vreduceps, (d), (k), (i), (s))
#define INSTR_CREATE_vreducepd_mask(dc, d, k, i, s) \
    instr_create_1dst_3src((dc), OP_vreducepd, (d), (k), (i), (s))
#define INSTR_CREATE_vrndscaleps_mask(dc, d, k, i, s) \
    instr_create_1dst_3src((dc), OP_vrndscaleps, (d), (k), (i), (s))
#define INSTR_CREATE_vrndscalepd_mask(dc, d, k, i, s) \
    instr_create_1dst_3src((dc), OP_vrndscalepd, (d), (k), (i), (s))
#define INSTR_CREATE_vfpclassps_mask(dc, d, k, i, s) \
    instr_create_1dst_3src((dc), OP_vfpclassps, (d), (k), (i), (s))
#define INSTR_CREATE_vfpclasspd_mask(dc, d, k, i, s) \
    instr_create_1dst_3src((dc), OP_vfpclasspd, (d), (k), (i), (s))
#define INSTR_CREATE_vfpclassss_mask(dc, d, k, i, s) \
    instr_create_1dst_3src((dc), OP_vfpclassss, (d), (k), (i), (s))
#define INSTR_CREATE_vfpclasssd_mask(dc, d, k, i, s) \
    instr_create_1dst_3src((dc), OP_vfpclasssd, (d), (k), (i), (s))
/** @} */ /* end doxygen group */

/** @name 1 destination, 2 non-immediate sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s1 The opnd_t explicit first source operand for the instruction.
 * \param s2 The opnd_t explicit second source operand for the instruction
 */
/* AVX: some of these have immeds, we don't bother to distinguish */
/* NDS means "Non-Destructive Source" */
#define INSTR_CREATE_vmovlps_NDS(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vmovlps, (d), (s1), (s2))
#define INSTR_CREATE_vmovlpd_NDS(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vmovlpd, (d), (s1), (s2))
#define INSTR_CREATE_vunpcklps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vunpcklps, (d), (s1), (s2))
#define INSTR_CREATE_vunpcklpd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vunpcklpd, (d), (s1), (s2))
#define INSTR_CREATE_vunpckhps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vunpckhps, (d), (s1), (s2))
#define INSTR_CREATE_vunpckhpd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vunpckhpd, (d), (s1), (s2))
#define INSTR_CREATE_vmovhps_NDS(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vmovhps, (d), (s1), (s2))
#define INSTR_CREATE_vmovhpd_NDS(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vmovhpd, (d), (s1), (s2))
#define INSTR_CREATE_vcvtsi2ss(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vcvtsi2ss, (d), (s1), (s2))
#define INSTR_CREATE_vcvtsi2sd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vcvtsi2sd, (d), (s1), (s2))
#define INSTR_CREATE_vsqrtss(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vsqrtss, (d), (s1), (s2))
#define INSTR_CREATE_vsqrtsd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vsqrtsd, (d), (s1), (s2))
#define INSTR_CREATE_vrsqrtss(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vrsqrtss, (d), (s1), (s2))
#define INSTR_CREATE_vrcpss(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vrcpss, (d), (s1), (s2))
#define INSTR_CREATE_vandps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vandps, (d), (s1), (s2))
#define INSTR_CREATE_vandpd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vandpd, (d), (s1), (s2))
#define INSTR_CREATE_vandnps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vandnps, (d), (s1), (s2))
#define INSTR_CREATE_vandnpd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vandnpd, (d), (s1), (s2))
#define INSTR_CREATE_vorps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vorps, (d), (s1), (s2))
#define INSTR_CREATE_vorpd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vorpd, (d), (s1), (s2))
#define INSTR_CREATE_vxorps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vxorps, (d), (s1), (s2))
#define INSTR_CREATE_vxorpd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vxorpd, (d), (s1), (s2))
#define INSTR_CREATE_vaddps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vaddps, (d), (s1), (s2))
#define INSTR_CREATE_vaddss(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vaddss, (d), (s1), (s2))
#define INSTR_CREATE_vaddpd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vaddpd, (d), (s1), (s2))
#define INSTR_CREATE_vaddsd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vaddsd, (d), (s1), (s2))
#define INSTR_CREATE_vmulps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vmulps, (d), (s1), (s2))
#define INSTR_CREATE_vmulss(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vmulss, (d), (s1), (s2))
#define INSTR_CREATE_vmulpd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vmulpd, (d), (s1), (s2))
#define INSTR_CREATE_vmulsd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vmulsd, (d), (s1), (s2))
#define INSTR_CREATE_vcvtss2sd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vcvtss2sd, (d), (s1), (s2))
#define INSTR_CREATE_vcvtsd2ss(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vcvtsd2ss, (d), (s1), (s2))
#define INSTR_CREATE_vsubps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vsubps, (d), (s1), (s2))
#define INSTR_CREATE_vsubss(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vsubss, (d), (s1), (s2))
#define INSTR_CREATE_vsubpd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vsubpd, (d), (s1), (s2))
#define INSTR_CREATE_vsubsd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vsubsd, (d), (s1), (s2))
#define INSTR_CREATE_vminps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vminps, (d), (s1), (s2))
#define INSTR_CREATE_vminss(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vminss, (d), (s1), (s2))
#define INSTR_CREATE_vminpd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vminpd, (d), (s1), (s2))
#define INSTR_CREATE_vminsd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vminsd, (d), (s1), (s2))
#define INSTR_CREATE_vdivps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vdivps, (d), (s1), (s2))
#define INSTR_CREATE_vdivss(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vdivss, (d), (s1), (s2))
#define INSTR_CREATE_vdivpd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vdivpd, (d), (s1), (s2))
#define INSTR_CREATE_vdivsd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vdivsd, (d), (s1), (s2))
#define INSTR_CREATE_vmaxps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vmaxps, (d), (s1), (s2))
#define INSTR_CREATE_vmaxss(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vmaxss, (d), (s1), (s2))
#define INSTR_CREATE_vmaxpd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vmaxpd, (d), (s1), (s2))
#define INSTR_CREATE_vmaxsd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vmaxsd, (d), (s1), (s2))
#define INSTR_CREATE_vpunpcklbw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpunpcklbw, (d), (s1), (s2))
#define INSTR_CREATE_vpunpcklwd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpunpcklwd, (d), (s1), (s2))
#define INSTR_CREATE_vpunpckldq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpunpckldq, (d), (s1), (s2))
#define INSTR_CREATE_vpacksswb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpacksswb, (d), (s1), (s2))
#define INSTR_CREATE_vpcmpgtb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpcmpgtb, (d), (s1), (s2))
#define INSTR_CREATE_vpcmpgtw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpcmpgtw, (d), (s1), (s2))
#define INSTR_CREATE_vpcmpgtd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpcmpgtd, (d), (s1), (s2))
#define INSTR_CREATE_vpackuswb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpackuswb, (d), (s1), (s2))
#define INSTR_CREATE_vpunpckhbw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpunpckhbw, (d), (s1), (s2))
#define INSTR_CREATE_vpunpckhwd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpunpckhwd, (d), (s1), (s2))
#define INSTR_CREATE_vpunpckhdq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpunpckhdq, (d), (s1), (s2))
#define INSTR_CREATE_vpackssdw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpackssdw, (d), (s1), (s2))
#define INSTR_CREATE_vpunpcklqdq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpunpcklqdq, (d), (s1), (s2))
#define INSTR_CREATE_vpunpckhqdq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpunpckhqdq, (d), (s1), (s2))
#define INSTR_CREATE_vpshufhw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpshufhw, (d), (s1), (s2))
#define INSTR_CREATE_vpshufd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpshufd, (d), (s1), (s2))
#define INSTR_CREATE_vpshuflw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpshuflw, (d), (s1), (s2))
#define INSTR_CREATE_vpcmpeqb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpcmpeqb, (d), (s1), (s2))
#define INSTR_CREATE_vpcmpeqw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpcmpeqw, (d), (s1), (s2))
#define INSTR_CREATE_vpcmpeqd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpcmpeqd, (d), (s1), (s2))
#define INSTR_CREATE_vpextrw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpextrw, (d), (s1), (s2))
#define INSTR_CREATE_vpsrlw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsrlw, (d), (s1), (s2))
#define INSTR_CREATE_vpsrld(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsrld, (d), (s1), (s2))
#define INSTR_CREATE_vpsrlq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsrlq, (d), (s1), (s2))
#define INSTR_CREATE_vpaddq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpaddq, (d), (s1), (s2))
#define INSTR_CREATE_vpmullw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpmullw, (d), (s1), (s2))
#define INSTR_CREATE_vpsubusb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsubusb, (d), (s1), (s2))
#define INSTR_CREATE_vpsubusw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsubusw, (d), (s1), (s2))
#define INSTR_CREATE_vpminub(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpminub, (d), (s1), (s2))
#define INSTR_CREATE_vpand(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpand, (d), (s1), (s2))
#define INSTR_CREATE_vpaddusb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpaddusb, (d), (s1), (s2))
#define INSTR_CREATE_vpaddusw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpaddusw, (d), (s1), (s2))
#define INSTR_CREATE_vpmaxub(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpmaxub, (d), (s1), (s2))
#define INSTR_CREATE_vpandn(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpandn, (d), (s1), (s2))
#define INSTR_CREATE_vpavgb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpavgb, (d), (s1), (s2))
#define INSTR_CREATE_vpsraw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsraw, (d), (s1), (s2))
#define INSTR_CREATE_vpsrad(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsrad, (d), (s1), (s2))
#define INSTR_CREATE_vpavgw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpavgw, (d), (s1), (s2))
#define INSTR_CREATE_vpmulhuw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpmulhuw, (d), (s1), (s2))
#define INSTR_CREATE_vpmulhw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpmulhw, (d), (s1), (s2))
#define INSTR_CREATE_vpsubsb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsubsb, (d), (s1), (s2))
#define INSTR_CREATE_vpsubsw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsubsw, (d), (s1), (s2))
#define INSTR_CREATE_vpminsw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpminsw, (d), (s1), (s2))
#define INSTR_CREATE_vpor(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpor, (d), (s1), (s2))
#define INSTR_CREATE_vpaddsb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpaddsb, (d), (s1), (s2))
#define INSTR_CREATE_vpaddsw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpaddsw, (d), (s1), (s2))
#define INSTR_CREATE_vpmaxsw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpmaxsw, (d), (s1), (s2))
#define INSTR_CREATE_vpxor(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpxor, (d), (s1), (s2))
#define INSTR_CREATE_vpsllw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsllw, (d), (s1), (s2))
#define INSTR_CREATE_vpslld(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpslld, (d), (s1), (s2))
#define INSTR_CREATE_vpsllq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsllq, (d), (s1), (s2))
#define INSTR_CREATE_vpmuludq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpmuludq, (d), (s1), (s2))
#define INSTR_CREATE_vpmaddwd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpmaddwd, (d), (s1), (s2))
#define INSTR_CREATE_vpsadbw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsadbw, (d), (s1), (s2))
#define INSTR_CREATE_vpsubb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsubb, (d), (s1), (s2))
#define INSTR_CREATE_vpsubw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsubw, (d), (s1), (s2))
#define INSTR_CREATE_vpsubd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsubd, (d), (s1), (s2))
#define INSTR_CREATE_vpsubq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsubq, (d), (s1), (s2))
#define INSTR_CREATE_vpaddb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpaddb, (d), (s1), (s2))
#define INSTR_CREATE_vpaddw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpaddw, (d), (s1), (s2))
#define INSTR_CREATE_vpaddd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpaddd, (d), (s1), (s2))
#define INSTR_CREATE_vpsrldq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsrldq, (d), (s1), (s2))
#define INSTR_CREATE_vpslldq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpslldq, (d), (s1), (s2))
#define INSTR_CREATE_vhaddpd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vhaddpd, (d), (s1), (s2))
#define INSTR_CREATE_vhaddps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vhaddps, (d), (s1), (s2))
#define INSTR_CREATE_vhsubpd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vhsubpd, (d), (s1), (s2))
#define INSTR_CREATE_vhsubps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vhsubps, (d), (s1), (s2))
#define INSTR_CREATE_vaddsubpd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vaddsubpd, (d), (s1), (s2))
#define INSTR_CREATE_vaddsubps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vaddsubps, (d), (s1), (s2))
#define INSTR_CREATE_vpshufb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpshufb, (d), (s1), (s2))
#define INSTR_CREATE_vphaddw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vphaddw, (d), (s1), (s2))
#define INSTR_CREATE_vphaddd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vphaddd, (d), (s1), (s2))
#define INSTR_CREATE_vphaddsw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vphaddsw, (d), (s1), (s2))
#define INSTR_CREATE_vpmaddubsw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpmaddubsw, (d), (s1), (s2))
#define INSTR_CREATE_vphsubw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vphsubw, (d), (s1), (s2))
#define INSTR_CREATE_vphsubd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vphsubd, (d), (s1), (s2))
#define INSTR_CREATE_vphsubsw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vphsubsw, (d), (s1), (s2))
#define INSTR_CREATE_vpsignb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsignb, (d), (s1), (s2))
#define INSTR_CREATE_vpsignw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsignw, (d), (s1), (s2))
#define INSTR_CREATE_vpsignd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsignd, (d), (s1), (s2))
#define INSTR_CREATE_vpmulhrsw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpmulhrsw, (d), (s1), (s2))
#define INSTR_CREATE_vpmuldq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpmuldq, (d), (s1), (s2))
#define INSTR_CREATE_vpcmpeqq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpcmpeqq, (d), (s1), (s2))
#define INSTR_CREATE_vpackusdw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpackusdw, (d), (s1), (s2))
#define INSTR_CREATE_vpcmpgtq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpcmpgtq, (d), (s1), (s2))
#define INSTR_CREATE_vpminsb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpminsb, (d), (s1), (s2))
#define INSTR_CREATE_vpminsd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpminsd, (d), (s1), (s2))
#define INSTR_CREATE_vpminuw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpminuw, (d), (s1), (s2))
#define INSTR_CREATE_vpminud(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpminud, (d), (s1), (s2))
#define INSTR_CREATE_vpmaxsb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpmaxsb, (d), (s1), (s2))
#define INSTR_CREATE_vpmaxsd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpmaxsd, (d), (s1), (s2))
#define INSTR_CREATE_vpmaxuw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpmaxuw, (d), (s1), (s2))
#define INSTR_CREATE_vpmaxud(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpmaxud, (d), (s1), (s2))
#define INSTR_CREATE_vpmulld(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpmulld, (d), (s1), (s2))
#define INSTR_CREATE_vaesenc(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vaesenc, (d), (s1), (s2))
#define INSTR_CREATE_vaesenclast(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vaesenclast, (d), (s1), (s2))
#define INSTR_CREATE_vaesdec(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vaesdec, (d), (s1), (s2))
#define INSTR_CREATE_vaesdeclast(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vaesdeclast, (d), (s1), (s2))
#define INSTR_CREATE_vpextrb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpextrb, (d), (s1), (s2))
#define INSTR_CREATE_vpextrd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpextrd, (d), (s1), (s2))
#define INSTR_CREATE_vextractps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vextractps, (d), (s1), (s2))
#define INSTR_CREATE_vroundps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vroundps, (d), (s1), (s2))
#define INSTR_CREATE_vroundpd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vroundpd, (d), (s1), (s2))
#define INSTR_CREATE_vaeskeygenassist(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vaeskeygenassist, (d), (s1), (s2))
#define INSTR_CREATE_vmovss_NDS(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vmovss, (d), (s1), (s2))
#define INSTR_CREATE_vmovsd_NDS(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vmovsd, (d), (s1), (s2))
#define INSTR_CREATE_vcvtps2ph(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vcvtps2ph, (d), (s1), (s2))
#define INSTR_CREATE_vmaskmovps(dc, d, s1, s2)                               \
    INSTR_PRED(instr_create_1dst_2src((dc), OP_vmaskmovps, (d), (s1), (s2)), \
               DR_PRED_COMPLEX)
#define INSTR_CREATE_vmaskmovpd(dc, d, s1, s2)                               \
    INSTR_PRED(instr_create_1dst_2src((dc), OP_vmaskmovpd, (d), (s1), (s2)), \
               DR_PRED_COMPLEX)
#define INSTR_CREATE_vpermilps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpermilps, (d), (s1), (s2))
#define INSTR_CREATE_vpermilpd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpermilpd, (d), (s1), (s2))
#define INSTR_CREATE_vextractf128(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vextractf128, (d), (s1), (s2))
/* XOP */
/* FYI: OP_vprot* have a variant that takes an immediate */
#define INSTR_CREATE_vprotb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vprotb, (d), (s1), (s2))
#define INSTR_CREATE_vprotw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vprotw, (d), (s1), (s2))
#define INSTR_CREATE_vprotd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vprotd, (d), (s1), (s2))
#define INSTR_CREATE_vprotq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vprotq, (d), (s1), (s2))
#define INSTR_CREATE_vpshlb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpshlb, (d), (s1), (s2))
#define INSTR_CREATE_vpshld(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpshld, (d), (s1), (s2))
#define INSTR_CREATE_vpshlq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpshlq, (d), (s1), (s2))
#define INSTR_CREATE_vpshlw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpshlw, (d), (s1), (s2))
#define INSTR_CREATE_vpshab(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpshab, (d), (s1), (s2))
#define INSTR_CREATE_vpshad(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpshad, (d), (s1), (s2))
#define INSTR_CREATE_vpshaq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpshaq, (d), (s1), (s2))
#define INSTR_CREATE_vpshaw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpshaw, (d), (s1), (s2))
/* TBM */
/* Has a variant that takes an immediate */
#define INSTR_CREATE_bextr(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_bextr, (d), (s1), (s2))
/* BMI1 */
#define INSTR_CREATE_andn(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_andn, (d), (s1), (s2))
/* BMI2 */
#define INSTR_CREATE_bzhi(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_bzhi, (d), (s1), (s2))
#define INSTR_CREATE_pext(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_pext, (d), (s1), (s2))
#define INSTR_CREATE_pdep(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_pdep, (d), (s1), (s2))
#define INSTR_CREATE_sarx(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_sarx, (d), (s1), (s2))
#define INSTR_CREATE_shlx(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_shlx, (d), (s1), (s2))
#define INSTR_CREATE_shrx(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_shrx, (d), (s1), (s2))
/* Takes an immediate for s2 */
#define INSTR_CREATE_rorx(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_rorx, (d), (s1), (s2))
/* AVX2 */
#define INSTR_CREATE_vpermps(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpermps, (d), (s1), (s2))
#define INSTR_CREATE_vpermd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpermd, (d), (s1), (s2))
#define INSTR_CREATE_vpsravd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsravd, (d), (s1), (s2))
#define INSTR_CREATE_vextracti128(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vextracti128, (d), (s1), (s2))
#define INSTR_CREATE_vpermq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpermq, (d), (s1), (s2))
#define INSTR_CREATE_vpermpd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpermpd, (d), (s1), (s2))
#define INSTR_CREATE_vpmaskmovd(dc, d, s1, s2)                               \
    INSTR_PRED(instr_create_1dst_2src((dc), OP_vpmaskmovd, (d), (s1), (s2)), \
               DR_PRED_COMPLEX)
#define INSTR_CREATE_vpmaskmovq(dc, d, s1, s2)                               \
    INSTR_PRED(instr_create_1dst_2src((dc), OP_vpmaskmovq, (d), (s1), (s2)), \
               DR_PRED_COMPLEX)
#define INSTR_CREATE_vpsllvd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsllvd, (d), (s1), (s2))
#define INSTR_CREATE_vpsllvq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsllvq, (d), (s1), (s2))
#define INSTR_CREATE_vpsrlvd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsrlvd, (d), (s1), (s2))
#define INSTR_CREATE_vpsrlvq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpsrlvq, (d), (s1), (s2))
/* AVX-512 VEX */
#define INSTR_CREATE_kandw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kandw, (d), (s1), (s2))
#define INSTR_CREATE_kandb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kandb, (d), (s1), (s2))
#define INSTR_CREATE_kandq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kandq, (d), (s1), (s2))
#define INSTR_CREATE_kandd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kandd, (d), (s1), (s2))
#define INSTR_CREATE_kandnw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kandnw, (d), (s1), (s2))
#define INSTR_CREATE_kandnb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kandnb, (d), (s1), (s2))
#define INSTR_CREATE_kandnq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kandnq, (d), (s1), (s2))
#define INSTR_CREATE_kandnd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kandnd, (d), (s1), (s2))
#define INSTR_CREATE_kunpckbw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kunpckbw, (d), (s1), (s2))
#define INSTR_CREATE_kunpckwd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kunpckwd, (d), (s1), (s2))
#define INSTR_CREATE_kunpckdq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kunpckdq, (d), (s1), (s2))
#define INSTR_CREATE_korw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_korw, (d), (s1), (s2))
#define INSTR_CREATE_korb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_korb, (d), (s1), (s2))
#define INSTR_CREATE_korq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_korq, (d), (s1), (s2))
#define INSTR_CREATE_kord(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kord, (d), (s1), (s2))
#define INSTR_CREATE_kxnorw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kxnorw, (d), (s1), (s2))
#define INSTR_CREATE_kxnorb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kxnorb, (d), (s1), (s2))
#define INSTR_CREATE_kxnorq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kxnorq, (d), (s1), (s2))
#define INSTR_CREATE_kxnord(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kxnord, (d), (s1), (s2))
#define INSTR_CREATE_kxorw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kxorw, (d), (s1), (s2))
#define INSTR_CREATE_kxorb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kxorb, (d), (s1), (s2))
#define INSTR_CREATE_kxorq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kxorq, (d), (s1), (s2))
#define INSTR_CREATE_kxord(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kxord, (d), (s1), (s2))
#define INSTR_CREATE_kaddw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kaddw, (d), (s1), (s2))
#define INSTR_CREATE_kaddb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kaddb, (d), (s1), (s2))
#define INSTR_CREATE_kaddq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kaddq, (d), (s1), (s2))
#define INSTR_CREATE_kaddd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kaddd, (d), (s1), (s2))
#define INSTR_CREATE_kshiftlw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kshiftlw, (d), (s1), (s2))
#define INSTR_CREATE_kshiftlb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kshiftlb, (d), (s1), (s2))
#define INSTR_CREATE_kshiftlq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kshiftlq, (d), (s1), (s2))
#define INSTR_CREATE_kshiftld(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kshiftld, (d), (s1), (s2))
#define INSTR_CREATE_kshiftrw(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kshiftrw, (d), (s1), (s2))
#define INSTR_CREATE_kshiftrb(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kshiftrb, (d), (s1), (s2))
#define INSTR_CREATE_kshiftrq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kshiftrq, (d), (s1), (s2))
#define INSTR_CREATE_kshiftrd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_kshiftrd, (d), (s1), (s2))
/* AVX-512 EVEX */
#define INSTR_CREATE_vcvtusi2ss(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vcvtusi2ss, (d), (s1), (s2))
#define INSTR_CREATE_vcvtusi2sd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vcvtusi2sd, (d), (s1), (s2))
#define INSTR_CREATE_vpextrq(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpextrq, (d), (s1), (s2))
/* AVX VNNI */
#define INSTR_CREATE_vpdpbusd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpdpbusd, (d), (s1), (s2))
#define INSTR_CREATE_vpdpbusds(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpdpbusds, (d), (s1), (s2))
#define INSTR_CREATE_vpdpwssd(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpdpwssd, (d), (s1), (s2))
#define INSTR_CREATE_vpdpwssds(dc, d, s1, s2) \
    instr_create_1dst_2src((dc), OP_vpdpwssds, (d), (s1), (s2))
/** @} */ /* end doxygen group */

/** @name 1 destination, 1 mask, and 1 non-immediate source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx_mask macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands. The first source
 * operand is the instruction's mask.
 *
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param k The opnd_t explicit mask source operand for the instruction.
 * \param s The opnd_t explicit first source operand for the instruction
 */
/* AVX-512 EVEX */
#define INSTR_CREATE_vmovups_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vmovups, (d), (k), (s))
#define INSTR_CREATE_vmovupd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vmovupd, (d), (k), (s))
#define INSTR_CREATE_vmovaps_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vmovaps, (d), (k), (s))
#define INSTR_CREATE_vmovapd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vmovapd, (d), (k), (s))
#define INSTR_CREATE_vmovdqa32_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vmovdqa32, (d), (k), (s))
#define INSTR_CREATE_vmovdqa64_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vmovdqa64, (d), (k), (s))
#define INSTR_CREATE_vmovdqu8_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vmovdqu8, (d), (k), (s))
#define INSTR_CREATE_vmovdqu16_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vmovdqu16, (d), (k), (s))
#define INSTR_CREATE_vmovdqu32_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vmovdqu32, (d), (k), (s))
#define INSTR_CREATE_vmovdqu64_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vmovdqu64, (d), (k), (s))
#define INSTR_CREATE_vmovss_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vmovss, (d), (k), (s))
#define INSTR_CREATE_vmovsd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vmovsd, (d), (k), (s))
#define INSTR_CREATE_vmovsldup_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vmovsldup, (d), (k), (s))
#define INSTR_CREATE_vmovddup_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vmovddup, (d), (k), (s))
#define INSTR_CREATE_vmovshdup_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vmovshdup, (d), (k), (s))
#define INSTR_CREATE_vcvtps2pd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtps2pd, (d), (k), (s))
#define INSTR_CREATE_vcvtpd2ps_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtpd2ps, (d), (k), (s))
#define INSTR_CREATE_vcvtdq2ps_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtdq2ps, (d), (k), (s))
#define INSTR_CREATE_vcvttps2dq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvttps2dq, (d), (k), (s))
#define INSTR_CREATE_vcvtps2dq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtps2dq, (d), (k), (s))
#define INSTR_CREATE_vcvtdq2pd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtdq2pd, (d), (k), (s))
#define INSTR_CREATE_vcvttpd2dq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvttpd2dq, (d), (k), (s))
#define INSTR_CREATE_vcvtpd2dq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtpd2dq, (d), (k), (s))
#define INSTR_CREATE_vcvtph2ps_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtph2ps, (d), (k), (s))
#define INSTR_CREATE_vcvtpd2qq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtpd2qq, (d), (k), (s))
#define INSTR_CREATE_vcvtps2udq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtps2udq, (d), (k), (s))
#define INSTR_CREATE_vcvtpd2udq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtpd2udq, (d), (k), (s))
#define INSTR_CREATE_vcvtps2uqq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtps2uqq, (d), (k), (s))
#define INSTR_CREATE_vcvtpd2uqq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtpd2uqq, (d), (k), (s))
#define INSTR_CREATE_vcvtps2qq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtps2qq, (d), (k), (s))
#define INSTR_CREATE_vcvttps2udq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvttps2udq, (d), (k), (s))
#define INSTR_CREATE_vcvttpd2udq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvttpd2udq, (d), (k), (s))
#define INSTR_CREATE_vcvttps2qq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvttps2qq, (d), (k), (s))
#define INSTR_CREATE_vcvttpd2qq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvttpd2qq, (d), (k), (s))
#define INSTR_CREATE_vcvttps2uqq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvttps2uqq, (d), (k), (s))
#define INSTR_CREATE_vcvttpd2uqq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvttpd2uqq, (d), (k), (s))
#define INSTR_CREATE_vcvtqq2ps_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtqq2ps, (d), (k), (s))
#define INSTR_CREATE_vcvtqq2pd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtqq2pd, (d), (k), (s))
#define INSTR_CREATE_vcvtudq2ps_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtudq2ps, (d), (k), (s))
#define INSTR_CREATE_vcvtudq2pd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtudq2pd, (d), (k), (s))
#define INSTR_CREATE_vcvtuqq2ps_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtuqq2ps, (d), (k), (s))
#define INSTR_CREATE_vcvtuqq2pd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtuqq2pd, (d), (k), (s))
#define INSTR_CREATE_vrcp14ps_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vrcp14ps, (d), (k), (s))
#define INSTR_CREATE_vrcp14pd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vrcp14pd, (d), (k), (s))
#define INSTR_CREATE_vrcp28ps_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vrcp28ps, (d), (k), (s))
#define INSTR_CREATE_vrcp28pd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vrcp28pd, (d), (k), (s))
#define INSTR_CREATE_vpmovsxbw_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovsxbw, (d), (k), (s))
#define INSTR_CREATE_vpmovsxbd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovsxbd, (d), (k), (s))
#define INSTR_CREATE_vpmovsxbq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovsxbq, (d), (k), (s))
#define INSTR_CREATE_vpmovsxwd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovsxwd, (d), (k), (s))
#define INSTR_CREATE_vpmovsxwq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovsxwq, (d), (k), (s))
#define INSTR_CREATE_vpmovsxdq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovsxdq, (d), (k), (s))
#define INSTR_CREATE_vpmovzxbw_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovzxbw, (d), (k), (s))
#define INSTR_CREATE_vpmovzxbd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovzxbd, (d), (k), (s))
#define INSTR_CREATE_vpmovzxbq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovzxbq, (d), (k), (s))
#define INSTR_CREATE_vpmovzxwd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovzxwd, (d), (k), (s))
#define INSTR_CREATE_vpmovzxwq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovzxwq, (d), (k), (s))
#define INSTR_CREATE_vpmovzxdq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovzxdq, (d), (k), (s))
#define INSTR_CREATE_vpmovqb_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovqb, (d), (k), (s))
#define INSTR_CREATE_vpmovsqb_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovsqb, (d), (k), (s))
#define INSTR_CREATE_vpmovusqb_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovusqb, (d), (k), (s))
#define INSTR_CREATE_vpmovqw_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovqw, (d), (k), (s))
#define INSTR_CREATE_vpmovsqw_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovsqw, (d), (k), (s))
#define INSTR_CREATE_vpmovusqw_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovusqw, (d), (k), (s))
#define INSTR_CREATE_vpmovqd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovqd, (d), (k), (s))
#define INSTR_CREATE_vpmovsqd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovsqd, (d), (k), (s))
#define INSTR_CREATE_vpmovusqd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovusqd, (d), (k), (s))
#define INSTR_CREATE_vpmovdb_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovdb, (d), (k), (s))
#define INSTR_CREATE_vpmovsdb_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovsdb, (d), (k), (s))
#define INSTR_CREATE_vpmovusdb_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovusdb, (d), (k), (s))
#define INSTR_CREATE_vpmovdw_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovdw, (d), (k), (s))
#define INSTR_CREATE_vpmovsdw_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovsdw, (d), (k), (s))
#define INSTR_CREATE_vpmovusdw_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovusdw, (d), (k), (s))
#define INSTR_CREATE_vpmovwb_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovwb, (d), (k), (s))
#define INSTR_CREATE_vpmovswb_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovswb, (d), (k), (s))
#define INSTR_CREATE_vpmovuswb_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovuswb, (d), (k), (s))
#define INSTR_CREATE_vpmovm2b_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovm2b, (d), (k), (s))
#define INSTR_CREATE_vpmovm2w_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovm2w, (d), (k), (s))
#define INSTR_CREATE_vpmovm2d_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovm2d, (d), (k), (s))
#define INSTR_CREATE_vpmovm2q_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpmovm2q, (d), (k), (s))
#define INSTR_CREATE_vpabsb_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpabsb, (d), (k), (s))
#define INSTR_CREATE_vpabsw_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpabsw, (d), (k), (s))
#define INSTR_CREATE_vpabsd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpabsd, (d), (k), (s))
#define INSTR_CREATE_vpabsq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpabsq, (d), (k), (s))
#define INSTR_CREATE_vbroadcastss_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vbroadcastss, (d), (k), (s))
#define INSTR_CREATE_vbroadcastsd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vbroadcastsd, (d), (k), (s))
#define INSTR_CREATE_vbroadcastf32x2_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vbroadcastf32x2, (d), (k), (s))
#define INSTR_CREATE_vbroadcastf32x4_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vbroadcastf32x4, (d), (k), (s))
#define INSTR_CREATE_vbroadcastf64x2_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vbroadcastf64x2, (d), (k), (s))
#define INSTR_CREATE_vbroadcastf32x8_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vbroadcastf32x8, (d), (k), (s))
#define INSTR_CREATE_vbroadcastf64x4_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vbroadcastf64x4, (d), (k), (s))
#define INSTR_CREATE_vpbroadcastb_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpbroadcastb, (d), (k), (s))
#define INSTR_CREATE_vpbroadcastw_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpbroadcastw, (d), (k), (s))
#define INSTR_CREATE_vpbroadcastd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpbroadcastd, (d), (k), (s))
#define INSTR_CREATE_vpbroadcastq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpbroadcastq, (d), (k), (s))
#define INSTR_CREATE_vbroadcasti32x2_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vbroadcasti32x2, (d), (k), (s))
#define INSTR_CREATE_vbroadcasti32x4_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vbroadcasti32x4, (d), (k), (s))
#define INSTR_CREATE_vbroadcasti64x2_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vbroadcasti64x2, (d), (k), (s))
#define INSTR_CREATE_vbroadcasti32x8_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vbroadcasti32x8, (d), (k), (s))
#define INSTR_CREATE_vbroadcasti64x4_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vbroadcasti64x4, (d), (k), (s))
#define INSTR_CREATE_vcompressps_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcompressps, (d), (k), (s))
#define INSTR_CREATE_vcompresspd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcompresspd, (d), (k), (s))
#define INSTR_CREATE_vexpandps_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vexpandps, (d), (k), (s))
#define INSTR_CREATE_vexpandpd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vexpandpd, (d), (k), (s))
#define INSTR_CREATE_vgetexpps_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vgetexpps, (d), (k), (s))
#define INSTR_CREATE_vgetexppd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vgetexppd, (d), (k), (s))
#define INSTR_CREATE_vpcompressd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpcompressd, (d), (k), (s))
#define INSTR_CREATE_vpcompressq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpcompressq, (d), (k), (s))
#define INSTR_CREATE_vpexpandd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpexpandd, (d), (k), (s))
#define INSTR_CREATE_vpexpandq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpexpandq, (d), (k), (s))
#define INSTR_CREATE_vrsqrt14ps_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vrsqrt14ps, (d), (k), (s))
#define INSTR_CREATE_vrsqrt14pd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vrsqrt14pd, (d), (k), (s))
#define INSTR_CREATE_vrsqrt28ps_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vrsqrt28ps, (d), (k), (s))
#define INSTR_CREATE_vrsqrt28pd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vrsqrt28pd, (d), (k), (s))
#define INSTR_CREATE_vexp2ps_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vexp2ps, (d), (k), (s))
#define INSTR_CREATE_vexp2pd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vexp2pd, (d), (k), (s))
#define INSTR_CREATE_vpconflictd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpconflictd, (d), (k), (s))
#define INSTR_CREATE_vpconflictq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpconflictq, (d), (k), (s))
#define INSTR_CREATE_vplzcntd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vplzcntd, (d), (k), (s))
#define INSTR_CREATE_vplzcntq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vplzcntq, (d), (k), (s))
#define INSTR_CREATE_vsqrtps_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vsqrtps, (d), (k), (s))
#define INSTR_CREATE_vsqrtpd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vsqrtpd, (d), (k), (s))
/* AVX512 BF16 */
#define INSTR_CREATE_vcvtneps2bf16_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vcvtneps2bf16, (d), (k), (s))
/* AVX512 VPOPCNTDQ */
#define INSTR_CREATE_vpopcntd_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpopcntd, (d), (k), (s))
#define INSTR_CREATE_vpopcntq_mask(dc, d, k, s) \
    instr_create_1dst_2src((dc), OP_vpopcntq, (d), (k), (s))

/** @} */ /* end doxygen group */

/* 1 destination, 2 sources: 1 explicit, 1 implicit */
/** @name 1 destination, 2 sources: 1 explicit, 1 implicit */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s The opnd_t explicit source operand for the instruction.
 */
#define INSTR_CREATE_add(dc, d, s) instr_create_1dst_2src((dc), OP_add, (d), (s), (d))
#define INSTR_CREATE_or(dc, d, s) instr_create_1dst_2src((dc), OP_or, (d), (s), (d))
#define INSTR_CREATE_adc(dc, d, s) instr_create_1dst_2src((dc), OP_adc, (d), (s), (d))
#define INSTR_CREATE_sbb(dc, d, s) instr_create_1dst_2src((dc), OP_sbb, (d), (s), (d))
#define INSTR_CREATE_and(dc, d, s) instr_create_1dst_2src((dc), OP_and, (d), (s), (d))
#define INSTR_CREATE_sub(dc, d, s) instr_create_1dst_2src((dc), OP_sub, (d), (s), (d))
#define INSTR_CREATE_xor(dc, d, s) instr_create_1dst_2src((dc), OP_xor, (d), (s), (d))
#define INSTR_CREATE_punpcklbw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_punpcklbw, (d), (s), (d))
#define INSTR_CREATE_punpcklwd(dc, d, s) \
    instr_create_1dst_2src((dc), OP_punpcklwd, (d), (s), (d))
#define INSTR_CREATE_punpckldq(dc, d, s) \
    instr_create_1dst_2src((dc), OP_punpckldq, (d), (s), (d))
#define INSTR_CREATE_packsswb(dc, d, s) \
    instr_create_1dst_2src((dc), OP_packsswb, (d), (s), (d))
#define INSTR_CREATE_pcmpgtb(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pcmpgtb, (d), (s), (d))
#define INSTR_CREATE_pcmpgtw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pcmpgtw, (d), (s), (d))
#define INSTR_CREATE_pcmpgtd(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pcmpgtd, (d), (s), (d))
#define INSTR_CREATE_packuswb(dc, d, s) \
    instr_create_1dst_2src((dc), OP_packuswb, (d), (s), (d))
#define INSTR_CREATE_punpckhbw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_punpckhbw, (d), (s), (d))
#define INSTR_CREATE_punpckhwd(dc, d, s) \
    instr_create_1dst_2src((dc), OP_punpckhwd, (d), (s), (d))
#define INSTR_CREATE_punpckhdq(dc, d, s) \
    instr_create_1dst_2src((dc), OP_punpckhdq, (d), (s), (d))
#define INSTR_CREATE_packssdw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_packssdw, (d), (s), (d))
#define INSTR_CREATE_punpcklqdq(dc, d, s) \
    instr_create_1dst_2src((dc), OP_punpcklqdq, (d), (s), (d))
#define INSTR_CREATE_punpckhqdq(dc, d, s) \
    instr_create_1dst_2src((dc), OP_punpckhqdq, (d), (s), (d))
#define INSTR_CREATE_pcmpeqb(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pcmpeqb, (d), (s), (d))
#define INSTR_CREATE_pcmpeqw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pcmpeqw, (d), (s), (d))
#define INSTR_CREATE_pcmpeqd(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pcmpeqd, (d), (s), (d))
#define INSTR_CREATE_psrlw(dc, d, s) instr_create_1dst_2src((dc), OP_psrlw, (d), (s), (d))
#define INSTR_CREATE_psrld(dc, d, s) instr_create_1dst_2src((dc), OP_psrld, (d), (s), (d))
#define INSTR_CREATE_psrlq(dc, d, s) instr_create_1dst_2src((dc), OP_psrlq, (d), (s), (d))
#define INSTR_CREATE_paddq(dc, d, s) instr_create_1dst_2src((dc), OP_paddq, (d), (s), (d))
#define INSTR_CREATE_pmullw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pmullw, (d), (s), (d))
#define INSTR_CREATE_psubusb(dc, d, s) \
    instr_create_1dst_2src((dc), OP_psubusb, (d), (s), (d))
#define INSTR_CREATE_psubusw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_psubusw, (d), (s), (d))
#define INSTR_CREATE_pminub(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pminub, (d), (s), (d))
#define INSTR_CREATE_pand(dc, d, s) instr_create_1dst_2src((dc), OP_pand, (d), (s), (d))
#define INSTR_CREATE_paddusb(dc, d, s) \
    instr_create_1dst_2src((dc), OP_paddusb, (d), (s), (d))
#define INSTR_CREATE_paddusw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_paddusw, (d), (s), (d))
#define INSTR_CREATE_pmaxub(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pmaxub, (d), (s), (d))
#define INSTR_CREATE_pandn(dc, d, s) instr_create_1dst_2src((dc), OP_pandn, (d), (s), (d))
#define INSTR_CREATE_pavgb(dc, d, s) instr_create_1dst_2src((dc), OP_pavgb, (d), (s), (d))
#define INSTR_CREATE_psraw(dc, d, s) instr_create_1dst_2src((dc), OP_psraw, (d), (s), (d))
#define INSTR_CREATE_psrad(dc, d, s) instr_create_1dst_2src((dc), OP_psrad, (d), (s), (d))
#define INSTR_CREATE_pavgw(dc, d, s) instr_create_1dst_2src((dc), OP_pavgw, (d), (s), (d))
#define INSTR_CREATE_pmulhuw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pmulhuw, (d), (s), (d))
#define INSTR_CREATE_pmulhw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pmulhw, (d), (s), (d))
#define INSTR_CREATE_psubsb(dc, d, s) \
    instr_create_1dst_2src((dc), OP_psubsb, (d), (s), (d))
#define INSTR_CREATE_psubsw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_psubsw, (d), (s), (d))
#define INSTR_CREATE_pminsw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pminsw, (d), (s), (d))
#define INSTR_CREATE_por(dc, d, s) instr_create_1dst_2src((dc), OP_por, (d), (s), (d))
#define INSTR_CREATE_paddsb(dc, d, s) \
    instr_create_1dst_2src((dc), OP_paddsb, (d), (s), (d))
#define INSTR_CREATE_paddsw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_paddsw, (d), (s), (d))
#define INSTR_CREATE_pmaxsw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pmaxsw, (d), (s), (d))
#define INSTR_CREATE_pxor(dc, d, s) instr_create_1dst_2src((dc), OP_pxor, (d), (s), (d))
#define INSTR_CREATE_psllw(dc, d, s) instr_create_1dst_2src((dc), OP_psllw, (d), (s), (d))
#define INSTR_CREATE_pslld(dc, d, s) instr_create_1dst_2src((dc), OP_pslld, (d), (s), (d))
#define INSTR_CREATE_psllq(dc, d, s) instr_create_1dst_2src((dc), OP_psllq, (d), (s), (d))
#define INSTR_CREATE_pmuludq(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pmuludq, (d), (s), (d))
#define INSTR_CREATE_pmaddwd(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pmaddwd, (d), (s), (d))
#define INSTR_CREATE_psadbw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_psadbw, (d), (s), (d))
#define INSTR_CREATE_psubb(dc, d, s) instr_create_1dst_2src((dc), OP_psubb, (d), (s), (d))
#define INSTR_CREATE_psubw(dc, d, s) instr_create_1dst_2src((dc), OP_psubw, (d), (s), (d))
#define INSTR_CREATE_psubd(dc, d, s) instr_create_1dst_2src((dc), OP_psubd, (d), (s), (d))
#define INSTR_CREATE_psubq(dc, d, s) instr_create_1dst_2src((dc), OP_psubq, (d), (s), (d))
#define INSTR_CREATE_paddb(dc, d, s) instr_create_1dst_2src((dc), OP_paddb, (d), (s), (d))
#define INSTR_CREATE_paddw(dc, d, s) instr_create_1dst_2src((dc), OP_paddw, (d), (s), (d))
#define INSTR_CREATE_paddd(dc, d, s) instr_create_1dst_2src((dc), OP_paddd, (d), (s), (d))
#define INSTR_CREATE_psrldq(dc, d, s) \
    instr_create_1dst_2src((dc), OP_psrldq, (d), (s), (d))
#define INSTR_CREATE_pslldq(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pslldq, (d), (s), (d))
#define INSTR_CREATE_unpcklps(dc, d, s) \
    instr_create_1dst_2src((dc), OP_unpcklps, (d), (s), (d))
#define INSTR_CREATE_unpcklpd(dc, d, s) \
    instr_create_1dst_2src((dc), OP_unpcklpd, (d), (s), (d))
#define INSTR_CREATE_unpckhps(dc, d, s) \
    instr_create_1dst_2src((dc), OP_unpckhps, (d), (s), (d))
#define INSTR_CREATE_unpckhpd(dc, d, s) \
    instr_create_1dst_2src((dc), OP_unpckhpd, (d), (s), (d))
#define INSTR_CREATE_andps(dc, d, s) instr_create_1dst_2src((dc), OP_andps, (d), (s), (d))
#define INSTR_CREATE_andpd(dc, d, s) instr_create_1dst_2src((dc), OP_andpd, (d), (s), (d))
#define INSTR_CREATE_andnps(dc, d, s) \
    instr_create_1dst_2src((dc), OP_andnps, (d), (s), (d))
#define INSTR_CREATE_andnpd(dc, d, s) \
    instr_create_1dst_2src((dc), OP_andnpd, (d), (s), (d))
#define INSTR_CREATE_orps(dc, d, s) instr_create_1dst_2src((dc), OP_orps, (d), (s), (d))
#define INSTR_CREATE_orpd(dc, d, s) instr_create_1dst_2src((dc), OP_orpd, (d), (s), (d))
#define INSTR_CREATE_xorps(dc, d, s) instr_create_1dst_2src((dc), OP_xorps, (d), (s), (d))
#define INSTR_CREATE_xorpd(dc, d, s) instr_create_1dst_2src((dc), OP_xorpd, (d), (s), (d))
#define INSTR_CREATE_addps(dc, d, s) instr_create_1dst_2src((dc), OP_addps, (d), (s), (d))
#define INSTR_CREATE_addss(dc, d, s) instr_create_1dst_2src((dc), OP_addss, (d), (s), (d))
#define INSTR_CREATE_addpd(dc, d, s) instr_create_1dst_2src((dc), OP_addpd, (d), (s), (d))
#define INSTR_CREATE_addsd(dc, d, s) instr_create_1dst_2src((dc), OP_addsd, (d), (s), (d))
#define INSTR_CREATE_mulps(dc, d, s) instr_create_1dst_2src((dc), OP_mulps, (d), (s), (d))
#define INSTR_CREATE_mulss(dc, d, s) instr_create_1dst_2src((dc), OP_mulss, (d), (s), (d))
#define INSTR_CREATE_mulpd(dc, d, s) instr_create_1dst_2src((dc), OP_mulpd, (d), (s), (d))
#define INSTR_CREATE_mulsd(dc, d, s) instr_create_1dst_2src((dc), OP_mulsd, (d), (s), (d))
#define INSTR_CREATE_subps(dc, d, s) instr_create_1dst_2src((dc), OP_subps, (d), (s), (d))
#define INSTR_CREATE_subss(dc, d, s) instr_create_1dst_2src((dc), OP_subss, (d), (s), (d))
#define INSTR_CREATE_subpd(dc, d, s) instr_create_1dst_2src((dc), OP_subpd, (d), (s), (d))
#define INSTR_CREATE_subsd(dc, d, s) instr_create_1dst_2src((dc), OP_subsd, (d), (s), (d))
#define INSTR_CREATE_minps(dc, d, s) instr_create_1dst_2src((dc), OP_minps, (d), (s), (d))
#define INSTR_CREATE_minss(dc, d, s) instr_create_1dst_2src((dc), OP_minss, (d), (s), (d))
#define INSTR_CREATE_minpd(dc, d, s) instr_create_1dst_2src((dc), OP_minpd, (d), (s), (d))
#define INSTR_CREATE_minsd(dc, d, s) instr_create_1dst_2src((dc), OP_minsd, (d), (s), (d))
#define INSTR_CREATE_divps(dc, d, s) instr_create_1dst_2src((dc), OP_divps, (d), (s), (d))
#define INSTR_CREATE_divss(dc, d, s) instr_create_1dst_2src((dc), OP_divss, (d), (s), (d))
#define INSTR_CREATE_divpd(dc, d, s) instr_create_1dst_2src((dc), OP_divpd, (d), (s), (d))
#define INSTR_CREATE_divsd(dc, d, s) instr_create_1dst_2src((dc), OP_divsd, (d), (s), (d))
#define INSTR_CREATE_maxps(dc, d, s) instr_create_1dst_2src((dc), OP_maxps, (d), (s), (d))
#define INSTR_CREATE_maxss(dc, d, s) instr_create_1dst_2src((dc), OP_maxss, (d), (s), (d))
#define INSTR_CREATE_maxpd(dc, d, s) instr_create_1dst_2src((dc), OP_maxpd, (d), (s), (d))
#define INSTR_CREATE_maxsd(dc, d, s) instr_create_1dst_2src((dc), OP_maxsd, (d), (s), (d))
/* SSE3 */
#define INSTR_CREATE_haddpd(dc, d, s) \
    instr_create_1dst_2src((dc), OP_haddpd, (d), (s), (d))
#define INSTR_CREATE_haddps(dc, d, s) \
    instr_create_1dst_2src((dc), OP_haddps, (d), (s), (d))
#define INSTR_CREATE_hsubpd(dc, d, s) \
    instr_create_1dst_2src((dc), OP_hsubpd, (d), (s), (d))
#define INSTR_CREATE_hsubps(dc, d, s) \
    instr_create_1dst_2src((dc), OP_hsubps, (d), (s), (d))
#define INSTR_CREATE_addsubpd(dc, d, s) \
    instr_create_1dst_2src((dc), OP_addsubpd, (d), (s), (d))
#define INSTR_CREATE_addsubps(dc, d, s) \
    instr_create_1dst_2src((dc), OP_addsubps, (d), (s), (d))
/* 3D-Now */
#define INSTR_CREATE_pavgusb(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pavgusb, (d), (s), (d))
#define INSTR_CREATE_pfadd(dc, d, s) instr_create_1dst_2src((dc), OP_pfadd, (d), (s), (d))
#define INSTR_CREATE_pfacc(dc, d, s) instr_create_1dst_2src((dc), OP_pfacc, (d), (s), (d))
#define INSTR_CREATE_pfcmpge(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pfcmpge, (d), (s), (d))
#define INSTR_CREATE_pfcmpgt(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pfcmpgt, (d), (s), (d))
#define INSTR_CREATE_pfcmpeq(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pfcmpeq, (d), (s), (d))
#define INSTR_CREATE_pfmin(dc, d, s) instr_create_1dst_2src((dc), OP_pfmin, (d), (s), (d))
#define INSTR_CREATE_pfmax(dc, d, s) instr_create_1dst_2src((dc), OP_pfmax, (d), (s), (d))
#define INSTR_CREATE_pfmul(dc, d, s) instr_create_1dst_2src((dc), OP_pfmul, (d), (s), (d))
#define INSTR_CREATE_pfrcp(dc, d, s) instr_create_1dst_2src((dc), OP_pfrcp, (d), (s), (d))
#define INSTR_CREATE_pfrcpit1(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pfrcpit1, (d), (s), (d))
#define INSTR_CREATE_pfrcpit2(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pfrcpit2, (d), (s), (d))
#define INSTR_CREATE_pfrsqrt(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pfrsqrt, (d), (s), (d))
#define INSTR_CREATE_pfrsqit1(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pfrsqit1, (d), (s), (d))
#define INSTR_CREATE_pmulhrw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pmulhrw, (d), (s), (d))
#define INSTR_CREATE_pfsub(dc, d, s) instr_create_1dst_2src((dc), OP_pfsub, (d), (s), (d))
#define INSTR_CREATE_pfsubr(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pfsubr, (d), (s), (d))
#define INSTR_CREATE_pi2fd(dc, d, s) instr_create_1dst_2src((dc), OP_pi2fd, (d), (s), (d))
#define INSTR_CREATE_pf2id(dc, d, s) instr_create_1dst_2src((dc), OP_pf2id, (d), (s), (d))
#define INSTR_CREATE_pi2fw(dc, d, s) instr_create_1dst_2src((dc), OP_pi2fw, (d), (s), (d))
#define INSTR_CREATE_pf2iw(dc, d, s) instr_create_1dst_2src((dc), OP_pf2iw, (d), (s), (d))
#define INSTR_CREATE_pfnacc(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pfnacc, (d), (s), (d))
#define INSTR_CREATE_pfpnacc(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pfpnacc, (d), (s), (d))
#define INSTR_CREATE_pswapd(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pswapd, (d), (s), (d))
/* SSSE3 */
#define INSTR_CREATE_phaddw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_phaddw, (d), (s), (d))
#define INSTR_CREATE_phaddd(dc, d, s) \
    instr_create_1dst_2src((dc), OP_phaddd, (d), (s), (d))
#define INSTR_CREATE_phaddsw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_phaddsw, (d), (s), (d))
#define INSTR_CREATE_pmaddubsw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pmaddubsw, (d), (s), (d))
#define INSTR_CREATE_phsubw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_phsubw, (d), (s), (d))
#define INSTR_CREATE_phsubd(dc, d, s) \
    instr_create_1dst_2src((dc), OP_phsubd, (d), (s), (d))
#define INSTR_CREATE_phsubsw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_phsubsw, (d), (s), (d))
#define INSTR_CREATE_psignb(dc, d, s) \
    instr_create_1dst_2src((dc), OP_psignb, (d), (s), (d))
#define INSTR_CREATE_psignw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_psignw, (d), (s), (d))
#define INSTR_CREATE_psignd(dc, d, s) \
    instr_create_1dst_2src((dc), OP_psignd, (d), (s), (d))
#define INSTR_CREATE_pmulhrsw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pmulhrsw, (d), (s), (d))
#define INSTR_CREATE_pshufb(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pshufb, (d), (s), (d))
/* SSE4 */
#define INSTR_CREATE_crc32(dc, d, s) instr_create_1dst_2src((dc), OP_crc32, (d), (s), (d))
#define INSTR_CREATE_packusdw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_packusdw, (d), (s), (d))
#define INSTR_CREATE_pcmpeqq(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pcmpeqq, (d), (s), (d))
#define INSTR_CREATE_pcmpgtq(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pcmpgtq, (d), (s), (d))
#define INSTR_CREATE_pminsb(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pminsb, (d), (s), (d))
#define INSTR_CREATE_pminsd(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pminsd, (d), (s), (d))
#define INSTR_CREATE_pminuw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pminuw, (d), (s), (d))
#define INSTR_CREATE_pminud(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pminud, (d), (s), (d))
#define INSTR_CREATE_pmaxsb(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pmaxsb, (d), (s), (d))
#define INSTR_CREATE_pmaxsd(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pmaxsd, (d), (s), (d))
#define INSTR_CREATE_pmaxuw(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pmaxuw, (d), (s), (d))
#define INSTR_CREATE_pmaxud(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pmaxud, (d), (s), (d))
#define INSTR_CREATE_pmuldq(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pmuldq, (d), (s), (d))
#define INSTR_CREATE_pmulld(dc, d, s) \
    instr_create_1dst_2src((dc), OP_pmulld, (d), (s), (d))
#define INSTR_CREATE_aesenc(dc, d, s) \
    instr_create_1dst_2src((dc), OP_aesenc, (d), (s), (d))
#define INSTR_CREATE_aesenclast(dc, d, s) \
    instr_create_1dst_2src((dc), OP_aesenclast, (d), (s), (d))
#define INSTR_CREATE_aesdec(dc, d, s) \
    instr_create_1dst_2src((dc), OP_aesdec, (d), (s), (d))
#define INSTR_CREATE_aesdeclast(dc, d, s) \
    instr_create_1dst_2src((dc), OP_aesdeclast, (d), (s), (d))
/* ADX */
#define INSTR_CREATE_adox(dc, d, s) instr_create_1dst_2src((dc), OP_adox, (d), (s), (d))
#define INSTR_CREATE_adcx(dc, d, s) instr_create_1dst_2src((dc), OP_adcx, (d), (s), (d))
/* SHA */
#define INSTR_CREATE_sha1msg1(dc, d, s) \
    instr_create_1dst_2src((dc), OP_sha1msg1, (d), (s), (d))
#define INSTR_CREATE_sha1msg2(dc, d, s) \
    instr_create_1dst_2src((dc), OP_sha1msg2, (d), (s), (d))
#define INSTR_CREATE_sha1nexte(dc, d, s) \
    instr_create_1dst_2src((dc), OP_sha1nexte, (d), (s), (d))
#define INSTR_CREATE_sha256msg1(dc, d, s) \
    instr_create_1dst_2src((dc), OP_sha256msg1, (d), (s), (d))
#define INSTR_CREATE_sha256msg2(dc, d, s) \
    instr_create_1dst_2src((dc), OP_sha256msg2, (d), (s), (d))
/** @} */ /* end doxygen group */

/** @name 1 destination, 1 explicit register-or-immediate source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param ri The opnd_t explicit source operand for the instruction, which can
 * be a register (opnd_create_reg()) or an immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_bts(dc, d, ri) instr_create_1dst_2src((dc), OP_bts, (d), (ri), (d))
#define INSTR_CREATE_btr(dc, d, ri) instr_create_1dst_2src((dc), OP_btr, (d), (ri), (d))
#define INSTR_CREATE_btc(dc, d, ri) instr_create_1dst_2src((dc), OP_btc, (d), (ri), (d))
/** @} */ /* end doxygen group */

/**
 * Creates an instr_t for a conditional move instruction with the given opcode
 * and destination operand.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param op The OP_xxx opcode for the instruction, which should be in the range
 * [OP_cmovo, OP_cmovnle].
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s The opnd_t explicit source operand for the instruction.
 */
#define INSTR_CREATE_cmovcc(dc, op, d, s) \
    INSTR_PRED(instr_create_1dst_1src((dc), (op), (d), (s)), DR_PRED_O + (op)-OP_cmovo)

/**
 * This INSTR_CREATE_xxx_imm macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands. The _imm
 * suffix distinguishes between alternative forms of the same opcode: this
 * form takes an explicit immediate.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s The opnd_t explicit source operand for the instruction.
 * \param i The opnd_t explicit second source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_imul_imm(dc, d, s, i) \
    instr_create_1dst_2src((dc), OP_imul, (d), (s), (i))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s The opnd_t explicit source operand for the instruction.
 */
#define INSTR_CREATE_imul(dc, d, s) instr_create_1dst_2src((dc), OP_imul, (d), (s), (d))
/** @name 1 implicit destination, 1 explicit source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx, INSTR_CREATE_xxx_1, or INSTR_CREATE_xxx_4 macro creates an
 * instr_t with opcode OP_xxx and the given explicit operands, automatically
 * supplying any implicit operands.    The _1 or _4 suffixes distinguish between
 * alternative forms of the same opcode (1 and 4 identify the operand size).
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s The opnd_t explicit source operand for the instruction.
 */
#define INSTR_CREATE_imul_1(dc, s)                                         \
    instr_create_1dst_2src((dc), OP_imul, opnd_create_reg(DR_REG_AX), (s), \
                           opnd_create_reg(DR_REG_AL))
#define INSTR_CREATE_imul_4(dc, s)                                     \
    instr_create_2dst_2src((dc), OP_imul, opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX), (s),           \
                           opnd_create_reg(DR_REG_EAX))
#define INSTR_CREATE_mul_1(dc, s)                                         \
    instr_create_1dst_2src((dc), OP_mul, opnd_create_reg(DR_REG_AX), (s), \
                           opnd_create_reg(DR_REG_AL))
#define INSTR_CREATE_mul_4(dc, s)                                     \
    instr_create_2dst_2src((dc), OP_mul, opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX), (s),          \
                           opnd_create_reg(DR_REG_EAX))
#define INSTR_CREATE_div_1(dc, s)                                    \
    instr_create_2dst_2src((dc), OP_div, opnd_create_reg(DR_REG_AH), \
                           opnd_create_reg(DR_REG_AL), (s), opnd_create_reg(DR_REG_AX))
#define INSTR_CREATE_div_4(dc, s)                                     \
    instr_create_2dst_3src((dc), OP_div, opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX), (s),          \
                           opnd_create_reg(DR_REG_EDX), opnd_create_reg(DR_REG_EAX))
#define INSTR_CREATE_idiv_1(dc, s)                                    \
    instr_create_2dst_2src((dc), OP_idiv, opnd_create_reg(DR_REG_AH), \
                           opnd_create_reg(DR_REG_AL), (s), opnd_create_reg(DR_REG_AX))
#define INSTR_CREATE_idiv_4(dc, s)                                     \
    instr_create_2dst_3src((dc), OP_idiv, opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX), (s),           \
                           opnd_create_reg(DR_REG_EDX), opnd_create_reg(DR_REG_EAX))
/** @} */ /* end doxygen group */

/** @name 1 destination, 1 explicit source that is cl, an immediate, or a constant */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param ri The opnd_t explicit source operand for the instruction, which must
 * be one of the following:
 * -# The register cl (#opnd_create_reg(#DR_REG_CL));
 * -# An immediate integer (opnd_create_immed_int()) of size #OPSZ_1;
 * -# An immediate integer with value 1 and size #OPSZ_0
 * (#opnd_create_immed_int(1, #OPSZ_0)), which will become an implicit operand
 * (whereas #opnd_create_immed_int(1, #OPSZ_1) will be encoded explicitly).
 */
#define INSTR_CREATE_rol(dc, d, ri) instr_create_1dst_2src((dc), OP_rol, (d), (ri), (d))
#define INSTR_CREATE_ror(dc, d, ri) instr_create_1dst_2src((dc), OP_ror, (d), (ri), (d))
#define INSTR_CREATE_rcl(dc, d, ri) instr_create_1dst_2src((dc), OP_rcl, (d), (ri), (d))
#define INSTR_CREATE_rcr(dc, d, ri) instr_create_1dst_2src((dc), OP_rcr, (d), (ri), (d))
#define INSTR_CREATE_shl(dc, d, ri) instr_create_1dst_2src((dc), OP_shl, (d), (ri), (d))
#define INSTR_CREATE_shr(dc, d, ri) instr_create_1dst_2src((dc), OP_shr, (d), (ri), (d))
#define INSTR_CREATE_sar(dc, d, ri) instr_create_1dst_2src((dc), OP_sar, (d), (ri), (d))
/** @} */ /* end doxygen group */

/** @name 1 implicit destination, 2 explicit sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s1 The opnd_t first source operand for the instruction.
 * \param s2 The opnd_t second source operand for the instruction.
 */
#define INSTR_CREATE_maskmovq(dc, s1, s2)                                              \
    INSTR_PRED(instr_create_1dst_2src((dc), OP_maskmovq,                               \
                                      opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XDI, \
                                                                DR_REG_NULL, 0, 0,     \
                                                                OPSZ_maskmovq),        \
                                      (s1), (s2)),                                     \
               DR_PRED_COMPLEX)
#define INSTR_CREATE_maskmovdqu(dc, s1, s2)                                            \
    INSTR_PRED(instr_create_1dst_2src((dc), OP_maskmovdqu,                             \
                                      opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XDI, \
                                                                DR_REG_NULL, 0, 0,     \
                                                                OPSZ_maskmovdqu),      \
                                      (s1), (s2)),                                     \
               DR_PRED_COMPLEX)
#define INSTR_CREATE_vmaskmovdqu(dc, s1, s2)                                           \
    INSTR_PRED(instr_create_1dst_2src((dc), OP_vmaskmovdqu,                            \
                                      opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XDI, \
                                                                DR_REG_NULL, 0, 0,     \
                                                                OPSZ_maskmovdqu),      \
                                      (s1), (s2)),                                     \
               DR_PRED_COMPLEX)
/** @} */ /* end doxygen group */

/* floating-point */
/** @name Floating-point with explicit destination and explicit mem-or-fp-reg source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given operands, automatically supplying any further implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param f The opnd_t destination (and implicit source) operand for the
 * instruction, which must be a floating point register (opnd_create_reg()).
 * \param s The opnd_t source (and non-destination) operand for the
 * instruction, which must be one of the following:
 * -# A floating point register (opnd_create_reg()).
 * -# A memory reference (opnd_create_base_disp() or opnd_create_far_base_disp()),
 *    in which case the destination \p f must be #DR_REG_ST0.
 */
#define INSTR_CREATE_fadd(dc, f, s) instr_create_1dst_2src((dc), OP_fadd, (f), (s), (f))
#define INSTR_CREATE_fmul(dc, f, s) instr_create_1dst_2src((dc), OP_fmul, (f), (s), (f))
#define INSTR_CREATE_fdiv(dc, f, s) instr_create_1dst_2src((dc), OP_fdiv, (f), (s), (f))
#define INSTR_CREATE_fdivr(dc, f, s) instr_create_1dst_2src((dc), OP_fdivr, (f), (s), (f))
#define INSTR_CREATE_fsub(dc, f, s) instr_create_1dst_2src((dc), OP_fsub, (f), (s), (f))
#define INSTR_CREATE_fsubr(dc, f, s) instr_create_1dst_2src((dc), OP_fsubr, (f), (s), (f))
/** @} */ /* end doxygen group */
/** @name Floating-point with explicit destination and implicit source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with
 * opcode OP_xxx and the given explicit register operand, automatically
 * supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param f The opnd_t explicit destination + source operand for the instruction, which
 * must be a floating point register (opnd_create_reg()).
 */
#define INSTR_CREATE_faddp(dc, f) \
    instr_create_1dst_2src((dc), OP_faddp, (f), opnd_create_reg(DR_REG_ST0), (f))
#define INSTR_CREATE_fmulp(dc, f) \
    instr_create_1dst_2src((dc), OP_fmulp, (f), opnd_create_reg(DR_REG_ST0), (f))
#define INSTR_CREATE_fdivp(dc, f) \
    instr_create_1dst_2src((dc), OP_fdivp, (f), opnd_create_reg(DR_REG_ST0), (f))
#define INSTR_CREATE_fdivrp(dc, f) \
    instr_create_1dst_2src((dc), OP_fdivrp, (f), opnd_create_reg(DR_REG_ST0), (f))
#define INSTR_CREATE_fsubp(dc, f) \
    instr_create_1dst_2src((dc), OP_fsubp, (f), opnd_create_reg(DR_REG_ST0), (f))
#define INSTR_CREATE_fsubrp(dc, f) \
    instr_create_1dst_2src((dc), OP_fsubrp, (f), opnd_create_reg(DR_REG_ST0), (f))
/** @} */ /* end doxygen group */
/** @name Floating-point with implicit destination and explicit memory source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit memory operand, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param m The opnd_t explicit source operand for the instruction, which must be
 * a memory reference (opnd_create_base_disp() or opnd_create_far_base_disp()).
 */
#define INSTR_CREATE_fiadd(dc, m)                                            \
    instr_create_1dst_2src((dc), OP_fiadd, opnd_create_reg(DR_REG_ST0), (m), \
                           opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_fimul(dc, m)                                            \
    instr_create_1dst_2src((dc), OP_fimul, opnd_create_reg(DR_REG_ST0), (m), \
                           opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_fidiv(dc, m)                                            \
    instr_create_1dst_2src((dc), OP_fidiv, opnd_create_reg(DR_REG_ST0), (m), \
                           opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_fidivr(dc, m)                                            \
    instr_create_1dst_2src((dc), OP_fidivr, opnd_create_reg(DR_REG_ST0), (m), \
                           opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_fisub(dc, m)                                            \
    instr_create_1dst_2src((dc), OP_fisub, opnd_create_reg(DR_REG_ST0), (m), \
                           opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_fisubr(dc, m)                                            \
    instr_create_1dst_2src((dc), OP_fisubr, opnd_create_reg(DR_REG_ST0), (m), \
                           opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_ficom(dc, m)                                            \
    instr_create_1dst_2src((dc), OP_ficom, opnd_create_reg(DR_REG_ST0), (m), \
                           opnd_create_reg(DR_REG_ST0))
#define INSTR_CREATE_ficomp(dc, m)                                            \
    instr_create_1dst_2src((dc), OP_ficomp, opnd_create_reg(DR_REG_ST0), (m), \
                           opnd_create_reg(DR_REG_ST0))
/** @} */ /* end doxygen group */

/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param r The opnd_t explicit source operand for the instruction, which
 * must be an xmm register (opnd_create_reg()).
 */
#define INSTR_CREATE_extrq(dc, d, r) instr_create_1dst_1src((dc), OP_extrq, (d), (r))
/**
 * This INSTR_CREATE_xxx_imm macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands. The _imm
 * suffix distinguishes between alternative forms of the same opcode: this
 * form takes explicit immediates.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param i1 The opnd_t explicit first source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()).
 * \param i2 The opnd_t explicit second source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_extrq_imm(dc, d, i1, i2) \
    instr_create_1dst_2src((dc), OP_extrq, (d), (i1), (i2))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param r The opnd_t explicit source operand for the instruction, which
 * must be an xmm register (opnd_create_reg()).
 */
#define INSTR_CREATE_insertq(dc, d, r) instr_create_1dst_1src((dc), OP_insertq, (d), (r))
/**
 * This INSTR_CREATE_xxx_imm macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands. The _imm
 * suffix distinguishes between alternative forms of the same opcode: this
 * form takes explicit immediates.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param r The opnd_t explicit first source operand for the instruction, which
 * must be an xmm register (opnd_create_reg()).
 * \param i1 The opnd_t explicit second source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()).
 * \param i2 The opnd_t explicit third source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_insertq_imm(dc, d, r, i1, i2) \
    instr_create_1dst_3src((dc), OP_insertq, (d), (r), (i1), (i2))

/* 1 destination, 2 implicit sources */
/** @name 1 destination, 2 implicit sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction, which can be
 * created with OPND_CREATE_MEM_xsave() to get the appropriate operand size.
 */
#define INSTR_CREATE_xsave32(dc, d)                                            \
    instr_create_1dst_2src((dc), OP_xsave32, (d), opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX))
#define INSTR_CREATE_xsave64(dc, d)                                            \
    instr_create_1dst_2src((dc), OP_xsave64, (d), opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX))
#define INSTR_CREATE_xsaveopt32(dc, d)                                            \
    instr_create_1dst_2src((dc), OP_xsaveopt32, (d), opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX))
#define INSTR_CREATE_xsaveopt64(dc, d)                                            \
    instr_create_1dst_2src((dc), OP_xsaveopt64, (d), opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX))
#define INSTR_CREATE_xsavec32(dc, d)                                            \
    instr_create_1dst_2src((dc), OP_xsavec32, (d), opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX))
#define INSTR_CREATE_xsavec64(dc, d)                                            \
    instr_create_1dst_2src((dc), OP_xsavec64, (d), opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX))
/** @} */ /* end doxygen group */

/* 1 implicit destination, 2 sources: 1 explicit, 1 implicit */
/** @name 1 implicit destination, 2 sources: 1 explicit, 1 implicit */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param i The opnd_t explicit source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_aam(dc, i)                                           \
    instr_create_1dst_2src((dc), OP_aam, opnd_create_reg(DR_REG_AX), (i), \
                           opnd_create_reg(DR_REG_AX))
#define INSTR_CREATE_aad(dc, i)                                           \
    instr_create_1dst_2src((dc), OP_aad, opnd_create_reg(DR_REG_AX), (i), \
                           opnd_create_reg(DR_REG_AX))
/** @} */ /* end doxygen group */
/** @name Loop instructions */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param t The opnd_t target operand for the instruction, which can be either
 * a pc (opnd_create_pc()) or an instr_t (opnd_create_instr()).
 */
#define INSTR_CREATE_loopne(dc, t)                                            \
    instr_create_1dst_2src((dc), OP_loopne, opnd_create_reg(DR_REG_XCX), (t), \
                           opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_loope(dc, t)                                            \
    instr_create_1dst_2src((dc), OP_loope, opnd_create_reg(DR_REG_XCX), (t), \
                           opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_loop(dc, t)                                            \
    instr_create_1dst_2src((dc), OP_loop, opnd_create_reg(DR_REG_XCX), (t), \
                           opnd_create_reg(DR_REG_XCX))
/** @} */ /* end doxygen group */

/* 1 implicit destination, 2 implicit sources */
/** @name 1 implicit destination, 2 implicit sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx, automatically
 * supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define INSTR_CREATE_popf(dc)                                                    \
    instr_create_1dst_2src(                                                      \
        (dc), OP_popf, opnd_create_reg(DR_REG_XSP), opnd_create_reg(DR_REG_XSP), \
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_STACK))
#define INSTR_CREATE_ret(dc)                                                    \
    instr_create_1dst_2src(                                                     \
        (dc), OP_ret, opnd_create_reg(DR_REG_XSP), opnd_create_reg(DR_REG_XSP), \
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_ret))
/* XXX: blindly asking for rex.w (b/c 32-bit is default for x64) but don't
 * know x64 mode! */
#define INSTR_CREATE_ret_far(dc)                                                \
    instr_create_1dst_2src((dc), OP_ret_far, opnd_create_reg(DR_REG_XSP),       \
                           opnd_create_reg(DR_REG_XSP),                         \
                           opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, \
                                                 IF_X64_ELSE(OPSZ_16, OPSZ_8)))
/* XXX: blindly asking for rex.w (b/c 32-bit is default for x64) but don't
 * know x64 mode! */
#define INSTR_CREATE_iret(dc)                                                   \
    instr_create_1dst_2src((dc), OP_iret, opnd_create_reg(DR_REG_XSP),          \
                           opnd_create_reg(DR_REG_XSP),                         \
                           opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, \
                                                 IF_X64_ELSE(OPSZ_40, OPSZ_12)))
/** @} */ /* end doxygen group */

/* 1 destination, 3 sources */
/** @name 1 destination, 3 non-immediate sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s1 The opnd_t explicit first source operand for the instruction.
 * \param s2 The opnd_t explicit second source operand for the instruction
 * \param s3 The opnd_t explicit third source operand for the instruction
 */
/* AVX */
#define INSTR_CREATE_vpblendvb(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpblendvb, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vblendvps(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vblendvps, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vblendvpd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vblendvpd, (d), (s1), (s2), (s3))
/* AVX2 */
/* these take immediates: not bothering to call out in docs */
#define INSTR_CREATE_vinserti128(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vinserti128, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpblendd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpblendd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vperm2i128(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vperm2i128, (d), (s1), (s2), (s3))
/** @} */ /* end doxygen group */

/** @name 1 destination, 1 mask, and 2 non-immediate sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx_mask macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands. The first source
 * operand is the instruction's mask.
 *
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param k The opnd_t explicit mask source operand for the instruction.
 * \param s1 The opnd_t explicit first source operand for the instruction
 * \param s2 The opnd_t explicit second source operand for the instruction
 */
/* AVX-512 EVEX */
#define INSTR_CREATE_vmovss_NDS_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vmovss, (d), (k), (s1), (s2))
#define INSTR_CREATE_vmovsd_NDS_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vmovsd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vunpcklps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vunpcklps, (d), (k), (s1), (s2))
#define INSTR_CREATE_vunpcklpd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vunpcklpd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vunpckhps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vunpckhps, (d), (k), (s1), (s2))
#define INSTR_CREATE_vunpckhpd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vunpckhpd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vandps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vandps, (d), (k), (s1), (s2))
#define INSTR_CREATE_vandpd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vandpd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vorps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vorps, (d), (k), (s1), (s2))
#define INSTR_CREATE_vorpd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vorpd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vxorps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vxorps, (d), (k), (s1), (s2))
#define INSTR_CREATE_vxorpd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vxorpd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vandnps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vandnps, (d), (k), (s1), (s2))
#define INSTR_CREATE_vandnpd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vandnpd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpandd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpandd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpandq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpandq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpandnd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpandnd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpandnq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpandnq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpord_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpord, (d), (k), (s1), (s2))
#define INSTR_CREATE_vporq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vporq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpxord_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpxord, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpxorq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpxorq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vaddps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vaddps, (d), (k), (s1), (s2))
#define INSTR_CREATE_vaddpd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vaddpd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vsubps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vsubps, (d), (k), (s1), (s2))
#define INSTR_CREATE_vsubpd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vsubpd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vaddss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vaddss, (d), (k), (s1), (s2))
#define INSTR_CREATE_vaddsd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vaddsd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vsubss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vsubss, (d), (k), (s1), (s2))
#define INSTR_CREATE_vsubsd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vsubsd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpaddb_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpaddb, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpaddw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpaddw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpaddd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpaddd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpaddq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpaddq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsubb_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsubb, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsubw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsubw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsubd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsubd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsubq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsubq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpaddusb_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpaddusb, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpaddusw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpaddusw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpaddsb_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpaddsb, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpaddsw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpaddsw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsubusb_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsubusb, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsubusw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsubusw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsubsb_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsubsb, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsubsw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsubsw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmaddwd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmaddwd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmaddubsw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmaddubsw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vmulps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vmulps, (d), (k), (s1), (s2))
#define INSTR_CREATE_vmulpd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vmulpd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vmulss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vmulss, (d), (k), (s1), (s2))
#define INSTR_CREATE_vmulsd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vmulsd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmullw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmullw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmulld_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmulld, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmullq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmullq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmuldq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmuldq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmulhw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmulhw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmulhuw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmulhuw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmuludq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmuludq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmulhrsw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmulhrsw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vdivps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vdivps, (d), (k), (s1), (s2))
#define INSTR_CREATE_vdivpd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vdivpd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vdivss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vdivss, (d), (k), (s1), (s2))
#define INSTR_CREATE_vdivsd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vdivsd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vminps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vminps, (d), (k), (s1), (s2))
#define INSTR_CREATE_vminpd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vminpd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vminss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vminss, (d), (k), (s1), (s2))
#define INSTR_CREATE_vminsd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vminsd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vmaxps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vmaxps, (d), (k), (s1), (s2))
#define INSTR_CREATE_vmaxpd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vmaxpd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vmaxss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vmaxss, (d), (k), (s1), (s2))
#define INSTR_CREATE_vmaxsd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vmaxsd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vcvtss2sd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vcvtss2sd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vcvtsd2ss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vcvtsd2ss, (d), (k), (s1), (s2))
#define INSTR_CREATE_vcvtps2ph_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vcvtps2ph, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermilps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermilps, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermilpd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermilpd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermps, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermb_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermb, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermpd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermpd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermi2ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermi2ps, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermi2pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermi2pd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermi2d_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermi2d, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermi2q_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermi2q, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermi2b_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermi2b, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermi2w_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermi2w, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermt2d_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermt2d, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermt2q_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermt2q, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermt2b_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermt2b, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermt2w_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermt2w, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermt2ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermt2ps, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpermt2pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpermt2pd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpunpcklbw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpunpcklbw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpunpcklwd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpunpcklwd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpunpckldq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpunpckldq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpunpcklqdq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpunpcklqdq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpunpckhbw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpunpckhbw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpunpckhwd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpunpckhwd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpunpckhdq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpunpckhdq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpunpckhqdq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpunpckhqdq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpacksswb_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpacksswb, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpackssdw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpackssdw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpackuswb_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpackuswb, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpackusdw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpackusdw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vextractf32x4_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vextractf32x4, (d), (k), (s1), (s2))
#define INSTR_CREATE_vextractf64x2_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vextractf64x2, (d), (k), (s1), (s2))
#define INSTR_CREATE_vextractf32x8_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vextractf32x8, (d), (k), (s1), (s2))
#define INSTR_CREATE_vextractf64x4_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vextractf64x4, (d), (k), (s1), (s2))
#define INSTR_CREATE_vextracti32x4_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vextracti32x4, (d), (k), (s1), (s2))
#define INSTR_CREATE_vextracti64x2_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vextracti64x2, (d), (k), (s1), (s2))
#define INSTR_CREATE_vextracti32x8_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vextracti32x8, (d), (k), (s1), (s2))
#define INSTR_CREATE_vextracti64x4_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vextracti64x4, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpcmpgtb_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpcmpgtb, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpcmpgtw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpcmpgtw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpcmpgtd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpcmpgtd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpcmpgtq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpcmpgtq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpcmpeqb_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpcmpeqb, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpcmpeqw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpcmpeqw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpcmpeqd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpcmpeqd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpcmpeqq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpcmpeqq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpminsb_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpminsb, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpminsw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpminsw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpminsd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpminsd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpminsq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpminsq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmaxsb_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmaxsb, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmaxsw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmaxsw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmaxsd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmaxsd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmaxsq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmaxsq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpminub_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpminub, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpminuw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpminuw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpminud_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpminud, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpminuq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpminuq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmaxub_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmaxub, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmaxuw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmaxuw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmaxud_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmaxud, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmaxuq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmaxuq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vprolvd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vprolvd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vprold_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vprold, (d), (k), (s1), (s2))
#define INSTR_CREATE_vprolvq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vprolvq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vprolq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vprolq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vprorvd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vprorvd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vprord_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vprord, (d), (k), (s1), (s2))
#define INSTR_CREATE_vprorvq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vprorvq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vprorq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vprorq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsraw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsraw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsrad_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsrad, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsraq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsraq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsrlw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsrlw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsrld_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsrld, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsrlq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsrlq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsravw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsravw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsravd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsravd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsravq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsravq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsrlvw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsrlvw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsrlvd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsrlvd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsrlvq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsrlvq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsllw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsllw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpslld_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpslld, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsllq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsllq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsllvw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsllvw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsllvd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsllvd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpsllvq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpsllvq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vrcp14ss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vrcp14ss, (d), (k), (s1), (s2))
#define INSTR_CREATE_vrcp14sd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vrcp14sd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vrcp28ss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vrcp28ss, (d), (k), (s1), (s2))
#define INSTR_CREATE_vrcp28sd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vrcp28sd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpshufb_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpshufb, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpavgb_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpavgb, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpavgw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpavgw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vblendmps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vblendmps, (d), (k), (s1), (s2))
#define INSTR_CREATE_vblendmpd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vblendmpd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vgetexpss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vgetexpss, (d), (k), (s1), (s2))
#define INSTR_CREATE_vgetexpsd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vgetexpsd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpblendmb_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpblendmb, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpblendmw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpblendmw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpblendmd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpblendmd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpblendmq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpblendmq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vptestmb_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vptestmb, (d), (k), (s1), (s2))
#define INSTR_CREATE_vptestmw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vptestmw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vptestmd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vptestmd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vptestmq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vptestmq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vptestnmb_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vptestnmb, (d), (k), (s1), (s2))
#define INSTR_CREATE_vptestnmw_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vptestnmw, (d), (k), (s1), (s2))
#define INSTR_CREATE_vptestnmd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vptestnmd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vptestnmq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vptestnmq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vrsqrt14ss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vrsqrt14ss, (d), (k), (s1), (s2))
#define INSTR_CREATE_vrsqrt14sd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vrsqrt14sd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vrsqrt28ss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vrsqrt28ss, (d), (k), (s1), (s2))
#define INSTR_CREATE_vrsqrt28sd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vrsqrt28sd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vscalefps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vscalefps, (d), (k), (s1), (s2))
#define INSTR_CREATE_vscalefpd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vscalefpd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vscalefss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vscalefss, (d), (k), (s1), (s2))
#define INSTR_CREATE_vscalefsd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vscalefsd, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmadd52huq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmadd52huq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vpmadd52luq_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vpmadd52luq, (d), (k), (s1), (s2))
#define INSTR_CREATE_vsqrtss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vsqrtss, (d), (k), (s1), (s2))
#define INSTR_CREATE_vsqrtsd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vsqrtsd, (d), (k), (s1), (s2))
/** @} */ /* end doxygen group */

/** @name 1 destination, 3 sources including one immediate */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s1 The opnd_t explicit first source operand for the instruction.
 * \param s2 The opnd_t explicit second source operand for the instruction
 * \param i  The opnd_t explicit third source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()).
 */
/* AVX */
#define INSTR_CREATE_vcmpps(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vcmpps, (d), (s1), (s2), (i))
#define INSTR_CREATE_vcmpss(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vcmpss, (d), (s1), (s2), (i))
#define INSTR_CREATE_vcmppd(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vcmppd, (d), (s1), (s2), (i))
#define INSTR_CREATE_vcmpsd(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vcmpsd, (d), (s1), (s2), (i))
#define INSTR_CREATE_vpinsrw(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vpinsrw, (d), (s1), (s2), (i))
#define INSTR_CREATE_vshufps(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vshufps, (d), (s1), (s2), (i))
#define INSTR_CREATE_vshufpd(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vshufpd, (d), (s1), (s2), (i))
#define INSTR_CREATE_vpalignr(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vpalignr, (d), (s1), (s2), (i))
#define INSTR_CREATE_vblendps(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vblendps, (d), (s1), (s2), (i))
#define INSTR_CREATE_vblendpd(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vblendpd, (d), (s1), (s2), (i))
#define INSTR_CREATE_vpblendw(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vpblendw, (d), (s1), (s2), (i))
#define INSTR_CREATE_vpinsrb(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vpinsrb, (d), (s1), (s2), (i))
#define INSTR_CREATE_vinsertps(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vinsertps, (d), (s1), (s2), (i))
#define INSTR_CREATE_vpinsrd(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vpinsrd, (d), (s1), (s2), (i))
#define INSTR_CREATE_vpinsrq(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vpinsrq, (d), (s1), (s2), (i))
#define INSTR_CREATE_vdpps(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vdpps, (d), (s1), (s2), (i))
#define INSTR_CREATE_vdppd(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vdppd, (d), (s1), (s2), (i))
#define INSTR_CREATE_vmpsadbw(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vmpsadbw, (d), (s1), (s2), (i))
#define INSTR_CREATE_vpclmulqdq(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vpclmulqdq, (d), (s1), (s2), (i))
#define INSTR_CREATE_vroundss(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vroundss, (d), (s1), (s2), (i))
#define INSTR_CREATE_vroundsd(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vroundsd, (d), (s1), (s2), (i))
#define INSTR_CREATE_vperm2f128(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vperm2f128, (d), (s1), (s2), (i))
#define INSTR_CREATE_vinsertf128(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vinsertf128, (d), (s1), (s2), (i))
/** @} */ /* end doxygen group */

/* 1 destination, 3 sources: 1 implicit */
/** @name 1 destination, 3 sources: 1 implicit */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s The opnd_t explicit source operand for the instruction.
 * \param ri The opnd_t explicit source operand for the instruction, which must
 * be one of the following:
 * -# The register cl (#opnd_create_reg(#DR_REG_CL));
 * -# An immediate integer (opnd_create_immed_int()) of size #OPSZ_1;
 */
#define INSTR_CREATE_shld(dc, d, s, ri) \
    instr_create_1dst_3src((dc), OP_shld, (d), (s), (ri), (d))
#define INSTR_CREATE_shrd(dc, d, s, ri) \
    instr_create_1dst_3src((dc), OP_shrd, (d), (s), (ri), (d))
/** @} */ /* end doxygen group */
/** @name 1 destination, 3 sources: 1 implicit, 1 immed */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s The opnd_t explicit source operand for the instruction.
 * \param i The opnd_t explicit second source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_pclmulqdq(dc, d, s, i) \
    instr_create_1dst_3src((dc), OP_pclmulqdq, (d), (s), (i), (d))
#define INSTR_CREATE_blendps(dc, d, s, i) \
    instr_create_1dst_3src((dc), OP_blendps, (d), (s), (i), (d))
#define INSTR_CREATE_blendpd(dc, d, s, i) \
    instr_create_1dst_3src((dc), OP_blendpd, (d), (s), (i), (d))
#define INSTR_CREATE_pblendw(dc, d, s, i) \
    instr_create_1dst_3src((dc), OP_pblendw, (d), (s), (i), (d))
/** @} */ /* end doxygen group */
/** @name 1 explicit destination, 2 explicit sources, 1 implicit source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s The opnd_t explicit source operand for the instruction.
 * \param i The opnd_t explicit second source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_shufps(dc, d, s, i) \
    instr_create_1dst_3src((dc), OP_shufps, (d), (s), (i), (d))
#define INSTR_CREATE_shufpd(dc, d, s, i) \
    instr_create_1dst_3src((dc), OP_shufpd, (d), (s), (i), (d))
#define INSTR_CREATE_cmpps(dc, d, s, i) \
    instr_create_1dst_3src((dc), OP_cmpps, (d), (s), (i), (d))
#define INSTR_CREATE_cmpss(dc, d, s, i) \
    instr_create_1dst_3src((dc), OP_cmpss, (d), (s), (i), (d))
#define INSTR_CREATE_cmppd(dc, d, s, i) \
    instr_create_1dst_3src((dc), OP_cmppd, (d), (s), (i), (d))
#define INSTR_CREATE_cmpsd(dc, d, s, i) \
    instr_create_1dst_3src((dc), OP_cmpsd, (d), (s), (i), (d))
#define INSTR_CREATE_palignr(dc, d, s, i) \
    instr_create_1dst_3src((dc), OP_palignr, (d), (s), (i), (d))
#define INSTR_CREATE_dpps(dc, d, s, i) \
    instr_create_1dst_3src((dc), OP_dpps, (d), (s), (i), (d))
#define INSTR_CREATE_dppd(dc, d, s, i) \
    instr_create_1dst_3src((dc), OP_dppd, (d), (s), (i), (d))
#define INSTR_CREATE_mpsadbw(dc, d, s, i) \
    instr_create_1dst_3src((dc), OP_mpsadbw, (d), (s), (i), (d))
/* SHA */
#define INSTR_CREATE_sha1rnds4(dc, d, s, i) \
    instr_create_1dst_3src((dc), OP_sha1rnds4, (d), (s), (i), (d))
/** @} */ /* end doxygen group */

/** @name 1 explicit destination, 2 explicit sources, dest is implicit source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s1 The opnd_t explicit first source operand for the instruction.
 * \param s2 The opnd_t explicit second source operand for the instruction.
 */
/* FMA */
#define INSTR_CREATE_vfmadd132ps(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmadd132ps, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd132pd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmadd132pd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd213ps(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmadd213ps, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd213pd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmadd213pd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd231ps(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmadd231ps, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd231pd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmadd231pd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd132ss(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmadd132ss, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd132sd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmadd132sd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd213ss(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmadd213ss, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd213sd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmadd213sd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd231ss(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmadd231ss, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd231sd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmadd231sd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmaddsub132ps(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmaddsub132ps, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmaddsub132pd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmaddsub132pd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmaddsub213ps(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmaddsub213ps, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmaddsub213pd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmaddsub213pd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmaddsub231ps(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmaddsub231ps, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmaddsub231pd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmaddsub231pd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmsubadd132ps(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmsubadd132ps, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmsubadd132pd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmsubadd132pd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmsubadd213ps(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmsubadd213ps, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmsubadd213pd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmsubadd213pd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmsubadd231ps(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmsubadd231ps, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmsubadd231pd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmsubadd231pd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub132ps(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmsub132ps, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub132pd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmsub132pd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub213ps(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmsub213ps, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub213pd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmsub213pd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub231ps(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmsub231ps, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub231pd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmsub231pd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub132ss(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmsub132ss, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub132sd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmsub132sd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub213ss(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmsub213ss, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub213sd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmsub213sd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub231ss(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmsub231ss, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub231sd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfmsub231sd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd132ps(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmadd132ps, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd132pd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmadd132pd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd213ps(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmadd213ps, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd213pd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmadd213pd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd231ps(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmadd231ps, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd231pd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmadd231pd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd132ss(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmadd132ss, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd132sd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmadd132sd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd213ss(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmadd213ss, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd213sd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmadd213sd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd231ss(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmadd231ss, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd231sd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmadd231sd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub132ps(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmsub132ps, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub132pd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmsub132pd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub213ps(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmsub213ps, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub213pd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmsub213pd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub231ps(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmsub231ps, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub231pd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmsub231pd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub132ss(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmsub132ss, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub132sd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmsub132sd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub213ss(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmsub213ss, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub213sd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmsub213sd, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub231ss(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmsub231ss, (d), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub231sd(dc, d, s1, s2) \
    instr_create_1dst_3src((dc), OP_vfnmsub231sd, (d), (s1), (s2), (d))
/** @} */ /* end doxygen group */

/** @name 1 explicit destination, 1 mask, 2 explicit sources, dest is implicit source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param k The opnd_t explicit mask source operand for the instruction.
 * \param s1 The opnd_t explicit first source operand for the instruction.
 * \param s2 The opnd_t explicit second source operand for the instruction.
 */
/* AVX-512 FMA */
#define INSTR_CREATE_vfmadd132ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmadd132ps, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd132pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmadd132pd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd213ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmadd213ps, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd213pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmadd213pd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd231ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmadd231ps, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd231pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmadd231pd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd132ss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmadd132ss, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd132sd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmadd132sd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd213ss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmadd213ss, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd213sd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmadd213sd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd231ss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmadd231ss, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmadd231sd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmadd231sd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmaddsub132ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmaddsub132ps, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmaddsub132pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmaddsub132pd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmaddsub213ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmaddsub213ps, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmaddsub213pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmaddsub213pd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmaddsub231ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmaddsub231ps, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmaddsub231pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmaddsub231pd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmsubadd132ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmsubadd132ps, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmsubadd132pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmsubadd132pd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmsubadd213ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmsubadd213ps, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmsubadd213pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmsubadd213pd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmsubadd231ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmsubadd231ps, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmsubadd231pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmsubadd231pd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub132ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmsub132ps, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub132pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmsub132pd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub213ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmsub213ps, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub213pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmsub213pd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub231ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmsub231ps, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub231pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmsub231pd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub132ss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmsub132ss, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub132sd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmsub132sd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub213ss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmsub213ss, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub213sd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmsub213sd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub231ss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmsub231ss, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfmsub231sd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfmsub231sd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd132ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmadd132ps, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd132pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmadd132pd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd213ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmadd213ps, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd213pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmadd213pd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd231ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmadd231ps, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd231pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmadd231pd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd132ss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmadd132ss, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd132sd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmadd132sd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd213ss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmadd213ss, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd213sd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmadd213sd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd231ss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmadd231ss, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmadd231sd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmadd231sd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub132ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmsub132ps, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub132pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmsub132pd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub213ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmsub213ps, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub213pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmsub213pd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub231ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmsub231ps, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub231pd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmsub231pd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub132ss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmsub132ss, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub132sd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmsub132sd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub213ss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmsub213ss, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub213sd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmsub213sd, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub231ss_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmsub231ss, (d), (k), (s1), (s2), (d))
#define INSTR_CREATE_vfnmsub231sd_mask(dc, d, k, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfnmsub231sd, (d), (k), (s1), (s2), (d))
/* AVX512 BF16*/
#define INSTR_CREATE_vcvtne2ps2bf16_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vcvtne2ps2bf16, (d), (k), (s1), (s2))
#define INSTR_CREATE_vdpbf16ps_mask(dc, d, k, s1, s2) \
    instr_create_1dst_3src((dc), OP_vdpbf16ps, (d), (k), (s1), (s2))
/** @} */ /* end doxygen group */

/** @name 1 explicit destination, 3 explicit sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s1 The opnd_t explicit first source operand for the instruction.
 * \param s2 The opnd_t explicit second source operand for the instruction.
 * \param s3 The opnd_t explicit third source operand for the instruction.
 */
/* FMA4 */
#define INSTR_CREATE_vfmaddsubps(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfmaddsubps, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfmaddsubpd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfmaddsubpd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfmsubaddps(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfmsubaddps, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfmsubaddpd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfmsubaddpd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfmaddps(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfmaddps, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfmaddpd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfmaddpd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfmaddss(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfmaddss, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfmaddsd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfmaddsd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfmsubps(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfmsubps, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfmsubpd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfmsubpd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfmsubss(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfmsubss, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfmsubsd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfmsubsd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfnmaddps(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfnmaddps, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfnmaddpd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfnmaddpd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfnmaddss(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfnmaddss, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfnmaddsd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfnmaddsd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfnmsubps(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfnmsubps, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfnmsubpd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfnmsubpd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfnmsubss(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfnmsubss, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vfnmsubsd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vfnmsubsd, (d), (s1), (s2), (s3))
/* XOP */
#define INSTR_CREATE_vpmacssww(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpmacssww, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpmacsswd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpmacsswd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpmacssdql(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpmacssdql, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpmacssdd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpmacssdd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpmacssdqh(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpmacssdqh, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpmacsww(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpmacsww, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpmacswd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpmacswd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpmacsdql(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpmacsdql, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpmacsdd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpmacsdd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpmacsdqh(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpmacsdqh, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpmadcsswd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpmadcsswd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpmadcswd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpmadcswd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpperm(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpperm, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpcmov(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpcmov, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpermil2pd(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpermil2pd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpermil2ps(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpermil2ps, (d), (s1), (s2), (s3))
/* AVX512 VNNI */
#define INSTR_CREATE_vpdpbusd_mask(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpdpbusd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpdpbusds_mask(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpdpbusds, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpdpwssd_mask(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpdpwssd, (d), (s1), (s2), (s3))
#define INSTR_CREATE_vpdpwssds_mask(dc, d, s1, s2, s3) \
    instr_create_1dst_3src((dc), OP_vpdpwssds, (d), (s1), (s2), (s3))
/** @} */ /* end doxygen group */
/** @name 1 destination, 3 sources where the final is an immediate */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s1 The opnd_t explicit first source operand for the instruction.
 * \param s2 The opnd_t explicit second source operand for the instruction.
 * \param i The opnd_t explicit third source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_vpcomb(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vpcomb, (d), (s1), (s2), (i))
#define INSTR_CREATE_vpcomw(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vpcomw, (d), (s1), (s2), (i))
#define INSTR_CREATE_vpcomd(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vpcomd, (d), (s1), (s2), (i))
#define INSTR_CREATE_vpcomq(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vpcomq, (d), (s1), (s2), (i))
#define INSTR_CREATE_vpcomub(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vpcomub, (d), (s1), (s2), (i))
#define INSTR_CREATE_vpcomuw(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vpcomuw, (d), (s1), (s2), (i))
#define INSTR_CREATE_vpcomud(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vpcomud, (d), (s1), (s2), (i))
#define INSTR_CREATE_vpcomuq(dc, d, s1, s2, i) \
    instr_create_1dst_3src((dc), OP_vpcomuq, (d), (s1), (s2), (i))
/** @} */ /* end doxygen group */

/** @name 1 explicit destination, 1 mask, 3 sources where the final is an immediate */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param k The opnd_t explicit mask source operand for the instruction.
 * \param i The opnd_t explicit third source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()).
 * \param s1 The opnd_t explicit first source operand for the instruction.
 * \param s2 The opnd_t explicit second source operand for the instruction.
 */
/* AVX-512 EVEX */
#define INSTR_CREATE_vinsertf32x4_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vinsertf32x4, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vinsertf64x2_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vinsertf64x2, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vinsertf32x8_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vinsertf32x8, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vinsertf64x4_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vinsertf64x4, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vinserti32x4_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vinserti32x4, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vinserti64x2_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vinserti64x2, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vinserti32x8_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vinserti32x8, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vinserti64x4_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vinserti64x4, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vpcmpb_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vpcmpb, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vpcmpw_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vpcmpw, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vpcmpd_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vpcmpd, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vpcmpq_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vpcmpq, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vpcmpub_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vpcmpub, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vpcmpuw_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vpcmpuw, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vpcmpud_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vpcmpud, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vpcmpuq_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vpcmpuq, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vcmpps_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vcmpps, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vcmpss_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vcmpss, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vcmppd_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vcmppd, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vcmpsd_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vcmpsd, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vshufps_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vshufps, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vshufpd_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vshufpd, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vshuff32x4_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vshuff32x4, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vshuff64x2_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vshuff64x2, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vshufi32x4_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vshufi32x4, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vshufi64x2_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vshufi64x2, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vpalignr_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vpalignr, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_valignd_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_valignd, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_valignq_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_valignq, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vfixupimmps_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfixupimmps, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vfixupimmpd_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfixupimmpd, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vfixupimmss_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfixupimmss, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vfixupimmsd_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vfixupimmsd, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vgetmantss_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vgetmantss, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vgetmantsd_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vgetmantsd, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vrangeps_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vrangeps, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vrangepd_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vrangepd, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vrangess_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vrangess, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vrangesd_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vrangesd, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vreducess_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vreducess, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vreducesd_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vreducesd, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vdbpsadbw_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vdbpsadbw, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vpternlogd_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vpternlogd, (d), (k), (i), (s1), (s2))
#define INSTR_CREATE_vpternlogq_mask(dc, d, k, i, s1, s2) \
    instr_create_1dst_4src((dc), OP_vpternlogq, (d), (k), (i), (s1), (s2))
/** @} */ /* end doxygen group */

/** @name 1 destination, 3 sources where 2 are implicit */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s  The opnd_t explicit source operand for the instruction.
 */
/* SSE4 */
#define INSTR_CREATE_pblendvb(dc, d, s) \
    instr_create_1dst_3src((dc), OP_pblendvb, (d), (s), opnd_create_reg(DR_REG_XMM0), (d))
#define INSTR_CREATE_blendvps(dc, d, s) \
    instr_create_1dst_3src((dc), OP_blendvps, (d), (s), opnd_create_reg(DR_REG_XMM0), (d))
#define INSTR_CREATE_blendvpd(dc, d, s) \
    instr_create_1dst_3src((dc), OP_blendvpd, (d), (s), opnd_create_reg(DR_REG_XMM0), (d))
/* SHA */
#define INSTR_CREATE_sha256rnds2(dc, d, s)                                               \
    instr_create_1dst_3src((dc), OP_sha256rnds2, (d), (s), opnd_create_reg(DR_REG_XMM0), \
                           (d))
/** @} */ /* end doxygen group */

/* 1 implicit destination, 3 sources */
/** @name 1 implicit destination, 3 sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s1 The opnd_t explicit first source operand for the instruction.
 * \param s2 The opnd_t explicit second source operand for the instruction.
 * \param i The opnd_t explicit third source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_pcmpistrm(dc, s1, s2, i)                                            \
    instr_create_1dst_3src((dc), OP_pcmpistrm, opnd_create_reg(DR_REG_XMM0), (s1), (s2), \
                           (i))
#define INSTR_CREATE_pcmpistri(dc, s1, s2, i)                                           \
    instr_create_1dst_3src((dc), OP_pcmpistri, opnd_create_reg(DR_REG_ECX), (s1), (s2), \
                           (i))
#define INSTR_CREATE_vpcmpistrm(dc, s1, s2, i)                                      \
    instr_create_1dst_3src((dc), OP_vpcmpistrm, opnd_create_reg(DR_REG_XMM0), (s1), \
                           (s2), (i))
#define INSTR_CREATE_vpcmpistri(dc, s1, s2, i)                                           \
    instr_create_1dst_3src((dc), OP_vpcmpistri, opnd_create_reg(DR_REG_ECX), (s1), (s2), \
                           (i))
/** @} */ /* end doxygen group */

/* 1 implicit destination, 3 sources: 2 implicit */
/** @name 1 implicit destination, 3 sources: 2 implicit */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx_imm macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands. The _imm
 * suffix distinguishes between alternative forms of the same opcode: these
 * forms take an explicit immediate.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param i The opnd_t explicit second source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_ret_imm(dc, i)                                                  \
    instr_create_1dst_3src(                                                          \
        (dc), OP_ret, opnd_create_reg(DR_REG_XSP), (i), opnd_create_reg(DR_REG_XSP), \
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_ret))
/* XXX: blindly asking for rex.w (b/c 32-bit is default for x64) but don't
 * know x64 mode! */
#define INSTR_CREATE_ret_far_imm(dc, i)                                         \
    instr_create_1dst_3src((dc), OP_ret_far, opnd_create_reg(DR_REG_XSP), (i),  \
                           opnd_create_reg(DR_REG_XSP),                         \
                           opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, \
                                                 IF_X64_ELSE(OPSZ_16, OPSZ_8)))
/** @} */ /* end doxygen group */

/* 1 implicit destination, 5 sources: 2 implicit */
/** @name 1 implicit destination, 5 sources: 2 implicit */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s1 The opnd_t explicit first source operand for the instruction.
 * \param s2 The opnd_t explicit second source operand for the instruction.
 * \param i The opnd_t explicit third source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_pcmpestrm(dc, s1, s2, i)                                            \
    instr_create_1dst_5src((dc), OP_pcmpestrm, opnd_create_reg(DR_REG_XMM0), (s1), (s2), \
                           (i), opnd_create_reg(DR_REG_EAX),                             \
                           opnd_create_reg(DR_REG_EDX))
#define INSTR_CREATE_pcmpestri(dc, s1, s2, i)                                           \
    instr_create_1dst_5src((dc), OP_pcmpestri, opnd_create_reg(DR_REG_ECX), (s1), (s2), \
                           (i), opnd_create_reg(DR_REG_EAX),                            \
                           opnd_create_reg(DR_REG_EDX))
/* AVX */
#define INSTR_CREATE_vpcmpestrm(dc, s1, s2, i)                                      \
    instr_create_1dst_5src((dc), OP_vpcmpestrm, opnd_create_reg(DR_REG_XMM0), (s1), \
                           (s2), (i), opnd_create_reg(DR_REG_EAX),                  \
                           opnd_create_reg(DR_REG_EDX))
#define INSTR_CREATE_vpcmpestri(dc, s1, s2, i)                                           \
    instr_create_1dst_5src((dc), OP_vpcmpestri, opnd_create_reg(DR_REG_ECX), (s1), (s2), \
                           (i), opnd_create_reg(DR_REG_EAX),                             \
                           opnd_create_reg(DR_REG_EDX))
/** @} */ /* end doxygen group */

/* 2 implicit destinations, no sources */
/** @name 2 implicit destinations, no sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx, automatically
 * supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define INSTR_CREATE_rdtsc(dc)                                          \
    instr_create_2dst_0src((dc), OP_rdtsc, opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX))
#ifdef X64
#    define INSTR_CREATE_syscall(dc)                                          \
        instr_create_2dst_0src((dc), OP_syscall, opnd_create_reg(DR_REG_XCX), \
                               opnd_create_reg(DR_REG_R11))
#endif
/** @} */ /* end doxygen group */

/* 2 destinations: 1 implicit, 1 source */
/** @name 2 destinations: 1 implicit, 1 source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s The opnd_t explicit source operand for the instruction.
 */
#define INSTR_CREATE_lds(dc, d, s) \
    instr_create_2dst_1src((dc), OP_lds, (d), opnd_create_reg(DR_SEG_DS), (s))
#define INSTR_CREATE_lss(dc, d, s) \
    instr_create_2dst_1src((dc), OP_lss, (d), opnd_create_reg(DR_SEG_SS), (s))
#define INSTR_CREATE_les(dc, d, s) \
    instr_create_2dst_1src((dc), OP_les, (d), opnd_create_reg(DR_SEG_ES), (s))
#define INSTR_CREATE_lfs(dc, d, s) \
    instr_create_2dst_1src((dc), OP_lfs, (d), opnd_create_reg(DR_SEG_FS), (s))
#define INSTR_CREATE_lgs(dc, d, s) \
    instr_create_2dst_1src((dc), OP_lgs, (d), opnd_create_reg(DR_SEG_GS), (s))
/** @} */ /* end doxygen group */

/* 2 implicit destinations, 1 implicit source */
/** @name 2 implicit destinations, 1 implicit source */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx, automatically
 * supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define INSTR_CREATE_pushf(dc)                                                     \
    instr_create_2dst_1src((dc), OP_pushf, opnd_create_reg(DR_REG_XSP),            \
                           opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,       \
                                                 IF_X64_ELSE(-8, -4), OPSZ_STACK), \
                           opnd_create_reg(DR_REG_XSP))
#define INSTR_CREATE_rdmsr(dc)                                          \
    instr_create_2dst_1src((dc), OP_rdmsr, opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_ECX))
#define INSTR_CREATE_rdpmc(dc)                                          \
    instr_create_2dst_1src((dc), OP_rdpmc, opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_ECX))
#define INSTR_CREATE_xgetbv(dc)                                          \
    instr_create_2dst_1src((dc), OP_xgetbv, opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_ECX))
#define INSTR_CREATE_rdpkru(dc)                                          \
    instr_create_2dst_1src((dc), OP_rdpkru, opnd_create_reg(DR_REG_EAX), \
                           opnd_create_reg(DR_REG_EDX), opnd_create_reg(DR_REG_ECX))
/** @} */ /* end doxygen group */

/* 2 destinations: 1 implicit, 2 sources */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 */
#define INSTR_CREATE_pop(dc, d)                                                      \
    instr_create_2dst_2src(                                                          \
        (dc), OP_pop, (d), opnd_create_reg(DR_REG_XSP), opnd_create_reg(DR_REG_XSP), \
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_VARSTACK))
/** @name 2 implicit destinations, 3 sources: 1 implicit */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx, automatically
 * supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s1 The opnd_t first source operand for the instruction.
 * \param s2 The opnd_t second source operand for the instruction.
 */
#define INSTR_CREATE_vpgatherdd(dc, d, s1, s2) \
    instr_create_2dst_2src((dc), OP_vpgatherdd, (d), (s2), (s1), (s2))
#define INSTR_CREATE_vpgatherdq(dc, d, s1, s2) \
    instr_create_2dst_2src((dc), OP_vpgatherdq, (d), (s2), (s1), (s2))
#define INSTR_CREATE_vpgatherqd(dc, d, s1, s2) \
    instr_create_2dst_2src((dc), OP_vpgatherqd, (d), (s2), (s1), (s2))
#define INSTR_CREATE_vpgatherqq(dc, d, s1, s2) \
    instr_create_2dst_2src((dc), OP_vpgatherqq, (d), (s2), (s1), (s2))
#define INSTR_CREATE_vgatherdps(dc, d, s1, s2) \
    instr_create_2dst_2src((dc), OP_vgatherdps, (d), (s2), (s1), (s2))
#define INSTR_CREATE_vgatherdpd(dc, d, s1, s2) \
    instr_create_2dst_2src((dc), OP_vgatherdpd, (d), (s2), (s1), (s2))
#define INSTR_CREATE_vgatherqps(dc, d, s1, s2) \
    instr_create_2dst_2src((dc), OP_vgatherqps, (d), (s2), (s1), (s2))
#define INSTR_CREATE_vgatherqpd(dc, d, s1, s2) \
    instr_create_2dst_2src((dc), OP_vgatherqpd, (d), (s2), (s1), (s2))
/** @} */ /* end doxygen group */

/** @name 2 implicit destinations, 1 mask, andd 3 sources: The mask is implicit */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx, automatically
 * supplying the mask as an implicit operand.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param k The opnd_t first source operand for the instruction.
 * \param s The opnd_t second source operand for the instruction.
 */
/* AVX-512 EVEX */
/* Technically we wouldn't have to provide a separate macro for the AVX-512 version
 * of the instruction since the number of operands are the same. But the AVX-512
 * derivative of vgather is distinct in taking an EVEX mask instead of an xmm mask. The
 * EVEX mask derivatives of all opcodes have a _mask macro version, so we provide one
 * here as well.
 */
#define INSTR_CREATE_vpgatherdd_mask(dc, d, k, s) \
    instr_create_2dst_2src((dc), OP_vpgatherdd, (d), (k), (k), (s))
#define INSTR_CREATE_vpgatherdq_mask(dc, d, k, s) \
    instr_create_2dst_2src((dc), OP_vpgatherdq, (d), (k), (k), (s))
#define INSTR_CREATE_vpgatherqd_mask(dc, d, k, s) \
    instr_create_2dst_2src((dc), OP_vpgatherqd, (d), (k), (k), (s))
#define INSTR_CREATE_vpgatherqq_mask(dc, d, k, s) \
    instr_create_2dst_2src((dc), OP_vpgatherqq, (d), (k), (k), (s))
#define INSTR_CREATE_vgatherdps_mask(dc, d, k, s) \
    instr_create_2dst_2src((dc), OP_vgatherdps, (d), (k), (k), (s))
#define INSTR_CREATE_vgatherdpd_mask(dc, d, k, s) \
    instr_create_2dst_2src((dc), OP_vgatherdpd, (d), (k), (k), (s))
#define INSTR_CREATE_vgatherqps_mask(dc, d, k, s) \
    instr_create_2dst_2src((dc), OP_vgatherqps, (d), (k), (k), (s))
#define INSTR_CREATE_vgatherqpd_mask(dc, d, k, s) \
    instr_create_2dst_2src((dc), OP_vgatherqpd, (d), (k), (k), (s))
#define INSTR_CREATE_vpscatterdd_mask(dc, d, k, s) \
    instr_create_2dst_2src((dc), OP_vpscatterdd, (d), (k), (k), (s))
#define INSTR_CREATE_vpscatterdq_mask(dc, d, k, s) \
    instr_create_2dst_2src((dc), OP_vpscatterdq, (d), (k), (k), (s))
#define INSTR_CREATE_vpscatterqd_mask(dc, d, k, s) \
    instr_create_2dst_2src((dc), OP_vpscatterqd, (d), (k), (k), (s))
#define INSTR_CREATE_vpscatterqq_mask(dc, d, k, s) \
    instr_create_2dst_2src((dc), OP_vpscatterqq, (d), (k), (k), (s))
#define INSTR_CREATE_vscatterdps_mask(dc, d, k, s) \
    instr_create_2dst_2src((dc), OP_vscatterdps, (d), (k), (k), (s))
#define INSTR_CREATE_vscatterdpd_mask(dc, d, k, s) \
    instr_create_2dst_2src((dc), OP_vscatterdpd, (d), (k), (k), (s))
#define INSTR_CREATE_vscatterqps_mask(dc, d, k, s) \
    instr_create_2dst_2src((dc), OP_vscatterqps, (d), (k), (k), (s))
#define INSTR_CREATE_vscatterqpd_mask(dc, d, k, s) \
    instr_create_2dst_2src((dc), OP_vscatterqpd, (d), (k), (k), (s))
/** @} */ /* end doxygen group */

/* 2 destinations: 1 implicit, 2 sources: 1 implicit */
/** @name 2 destinations: 1 implicit, 2 sources: 1 implicit */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s The opnd_t explicit source operand for the instruction.
 */
#define INSTR_CREATE_xchg(dc, d, s) \
    instr_create_2dst_2src((dc), OP_xchg, (d), (s), (d), (s))
#define INSTR_CREATE_xadd(dc, d, s) \
    instr_create_2dst_2src((dc), OP_xadd, (d), (s), (d), (s))
/** @} */ /* end doxygen group */

/* string instructions */
/** @name String instructions */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx_1 or INSTR_CREATE_xxx_4 macro creates an instr_t with opcode
 * OP_xxx, automatically supplying any implicit operands.  The _1 or _4 suffixes
 * distinguish between alternative forms of the same opcode (1 and 4 identify the
 * operand size).
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define INSTR_CREATE_ins_1(dc)                                                       \
    instr_create_2dst_2src(                                                          \
        (dc), OP_ins,                                                                \
        opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, 0, 0, OPSZ_1), \
        opnd_create_reg(DR_REG_XDI), opnd_create_reg(DR_REG_DX),                     \
        opnd_create_reg(DR_REG_XDI))
#define INSTR_CREATE_ins_4(dc)                                                           \
    instr_create_2dst_2src((dc), OP_ins,                                                 \
                           opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_reg(DR_REG_XDI), opnd_create_reg(DR_REG_DX),      \
                           opnd_create_reg(DR_REG_XDI))
#define INSTR_CREATE_stos_1(dc)                                                      \
    instr_create_2dst_2src(                                                          \
        (dc), OP_stos,                                                               \
        opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, 0, 0, OPSZ_1), \
        opnd_create_reg(DR_REG_XDI), opnd_create_reg(DR_REG_AL),                     \
        opnd_create_reg(DR_REG_XDI))
#define INSTR_CREATE_stos_4(dc)                                                          \
    instr_create_2dst_2src((dc), OP_stos,                                                \
                           opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_short2),               \
                           opnd_create_reg(DR_REG_XDI), opnd_create_reg(DR_REG_EAX),     \
                           opnd_create_reg(DR_REG_XDI))
#define INSTR_CREATE_stos_8(dc)                                                          \
    instr_create_2dst_2src((dc), OP_stos,                                                \
                           opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, \
                                                     0, 0, OPSZ_8_short2),               \
                           opnd_create_reg(DR_REG_XDI), opnd_create_reg(DR_REG_XAX),     \
                           opnd_create_reg(DR_REG_XDI))
#define INSTR_CREATE_lods_1(dc)                                                      \
    instr_create_2dst_2src(                                                          \
        (dc), OP_lods, opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_XSI),      \
        opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, 0, 0, OPSZ_1), \
        opnd_create_reg(DR_REG_XSI))
#define INSTR_CREATE_lods_4(dc)                                                          \
    instr_create_2dst_2src((dc), OP_lods, opnd_create_reg(DR_REG_EAX),                   \
                           opnd_create_reg(DR_REG_XSI),                                  \
                           opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_short2),               \
                           opnd_create_reg(DR_REG_XSI))
#define INSTR_CREATE_lods_8(dc)                                                          \
    instr_create_2dst_2src((dc), OP_lods, opnd_create_reg(DR_REG_XAX),                   \
                           opnd_create_reg(DR_REG_XSI),                                  \
                           opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, \
                                                     0, 0, OPSZ_8_short2),               \
                           opnd_create_reg(DR_REG_XSI))
#define INSTR_CREATE_movs_1(dc)                                                      \
    instr_create_3dst_3src(                                                          \
        (dc), OP_movs,                                                               \
        opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, 0, 0, OPSZ_1), \
        opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI),                    \
        opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, 0, 0, OPSZ_1), \
        opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI))
#define INSTR_CREATE_movs_4(dc)                                                          \
    instr_create_3dst_3src((dc), OP_movs,                                                \
                           opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI),     \
                           opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI))
#define INSTR_CREATE_rep_ins_1(dc)                                                   \
    instr_create_3dst_3src(                                                          \
        (dc), OP_rep_ins,                                                            \
        opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, 0, 0, OPSZ_1), \
        opnd_create_reg(DR_REG_XDI), opnd_create_reg(DR_REG_XCX),                    \
        opnd_create_reg(DR_REG_DX), opnd_create_reg(DR_REG_XDI),                     \
        opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_rep_ins_4(dc)                                                       \
    instr_create_3dst_3src((dc), OP_rep_ins,                                             \
                           opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_reg(DR_REG_XDI), opnd_create_reg(DR_REG_XCX),     \
                           opnd_create_reg(DR_REG_DX), opnd_create_reg(DR_REG_XDI),      \
                           opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_rep_stos_1(dc)                                                  \
    instr_create_3dst_3src(                                                          \
        (dc), OP_rep_stos,                                                           \
        opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, 0, 0, OPSZ_1), \
        opnd_create_reg(DR_REG_XDI), opnd_create_reg(DR_REG_XCX),                    \
        opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_XDI),                     \
        opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_rep_stos_4(dc)                                                      \
    instr_create_3dst_3src((dc), OP_rep_stos,                                            \
                           opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_reg(DR_REG_XDI), opnd_create_reg(DR_REG_XCX),     \
                           opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_XDI),     \
                           opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_rep_lods_1(dc)                                                  \
    instr_create_3dst_3src(                                                          \
        (dc), OP_rep_lods, opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_XSI),  \
        opnd_create_reg(DR_REG_XCX),                                                 \
        opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, 0, 0, OPSZ_1), \
        opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_rep_lods_4(dc)                                                      \
    instr_create_3dst_3src((dc), OP_rep_lods, opnd_create_reg(DR_REG_EAX),               \
                           opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XCX),     \
                           opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_rep_movs_1(dc)                                                  \
    instr_create_4dst_4src(                                                          \
        (dc), OP_rep_movs,                                                           \
        opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, 0, 0, OPSZ_1), \
        opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI),                    \
        opnd_create_reg(DR_REG_XCX),                                                 \
        opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, 0, 0, OPSZ_1), \
        opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI),                    \
        opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_rep_movs_4(dc)                                                      \
    instr_create_4dst_4src((dc), OP_rep_movs,                                            \
                           opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI),     \
                           opnd_create_reg(DR_REG_XCX),                                  \
                           opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI),     \
                           opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_outs_1(dc)                                                      \
    instr_create_1dst_3src(                                                          \
        (dc), OP_outs, opnd_create_reg(DR_REG_XSI),                                  \
        opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, 0, 0, OPSZ_1), \
        opnd_create_reg(DR_REG_DX), opnd_create_reg(DR_REG_XSI))
#define INSTR_CREATE_outs_4(dc)                                                          \
    instr_create_1dst_3src((dc), OP_outs, opnd_create_reg(DR_REG_XSI),                   \
                           opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_reg(DR_REG_DX), opnd_create_reg(DR_REG_XSI))
#define INSTR_CREATE_cmps_1(dc)                                                      \
    instr_create_2dst_4src(                                                          \
        (dc), OP_cmps, opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI),     \
        opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, 0, 0, OPSZ_1), \
        opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, 0, 0, OPSZ_1), \
        opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI))
#define INSTR_CREATE_cmps_4(dc)                                                          \
    instr_create_2dst_4src((dc), OP_cmps, opnd_create_reg(DR_REG_XSI),                   \
                           opnd_create_reg(DR_REG_XDI),                                  \
                           opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI))
#define INSTR_CREATE_scas_1(dc)                                                      \
    instr_create_1dst_3src(                                                          \
        (dc), OP_scas, opnd_create_reg(DR_REG_XDI),                                  \
        opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, 0, 0, OPSZ_1), \
        opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_XDI))
#define INSTR_CREATE_scas_4(dc)                                                          \
    instr_create_1dst_3src((dc), OP_scas, opnd_create_reg(DR_REG_XDI),                   \
                           opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_XDI))
#define INSTR_CREATE_rep_outs_1(dc)                                                  \
    instr_create_2dst_4src(                                                          \
        (dc), OP_rep_outs, opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XCX), \
        opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, 0, 0, OPSZ_1), \
        opnd_create_reg(DR_REG_DX), opnd_create_reg(DR_REG_XSI),                     \
        opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_rep_outs_4(dc)                                                      \
    instr_create_2dst_4src((dc), OP_rep_outs, opnd_create_reg(DR_REG_XSI),               \
                           opnd_create_reg(DR_REG_XCX),                                  \
                           opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_reg(DR_REG_DX), opnd_create_reg(DR_REG_XSI),      \
                           opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_rep_cmps_1(dc)                                                  \
    instr_create_3dst_5src(                                                          \
        (dc), OP_rep_cmps, opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI), \
        opnd_create_reg(DR_REG_XCX),                                                 \
        opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, 0, 0, OPSZ_1), \
        opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, 0, 0, OPSZ_1), \
        opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI),                    \
        opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_rep_cmps_4(dc)                                                      \
    instr_create_3dst_5src((dc), OP_rep_cmps, opnd_create_reg(DR_REG_XSI),               \
                           opnd_create_reg(DR_REG_XDI), opnd_create_reg(DR_REG_XCX),     \
                           opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI),     \
                           opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_repne_cmps_1(dc)                                                  \
    instr_create_3dst_5src(                                                            \
        (dc), OP_repne_cmps, opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI), \
        opnd_create_reg(DR_REG_XCX),                                                   \
        opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, 0, 0, OPSZ_1),   \
        opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, 0, 0, OPSZ_1),   \
        opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI),                      \
        opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_repne_cmps_4(dc)                                                    \
    instr_create_3dst_5src((dc), OP_repne_cmps, opnd_create_reg(DR_REG_XSI),             \
                           opnd_create_reg(DR_REG_XDI), opnd_create_reg(DR_REG_XCX),     \
                           opnd_create_far_base_disp(DR_SEG_DS, DR_REG_XSI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI),     \
                           opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_rep_scas_1(dc)                                                  \
    instr_create_2dst_4src(                                                          \
        (dc), OP_rep_scas, opnd_create_reg(DR_REG_XDI), opnd_create_reg(DR_REG_XCX), \
        opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, 0, 0, OPSZ_1), \
        opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_XDI),                     \
        opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_rep_scas_4(dc)                                                      \
    instr_create_2dst_4src((dc), OP_rep_scas, opnd_create_reg(DR_REG_XDI),               \
                           opnd_create_reg(DR_REG_XCX),                                  \
                           opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_XDI),     \
                           opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_repne_scas_1(dc)                                                  \
    instr_create_2dst_4src(                                                            \
        (dc), OP_repne_scas, opnd_create_reg(DR_REG_XDI), opnd_create_reg(DR_REG_XCX), \
        opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, 0, 0, OPSZ_1),   \
        opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_XDI),                       \
        opnd_create_reg(DR_REG_XCX))
#define INSTR_CREATE_repne_scas_4(dc)                                                    \
    instr_create_2dst_4src((dc), OP_repne_scas, opnd_create_reg(DR_REG_XDI),             \
                           opnd_create_reg(DR_REG_XCX),                                  \
                           opnd_create_far_base_disp(DR_SEG_ES, DR_REG_XDI, DR_REG_NULL, \
                                                     0, 0, OPSZ_4_rex8_short2),          \
                           opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_XDI),     \
                           opnd_create_reg(DR_REG_XCX))
/** @} */ /* end doxygen group */

/* floating point */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit register operand, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param f The opnd_t explicit source operand for the instruction, which must
 * be a floating point register (opnd_create_reg()).
 */
#define INSTR_CREATE_fxch(dc, f)                                            \
    instr_create_2dst_2src((dc), OP_fxch, opnd_create_reg(DR_REG_ST0), (f), \
                           opnd_create_reg(DR_REG_ST0), (f))

/* 2 destinations, 2 sources: 1 implicit */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param t The opnd_t target operand for the instruction, which can be either
 * a pc (opnd_create_pc()) or an instr_t (opnd_create_instr()).
 */
#define INSTR_CREATE_call(dc, t)                                                   \
    instr_create_2dst_2src((dc), OP_call, opnd_create_reg(DR_REG_XSP),             \
                           opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,       \
                                                 IF_X64_ELSE(-8, -4), OPSZ_STACK), \
                           (t), opnd_create_reg(DR_REG_XSP))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param t The opnd_t target operand for the instruction, which should be
 * a memory reference created with opnd_create_base_disp().
 */
#define INSTR_CREATE_call_ind(dc, t)                                               \
    instr_create_2dst_2src((dc), OP_call_ind, opnd_create_reg(DR_REG_XSP),         \
                           opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,       \
                                                 IF_X64_ELSE(-8, -4), OPSZ_STACK), \
                           (t), opnd_create_reg(DR_REG_XSP))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param t The opnd_t target operand for the instruction, which should be
 * a far pc operand created with opnd_create_far_pc().
 */
/* note: unlike iret/ret_far, 32-bit is typical desired size even for 64-bit mode */
#define INSTR_CREATE_call_far(dc, t)                                        \
    instr_create_2dst_2src(                                                 \
        (dc), OP_call_far, opnd_create_reg(DR_REG_XSP),                     \
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, -8, OPSZ_8), (t), \
        opnd_create_reg(DR_REG_XSP))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param t The opnd_t target operand for the instruction, which should be
 * a far memory reference created with opnd_create_far_base_disp().
 */
/* note: unlike iret/ret_far, 32-bit is typical desired size even for 64-bit mode */
#define INSTR_CREATE_call_far_ind(dc, t)                                    \
    instr_create_2dst_2src(                                                 \
        (dc), OP_call_far_ind, opnd_create_reg(DR_REG_XSP),                 \
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, -8, OPSZ_8), (t), \
        opnd_create_reg(DR_REG_XSP))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param s The opnd_t explicit source operand for the instruction.
 */
#define INSTR_CREATE_push(dc, s)                                                      \
    instr_create_2dst_2src((dc), OP_push, opnd_create_reg(DR_REG_XSP),                \
                           opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,          \
                                                 IF_X64_ELSE(-8, -4), OPSZ_VARSTACK), \
                           (s), opnd_create_reg(DR_REG_XSP))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param i The opnd_t explicit second source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()).
 */
#define INSTR_CREATE_push_imm(dc, i)                                                  \
    instr_create_2dst_2src((dc), OP_push_imm, opnd_create_reg(DR_REG_XSP),            \
                           opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,          \
                                                 IF_X64_ELSE(-8, -4), OPSZ_VARSTACK), \
                           (i), opnd_create_reg(DR_REG_XSP))
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the
 * given explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d1 The opnd_t explicit first destination source operand for the
 * instruction, which must be a general-purpose register.
 * \param d2 The opnd_t explicit first destination source operand for the
 * instruction, which must be a general-purpose register.
 * \param s The opnd_t explicit source operand for the instruction.
 */
#define INSTR_CREATE_mulx(dc, d1, d2, s) \
    instr_create_2dst_2src(              \
        (dc), OP_mulx, (d1), (d2), (s),  \
        opnd_create_reg(reg_resize_to_opsz(DR_REG_EDX, opnd_get_size(d1))))

/* 2 destinations: 1 implicit, 3 sources: 1 implicit */
/** @name 2 destinations: 1 implicit, 3 sources: 1 implicit */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx_1, INSTR_CREATE_xxx_4, or INSTR_CREATE_xxx_8 macro creates an
 * instr_t with opcode OP_xxx and the given explicit operands, automatically
 * supplying any implicit operands.    The _1, _4, or _8 suffixes distinguish between
 * alternative forms of the same opcode with the given operand size.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 * \param s The opnd_t explicit source operand for the instruction.
 */
#define INSTR_CREATE_cmpxchg_1(dc, d, s)                                                \
    instr_create_2dst_3src((dc), OP_cmpxchg, (d), opnd_create_reg(DR_REG_AL), (s), (d), \
                           opnd_create_reg(DR_REG_AL))
#define INSTR_CREATE_cmpxchg_4(dc, d, s)                                                 \
    instr_create_2dst_3src((dc), OP_cmpxchg, (d), opnd_create_reg(DR_REG_EAX), (s), (d), \
                           opnd_create_reg(DR_REG_EAX))
#define INSTR_CREATE_cmpxchg_8(dc, d, s)                                                 \
    instr_create_2dst_3src((dc), OP_cmpxchg, (d), opnd_create_reg(DR_REG_RAX), (s), (d), \
                           opnd_create_reg(DR_REG_RAX))
/** @} */ /* end doxygen group */

/* 2 implicit destinations, 3 implicit sources */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx, automatically
 * supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define INSTR_CREATE_leave(dc)                                                    \
    instr_create_2dst_3src(                                                       \
        (dc), OP_leave, opnd_create_reg(DR_REG_XSP), opnd_create_reg(DR_REG_XBP), \
        opnd_create_reg(DR_REG_XBP), opnd_create_reg(DR_REG_XSP),                 \
        opnd_create_base_disp(DR_REG_XBP, DR_REG_NULL, 0, 0, OPSZ_STACK))

/** @name No destination, many implicit sources */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx, automatically
 * supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
/* 2 implicit destinations, 8 implicit sources */
#define INSTR_CREATE_pusha(dc) instr_create_pusha((dc))

/* 3 implicit destinations, no sources */
#define INSTR_CREATE_rdtscp(dc)                                          \
    instr_create_3dst_0src((dc), OP_rdtscp, opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_ECX))

/* 4 implicit destinations, 2 implicit sources */
#define INSTR_CREATE_cpuid(dc)                                                       \
    instr_create_4dst_2src((dc), OP_cpuid, opnd_create_reg(DR_REG_EAX),              \
                           opnd_create_reg(DR_REG_EBX), opnd_create_reg(DR_REG_ECX), \
                           opnd_create_reg(DR_REG_EDX), opnd_create_reg(DR_REG_EAX), \
                           opnd_create_reg(DR_REG_ECX))

/* 4 implicit destinations, 4 implicit sources */
#define INSTR_CREATE_encls(dc)                                                       \
    instr_create_4dst_4src((dc), OP_encls, opnd_create_reg(DR_REG_EAX),              \
                           opnd_create_reg(DR_REG_EBX), opnd_create_reg(DR_REG_ECX), \
                           opnd_create_reg(DR_REG_EDX), opnd_create_reg(DR_REG_EAX), \
                           opnd_create_reg(DR_REG_EBX), opnd_create_reg(DR_REG_ECX), \
                           opnd_create_reg(DR_REG_EDX))
#define INSTR_CREATE_enclu(dc)                                                       \
    instr_create_4dst_4src((dc), OP_enclu, opnd_create_reg(DR_REG_EAX),              \
                           opnd_create_reg(DR_REG_EBX), opnd_create_reg(DR_REG_ECX), \
                           opnd_create_reg(DR_REG_EDX), opnd_create_reg(DR_REG_EAX), \
                           opnd_create_reg(DR_REG_EBX), opnd_create_reg(DR_REG_ECX), \
                           opnd_create_reg(DR_REG_EDX))
#define INSTR_CREATE_enclv(dc)                                                       \
    instr_create_4dst_4src((dc), OP_enclv, opnd_create_reg(DR_REG_EAX),              \
                           opnd_create_reg(DR_REG_EBX), opnd_create_reg(DR_REG_ECX), \
                           opnd_create_reg(DR_REG_EDX), opnd_create_reg(DR_REG_EAX), \
                           opnd_create_reg(DR_REG_EBX), opnd_create_reg(DR_REG_ECX), \
                           opnd_create_reg(DR_REG_EDX))
/** @} */ /* end doxygen group */

/* 3 implicit destinations, 3 implicit sources */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx, automatically
 * supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define INSTR_CREATE_getsec(dc)                                               \
    INSTR_PRED(instr_create_3dst_2src(                                        \
                   (dc), OP_getsec, opnd_create_reg(DR_REG_EAX),              \
                   opnd_create_reg(DR_REG_EBX), opnd_create_reg(DR_REG_ECX),  \
                   opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_EBX)), \
               DR_PRED_COMPLEX)

/* 3 destinations: 2 implicit, 5 implicit sources */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param d The opnd_t explicit destination operand for the instruction.
 */
#define INSTR_CREATE_cmpxchg8b(dc, d)                                                \
    instr_create_3dst_5src((dc), OP_cmpxchg8b, (d), opnd_create_reg(DR_REG_EAX),     \
                           opnd_create_reg(DR_REG_EDX), (d),                         \
                           opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_EDX), \
                           opnd_create_reg(DR_REG_ECX), opnd_create_reg(DR_REG_EBX))

/* 3 implicit destinations, 4 sources: 2 implicit */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and the given
 * explicit operands, automatically supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param i16 The opnd_t explicit second source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()) of OPSZ_2.
 * \param i8 The opnd_t explicit second source operand for the instruction, which
 * must be an immediate integer (opnd_create_immed_int()) of OPSZ_1.
 */
/* XXX: IR ignores non-zero immed for size+disp */
#define INSTR_CREATE_enter(dc, i16, i8)                                            \
    instr_create_3dst_4src((dc), OP_enter, opnd_create_reg(DR_REG_XSP),            \
                           opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,       \
                                                 IF_X64_ELSE(-8, -4), OPSZ_STACK), \
                           opnd_create_reg(DR_REG_XBP), (i16), (i8),               \
                           opnd_create_reg(DR_REG_XSP), opnd_create_reg(DR_REG_XBP))

/* 8 implicit destinations, 2 implicit sources */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx, automatically
 * supplying any implicit operands.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define INSTR_CREATE_popa(dc) instr_create_popa((dc))

/****************************************************************************/

/** @name Nops */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * Convenience routine for nop of certain size.  We choose edi as working register
 * for multibyte nops (seems least likely to impact performance: Microsoft uses it
 * and DR used to steal it).
 * Note that Intel now recommends a different set of multi-byte nops,
 * but we stick with these as our tools (mainly windbg) don't understand
 * the OP_nop_modrm encoding (though should work on PPro+).
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define INSTR_CREATE_nop1byte(dc) INSTR_CREATE_nop(dc)
#define INSTR_CREATE_nop2byte(dc) INSTR_CREATE_nop2byte_reg(dc, DR_REG_XDI)
#define INSTR_CREATE_nop3byte(dc) INSTR_CREATE_nop3byte_reg(dc, DR_REG_XDI)
/** @} */ /* end doxygen group */
/** @name 2-byte reg nops */
/** @{ */ /* doxygen start group; w/ DISTRIBUTE_GROUP_DOC=YES, one comment suffices. */
/**
 * Convenience routine for nop of certain size.
 * Note that Intel now recommends a different set of multi-byte nops,
 * but we stick with these as our tools (mainly windbg) don't understand
 * the OP_nop_modrm encoding (though should work on PPro+).
 * AMD recommends 0x66 0x66 ... 0x90 for older processors.
 * \param drcontext The void * dcontext used to allocate memory for the instr_t.
 * \param reg A reg_id_t (NOT opnd_t) to use as source and destination.
 * For 64-bit mode, use a 64-bit register, but NOT rbp or rsp for the 3-byte form.
 */
static inline instr_t *
INSTR_CREATE_nop2byte_reg(void *drcontext, reg_id_t reg)
{
#ifdef X64
    if (!get_x86_mode(drcontext)) {
        /* 32-bit register target zeroes out the top bits, so we use the Intel
         * and AMD recommended 0x66 0x90 */
        instr_t *in = instr_build_bits(drcontext, OP_nop, 2);
#    ifdef WINDOWS
        /* avoid warning C4100: 'reg' : unreferenced formal parameter */
        UNREFERENCED_PARAMETER(reg);
#    endif
        instr_set_raw_byte(in, 0, 0x66);
        instr_set_raw_byte(in, 1, 0x90);
        instr_set_operands_valid(in, true);
        return in;
    } else {
#endif
        return INSTR_CREATE_mov_st(drcontext, opnd_create_reg(reg), opnd_create_reg(reg));
#ifdef X64
        /* XXX: could have INSTR_CREATE_nop{1,2,3}byte() pick DR_REG_EDI for x86
         * mode, or could call instr_shrink_to_32_bits() here, but we aren't planning
         * to change any of the other regular macros in this file: only those that
         * have completely different forms in the two modes, and we expect caller to
         * shrink to 32 for all INSTR_CREATE_* functions/macros.
         */
    }
#endif
}
/* lea's target is 32-bit but address register is 64: so we eliminate the
 * displacement and put in rex.w
 */
static inline instr_t *
INSTR_CREATE_nop3byte_reg(void *drcontext, reg_id_t reg)
{
#ifdef X64
    if (!get_x86_mode(drcontext)) {
        return INSTR_CREATE_lea(drcontext, opnd_create_reg(reg),
                                OPND_CREATE_MEM_lea(reg, DR_REG_NULL, 0, 0));
    } else {
#endif
        return INSTR_CREATE_lea(drcontext, opnd_create_reg(reg),
                                opnd_create_base_disp_ex(reg, DR_REG_NULL, 0, 0, OPSZ_lea,
                                                         true /*encode 0*/, false,
                                                         false));
#ifdef X64
        /* see note above for whether to auto-shrink */
    }
#endif
}
/** @} */ /* end doxygen group */

#endif /* _DR_IR_MACROS_X86_H_ */
