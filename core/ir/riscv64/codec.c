/* **********************************************************
 * Copyright (c) 2022 Rivos, Inc.  All rights reserved.
 * **********************************************************/

/* Redistribution and use in source and binary forms, with or without
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
#include "trie.h"

/* RISC-V extended instruction information structure.
 *
 * Holds extra elements required for encoding/decoding. Since instr_info_t is
 * 48 bytes large, there are 16 bytes available to a single cache-line (assuming
 * 64 byte lines).
 */
typedef struct {
    /* The instruction information contains:
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
    instr_info_t info;
    riscv64_isa_ext_t ext; /* ISA or extension of this instruction. */
} rv_instr_info_t;

#if !defined(X64)
#    error RISC-V codec only supports 64-bit architectures mask+match -> code.
#endif

/* Instruction operand decoder function.
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

#define INFO_NDST(opcode) GET_FIELD((opcode), 31, 31);
#define INFO_NSRC(opcode) GET_FIELD((opcode), 30, 28);

/*
 * End of helper functions.
 **********************************************************/

/* Include a generated list of rv_instr_info_t and an accompanying trie
 * structure for fast lookup:
 * static rv_instr_info_t instr_infos[];
 * static trie_node_t instr_infos_trie[];
 */
#include "instr_info_trie.h"

/**********************************************************
 * Format decoding functions.
 */

/* Dummy function for catching invalid operand values. Should never be called.
 */
static bool
decode_none_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                 int idx, instr_t *out)
{
    ASSERT_NOT_REACHED();
    return false;
}

/* Decode the destination fixed-point register field:
 * |31 12|11   7|6      0|
 * | ... |  rd  | opcode |
 *        ^----^
 * Applies to R, R4, I, U and J uncompressed formats.
 */
static bool
decode_rd_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc, int idx,
               instr_t *out)
{
    reg_t rd = DR_REG_X0 + GET_FIELD(inst, 11, 7);
    opnd_t opnd = opnd_create_reg(rd);
    instr_set_dst(out, idx, opnd);
    return true;
}

/* Decode the destination floating-point register field:
 * |31 12|11   7|6      0|
 * | ... |  rd  | opcode |
 *        ^----^
 * Applies to R, R4, I, U and J uncompressed formats.
 */
static bool
decode_rdfp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                 int idx, instr_t *out)
{
    reg_t reg = DR_REG_F0 + GET_FIELD(inst, 11, 7);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_dst(out, idx, opnd);
    return true;
}

/* Decode the 1st source fixed-point register field:
 * |31 20|19   15|14  7|6      0|
 * | ... |  rs1  | ... | opcode |
 *        ^-----^
 * Applies to R, R4, I, S and B uncompressed formats.
 */
static bool
decode_rs1_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                int idx, instr_t *out)
{
    reg_t reg = DR_REG_X0 + GET_FIELD(inst, 19, 15);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the 1st source floating-point register field:
 * |31 20|19   15|14  7|6      0|
 * | ... |  rs1  | ... | opcode |
 *        ^-----^
 * Applies to R, R4, I, S and B uncompressed formats.
 */
static bool
decode_rs1fp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    reg_t reg = DR_REG_F0 + GET_FIELD(inst, 19, 15);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the rs1 field as a base register:
 * |31 20|19    15|14  7|6      0|
 * | ... |  base  | ... | opcode |
 *        ^------^
 * Applies to instructions of the Zicbom and Zicbop extensions.
 */
static bool
decode_base_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                 int idx, instr_t *out)
{
    reg_t reg = DR_REG_X0 + GET_FIELD(inst, 19, 15);
    opnd_t opnd = opnd_create_base_disp(reg, DR_REG_NULL, 0, 0, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the 2nd source fixed-point register field:
 * |31 25|24   20|19  7|6      0|
 * | ... |  rs2  | ... | opcode |
 *        ^-----^
 * Applies to R, R4, S and B uncompressed formats.
 */
static bool
decode_rs2_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                int idx, instr_t *out)
{
    reg_t reg = DR_REG_X0 + GET_FIELD(inst, 24, 20);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the 2nd source floating-point register field:
 * |31 25|24   20|19  7|6      0|
 * | ... |  rs2  | ... | opcode |
 *        ^-----^
 * Applies to R, R4, S and B uncompressed formats.
 */
static bool
decode_rs2fp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    reg_t reg = DR_REG_F0 + GET_FIELD(inst, 24, 20);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the 3rd source fixed-point register field:
 * |31 27|26  7|6      0|
 * | rs3 | ... | opcode |
 *  ^---^
 * Applies to the R4 uncompressed format.
 */
static bool
decode_rs3fp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    reg_t reg = DR_REG_F0 + GET_FIELD(inst, 31, 27);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the fence mode field of the "fence" instruction:
 * |31  28| 27 | 26 | 25 | 24 | 23 | 22 | 21 | 20 |19 15|14    12|11 7|6   0|
 * |  fm  | PI | PO | PR | PW | SI | SO | SR | SW | rs1 | funct3 | rd | 0xF |
 *  ^----^
 */
static bool
decode_fm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc, int idx,
               instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 31, 28);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode all predecessor bits of the "fence" instruction:
 * |31  28| 27 | 26 | 25 | 24 | 23 | 22 | 21 | 20 |19 15|14    12|11 7|6   0|
 * |  fm  | PI | PO | PR | PW | SI | SO | SR | SW | rs1 | funct3 | rd | 0xF |
 *         ^-----------------^
 */
static bool
decode_pred_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                 int idx, instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 27, 24);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode all successor bits of the "fence" instruction:
 * |31  28| 27 | 26 | 25 | 24 | 23 | 22 | 21 | 20 |19 15|14    12|11 7|6   0|
 * |  fm  | PI | PO | PR | PW | SI | SO | SR | SW | rs1 | funct3 | rd | 0xF |
 *                             ^-----------------^
 */
static bool
decode_succ_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                 int idx, instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 23, 20);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode acquire-release semantics of an atomic instruction (A extension):
 * |31 27| 26 | 25 |24  7|6      0|
 * | ... | aq | rl | ... | opcode |
 *        ^-------^
 */
static bool
decode_aqrl_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                 int idx, instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 26, 25);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the CSR number in instructions from the Zicsr extension:
 * |31 20|19  7|6      0|
 * | csr | ... | opcode |
 *  ^---^
 */
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

/* Decode the rounding mode in floating-point instructions:
 * |31 15|14  12|11  7|6      0|
 * | ... |  rm  | ... | opcode |
 *        ^----^
 * The valid values can be found in Table 11.1 in the RISC-V
 * Instruction Set Manual Volume I: Unprivileged ISA (ver. 20191213).
 */
static bool
decode_rm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc, int idx,
               instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 14, 12);
    /* Invalid. Reserved for future use. */
    ASSERT(imm != 0b101 && imm != 0b110);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the 6-bit (6th bit always 0 in rv32) shift amount:
 * |31 26|25   20|19  7|6      0|
 * | ... | shamt | ... | opcode |
 *        ^-----^
 */
