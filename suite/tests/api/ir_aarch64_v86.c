/* **********************************************************
 * Copyright (c) 2023 ARM Limited. All rights reserved.
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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Define DR_FAST_IR to verify that everything compiles when we call the inline
 * versions of these routines.
 */
#ifndef STANDALONE_DECODER
#    define DR_FAST_IR 1
#endif

/* Uses the DR API, using DR as a standalone library, rather than
 * being a client library working with DR on a target program.
 */

#include "configure.h"
#include "dr_api.h"
#include "tools.h"

#include "ir_aarch64.h"

TEST_INSTR(bfcvt)
{
    /* Testing BFCVT   <Hd>, <Sn> */
    const char *const expected_0_0[6] = {
        "bfcvt  %s0 -> %h0",   "bfcvt  %s6 -> %h5",   "bfcvt  %s11 -> %h10",
        "bfcvt  %s17 -> %h16", "bfcvt  %s22 -> %h21", "bfcvt  %s31 -> %h31",
    };
    TEST_LOOP(bfcvt, bfcvt, 6, expected_0_0[i], opnd_create_reg(Vdn_h_six_offset_0[i]),
              opnd_create_reg(Vdn_s_six_offset_1[i]));
}

TEST_INSTR(bfcvtn2_vector)
{
    /* Testing BFCVTN2 <Hd>.8H, <Sn>.4S */
    const char *const expected_0_0[6] = {
        "bfcvtn2 %q0 $0x02 -> %q0",   "bfcvtn2 %q6 $0x02 -> %q5",
        "bfcvtn2 %q11 $0x02 -> %q10", "bfcvtn2 %q17 $0x02 -> %q16",
        "bfcvtn2 %q22 $0x02 -> %q21", "bfcvtn2 %q31 $0x02 -> %q31",
    };
    TEST_LOOP(bfcvtn2, bfcvtn2_vector, 6, expected_0_0[i],
              opnd_create_reg(Vdn_q_six_offset_0[i]),
              opnd_create_reg(Vdn_q_six_offset_1[i]));
}

TEST_INSTR(bfcvtn_vector)
{
    /* Testing BFCVTN  <Hd>.4H, <Sn>.4S */
    const char *const expected_0_0[6] = {
        "bfcvtn %q0 $0x02 -> %d0",   "bfcvtn %q6 $0x02 -> %d5",
        "bfcvtn %q11 $0x02 -> %d10", "bfcvtn %q17 $0x02 -> %d16",
        "bfcvtn %q22 $0x02 -> %d21", "bfcvtn %q31 $0x02 -> %d31",
    };
    TEST_LOOP(bfcvtn, bfcvtn_vector, 6, expected_0_0[i],
              opnd_create_reg(Vdn_d_six_offset_0[i]),
              opnd_create_reg(Vdn_q_six_offset_1[i]));
}

TEST_INSTR(bfdot_vector)
{
    /* Testing BFDOT   <Sd>.<Ts>, <Hn>.<Tb>, <Hm>.<Tb> */
    const char *const expected_0_0[6] = {
        "bfdot  %d0 %d0 %d0 $0x01 -> %d0",     "bfdot  %d5 %d6 %d7 $0x01 -> %d5",
        "bfdot  %d10 %d11 %d12 $0x01 -> %d10", "bfdot  %d16 %d17 %d18 $0x01 -> %d16",
        "bfdot  %d21 %d22 %d23 $0x01 -> %d21", "bfdot  %d31 %d31 %d31 $0x01 -> %d31",
    };
    TEST_LOOP(
        bfdot, bfdot_vector, 6, expected_0_0[i], opnd_create_reg(Vdn_d_six_offset_0[i]),
        opnd_create_reg(Vdn_d_six_offset_1[i]), opnd_create_reg(Vdn_d_six_offset_2[i]));

    const char *const expected_0_1[6] = {
        "bfdot  %q0 %q0 %q0 $0x01 -> %q0",     "bfdot  %q5 %q6 %q7 $0x01 -> %q5",
        "bfdot  %q10 %q11 %q12 $0x01 -> %q10", "bfdot  %q16 %q17 %q18 $0x01 -> %q16",
        "bfdot  %q21 %q22 %q23 $0x01 -> %q21", "bfdot  %q31 %q31 %q31 $0x01 -> %q31",
    };
    TEST_LOOP(
        bfdot, bfdot_vector, 6, expected_0_1[i], opnd_create_reg(Vdn_q_six_offset_0[i]),
        opnd_create_reg(Vdn_q_six_offset_1[i]), opnd_create_reg(Vdn_q_six_offset_2[i]));
}

TEST_INSTR(bfdot_vector_idx)
{
    /* Testing BFDOT   <Sd>.<Ts>, <Hn>.<Tb>, <Hm>.2H[<index>] */
    static const uint index_0_0[6] = { 0, 3, 0, 1, 2, 3 };
    const char *const expected_0_0[6] = {
        "bfdot  %d0 %d0 %q0 $0x00 $0x01 -> %d0",
        "bfdot  %d5 %d6 %q7 $0x03 $0x01 -> %d5",
        "bfdot  %d10 %d11 %q12 $0x00 $0x01 -> %d10",
        "bfdot  %d16 %d17 %q18 $0x01 $0x01 -> %d16",
        "bfdot  %d21 %d22 %q23 $0x02 $0x01 -> %d21",
        "bfdot  %d31 %d31 %q31 $0x03 $0x01 -> %d31",
    };
    TEST_LOOP(bfdot, bfdot_vector_idx, 6, expected_0_0[i],
              opnd_create_reg(Vdn_d_six_offset_0[i]),
              opnd_create_reg(Vdn_d_six_offset_1[i]),
              opnd_create_reg(Vdn_q_six_offset_2[i]),
              opnd_create_immed_uint(index_0_0[i], OPSZ_2b));

    const char *const expected_0_1[6] = {
        "bfdot  %q0 %q0 %q0 $0x00 $0x01 -> %q0",
        "bfdot  %q5 %q6 %q7 $0x03 $0x01 -> %q5",
        "bfdot  %q10 %q11 %q12 $0x00 $0x01 -> %q10",
        "bfdot  %q16 %q17 %q18 $0x01 $0x01 -> %q16",
        "bfdot  %q21 %q22 %q23 $0x02 $0x01 -> %q21",
        "bfdot  %q31 %q31 %q31 $0x03 $0x01 -> %q31",
    };
    TEST_LOOP(bfdot, bfdot_vector_idx, 6, expected_0_1[i],
              opnd_create_reg(Vdn_q_six_offset_0[i]),
              opnd_create_reg(Vdn_q_six_offset_1[i]),
              opnd_create_reg(Vdn_q_six_offset_2[i]),
              opnd_create_immed_uint(index_0_0[i], OPSZ_2b));
}

