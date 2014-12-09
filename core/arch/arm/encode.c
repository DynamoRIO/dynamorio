/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
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

/* ARM encoder */

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "decode.h"
#include "disassemble.h"
#include "decode_private.h"

/* Extra logging for encoding */
#define ENC_LEVEL 6

/* Order corresponds to DR_REG_ enum. */
const char * const reg_names[] = {
    "<NULL>",
    "<invalid>",
    "x0",  "x1",   "x2",   "x3",    "x4",  "x5",   "x6",   "x7",
    "x8",  "x9",   "x10",  "x11",  "x12", "x13",  "x14",  "x15",
    "x16", "x17",  "x18",  "x19",  "x20", "x21",  "x22",  "x23",
    "x24", "x25",  "x26",  "x27",  "x28", "x29",  "lr",   "sp", /* sometimes "xzr" */
    "w0",  "w1",   "w2",   "w3",    "w4",  "w5",   "w6",   "w7",
    "w8",  "w9",   "w10",  "w11",  "w12", "w13",  "w14",  "w15",
    "w16", "w17",  "w18",  "w19",  "w20", "w21",  "w22",  "w23",
    "w24", "w25",  "w26",  "w27",  "w28", "w29",  "w30",  "w31", /* sometimes "wzr" */
#ifndef X64
    "r0",  "r1",   "r2",   "r3",    "r4",  "r5",   "r6",   "r7",
    "r8",  "r9",   "r10",  "r11",  "r12",  "sp",   "lr",   "pc",
#endif
    "q0",  "q1",   "q2",   "q3",    "q4",  "q5",   "q6",   "q7",
    "q8",  "q9",   "q10",  "q11",  "q12", "q13",  "q14",  "q15",
    "q16", "q17",  "q18",  "q19",  "q20", "q21",  "q22",  "q23",
    "q24", "q25",  "q26",  "q27",  "q28", "q29",  "q30",  "q31",
    "d0",  "d1",   "d2",   "d3",   "d4",  "d5",   "d6",   "d7",
    "d8",  "d9",   "d10",  "d11",  "d12", "d13",  "d14",  "d15",
    "d16", "d17",  "d18",  "d19",  "d20", "d21",  "d22",  "d23",
    "d24", "d25",  "d26",  "d27",  "d28", "d29",  "d30",  "d31",
    "s0",  "s1",   "s2",   "s3",   "s4",  "s5",   "s6",   "s7",
    "s8",  "s9",   "s10",  "s11",  "s12", "s13",  "s14",  "s15",
    "s16", "s17",  "s18",  "s19",  "s20", "s21",  "s22",  "s23",
    "s24", "s25",  "s26",  "s27",  "s28", "s29",  "s30",  "s31",
    "h0",  "h1",   "h2",   "h3",   "h4",  "h5",   "h6",   "h7",
    "h8",  "h9",   "h10",  "h11",  "h12", "h13",  "h14",  "h15",
    "h16", "h17",  "h18",  "h19",  "h20", "h21",  "h22",  "h23",
    "h24", "h25",  "h26",  "h27",  "h28", "h29",  "h30",  "h31",
    "b0",  "b1",   "b2",   "b3",   "b4",  "b5",   "b6",   "b7",
    "b8",  "b9",   "b10",  "b11",  "b12", "b13",  "b14",  "b15",
    "b16", "b17",  "b18",  "b19",  "b20", "b21",  "b22",  "b23",
    "b24", "b25",  "b26",  "b27",  "b28", "b29",  "b30",  "b31",
    "cr0", "cr1",  "cr2",  "cr3",   "cr4", "cr5",  "cr6",  "cr7",
    "cr8", "cr9",  "cr10", "cr11", "cr12","cr13", "cr14", "cr15",
    "cpsr", "spsr", "fpscr",
    IF_X64_ELSE("tpidr_el0",   "tpidrurw"),
    IF_X64_ELSE("tpidrro_el0", "tpidruro"),
};

/* Maps sub-registers to their containing register. */
/* Order corresponds to DR_REG_ enum. */
const reg_id_t dr_reg_fixer[] = {
    REG_NULL,
    REG_NULL,
    DR_REG_X0,  DR_REG_X1,   DR_REG_X2,   DR_REG_X3,
    DR_REG_X4,  DR_REG_X5,   DR_REG_X6,   DR_REG_X7,
    DR_REG_X8,  DR_REG_X9,   DR_REG_X10,  DR_REG_X11,
    DR_REG_X12, DR_REG_X13,  DR_REG_X14,  DR_REG_X15,
    DR_REG_X16, DR_REG_X17,  DR_REG_X18,  DR_REG_X19,
    DR_REG_X20, DR_REG_X21,  DR_REG_X22,  DR_REG_X23,
    DR_REG_X24, DR_REG_X25,  DR_REG_X26,  DR_REG_X27,
    DR_REG_X28, DR_REG_X29,  DR_REG_X30,  DR_REG_X31,
    DR_REG_X0,  DR_REG_X1,   DR_REG_X2,   DR_REG_X3,
    DR_REG_X4,  DR_REG_X5,   DR_REG_X6,   DR_REG_X7,
    DR_REG_X8,  DR_REG_X9,   DR_REG_X10,  DR_REG_X11,
    DR_REG_X12, DR_REG_X13,  DR_REG_X14,  DR_REG_X15,
    DR_REG_X16, DR_REG_X17,  DR_REG_X18,  DR_REG_X19,
    DR_REG_X20, DR_REG_X21,  DR_REG_X22,  DR_REG_X23,
    DR_REG_X24, DR_REG_X25,  DR_REG_X26,  DR_REG_X27,
    DR_REG_X28, DR_REG_X29,  DR_REG_X30,  DR_REG_X31,
#ifndef X64
    DR_REG_R0,  DR_REG_R1,   DR_REG_R2,   DR_REG_R3,
    DR_REG_R4,  DR_REG_R5,   DR_REG_R6,   DR_REG_R7,
    DR_REG_R8,  DR_REG_R9,   DR_REG_R10,  DR_REG_R11,
    DR_REG_R12, DR_REG_R13,  DR_REG_R14,  DR_REG_R15,
#endif
    /* q0-q31 */
    DR_REG_Q0,  DR_REG_Q1,   DR_REG_Q2,   DR_REG_Q3,
    DR_REG_Q4,  DR_REG_Q5,   DR_REG_Q6,   DR_REG_Q7,
    DR_REG_Q8,  DR_REG_Q9,   DR_REG_Q10,  DR_REG_Q11,
    DR_REG_Q12, DR_REG_Q13,  DR_REG_Q14,  DR_REG_Q15,
    /* x64-only but simpler code to not ifdef it */
    DR_REG_Q16, DR_REG_Q17,  DR_REG_Q18,  DR_REG_Q19,
    DR_REG_Q20, DR_REG_Q21,  DR_REG_Q22,  DR_REG_Q23,
    DR_REG_Q24, DR_REG_Q25,  DR_REG_Q26,  DR_REG_Q27,
    DR_REG_Q28, DR_REG_Q29,  DR_REG_Q30,  DR_REG_Q31,
    /* d0-d31 */
    /* For AArch64, the smaller SIMD names refer to the lower
     * bits of the corresponding same-number larger SIMD register.
     * But for AArch32, the smaller ones are compressed such that
     * they refer to the top and bottom.  B and H are AArch64-only.
     */
#ifdef X64
    DR_REG_Q0,  DR_REG_Q1,   DR_REG_Q2,   DR_REG_Q3,
    DR_REG_Q4,  DR_REG_Q5,   DR_REG_Q6,   DR_REG_Q7,
    DR_REG_Q8,  DR_REG_Q9,   DR_REG_Q10,  DR_REG_Q11,
    DR_REG_Q12, DR_REG_Q13,  DR_REG_Q14,  DR_REG_Q15,
    DR_REG_Q16, DR_REG_Q17,  DR_REG_Q18,  DR_REG_Q19,
    DR_REG_Q20, DR_REG_Q21,  DR_REG_Q22,  DR_REG_Q23,
    DR_REG_Q24, DR_REG_Q25,  DR_REG_Q26,  DR_REG_Q27,
    DR_REG_Q28, DR_REG_Q29,  DR_REG_Q30,  DR_REG_Q31,
#else
    DR_REG_Q0,  DR_REG_Q0,   DR_REG_Q1,   DR_REG_Q1,
    DR_REG_Q2,  DR_REG_Q2,   DR_REG_Q3,   DR_REG_Q3,
    DR_REG_Q4,  DR_REG_Q4,   DR_REG_Q5,   DR_REG_Q5,
    DR_REG_Q6,  DR_REG_Q6,   DR_REG_Q7,   DR_REG_Q7,
    DR_REG_Q8,  DR_REG_Q8,   DR_REG_Q9,   DR_REG_Q9,
    DR_REG_Q10, DR_REG_Q10,  DR_REG_Q11,  DR_REG_Q11,
    DR_REG_Q12, DR_REG_Q12,  DR_REG_Q13,  DR_REG_Q13,
    DR_REG_Q14, DR_REG_Q14,  DR_REG_Q15,  DR_REG_Q15,
#endif
    /* s0-s31 */
#ifdef X64
    DR_REG_Q0,  DR_REG_Q1,   DR_REG_Q2,   DR_REG_Q3,
    DR_REG_Q4,  DR_REG_Q5,   DR_REG_Q6,   DR_REG_Q7,
    DR_REG_Q8,  DR_REG_Q9,   DR_REG_Q10,  DR_REG_Q11,
    DR_REG_Q12, DR_REG_Q13,  DR_REG_Q14,  DR_REG_Q15,
    DR_REG_Q16, DR_REG_Q17,  DR_REG_Q18,  DR_REG_Q19,
    DR_REG_Q20, DR_REG_Q21,  DR_REG_Q22,  DR_REG_Q23,
    DR_REG_Q24, DR_REG_Q25,  DR_REG_Q26,  DR_REG_Q27,
    DR_REG_Q28, DR_REG_Q29,  DR_REG_Q30,  DR_REG_Q31,
#else
    DR_REG_Q0,  DR_REG_Q0,   DR_REG_Q0,   DR_REG_Q0,
    DR_REG_Q1,  DR_REG_Q1,   DR_REG_Q1,   DR_REG_Q1,
    DR_REG_Q2,  DR_REG_Q2,   DR_REG_Q2,   DR_REG_Q2,
    DR_REG_Q3,  DR_REG_Q3,   DR_REG_Q3,   DR_REG_Q3,
    DR_REG_Q4,  DR_REG_Q4,   DR_REG_Q4,   DR_REG_Q4,
    DR_REG_Q5,  DR_REG_Q5,   DR_REG_Q5,   DR_REG_Q5,
    DR_REG_Q6,  DR_REG_Q6,   DR_REG_Q6,   DR_REG_Q6,
    DR_REG_Q7,  DR_REG_Q7,   DR_REG_Q7,   DR_REG_Q7,
#endif
    /* h0-h31: AArch64-only */
    DR_REG_Q0,  DR_REG_Q1,   DR_REG_Q2,   DR_REG_Q3,
    DR_REG_Q4,  DR_REG_Q5,   DR_REG_Q6,   DR_REG_Q7,
    DR_REG_Q8,  DR_REG_Q9,   DR_REG_Q10,  DR_REG_Q11,
    DR_REG_Q12, DR_REG_Q13,  DR_REG_Q14,  DR_REG_Q15,
#ifdef X64
    DR_REG_Q16, DR_REG_Q17,  DR_REG_Q18,  DR_REG_Q19,
    DR_REG_Q20, DR_REG_Q21,  DR_REG_Q22,  DR_REG_Q23,
    DR_REG_Q24, DR_REG_Q25,  DR_REG_Q26,  DR_REG_Q27,
    DR_REG_Q28, DR_REG_Q29,  DR_REG_Q30,  DR_REG_Q31,
#else
    DR_REG_D16, DR_REG_D17,  DR_REG_D18,  DR_REG_D19,
    DR_REG_D20, DR_REG_D21,  DR_REG_D22,  DR_REG_D23,
    DR_REG_D24, DR_REG_D25,  DR_REG_D26,  DR_REG_D27,
    DR_REG_D28, DR_REG_D29,  DR_REG_D30,  DR_REG_D31,
#endif
    /* b0-b31: AArch64-only */
    DR_REG_Q0,  DR_REG_Q1,   DR_REG_Q2,   DR_REG_Q3,
    DR_REG_Q4,  DR_REG_Q5,   DR_REG_Q6,   DR_REG_Q7,
    DR_REG_Q8,  DR_REG_Q9,   DR_REG_Q10,  DR_REG_Q11,
    DR_REG_Q12, DR_REG_Q13,  DR_REG_Q14,  DR_REG_Q15,
#ifdef X64
    DR_REG_Q16, DR_REG_Q17,  DR_REG_Q18,  DR_REG_Q19,
    DR_REG_Q20, DR_REG_Q21,  DR_REG_Q22,  DR_REG_Q23,
    DR_REG_Q24, DR_REG_Q25,  DR_REG_Q26,  DR_REG_Q27,
    DR_REG_Q28, DR_REG_Q29,  DR_REG_Q30,  DR_REG_Q31,
#else
    DR_REG_D16, DR_REG_D17,  DR_REG_D18,  DR_REG_D19,
    DR_REG_D20, DR_REG_D21,  DR_REG_D22,  DR_REG_D23,
    DR_REG_D24, DR_REG_D25,  DR_REG_D26,  DR_REG_D27,
    DR_REG_D28, DR_REG_D29,  DR_REG_D30,  DR_REG_D31,
#endif
    DR_REG_CR0,  DR_REG_CR1,  DR_REG_CR2,  DR_REG_CR3,
    DR_REG_CR4,  DR_REG_CR5,  DR_REG_CR6,  DR_REG_CR7,
    DR_REG_CR8,  DR_REG_CR9,  DR_REG_CR10, DR_REG_CR11,
    DR_REG_CR12, DR_REG_CR13, DR_REG_CR14, DR_REG_CR15,
    DR_REG_CPSR, DR_REG_SPSR, DR_REG_FPSCR,
    DR_REG_TPIDRURW, DR_REG_TPIDRURO,
};