static bool
decode_shamt_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 25, 20);
    opnd_t opnd =
        opnd_add_flags(opnd_create_immed_int(imm, op_sz), DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the 5-bit shift amount in rv64:
 * |31 25|24    20|19  7|6      0|
 * | ... | shamt5 | ... | opcode |
 *        ^------^
 */
static bool
decode_shamt5_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                   int idx, instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 24, 20);
    opnd_t opnd =
        opnd_add_flags(opnd_create_immed_int(imm, op_sz), DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the 7-bit (7th bit always 0 in rv64) shift amount in rv64:
 * |31 27|26    20|19  7|6      0|
 * | ... | shamt6 | ... | opcode |
 *        ^------^
 */
static bool
decode_shamt6_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                   int idx, instr_t *out)
{
    /* shamt6 >= 64 only makes sense on RV128 but let user take care of it. */
    int32_t imm = GET_FIELD(inst, 26, 20);
    opnd_t opnd =
        opnd_add_flags(opnd_create_immed_int(imm, op_sz), DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the immediate field of the I-type format:
 * |31       20|19   15|14    12|11   7|6      0|
 * | imm[11:0] |  rs1  | funct3 |  rd  | opcode |
 *  ^---------^
 * Into:
 * |31       11|10        0|
 * |  imm[11]  | imm[10:0] |
 */
static bool
decode_i_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    int32_t imm = SIGN_EXTEND(GET_FIELD(inst, 31, 20), 12);
    opnd_t opnd =
        opnd_add_flags(opnd_create_immed_int(imm, op_sz), DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the immediate field of the S-type format:
 * |31       25|24   20|19   15|14    12|11       7|6      0|
 * | imm[11:5] |  rs2  |  rs1  | funct3 | imm[4:0] | opcode |
 *  ^---------^                          ^--------^
 * Into:
 * |31       11|10        5|4        0|
 * |  imm[11]  | imm[10:5] | imm[4:0] |
 */
static bool
decode_s_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    int32_t imm = (GET_FIELD(inst, 31, 25) << 5) | (GET_FIELD(inst, 11, 7));
    imm = SIGN_EXTEND(imm, 12);
    opnd_t opnd =
        opnd_add_flags(opnd_create_immed_int(imm, op_sz), DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the immediate field of the B-type format as a pc-relative offset:
 * |  31   |30     25|24   20|19   15|14    12|11     8|   7   |6      0|
 * |imm[12]|imm[10:5]|  rs2  |  rs1  | funct3 |imm[4:1]|imm[11]| opcode |
 *  ^---------------^                          ^--------------^
 * Into:
 * |31       12|  11   |10        5|4        1| 0 |
 * |  imm[12]  |imm[11]| imm[10:5] | imm[4:1] | 0 |
 */
static bool
decode_b_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    int32_t imm = BIT(inst, 31) << 12;
    imm |= BIT(inst, 7) << 11;
    imm |= GET_FIELD(inst, 30, 25) << 5;
    imm |= GET_FIELD(inst, 11, 8) << 1;
    imm = SIGN_EXTEND(imm, 13);

    opnd_t opnd = opnd_create_pc(orig_pc + imm);
    instr_set_target(out, opnd);
    return true;
}

/* Decode the immediate field of the U-type format:
 * |31        12|11   7|6      0|
 * | imm[31:12] |  rd  | opcode |
 *  ^----------^
 * Into:
 * |31        12|11  0|
 * | imm[31:12] |  0  |
 */
static bool
decode_u_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    uint uimm = GET_FIELD(inst, 31, 12);
    opnd_t opnd = opnd_create_immed_int(uimm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the immediate field of the U-type format (PC-relative):
 * |31        12|11   7|6      0|
 * | imm[31:12] |  rd  | opcode |
 *  ^----------^
 * Into:
 * |31        12|11  0|
 * | imm[31:12] |  0  |
 */
static bool
decode_u_immpc_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                    int idx, instr_t *out)
{
    uint uimm = GET_FIELD(inst, 31, 12);
    opnd_t opnd = opnd_create_pc(orig_pc + (uimm << 12));
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the immediate field of the J-type format as a pc-relative offset:
 * |   31    |30       21|   20    |19        12|11   7|6      0|
 * | imm[20] | imm[10:1] | imm[11] | imm[19:12] |  rd  | opcode |
 *  ^------------------------------------------^
 * Into:
 * |31     20|19        12|   11    |10        1| 0 |
 * | imm[20] | imm[19:12] | imm[11] | imm[10:1] | 0 |
 */
static bool
decode_j_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    int32_t imm = BIT(inst, 31) << 20;
    imm |= GET_FIELD(inst, 19, 12) << 12;
    imm |= BIT(inst, 20) << 11;
    imm |= GET_FIELD(inst, 30, 21) << 1;
    imm = SIGN_EXTEND(imm, 21);

    opnd_t opnd = opnd_create_pc(orig_pc + imm);
    instr_set_target(out, opnd);
    return true;
}

/* Decode the destination fixed-point register field:
 * |31 12|11   7|6   2|1      0|
 * | ... |  rd  | ... | opcode |
 *        ^----^
 * Applies to CR and CI compressed formats.
 */
static bool
decode_crd_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                int idx, instr_t *out)
{
    reg_t reg = DR_REG_X0 + GET_FIELD(inst, 11, 7);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_dst(out, idx, opnd);
    return true;
}

/* Decode the destination floating-point register field:
 * |31 12|11   7|6   2|1      0|
 * | ... |  rd  | ... | opcode |
 *        ^----^
 * Applies to CR and CI compressed formats.
 */
static bool
decode_crdfp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    reg_t reg = DR_REG_F0 + GET_FIELD(inst, 11, 7);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_dst(out, idx, opnd);
    return true;
}

/* Decode the 1st source fixed-point register field:
 * |31 12|11   7|6   2|1      0|
 * | ... |  rd  | ... | opcode |
 *        ^----^
 * Applies to CR and CI compressed formats.
 */
static bool
decode_crs1_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                 int idx, instr_t *out)
{
    reg_t reg = DR_REG_X0 + GET_FIELD(inst, 11, 7);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the 2nd source fixed-point register field:
 * |31  7|6   2|1      0|
 * | ... | rs2 | opcode |
 *        ^---^
 * Applies to CR and CSS compressed formats.
 */
static bool
decode_crs2_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                 int idx, instr_t *out)
{
    reg_t reg = DR_REG_X0 + GET_FIELD(inst, 6, 2);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the 2nd source floating-point register field:
 * |31  7|6   2|1      0|
 * | ... | rs2 | opcode |
 *        ^---^
 * Applies to CR and CSS compressed formats.
 */
static bool
decode_crs2fp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                   int idx, instr_t *out)
{
    reg_t reg = DR_REG_F0 + GET_FIELD(inst, 6, 2);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the limited range (x8-x15) destination fixed-point register field:
 * |31  5|4   2|1      0|
 * | ... | rd' | opcode |
 *        ^---^
 * Applies to CIW and CL compressed formats.
 */
static bool
decode_crd__opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                 int idx, instr_t *out)
{
    reg_t reg = DR_REG_X8 + GET_FIELD(inst, 4, 2);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_dst(out, idx, opnd);
    return true;
}

/* Decode the limited range (x8-x15) destination floating-point register field:
 * |31  5|4   2|1      0|
 * | ... | rd' | opcode |
 *        ^---^
 * Applies to CIW and CL compressed formats.
 */
static bool
decode_crd_fp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                   int idx, instr_t *out)
{
    reg_t reg = DR_REG_F8 + GET_FIELD(inst, 4, 2);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_dst(out, idx, opnd);
    return true;
}

/* Decode the limited range (x8-x15) 1st source fixed-point register field:
 * |31 10|9    7|6   2|1      0|
 * | ... | rs1' | ... | opcode |
 *        ^---^
 * Applies to CL, CS, CA and CB compressed formats.
 */
static bool
decode_crs1__opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    reg_t reg = DR_REG_X8 + GET_FIELD(inst, 9, 7);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the limited range (x8-x15) 2nd source fixed-point register field:
 * |31  5|4    2|1      0|
 * | ... | rs2' | opcode |
 *        ^---^
 * Applies to CS and CA compressed formats.
 */
static bool
decode_crs2__opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    reg_t reg = DR_REG_X8 + GET_FIELD(inst, 4, 2);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the limited range (x8-x15) 2nd source floating-point register field:
 * |31  5|4    2|1      0|
 * | ... | rs2' | opcode |
 *        ^---^
 * Applies to CS and CA compressed formats.
 */
static bool
decode_crs2_fp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                    int idx, instr_t *out)
{
    reg_t reg = DR_REG_F8 + GET_FIELD(inst, 4, 2);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the limited range (x8-x15) destination fixed-point register field:
 * |31  5|4   2|1      0|
 * | ... | rd' | opcode |
 *        ^---^
 * Applies to the CA compressed format.
 */
static bool
decode_crd___opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    reg_t reg = DR_REG_X8 + GET_FIELD(inst, 9, 7);
    opnd_t opnd = opnd_create_reg(reg);
    instr_set_dst(out, idx, opnd);
    return true;
}

/* Decode the 6-bit (6th bit always 0 in rv32) shift amount:
 * |15    13|   12   |11    10|9    7|6        2|1      0|
 * | funct3 | imm[5] | funct2 | rs1' | imm[4:0] | opcode |
 *           ^------^                 ^--------^
 */