TEST_INSTR(bfmlalb_vector)
{
    /* Testing BFMLALB <Sd>.4S, <Hn>.8H, <Hm>.8H */
    const char *const expected_0_0[6] = {
        "bfmlalb %q0 %q0 %q0 $0x01 -> %q0",     "bfmlalb %q5 %q6 %q7 $0x01 -> %q5",
        "bfmlalb %q10 %q11 %q12 $0x01 -> %q10", "bfmlalb %q16 %q17 %q18 $0x01 -> %q16",
        "bfmlalb %q21 %q22 %q23 $0x01 -> %q21", "bfmlalb %q31 %q31 %q31 $0x01 -> %q31",
    };
    TEST_LOOP(bfmlalb, bfmlalb_vector, 6, expected_0_0[i],
              opnd_create_reg(Vdn_q_six_offset_0[i]),
              opnd_create_reg(Vdn_q_six_offset_1[i]),
              opnd_create_reg(Vdn_q_six_offset_2[i]));
}

TEST_INSTR(bfmlalb_vector_idx)
{
    /* Testing BFMLALB <Sd>.4S, <Hn>.8H, <Hm>.H[<index>] */
    static const reg_id_t Rm_0_0[6] = { DR_REG_Q0,  DR_REG_Q4,  DR_REG_Q7,
                                        DR_REG_Q10, DR_REG_Q12, DR_REG_Q15 };
    static const uint index_0_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_0_0[6] = {
        "bfmlalb %q0 %q0 %q0 $0x00 $0x01 -> %q0",
        "bfmlalb %q5 %q6 %q4 $0x04 $0x01 -> %q5",
        "bfmlalb %q10 %q11 %q7 $0x05 $0x01 -> %q10",
        "bfmlalb %q16 %q17 %q10 $0x07 $0x01 -> %q16",
        "bfmlalb %q21 %q22 %q12 $0x00 $0x01 -> %q21",
        "bfmlalb %q31 %q31 %q15 $0x07 $0x01 -> %q31",
    };
    TEST_LOOP(bfmlalb, bfmlalb_vector_idx, 6, expected_0_0[i],
              opnd_create_reg(Vdn_q_six_offset_0[i]),
              opnd_create_reg(Vdn_q_six_offset_1[i]), opnd_create_reg(Rm_0_0[i]),
              opnd_create_immed_uint(index_0_0[i], OPSZ_3b));
}

TEST_INSTR(bfmlalt_vector)
{
    /* Testing BFMLALT <Sd>.4S, <Hn>.8H, <Hm>.8H */
    const char *const expected_0_0[6] = {
        "bfmlalt %q0 %q0 %q0 $0x01 -> %q0",     "bfmlalt %q5 %q6 %q7 $0x01 -> %q5",
        "bfmlalt %q10 %q11 %q12 $0x01 -> %q10", "bfmlalt %q16 %q17 %q18 $0x01 -> %q16",
        "bfmlalt %q21 %q22 %q23 $0x01 -> %q21", "bfmlalt %q31 %q31 %q31 $0x01 -> %q31",
    };
    TEST_LOOP(bfmlalt, bfmlalt_vector, 6, expected_0_0[i],
              opnd_create_reg(Vdn_q_six_offset_0[i]),
              opnd_create_reg(Vdn_q_six_offset_1[i]),
              opnd_create_reg(Vdn_q_six_offset_2[i]));
}

TEST_INSTR(bfmlalt_vector_idx)
{
    /* Testing BFMLALT <Sd>.4S, <Hn>.8H, <Hm>.H[<index>] */
    static const reg_id_t Rm_0_0[6] = { DR_REG_Q0,  DR_REG_Q4,  DR_REG_Q7,
                                        DR_REG_Q10, DR_REG_Q12, DR_REG_Q15 };
    static const uint index_0_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_0_0[6] = {
        "bfmlalt %q0 %q0 %q0 $0x00 $0x01 -> %q0",
        "bfmlalt %q5 %q6 %q4 $0x04 $0x01 -> %q5",
        "bfmlalt %q10 %q11 %q7 $0x05 $0x01 -> %q10",
        "bfmlalt %q16 %q17 %q10 $0x07 $0x01 -> %q16",
        "bfmlalt %q21 %q22 %q12 $0x00 $0x01 -> %q21",
        "bfmlalt %q31 %q31 %q15 $0x07 $0x01 -> %q31",
    };
    TEST_LOOP(bfmlalt, bfmlalt_vector_idx, 6, expected_0_0[i],
              opnd_create_reg(Vdn_q_six_offset_0[i]),
              opnd_create_reg(Vdn_q_six_offset_1[i]), opnd_create_reg(Rm_0_0[i]),
              opnd_create_immed_uint(index_0_0[i], OPSZ_3b));
}

