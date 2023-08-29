/* **********************************************************
 * Copyright (c) 2016 RIVOS, Inc. All rights reserved.
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

#ifndef CODEC_H
#define CODEC_H 1

#include <stdint.h>

#include "decode.h"

/* Interface to the RISC-V instruction codec (encoder/decoder).
 *
 * Note that the encoder does not verify validity of operand values. This is
 * because currently invalid bit encodings are either reserved for future
 * extensions (i.e. C.ADDI or C.SLLI) and execute as NOP or HINT instructions
 * (which might be used as vendor ISA extensions).
 */

#define ENCFAIL (uint)0x0 /* An invalid instruction (a.k.a c.unimp). */

/* List of ISA extensions handled by the codec. */
typedef enum {
    RISCV64_ISA_EXT_RV32A,
    RISCV64_ISA_EXT_RV32C,
    RISCV64_ISA_EXT_RV32D,
    RISCV64_ISA_EXT_RV32F,
    RISCV64_ISA_EXT_RV32H,
    RISCV64_ISA_EXT_RV32I,
    RISCV64_ISA_EXT_RV32M,
    RISCV64_ISA_EXT_RV32Q,
    RISCV64_ISA_EXT_RV32ZBA,
    RISCV64_ISA_EXT_RV32ZBB,
    RISCV64_ISA_EXT_RV32ZBC,
    RISCV64_ISA_EXT_RV32ZBS,
    RISCV64_ISA_EXT_RV64A,
    RISCV64_ISA_EXT_RV64C,
    RISCV64_ISA_EXT_RV64D,
    RISCV64_ISA_EXT_RV64F,
    RISCV64_ISA_EXT_RV64H,
    RISCV64_ISA_EXT_RV64I,
    RISCV64_ISA_EXT_RV64M,
    RISCV64_ISA_EXT_RV64Q,
    RISCV64_ISA_EXT_RV64ZBA,
    RISCV64_ISA_EXT_RV64ZBB,
    RISCV64_ISA_EXT_RVC,
    RISCV64_ISA_EXT_SVINVAL,
    RISCV64_ISA_EXT_SYSTEM,
    RISCV64_ISA_EXT_ZICBOM,
    RISCV64_ISA_EXT_ZICBOP,
    RISCV64_ISA_EXT_ZICBOZ,
    RISCV64_ISA_EXT_ZICSR,
    RISCV64_ISA_EXT_ZIFENCEI,
    RISCV64_ISA_EXT_CNT, /* Keep this last */
} riscv64_isa_ext_t;

/* List of instruction formats handled by the codec.
 *
 * Note that enum names have to match ones defined in the codec.py.
 */