static bool
decode_cshamt_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                   int idx, instr_t *out)
{
    int32_t imm = (BIT(inst, 12) << 5) | GET_FIELD(inst, 6, 2);
    opnd_t opnd =
        opnd_add_flags(opnd_create_immed_int(imm, op_sz), DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the CSR immediate in instructions from the Zicsr extension:
 * |31 20|19      15|14  7|6      0|
 * | csr | imm[4:0] | ... | opcode |
 *        ^--------^
 * Into:
 * |31  5|4        0|
 * |  0  | imm[4:0] |
 */
static bool
decode_csr_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                    int idx, instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 19, 15);
    opnd_t opnd =
        opnd_add_flags(opnd_create_immed_int(imm, op_sz), DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the immediate of the caddi16sp instruction:
 * |15 13|   12   |11  7|6              2|1      0|
 * | ... | imm[9] | ... | imm[4|6|8:7|5] | opcode |
 *        ^------^       ^--------------^
 * Into:
 * |31     9|8        4|3   0|
 * | imm[9] | imm[8:4] |  0  |
 */
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
    opnd_t opnd =
        opnd_add_flags(opnd_create_immed_int(imm, op_sz), DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the SP-based immediate offset of c.lwsp and c.flwsp instructions:
 * |15 13|   12   |11  7|6            2|1      0|
 * | ... | imm[5] | ... | imm[4:2|7:6] | opcode |
 *        ^------^       ^------------^
 * Into:
 *      |31  8|7        2|3   0|
 * sp + |  0  | imm[7:2] |  0  |
 */
static bool
decode_clwsp_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                      int idx, instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 3, 2) << 6;
    imm |= BIT(inst, 12) << 5;
    imm |= GET_FIELD(inst, 6, 4) << 2;
    opnd_t opnd =
        opnd_add_flags(opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, imm, OPSZ_4),
                       DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the SP-based immediate offset of c.ldsp and c.fldsp instructions:
 * |15 13|   12   |11  7|6            2|1      0|
 * | ... | imm[5] | ... | imm[4:3|8:6] | opcode |
 *        ^------^       ^------------^
 * Into:
 *      |31  9|8        2|3   0|
 * sp + |  0  | imm[8:3] |  0  |
 */
static bool
decode_cldsp_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                      int idx, instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 4, 2) << 6;
    imm |= BIT(inst, 12) << 5;
    imm |= GET_FIELD(inst, 6, 5) << 3;
    opnd_t opnd =
        opnd_add_flags(opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, imm, OPSZ_8),
                       DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the immediate of the c.lui instruction:
 * |15 13|   12    |11  7|6          2|1      0|
 * | ... | imm[17] | ... | imm[16:12] | opcode |
 *        ^-------^       ^----------^
 * Into:
 * |31     17|16        12|11  0|
 * | imm[17] | imm[16:12] |  0  |
 */
static bool
decode_clui_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                     int idx, instr_t *out)
{
    int32_t imm = (BIT(inst, 12) << 5) | GET_FIELD(inst, 6, 2);
    opnd_t opnd = opnd_create_immed_int(imm, op_sz);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the SP-based offset immediate of c.swsp and c.fswsp instructions:
 * |15 13|12           7|6   2|1      0|
 * | ... | imm[5:2|7:6] | ... | opcode |
 *        ^------------^
 * Into:
 *      |31  8|7        2|1 0|
 * sp + |  0  | imm[7:2] | 0 |
 */
static bool
decode_cswsp_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                      int idx, instr_t *out)
{
    int32_t imm = (GET_FIELD(inst, 8, 7) << 6) | (GET_FIELD(inst, 12, 9) << 2);
    opnd_t opnd =
        opnd_add_flags(opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, imm, OPSZ_4),
                       DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_dst(out, idx, opnd);
    return true;
}

/* Decode the SP-based offset immediate of c.sdsp and c.fsdsp instructions:
 * |15 13|12           7|6   2|1      0|
 * | ... | imm[5:3|8:6] | ... | opcode |
 *        ^------------^
 * Into:
 *      |31  9|8        3|2 0|
 * sp + |  0  | imm[7:3] | 0 |
 */
static bool
decode_csdsp_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                      int idx, instr_t *out)
{
    int32_t imm = (GET_FIELD(inst, 9, 7) << 6) | (GET_FIELD(inst, 12, 10) << 3);
    opnd_t opnd =
        opnd_add_flags(opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, imm, OPSZ_8),
                       DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_dst(out, idx, opnd);
    return true;
}

/* Decode the immediate of the c.addi4spn instruction:
 * |15 13|12               5|4   2|1      0|
 * | ... | imm[5:4|9:6|2|3] | ... | opcode |
 *        ^----------------^
 * Into:
 * |31 10|9        2|1 0|
 * |  0  | imm[9:2] | 0 |
 */