TEST_INSTR(bfmmla_vector)
{
    /* Testing BFMMLA  <Sd>.4S, <Hn>.8H, <Hm>.8H */
    const char *const expected_0_0[6] = {
        "bfmmla %q0 %q0 %q0 $0x01 -> %q0",     "bfmmla %q5 %q6 %q7 $0x01 -> %q5",
        "bfmmla %q10 %q11 %q12 $0x01 -> %q10", "bfmmla %q16 %q17 %q18 $0x01 -> %q16",
        "bfmmla %q21 %q22 %q23 $0x01 -> %q21", "bfmmla %q31 %q31 %q31 $0x01 -> %q31",
    };
    TEST_LOOP(
        bfmmla, bfmmla_vector, 6, expected_0_0[i], opnd_create_reg(Vdn_q_six_offset_0[i]),
        opnd_create_reg(Vdn_q_six_offset_1[i]), opnd_create_reg(Vdn_q_six_offset_2[i]));
}

TEST_INSTR(smmla_vector)
{
    /* Testing SMMLA   <Vd>.4S, <Vn>.16B, <Vm>.16B */
    const char *const expected_0_0[6] = {
        "smmla  %q0 %q0 %q0 $0x00 -> %q0",     "smmla  %q5 %q6 %q7 $0x00 -> %q5",
        "smmla  %q10 %q11 %q12 $0x00 -> %q10", "smmla  %q16 %q17 %q18 $0x00 -> %q16",
        "smmla  %q21 %q22 %q23 $0x00 -> %q21", "smmla  %q31 %q31 %q31 $0x00 -> %q31",
    };
    TEST_LOOP(
        smmla, smmla_vector, 6, expected_0_0[i], opnd_create_reg(Vdn_q_six_offset_0[i]),
        opnd_create_reg(Vdn_q_six_offset_1[i]), opnd_create_reg(Vdn_q_six_offset_2[i]));
}

TEST_INSTR(sudot_vector_idx)
{
    /* Testing SUDOT   <Vd>.<Ts>, <Vn>.<Tb>, <Vm>.4B[<index>] */
    static const uint index_0_0[6] = { 0, 3, 0, 1, 2, 3 };
    const char *const expected_0_0[6] = {
        "sudot  %d0 %d0 %q0 $0x00 $0x00 -> %d0",
        "sudot  %d5 %d6 %q7 $0x03 $0x00 -> %d5",
        "sudot  %d10 %d11 %q12 $0x00 $0x00 -> %d10",
        "sudot  %d16 %d17 %q18 $0x01 $0x00 -> %d16",
        "sudot  %d21 %d22 %q23 $0x02 $0x00 -> %d21",
        "sudot  %d31 %d31 %q31 $0x03 $0x00 -> %d31",
    };
    TEST_LOOP(sudot, sudot_vector_idx, 6, expected_0_0[i],
              opnd_create_reg(Vdn_d_six_offset_0[i]),
              opnd_create_reg(Vdn_d_six_offset_1[i]),
              opnd_create_reg(Vdn_q_six_offset_2[i]),
              opnd_create_immed_uint(index_0_0[i], OPSZ_2b));

    const char *const expected_0_1[6] = {
        "sudot  %q0 %q0 %q0 $0x00 $0x00 -> %q0",
        "sudot  %q5 %q6 %q7 $0x03 $0x00 -> %q5",
        "sudot  %q10 %q11 %q12 $0x00 $0x00 -> %q10",
        "sudot  %q16 %q17 %q18 $0x01 $0x00 -> %q16",
        "sudot  %q21 %q22 %q23 $0x02 $0x00 -> %q21",
        "sudot  %q31 %q31 %q31 $0x03 $0x00 -> %q31",
    };
    TEST_LOOP(sudot, sudot_vector_idx, 6, expected_0_1[i],
              opnd_create_reg(Vdn_q_six_offset_0[i]),
              opnd_create_reg(Vdn_q_six_offset_1[i]),
              opnd_create_reg(Vdn_q_six_offset_2[i]),
              opnd_create_immed_uint(index_0_0[i], OPSZ_2b));
}

TEST_INSTR(ummla_vector)
{
    /* Testing UMMLA   <Vd>.4S, <Vn>.16B, <Vm>.16B */
    const char *const expected_0_0[6] = {
        "ummla  %q0 %q0 %q0 $0x00 -> %q0",     "ummla  %q5 %q6 %q7 $0x00 -> %q5",
        "ummla  %q10 %q11 %q12 $0x00 -> %q10", "ummla  %q16 %q17 %q18 $0x00 -> %q16",
        "ummla  %q21 %q22 %q23 $0x00 -> %q21", "ummla  %q31 %q31 %q31 $0x00 -> %q31",
    };
    TEST_LOOP(
        ummla, ummla_vector, 6, expected_0_0[i], opnd_create_reg(Vdn_q_six_offset_0[i]),
        opnd_create_reg(Vdn_q_six_offset_1[i]), opnd_create_reg(Vdn_q_six_offset_2[i]));
}

TEST_INSTR(usmmla_vector)
{
    /* Testing USMMLA  <Vd>.4S, <Vn>.16B, <Vm>.16B */
    const char *const expected_0_0[6] = {
        "usmmla %q0 %q0 %q0 $0x00 -> %q0",     "usmmla %q5 %q6 %q7 $0x00 -> %q5",
        "usmmla %q10 %q11 %q12 $0x00 -> %q10", "usmmla %q16 %q17 %q18 $0x00 -> %q16",
        "usmmla %q21 %q22 %q23 $0x00 -> %q21", "usmmla %q31 %q31 %q31 $0x00 -> %q31",
    };
    TEST_LOOP(
        usmmla, usmmla_vector, 6, expected_0_0[i], opnd_create_reg(Vdn_q_six_offset_0[i]),
        opnd_create_reg(Vdn_q_six_offset_1[i]), opnd_create_reg(Vdn_q_six_offset_2[i]));
}