typedef enum {
    /* Uncompressed instruction formats - Chapter 2.2 in the RISC-V Instruction
     * Set Manual Volume I: Unprivileged ISA (ver. 20191213).
     */

    /* R-type format:
     * |31    25|24   20|19   15|14    12|11   7|6      0|
     * | funct7 |  rs2  |  rs1  | funct3 |  rd  | opcode |
     */
    RISCV64_FMT_R = 0,
    /* R4-type format:
     * |31 27|26    25|24   20|19   15|14    12|11   7|6      0|
     * | rs3 | funct2 |  rs2  |  rs1  | funct3 |  rd  | opcode |
     */
    RISCV64_FMT_R4,
    /* I-type format:
     * |31       20|19   15|14    12|11   7|6      0|
     * | imm[11:0] |  rs1  | funct3 |  rd  | opcode |
     */
    RISCV64_FMT_I,
    /* S-type format:
     * |31       25|24   20|19   15|14    12|11       7|6      0|
     * | imm[11:5] |  rs2  |  rs1  | funct3 | imm[4:0] | opcode |
     */
    RISCV64_FMT_S,
    /* B-type format:
     * |  31   |30     25|24   20|19   15|14    12|11     8|   7   |6      0|
     * |imm[12]|imm[10:5]|  rs2  |  rs1  | funct3 |imm[4:1]|imm[11]| opcode |
     */
    RISCV64_FMT_B,
    /* U-type format:
     * |31        12|11   7|6      0|
     * | imm[31:12] |  rd  | opcode |
     */
    RISCV64_FMT_U,
    /* J-type format:
     * |   31    |30       21|   20    |19        12|11   7|6      0|
     * | imm[20] | imm[10:1] | imm[11] | imm[19:12] |  rd  | opcode |
     */
    RISCV64_FMT_J,
    /* Compressed instruction formats - Chapter 16.2 in the RISC-V Instruction
     * Set Manual Volume I: Unprivileged ISA (ver. 20191213).
     * Unlike uncompressed formats, the bit layout of immediate fields (imm,
     * offset) depends on the instruction.
     */

    /* Compressed Register (CR) format:
     * |15    12|11     7|6   2|1      0|
     * | funct4 | rd/rs1 | rs2 | opcode |
     */
    RISCV64_FMT_CR,
    /* Compressed Immediate (CI) format:
     * |15    13|  12 |11     7|6   2|1      0|
     * | funct3 | imm | rd/rs1 | imm | opcode |
     */
    RISCV64_FMT_CI,
    /* Compressed Stack-relative Store (CSS) format:
     * |15    13|12  7|6   2|1      0|
     * | funct3 | imm | rs2 | opcode |
     */
    RISCV64_FMT_CSS,
    /* Compressed Wide Immediate (CIW) format:
     * |15    13|12  5|4   2|1      0|
     * | funct3 | imm | rd' | opcode |
     */
    RISCV64_FMT_CIW,
    /* Compressed Load (CL) format:
     * |15    13|12 10|9    7|6   5|4   2|1      0|
     * | funct3 | imm | rs1' | imm | rd' | opcode |
     */
    RISCV64_FMT_CL,
    /* Compressed Store (CS) format:
     * |15    13|12 10|9    7|6   5|4    2|1      0|
     * | funct3 | imm | rs1' | imm | rs2' | opcode |
     */
    RISCV64_FMT_CS,
    /* Compressed Arithmetic (CA) format:
     * |15    10|9        7|6      5|4    2|1      0|
     * | funct6 | rd'/rs1' | funct2 | rs2' | opcode |
     */
    RISCV64_FMT_CA,
    /* Compressed Branch (CB) format:
     * |15    13|12    10|9    7|6      2|1      0|
     * | funct3 | offset | rs1' | offset | opcode |
     */
    RISCV64_FMT_CB,
    /* Compressed Jump (CJ) format:
     * |15    13|12          2|1      0|
     * | funct3 | jump target | opcode |
     */
    RISCV64_FMT_CJ,
    RISCV64_FMT_CNT, /* Keep this last */
} riscv64_inst_fmt_t;

/* List of instruction fields handled by the codec.
 *
 * Note that enum names have to match ones defined in the codec.py.
 */