const char * const type_names[] = {
    "TYPE_NONE",
    "TYPE_R_A",
    "TYPE_R_B",
    "TYPE_R_C",
    "TYPE_R_D",
    "TYPE_R_A_TOP",
    "TYPE_R_B_TOP",
    "TYPE_R_C_TOP",
    "TYPE_R_D_TOP",
    "TYPE_R_D_NEGATED",
    "TYPE_R_B_EVEN",
    "TYPE_R_B_PLUS1",
    "TYPE_R_D_EVEN",
    "TYPE_R_D_PLUS1",
    "TYPE_CR_A",
    "TYPE_CR_B",
    "TYPE_CR_C",
    "TYPE_CR_D",
    "TYPE_V_A",
    "TYPE_V_B",
    "TYPE_V_C",
    "TYPE_V_C_3b",
    "TYPE_V_C_4b",
    "TYPE_W_A",
    "TYPE_W_B",
    "TYPE_W_C",
    "TYPE_W_C_PLUS1",
    "TYPE_SPSR",
    "TYPE_CPSR",
    "TYPE_FPSCR",
    "TYPE_LR",
    "TYPE_SP",
    "TYPE_I_b0",
    "TYPE_NI_b0",
    "TYPE_I_x4_b0",
    "TYPE_I_b3",
    "TYPE_I_b4",
    "TYPE_I_b5",
    "TYPE_I_b6",
    "TYPE_I_b7",
    "TYPE_I_b8",
    "TYPE_I_b9",
    "TYPE_I_b10",
    "TYPE_I_b16",
    "TYPE_I_b17",
    "TYPE_I_b18",
    "TYPE_I_b19",
    "TYPE_I_b20",
    "TYPE_I_b21",
    "TYPE_I_b0_b5",
    "TYPE_I_b0_b24",
    "TYPE_I_b5_b3",
    "TYPE_I_b8_b0",
    "TYPE_NI_b8_b0",
    "TYPE_I_b8_b16",
    "TYPE_I_b16_b0",
    "TYPE_I_b21_b5",
    "TYPE_I_b21_b6",
    "TYPE_I_b24_b16_b0",
    "TYPE_SHIFT_b5",
    "TYPE_SHIFT_b6",
    "TYPE_SHIFT_LSL",
    "TYPE_SHIFT_ASR",
    "TYPE_L_8b",
    "TYPE_L_13b",
    "TYPE_L_16b",
    "TYPE_L_CONSEC",
    "TYPE_L_VBx2",
    "TYPE_L_VBx3",
    "TYPE_L_VBx4",
    "TYPE_L_VBx2D",
    "TYPE_L_VBx3D",
    "TYPE_L_VBx4D",
    "TYPE_L_VAx2",
    "TYPE_L_VAx3",
    "TYPE_L_VAx4",
    "TYPE_M",
    "TYPE_M_POS_REG",
    "TYPE_M_NEG_REG",
    "TYPE_M_POS_SHREG",
    "TYPE_M_NEG_SHREG",
    "TYPE_M_POS_I12",
    "TYPE_M_NEG_I12",
    "TYPE_M_SI9",
    "TYPE_M_POS_I8",
    "TYPE_M_NEG_I8",
    "TYPE_M_POS_I4_4",
    "TYPE_M_NEG_I4_4",
    "TYPE_M_SI7",
    "TYPE_M_POS_I5",
    "TYPE_M_PCREL_S9",
    "TYPE_M_PCREL_U9",
    "TYPE_M_UP_OFFS",
    "TYPE_M_DOWN",
    "TYPE_M_DOWN_OFFS",
    "TYPE_K",
};

#ifdef DEBUG
void
encode_debug_checks(void)
{
    CLIENT_ASSERT(sizeof(dr_reg_fixer)/sizeof(dr_reg_fixer[0]) == REG_LAST_ENUM + 1,
                  "internal register enum error");
    CLIENT_ASSERT(sizeof(reg_names)/sizeof(reg_names[0]) == REG_LAST_ENUM + 1,
                  "reg_names missing an entry");
    CLIENT_ASSERT(sizeof(type_names)/sizeof(type_names[0]) == TYPE_BEYOND_LAST_ENUM,
                  "type_names missing an entry");
}
#endif