TEST_INSTR(usdot_vector)
{
    /* Testing USDOT   <Vd>.<Ts>, <Vn>.<Tb>, <Vm>.<Tb> */
    const char *const expected_0_0[6] = {
        "usdot  %d0 %d0 %d0 $0x00 -> %d0",     "usdot  %d5 %d6 %d7 $0x00 -> %d5",
        "usdot  %d10 %d11 %d12 $0x00 -> %d10", "usdot  %d16 %d17 %d18 $0x00 -> %d16",
        "usdot  %d21 %d22 %d23 $0x00 -> %d21", "usdot  %d31 %d31 %d31 $0x00 -> %d31",
    };
    TEST_LOOP(
        usdot, usdot_vector, 6, expected_0_0[i], opnd_create_reg(Vdn_d_six_offset_0[i]),
        opnd_create_reg(Vdn_d_six_offset_1[i]), opnd_create_reg(Vdn_d_six_offset_2[i]));

    const char *const expected_0_1[6] = {
        "usdot  %q0 %q0 %q0 $0x00 -> %q0",     "usdot  %q5 %q6 %q7 $0x00 -> %q5",
        "usdot  %q10 %q11 %q12 $0x00 -> %q10", "usdot  %q16 %q17 %q18 $0x00 -> %q16",
        "usdot  %q21 %q22 %q23 $0x00 -> %q21", "usdot  %q31 %q31 %q31 $0x00 -> %q31",
    };
    TEST_LOOP(
        usdot, usdot_vector, 6, expected_0_1[i], opnd_create_reg(Vdn_q_six_offset_0[i]),
        opnd_create_reg(Vdn_q_six_offset_1[i]), opnd_create_reg(Vdn_q_six_offset_2[i]));
}

TEST_INSTR(usdot_vector_idx)
{
    /* Testing USDOT   <Vd>.<Ts>, <Vn>.<Tb>, <Vm>.4B[<index>] */
    static const uint index_0_0[6] = { 0, 3, 0, 1, 2, 3 };
    const char *const expected_0_0[6] = {
        "usdot  %d0 %d0 %q0 $0x00 $0x00 -> %d0",
        "usdot  %d5 %d6 %q7 $0x03 $0x00 -> %d5",
        "usdot  %d10 %d11 %q12 $0x00 $0x00 -> %d10",
        "usdot  %d16 %d17 %q18 $0x01 $0x00 -> %d16",
        "usdot  %d21 %d22 %q23 $0x02 $0x00 -> %d21",
        "usdot  %d31 %d31 %q31 $0x03 $0x00 -> %d31",
    };
    TEST_LOOP(usdot, usdot_vector_idx, 6, expected_0_0[i],
              opnd_create_reg(Vdn_d_six_offset_0[i]),
              opnd_create_reg(Vdn_d_six_offset_1[i]),
              opnd_create_reg(Vdn_q_six_offset_2[i]),
              opnd_create_immed_uint(index_0_0[i], OPSZ_2b));

    const char *const expected_0_1[6] = {
        "usdot  %q0 %q0 %q0 $0x00 $0x00 -> %q0",
        "usdot  %q5 %q6 %q7 $0x03 $0x00 -> %q5",
        "usdot  %q10 %q11 %q12 $0x00 $0x00 -> %q10",
        "usdot  %q16 %q17 %q18 $0x01 $0x00 -> %q16",
        "usdot  %q21 %q22 %q23 $0x02 $0x00 -> %q21",
        "usdot  %q31 %q31 %q31 $0x03 $0x00 -> %q31",
    };
    TEST_LOOP(usdot, usdot_vector_idx, 6, expected_0_1[i],
              opnd_create_reg(Vdn_q_six_offset_0[i]),
              opnd_create_reg(Vdn_q_six_offset_1[i]),
              opnd_create_reg(Vdn_q_six_offset_2[i]),
              opnd_create_immed_uint(index_0_0[i], OPSZ_2b));
}