typedef enum {
    RISCV64_FLD_NONE = 0, /*< Value indicating lack of a given field. */
    /* Uncompressed instruction fields */
    RISCV64_FLD_RD,
    RISCV64_FLD_RDFP,
    RISCV64_FLD_RS1,
    RISCV64_FLD_RS1FP,
    RISCV64_FLD_BASE,
    RISCV64_FLD_RS2,
    RISCV64_FLD_RS2FP,
    RISCV64_FLD_RS3FP,
    RISCV64_FLD_FM,
    RISCV64_FLD_PRED,
    RISCV64_FLD_SUCC,
    RISCV64_FLD_AQRL,
    RISCV64_FLD_CSR,
    RISCV64_FLD_RM,
    RISCV64_FLD_SHAMT,
    RISCV64_FLD_SHAMT5,
    RISCV64_FLD_SHAMT6,
    RISCV64_FLD_I_IMM,
    RISCV64_FLD_S_IMM,
    RISCV64_FLD_B_IMM,
    RISCV64_FLD_U_IMM,
    RISCV64_FLD_U_IMMPC,
    RISCV64_FLD_J_IMM,
    /* Compressed instruction fields */
    RISCV64_FLD_CRD,
    RISCV64_FLD_CRDFP,
    RISCV64_FLD_CRS1,
    RISCV64_FLD_CRS2,
    RISCV64_FLD_CRS2FP,
    RISCV64_FLD_CRD_,
    RISCV64_FLD_CRD_FP,
    RISCV64_FLD_CRS1_,
    RISCV64_FLD_CRS2_,
    RISCV64_FLD_CRS2_FP,
    RISCV64_FLD_CRD__,
    RISCV64_FLD_CSHAMT,
    RISCV64_FLD_CSR_IMM,
    RISCV64_FLD_CADDI16SP_IMM,
    RISCV64_FLD_CLWSP_IMM,
    RISCV64_FLD_CLDSP_IMM,
    RISCV64_FLD_CLUI_IMM,
    RISCV64_FLD_CSWSP_IMM,
    RISCV64_FLD_CSDSP_IMM,
    RISCV64_FLD_CIW_IMM,
    RISCV64_FLD_CLW_IMM,
    RISCV64_FLD_CLD_IMM,
    RISCV64_FLD_CSW_IMM,
    RISCV64_FLD_CSD_IMM,
    RISCV64_FLD_CIMM5,
    RISCV64_FLD_CB_IMM,
    RISCV64_FLD_CJ_IMM,
    /* Virtual fields - en/decode special cases, i.e. base+disp combination */
    RISCV64_FLD_V_L_RS1_DISP,
    RISCV64_FLD_V_S_RS1_DISP,
    RISCV64_FLD_CNT, /* Keep this last */
} riscv64_fld_t;

#define BIT(v, b) (((v) >> b) & 1)
#define GET_FIELD(v, high, low) (((v) >> low) & ((1ULL << (high - low + 1)) - 1))
#define SET_FIELD(v, high, low) (((v) & ((1ULL << (high - low + 1)) - 1)) << low)
#define SIGN_EXTEND(val, val_sz) (((int32_t)(val) << (32 - (val_sz))) >> (32 - (val_sz)))

/* Calculate instruction width.
 *
 * Returns a negative number on an invalid instruction width.
 */
static inline int
instruction_width(uint16_t lower16b)
{
    /*    xxxxxxxxxxxxxxaa -> 16-bit (aa != 11) */
    if (!TESTALL(0b11, GET_FIELD(lower16b, 1, 0)))
        return 2;
    /* ...xxxxxxxxxxxbbb11 -> 32-bit (bbb != 111) */
    else if (!TESTALL(0b111, GET_FIELD(lower16b, 4, 2)))
        return 4;
    /* ...xxxxxxxxxx011111 -> 48-bit */
    else if (TESTALL(0b011111, GET_FIELD(lower16b, 5, 0)))
        return 6;
    /* ...xxxxxxxxx0111111 -> 64-bit */
    else if (TESTALL(0b0111111, GET_FIELD(lower16b, 6, 0)))
        return 8;
    /* ...xnnnxxxxx1111111 -> nnn != 0b111 */
    else if (TESTALL(0b1111111, GET_FIELD(lower16b, 6, 0)) &&
             !TESTALL(0b111, GET_FIELD(lower16b, 14, 12)))
        return 80 + 16 * GET_FIELD(lower16b, 14, 12);
    else
        return 0;
}

/* Return instr_info_t for a given opcode. */
instr_info_t *
get_instruction_info(uint opc);

byte *
decode_common(dcontext_t *dc, byte *pc, byte *orig_pc, instr_t *instr);
uint
encode_common(byte *pc, instr_t *i, decode_info_t *di);

#endif /* CODEC_H */