opnd_size_t
resolve_size_upward(opnd_size_t size)
{
    switch (size) {
    case OPSZ_1_of_8:
    case OPSZ_2_of_8:
    case OPSZ_4_of_8:
        return OPSZ_8;

    case OPSZ_1_of_16:
    case OPSZ_2_of_16:
    case OPSZ_4_of_16:
    case OPSZ_8_of_16:
    case OPSZ_12_of_16:
    case OPSZ_14_of_16:
    case OPSZ_15_of_16:
        return OPSZ_16;

    case OPSZ_16_of_32:
        return OPSZ_32;
    default:
        return size;
    }
}

opnd_size_t
resolve_size_downward(opnd_size_t size)
{
    switch (size) {
    case OPSZ_1_of_8:
    case OPSZ_1_of_16:
        return OPSZ_1;
    case OPSZ_2_of_8:
    case OPSZ_2_of_16:
        return OPSZ_2;
    case OPSZ_4_of_16:
    case OPSZ_4_of_8:
        return OPSZ_4;
    case OPSZ_8_of_16:
        return OPSZ_8;
    case OPSZ_12_of_16:
        return OPSZ_12;
    case OPSZ_14_of_16:
        return OPSZ_14;
    case OPSZ_15_of_16:
        return OPSZ_15;
    case OPSZ_16_of_32:
        return OPSZ_16;
    default:
        return size;
    }
}

static bool
reg_is_cpreg(reg_id_t reg)
{
    return (reg >= DR_REG_CR0 && reg <= DR_REG_CR15);
}

static reg_id_t
reg_simd_start(reg_id_t reg)
{
    if (reg >= DR_REG_B0 && reg <= DR_REG_B31)
        return DR_REG_B0;
    if (reg >= DR_REG_H0 && reg <= DR_REG_H31)
        return DR_REG_H0;
    if (reg >= DR_REG_S0 && reg <= DR_REG_S31)
        return DR_REG_S0;
    if (reg >= DR_REG_D0 && reg <= DR_REG_D31)
        return DR_REG_D0;
    if (reg >= DR_REG_Q0 && reg <= DR_REG_Q31)
        return DR_REG_Q0;
    CLIENT_ASSERT(false, "internal encoder error: not a simd reg");
    return DR_REG_NULL;
}

static void
encode_index_shift_values(opnd_t memop, ptr_int_t *sh2 OUT, ptr_int_t *val OUT)
{
    uint amount;
    dr_shift_type_t shift = opnd_get_index_shift(memop, &amount);
    if (shift == DR_SHIFT_NONE) {
        CLIENT_ASSERT(amount == 0, "invalid shift amount");
        *sh2 = 0;
        *val = 0;
    } else if (shift == DR_SHIFT_LSL) {
        *sh2 = SHIFT_ENCODING_LSL;
        *val = amount;
    } else if (shift == DR_SHIFT_LSR) {
        *sh2 = SHIFT_ENCODING_LSR;
        *val = (amount == 32) ? 0 : amount;
    } else if (shift == DR_SHIFT_ASR) {
        *sh2 = SHIFT_ENCODING_ASR;
        *val = (amount == 32) ? 0 : amount;
    } else if (shift == DR_SHIFT_RRX) {
        CLIENT_ASSERT(amount == 1, "invalid shift amount");
        *sh2 = SHIFT_ENCODING_RRX;
        *val = 0;
    } else {
        CLIENT_ASSERT(shift == DR_SHIFT_ROR, "invalid shift type");
        CLIENT_ASSERT(amount > 0, "invalid shift amount");
        *sh2 = SHIFT_ENCODING_RRX;
        *val = amount;
    }
}

/* 0 stride means no stride */
static bool
encode_reglist_ok(decode_info_t *di, opnd_size_t size_temp, instr_t *in,
                  bool is_dst, uint *counter INOUT, uint max_num,
                  bool is_simd, uint stride, uint prior)
{
    opnd_size_t size_temp_up = resolve_size_upward(size_temp);
    uint i;
    opnd_t opnd;
    opnd_size_t size_op;
    reg_id_t reg, last_reg = DR_REG_NULL;
    /* Undo what encode_opnd_ok already did */
    (*counter)--;
    /* We rule out more than one reglist per template in decode_debug_checks_arch() */
    di->reglist_start = *counter;
    for (i = 0; i < max_num; i++) {
        uint opnum = *counter;
        if (is_dst) {
            if (opnum >= instr_num_dsts(in))
                break;
            opnd = instr_get_dst(in, opnum);
        } else {
            if (opnum >= instr_num_srcs(in))
                break;
            opnd = instr_get_src(in, opnum);
        }
        size_op = opnd_get_size(opnd);
        if (!opnd_is_reg(opnd))
            break;
        reg = opnd_get_reg(opnd);
        if (i > 0 && stride > 0 && reg != last_reg + stride)
            break;
        if (is_simd ? !reg_is_simd(reg) : !reg_is_gpr(reg))
            break;
        if (!(size_op == size_temp || size_op == size_temp_up))
            break;
        last_reg = reg;
        (*counter)++;
    }
    LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "  reglist_stop: %d\n", *counter);
    di->reglist_stop = *counter;
    /* Due to possible rollback of greedy reglists we can't compare to the
     * memory size here so we check later.
     */
    di->reglist_sz = (prior + di->reglist_stop - di->reglist_start) *
        /* Be sure to use the sub-reg size from the template */
        opnd_size_in_bytes(size_temp);
    return true;
}

static bool
check_reglist_size(decode_info_t *di)
{
    /* Rollback of greedy reglists means we can't check reglist sizes until the end */
    if (di->memop_sz == OPSZ_VAR_REGLIST && di->reglist_sz == 0) {
        di->errmsg = "No register list found to match memory operand size";
        return false;
    } else if (di->reglist_sz > 0 && di->memop_sz != OPSZ_NA &&
        di->reglist_sz != opnd_size_in_bytes(di->memop_sz) &&
        di->memop_sz != OPSZ_VAR_REGLIST) {
        LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "  check reglist=%d memop=%s(%d)\n",
            di->reglist_sz, size_names[di->memop_sz], opnd_size_in_bytes(di->memop_sz));
        di->errmsg = "Register list size %d bytes does not match memory operand size";
        di->errmsg_param = di->reglist_sz;
        return false;
    }
    return true;
}

static ptr_int_t
get_immed_val_shared(decode_info_t *di, opnd_t opnd, bool relative, bool selected)
{
    if (opnd_is_immed_int(opnd))
        return opnd_get_immed_int(opnd);
    else if (opnd_is_near_instr(opnd)) {
        if (selected)
            di->has_instr_opnds = true;
        if (relative) {
            /* For A32, "cur PC" is really "PC + 8" */
            return (ptr_int_t)opnd_get_instr(opnd)->note - (di->cur_note + 8);
        } else {
            return (ptr_int_t)opnd_get_instr(opnd)->note - (di->cur_note) +
                (ptr_int_t)di->final_pc;
        }
    }
    CLIENT_ASSERT(false, "invalid immed opnd type");
    return 0;
}

static ptr_int_t
get_immed_val_rel(decode_info_t *di, opnd_t opnd)
{
    return get_immed_val_shared(di, opnd, true/*relative*/, true/*selected*/);
}

static ptr_int_t
get_immed_val_abs(decode_info_t *di, opnd_t opnd)
{
    return get_immed_val_shared(di, opnd, false/*relative*/, true/*selected*/);
}

static bool
encode_immed_ok(decode_info_t *di, opnd_size_t size_temp, ptr_int_t val,
                bool is_signed, bool negated)
{
    uint bits = opnd_size_in_bits(size_temp);
    /* Ensure writeback disp matches memop disp */
    if (di->check_wb_disp_sz != OPSZ_NA &&
        di->check_wb_disp_sz == size_temp &&
        di->check_wb_disp != (negated ? -val : val))
        return false;
    LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "  immed ok: val %d vs bits %d => %d\n",
        val, bits, 1 << bits);
    if (is_signed) {
        if (val < 0)
            return -val <= (1 << (bits - 1));
        else
            return val < (1 << (bits - 1));
    } else {
        ptr_uint_t uval = (ptr_uint_t) val;
        return uval < (1 << bits);
    }
}

static bool
encode_immed_int_or_instr_ok(decode_info_t *di, opnd_size_t size_temp, int multiply,
                             opnd_t opnd, bool is_signed, bool negated, bool relative)
{
    if (opnd_is_immed_int(opnd) || opnd_is_near_instr(opnd)) {
        ptr_int_t val = get_immed_val_shared(di, opnd, relative, false/*just checking*/);
        return (encode_immed_ok(di, size_temp, val / multiply, is_signed, negated) &&
                val % multiply == 0);
    }
    return false;
}

static ptr_int_t
get_mem_instr_delta(decode_info_t *di, opnd_t opnd)
{
    /* For A32, "cur PC" is really "PC + 8" */
    return (ptr_int_t)opnd_get_instr(opnd)->note - (di->cur_note + 8) +
        opnd_get_mem_instr_disp(opnd);
}

static bool
encode_mem_instr_ok(decode_info_t *di, opnd_size_t size_immed,
                    opnd_t opnd, bool is_signed, bool negated)
{
    if (opnd_is_mem_instr(opnd)) {
        ptr_int_t delta = get_mem_instr_delta(di, opnd);
        bool res = false;
        if (negated) {
            res = (delta < 0 &&
                   encode_immed_ok(di, size_immed, -delta, false/*unsigned*/, negated));
        } else {
            res = (delta >= 0 &&
                   encode_immed_ok(di, size_immed, delta, false/*unsigned*/, negated));
        }
        if (res) {
            di->check_wb_base = DR_REG_PC;
            di->check_wb_disp_sz = size_immed;
            di->check_wb_disp = delta;
        }
        return res;
    }
    return false;
}