static bool
decode_ciw_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                    int idx, instr_t *out)
{
    int32_t imm = GET_FIELD(inst, 10, 7) << 6;
    imm |= GET_FIELD(inst, 12, 11) << 4;
    imm |= BIT(inst, 5) << 3;
    imm |= BIT(inst, 6) << 2;
    opnd_t opnd =
        opnd_add_flags(opnd_create_immed_int(imm, op_sz), DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the base register and offset immediate of c.lw and c.flw
 * instructions:
 * |15 13|12      10|9   7|6        5|4   2|1      0|
 * | ... | imm[5:3] | rs1' | imm[2|6] | ... | opcode |
 *        ^--------^ ^----^ ^--------^
 * Into:
 *        |31  7|6        2|1 0|
 * rs1' + |  0  | imm[6:2] | 0 |
 */
static bool
decode_clw_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                    int idx, instr_t *out)
{
    reg_t reg = DR_REG_X8 + GET_FIELD(inst, 9, 7);
    int32_t imm = BIT(inst, 5) << 6;
    imm |= GET_FIELD(inst, 12, 10) << 3;
    imm |= BIT(inst, 6) << 2;
    opnd_t opnd = opnd_add_flags(opnd_create_base_disp(reg, DR_REG_NULL, 0, imm, OPSZ_4),
                                 DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the base register and offset immediate of c.ld and c.fld
 * instructions:
 * |15 13|12      10|9    7|6        5|4   2|1      0|
 * | ... | imm[5:3] | rs1' | imm[7:6] | ... | opcode |
 *        ^--------^ ^----^ ^--------^
 * Into:
 *        |31  8|7        3|2 0|
 * rs1' + |  0  | imm[7:3] | 0 |
 */
static bool
decode_cld_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                    int idx, instr_t *out)
{
    reg_t reg = DR_REG_X8 + GET_FIELD(inst, 9, 7);
    int32_t imm = (GET_FIELD(inst, 6, 5) << 6) | (GET_FIELD(inst, 12, 10) << 3);
    opnd_t opnd = opnd_add_flags(opnd_create_base_disp(reg, DR_REG_NULL, 0, imm, OPSZ_8),
                                 DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the base register and offset immediate of c.sw and c.fsw
 * instructions:
 * |15 13|12      10|9    7|6        5|4   2|1      0|
 * | ... | imm[5:3] | rs1' | imm[2|6] | ... | opcode |
 *        ^--------^ ^----^ ^--------^
 * Into:
 *        |31  7|6        2|1 0|
 * rs1' + |  0  | imm[6:2] | 0 |
 */
static bool
decode_csw_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                    int idx, instr_t *out)
{
    reg_t reg = DR_REG_X8 + GET_FIELD(inst, 9, 7);
    int32_t imm = BIT(inst, 5) << 6;
    imm |= GET_FIELD(inst, 12, 10) << 3;
    imm |= BIT(inst, 6) << 2;
    opnd_t opnd = opnd_add_flags(opnd_create_base_disp(reg, DR_REG_NULL, 0, imm, OPSZ_4),
                                 DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_dst(out, idx, opnd);
    return true;
}

/* Decode the base register and offset immediate of c.sd and c.fsd
 * instructions:
 * |15 13|12      10|9    7|6        5|4   2|1      0|
 * | ... | imm[5:3] | rs1' | imm[7:6] | ... | opcode |
 *        ^--------^ ^----^ ^--------^
 * Into:
 *        |31  8|7        3|2 0|
 * rs1' + |  0  | imm[7:3] | 0 |
 */
static bool
decode_csd_imm_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                    int idx, instr_t *out)
{
    reg_t reg = DR_REG_X8 + GET_FIELD(inst, 9, 7);
    int32_t imm = (GET_FIELD(inst, 6, 5) << 6) | (GET_FIELD(inst, 12, 10) << 3);
    opnd_t opnd = opnd_add_flags(opnd_create_base_disp(reg, DR_REG_NULL, 0, imm, OPSZ_8),
                                 DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_dst(out, idx, opnd);
    return true;
}

/* Decode the base immediate of c.addi, c.addiw, c.li, c.andi instructions:
 * |15 13|   12   |11  7|6        2|1      0|
 * | ... | imm[5] | ... | imm[4:0] | opcode |
 *        ^------^       ^--------^
 * Into:
 * |31     5|4        0|
 * | imm[5] | imm[4:0] |
 */
static bool
decode_cimm5_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc, byte *orig_pc,
                  int idx, instr_t *out)
{
    int32_t imm = SIGN_EXTEND((BIT(inst, 12) << 5) | GET_FIELD(inst, 6, 2), 6);
    opnd_t opnd =
        opnd_add_flags(opnd_create_immed_int(imm, op_sz), DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the immediate field of the CB-type format as a pc-relative offset:
 * |15 13|12        10|9   7|6              2|1      0|
 * | ... | imm[8|4:3] | ... | imm[7:6|2:1|5] | opcode |
 *        ^----------^       ^--------------^
 * Into:
 * |31     8|7        1| 0 |
 * | imm[8] | imm[7:1] | 0 |
 */
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

    opnd_t opnd = opnd_create_pc(orig_pc + imm);
    instr_set_target(out, opnd);
    return true;
}

/* Decode the immediate field of the CJ-type format as a pc-relative offset:
 * |15 13|12                      2|1      0|
 * | ... | [11|4|9:8|10|6|7|3:1|5] | opcode |
 *        ^-----------------------^
 * Into:
 * |31     11|10        1| 0 |
 * | imm[11] | imm[10:1] | 0 |
 */
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

    opnd_t opnd = opnd_create_pc(orig_pc + imm);
    instr_set_target(out, opnd);
    return true;
}

/* Decode the base register and immediate offset of a virtual load-like field:
 * |31       20|19   15|14   7|6      0|
 * | imm[11:0] |  rs1  | ...  | opcode |
 *  ^---------^ ^-----^
 * Into:
 *       |31     11|7         0|
 * rs1 + | imm[11] | imm[10:0] |
 *
 * Note that this is a virtual field injected by codec.py into instructions
 * which share the immediate field type with other non-base+disp instructions.
 */
static bool
decode_v_l_rs1_disp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc,
                         byte *orig_pc, int idx, instr_t *out)
{
    reg_t reg = DR_REG_X0 + GET_FIELD(inst, 19, 15);
    int32_t imm = SIGN_EXTEND(GET_FIELD(inst, 31, 20), 12);
    opnd_t opnd = opnd_add_flags(opnd_create_base_disp(reg, DR_REG_NULL, 0, imm, op_sz),
                                 DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_src(out, idx, opnd);
    return true;
}

/* Decode the base register and immediate offset of a virtual store-like field:
 * |31       25|24   20|19   15|14    12|11       7|6      0|
 * | imm[11:5] |  rs2  |  rs1  | funct3 | imm[4:0] | opcode |
 *  ^---------^         ^-----^          ^--------^
 * Into:
 *       |31     11|7         0|
 * rs1 + | imm[11] | imm[10:0] |
 *
 * Note that this is a virtual field injected by codec.py into instructions
 * which share the immediate field type with other non-base+disp instructions.
 */
static bool
decode_v_s_rs1_disp_opnd(dcontext_t *dc, uint32_t inst, int op_sz, byte *pc,
                         byte *orig_pc, int idx, instr_t *out)
{
    reg_t reg = DR_REG_X0 + GET_FIELD(inst, 19, 15);
    int32_t imm = (GET_FIELD(inst, 31, 25) << 5) | GET_FIELD(inst, 11, 7);
    imm = SIGN_EXTEND(imm, 12);
    opnd_t opnd = opnd_add_flags(opnd_create_base_disp(reg, DR_REG_NULL, 0, imm, op_sz),
                                 DR_OPND_IMM_PRINT_DECIMAL);
    instr_set_dst(out, idx, opnd);
    return true;
}

/* Array of operand decode functions indexed by riscv64_fld_t.
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
    [RISCV64_FLD_RS3FP] = decode_rs3fp_opnd,
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
    [RISCV64_FLD_U_IMMPC] = decode_u_immpc_opnd,
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

/* Decode RVC quadrant 0.
 *
 * The values are derived from table 16.5 in the RISC-V Instruction Set Manual
 * Volume I: Unprivileged ISA (ver. 20191213).
 */
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
    // 4 is reserved.
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

/* Decode RVC quadrant 1.
 *
 * The values are derived from table 16.6 in the RISC-V Instruction Set Manual
 * Volume I: Unprivileged ISA (ver. 20191213).
 */
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

/* Decode RVC quadrant 2
 *
 * The values are derived from table 16.7 in the RISC-V Instruction Set Manual
 * Volume I: Unprivileged ISA (ver. 20191213).
 */
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
    /* 0 is an illegal instruction which is often used as a canary. */
    if (inst == 0)
        return &instr_infos[OP_unimp];

    int op = GET_FIELD(inst, 1, 0);
    int funct = GET_FIELD(inst, 15, 13);
    int bit11to7 = GET_FIELD(inst, 11, 7);
    int funct2 = GET_FIELD(inst, 11, 10);
    int bit12 = BIT(inst, 12);
    int funct3 = GET_FIELD(inst, 6, 5);
    int bit6to2 = GET_FIELD(inst, 6, 2);
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
    mask = GET_FIELD(info->info.code, 31, 0);
    match = GET_FIELD(info->info.code, 63, 32);

    ASSERT_MESSAGE(CHKLVL_DEFAULT, "Malformed matching in RVC", (inst & mask) == match);
    return info;
}

#define OPCODE_FLD_MASK 0x7f

static rv_instr_info_t *
get_rv_instr_info(uint32_t inst, trie_node_t trie[])
{
    rv_instr_info_t *info;
    uint32_t mask, match;
    size_t index;

    /* The initial lookup loop will always index with the OPCODE field so just skip this
     * for faster lookup.
     */
    index = (inst & OPCODE_FLD_MASK) + 1;
    index = trie_lookup(trie, inst, index);

    if (index == TRIE_NODE_EMPTY)
        return NULL;
    info = &instr_infos[index];
    mask = GET_FIELD(info->info.code, 31, 0);
    match = GET_FIELD(info->info.code, 63, 32);
    /* Don't assert, rather allow for an unknown instruction. */
    if ((inst & mask) != match)
        return NULL;
    return info;
}

/*
 * End of format decoding functions.
 **********************************************************/

instr_info_t *
get_instruction_info(uint opc)
{
    if (opc >= BUFFER_SIZE_ELEMENTS(instr_infos))
        return NULL;
    return &instr_infos[opc].info;
}

byte *
decode_common(dcontext_t *dcontext, byte *pc, byte *orig_pc, instr_t *instr)
{
    /* Decode instruction width from the opcode. */
    int width = instruction_width(*(uint16_t *)pc);
    /* Start assuming a compressed instruction. Code memory should be 2b aligned. */
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

    nsrc = INFO_NSRC(info->info.opcode);
    ndst = INFO_NDST(info->info.opcode);
    CLIENT_ASSERT(ndst >= 0 || ndst <= 1, "Invalid number of destination operands.");
    CLIENT_ASSERT(nsrc >= 0 || nsrc <= 4, "Invalid number of source operands.");

    instr_set_opcode(instr, info->info.type);
    instr_set_num_opnds(dcontext, instr, ndst, nsrc);

    CLIENT_ASSERT(info->info.dst1_type < RISCV64_FLD_CNT, "Invalid dst1_type.");
    if (ndst > 0 &&
        !opnd_decoders[info->info.dst1_type](dcontext, inst, info->info.dst1_size, pc,
                                             orig_pc, 0, instr))
        goto decode_failure;
    switch (nsrc) {
    case 4:
        CLIENT_ASSERT(info->info.dst2_type < RISCV64_FLD_CNT, "Invalid dst2_type.");
        if (!opnd_decoders[info->info.dst2_type](dcontext, inst, info->info.dst2_size, pc,
                                                 orig_pc, 3, instr))
            goto decode_failure;
    case 3:
        CLIENT_ASSERT(info->info.src3_type < RISCV64_FLD_CNT, "Invalid src3_type.");
        if (!opnd_decoders[info->info.src3_type](dcontext, inst, info->info.src3_size, pc,
                                                 orig_pc, 2, instr))
            goto decode_failure;
    case 2:
        CLIENT_ASSERT(info->info.src2_type < RISCV64_FLD_CNT, "Invalid src2_type.");
        if (!opnd_decoders[info->info.src2_type](dcontext, inst, info->info.src2_size, pc,
                                                 orig_pc, 1, instr))
            goto decode_failure;
    case 1:
        CLIENT_ASSERT(info->info.src1_type < RISCV64_FLD_CNT, "Invalid src1_type.");
        if (!opnd_decoders[info->info.src1_type](dcontext, inst, info->info.src1_size, pc,
                                                 orig_pc, 0, instr))
            goto decode_failure;
    case 0: break;
    default: ASSERT_NOT_REACHED();
    }

    if (orig_pc != pc) {
        /* We do not want to copy when encoding and condone an invalid
         * relative target.
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

/* Instruction operand encoder function.
 *
 * Encodes an operand from a given instr_t into the instruction.
 */
typedef bool (*opnd_enc_func_t)(instr_t *instr, byte *pc, int idx, uint32_t *out);

/**********************************************************
 * Format encoding functions.
 */

/* Dummy function for catching invalid operand values. Should never be called.
 */
static bool
encode_none_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    ASSERT_NOT_REACHED();
    return false;
}

/* Encode the destination fixed-point register field:
 * |31 12|11   7|6      0|
 * | ... |  rd  | opcode |
 *        ^----^
 * Applies to R, R4, I, U and J uncompressed formats.
 */
static bool
encode_rd_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_dst(instr, idx);
    uint32_t rd = opnd_get_reg(opnd) - DR_REG_X0;
    *out |= SET_FIELD(rd, 11, 7);
    return true;
}

/* Encode the destination floating-point register field:
 * |31 12|11   7|6      0|
 * | ... |  rd  | opcode |
 *        ^----^
 * Applies to R, R4, I, U and J uncompressed formats.
 */
static bool
encode_rdfp_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_dst(instr, idx);
    ASSERT(opnd_get_reg(opnd) >= DR_REG_F0);
    uint32_t rd = opnd_get_reg(opnd) - DR_REG_F0;
    *out |= SET_FIELD(rd, 11, 7);
    return true;
}

/* Encode the 1st source fixed-point register field:
 * |31 20|19   15|14  7|6      0|
 * | ... |  rs1  | ... | opcode |
 *        ^-----^
 * Applies to R, R4, I, S and B uncompressed formats.
 */
static bool
encode_rs1_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    uint32_t rd = opnd_get_reg(opnd) - DR_REG_X0;
    *out |= SET_FIELD(rd, 19, 15);
    return true;
}

