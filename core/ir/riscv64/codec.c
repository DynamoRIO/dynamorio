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

#include <stdint.h>

#include "../globals.h"
#include "codec.h"

/**
 * RISC-V extended instruction information structure.
 *
 * Holds extra elements required for encoding/decoding. Since instr_info_t is
 * 48 bytes large, there is 16 bytes available to a single cache-line (assuming
 * 64 byte lines).
 */
typedef struct {
    /*
     * The instruction information contains:
     * - OP_* opcode -> type
     * - N(dst) - there can either by 0 or 1 destination -> opcode[31]
     * - N(src) - there can be up to 4 sources -> opcode[30:28]
     * - Operands - Current instruction set allows maximum of 5 operands
     *              (including semantically divided immediate parts). At most one
     *              of those can be a destination register and if there are 5
     *              operands, there is always a destination register. Therefore:
     *   - Destination type (riscv64_fld_t) -> dst1_type
     *   - 1st source operand (riscv64_fld_t) -> src1_type
     *   - 2nd source operand (riscv64_fld_t) -> src2_type
     *   - 3rd source operand (riscv64_fld_t) -> src3_type
     *   - 4th source operand (riscv64_fld_t) -> dst2_type
     * - Match - fixed bits of the instruction -> code[63:32]
     * - Mask - fixed bits mask for encoding validation -> code[31:0]
     */
    instr_info_t nfo;
    riscv64_isa_ext_t ext; /**< ISA or extension of this instruction. */
} rv_instr_info_t;

#if !defined(X64)
#    error RISC-V codec only supports 64-bit architectures (mask+match -> code).
#endif

/**
 * A prefix-tree node.
 */
typedef struct {
    byte mask;      /**< The mask to apply to an instruction after applying shift. */
    byte shift;     /**< The shift to apply to an instruction before applying mask. */
    uint16_t index; /**< The index into the trie table. If mask == 0, index is the index
                        into instr_infos. */
} trie_node_t;

/**
 * Instruction operand decoder function.
 *
 * Decodes an operand from a given instruction into the instr_t structure
 * provided by the caller.
 *
 * @param[in] dc DynamoRIO context.
 * @param[in] inst instruction bytes.
 * @param[in] op_sz Operand size (OP_* enum value).
 * @param[in] pc Program Counter. Effectively pointer to the bytestream.
 * @param[in] orig_pc If the code was translated, the original program counter.
 * @param[in] info Instruction info for decoder use.
 * @param[in] idx Source/destination index.
 * @param[inout] out The instruction structure to decode the operand to.
 *
 * @return True if decoding succeeded, false otherwise. This function will log
 *         the error.
 */
typedef bool (*opnd_dec_func_t)(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc,
                                byte *orig_pc, int idx, instr_t *out);

/**********************************************************
 * Helper functions.
 */

#define ARRAY_SIZE(x) sizeof(x) / sizeof(x[0])
#define BIT(v, b) (((v) >> b) & 1)
#define GET_FIELD(v, high, low) (((v) >> low) & ((1 << (high - low + 1)) - 1))
#define SIGN_EXTEND(val, val_sz) (((int32_t)(val) << (32 - (val_sz))) >> (32 - (val_sz)))

#define INFO_NDST(opcode) GET_FIELD((opcode), 31, 31);
#define INFO_NSRC(opcode) GET_FIELD((opcode), 30, 28);

/*
 * End of helper functions.
 **********************************************************/

/* Include a generated list of rv_instr_info_t and an acompanying trie structure
 * for fast lookup:
 * static rv_instr_info_t instr_infos[];
 * static trie_node_t instr_infos_trie[];
 */
#include "instr_info_trie.h"

/**********************************************************
 * Format decoding functions.
 */

/**
 * Dummy function for catching invalid operand values. Should never be called.
 */
static bool
decode_none_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                 int idx, instr_t *out)
{
    ASSERT_NOT_REACHED();
    return false;
}