TEST_INSTR(ldg)
{
    /* Testing LDG     <Xt>, [<Xn|SP>, #<simm>] */
    static const int imm9[6] = { -4096, -2704, -1344, 32, 1392, 4080 };
    const char *const expected[6] = {
        "ldg    %x0 -0x1000(%x0) -> %x0",    "ldg    %x5 -0x0a90(%x6) -> %x5",
        "ldg    %x10 -0x0540(%x11) -> %x10", "ldg    %x15 +0x20(%x16) -> %x15",
        "ldg    %x20 +0x0570(%x21) -> %x20", "ldg    %x30 +0x0ff0(%sp) -> %x30",
    };
    TEST_LOOP(ldg, ldg, 6, expected[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_base_disp_aarch64(Xn_six_offset_1_sp[i], DR_REG_NULL,
                                            DR_EXTEND_UXTX, 0, imm9[i], 0, OPSZ_0));
}

TEST_INSTR(st2g)
{
    /* Testing ST2G    <Xt|SP>, [<Xn|SP>], #<simm> */
    static const int imm9[6] = { -4096, -2704, -1344, 32, 1392, 4080 };
    const char *const expected_0[6] = {
        "st2g   %x0 %x0 $0xfffffffffffff000 -> (%x0) %x0",
        "st2g   %x6 %x5 $0xfffffffffffff570 -> (%x5) %x5",
        "st2g   %x11 %x10 $0xfffffffffffffac0 -> (%x10) %x10",
        "st2g   %x16 %x15 $0x0000000000000020 -> (%x15) %x15",
        "st2g   %x21 %x20 $0x0000000000000570 -> (%x20) %x20",
        "st2g   %sp %sp $0x0000000000000ff0 -> (%sp) %sp",
    };
    TEST_LOOP(st2g, st2g_post, 6, expected_0[i],
              opnd_create_base_disp_aarch64(Xn_six_offset_0_sp[i], DR_REG_NULL,
                                            DR_EXTEND_UXTX, 0, 0, 0, OPSZ_0),
              opnd_create_reg(Xn_six_offset_1_sp[i]), OPND_CREATE_INT(imm9[i]));

    /* Testing ST2G    <Xt|SP>, [<Xn|SP>, #<simm>]! */
    const char *const expected_1[6] = {
        "st2g   %x0 %x0 $0xfffffffffffff000 -> -0x1000(%x0) %x0",
        "st2g   %x6 %x5 $0xfffffffffffff570 -> -0x0a90(%x5) %x5",
        "st2g   %x11 %x10 $0xfffffffffffffac0 -> -0x0540(%x10) %x10",
        "st2g   %x16 %x15 $0x0000000000000020 -> +0x20(%x15) %x15",
        "st2g   %x21 %x20 $0x0000000000000570 -> +0x0570(%x20) %x20",
        "st2g   %sp %sp $0x0000000000000ff0 -> +0x0ff0(%sp) %sp",
    };
    TEST_LOOP(
        st2g, st2g_pre, 6, expected_1[i],
        opnd_create_base_disp(Xn_six_offset_0_sp[i], DR_REG_NULL, 0, imm9[i], OPSZ_0),
        opnd_create_reg(Xn_six_offset_1_sp[i]));

    //    /* Testing ST2G    <Xt|SP>, [<Xn|SP>, #<simm>] */
    const char *const expected_2[6] = {
        "st2g   %x0 -> -0x1000(%x0)",   "st2g   %x6 -> -0x0a90(%x5)",
        "st2g   %x11 -> -0x0540(%x10)", "st2g   %x16 -> +0x20(%x15)",
        "st2g   %x21 -> +0x0570(%x20)", "st2g   %sp -> +0x0ff0(%sp)",
    };
    TEST_LOOP(st2g, st2g_offset, 6, expected_2[i],
              opnd_create_base_disp_aarch64(Xn_six_offset_0_sp[i], DR_REG_NULL,
                                            DR_EXTEND_UXTX, 0, imm9[i], 0, OPSZ_0),
              opnd_create_reg(Xn_six_offset_1_sp[i]));
}

TEST_INSTR(stg)
{
    /* Testing STG     <Xt|SP>, [<Xn|SP>], #<simm> */
    static const int imm9[6] = { -4096, -2704, -1344, 32, 1392, 4080 };
    const char *const expected_0[6] = {
        "stg    %x0 %x0 $0xfffffffffffff000 -> (%x0) %x0",
        "stg    %x6 %x5 $0xfffffffffffff570 -> (%x5) %x5",
        "stg    %x11 %x10 $0xfffffffffffffac0 -> (%x10) %x10",
        "stg    %x16 %x15 $0x0000000000000020 -> (%x15) %x15",
        "stg    %x21 %x20 $0x0000000000000570 -> (%x20) %x20",
        "stg    %sp %sp $0x0000000000000ff0 -> (%sp) %sp",
    };
    TEST_LOOP(stg, stg_post, 6, expected_0[i],
              opnd_create_base_disp_aarch64(Xn_six_offset_0_sp[i], DR_REG_NULL,
                                            DR_EXTEND_UXTX, 0, 0, 0, OPSZ_0),
              opnd_create_reg(Xn_six_offset_1_sp[i]), OPND_CREATE_INT(imm9[i]));

    /* Testing STG     <Xt|SP>, [<Xn|SP>, #<simm>]! */
    const char *const expected_1[6] = {
        "stg    %x0 %x0 $0xfffffffffffff000 -> -0x1000(%x0) %x0",
        "stg    %x6 %x5 $0xfffffffffffff570 -> -0x0a90(%x5) %x5",
        "stg    %x11 %x10 $0xfffffffffffffac0 -> -0x0540(%x10) %x10",
        "stg    %x16 %x15 $0x0000000000000020 -> +0x20(%x15) %x15",
        "stg    %x21 %x20 $0x0000000000000570 -> +0x0570(%x20) %x20",
        "stg    %sp %sp $0x0000000000000ff0 -> +0x0ff0(%sp) %sp",
    };
    TEST_LOOP(
        stg, stg_pre, 6, expected_1[i],
        opnd_create_base_disp(Xn_six_offset_0_sp[i], DR_REG_NULL, 0, imm9[i], OPSZ_0),
        opnd_create_reg(Xn_six_offset_1_sp[i]));

    /* Testing STG     <Xt|SP>, [<Xn|SP>, #<simm>] */
    const char *const expected_2[6] = {
        "stg    %x0 -> -0x1000(%x0)",   "stg    %x6 -> -0x0a90(%x5)",
        "stg    %x11 -> -0x0540(%x10)", "stg    %x16 -> +0x20(%x15)",
        "stg    %x21 -> +0x0570(%x20)", "stg    %sp -> +0x0ff0(%sp)",
    };
    TEST_LOOP(stg, stg_offset, 6, expected_2[i],
              opnd_create_base_disp_aarch64(Xn_six_offset_0_sp[i], DR_REG_NULL,
                                            DR_EXTEND_UXTX, 0, imm9[i], 0, OPSZ_0),
              opnd_create_reg(Xn_six_offset_1_sp[i]));
}

TEST_INSTR(stz2g)
{
    /* Testing STZ2G   <Xt|SP>, [<Xn|SP>], #<simm> */
    static const int imm9[6] = { -4096, -2704, -1344, 32, 1392, 4080 };
    const char *const expected_0[6] = {
        "stz2g  %x0 %x0 $0xfffffffffffff000 -> (%x0)[32byte] %x0",
        "stz2g  %x6 %x5 $0xfffffffffffff570 -> (%x5)[32byte] %x5",
        "stz2g  %x11 %x10 $0xfffffffffffffac0 -> (%x10)[32byte] %x10",
        "stz2g  %x16 %x15 $0x0000000000000020 -> (%x15)[32byte] %x15",
        "stz2g  %x21 %x20 $0x0000000000000570 -> (%x20)[32byte] %x20",
        "stz2g  %sp %sp $0x0000000000000ff0 -> (%sp)[32byte] %sp",
    };
    TEST_LOOP(stz2g, stz2g_post, 6, expected_0[i],
              opnd_create_base_disp_aarch64(Xn_six_offset_0_sp[i], DR_REG_NULL,
                                            DR_EXTEND_UXTX, 0, 0, 0, OPSZ_32),
              opnd_create_reg(Xn_six_offset_1_sp[i]), OPND_CREATE_INT(imm9[i]));

    /* Testing STZ2G   <Xt|SP>, [<Xn|SP>, #<simm>]! */
    const char *const expected_1[6] = {
        "stz2g  %x0 %x0 $0xfffffffffffff000 -> -0x1000(%x0)[32byte] %x0",
        "stz2g  %x6 %x5 $0xfffffffffffff570 -> -0x0a90(%x5)[32byte] %x5",
        "stz2g  %x11 %x10 $0xfffffffffffffac0 -> -0x0540(%x10)[32byte] %x10",
        "stz2g  %x16 %x15 $0x0000000000000020 -> +0x20(%x15)[32byte] %x15",
        "stz2g  %x21 %x20 $0x0000000000000570 -> +0x0570(%x20)[32byte] %x20",
        "stz2g  %sp %sp $0x0000000000000ff0 -> +0x0ff0(%sp)[32byte] %sp",
    };
    TEST_LOOP(
        stz2g, stz2g_pre, 6, expected_1[i],
        opnd_create_base_disp(Xn_six_offset_0_sp[i], DR_REG_NULL, 0, imm9[i], OPSZ_32),
        opnd_create_reg(Xn_six_offset_1_sp[i]));

    /* Testing STZ2G   <Xt|SP>, [<Xn|SP>, #<simm>] */
    const char *const expected_2[6] = {
        "stz2g  %x0 -> -0x1000(%x0)[32byte]",   "stz2g  %x6 -> -0x0a90(%x5)[32byte]",
        "stz2g  %x11 -> -0x0540(%x10)[32byte]", "stz2g  %x16 -> +0x20(%x15)[32byte]",
        "stz2g  %x21 -> +0x0570(%x20)[32byte]", "stz2g  %sp -> +0x0ff0(%sp)[32byte]",
    };
    TEST_LOOP(stz2g, stz2g_offset, 6, expected_2[i],
              opnd_create_base_disp_aarch64(Xn_six_offset_0_sp[i], DR_REG_NULL,
                                            DR_EXTEND_UXTX, 0, imm9[i], 0, OPSZ_32),
              opnd_create_reg(Xn_six_offset_1_sp[i]));
}

TEST_INSTR(stzg)
{
    /* Testing STZG    <Xt|SP>, [<Xn|SP>], #<simm> */
    static const int imm9[6] = { -4096, -2704, -1344, 32, 1392, 4080 };
    const char *const expected_0[6] = {
        "stzg   %x0 %x0 $0xfffffffffffff000 -> (%x0)[16byte] %x0",
        "stzg   %x6 %x5 $0xfffffffffffff570 -> (%x5)[16byte] %x5",
        "stzg   %x11 %x10 $0xfffffffffffffac0 -> (%x10)[16byte] %x10",
        "stzg   %x16 %x15 $0x0000000000000020 -> (%x15)[16byte] %x15",
        "stzg   %x21 %x20 $0x0000000000000570 -> (%x20)[16byte] %x20",
        "stzg   %sp %sp $0x0000000000000ff0 -> (%sp)[16byte] %sp",
    };
    TEST_LOOP(stzg, stzg_post, 6, expected_0[i],
              opnd_create_base_disp_aarch64(Xn_six_offset_0_sp[i], DR_REG_NULL,
                                            DR_EXTEND_UXTX, 0, 0, 0, OPSZ_16),
              opnd_create_reg(Xn_six_offset_1_sp[i]), OPND_CREATE_INT(imm9[i]));

    /* Testing STZG    <Xt|SP>, [<Xn|SP>, #<simm>]! */
    const char *const expected_1[6] = {
        "stzg   %x0 %x0 $0xfffffffffffff000 -> -0x1000(%x0)[16byte] %x0",
        "stzg   %x6 %x5 $0xfffffffffffff570 -> -0x0a90(%x5)[16byte] %x5",
        "stzg   %x11 %x10 $0xfffffffffffffac0 -> -0x0540(%x10)[16byte] %x10",
        "stzg   %x16 %x15 $0x0000000000000020 -> +0x20(%x15)[16byte] %x15",
        "stzg   %x21 %x20 $0x0000000000000570 -> +0x0570(%x20)[16byte] %x20",
        "stzg   %sp %sp $0x0000000000000ff0 -> +0x0ff0(%sp)[16byte] %sp",
    };
    TEST_LOOP(
        stzg, stzg_pre, 6, expected_1[i],
        opnd_create_base_disp(Xn_six_offset_0_sp[i], DR_REG_NULL, 0, imm9[i], OPSZ_16),
        opnd_create_reg(Xn_six_offset_1_sp[i]));

    /* Testing STZG    <Xt|SP>, [<Xn|SP>, #<simm>] */
    const char *const expected_2[6] = {
        "stzg   %x0 -> -0x1000(%x0)[16byte]",   "stzg   %x6 -> -0x0a90(%x5)[16byte]",
        "stzg   %x11 -> -0x0540(%x10)[16byte]", "stzg   %x16 -> +0x20(%x15)[16byte]",
        "stzg   %x21 -> +0x0570(%x20)[16byte]", "stzg   %sp -> +0x0ff0(%sp)[16byte]",
    };
    TEST_LOOP(stzg, stzg_offset, 6, expected_2[i],
              opnd_create_base_disp_aarch64(Xn_six_offset_0_sp[i], DR_REG_NULL,
                                            DR_EXTEND_UXTX, 0, imm9[i], 0, OPSZ_16),
              opnd_create_reg(Xn_six_offset_1_sp[i]));
}

TEST_INSTR(stgp)
{
    /* Testing STGP    <Xt>, <Xt2>, [<Xn|SP>], #<simm> */
    static const int imm7[6] = { -1024, -640, -304, 48, 384, 1008 };
    const char *const expected_0_0[6] = {
        "stgp   %x0 %x0 %x0 $0xfffffffffffffc00 -> (%x0)[16byte] %x0",
        "stgp   %x6 %x7 %x5 $0xfffffffffffffd80 -> (%x5)[16byte] %x5",
        "stgp   %x11 %x12 %x10 $0xfffffffffffffed0 -> (%x10)[16byte] %x10",
        "stgp   %x16 %x17 %x15 $0x0000000000000030 -> (%x15)[16byte] %x15",
        "stgp   %x21 %x22 %x20 $0x0000000000000180 -> (%x20)[16byte] %x20",
        "stgp   %x30 %x30 %sp $0x00000000000003f0 -> (%sp)[16byte] %sp",
    };
    TEST_LOOP(stgp, stgp_post, 6, expected_0_0[i],
              opnd_create_base_disp_aarch64(Xn_six_offset_0_sp[i], DR_REG_NULL,
                                            DR_EXTEND_UXTX, 0, 0, 0, OPSZ_16),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]),
              OPND_CREATE_INT(imm7[i]));

    /* Testing STGP    <Xt>, <Xt2>, [<Xn|SP>, #<simm>]! */
    const char *const expected_1_0[6] = {
        "stgp   %x0 %x0 %x0 $0xfffffffffffffc00 -> -0x0400(%x0)[16byte] %x0",
        "stgp   %x6 %x7 %x5 $0xfffffffffffffd80 -> -0x0280(%x5)[16byte] %x5",
        "stgp   %x11 %x12 %x10 $0xfffffffffffffed0 -> -0x0130(%x10)[16byte] %x10",
        "stgp   %x16 %x17 %x15 $0x0000000000000030 -> +0x30(%x15)[16byte] %x15",
        "stgp   %x21 %x22 %x20 $0x0000000000000180 -> +0x0180(%x20)[16byte] %x20",
        "stgp   %x30 %x30 %sp $0x00000000000003f0 -> +0x03f0(%sp)[16byte] %sp",
    };
    TEST_LOOP(
        stgp, stgp_pre, 6, expected_1_0[i],
        opnd_create_base_disp(Xn_six_offset_0_sp[i], DR_REG_NULL, 0, imm7[i], OPSZ_16),
        opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    /* Testing STGP    <Xt>, <Xt2>, [<Xn|SP>, #<simm>] */
    const char *const expected_2_0[6] = {
        "stgp   %x0 %x0 -> -0x0400(%x0)[16byte]",
        "stgp   %x5 %x6 -> -0x0280(%x7)[16byte]",
        "stgp   %x10 %x11 -> -0x0130(%x12)[16byte]",
        "stgp   %x15 %x16 -> +0x30(%x17)[16byte]",
        "stgp   %x20 %x21 -> +0x0180(%x22)[16byte]",
        "stgp   %x30 %x30 -> +0x03f0(%sp)[16byte]",
    };
    TEST_LOOP(stgp, stgp_offset, 6, expected_2_0[i],
              opnd_create_base_disp_aarch64(Xn_six_offset_2_sp[i], DR_REG_NULL,
                                            DR_EXTEND_UXTX, 0, imm7[i], 0, OPSZ_16),
              opnd_create_reg(Xn_six_offset_0[i]), opnd_create_reg(Xn_six_offset_1[i]));
}