/* Encode the 1st source floating-point register field:
 * |31 20|19   15|14  7|6      0|
 * | ... |  rs1  | ... | opcode |
 *        ^-----^
 * Applies to R, R4, I, S and B uncompressed formats.
 */
static bool
encode_rs1fp_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    ASSERT(opnd_get_reg(opnd) >= DR_REG_F0);
    uint32_t rd = opnd_get_reg(opnd) - DR_REG_F0;
    *out |= SET_FIELD(rd, 19, 15);
    return true;
}

/* Encode the rs1 field as a base register:
 * |31 20|19    15|14  7|6      0|
 * | ... |  base  | ... | opcode |
 *        ^------^
 * Applies to instructions of the Zicbom and Zicbop extensions.
 */
static bool
encode_base_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    uint32_t rd = opnd_get_base(opnd) - DR_REG_X0;
    *out |= SET_FIELD(rd, 19, 15);
    return true;
}

/* Encode the 2nd source fixed-point register field:
 * |31 25|24   20|19  7|6      0|
 * | ... |  rs2  | ... | opcode |
 *        ^-----^
 * Applies to R, R4, S and B uncompressed formats.
 */
static bool
encode_rs2_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    uint32_t rd = opnd_get_reg(opnd) - DR_REG_X0;
    *out |= SET_FIELD(rd, 24, 20);
    return true;
}

/* Encode the 2nd source floating-point register field:
 * |31 25|24   20|19  7|6      0|
 * | ... |  rs2  | ... | opcode |
 *        ^-----^
 * Applies to R, R4, S and B uncompressed formats.
 */
static bool
encode_rs2fp_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    ASSERT(opnd_get_reg(opnd) >= DR_REG_F0);
    uint32_t rd = opnd_get_reg(opnd) - DR_REG_F0;
    *out |= SET_FIELD(rd, 24, 20);
    return true;
}

/* Encode the 3rd source fixed-point register field:
 * |31 27|26  7|6      0|
 * | rs3 | ... | opcode |
 *  ^---^
 * Applies to the R4 uncompressed format.
 */
static bool
encode_rs3fp_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    uint32_t rd = opnd_get_reg(opnd) - DR_REG_F0;
    *out |= SET_FIELD(rd, 31, 27);
    return true;
}

/* Encode the fence mode field of the "fence" instruction:
 * |31  28| 27 | 26 | 25 | 24 | 23 | 22 | 21 | 20 |19 15|14    12|11 7|6   0|
 * |  fm  | PI | PO | PR | PW | SI | SO | SR | SW | rs1 | funct3 | rd | 0xF |
 *  ^----^
 */
static bool
encode_fm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_immed_int(opnd);
    *out |= SET_FIELD(imm, 31, 28);
    return true;
}

/* Encode all predecessor bits of the "fence" instruction:
 * |31  28| 27 | 26 | 25 | 24 | 23 | 22 | 21 | 20 |19 15|14    12|11 7|6   0|
 * |  fm  | PI | PO | PR | PW | SI | SO | SR | SW | rs1 | funct3 | rd | 0xF |
 *         ^-----------------^
 */
static bool
encode_pred_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_immed_int(opnd);
    *out |= SET_FIELD(imm, 27, 24);
    return true;
}

/* Encode all successor bits of the "fence" instruction:
 * |31  28| 27 | 26 | 25 | 24 | 23 | 22 | 21 | 20 |19 15|14    12|11 7|6   0|
 * |  fm  | PI | PO | PR | PW | SI | SO | SR | SW | rs1 | funct3 | rd | 0xF |
 *                             ^-----------------^
 */
static bool
encode_succ_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_immed_int(opnd);
    *out |= SET_FIELD(imm, 23, 20);
    return true;
}

/* Encode acquire-release semantics of an atomic instruction (A extension):
 * |31 27| 26 | 25 |24  7|6      0|
 * | ... | aq | rl | ... | opcode |
 *        ^-------^
 */
static bool
encode_aqrl_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_immed_int(opnd);
    *out |= SET_FIELD(imm, 26, 25);
    return true;
}

/* Encode the CSR number in instructions from the Zicsr extension:
 * |31 20|19  7|6      0|
 * | csr | ... | opcode |
 *  ^---^
 */
static bool
encode_csr_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_immed_int(opnd);
    *out |= SET_FIELD(imm, 31, 20);
    return true;
}

/* Encode the rounding mode in floating-point instructions:
 * |31 15|14  12|11  7|6      0|
 * | ... |  rm  | ... | opcode |
 *        ^----^
 * The valid values can be found in Table 11.1 in the RISC-V
 * Instruction Set Manual Volume I: Unprivileged ISA (ver. 20191213).
 */
static bool
encode_rm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_immed_int(opnd);
    /* Invalid. Reserved for future use. */
    ASSERT(imm != 0b101 && imm != 0b110);
    *out |= SET_FIELD(imm, 14, 12);
    return true;
}

/* Encode the 6-bit (6th bit always 0 in rv32) shift amount:
 * |31 26|25   20|19  7|6      0|
 * | ... | shamt | ... | opcode |
 *        ^-----^
 */
static bool
encode_shamt_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_immed_int(opnd);
    *out |= SET_FIELD(imm, 25, 20);
    return true;
}

/* Encode the 5-bit shift amount in rv64:
 * |31 25|24    20|19  7|6      0|
 * | ... | shamt5 | ... | opcode |
 *        ^------^
 */
static bool
encode_shamt5_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_immed_int(opnd);
    *out |= SET_FIELD(imm, 24, 20);
    return true;
}

/* Encode the 7-bit (7th bit always 0 in rv64) shift amount in rv64:
 * |31 27|26    20|19  7|6      0|
 * | ... | shamt6 | ... | opcode |
 *        ^------^
 */
static bool
encode_shamt6_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    /* shamt6 >= 64 only makes sense on RV128 but let user take care of it. */
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_immed_int(opnd);
    *out |= SET_FIELD(imm, 26, 20);
    return true;
}

/* Encode the immediate field of the I-type format:
 * |31       20|19   15|14    12|11   7|6      0|
 * | imm[11:0] |  rs1  | funct3 |  rd  | opcode |
 *  ^---------^
 * From:
 * |31       11|10        0|
 * |  imm[11]  | imm[10:0] |
 */
static bool
encode_i_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_immed_int(opnd);
    *out |= SET_FIELD(imm, 31, 20);
    return true;
}

/* Encode the immediate field of the S-type format:
 * |31       25|24   20|19   15|14    12|11       7|6      0|
 * | imm[11:5] |  rs2  |  rs1  | funct3 | imm[4:0] | opcode |
 *  ^---------^                          ^--------^
 * From:
 * |31       11|10        5|4        0|
 * |  imm[11]  | imm[10:5] | imm[4:0] |
 */
static bool
encode_s_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_immed_int(opnd);
    *out |= SET_FIELD(imm >> 5, 31, 25) | SET_FIELD(imm, 11, 7);
    return true;
}

/* Encode the immediate field of the B-type format as a pc-relative offset:
 * |  31   |30     25|24   20|19   15|14    12|11     8|   7   |6      0|
 * |imm[12]|imm[10:5]|  rs2  |  rs1  | funct3 |imm[4:1]|imm[11]| opcode |
 *  ^---------------^                          ^--------------^
 * From:
 * |31       12|  11   |10        5|4        1| 0 |
 * |  imm[12]  |imm[11]| imm[10:5] | imm[4:1] | 0 |
 */
static bool
encode_b_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_target(instr);
    int32_t imm;
    if (opnd.kind == PC_kind)
        imm = opnd_get_pc(opnd) - pc;
    else if (opnd.kind == INSTR_kind)
        imm = (byte *)opnd_get_instr(opnd)->offset - (byte *)instr->offset;
    else
        return false;

    *out |= SET_FIELD(imm >> 11, 7, 7) | SET_FIELD(imm >> 1, 11, 8) |
        SET_FIELD(imm >> 5, 30, 25) | SET_FIELD(imm >> 12, 31, 31);
    return true;
}

/* Encode the immediate field of the U-type format:
 * |31        12|11   7|6      0|
 * | imm[31:12] |  rd  | opcode |
 *  ^----------^
 * From:
 * |31        12|11  0|
 * | imm[31:12] |  0  |
 */