static bool
decode_rd_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc, int idx,
               instr_t *out)
{
    reg_t rd = DR_REG_X0 + GET_FIELD(inst, 11, 7);
    opnd_t opnd = opnd_create_reg(rd);
    instr_set_dst(out, idx, opnd);
    return true;
}

static bool
decode_rdfp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                 int idx, instr_t *out)
{
    reg_t reg = DR_REG_F0 + GET_FIELD(inst, 11, 7);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_dst(out, idx, opnd);
    return true;
}

static bool
decode_rs1_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                int idx, instr_t *out)
{
    reg_t reg = DR_REG_X0 + GET_FIELD(inst, 19, 15);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_rs1fp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    reg_t reg = DR_REG_F0 + GET_FIELD(inst, 19, 15);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_base_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                 int idx, instr_t *out)
{
    reg_t reg = DR_REG_X0 + GET_FIELD(inst, 19, 15);
    opnd_t opnd = opnd_create_base_disp(reg, DR_REG_NULL, 0, 0, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_rs2_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                int idx, instr_t *out)
{
    reg_t reg = DR_REG_X0 + GET_FIELD(inst, 24, 20);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_rs2fp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    reg_t reg = DR_REG_F0 + GET_FIELD(inst, 24, 20);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_rs3_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                int idx, instr_t *out)
{
    reg_t reg = DR_REG_F0 + GET_FIELD(inst, 31, 27);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_fm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc, int idx,
               instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 31, 28);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_pred_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                 int idx, instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 27, 24);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_succ_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                 int idx, instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 23, 20);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_aqrl_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                 int idx, instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 26, 25);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_csr_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                int idx, instr_t *out)
{
    /* FIXME i#3544: Should CSRs be as DR_REG_* or rather as hex defines? Their
     * set is extensible by platform implementers and various extensions, so
     * for now let's leave it as an int.
     */
    int32_t imm = GET_FIELD(inst, 31, 20);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_rm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc, int idx,
               instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 14, 12);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_shamt_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 25, 20);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_shamt5_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                   int idx, instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 24, 20);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_shamt6_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                   int idx, instr_t *out)
{
    /* shamt6 >= 64 only makes sense on RV128 but let user take care of it. */
    int32_t imm = GET_FIELD(inst, 26, 20);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_i_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    int32_t imm = SIGN_EXTEND(GET_FIELD(inst, 31, 20), 12);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_s_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    int32_t imm = (GET_FIELD(inst, 31, 25) << 5) | (GET_FIELD(inst, 11, 7));
    imm = SIGN_EXTEND(imm, 12);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_b_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    int32_t imm = BIT(inst, 31) << 12;
    imm |= BIT(inst, 7) << 11;
    imm |= GET_FIELD(inst, 30, 25) << 5;
    imm |= GET_FIELD(inst, 11, 8) << 1;
    imm = SIGN_EXTEND(imm, 13);
    /* FIXME i#3544: Should PC-relative jumps targets be encoded as mem_instr or
     * rather rel_addr?
     */
    opnd_t opnd = opnd_create_mem_instr(out, imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_u_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    uint uimm = GET_FIELD(inst, 31, 12);
    opnd_t opnd = opnd_create_immed_int(uimm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_j_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    int32_t imm = BIT(inst, 31) << 20;
    imm |= GET_FIELD(inst, 19, 12) << 12;
    imm |= BIT(inst, 20) << 11;
    imm |= GET_FIELD(inst, 30, 21) << 1;
    imm = SIGN_EXTEND(imm, 21);
    /* FIXME i#3544: Should PC-relative jumps targets be encoded as mem_instr or
     * rather rel_addr?
     */
    opnd_t opnd = opnd_create_mem_instr(out, imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_crd_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                int idx, instr_t *out)
{
    reg_t reg = DR_REG_X0 + GET_FIELD(inst, 11, 7);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_dst(out, idx, opnd);
    return true;
}

static bool
decode_crdfp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    reg_t reg = DR_REG_F0 + GET_FIELD(inst, 11, 7);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_dst(out, idx, opnd);
    return true;
}

static bool
decode_crs1_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                 int idx, instr_t *out)
{
    reg_t reg = DR_REG_X0 + GET_FIELD(inst, 11, 7);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_crs2_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                 int idx, instr_t *out)
{
    reg_t reg = DR_REG_X0 + GET_FIELD(inst, 6, 2);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_crs2fp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                   int idx, instr_t *out)
{
    reg_t reg = DR_REG_F0 + GET_FIELD(inst, 6, 2);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_crd__opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                 int idx, instr_t *out)
{
    reg_t reg = DR_REG_X8 + GET_FIELD(inst, 4, 2);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_dst(out, idx, opnd);
    return true;
}

static bool
decode_crd_fp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                   int idx, instr_t *out)
{
    reg_t reg = DR_REG_F8 + GET_FIELD(inst, 4, 2);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_dst(out, idx, opnd);
    return true;
}

