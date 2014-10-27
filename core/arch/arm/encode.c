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
    "r0_th",  "r1_th",  "r2_th",  "r3_th",   "r4_th",  "r5_th",  "r6_th",  "r7_th",
    "r8_th",  "r9_th",  "r10_th", "r11_th", "r12_th", "r13_th", "r14_th", "r15_th",
    "r16_th", "r17_th", "r18_th", "r19_th", "r20_th", "r21_th", "r22_th", "r23_th",
    "r24_th", "r25_th", "r26_th", "r27_th", "r28_th", "r29_th", "r30_th", "r31_th",
#ifndef X64
    "r0_bh",  "r1_bh",  "r2_bh",  "r3_bh",   "r4_bh",  "r5_bh",  "r6_bh",  "r7_bh",
    "r8_bh",  "r9_bh",  "r10_bh", "r11_bh", "r12_bh", "r13_bh", "r14_bh", "r15_bh",
#endif
    "r0_bb",  "r1_bb",  "r2_bb",  "r3_bb",   "r4_bb",  "r5_bb",  "r6_bb",  "r7_bb",
    "r8_bb",  "r9_bb",  "r10_bb", "r11_bb", "r12_bb", "r13_bb", "r14_bb", "r15_bb",
    "r16_bb", "r17_bb", "r18_bb", "r19_bb", "r20_bb", "r21_bb", "r22_bb", "r23_bb",
    "r24_bb", "r25_bb", "r26_bb", "r27_bb", "r28_bb", "r29_bb", "r30_bb", "r31_bb",
    "cpsr", "spsr",
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
    /* s0-s31 */
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
    /* h0-h31 */
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
    /* b0-b31 */
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
    /* top half */
    DR_REG_R0,  DR_REG_R1,   DR_REG_R2,   DR_REG_R3,
    DR_REG_R4,  DR_REG_R5,   DR_REG_R6,   DR_REG_R7,
    DR_REG_R8,  DR_REG_R9,   DR_REG_R10,  DR_REG_R11,
    DR_REG_R12, DR_REG_R13,  DR_REG_R14,  DR_REG_R15,
    /* x64-only but simpler code to not ifdef it */
    DR_REG_X16, DR_REG_X17,  DR_REG_X18,  DR_REG_X19,
    DR_REG_X20, DR_REG_X21,  DR_REG_X22,  DR_REG_X23,
    DR_REG_X24, DR_REG_X25,  DR_REG_X26,  DR_REG_X27,
    DR_REG_X28, DR_REG_X29,  DR_REG_X30,  DR_REG_X31,
#ifndef X64
    /* bottom half */
    DR_REG_R0,  DR_REG_R1,   DR_REG_R2,   DR_REG_R3,
    DR_REG_R4,  DR_REG_R5,   DR_REG_R6,   DR_REG_R7,
    DR_REG_R8,  DR_REG_R9,   DR_REG_R10,  DR_REG_R11,
    DR_REG_R12, DR_REG_R13,  DR_REG_R14,  DR_REG_R15,
#endif
    /* bottom byte */
    DR_REG_R0,  DR_REG_R1,   DR_REG_R2,   DR_REG_R3,
    DR_REG_R4,  DR_REG_R5,   DR_REG_R6,   DR_REG_R7,
    DR_REG_R8,  DR_REG_R9,   DR_REG_R10,  DR_REG_R11,
    DR_REG_R12, DR_REG_R13,  DR_REG_R14,  DR_REG_R15,
    /* x64-only but simpler code to not ifdef it */
    DR_REG_X16, DR_REG_X17,  DR_REG_X18,  DR_REG_X19,
    DR_REG_X20, DR_REG_X21,  DR_REG_X22,  DR_REG_X23,
    DR_REG_X24, DR_REG_X25,  DR_REG_X26,  DR_REG_X27,
    DR_REG_X28, DR_REG_X29,  DR_REG_X30,  DR_REG_X31,
    DR_REG_CPSR, DR_REG_SPSR,
};

#ifdef DEBUG
void
reg_check_reg_fixer(void)
{
    CLIENT_ASSERT(sizeof(dr_reg_fixer)/sizeof(dr_reg_fixer[0]) == REG_LAST_ENUM + 1,
                  "internal register enum error");
}
#endif

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

const instr_info_t *
get_encoding_info(instr_t *instr)
{
    /* FIXME i#1551: write ARM encoder */
    /* Probably we can share this same routine from x86/encode.c, esp
     * if we make isa_mode generic, and then just have
     * encoding_possible() be arch-specific.
     */
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