TEST_INSTR(gmi)
{
    /* Testing GMI     <Xd>, <Xn|SP>, <Xm> */
    const char *const expected_0_0[6] = {
        "gmi    %x0 %x0 -> %x0",    "gmi    %x6 %x7 -> %x5",
        "gmi    %x11 %x12 -> %x10", "gmi    %x16 %x17 -> %x15",
        "gmi    %x21 %x22 -> %x20", "gmi    %sp %x30 -> %x30",
    };
    TEST_LOOP(gmi, gmi, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg(Xn_six_offset_1_sp[i]),
              opnd_create_reg(Xn_six_offset_2[i]));
}

TEST_INSTR(irg)
{
    /* Testing IRG     <Xd|SP>, <Xn|SP>, <Xm> */
    const char *const expected_0_0[6] = {
        "irg    %x0 %x0 -> %x0",    "irg    %x6 %x7 -> %x5",
        "irg    %x11 %x12 -> %x10", "irg    %x16 %x17 -> %x15",
        "irg    %x21 %x22 -> %x20", "irg    %sp %x30 -> %sp",
    };
    TEST_LOOP(irg, irg, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0_sp[i]),
              opnd_create_reg(Xn_six_offset_1_sp[i]),
              opnd_create_reg(Xn_six_offset_2[i]));
}