static bool
decode_crs1__opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    reg_t reg = DR_REG_X8 + GET_FIELD(inst, 9, 7);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_crs2__opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    reg_t reg = DR_REG_X8 + GET_FIELD(inst, 4, 2);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_crs2_fp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                    int idx, instr_t *out)
{
    reg_t reg = DR_REG_F8 + GET_FIELD(inst, 4, 2);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_crd___opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    reg_t reg = DR_REG_X8 + GET_FIELD(inst, 9, 7);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_dst(out, idx, opnd);
    return true;
}

static bool
decode_cshamt_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                   int idx, instr_t *out)
{
    int32_t imm = (BIT(inst, 12) << 5) | GET_FIELD(inst, 6, 2);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_csr_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                    int idx, instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 19, 15);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_caddi16sp_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc,
                          byte *orig_pc, int idx, instr_t *out)
{
    int32_t imm = (BIT(inst, 12) << 9);
    imm |= (GET_FIELD(inst, 4, 3) << 7);
    imm |= (BIT(inst, 5) << 6);
    imm |= (BIT(inst, 2) << 5);
    imm |= (BIT(inst, 6) << 4);
    imm = SIGN_EXTEND(imm, 10);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_clwsp_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                      int idx, instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 3, 2) << 6;
    imm |= BIT(inst, 12) << 5;
    imm |= GET_FIELD(inst, 6, 4) << 2;
    opnd_t opnd = opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, imm, OPSZ_4);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_cldsp_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                      int idx, instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 4, 2) << 6;
    imm |= BIT(inst, 12) << 5;
    imm |= GET_FIELD(inst, 6, 5) << 3;
    opnd_t opnd = opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, imm, OPSZ_8);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_clui_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                     int idx, instr_t *out)
{
    int32_t imm = (BIT(inst, 12) << 5) | GET_FIELD(inst, 6, 2);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_cswsp_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                      int idx, instr_t *out)
{
    int32_t imm = (GET_FIELD(inst, 8, 7) << 6) | (GET_FIELD(inst, 12, 9) << 2);
    opnd_t opnd = opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, imm, OPSZ_4);
    instr_set_dst(out, idx, opnd);
    return true;
}

static bool
decode_csdsp_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                      int idx, instr_t *out)
{
    int32_t imm = (GET_FIELD(inst, 9, 7) << 6) | (GET_FIELD(inst, 12, 10) << 3);
    opnd_t opnd = opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, imm, OPSZ_8);
    instr_set_dst(out, idx, opnd);
    return true;
}