static int
opnd_get_signed_disp(opnd_t opnd)
{
    int disp = opnd_get_disp(opnd);
    if (TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)))
        return -disp;
    else
        return disp;
}

static bool
encode_opnd_ok(decode_info_t *di, byte optype, opnd_size_t size_temp, instr_t *in,
               bool is_dst, uint *counter INOUT)
{
    uint opnum = (*counter)++;
    opnd_t opnd;
    opnd_size_t size_temp_up = resolve_size_upward(size_temp);
    opnd_size_t size_op, size_op_up;

    /* Roll back greedy reglist if necessary */
    if (di->reglist_stop > 0 && optype_is_reg(optype) &&
        di->reglist_stop - 1 > di->reglist_start && di->reglist_stop == opnum) {
        if ((is_dst &&
             (opnum >= instr_num_dsts(in) || !opnd_is_reg(instr_get_dst(in, opnum)))) ||
            (!is_dst &&
             (opnum >= instr_num_srcs(in) || !opnd_is_reg(instr_get_src(in, opnum))))) {
            LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "  reglist rollback from %d-%d\n",
                di->reglist_start, di->reglist_stop);
            CLIENT_ASSERT(*counter > 1, "non-empty reglist plus inc here -> >= 2");
            di->reglist_stop--;
            (*counter)--;
            opnum--;
        }
    }

    if (optype == TYPE_NONE) {
        return (is_dst ? (instr_num_dsts(in) < opnum) : (instr_num_srcs(in) < opnum));
    } else if (is_dst) {
        if (opnum >= instr_num_dsts(in))
            return false;
        opnd = instr_get_dst(in, opnum);
    } else {
        if (opnum >= instr_num_srcs(in))
            return false;
        opnd = instr_get_src(in, opnum);
    }

    DOLOG(1, LOG_EMIT, {
        LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "  %s_ok %s %d %-15s ", __FUNCTION__,
            is_dst ? "dst" : "src", *counter - 1, type_names[optype]);
        opnd_disassemble(GLOBAL_DCONTEXT, opnd, THREAD_GET);
        LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "\n");
    });

    size_op = opnd_get_size(opnd);
    size_op_up = resolve_size_upward(size_op);

    switch (optype) {

    /* Register types */
    /* For registers, we support requesting whole reg when only part is in template */
    case TYPE_R_B:
    case TYPE_R_C:
    case TYPE_R_A_TOP:
    case TYPE_R_B_TOP:
    case TYPE_R_C_TOP:
    case TYPE_R_D_TOP:
    case TYPE_R_D_NEGATED:
        return (opnd_is_reg(opnd) && reg_is_gpr(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up));
    case TYPE_R_A:
        return (opnd_is_reg(opnd) && reg_is_gpr(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up) &&
                /* Ensure writeback matches memop base */
                (di->check_wb_base == DR_REG_NULL ||
                 di->check_wb_base == opnd_get_reg(opnd)));
    case TYPE_R_D:
        return (opnd_is_reg(opnd) && reg_is_gpr(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up) &&
                /* Ensure writeback index matches memop index */
                (di->check_wb_index == DR_REG_NULL ||
                 di->check_wb_index == opnd_get_reg(opnd)));
    case TYPE_R_B_EVEN:
    case TYPE_R_D_EVEN:
        return (opnd_is_reg(opnd) && reg_is_gpr(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up) &&
                ((dr_reg_fixer[opnd_get_reg(opnd)] - DR_REG_START_GPR) % 2 == 0));
    case TYPE_R_B_PLUS1:
    case TYPE_R_D_PLUS1: {
        opnd_t prior;
        if (opnum ==  0)
            return false;
        if (is_dst)
            prior = instr_get_dst(in, opnum - 1);
        else
            prior = instr_get_src(in, opnum - 1);
        return (opnd_is_reg(opnd) && reg_is_gpr(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up) &&
                opnd_is_reg(prior) && opnd_get_reg(prior) + 1 == opnd_get_reg(opnd));
    }
    case TYPE_CR_A:
    case TYPE_CR_B:
    case TYPE_CR_C:
    case TYPE_CR_D:
        return (opnd_is_reg(opnd) && reg_is_cpreg(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up));
    case TYPE_V_A:
    case TYPE_V_B:
    case TYPE_V_C:
    case TYPE_W_A:
    case TYPE_W_B:
    case TYPE_W_C:
        return (opnd_is_reg(opnd) && reg_is_simd(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up));
    case TYPE_V_C_3b:
        return (opnd_is_reg(opnd) && reg_is_simd(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up) &&
                (opnd_get_reg(opnd) - reg_simd_start(opnd_get_reg(opnd)) < 8));
    case TYPE_V_C_4b:
        return (opnd_is_reg(opnd) && reg_is_simd(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up) &&
                (opnd_get_reg(opnd) - reg_simd_start(opnd_get_reg(opnd)) < 16));
    case TYPE_W_C_PLUS1: {
        opnd_t prior;
        if (opnum ==  0)
            return false;
        if (is_dst)
            prior = instr_get_dst(in, opnum - 1);
        else
            prior = instr_get_src(in, opnum - 1);
        return (opnd_is_reg(opnd) && reg_is_simd(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up) &&
                opnd_is_reg(prior) && opnd_get_reg(prior) + 1 == opnd_get_reg(opnd));
    }
    case TYPE_SPSR:
        return (opnd_is_reg(opnd) && opnd_get_reg(opnd) == DR_REG_SPSR);
    case TYPE_CPSR:
        return (opnd_is_reg(opnd) && opnd_get_reg(opnd) == DR_REG_CPSR);
    case TYPE_FPSCR:
        return (opnd_is_reg(opnd) && opnd_get_reg(opnd) == DR_REG_FPSCR);
    case TYPE_LR:
        return (opnd_is_reg(opnd) && opnd_get_reg(opnd) == DR_REG_LR);
    case TYPE_SP:
        return (opnd_is_reg(opnd) && opnd_get_reg(opnd) == DR_REG_SP);

    /* Register lists */
    case TYPE_L_8b:
    case TYPE_L_13b:
    case TYPE_L_16b: {
        /* Strategy: first, we disallow any template with a reglist followed by more
         * than one plain register type (checked in decode_debug_checks_arch()).
         * Then, we greedily eat all regs here.  On a subsequent reg type, we remove
         * one entry from the list if necessary.  This is simpler than trying to look
         * ahead, or to disallow any reg after a reglist (that would lead to
         * wrong-order-vs-asm for OP_vtbl and others).
         */
        uint max_num = (optype == TYPE_L_8b ? 8 : (optype == TYPE_L_13b ? 13 : 16));
        if (!encode_reglist_ok(di, size_temp, in, is_dst, counter, max_num, false/*gpr*/,
                               0/*no restrictions*/, 0))
            return false;
        /* We refuse to encode as an empty list ("unpredictable", and harder to ensure
         * encoding templates are distinguishable)
         */
        return (di->reglist_stop > di->reglist_start);
    }
    case TYPE_L_CONSEC: {
        uint max_num = 32; /* # of simd regs */
        opnd_t prior;
        if (opnum == 0)
            return false;
        if (is_dst)
            prior = instr_get_dst(in, opnum - 1);
        else
            prior = instr_get_src(in, opnum - 1);
        if (!opnd_is_reg(prior) || !reg_is_simd(opnd_get_reg(prior)))
            return false;
        if (!encode_reglist_ok(di, size_temp, in, is_dst, counter, max_num, true/*simd*/,
                               1/*consec*/, 0))
            return false;
        /* We have to allow an empty list b/c the template has the 1st entry */
        return true;
    }
    case TYPE_L_VAx2:
    case TYPE_L_VBx2:
        if (!encode_reglist_ok(di, size_temp, in, is_dst, counter, 2, true/*simd*/,
                               1/*consec*/, 1/*prior entry*/))
            return false;
        return (di->reglist_stop > di->reglist_start);
    case TYPE_L_VAx3:
    case TYPE_L_VBx3:
        if (!encode_reglist_ok(di, size_temp, in, is_dst, counter, 3, true/*simd*/,
                               1/*consec*/, 0))
            return false;
        return (di->reglist_stop > di->reglist_start);
    case TYPE_L_VAx4:
    case TYPE_L_VBx4:
        if (!encode_reglist_ok(di, size_temp, in, is_dst, counter, 4, true/*simd*/,
                               1/*consec*/, 0))
            return false;
        return (di->reglist_stop > di->reglist_start);

    case TYPE_L_VBx2D:
        if (!encode_reglist_ok(di, size_temp, in, is_dst, counter, 2, true/*simd*/,
                               2/*doubly-spaced*/, 0))
            return false;
        return (di->reglist_stop > di->reglist_start);
    case TYPE_L_VBx3D:
        if (!encode_reglist_ok(di, size_temp, in, is_dst, counter, 3, true/*simd*/,
                               2/*doubly-spaced*/, 0))
            return false;
        return (di->reglist_stop > di->reglist_start);
    case TYPE_L_VBx4D:
        if (!encode_reglist_ok(di, size_temp, in, is_dst, counter, 4, true/*simd*/,
                               2/*doubly-spaced*/, 0))
            return false;
        return (di->reglist_stop > di->reglist_start);

    /* Immeds */
    case TYPE_I_b0:
    case TYPE_I_b3:
    case TYPE_I_b4:
    case TYPE_I_b6:
    case TYPE_I_b7:
    case TYPE_I_b8:
    case TYPE_I_b9:
    case TYPE_I_b10:
    case TYPE_I_b16:
    case TYPE_I_b17:
    case TYPE_I_b18:
    case TYPE_I_b19:
    case TYPE_I_b20:
    case TYPE_I_b21:
    case TYPE_I_b0_b5:
    case TYPE_I_b5_b3:
    case TYPE_I_b8_b0:
    case TYPE_I_b8_b16:
    case TYPE_I_b16_b0:
    case TYPE_I_b21_b5:
    case TYPE_I_b21_b6:
    case TYPE_I_b24_b16_b0:
        return encode_immed_int_or_instr_ok(di, size_temp, 1, opnd, false/*unsigned*/,
                                            false/*pos*/, false/*abs*/);
    case TYPE_I_x4_b0:
        return encode_immed_int_or_instr_ok(di, size_temp, 4, opnd, true/*signed*/,
                                            false/*pos*/, true/*rel*/);
    case TYPE_NI_b0:
    case TYPE_NI_b8_b0:
        return (opnd_is_immed_int(opnd) &&
                encode_immed_ok(di, size_temp, -opnd_get_immed_int(opnd),
                                false/*unsigned*/, true/*negated*/));
    case TYPE_I_b5:
        return (encode_immed_int_or_instr_ok(di, size_temp, 1, opnd, false/*unsigned*/,
                                             false/*pos*/, false/*abs*/) &&
                /* Ensure writeback shift matches memop shift */
                (opnd_is_instr(opnd) || !di->check_wb_shift ||
                 di->check_wb_shift_amount == opnd_get_immed_int(opnd)));
    case TYPE_SHIFT_b5:
    case TYPE_SHIFT_b6:
        return (opnd_is_immed_int(opnd) &&
                encode_immed_ok(di, size_temp, opnd_get_immed_int(opnd),
                                false/*unsigned*/, false/*pos*/) &&
                /* Ensure writeback shift matches memop shift */
                (!di->check_wb_shift ||
                 di->check_wb_shift_type == opnd_get_immed_int(opnd)));
    case TYPE_I_b0_b24: /* OP_blx imm24:H:0 */
        return encode_immed_int_or_instr_ok(di, size_temp, 2, opnd, false/*unsigned*/,
                                            false/*pos*/, true/*rel*/);
    case TYPE_SHIFT_LSL:
        return (opnd_is_immed_int(opnd) &&
                opnd_get_immed_int(opnd) == SHIFT_ENCODING_LSL);
    case TYPE_SHIFT_ASR:
        return (opnd_is_immed_int(opnd) &&
                opnd_get_immed_int(opnd) == SHIFT_ENCODING_ASR);
    case TYPE_K:
        return (opnd_is_immed_int(opnd) &&
                size_op == OPSZ_0);

    /* Memory */
    case TYPE_M:
        if (opnd_is_base_disp(opnd) &&
            opnd_get_base(opnd) != REG_NULL &&
            opnd_get_index(opnd) == REG_NULL &&
            opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
            opnd_get_disp(opnd) == 0 &&
            /* We check for OPSZ_VAR_REGLIST but no reglist in check_reglist_size() */
            (size_op == size_temp || size_op == OPSZ_VAR_REGLIST)) {
            di->memop_sz = size_op;
            di->check_wb_base = opnd_get_base(opnd);
            return true;
        } else
            return false;
    case TYPE_M_POS_I12:
    case TYPE_M_NEG_I12:
        if (opnd_is_base_disp(opnd) &&
            opnd_get_base(opnd) != REG_NULL &&
            opnd_get_index(opnd) == REG_NULL &&
            opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
            BOOLS_MATCH(TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)),
                        optype == TYPE_M_NEG_I12) &&
            encode_immed_ok(di, OPSZ_12b, opnd_get_disp(opnd), false/*unsigned*/,
                            TEST(DR_OPND_NEGATED, opnd_get_flags(opnd))) &&
            size_op == size_temp) {
            di->check_wb_base = opnd_get_base(opnd);
            di->check_wb_disp_sz = OPSZ_12b;
            di->check_wb_disp = opnd_get_signed_disp(opnd);
            return true;
        } else {
            return encode_mem_instr_ok(di, OPSZ_12b, opnd, false/*unsigned*/,
                                       optype == TYPE_M_NEG_I12);
        }
    case TYPE_M_POS_REG:
    case TYPE_M_NEG_REG:
        if (opnd_is_base_disp(opnd) &&
            opnd_get_base(opnd) != REG_NULL &&
            opnd_get_index(opnd) != REG_NULL &&
            opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
            BOOLS_MATCH(TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)),
                        optype == TYPE_M_NEG_REG) &&
            opnd_get_disp(opnd) == 0 &&
            size_op == size_temp) {
            di->check_wb_base = opnd_get_base(opnd);
            di->check_wb_index = opnd_get_index(opnd);
            return true;
        } else
            return false;
    case TYPE_M_POS_SHREG:
    case TYPE_M_NEG_SHREG:
        if (opnd_is_base_disp(opnd) &&
            opnd_get_base(opnd) != REG_NULL &&
            opnd_get_index(opnd) != REG_NULL &&
            BOOLS_MATCH(TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)),
                        optype == TYPE_M_NEG_SHREG) &&
            opnd_get_disp(opnd) == 0 &&
            size_op == size_temp) {
            ptr_int_t sh2, val;
            encode_index_shift_values(opnd, &sh2, &val);
            di->check_wb_base = opnd_get_base(opnd);
            di->check_wb_index = opnd_get_index(opnd);
            di->check_wb_shift = true;
            di->check_wb_shift_type = sh2;
            di->check_wb_shift_amount = val;
            return true;
        } else
            return false;
    case TYPE_M_SI9:
        return (opnd_is_base_disp(opnd) &&
                opnd_get_base(opnd) != REG_NULL &&
                opnd_get_index(opnd) == REG_NULL &&
                opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
                encode_immed_ok(di, OPSZ_9b, opnd_get_signed_disp(opnd),
                                true/*signed*/, false/*pos*/) &&
                size_op == size_temp);
    case TYPE_M_SI7:
        return (opnd_is_base_disp(opnd) &&
                opnd_get_base(opnd) != REG_NULL &&
                opnd_get_index(opnd) == REG_NULL &&
                opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
                encode_immed_ok(di, OPSZ_7b, opnd_get_signed_disp(opnd),
                                true/*signed*/, false/*pos*/) &&
                size_op == size_temp);
    case TYPE_M_POS_I8:
    case TYPE_M_NEG_I8:
        if (opnd_is_base_disp(opnd) &&
            opnd_get_base(opnd) != REG_NULL &&
            opnd_get_index(opnd) == REG_NULL &&
            opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
            BOOLS_MATCH(TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)),
                        optype == TYPE_M_NEG_I8) &&
            opnd_get_disp(opnd) % 4 == 0 &&
            encode_immed_ok(di, OPSZ_1, opnd_get_disp(opnd)/4, false/*unsigned*/,
                            TEST(DR_OPND_NEGATED, opnd_get_flags(opnd))) &&
            size_op == size_temp) {
            di->check_wb_base = opnd_get_base(opnd);
            di->check_wb_disp_sz = OPSZ_1;
            di->check_wb_disp = opnd_get_signed_disp(opnd);
            return true;
        } else {
            return encode_mem_instr_ok(di, OPSZ_1, opnd, false/*unsigned*/,
                                       optype == TYPE_M_NEG_I8);
        }
    case TYPE_M_POS_I4_4:
    case TYPE_M_NEG_I4_4:
        if (opnd_is_base_disp(opnd) &&
            opnd_get_base(opnd) != REG_NULL &&
            opnd_get_index(opnd) == REG_NULL &&
            opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
            BOOLS_MATCH(TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)),
                        optype == TYPE_M_NEG_I4_4) &&
            encode_immed_ok(di, OPSZ_1, opnd_get_disp(opnd), false/*unsigned*/,
                            TEST(DR_OPND_NEGATED, opnd_get_flags(opnd))) &&
            size_op == size_temp) {
            di->check_wb_base = opnd_get_base(opnd);
            di->check_wb_disp_sz = OPSZ_1;
            di->check_wb_disp = opnd_get_signed_disp(opnd);
            return true;
        } else {
            return encode_mem_instr_ok(di, OPSZ_1, opnd, false/*unsigned*/,
                                       optype == TYPE_M_NEG_I4_4);
        }
    case TYPE_M_POS_I5:
        return (opnd_is_base_disp(opnd) &&
                opnd_get_base(opnd) != REG_NULL &&
                opnd_get_index(opnd) == REG_NULL &&
                opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
                !TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)) &&
                encode_immed_ok(di, OPSZ_5b, opnd_get_disp(opnd), false/*unsigned*/,
                                false/*pos*/) &&
                size_op == size_temp);
    case TYPE_M_UP_OFFS:
    case TYPE_M_DOWN_OFFS:
    case TYPE_M_DOWN:
        di->memop_sz = size_op;
        return (opnd_is_base_disp(opnd) &&
                opnd_get_base(opnd) != REG_NULL &&
                opnd_get_index(opnd) == REG_NULL &&
                opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
                /* We check for OPSZ_VAR_REGLIST but no reglist in check_reglist_size() */
                (size_op == OPSZ_VAR_REGLIST ||
                 (size_op == size_temp &&
                  ((optype == TYPE_M_UP_OFFS &&
                    opnd_get_disp(opnd) == sizeof(void*)) ||
                   (optype == TYPE_M_DOWN_OFFS &&
                    opnd_get_disp(opnd) == opnd_size_in_bytes(size_op)-sizeof(void*)) ||
                   (optype == TYPE_M_DOWN &&
                    opnd_get_disp(opnd) == opnd_size_in_bytes(size_op))))));

    default:
        CLIENT_ASSERT(false, "encode-ok error: unknown operand type");
    }

    return false;
}