TEST_INSTR(subp)
{
    /* Testing SUBP    <Xd>, <Xn|SP>, <Xm|SP> */
    const char *const expected_0_0[6] = {
        "subp   %x0 %x0 -> %x0",    "subp   %x6 %x7 -> %x5",
        "subp   %x11 %x12 -> %x10", "subp   %x16 %x17 -> %x15",
        "subp   %x21 %x22 -> %x20", "subp   %sp %sp -> %x30",
    };
    TEST_LOOP(subp, subp, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg(Xn_six_offset_1_sp[i]),
              opnd_create_reg(Xn_six_offset_2_sp[i]));
}

TEST_INSTR(subps)
{
    /* Testing SUBPS   <Xd>, <Xn|SP>, <Xm|SP> */
    const char *const expected_0_0[6] = {
        "subps  %x0 %x0 -> %x0",    "subps  %x6 %x7 -> %x5",
        "subps  %x11 %x12 -> %x10", "subps  %x16 %x17 -> %x15",
        "subps  %x21 %x22 -> %x20", "subps  %sp %sp -> %x30",
    };
    TEST_LOOP(subps, subps, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg(Xn_six_offset_1_sp[i]),
              opnd_create_reg(Xn_six_offset_2_sp[i]));
}

TEST_INSTR(addg)
{
    /* Testing ADDG    <Xd|SP>, <Xn|SP>, #<imm1>, #<imm2> */
    static const uint uimm6_0_0[6] = { 0, 192, 368, 544, 704, 1008 };
    static const uint uimm4_0_0[6] = { 0, 5, 8, 11, 13, 15 };
    const char *const expected_0_0[6] = {
        "addg   %x0 $0x0000 $0x00 -> %x0",   "addg   %x6 $0x00c0 $0x05 -> %x5",
        "addg   %x11 $0x0170 $0x08 -> %x10", "addg   %x16 $0x0220 $0x0b -> %x15",
        "addg   %x21 $0x02c0 $0x0d -> %x20", "addg   %sp $0x03f0 $0x0f -> %sp",
    };
    TEST_LOOP(addg, addg, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0_sp[i]),
              opnd_create_reg(Xn_six_offset_1_sp[i]),
              opnd_create_immed_uint(uimm6_0_0[i], OPSZ_10b),
              opnd_create_immed_uint(uimm4_0_0[i], OPSZ_4b));
}