static bool
decode_ciw_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                    int idx, instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 10, 7) << 6;
    imm |= GET_FIELD(inst, 12, 11) << 4;
    imm |= BIT(inst, 5) << 3;
    imm |= BIT(inst, 6) << 2;
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_clw_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                    int idx, instr_t *out)
{
    reg_t reg = DR_REG_X8 + GET_FIELD(inst, 9, 7);
    int32_t imm = BIT(inst, 5) << 6;
    imm |= GET_FIELD(inst, 12, 10) << 3;
    imm |= BIT(inst, 6) << 2;
    opnd_t opnd = opnd_create_base_disp(reg, DR_REG_NULL, 0, imm, OPSZ_4);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_cld_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                    int idx, instr_t *out)
{
    reg_t reg = DR_REG_X8 + GET_FIELD(inst, 9, 7);
    int32_t imm = (GET_FIELD(inst, 6, 5) << 6) | (GET_FIELD(inst, 12, 10) << 3);
    opnd_t opnd = opnd_create_base_disp(reg, DR_REG_NULL, 0, imm, OPSZ_8);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_csw_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                    int idx, instr_t *out)
{
    reg_t reg = DR_REG_X8 + GET_FIELD(inst, 9, 7);
    int32_t imm = BIT(inst, 5) << 6;
    imm |= GET_FIELD(inst, 12, 10) << 3;
    imm |= BIT(inst, 6) << 2;
    opnd_t opnd = opnd_create_base_disp(reg, DR_REG_NULL, 0, imm, OPSZ_4);
    instr_set_dst(out, idx, opnd);
    return true;
}

static bool
decode_csd_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                    int idx, instr_t *out)
{
    reg_t reg = DR_REG_X8 + GET_FIELD(inst, 9, 7);
    int32_t imm = (GET_FIELD(inst, 6, 5) << 6) | (GET_FIELD(inst, 12, 10) << 3);
    opnd_t opnd = opnd_create_base_disp(reg, DR_REG_NULL, 0, imm, OPSZ_8);
    instr_set_dst(out, idx, opnd);
    return true;
}

