/* **********************************************************
 * Copyright (c) 2022 Rivos, Inc.  All rights reserved.
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
 * * Neither the name of Rivos, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL RIVOS, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "../globals.h"
#include "instr.h"

bool
instr_set_isa_mode(instr_t *instr, dr_isa_mode_t mode)
{
    return (mode == DR_ISA_RV64IMAFDC);
}

dr_isa_mode_t
instr_get_isa_mode(instr_t *instr)
{
    return DR_ISA_RV64IMAFDC;
}

int
instr_length_arch(dcontext_t *dcontext, instr_t *instr)
{
    /* FIXME i#3544: Compressed Instructions ISA extension has a shorter instruction
     * length. */
    return RISCV64_INSTR_SIZE;
}

bool
opc_is_not_a_real_memory_load(int opc)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return opc == OP_auipc;
}

uint
instr_branch_type(instr_t *cti_instr)
{
    /* FIXME i#3544: The branch type depends on JAL(R) operands. More details
     * are provided in Table 2.1 of the The RISC-V Instruction Set Manual Volume I:
     * Unprivileged ISA (page 22 in version 20191213). */
    int opcode = instr_get_opcode(cti_instr);
    switch (opcode) {
    case OP_jal: return LINK_DIRECT | LINK_CALL;
    case OP_jalr: return LINK_INDIRECT | LINK_CALL;
    case OP_beq:
    case OP_bne:
    case OP_blt:
    case OP_bltu:
    case OP_bge:
    case OP_bgeu: return LINK_DIRECT | LINK_JMP;
    }
    CLIENT_ASSERT(false, "instr_branch_type: unknown opcode");
    return LINK_INDIRECT;
}

const char *
get_opcode_name(int opc)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return "<opcode>";
}

bool
instr_is_mov(instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
instr_is_call_arch(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_jal || opc == OP_jalr || opc == OP_c_j || opc == OP_c_jal ||
            opc == OP_c_jr || opc == OP_c_jalr);
}

bool
instr_is_call_direct(instr_t *instr)
{
    /* FIXME i#3544: Check if valid */
    int opc = instr_get_opcode(instr);
    return (opc == OP_jal);
}

bool
instr_is_near_call_direct(instr_t *instr)
{
    /* FIXME i#3544: Check if valid */
    int opc = instr_get_opcode(instr);
    return (opc == OP_jal);
}

bool
instr_is_call_indirect(instr_t *instr)
{
    /* FIXME i#3544: Check if valid */
    int opc = instr_get_opcode(instr);
    return (opc == OP_jalr);
}

bool
instr_is_return(instr_t *instr)
{
    /* FIXME i#3544: Check if valid */
    int opc = instr_get_opcode(instr);
    if (instr_num_srcs(instr) < 1 || instr_num_dsts(instr) < 1)
        return false;
    opnd_t rd = instr_get_dst(instr, 0);
    opnd_t rs = instr_get_src(instr, 0);
    return (opc == OP_jalr && opnd_get_reg(rd) == DR_REG_X0 &&
            opnd_is_memory_reference(rs) && opnd_get_base(rs) == DR_REG_X1 &&
            opnd_get_disp(rs) == 0);
}

bool
instr_is_cbr_arch(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return opc == OP_beq || opc == OP_bne || opc == OP_blt || opc == OP_bltu ||
        opc == OP_bge || opc == OP_bgeu || opc == OP_c_beqz || opc == OP_c_bnez;
}

bool
instr_is_mbr_arch(instr_t *instr)
{
    int opc = instr->opcode; /* caller ensures opcode is valid */
    switch (opc) {
    case OP_jalr:
    case OP_c_jalr:
    case OP_c_jr: return true;
    default: return false;
    }
}