static bool
encode_u_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    uint32_t imm = opnd_get_immed_int(opnd);
    *out |= SET_FIELD(imm, 31, 12);
    return true;
}

/* Encode the immediate field of the U-type format (PC-relative):
 * |31        12|11   7|6      0|
 * | imm[31:12] |  rd  | opcode |
 *  ^----------^
 * From:
 * |31        12|11  0|
 * | imm[31:12] |  0  |
 */
static bool
encode_u_immpc_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    uint32_t imm;
    if (opnd.kind == PC_kind)
        imm = opnd_get_pc(opnd) - pc;
    else if (opnd.kind == INSTR_kind)
        imm = (byte *)opnd_get_instr(opnd)->offset - (byte *)instr->offset;
    else
        return false;
    /* FIXME i#3544: Add an assertion here to ensure that the lower 12 bits of imm are all
     * 0. Assert only if decode_info_t.check_reachable is true. We should mark it as false
     * to skip the check in get_encoding_info(), as we did for AARCHXX. */
    *out |= SET_FIELD(imm >> 12, 31, 12);
    return true;
}

/* Encode the immediate field of the J-type format as a pc-relative offset:
 * |   31    |30       21|   20    |19        12|11   7|6      0|
 * | imm[20] | imm[10:1] | imm[11] | imm[19:12] |  rd  | opcode |
 *  ^------------------------------------------^
 * From:
 * |31     20|19        12|   11    |10        1| 0 |
 * | imm[20] | imm[19:12] | imm[11] | imm[10:1] | 0 |
 */
static bool
encode_j_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_target(instr);
    int32_t imm;
    if (opnd.kind == PC_kind)
        imm = opnd_get_pc(opnd) - pc;
    else if (opnd.kind == INSTR_kind)
        imm = (byte *)opnd_get_instr(opnd)->offset - (byte *)instr->offset;
    else
        return false;

    *out |= SET_FIELD(imm >> 1, 31, 21) | SET_FIELD(imm >> 11, 20, 20) |
        SET_FIELD(imm >> 12, 19, 12) | SET_FIELD(imm >> 20, 31, 31);
    return true;
}

/* Encode the destination fixed-point register field:
 * |31 12|11   7|6   2|1      0|
 * | ... |  rd  | ... | opcode |
 *        ^----^
 * Applies to CR and CI compressed formats.
 */
static bool
encode_crd_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_dst(instr, idx);
    uint32_t rd = opnd_get_reg(opnd) - DR_REG_X0;
    *out |= SET_FIELD(rd, 11, 7);
    return true;
}

/* Encode the destination floating-point register field:
 * |31 12|11   7|6   2|1      0|
 * | ... |  rd  | ... | opcode |
 *        ^----^
 * Applies to CR and CI compressed formats.
 */
static bool
encode_crdfp_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_dst(instr, idx);
    uint32_t rd = opnd_get_reg(opnd) - DR_REG_F0;
    *out |= SET_FIELD(rd, 11, 7);
    return true;
}

/* Encode the 1st source fixed-point register field:
 * |31 12|11   7|6   2|1      0|
 * | ... |  rd  | ... | opcode |
 *        ^----^
 * Applies to CR and CI compressed formats.
 */
static bool
encode_crs1_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    uint32_t rd = opnd_get_reg(opnd) - DR_REG_X0;
    *out |= SET_FIELD(rd, 11, 7);
    return true;
}

/* Encode the 2nd source fixed-point register field:
 * |31  7|6   2|1      0|
 * | ... | rs2 | opcode |
 *        ^---^
 * Applies to CR and CSS compressed formats.
 */
static bool
encode_crs2_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    uint32_t rd = opnd_get_reg(opnd) - DR_REG_X0;
    *out |= SET_FIELD(rd, 6, 2);
    return true;
}

/* Encode the 2nd source floating-point register field:
 * |31  7|6   2|1      0|
 * | ... | rs2 | opcode |
 *        ^---^
 * Applies to CR and CSS compressed formats.
 */
static bool
encode_crs2fp_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    uint32_t rd = opnd_get_reg(opnd) - DR_REG_F0;
    *out |= SET_FIELD(rd, 6, 2);
    return true;
}

/* Encode the limited range (x8-x15) destination fixed-point register field:
 * |31  5|4   2|1      0|
 * | ... | rd' | opcode |
 *        ^---^
 * Applies to CIW and CL compressed formats.
 */
static bool
encode_crd__opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_dst(instr, idx);
    uint32_t rd = opnd_get_reg(opnd) - DR_REG_X8;
    *out |= SET_FIELD(rd, 4, 2);
    return true;
}

/* Encode the limited range (x8-x15) destination floating-point register field:
 * |31  5|4   2|1      0|
 * | ... | rd' | opcode |
 *        ^---^
 * Applies to CIW and CL compressed formats.
 */
static bool
encode_crd_fp_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_dst(instr, idx);
    uint32_t rd = opnd_get_reg(opnd) - DR_REG_F8;
    *out |= SET_FIELD(rd, 4, 2);
    return true;
}

/* Encode the limited range (x8-x15) 1st source fixed-point register field:
 * |31 10|9    7|6   2|1      0|
 * | ... | rs1' | ... | opcode |
 *        ^---^
 * Applies to CL, CS, CA and CB compressed formats.
 */
static bool
encode_crs1__opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    uint32_t rd = opnd_get_reg(opnd) - DR_REG_X8;
    *out |= SET_FIELD(rd, 9, 7);
    return true;
}

/* Encode the limited range (x8-x15) 2nd source fixed-point register field:
 * |31  5|4    2|1      0|
 * | ... | rs2' | opcode |
 *        ^---^
 * Applies to CS and CA compressed formats.
 */
static bool
encode_crs2__opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    uint32_t rd = opnd_get_reg(opnd) - DR_REG_X8;
    *out |= SET_FIELD(rd, 4, 2);
    return true;
}

/* Encode the limited range (x8-x15) 2nd source floating-point register field:
 * |31  5|4    2|1      0|
 * | ... | rs2' | opcode |
 *        ^---^
 * Applies to CS and CA compressed formats.
 */
static bool
encode_crs2_fp_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    uint32_t rd = opnd_get_reg(opnd) - DR_REG_F8;
    *out |= SET_FIELD(rd, 4, 2);
    return true;
}

/* Encode the limited range (x8-x15) destination fixed-point register field:
 * |31  5|4   2|1      0|
 * | ... | rd' | opcode |
 *        ^---^
 * Applies to the CA compressed format.
 */
static bool
encode_crd___opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_dst(instr, idx);
    uint32_t rd = opnd_get_reg(opnd) - DR_REG_X8;
    *out |= SET_FIELD(rd, 9, 7);
    return true;
}

/* Encode the 6-bit (6th bit always 0 in rv32) shift amount:
 * |15    13|   12   |11    10|9    7|6        2|1      0|
 * | funct3 | imm[5] | funct2 | rs1' | imm[4:0] | opcode |
 *           ^------^                 ^--------^
 */
static bool
encode_cshamt_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_immed_int(opnd);
    *out |= SET_FIELD(imm, 6, 2) | SET_FIELD(imm >> 5, 12, 12);
    return true;
}

/* Encode the CSR immediate in instructions from the Zicsr extension:
 * |31 20|19      15|14  7|6      0|
 * | csr | imm[4:0] | ... | opcode |
 *        ^--------^
 * From:
 * |31  5|4        0|
 * |  0  | imm[4:0] |
 */
static bool
encode_csr_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_immed_int(opnd);
    *out |= SET_FIELD(imm, 19, 15);
    return true;
}

/* Encode the immediate of the caddi16sp instruction:
 * |15 13|   12   |11  7|6              2|1      0|
 * | ... | imm[9] | ... | imm[4|6|8:7|5] | opcode |
 *        ^------^       ^--------------^
 * From:
 * |31     9|8        4|3   0|
 * | imm[9] | imm[8:4] |  0  |
 */
static bool
encode_caddi16sp_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_immed_int(opnd);
    *out |= SET_FIELD(imm >> 5, 2, 2) | SET_FIELD(imm >> 7, 4, 3) |
        SET_FIELD(imm >> 6, 5, 5) | SET_FIELD(imm >> 4, 6, 6) |
        SET_FIELD(imm >> 9, 12, 12);
    return true;
}

/* Encode the SP-based immediate offset of c.lwsp and c.flwsp instructions:
 * |15 13|   12   |11  7|6            2|1      0|
 * | ... | imm[5] | ... | imm[4:2|7:6] | opcode |
 *        ^------^       ^------------^
 * From:
 *      |31  8|7        2|3   0|
 * sp + |  0  | imm[7:2] |  0  |
 */
static bool
encode_clwsp_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_disp(opnd);
    ASSERT(opnd_get_base(opnd) == DR_REG_SP);
    *out |= SET_FIELD(imm >> 6, 3, 2) | SET_FIELD(imm >> 2, 6, 4) |
        SET_FIELD(imm >> 5, 12, 12);
    return true;
}