static bool
decode_cimm5_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    int32_t imm = SIGN_EXTEND((BIT(inst, 12) << 5) | GET_FIELD(inst, 6, 2), 6);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_cb_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                   int idx, instr_t *out)
{
    int32_t imm = BIT(inst, 12) << 8;
    imm |= GET_FIELD(inst, 6, 5) << 6;
    imm |= BIT(inst, 2) << 5;
    imm |= GET_FIELD(inst, 11, 10) << 3;
    imm |= GET_FIELD(inst, 4, 3) << 1;
    imm = SIGN_EXTEND(imm, 9);
    /* FIXME i#3544: Should PC-relative jumps targets be encoded as mem_instr or
     * rather rel_addr?
     */
    opnd_t opnd = opnd_create_mem_instr(out, imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_cj_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                   int idx, instr_t *out)
{
    int32_t imm = BIT(inst, 12) << 11;
    imm |= BIT(inst, 8) << 10;
    imm |= GET_FIELD(inst, 10, 9) << 8;
    imm |= BIT(inst, 6) << 7;
    imm |= BIT(inst, 7) << 6;
    imm |= BIT(inst, 2) << 5;
    imm |= BIT(inst, 11) << 4;
    imm |= GET_FIELD(inst, 5, 3) << 1;
    imm = SIGN_EXTEND(imm, 12);
    /* FIXME i#3544: Should PC-relative jumps targets be encoded as mem_instr or
     * rather rel_addr?
     */
    opnd_t opnd = opnd_create_mem_instr(out, imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_v_l_rs1_disp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc,
                         byte *orig_pc, int idx, instr_t *out)
{
    reg_t reg = DR_REG_X0 + GET_FIELD(inst, 19, 15);
    int32_t imm = SIGN_EXTEND(GET_FIELD(inst, 31, 20), 12);
    opnd_t opnd = opnd_create_base_disp(reg, DR_REG_NULL, 0, imm, OPSZ_8);
    instr_set_src(out, idx, opnd);
    return true;
}

static bool
decode_v_s_rs1_disp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc,
                         byte *orig_pc, int idx, instr_t *out)
{
    reg_t reg = DR_REG_X0 + GET_FIELD(inst, 19, 15);
    int32_t imm = (GET_FIELD(inst, 31, 25) << 5) | GET_FIELD(inst, 11, 7);
    imm = SIGN_EXTEND(imm, 12);
    opnd_t opnd = opnd_create_base_disp(reg, DR_REG_NULL, 0, imm, OPSZ_8);
    instr_set_dst(out, idx, opnd);
    return true;
}
/**
 * Array of operand decode functions indexed by riscv64_fld_t.
 *
 * NOTE: After benchmarking, perhaps this could be placed in the same section as
 *       instr_infos and trie?
 */
opnd_dec_func_t opnd_decoders[] = {
    [RISCV64_FLD_NONE] = decode_none_opnd,
    [RISCV64_FLD_RD] = decode_rd_opnd,
    [RISCV64_FLD_RDFP] = decode_rdfp_opnd,
    [RISCV64_FLD_RS1] = decode_rs1_opnd,
    [RISCV64_FLD_RS1FP] = decode_rs1fp_opnd,
    [RISCV64_FLD_BASE] = decode_base_opnd,
    [RISCV64_FLD_RS2] = decode_rs2_opnd,
    [RISCV64_FLD_RS2FP] = decode_rs2fp_opnd,
    [RISCV64_FLD_RS3] = decode_rs3_opnd,
    [RISCV64_FLD_FM] = decode_fm_opnd,
    [RISCV64_FLD_PRED] = decode_pred_opnd,
    [RISCV64_FLD_SUCC] = decode_succ_opnd,
    [RISCV64_FLD_AQRL] = decode_aqrl_opnd,
    [RISCV64_FLD_CSR] = decode_csr_opnd,
    [RISCV64_FLD_RM] = decode_rm_opnd,
    [RISCV64_FLD_SHAMT] = decode_shamt_opnd,
    [RISCV64_FLD_SHAMT5] = decode_shamt5_opnd,
    [RISCV64_FLD_SHAMT6] = decode_shamt6_opnd,
    [RISCV64_FLD_I_IMM] = decode_i_imm_opnd,
    [RISCV64_FLD_S_IMM] = decode_s_imm_opnd,
    [RISCV64_FLD_B_IMM] = decode_b_imm_opnd,
    [RISCV64_FLD_U_IMM] = decode_u_imm_opnd,
    [RISCV64_FLD_J_IMM] = decode_j_imm_opnd,
    [RISCV64_FLD_CRD] = decode_crd_opnd,
    [RISCV64_FLD_CRDFP] = decode_crdfp_opnd,
    [RISCV64_FLD_CRS1] = decode_crs1_opnd,
    [RISCV64_FLD_CRS2] = decode_crs2_opnd,
    [RISCV64_FLD_CRS2FP] = decode_crs2fp_opnd,
    [RISCV64_FLD_CRD_] = decode_crd__opnd,
    [RISCV64_FLD_CRD_FP] = decode_crd_fp_opnd,
    [RISCV64_FLD_CRS1_] = decode_crs1__opnd,
    [RISCV64_FLD_CRS2_] = decode_crs2__opnd,
    [RISCV64_FLD_CRS2_FP] = decode_crs2_fp_opnd,
    [RISCV64_FLD_CRD__] = decode_crd___opnd,
    [RISCV64_FLD_CSHAMT] = decode_cshamt_opnd,
    [RISCV64_FLD_CSR_IMM] = decode_csr_imm_opnd,
    [RISCV64_FLD_CADDI16SP_IMM] = decode_caddi16sp_imm_opnd,
    [RISCV64_FLD_CLWSP_IMM] = decode_clwsp_imm_opnd,
    [RISCV64_FLD_CLDSP_IMM] = decode_cldsp_imm_opnd,
    [RISCV64_FLD_CLUI_IMM] = decode_clui_imm_opnd,
    [RISCV64_FLD_CSWSP_IMM] = decode_cswsp_imm_opnd,
    [RISCV64_FLD_CSDSP_IMM] = decode_csdsp_imm_opnd,
    [RISCV64_FLD_CIW_IMM] = decode_ciw_imm_opnd,
    [RISCV64_FLD_CLW_IMM] = decode_clw_imm_opnd,
    [RISCV64_FLD_CLD_IMM] = decode_cld_imm_opnd,
    [RISCV64_FLD_CSW_IMM] = decode_csw_imm_opnd,
    [RISCV64_FLD_CSD_IMM] = decode_csd_imm_opnd,
    [RISCV64_FLD_CIMM5] = decode_cimm5_opnd,
    [RISCV64_FLD_CB_IMM] = decode_cb_imm_opnd,
    [RISCV64_FLD_CJ_IMM] = decode_cj_imm_opnd,
    [RISCV64_FLD_V_L_RS1_DISP] = decode_v_l_rs1_disp_opnd,
    [RISCV64_FLD_V_S_RS1_DISP] = decode_v_s_rs1_disp_opnd,
};

static inline rv_instr_info_t *
match_op_0(int funct, bool rv32, bool rv64)
{
    switch (funct) {
    case 0: return &instr_infos[OP_c_addi4spn];
    case 1: return &instr_infos[OP_c_fld];
    case 2: return &instr_infos[OP_c_lw];
    case 3:
        if (rv32)
            return &instr_infos[OP_c_flw];
        if (rv64)
            return &instr_infos[OP_c_ld];
        return NULL;
    // 4 is reserived.
    case 5: return &instr_infos[OP_c_fsd];
    case 6: return &instr_infos[OP_c_sw];
    case 7:
        if (rv32)
            return &instr_infos[OP_c_fsw];
        else if (rv64)
            return &instr_infos[OP_c_sd];
        else
            return NULL;
    default: return NULL;
    }
}

static inline rv_instr_info_t *
match_op_1(int funct, int funct2, int funct3, int bit11to7, int bit12, bool rv32,
           bool rv64)
{
    switch (funct) {
    case 0:
        if (bit11to7 == 0)
            return &instr_infos[OP_c_nop];
        else
            return &instr_infos[OP_c_addi];
    case 1:
        if (rv32)
            return &instr_infos[OP_c_jal];
        else if (rv64)
            return &instr_infos[OP_c_addiw];
        else
            return NULL;
    case 2: return &instr_infos[OP_c_li];
    case 3:
        if (bit11to7 == 2)
            return &instr_infos[OP_c_addi16sp];
        else
            return &instr_infos[OP_c_lui];
    case 4:
        switch (funct2) {
        case 0: return &instr_infos[OP_c_srli];
        case 1: return &instr_infos[OP_c_srai];
        case 2: return &instr_infos[OP_c_andi];
        case 3:
            switch (bit12) {
            case 0:
                switch (funct3) {
                case 0: return &instr_infos[OP_c_sub];
                case 1: return &instr_infos[OP_c_xor];
                case 2: return &instr_infos[OP_c_or];
                case 3: return &instr_infos[OP_c_and];
                default: return NULL;
                }
            case 1:
                switch (funct3) {
                case 0: return &instr_infos[OP_c_subw];
                case 1: return &instr_infos[OP_c_addw];
                // 2 and 3 are reserved.
                default: return NULL;
                }
            default: return NULL;
            }
        default: return NULL;
        }
    case 5: return &instr_infos[OP_c_j];
    case 6: return &instr_infos[OP_c_beqz];
    case 7: return &instr_infos[OP_c_bnez];
    default: return NULL;
    };
}

static inline rv_instr_info_t *
match_op_2(int funct, int bit11to7, int bit6to2, int bit12, bool rv32, bool rv64)
{
    switch (funct) {
    case 0: return &instr_infos[OP_c_slli];
    case 1: return &instr_infos[OP_c_fldsp];
    case 2: return &instr_infos[OP_c_lwsp];
    case 3:
        if (rv32)
            return &instr_infos[OP_c_flwsp];
        else if (rv64)
            return &instr_infos[OP_c_ldsp];
        else
            return NULL;
    case 4:
        switch (bit12) {
        case 0:
            if (bit6to2 == 0)
                return &instr_infos[OP_c_jr];
            else
                return &instr_infos[OP_c_mv];
        case 1:
            if ((bit11to7 == 0) && (bit6to2 == 0))
                return &instr_infos[OP_c_ebreak];
            else if (bit6to2 == 0)
                return &instr_infos[OP_c_jalr];
            else
                return &instr_infos[OP_c_add];
        default: return NULL;
        }
    case 5: return &instr_infos[OP_c_fsdsp];
    case 6: return &instr_infos[OP_c_swsp];
    case 7:
        if (rv32)
            return &instr_infos[OP_c_fswsp];
        else if (rv64)
            return &instr_infos[OP_c_sdsp];
        else
            return NULL;
    default: return NULL;
    }
}

static rv_instr_info_t *
get_rvc_instr_info(uint32_t inst, int xlen)
{
    /* 0 is an illegal instruction which is often used as a canary */
    if (inst == 0)
        return &instr_infos[OP_unimp];

    int op = inst & 0b11;
    int funct = (inst >> 13) & 0b111;
    int bit11to7 = (inst >> 7) & 0b11111;
    int funct2 = (inst >> 10) & 0b11;
    int bit12 = (inst >> 12) & 0b1;
    int funct3 = (inst >> 5) & 0b11;
    int bit6to2 = (inst >> 2) & 0b11111;
    bool rv32 = xlen == 32;
    bool rv64 = xlen == 64;
    rv_instr_info_t *info = NULL;
    uint32_t mask, match;

    switch (op) {
    case 0: info = match_op_0(funct, rv32, rv64); break;
    case 1: info = match_op_1(funct, funct2, funct3, bit11to7, bit12, rv32, rv64); break;
    case 2: info = match_op_2(funct, bit11to7, bit6to2, bit12, rv32, rv64); break;
    }

    if (info == NULL)
        return NULL;
    mask = info->nfo.code & 0xFFFFFFFF;
    match = (info->nfo.code >> 32) & 0xFFFFFFFF;

    if ((inst & mask) != match)
        return NULL;
    return info;
}

static rv_instr_info_t *
get_rv_instr_info(uint32_t inst, trie_node_t trie[])
{
    rv_instr_info_t *info;
    uint32_t mask, match;
    trie_node_t *node;
    size_t index;

    /* We know the first index into trie straight from the instruction */
    index = (inst & 0x7f) + 1;
    while (index < (int16_t)-1) {
        node = &trie[index];
        ASSERT(node != NULL);
        if (node->mask == 0) {
            if (node->index >= ARRAY_SIZE(instr_infos))
                return NULL;
            info = &instr_infos[node->index];
            mask = info->nfo.code & 0xFFFFFFFF;
            match = (info->nfo.code >> 32) & 0xFFFFFFFF;
            if ((inst & mask) != match)
                return NULL;
            return info;
        }
        index = node->index + ((inst >> node->shift) & node->mask);
    }
    return NULL;
}

/*
 * End of format decoding functions.
 **********************************************************/

instr_info_t *
get_instruction_info(uint opc)
{
    if (opc >= ARRAY_SIZE(instr_infos))
        return NULL;
    return &instr_infos[opc].nfo;
}

byte *
decode_common(dcontext_t *dcontext, byte *pc, byte *orig_pc, instr_t *instr)
{
    /* Decode instruction width from the opcode */
    int width = instruction_width(*(uint16_t *)pc);
    /* Start assuming a compressed instructions. Code memory should be 2b aligned. */
    uint32_t inst = *(uint16_t *)pc;
    rv_instr_info_t *info = NULL;
    int nsrc = 0, ndst = 0;
    byte *next_pc;

    CLIENT_ASSERT(instr->opcode == OP_INVALID || instr->opcode == OP_UNDECODED,
                  "decode: instr is already decoded, may need to call instr_reset()");

    switch (width) {
    case 4:
        inst |= *((uint16_t *)pc + 1) << 16;
        info = get_rv_instr_info(inst, instr_infos_trie);
        break;
    case 2: info = get_rvc_instr_info(inst, 64); break;
    default:
        LOG(THREAD, LOG_INTERP, 3, "decode: unhandled instruction width %d at " PFX "\n",
            width, pc);
        CLIENT_ASSERT(false, "decode: invalid instr width");
        return NULL;
    }
    next_pc = pc + width;

    if (info == NULL) {
        LOG(THREAD, LOG_INTERP, 3, "decode: unknown instruction 0x%08x at " PFX "\n",
            inst, pc);
        return NULL;
    }

    nsrc = INFO_NSRC(info->nfo.opcode);
    ndst = INFO_NDST(info->nfo.opcode);
    CLIENT_ASSERT(ndst >= 0 || ndst <= 1, "Invalid number of destination operands.");
    CLIENT_ASSERT(nsrc >= 0 || nsrc <= 4, "Invalid number of source operands.");

    instr_set_opcode(instr, info->nfo.type);
    instr_set_num_opnds(dcontext, instr, ndst, nsrc);

    CLIENT_ASSERT(info->nfo.dst1_type < RISCV64_FLD_CNT, "Invalid dst1_type.");
    if (ndst > 0 &&
        !opnd_decoders[info->nfo.dst1_type](dcontext, inst, info->nfo.dst1_size, pc,
                                            orig_pc, 0, instr))
        goto decode_failure;
    switch (nsrc) {
    case 4:
        CLIENT_ASSERT(info->nfo.dst2_type < RISCV64_FLD_CNT, "Invalid dst2_type.");
        if (!opnd_decoders[info->nfo.dst2_type](dcontext, inst, info->nfo.dst2_size, pc,
                                                orig_pc, 3, instr))
            goto decode_failure;
    case 3:
        CLIENT_ASSERT(info->nfo.src3_type < RISCV64_FLD_CNT, "Invalid src3_type.");
        if (!opnd_decoders[info->nfo.src3_type](dcontext, inst, info->nfo.src3_size, pc,
                                                orig_pc, 2, instr))
            goto decode_failure;
    case 2:
        CLIENT_ASSERT(info->nfo.src2_type < RISCV64_FLD_CNT, "Invalid src2_type.");
        if (!opnd_decoders[info->nfo.src2_type](dcontext, inst, info->nfo.src2_size, pc,
                                                orig_pc, 1, instr))
            goto decode_failure;
    case 1:
        CLIENT_ASSERT(info->nfo.src1_type < RISCV64_FLD_CNT, "Invalid src1_type.");
        if (!opnd_decoders[info->nfo.src1_type](dcontext, inst, info->nfo.src1_size, pc,
                                                orig_pc, 0, instr))
            goto decode_failure;
    case 0: break;
    default: ASSERT_NOT_REACHED();
    }

    if (orig_pc != pc) {
        /* We do not want to copy when encoding and condone an invalid
         * relative target
         * FIXME i#3544: Add re-relativization support without having to re-encode.
         */
        instr_set_raw_bits_valid(instr, false);
        instr_set_translation(instr, orig_pc);
    } else {
        /* We set raw bits AFTER setting all srcs and dsts because setting
         * a src or dst marks instr as having invalid raw bits.
         */
        ASSERT(CHECK_TRUNCATE_TYPE_uint(next_pc - pc));
        instr_set_raw_bits(instr, pc, (uint)(next_pc - pc));
    }

    return next_pc;

decode_failure:
    instr_set_operands_valid(instr, false);
    instr_set_opcode(instr, OP_INVALID);
    return NULL;
}