bool
encoding_possible(decode_info_t *di, instr_t *in, const instr_info_t * ii)
{
    uint num_dsts = 0, num_srcs = 0;
    dr_pred_type_t pred = instr_get_predicate(in);

    if (ii == NULL || in == NULL)
        return false;

    LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "%s 0x%08x\n", __FUNCTION__, ii->opcode);

    /* FIXME i#1551: check isa mode vs THUMB_ONLY or ARM_ONLY ii->flags */

    /* Check predicate.  We're fine with DR_PRED_NONE == DR_PRED_AL. */
    if (pred == DR_PRED_OP) {
        di->errmsg = "DR_PRED_OP is an illegal predicate request";
        return false;
    } else if (TEST(DECODE_PREDICATE_AL_ONLY, ii->flags) && pred != DR_PRED_AL &&
               pred != DR_PRED_NONE) {
        di->errmsg = "DR_PRED_AL is the only valid predicate";
        return false;
    }
    else if (!TEST(DECODE_PREDICATE, ii->flags) && pred != DR_PRED_NONE) {
        di->errmsg = "No predicate is supported";
        return false;
    }

    /* Check each operand */
    do {
        if (ii->dst1_type != TYPE_NONE) {
            if (!encode_opnd_ok(di, ii->dst1_type, ii->dst1_size, in, true, &num_dsts)) {
                di->errmsg = "Destination operand #%d has wrong type/size";
                di->errmsg_param = num_dsts - 1;
                return false;
            }
        }
        if (ii->dst2_type != TYPE_NONE) {
            if (!encode_opnd_ok(di, ii->dst2_type, ii->dst2_size, in,
                                !TEST(DECODE_4_SRCS, ii->flags),
                                TEST(DECODE_4_SRCS, ii->flags) ? &num_srcs : &num_dsts)) {
                if (TEST(DECODE_4_SRCS, ii->flags)) {
                    di->errmsg = "Source operand #%d has wrong type/size";
                    di->errmsg_param = num_srcs - 1;
                } else {
                    di->errmsg = "Destination operand #%d has wrong type/size";
                    di->errmsg_param = num_dsts - 1;
                }
                return false;
            }
        }
        if (ii->src1_type != TYPE_NONE) {
            if (!encode_opnd_ok(di, ii->src1_type, ii->src1_size, in,
                                TEST(DECODE_3_DSTS, ii->flags),
                                TEST(DECODE_3_DSTS, ii->flags) ? &num_dsts : &num_srcs)) {
                if (TEST(DECODE_3_DSTS, ii->flags)) {
                    di->errmsg = "Destination operand #%d has wrong type/size";
                    di->errmsg_param = num_dsts - 1;
                } else {
                    di->errmsg = "Source operand #%d has wrong type/size";
                    di->errmsg_param = num_srcs - 1;
                }
                return false;
            }
        }
        if (ii->src2_type != TYPE_NONE) {
            if (!encode_opnd_ok(di, ii->src2_type, ii->src2_size, in, false, &num_srcs)) {
                di->errmsg = "Source operand #%d has wrong type/size";
                di->errmsg_param = num_srcs - 1;
                return false;
            }
        }
        if (ii->src3_type != TYPE_NONE) {
            if (!encode_opnd_ok(di, ii->src3_type, ii->src3_size, in, false, &num_srcs)) {
                di->errmsg = "Source operand #%d has wrong type/size";
                di->errmsg_param = num_srcs - 1;
                return false;
            }
        }
        ii = instr_info_extra_opnds(ii);
    } while (ii != NULL);

    LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "  checking %d vs %d, %d vs %d\n",
        num_dsts, instr_num_dsts(in), num_srcs, instr_num_srcs(in));
    if (num_dsts < instr_num_dsts(in) || num_srcs < instr_num_srcs(in))
        return false;

    return check_reglist_size(di);
}

