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
    int opcode = instr_get_opcode(instr);
    switch (opcode) {
    case OP_LABEL: return 0;
    case OP_c_flwsp:
    case OP_c_fswsp:
    case OP_c_flw:
    case OP_c_fsw:
    case OP_c_jal:
    case OP_c_ldsp:
    case OP_c_sdsp:
    case OP_c_ld:
    case OP_c_sd:
    case OP_c_addiw:
    case OP_c_addw:
    case OP_c_subw:
    case OP_c_lwsp:
    case OP_c_fldsp:
    case OP_c_swsp:
    case OP_c_fsdsp:
    case OP_c_lw:
    case OP_c_fld:
    case OP_c_sw:
    case OP_c_fsd:
    case OP_c_j:
    case OP_c_jr:
    case OP_c_jalr:
    case OP_c_beqz:
    case OP_c_bnez:
    case OP_c_li:
    case OP_c_lui:
    case OP_c_addi:
    case OP_c_addi16sp:
    case OP_c_addi4spn:
    case OP_c_slli:
    case OP_c_srli:
    case OP_c_srai:
    case OP_c_andi:
    case OP_c_mv:
    case OP_c_add:
    case OP_c_and:
    case OP_c_or:
    case OP_c_xor:
    case OP_c_sub:
    case OP_c_nop:
    case OP_c_ebreak: return RISCV64_INSTR_COMPRESSED_SIZE;
    default: return RISCV64_INSTR_SIZE;
    }
}

bool
opc_is_not_a_real_memory_load(int opc)
{
    return opc == OP_auipc;
}

uint
instr_branch_type(instr_t *cti_instr)
{
    int opcode = instr_get_opcode(cti_instr);
    switch (opcode) {
    case OP_jal:
    case OP_c_jal: /* C.JAL expands to JAL ra, offset, which is direct call. */
    case OP_c_j:   /* C.J expands to JAL zero, offset, which is a direct jump. */
        /* JAL non-zero, offset is a dierct call. */
        if (opnd_get_reg(instr_get_dst(cti_instr, 0)) != DR_REG_ZERO)
            return LINK_DIRECT | LINK_CALL;
        else
            return LINK_DIRECT | LINK_JMP;
    case OP_jalr:
    case OP_c_jr:   /* C.JR expands to JALR zero, 0(rs1). */
    case OP_c_jalr: /* C.JALR expands to JALR ra, 0(rs1) */
        /* JALR zero, 0(ra) is a return. */
        if (opnd_get_reg(instr_get_dst(cti_instr, 0)) == DR_REG_ZERO &&
            opnd_get_reg(instr_get_src(cti_instr, 0)) == DR_REG_RA &&
            opnd_get_immed_int(instr_get_src(cti_instr, 1)) == 0)
            return LINK_INDIRECT | LINK_RETURN;
        /* JALR non-zero, offset(rs1) is an indirect call. */
        else if (opnd_get_reg(instr_get_dst(cti_instr, 0)) != DR_REG_ZERO)
            return LINK_INDIRECT | LINK_CALL;
        else
            return LINK_INDIRECT | LINK_JMP;
    case OP_beq:
    case OP_bne:
    case OP_blt:
    case OP_bltu:
    case OP_bge:
    case OP_bgeu:
    case OP_c_beqz:
    case OP_c_bnez: return LINK_DIRECT | LINK_JMP;
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
    return ((opc == OP_jal || opc == OP_jalr || opc == OP_c_jal || opc == OP_c_jalr) &&
            opnd_get_reg(instr_get_dst(instr, 0)) != DR_REG_ZERO);
}

bool
instr_is_call_direct(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return ((opc == OP_jal || opc == OP_c_jal) &&
            opnd_get_reg(instr_get_dst(instr, 0)) != DR_REG_ZERO);
}

bool
instr_is_near_call_direct(instr_t *instr)
{
    return instr_is_call_direct(instr);
}

bool
instr_is_call_indirect(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return ((opc == OP_jalr || opc == OP_c_jalr) &&
            opnd_get_reg(instr_get_dst(instr, 0)) != DR_REG_ZERO);
}

bool
instr_is_return(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return ((opc == OP_c_jr || opc == OP_jalr) &&
            opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_X0 &&
            opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_RA &&
            opnd_get_immed_int(instr_get_src(instr, 1)) == 0);
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
    int opc = instr_get_opcode(instr);
    return (opc == OP_jalr || opc == OP_c_jr || opc == OP_c_jalr);
}

bool
instr_is_far_cti(instr_t *instr)
{
    return false;
}

bool
instr_is_ubr_arch(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return ((opc == OP_jal || opc == OP_c_j) &&
            opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_ZERO);
}

bool
instr_is_near_ubr(instr_t *instr)
{
    return instr_is_ubr(instr);
}

bool
instr_is_cti_short(instr_t *instr)
{
    /* The branch with smallest reach is direct branch, with range +/- 4 KiB.
     * We have restricted MAX_FRAGMENT_SIZE on RISCV64 accordingly.
     */
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
    int opc = instr_get_opcode(instr);
    return (opc == OP_ecall);
}

bool
instr_is_syscall(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_ecall);
}

bool
instr_is_mov_constant(instr_t *instr, ptr_int_t *value)
{
    uint opc = instr_get_opcode(instr);

    if (opc == OP_addi || opc == OP_addiw || opc == OP_c_addi || opc == OP_c_addiw ||
        opc == OP_c_addi4spn || opc == OP_c_addi16sp) {
        opnd_t op1 = instr_get_src(instr, 0);
        opnd_t op2 = instr_get_src(instr, 1);
        if (opnd_is_reg(op1) && opnd_get_reg(op1) == DR_REG_X0) {
            *value = opnd_get_immed_int(op2);
            return true;
        }
    }
    return false;
}

bool
instr_is_prefetch(instr_t *instr)
{
    switch (instr_get_opcode(instr)) {
    case OP_prefetch_i:
    case OP_prefetch_r:
    case OP_prefetch_w: return true;
    default: return false;
    }
}

bool
instr_is_string_op(instr_t *instr)
{
    return false;
}

bool
instr_is_rep_string_op(instr_t *instr)
{
    return false;
}

bool
instr_saves_float_pc(instr_t *instr)
{
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
    switch (opc) {
    case OP_beq: instr_set_opcode(instr, OP_bne); break;
    case OP_bne: instr_set_opcode(instr, OP_beq); break;
    case OP_blt: instr_set_opcode(instr, OP_bge); break;
    case OP_bltu: instr_set_opcode(instr, OP_bgeu); break;
    case OP_bge: instr_set_opcode(instr, OP_blt); break;
    case OP_bgeu: instr_set_opcode(instr, OP_bltu); break;
    case OP_c_beqz: instr_set_opcode(instr, OP_c_bnez); break;
    case OP_c_bnez: instr_set_opcode(instr, OP_c_beqz); break;
    default: CLIENT_ASSERT(false, "instr_invert_cbr: unknown opcode");
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
    switch (instr_get_opcode(instr)) {
    case OP_lr_w:
    case OP_lr_d: return true;
    }
    return false;
}

DR_API
bool
instr_is_exclusive_store(instr_t *instr)
{
    switch (instr_get_opcode(instr)) {
    case OP_sc_w:
    case OP_sc_d: return true;
    }
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