TEST_INSTR(subg)
{
    /* Testing SUBG    <Xd|SP>, <Xn|SP>, #<imm1>, #<imm2> */
    static const uint uimm6_0_0[6] = { 0, 192, 368, 544, 704, 1008 };
    static const uint uimm4_0_0[6] = { 0, 5, 8, 11, 13, 15 };
    const char *const expected_0_0[6] = {
        "subg   %x0 $0x0000 $0x00 -> %x0",   "subg   %x6 $0x00c0 $0x05 -> %x5",
        "subg   %x11 $0x0170 $0x08 -> %x10", "subg   %x16 $0x0220 $0x0b -> %x15",
        "subg   %x21 $0x02c0 $0x0d -> %x20", "subg   %sp $0x03f0 $0x0f -> %sp",
    };
    TEST_LOOP(subg, subg, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0_sp[i]),
              opnd_create_reg(Xn_six_offset_1_sp[i]),
              opnd_create_immed_uint(uimm6_0_0[i], OPSZ_10b),
              opnd_create_immed_uint(uimm4_0_0[i], OPSZ_4b));
}

TEST_INSTR(dc_gva)
{
    /* Testing DC      GVA, <Xt> */
    const char *const expected_0_0[6] = {
        "dc_gva  -> (%x0)[1byte]",  "dc_gva  -> (%x5)[1byte]",
        "dc_gva  -> (%x10)[1byte]", "dc_gva  -> (%x15)[1byte]",
        "dc_gva  -> (%x20)[1byte]", "dc_gva  -> (%x30)[1byte]",
    };
    TEST_LOOP(dc_gva, dc_gva, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]));
}

TEST_INSTR(dc_gzva)
{
    /* Testing DC      GZVA, <Xt> */
    const char *const expected_0_0[6] = {
        "dc_gzva  -> (%x0)[1byte]",  "dc_gzva  -> (%x5)[1byte]",
        "dc_gzva  -> (%x10)[1byte]", "dc_gzva  -> (%x15)[1byte]",
        "dc_gzva  -> (%x20)[1byte]", "dc_gzva  -> (%x30)[1byte]",
    };
    TEST_LOOP(dc_gzva, dc_gzva, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]));
}

int
main(int argc, char *argv[])
{
#ifdef STANDALONE_DECODER
    void *dcontext = GLOBAL_DCONTEXT;
#else
    void *dcontext = dr_standalone_init();
#endif
    bool result = true;
    bool test_result;
    instr_t *instr;

    enable_all_test_cpu_features();

    RUN_INSTR_TEST(bfcvt);
    RUN_INSTR_TEST(bfcvtn2_vector);
    RUN_INSTR_TEST(bfcvtn_vector);
    RUN_INSTR_TEST(bfdot_vector);
    RUN_INSTR_TEST(bfdot_vector_idx);
    RUN_INSTR_TEST(bfmlalb_vector);
    RUN_INSTR_TEST(bfmlalb_vector_idx);
    RUN_INSTR_TEST(bfmlalt_vector);
    RUN_INSTR_TEST(bfmlalt_vector_idx);
    RUN_INSTR_TEST(bfmmla_vector);

    RUN_INSTR_TEST(smmla_vector);
    RUN_INSTR_TEST(sudot_vector_idx);
    RUN_INSTR_TEST(ummla_vector);
    RUN_INSTR_TEST(usmmla_vector);
    RUN_INSTR_TEST(usdot_vector);
    RUN_INSTR_TEST(usdot_vector_idx);

    RUN_INSTR_TEST(ldg);
    RUN_INSTR_TEST(st2g);
    RUN_INSTR_TEST(stg);
    RUN_INSTR_TEST(stz2g);
    RUN_INSTR_TEST(stzg);
    RUN_INSTR_TEST(stgp);

    RUN_INSTR_TEST(gmi);
    RUN_INSTR_TEST(irg);
    RUN_INSTR_TEST(subp);
    RUN_INSTR_TEST(subps);
    RUN_INSTR_TEST(addg);
    RUN_INSTR_TEST(subg);
    RUN_INSTR_TEST(dc_gva);
    RUN_INSTR_TEST(dc_gzva);

    print("All v8.6 tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