void
decode_info_init_for_instr(decode_info_t *di, instr_t *instr)
{
    memset(di, 0, sizeof(*di));
    di->isa_mode = instr_get_isa_mode(instr);
}

static void
encode_regA(decode_info_t *di, reg_id_t reg)
{
    /* A32 = 19:16 */
    di->instr_word |= (reg - DR_REG_START_GPR) << 16;
}

static void
encode_regB(decode_info_t *di, reg_id_t reg)
{
    /* A32 = 15:12 */
    di->instr_word |= (reg - DR_REG_START_GPR) << 12;
}

static void
encode_regC(decode_info_t *di, reg_id_t reg)
{
    /* A32 = 11:8 */
    di->instr_word |= (reg - DR_REG_START_GPR) << 8;
}

static void
encode_regD(decode_info_t *di, reg_id_t reg)
{
    /* A32 = 3:0 */
    di->instr_word |= (reg - DR_REG_START_GPR);
}

static void
encode_immed(decode_info_t *di, uint start_bit, opnd_size_t size_temp,
             ptr_int_t val, bool is_signed)
{
    uint bits = opnd_size_in_bits(size_temp);
    di->instr_word |= (val & ((1 << bits) - 1)) << start_bit;
}

static void
encode_index_shift(decode_info_t *di, opnd_t opnd)
{
    ptr_int_t sh2, val;
    encode_index_shift_values(opnd, &sh2, &val);
    encode_immed(di, DECODE_INDEX_SHIFT_TYPE_BITPOS,
                 DECODE_INDEX_SHIFT_TYPE_SIZE, sh2, false);
    encode_immed(di, DECODE_INDEX_SHIFT_AMOUNT_BITPOS,
                 DECODE_INDEX_SHIFT_AMOUNT_SIZE, val, false);
}