/* Encode the SP-based immediate offset of c.ldsp and c.fldsp instructions:
 * |15 13|   12   |11  7|6            2|1      0|
 * | ... | imm[5] | ... | imm[4:3|8:6] | opcode |
 *        ^------^       ^------------^
 * From:
 *      |31  9|8        2|3   0|
 * sp + |  0  | imm[8:3] |  0  |
 */
static bool
encode_cldsp_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_disp(opnd);
    ASSERT(opnd_get_base(opnd) == DR_REG_SP);
    *out |= SET_FIELD(imm >> 6, 4, 2) | SET_FIELD(imm >> 3, 6, 5) |
        SET_FIELD(imm >> 5, 12, 12);
    return true;
}

/* Encode the immediate of the c.lui instruction:
 * |15 13|   12    |11  7|6          2|1      0|
 * | ... | imm[17] | ... | imm[16:12] | opcode |
 *        ^-------^       ^----------^
 * From:
 * |31     17|16        12|11  0|
 * | imm[17] | imm[16:12] |  0  |
 */
static bool
encode_clui_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_immed_int(opnd);
    *out |= SET_FIELD(imm, 6, 2) | SET_FIELD(imm >> 5, 12, 12);
    return true;
}

/* Encode the SP-based offset immediate of c.swsp and c.fswsp instructions:
 * |15 13|12           7|6   2|1      0|
 * | ... | imm[5:2|7:6] | ... | opcode |
 *        ^------------^
 * From:
 *      |31  8|7        2|1 0|
 * sp + |  0  | imm[7:2] | 0 |
 */
static bool
encode_cswsp_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_dst(instr, idx);
    int32_t imm = opnd_get_disp(opnd);
    ASSERT(opnd_get_base(opnd) == DR_REG_SP);
    *out |= SET_FIELD(imm >> 6, 8, 7) | SET_FIELD(imm >> 2, 12, 9);
    return true;
}

/* Encode the SP-based offset immediate of c.sdsp and c.fsdsp instructions:
 * |15 13|12           7|6   2|1      0|
 * | ... | imm[5:3|8:6] | ... | opcode |
 *        ^------------^
 * From:
 *      |31  9|8        3|2 0|
 * sp + |  0  | imm[7:3] | 0 |
 */
static bool
encode_csdsp_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_dst(instr, idx);
    int32_t imm = opnd_get_disp(opnd);
    ASSERT(opnd_get_base(opnd) == DR_REG_SP);
    *out |= SET_FIELD(imm >> 6, 9, 7) | SET_FIELD(imm >> 3, 12, 10);
    return true;
}

/* Encode the immediate of the c.addi4spn instruction:
 * |15 13|12               5|4   2|1      0|
 * | ... | imm[5:4|9:6|2|3] | ... | opcode |
 *        ^----------------^
 * From:
 * |31 10|9        2|1 0|
 * |  0  | imm[9:2] | 0 |
 */
static bool
encode_ciw_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_immed_int(opnd);
    *out |= SET_FIELD(imm >> 3, 5, 5) | SET_FIELD(imm >> 2, 6, 6) |
        SET_FIELD(imm >> 6, 10, 7) | SET_FIELD(imm >> 4, 12, 11);
    return true;
}

/* Encode the base register and offset immediate of c.lw and c.flw
 * instructions:
 * |15 13|12      10|9   7|6        5|4   2|1      0|
 * | ... | imm[5:3] | rs1' | imm[2|6] | ... | opcode |
 *        ^--------^ ^----^ ^--------^
 * From:
 *        |31  7|6        2|1 0|
 * rs1' + |  0  | imm[6:2] | 0 |
 */
static bool
encode_clw_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    uint32_t reg = opnd_get_base(opnd) - DR_REG_X8;
    *out |= SET_FIELD(reg, 9, 7);
    int32_t imm = opnd_get_disp(opnd);
    *out |= SET_FIELD(imm >> 6, 5, 5) | SET_FIELD(imm >> 2, 6, 6) |
        SET_FIELD(imm >> 3, 12, 10);
    return true;
}

/* Encode the base register and offset immediate of c.ld and c.fld
 * instructions:
 * |15 13|12      10|9    7|6        5|4   2|1      0|
 * | ... | imm[5:3] | rs1' | imm[7:6] | ... | opcode |
 *        ^--------^ ^----^ ^--------^
 * From:
 *        |31  8|7        3|2 0|
 * rs1' + |  0  | imm[7:3] | 0 |
 */
static bool
encode_cld_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    uint32_t reg = opnd_get_base(opnd) - DR_REG_X8;
    *out |= SET_FIELD(reg, 9, 7);
    int32_t imm = opnd_get_disp(opnd);
    *out |= SET_FIELD(imm >> 6, 6, 5) | SET_FIELD(imm >> 3, 12, 10);
    return true;
}

/* Encode the base register and offset immediate of c.sw and c.fsw
 * instructions:
 * |15 13|12      10|9    7|6        5|4   2|1      0|
 * | ... | imm[5:3] | rs1' | imm[2|6] | ... | opcode |
 *        ^--------^ ^----^ ^--------^
 * From:
 *        |31  7|6        2|1 0|
 * rs1' + |  0  | imm[6:2] | 0 |
 */
static bool
encode_csw_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_dst(instr, idx);
    uint32_t reg = opnd_get_base(opnd) - DR_REG_X8;
    *out |= SET_FIELD(reg, 9, 7);
    int32_t imm = opnd_get_disp(opnd);
    *out |= SET_FIELD(imm >> 6, 5, 5) | SET_FIELD(imm >> 2, 6, 6) |
        SET_FIELD(imm >> 3, 12, 10);
    return true;
}

/* Encode the base register and offset immediate of c.sd and c.fsd
 * instructions:
 * |15 13|12      10|9    7|6        5|4   2|1      0|
 * | ... | imm[5:3] | rs1' | imm[7:6] | ... | opcode |
 *        ^--------^ ^----^ ^--------^
 * From:
 *        |31  8|7        3|2 0|
 * rs1' + |  0  | imm[7:3] | 0 |
 */
static bool
encode_csd_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_dst(instr, idx);
    uint32_t reg = opnd_get_base(opnd) - DR_REG_X8;
    *out |= SET_FIELD(reg, 9, 7);
    int32_t imm = opnd_get_disp(opnd);
    *out |= SET_FIELD(imm >> 6, 6, 5) | SET_FIELD(imm >> 3, 12, 10);
    return true;
}

/* Encode the base immediate of c.addi, c.addiw, c.li, c.andi instructions:
 * |15 13|   12   |11  7|6        2|1      0|
 * | ... | imm[5] | ... | imm[4:0] | opcode |
 *        ^------^       ^--------^
 * From:
 * |31     5|4        0|
 * | imm[5] | imm[4:0] |
 */
static bool
encode_cimm5_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    int32_t imm = opnd_get_immed_int(opnd);
    *out |= SET_FIELD(imm, 6, 2) | SET_FIELD(imm >> 5, 12, 12);
    return true;
}

/* Encode the immediate field of the CB-type format as a pc-relative offset:
 * |15 13|12        10|9   7|6              2|1      0|
 * | ... | imm[8|4:3] | ... | imm[7:6|2:1|5] | opcode |
 *        ^----------^       ^--------------^
 * From:
 * |31     8|7        1| 0 |
 * | imm[8] | imm[7:1] | 0 |
 */
static bool
encode_cb_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_target(instr);

    int32_t imm;
    if (opnd.kind == PC_kind)
        imm = opnd_get_pc(opnd) - pc;
    else if (opnd.kind == INSTR_kind)
        imm = (byte *)opnd_get_instr(opnd)->offset - (byte *)instr->offset;
    else
        return false;

    *out |= SET_FIELD(imm >> 5, 2, 2) | SET_FIELD(imm >> 1, 4, 3) |
        SET_FIELD(imm >> 6, 6, 5) | SET_FIELD(imm >> 3, 11, 10) |
        SET_FIELD(imm >> 8, 12, 12);
    return true;
}

/* Encode the immediate field of the CJ-type format as a pc-relative offset:
 * |15 13|12                      2|1      0|
 * | ... | [11|4|9:8|10|6|7|3:1|5] | opcode |
 *        ^-----------------------^
 * From:
 * |31     11|10        1| 0 |
 * | imm[11] | imm[10:1] | 0 |
 */
static bool
encode_cj_imm_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_target(instr);

    int32_t imm;
    if (opnd.kind == PC_kind)
        imm = opnd_get_pc(opnd) - pc;
    else if (opnd.kind == INSTR_kind)
        imm = (byte *)opnd_get_instr(opnd)->offset - (byte *)instr->offset;
    else
        return false;

    *out |= SET_FIELD(imm >> 5, 2, 2) | SET_FIELD(imm >> 1, 5, 3) |
        SET_FIELD(imm >> 7, 6, 6) | SET_FIELD(imm >> 6, 7, 7) |
        SET_FIELD(imm >> 10, 8, 8) | SET_FIELD(imm >> 8, 10, 9) |
        SET_FIELD(imm >> 4, 11, 11) | SET_FIELD(imm >> 11, 12, 12);
    return true;
}