bool
instr_is_far_cti(instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
instr_is_ubr_arch(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return opc == OP_jal || opc == OP_jalr;
}

bool
instr_is_near_ubr(instr_t *instr)
{
    return instr_is_ubr(instr);
}

bool
instr_is_cti_short(instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
instr_is_cti_loop(instr_t *instr)
{
    return false;
}

bool
instr_is_cti_short_rewrite(instr_t *instr, byte *pc)
{
    return false;
}

bool
instr_is_interrupt(instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
instr_is_syscall(instr_t *instr)
{
    /* FIXME i#3544: Check if valid */
    int opc = instr_get_opcode(instr);
    return (opc == OP_ecall);
}

bool
instr_is_mov_constant(instr_t *instr, ptr_int_t *value)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
instr_is_prefetch(instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
instr_is_string_op(instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
instr_is_rep_string_op(instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
instr_saves_float_pc(instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
instr_is_undefined(instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

void
instr_invert_cbr(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    CLIENT_ASSERT(instr_is_cbr(instr), "instr_invert_cbr: instr not a cbr");
    if (opc == OP_beq) {
        instr_set_opcode(instr, OP_bne);
    } else if (opc == OP_bne) {
        instr_set_opcode(instr, OP_beq);
    } else if (opc == OP_blt) {
        instr_set_opcode(instr, OP_bge);
    } else if (opc == OP_bltu) {
        instr_set_opcode(instr, OP_bgeu);
    } else if (opc == OP_bge) {
        instr_set_opcode(instr, OP_blt);
    } else if (opc == OP_bgeu) {
        instr_set_opcode(instr, OP_bltu);
    } else {
        CLIENT_ASSERT(false, "instr_invert_cbr: unknown opcode");
    }
}

bool
instr_cbr_taken(instr_t *instr, priv_mcontext_t *mc, bool pre)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
instr_predicate_reads_srcs(dr_pred_type_t pred)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
instr_predicate_writes_eflags(dr_pred_type_t pred)
{
    return false;
}

bool
instr_predicate_is_cond(dr_pred_type_t pred)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
reg_is_gpr(reg_id_t reg)
{
    return (reg >= DR_REG_START_GPR && reg <= DR_REG_STOP_GPR);
}

bool
reg_is_simd(reg_id_t reg)
{
    return false;
}

bool
reg_is_vector_simd(reg_id_t reg)
{
    return false;
}

bool
reg_is_opmask(reg_id_t reg)
{
    return false;
}

bool
reg_is_bnd(reg_id_t reg)
{
    return false;
}

bool
reg_is_strictly_zmm(reg_id_t reg)
{
    return false;
}

bool
reg_is_ymm(reg_id_t reg)
{
    return false;
}

bool
reg_is_strictly_ymm(reg_id_t reg)
{
    return false;
}

bool
reg_is_xmm(reg_id_t reg)
{
    return false;
}

bool
reg_is_strictly_xmm(reg_id_t reg)
{
    return false;
}

bool
reg_is_mmx(reg_id_t reg)
{
    return false;
}

bool
instr_is_opmask(instr_t *instr)
{
    return false;
}

bool
reg_is_fp(reg_id_t reg)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
instr_is_nop(instr_t *instr)
{
    uint opc = instr_get_opcode(instr);
    if (instr_num_srcs(instr) < 2 || instr_num_dsts(instr) < 1)
        return false;
    opnd_t rd = instr_get_dst(instr, 0);
    opnd_t rs = instr_get_src(instr, 0);
    opnd_t i = instr_get_src(instr, 1);
    return (opc == OP_addi && opnd_get_reg(rd) == DR_REG_X0 &&
            opnd_get_reg(rs) == DR_REG_X0 && opnd_get_immed_int(i) == 0);
}

bool
opnd_same_sizes_ok(opnd_size_t s1, opnd_size_t s2, bool is_reg)
{
    return (s1 == s2);
}

instr_t *
instr_create_nbyte_nop(dcontext_t *dcontext, uint num_bytes, bool raw)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

DR_API
bool
instr_is_exclusive_load(instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

DR_API
bool
instr_is_exclusive_store(instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

DR_API
bool
instr_is_scatter(instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

DR_API
bool
instr_is_gather(instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
instr_is_jump_mem(instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}