static void
encode_operand(decode_info_t *di, byte optype, opnd_size_t size_temp, instr_t *in,
               bool is_dst, uint *counter INOUT)
{
    uint opnum = (*counter)++;
    opnd_size_t size_temp_up = resolve_size_upward(size_temp);
    opnd_t opnd;
    if (optype != TYPE_NONE) {
        if (is_dst)
            opnd = instr_get_dst(in, opnum);
        else
            opnd = instr_get_src(in, opnum);
    }

    switch (optype) {
    /* Registers */
    case TYPE_R_A:
    case TYPE_R_A_TOP:
        encode_regA(di, opnd_get_reg(opnd));
        break;
    case TYPE_R_B:
    case TYPE_R_B_TOP:
    case TYPE_R_B_EVEN:
        encode_regB(di, opnd_get_reg(opnd));
        break;
    case TYPE_R_C:
    case TYPE_R_C_TOP:
        encode_regC(di, opnd_get_reg(opnd));
        break;
    case TYPE_R_D:
    case TYPE_R_D_TOP:
    case TYPE_R_D_NEGATED:
    case TYPE_R_D_EVEN:
        encode_regD(di, opnd_get_reg(opnd));
        break;
    case TYPE_CR_A:
        encode_regA(di, opnd_get_reg(opnd) - DR_REG_CR0 + DR_REG_START_GPR);
        break;
    case TYPE_CR_B:
        encode_regB(di, opnd_get_reg(opnd) - DR_REG_CR0 + DR_REG_START_GPR);
        break;
    case TYPE_CR_C:
        encode_regC(di, opnd_get_reg(opnd) - DR_REG_CR0 + DR_REG_START_GPR);
        break;
    case TYPE_CR_D:
        encode_regD(di, opnd_get_reg(opnd) - DR_REG_CR0 + DR_REG_START_GPR);
        break;
    case TYPE_V_A:
    case TYPE_L_VAx2:
    case TYPE_L_VAx3:
    case TYPE_L_VAx4: {
        /* A32 = 7,19:16 */
        reg_id_t reg = opnd_get_reg(opnd);
        uint val = reg - reg_simd_start(reg);
        di->instr_word |= ((val & 0x10) << 3) | ((val & 0xf) << 16);
        if (di->reglist_stop > 0)
            (*counter) += (di->reglist_stop - 1 - di->reglist_start);
        break;
    }
    case TYPE_V_B:
    case TYPE_L_VBx2:
    case TYPE_L_VBx3:
    case TYPE_L_VBx4:
    case TYPE_L_VBx2D:
    case TYPE_L_VBx3D:
    case TYPE_L_VBx4D: {
        /* A32 = 22,15:12 */
        reg_id_t reg = opnd_get_reg(opnd);
        uint val = reg - reg_simd_start(reg);
        di->instr_word |= ((val & 0x10) << 18) | ((val & 0xf) << 12);
        if (di->reglist_stop > 0)
            (*counter) += (di->reglist_stop - 1 - di->reglist_start);
        break;
    }
    case TYPE_V_C: {
        /* A32 = 5,3:0 */
        reg_id_t reg = opnd_get_reg(opnd);
        uint val = reg - reg_simd_start(reg);
        di->instr_word |= ((val & 0x10) << 1) | (val & 0xf);
        break;
    }
    case TYPE_W_A: {
        /* A32 = 19:16,7 */
        reg_id_t reg = opnd_get_reg(opnd);
        uint val = reg - reg_simd_start(reg);
        di->instr_word |= ((val & 0x1e) << 15) | ((val & 0x1) << 7);
        break;
    }
    case TYPE_W_B: {
        /* A32 = 15:12,22 */
        reg_id_t reg = opnd_get_reg(opnd);
        uint val = reg - reg_simd_start(reg);
        di->instr_word |= ((val & 0x1e) << 11) | ((val & 0x1) << 22);
        break;
    }
    case TYPE_W_C: {
        /* A32 = 3:0,5 */
        reg_id_t reg = opnd_get_reg(opnd);
        uint val = reg - reg_simd_start(reg);
        di->instr_word |= ((val & 0x1e) >> 1) | ((val & 0xf) << 5);
        break;
    }
    case TYPE_V_C_3b: {
        /* A32 = 2:0 */
        reg_id_t reg = opnd_get_reg(opnd);
        uint val = reg - reg_simd_start(reg);
        di->instr_word |=  (val & 0x7);
        break;
    }
    case TYPE_V_C_4b: {
        /* A32 = 3:0 */
        reg_id_t reg = opnd_get_reg(opnd);
        uint val = reg - reg_simd_start(reg);
        di->instr_word |=  (val & 0xf);
        break;
    }

    /* Register lists */
    case TYPE_L_8b:
    case TYPE_L_13b:
    case TYPE_L_16b: {
        uint i;
        CLIENT_ASSERT(di->reglist_start == *counter - 1, "internal reglist encode error");
        for (i = di->reglist_start; i < di->reglist_stop; i++) {
            opnd = is_dst ? instr_get_dst(in, i) : instr_get_src(in, i);
            di->instr_word |= 1 << (opnd_get_reg(opnd) - DR_REG_START_GPR);
        }
        /* already incremented once */
        (*counter) += (di->reglist_stop - 1 - di->reglist_start);
        break;
    }
    case TYPE_L_CONSEC: {
        /* Consecutive multimedia regs: dword count in immed 7:0 */
        uint dwords = 1/*in template*/ + di->reglist_stop - di->reglist_start;
        if (size_temp_up == OPSZ_8)
            dwords *= 2;
        else
            CLIENT_ASSERT(size_temp_up == OPSZ_4, "invalid LC size");
        di->instr_word |= dwords;
        if (di->reglist_stop > di->reglist_start)
            (*counter) += (di->reglist_stop - 1 - di->reglist_start);
        break;
    }

    /* Immeds */
    case TYPE_I_b0:
        encode_immed(di, 0, size_temp, get_immed_val_abs(di, opnd), false/*unsigned*/);
        break;
    case TYPE_NI_b0:
        encode_immed(di, 0, size_temp, -get_immed_val_abs(di, opnd), false/*unsigned*/);
        break;
    case TYPE_I_b3:
        encode_immed(di, 3, size_temp, get_immed_val_abs(di, opnd), false/*unsigned*/);
        break;
    case TYPE_I_b4:
        encode_immed(di, 4, size_temp, get_immed_val_abs(di, opnd), false/*unsigned*/);
        break;
    case TYPE_I_b5:
        encode_immed(di, 5, size_temp, get_immed_val_abs(di, opnd), false/*unsigned*/);
        break;
    case TYPE_I_b6:
        encode_immed(di, 6, size_temp, get_immed_val_abs(di, opnd), false/*unsigned*/);
        break;
    case TYPE_I_b7:
        encode_immed(di, 7, size_temp, get_immed_val_abs(di, opnd), false/*unsigned*/);
        break;
    case TYPE_I_b8:
        encode_immed(di, 8, size_temp, get_immed_val_abs(di, opnd), false/*unsigned*/);
        break;
    case TYPE_I_b9:
        encode_immed(di, 9, size_temp, get_immed_val_abs(di, opnd), false/*unsigned*/);
        break;
    case TYPE_I_b10:
        encode_immed(di, 10, size_temp, get_immed_val_abs(di, opnd), false/*unsigned*/);
        break;
    case TYPE_I_b16:
        encode_immed(di, 16, size_temp, get_immed_val_abs(di, opnd), false/*unsigned*/);
        break;
    case TYPE_I_b17:
        encode_immed(di, 17, size_temp, get_immed_val_abs(di, opnd), false/*unsigned*/);
        break;
    case TYPE_I_b18:
        encode_immed(di, 18, size_temp, get_immed_val_abs(di, opnd), false/*unsigned*/);
        break;
    case TYPE_I_b19:
        encode_immed(di, 19, size_temp, get_immed_val_abs(di, opnd), false/*unsigned*/);
        break;
    case TYPE_I_b20:
        encode_immed(di, 20, size_temp, get_immed_val_abs(di, opnd), false/*unsigned*/);
        break;
    case TYPE_I_b21:
        encode_immed(di, 21, size_temp, get_immed_val_abs(di, opnd), false/*unsigned*/);
        break;
    case TYPE_I_b0_b5: {
        ptr_int_t val = get_immed_val_abs(di, opnd);
        if (size_temp == OPSZ_5b) {
            encode_immed(di, 5, OPSZ_1b, val, false/*unsigned*/);
            encode_immed(di, 0, OPSZ_4b, val >> 1, false/*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 0-5 split immed size");
        break;
    }
    case TYPE_I_b0_b24: { /* OP_blx imm24:H:0 */
        ptr_int_t val = get_immed_val_rel(di, opnd);
        if (size_temp == OPSZ_25b) {
            encode_immed(di, 24, OPSZ_1b, val >> 1, false/*unsigned*/);
            encode_immed(di, 0, OPSZ_3, val >> 2, false/*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 0-24 split immed size");
        break;
    }
    case TYPE_I_b5_b3: {
        ptr_int_t val = get_immed_val_abs(di, opnd);
        if (size_temp == OPSZ_2b) {
            encode_immed(di, 3, OPSZ_1b, val, false/*unsigned*/);
            encode_immed(di, 5, OPSZ_1b, val >> 1, false/*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 5-3 immed size");
        break;
    }
    case TYPE_NI_b8_b0:
    case TYPE_I_b8_b0: {
        ptr_int_t val = get_immed_val_abs(di, opnd);
        if (optype == TYPE_NI_b8_b0)
            val = -val;
        if (size_temp == OPSZ_2) {
            encode_immed(di, 0, OPSZ_4b, val, false/*unsigned*/);
            encode_immed(di, 8, OPSZ_12b, val >> 4, false/*unsigned*/);
        } else if (size_temp == OPSZ_1) {
            encode_immed(di, 0, OPSZ_4b, val, false/*unsigned*/);
            encode_immed(di, 8, OPSZ_4b, val >> 4, false/*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 8-0 split immed size");
        break;
    }
    case TYPE_I_b8_b16: {
        ptr_int_t val = get_immed_val_abs(di, opnd);
        if (size_temp == OPSZ_5b) {
            encode_immed(di, 16, OPSZ_4b, val, false/*unsigned*/);
            encode_immed(di, 8, OPSZ_1b, val >> 4, false/*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 8-16 split immed size");
        break;
    }
    case TYPE_I_b16_b0: {
        ptr_int_t val = get_immed_val_abs(di, opnd);
        if (size_temp == OPSZ_2) {
            encode_immed(di, 0, OPSZ_12b, val, false/*unsigned*/);
            encode_immed(di, 16, OPSZ_4b, val >> 12, false/*unsigned*/);
        } else if (size_temp == OPSZ_1) {
            encode_immed(di, 0, OPSZ_4b, val, false/*unsigned*/);
            encode_immed(di, 16, OPSZ_4b, val >> 4, false/*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 16-0 split immed size");
        break;
    }
    case TYPE_I_b21_b5: {
        ptr_int_t val = get_immed_val_abs(di, opnd);
        if (size_temp == OPSZ_3b) {
            encode_immed(di, 5, OPSZ_2b, val, false/*unsigned*/);
            encode_immed(di, 21, OPSZ_1b, val >> 2, false/*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 21-5 split immed size");
        break;
    }
    case TYPE_I_b21_b6: {
        ptr_int_t val = get_immed_val_abs(di, opnd);
        if (size_temp == OPSZ_2b) {
            encode_immed(di, 6, OPSZ_1b, val, false/*unsigned*/);
            encode_immed(di, 21, OPSZ_1b, val >> 1, false/*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 21-6 split immed size");
        break;
    }
    case TYPE_I_b24_b16_b0: {
        ptr_int_t val = get_immed_val_abs(di, opnd);
        if (size_temp == OPSZ_1) {
            encode_immed(di, 5, OPSZ_4b, val, false/*unsigned*/);
            encode_immed(di, 16, OPSZ_3b, val >> 4, false/*unsigned*/);
            encode_immed(di, 24, OPSZ_1b, val >> 7, false/*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 24-16-0 split immed size");
        break;
    }
    case TYPE_SHIFT_b5:
        encode_immed(di, 5, size_temp, opnd_get_immed_int(opnd), false/*unsigned*/);
        break;
    case TYPE_SHIFT_b6:
        encode_immed(di, 5, size_temp, opnd_get_immed_int(opnd) << 1, false/*unsigned*/);
        break;
    case TYPE_I_x4_b0:
        encode_immed(di, 0, size_temp, get_immed_val_rel(di, opnd) >> 2, true/*signed*/);
        break;

    /* Memory */
    case TYPE_M:
    case TYPE_M_UP_OFFS:
    case TYPE_M_DOWN:
    case TYPE_M_DOWN_OFFS:
        encode_regA(di, opnd_get_base(opnd));
        break;
    case TYPE_M_POS_I12:
    case TYPE_M_NEG_I12:
        if (opnd_is_base_disp(opnd)) {
            encode_regA(di, opnd_get_base(opnd));
            encode_immed(di, 0, OPSZ_12b, opnd_get_disp(opnd), false/*unsigned*/);
        } else if (opnd_is_mem_instr(opnd)) {
            ptr_int_t delta = get_mem_instr_delta(di, opnd);
            encode_regA(di, DR_REG_PC);
            encode_immed(di, 0, OPSZ_12b, delta < 0 ? -delta : delta, false/*unsigned*/);
        }
        break;
    case TYPE_M_POS_REG:
    case TYPE_M_NEG_REG:
        encode_regA(di, opnd_get_base(opnd));
        encode_regD(di, opnd_get_index(opnd));
        break;
    case TYPE_M_POS_SHREG:
    case TYPE_M_NEG_SHREG:
        encode_regA(di, opnd_get_base(opnd));
        encode_regD(di, opnd_get_index(opnd));
        encode_index_shift(di, opnd);
        break;
    case TYPE_M_SI9:
        encode_regA(di, opnd_get_base(opnd));
        encode_immed(di, 12, OPSZ_9b, opnd_get_signed_disp(opnd), true/*signed*/);
        break;
    case TYPE_M_SI7:
        encode_regA(di, opnd_get_base(opnd));
        encode_immed(di, 0, OPSZ_7b, opnd_get_signed_disp(opnd), true/*signed*/);
        break;
    case TYPE_M_POS_I8:
    case TYPE_M_NEG_I8:
        if (opnd_is_base_disp(opnd)) {
            encode_regA(di, opnd_get_base(opnd));
            encode_immed(di, 0, OPSZ_1, opnd_get_disp(opnd)/4, false/*unsigned*/);
        } else if (opnd_is_mem_instr(opnd)) {
            ptr_int_t delta = get_mem_instr_delta(di, opnd);
            encode_regA(di, DR_REG_PC);
            encode_immed(di, 0, OPSZ_1, delta < 0 ? -delta : delta, false/*unsigned*/);
        }
        break;
    case TYPE_M_POS_I4_4:
    case TYPE_M_NEG_I4_4:
        if (opnd_is_base_disp(opnd)) {
            encode_regA(di, opnd_get_base(opnd));
            encode_immed(di, 0, OPSZ_4b, opnd_get_disp(opnd), false/*unsigned*/);
            encode_immed(di, 8, OPSZ_4b, opnd_get_disp(opnd) >> 4, false/*unsigned*/);
        } else if (opnd_is_mem_instr(opnd)) {
            ptr_int_t delta = get_mem_instr_delta(di, opnd);
            encode_regA(di, DR_REG_PC);
            encode_immed(di, 0, OPSZ_4b, delta < 0 ? -delta : delta, false/*unsigned*/);
            encode_immed(di, 8, OPSZ_4b, (delta < 0 ? -delta : delta) >> 4,
                         false/*unsigned*/);
        }
        break;
    case TYPE_M_POS_I5:
        encode_regA(di, opnd_get_base(opnd));
        encode_immed(di, 0, OPSZ_5b, opnd_get_disp(opnd), false/*unsigned*/);
        break;

    case TYPE_NONE:
    case TYPE_R_D_PLUS1:
    case TYPE_R_B_PLUS1:
    case TYPE_W_C_PLUS1:
    case TYPE_SPSR:
    case TYPE_CPSR:
    case TYPE_FPSCR:
    case TYPE_LR:
    case TYPE_SP:
    case TYPE_SHIFT_LSL:
    case TYPE_SHIFT_ASR:
    case TYPE_K:
        break; /* implicit or empty */

    default:
        CLIENT_ASSERT(false, "encode error: unknown operand type");
    }

    LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "encode opnd %d => 0x%08x\n",
        *counter - 1, di->instr_word);
}

static void
encode_operands(decode_info_t *di, instr_t *in, const instr_info_t * ii)
{
    uint num_dsts = 0, num_srcs = 0;
    do {
        if (ii->dst1_type != TYPE_NONE)
            encode_operand(di, ii->dst1_type, ii->dst1_size, in, true, &num_dsts);
        if (ii->dst2_type != TYPE_NONE) {
            encode_operand(di, ii->dst2_type, ii->dst2_size, in,
                           !TEST(DECODE_4_SRCS, ii->flags),
                           TEST(DECODE_4_SRCS, ii->flags) ? &num_srcs : &num_dsts);
        }
        if (ii->src1_type != TYPE_NONE) {
            encode_operand(di, ii->src1_type, ii->src1_size, in,
                           TEST(DECODE_3_DSTS, ii->flags),
                           TEST(DECODE_3_DSTS, ii->flags) ? &num_dsts : &num_srcs);
        }
        if (ii->src2_type != TYPE_NONE)
            encode_operand(di, ii->src2_type, ii->src2_size, in, false, &num_srcs);
        if (ii->src3_type != TYPE_NONE)
            encode_operand(di, ii->src3_type, ii->src3_size, in, false, &num_srcs);
        ii = instr_info_extra_opnds(ii);
    } while (ii != NULL);
}

byte *
instr_encode_arch(dcontext_t *dcontext, instr_t *instr, byte *copy_pc, byte *final_pc,
                  bool check_reachable, bool *has_instr_opnds/*OUT OPTIONAL*/
                  _IF_DEBUG(bool assert_reachable))
{
    const instr_info_t * info;
    decode_info_t di;

    /* First, handle the already-encoded instructions */
    if (instr_raw_bits_valid(instr)) {
        CLIENT_ASSERT(check_reachable, "internal encode error: cannot encode raw "
                      "bits and ignore reachability");
        /* Copy raw bits, possibly re-relativizing */
        if (has_instr_opnds != NULL)
            *has_instr_opnds = false;
        return copy_and_re_relativize_raw_instr(dcontext, instr, copy_pc, final_pc);
    }
    if (instr_is_label(instr)) {
        if (has_instr_opnds != NULL)
            *has_instr_opnds = false;
        return copy_pc;
    }
    CLIENT_ASSERT(instr_operands_valid(instr), "instr_encode error: operands invalid");

    decode_info_init_for_instr(&di, instr);
    di.opcode = instr_get_opcode(instr);
    di.start_pc = copy_pc;
    di.final_pc = final_pc;
    di.cur_note = (ptr_int_t) instr->note;

    info = instr_get_instr_info(instr);
    if (info == NULL) {
        if (has_instr_opnds != NULL)
            *has_instr_opnds = false;
        return NULL;
    }

    while (!encoding_possible(&di, instr, info)) {
        LOG(THREAD, LOG_EMIT, ENC_LEVEL, "\tencoding for 0x%x no good...\n",
            info->opcode);
        info = get_next_instr_info(info);
        if (info == NULL || info->type == OP_CONTD) {
            /* Use the errmgs to try and give a more helpful message */
            SYSLOG_INTERNAL_ERROR(di.errmsg, di.errmsg_param);
            DOLOG(1, LOG_EMIT, {
                LOG(THREAD, LOG_EMIT, 1, "ERROR: Could not find encoding for: ");
                instr_disassemble(dcontext, instr, THREAD);
                LOG(THREAD, LOG_EMIT, 1, "\nReason: ");
                LOG(THREAD, LOG_EMIT, 1, di.errmsg, di.errmsg_param);
                LOG(THREAD, LOG_EMIT, 1, "\n");
            });
            CLIENT_ASSERT(false, "instr_encode error: no encoding found (see log)");
            return NULL;
        }
        /* We need to clear all the checking fields for each new template */
        memset(&di.reglist_sz, 0, sizeof(di) - offsetof(decode_info_t, reglist_sz));
    }

    /* Encode into di.instr_word */
    di.instr_word = info->opcode;
    if (TEST(DECODE_PREDICATE, info->flags)) {
        dr_pred_type_t pred = instr_get_predicate(instr);
        if (pred == DR_PRED_NONE)
            pred = DR_PRED_AL;
        di.instr_word |= (pred - DR_PRED_EQ) << 28;
    }
    encode_operands(&di, instr, info);

    *((uint *)copy_pc) = di.instr_word;
    if (has_instr_opnds != NULL)
        *has_instr_opnds = di.has_instr_opnds;
    return copy_pc + 4;
}

byte *
copy_and_re_relativize_raw_instr(dcontext_t *dcontext, instr_t *instr,
                                 byte *dst_pc, byte *final_pc)
{
    /* FIXME i#1551: NYI */
    ASSERT_NOT_REACHED();
    return NULL;
}