/* Encode the base register and immediate offset of a virtual load-like field:
 * |31       20|19   15|14   7|6      0|
 * | imm[11:0] |  rs1  | ...  | opcode |
 *  ^---------^ ^-----^
 * From:
 *       |31     11|7         0|
 * rs1 + | imm[11] | imm[10:0] |
 *
 * Note that this is a virtual field injected by codec.py into instructions
 * which share the immediate field type with other non-base+disp instructions.
 */
static bool
encode_v_l_rs1_disp_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_src(instr, idx);
    uint32_t reg = opnd_get_base(opnd) - DR_REG_X0;
    *out |= SET_FIELD(reg, 19, 15);
    int32_t imm = opnd_get_disp(opnd);
    *out |= SET_FIELD(imm, 31, 20);
    return true;
}

/* Encode the base register and immediate offset of a virtual store-like field:
 * |31       25|24   20|19   15|14    12|11       7|6      0|
 * | imm[11:5] |  rs2  |  rs1  | funct3 | imm[4:0] | opcode |
 *  ^---------^         ^-----^          ^--------^
 * From:
 *       |31     11|7         0|
 * rs1 + | imm[11] | imm[10:0] |
 *
 * Note that this is a virtual field injected by codec.py into instructions
 * which share the immediate field type with other non-base+disp instructions.
 */
static bool
encode_v_s_rs1_disp_opnd(instr_t *instr, byte *pc, int idx, uint32_t *out)
{
    opnd_t opnd = instr_get_dst(instr, idx);
    uint32_t reg = opnd_get_base(opnd) - DR_REG_X0;
    *out |= SET_FIELD(reg, 19, 15);
    int32_t imm = opnd_get_disp(opnd);
    *out |= SET_FIELD(imm, 11, 7) | SET_FIELD(imm >> 5, 31, 25);
    return true;
}

/* Array of operand encode functions indexed by riscv64_fld_t. */
opnd_enc_func_t opnd_encoders[] = {
    [RISCV64_FLD_NONE] = encode_none_opnd,
    [RISCV64_FLD_RD] = encode_rd_opnd,
    [RISCV64_FLD_RDFP] = encode_rdfp_opnd,
    [RISCV64_FLD_RS1] = encode_rs1_opnd,
    [RISCV64_FLD_RS1FP] = encode_rs1fp_opnd,
    [RISCV64_FLD_BASE] = encode_base_opnd,
    [RISCV64_FLD_RS2] = encode_rs2_opnd,
    [RISCV64_FLD_RS2FP] = encode_rs2fp_opnd,
    [RISCV64_FLD_RS3FP] = encode_rs3fp_opnd,
    [RISCV64_FLD_FM] = encode_fm_opnd,
    [RISCV64_FLD_PRED] = encode_pred_opnd,
    [RISCV64_FLD_SUCC] = encode_succ_opnd,
    [RISCV64_FLD_AQRL] = encode_aqrl_opnd,
    [RISCV64_FLD_CSR] = encode_csr_opnd,
    [RISCV64_FLD_RM] = encode_rm_opnd,
    [RISCV64_FLD_SHAMT] = encode_shamt_opnd,
    [RISCV64_FLD_SHAMT5] = encode_shamt5_opnd,
    [RISCV64_FLD_SHAMT6] = encode_shamt6_opnd,
    [RISCV64_FLD_I_IMM] = encode_i_imm_opnd,
    [RISCV64_FLD_S_IMM] = encode_s_imm_opnd,
    [RISCV64_FLD_B_IMM] = encode_b_imm_opnd,
    [RISCV64_FLD_U_IMM] = encode_u_imm_opnd,
    [RISCV64_FLD_U_IMMPC] = encode_u_immpc_opnd,
    [RISCV64_FLD_J_IMM] = encode_j_imm_opnd,
    [RISCV64_FLD_CRD] = encode_crd_opnd,
    [RISCV64_FLD_CRDFP] = encode_crdfp_opnd,
    [RISCV64_FLD_CRS1] = encode_crs1_opnd,
    [RISCV64_FLD_CRS2] = encode_crs2_opnd,
    [RISCV64_FLD_CRS2FP] = encode_crs2fp_opnd,
    [RISCV64_FLD_CRD_] = encode_crd__opnd,
    [RISCV64_FLD_CRD_FP] = encode_crd_fp_opnd,
    [RISCV64_FLD_CRS1_] = encode_crs1__opnd,
    [RISCV64_FLD_CRS2_] = encode_crs2__opnd,
    [RISCV64_FLD_CRS2_FP] = encode_crs2_fp_opnd,
    [RISCV64_FLD_CRD__] = encode_crd___opnd,
    [RISCV64_FLD_CSHAMT] = encode_cshamt_opnd,
    [RISCV64_FLD_CSR_IMM] = encode_csr_imm_opnd,
    [RISCV64_FLD_CADDI16SP_IMM] = encode_caddi16sp_imm_opnd,
    [RISCV64_FLD_CLWSP_IMM] = encode_clwsp_imm_opnd,
    [RISCV64_FLD_CLDSP_IMM] = encode_cldsp_imm_opnd,
    [RISCV64_FLD_CLUI_IMM] = encode_clui_imm_opnd,
    [RISCV64_FLD_CSWSP_IMM] = encode_cswsp_imm_opnd,
    [RISCV64_FLD_CSDSP_IMM] = encode_csdsp_imm_opnd,
    [RISCV64_FLD_CIW_IMM] = encode_ciw_imm_opnd,
    [RISCV64_FLD_CLW_IMM] = encode_clw_imm_opnd,
    [RISCV64_FLD_CLD_IMM] = encode_cld_imm_opnd,
    [RISCV64_FLD_CSW_IMM] = encode_csw_imm_opnd,
    [RISCV64_FLD_CSD_IMM] = encode_csd_imm_opnd,
    [RISCV64_FLD_CIMM5] = encode_cimm5_opnd,
    [RISCV64_FLD_CB_IMM] = encode_cb_imm_opnd,
    [RISCV64_FLD_CJ_IMM] = encode_cj_imm_opnd,
    [RISCV64_FLD_V_L_RS1_DISP] = encode_v_l_rs1_disp_opnd,
    [RISCV64_FLD_V_S_RS1_DISP] = encode_v_s_rs1_disp_opnd,
};

uint
encode_common(byte *pc, instr_t *instr, decode_info_t *di)
{
    ASSERT(((ptr_int_t)pc & 1) == 0);
    ASSERT(instr->opcode < sizeof(instr_infos) / sizeof(rv_instr_info_t));

    rv_instr_info_t *info = &instr_infos[instr->opcode];
    int ndst = INFO_NDST(info->info.opcode);
    int nsrc = INFO_NSRC(info->info.opcode);
    uint inst = info->info.code >> 32;

    CLIENT_ASSERT(ndst >= 0 || ndst <= 1, "Invalid number of destination operands.");
    CLIENT_ASSERT(nsrc >= 0 || nsrc <= 4, "Invalid number of source operands.");

    CLIENT_ASSERT(info->info.dst1_type < RISCV64_FLD_CNT, "Invalid dst1_type.");
    if (ndst > 0 && !opnd_encoders[info->info.dst1_type](instr, pc, 0, &inst))
        goto encode_failure;
    switch (nsrc) {
    case 4:
        CLIENT_ASSERT(info->info.dst2_type < RISCV64_FLD_CNT, "Invalid dst2_type.");
        if (!opnd_encoders[info->info.dst2_type](instr, pc, 3, &inst))
            goto encode_failure;
    case 3:
        CLIENT_ASSERT(info->info.src3_type < RISCV64_FLD_CNT, "Invalid src3_type.");
        if (!opnd_encoders[info->info.src3_type](instr, pc, 2, &inst))
            goto encode_failure;
    case 2:
        CLIENT_ASSERT(info->info.src2_type < RISCV64_FLD_CNT, "Invalid src2_type.");
        if (!opnd_encoders[info->info.src2_type](instr, pc, 1, &inst))
            goto encode_failure;
    case 1:
        CLIENT_ASSERT(info->info.src1_type < RISCV64_FLD_CNT, "Invalid src1_type.");
        if (!opnd_encoders[info->info.src1_type](instr, pc, 0, &inst))
            goto encode_failure;
    case 0: break;
    default: ASSERT_NOT_REACHED();
    }

    if (info->ext == RISCV64_ISA_EXT_RVC) {
        inst &= 0xFFFF;
    }
    return inst;

encode_failure:
    return ENCFAIL;
}
