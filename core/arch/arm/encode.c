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

/* FIXME i#1551: write ARM encoder */

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "decode.h"
#include "disassemble.h"
#include "decode_private.h"

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
    "cpsr", "spsr", "fpscr",
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
    DR_REG_CPSR, DR_REG_SPSR, DR_REG_FPSCR,
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
reg_check_reg_fixer(void)
{
    CLIENT_ASSERT(sizeof(dr_reg_fixer)/sizeof(dr_reg_fixer[0]) == REG_LAST_ENUM + 1,
                  "internal register enum error");
}
#endif

static opnd_size_t
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

static bool
encode_opnd_ok(decode_info_t *di, byte optype, opnd_size_t size_temp, instr_t *in,
               bool is_dst, uint *counter INOUT)
{
    uint opnum = (*counter)++;
    opnd_t opnd;
    opnd_size_t size_temp_up = resolve_size_upward(size_temp);
    opnd_size_t size_op, size_op_up;
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

    size_op = opnd_get_size(opnd);
    size_op_up = resolve_size_upward(size_op);

    switch (optype) {
    /* For registers, support requesting whole reg when only part is in template */
    case TYPE_R_A:
    case TYPE_R_B:
    case TYPE_R_C:
    case TYPE_R_D:
        return (opnd_is_reg(opnd) && reg_is_gpr(opnd_get_reg(opnd)) &&
                /* FIXME i#1551: ensure not a top-half GPR */
                (size_op == size_temp || size_op == size_temp_up));
    case TYPE_V_A:
    case TYPE_V_B:
    case TYPE_V_C:
    case TYPE_W_A:
    case TYPE_W_B:
    case TYPE_W_C:
        return (opnd_is_reg(opnd) && reg_is_simd(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up));
    }

    /* FIXME i#1551: add the rest of the types */

    return false;
}

bool
encoding_possible(decode_info_t *di, instr_t *in, const instr_info_t * ii)
{
    uint num_dsts = 0, num_srcs = 0;

    if (ii == NULL || in == NULL)
        return false;

    /* FIXME i#1551: check isa mode vs THUMB_ONLY or ARM_ONLY ii->flags */

    do {
        if (ii->dst1_type != TYPE_NONE) {
            if (!encode_opnd_ok(di, ii->dst1_type, ii->dst1_size, in, true, &num_dsts))
                return false;
        }
        if (ii->dst2_type != TYPE_NONE) {
            if (!encode_opnd_ok(di, ii->dst2_type, ii->dst2_size, in,
                                !TEST(DECODE_4_SRCS, ii->flags),
                                TEST(DECODE_4_SRCS, ii->flags) ? &num_srcs : &num_dsts))
                return false;
        }
        if (ii->src1_type != TYPE_NONE) {
            if (!encode_opnd_ok(di, ii->src1_type, ii->src1_size, in,
                                TEST(DECODE_3_DSTS, ii->flags),
                                TEST(DECODE_3_DSTS, ii->flags) ? &num_dsts : &num_srcs))
                return false;
        }
        if (ii->src2_type != TYPE_NONE) {
            if (!encode_opnd_ok(di, ii->src2_type, ii->src2_size, in, false, &num_srcs))
                return false;
        }
        if (ii->src3_type != TYPE_NONE) {
            if (!encode_opnd_ok(di, ii->src3_type, ii->src3_size, in, false, &num_srcs))
                return false;
        }
        ii = instr_info_extra_opnds(ii);
    } while (ii != NULL);

    return true;
}

void
decode_info_init_for_instr(decode_info_t *di, instr_t *instr)
{
    memset(di, 0, sizeof(*di));
    di->isa_mode = instr_get_isa_mode(instr);
}

byte *
instr_encode_ignore_reachability(dcontext_t *dcontext_t, instr_t *instr, byte *pc)
{
    /* FIXME i#1551: write ARM encoder */
    return NULL;
}

byte *
instr_encode_check_reachability(dcontext_t *dcontext_t, instr_t *instr, byte *pc,
                                bool *has_instr_opnds/*OUT OPTIONAL*/)
{
    /* FIXME i#1551: write ARM encoder */
    return NULL;
}

byte *
copy_and_re_relativize_raw_instr(dcontext_t *dcontext, instr_t *instr,
                                 byte *dst_pc, byte *final_pc)
{
    /* FIXME i#1551: write ARM encoder */
    return NULL;
}

byte *
instr_encode_to_copy(dcontext_t *dcontext, instr_t *instr, byte *copy_pc, byte *final_pc)
{
    /* FIXME i#1551: write ARM encoder */
    return NULL;
}

byte *
instr_encode(dcontext_t *dcontext, instr_t *instr, byte *pc)
{
    /* FIXME i#1551: write ARM encoder */
    /* FIXME: complain if DR_PRED_NONE set but only predicated encodings avail */
    return NULL;
}
