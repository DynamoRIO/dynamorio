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

#include "decode.h"

/**
 * @file codec.h
 * @brief Interface to the RISC-V instruction codec (encoder/decoder).
 *
 * Note that encoder does not verify validity of operand values. This is because
 * currently invalid bit encodings are either reserved for future extensions
 * (i.e. C.ADDI or C.SLLI) and execute as NOP or HINT instructions (which might
 * be used as vendor ISA extensions).
 */

/**
 * List of ISA extensions handled by the codec.
 */
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

/**
 * List of instruction formats handled by the codec.
 *
 * Note that enum names have to match ones defined in the codec.py.
 */
typedef enum {
    /* Uncompressed instruction formats */
    RISCV64_FMT_R = 0,
    RISCV64_FMT_R4,
    RISCV64_FMT_I,
    RISCV64_FMT_S,
    RISCV64_FMT_B,
    RISCV64_FMT_U,
    RISCV64_FMT_J,
    /* Compressed instruction formats */
    RISCV64_FMT_CR,
    RISCV64_FMT_CI,
    RISCV64_FMT_CSS,
    RISCV64_FMT_CIW,
    RISCV64_FMT_CL,
    RISCV64_FMT_CS,
    RISCV64_FMT_CA,
    RISCV64_FMT_CB,
    RISCV64_FMT_CJ,
    RISCV64_FMT_CNT, /* Keep this last */
} riscv64_inst_fmt_t;

/**
 * List of instruction fields handled by the codec.
 *
 * Note that enum names have to match ones defined in the codec.py.
 */
typedef enum {
    RISCV64_FLD_NONE = 0, /**< Value indicating lack of a given field. */
    /* Uncompressed instruction fields */
    RISCV64_FLD_RD,
    RISCV64_FLD_RDFP,
    RISCV64_FLD_RS1,
    RISCV64_FLD_RS1FP,
    RISCV64_FLD_BASE,
    RISCV64_FLD_RS2,
    RISCV64_FLD_RS2FP,
    RISCV64_FLD_RS3,
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

/**
 * Calculate instruction width.
 *
 * Returns a negative number on an invalid instruction width.
 */
static inline int
instruction_width(int16_t lower16b)
{
    if ((lower16b & 0b11) < 0b11)
        return 2;
    else if ((lower16b & 0b11100) < 0b11100)
        return 4;
    else if ((lower16b & 0b011111) == 0b011111)
        return 6;
    else if ((lower16b & 0b0111111) == 0b0111111)
        return 8;
    else if ((lower16b & 0b111000001111111) < 0b111000001111111)
        return 80 + 16 * (lower16b >> 12);
    else
        return 0;
}

/**
 * Return instr_info_t for a given opcode.
 */
instr_info_t *
get_instruction_info(uint opc);

byte *
decode_common(dcontext_t *dc, byte *pc, byte *orig_pc, instr_t *instr);

#endif /* CODEC_H */
