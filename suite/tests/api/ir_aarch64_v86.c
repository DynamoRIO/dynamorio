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

    print("All v8.6 tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
