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

TEST_INSTR(fcadd_vector)
{
    /* Testing FCADD   <Vd>.<Ts>, <Vn>.<Ts>, <Vm>.<Ts>, <imm> */
    static const uint rot_0_0[6] = { 90, 270, 270, 270, 90, 270 };
    const char *const expected_0_0[6] = {
        "fcadd  %d0 %d0 %d0 $0x005a $0x01 -> %d0",
        "fcadd  %d5 %d6 %d7 $0x010e $0x01 -> %d5",
        "fcadd  %d10 %d11 %d12 $0x010e $0x01 -> %d10",
        "fcadd  %d16 %d17 %d18 $0x010e $0x01 -> %d16",
        "fcadd  %d21 %d22 %d23 $0x005a $0x01 -> %d21",
        "fcadd  %d31 %d31 %d31 $0x010e $0x01 -> %d31",
    };
    TEST_LOOP(
        fcadd, fcadd_vector, 6, expected_0_0[i], opnd_create_reg(Vdn_d_six_offset_0[i]),
        opnd_create_reg(Vdn_d_six_offset_1[i]), opnd_create_reg(Vdn_d_six_offset_2[i]),
        opnd_create_immed_uint(rot_0_0[i], OPSZ_2), OPND_CREATE_HALF());

    const char *const expected_0_1[6] = {
        "fcadd  %d0 %d0 %d0 $0x005a $0x02 -> %d0",
        "fcadd  %d5 %d6 %d7 $0x010e $0x02 -> %d5",
        "fcadd  %d10 %d11 %d12 $0x010e $0x02 -> %d10",
        "fcadd  %d16 %d17 %d18 $0x010e $0x02 -> %d16",
        "fcadd  %d21 %d22 %d23 $0x005a $0x02 -> %d21",
        "fcadd  %d31 %d31 %d31 $0x010e $0x02 -> %d31",
    };
    TEST_LOOP(
        fcadd, fcadd_vector, 6, expected_0_1[i], opnd_create_reg(Vdn_d_six_offset_0[i]),
        opnd_create_reg(Vdn_d_six_offset_1[i]), opnd_create_reg(Vdn_d_six_offset_2[i]),
        opnd_create_immed_uint(rot_0_0[i], OPSZ_2), OPND_CREATE_SINGLE());

    const char *const expected_0_2[6] = {
        "fcadd  %q0 %q0 %q0 $0x005a $0x01 -> %q0",
        "fcadd  %q5 %q6 %q7 $0x010e $0x01 -> %q5",
        "fcadd  %q10 %q11 %q12 $0x010e $0x01 -> %q10",
        "fcadd  %q16 %q17 %q18 $0x010e $0x01 -> %q16",
        "fcadd  %q21 %q22 %q23 $0x005a $0x01 -> %q21",
        "fcadd  %q31 %q31 %q31 $0x010e $0x01 -> %q31",
    };
    TEST_LOOP(
        fcadd, fcadd_vector, 6, expected_0_2[i], opnd_create_reg(Vdn_q_six_offset_0[i]),
        opnd_create_reg(Vdn_q_six_offset_1[i]), opnd_create_reg(Vdn_q_six_offset_2[i]),
        opnd_create_immed_uint(rot_0_0[i], OPSZ_2), OPND_CREATE_HALF());

    const char *const expected_0_3[6] = {
        "fcadd  %q0 %q0 %q0 $0x005a $0x02 -> %q0",
        "fcadd  %q5 %q6 %q7 $0x010e $0x02 -> %q5",
        "fcadd  %q10 %q11 %q12 $0x010e $0x02 -> %q10",
        "fcadd  %q16 %q17 %q18 $0x010e $0x02 -> %q16",
        "fcadd  %q21 %q22 %q23 $0x005a $0x02 -> %q21",
        "fcadd  %q31 %q31 %q31 $0x010e $0x02 -> %q31",
    };
    TEST_LOOP(
        fcadd, fcadd_vector, 6, expected_0_3[i], opnd_create_reg(Vdn_q_six_offset_0[i]),
        opnd_create_reg(Vdn_q_six_offset_1[i]), opnd_create_reg(Vdn_q_six_offset_2[i]),
        opnd_create_immed_uint(rot_0_0[i], OPSZ_2), OPND_CREATE_SINGLE());
}

TEST_INSTR(fcmla_vector)
{
    /* Testing FCMLA   <Vd>.<Ts>, <Vn>.<Ts>, <Vm>.<Ts>, <imm> */
    static const uint rot_0_0[6] = { 0, 270, 0, 90, 90, 270 };
    const char *const expected_0_0[6] = {
        "fcmla  %d0 %d0 %d0 $0x0000 $0x01 -> %d0",
        "fcmla  %d5 %d6 %d7 $0x010e $0x01 -> %d5",
        "fcmla  %d10 %d11 %d12 $0x0000 $0x01 -> %d10",
        "fcmla  %d16 %d17 %d18 $0x005a $0x01 -> %d16",
        "fcmla  %d21 %d22 %d23 $0x005a $0x01 -> %d21",
        "fcmla  %d31 %d31 %d31 $0x010e $0x01 -> %d31",
    };
    TEST_LOOP(
        fcmla, fcmla_vector, 6, expected_0_0[i], opnd_create_reg(Vdn_d_six_offset_0[i]),
        opnd_create_reg(Vdn_d_six_offset_1[i]), opnd_create_reg(Vdn_d_six_offset_2[i]),
        opnd_create_immed_uint(rot_0_0[i], OPSZ_2), OPND_CREATE_HALF());

    const char *const expected_0_1[6] = {
        "fcmla  %d0 %d0 %d0 $0x0000 $0x02 -> %d0",
        "fcmla  %d5 %d6 %d7 $0x010e $0x02 -> %d5",
        "fcmla  %d10 %d11 %d12 $0x0000 $0x02 -> %d10",
        "fcmla  %d16 %d17 %d18 $0x005a $0x02 -> %d16",
        "fcmla  %d21 %d22 %d23 $0x005a $0x02 -> %d21",
        "fcmla  %d31 %d31 %d31 $0x010e $0x02 -> %d31",
    };
    TEST_LOOP(
        fcmla, fcmla_vector, 6, expected_0_1[i], opnd_create_reg(Vdn_d_six_offset_0[i]),
        opnd_create_reg(Vdn_d_six_offset_1[i]), opnd_create_reg(Vdn_d_six_offset_2[i]),
        opnd_create_immed_uint(rot_0_0[i], OPSZ_2), OPND_CREATE_SINGLE());

    const char *const expected_0_2[6] = {
        "fcmla  %q0 %q0 %q0 $0x0000 $0x01 -> %q0",
        "fcmla  %q5 %q6 %q7 $0x010e $0x01 -> %q5",
        "fcmla  %q10 %q11 %q12 $0x0000 $0x01 -> %q10",
        "fcmla  %q16 %q17 %q18 $0x005a $0x01 -> %q16",
        "fcmla  %q21 %q22 %q23 $0x005a $0x01 -> %q21",
        "fcmla  %q31 %q31 %q31 $0x010e $0x01 -> %q31",
    };
    TEST_LOOP(
        fcmla, fcmla_vector, 6, expected_0_2[i], opnd_create_reg(Vdn_q_six_offset_0[i]),
        opnd_create_reg(Vdn_q_six_offset_1[i]), opnd_create_reg(Vdn_q_six_offset_2[i]),
        opnd_create_immed_uint(rot_0_0[i], OPSZ_2), OPND_CREATE_HALF());

    const char *const expected_0_3[6] = {
        "fcmla  %q0 %q0 %q0 $0x0000 $0x02 -> %q0",
        "fcmla  %q5 %q6 %q7 $0x010e $0x02 -> %q5",
        "fcmla  %q10 %q11 %q12 $0x0000 $0x02 -> %q10",
        "fcmla  %q16 %q17 %q18 $0x005a $0x02 -> %q16",
        "fcmla  %q21 %q22 %q23 $0x005a $0x02 -> %q21",
        "fcmla  %q31 %q31 %q31 $0x010e $0x02 -> %q31",
    };
    TEST_LOOP(
        fcmla, fcmla_vector, 6, expected_0_3[i], opnd_create_reg(Vdn_q_six_offset_0[i]),
        opnd_create_reg(Vdn_q_six_offset_1[i]), opnd_create_reg(Vdn_q_six_offset_2[i]),
        opnd_create_immed_uint(rot_0_0[i], OPSZ_2), OPND_CREATE_SINGLE());
}

TEST_INSTR(fcmla_vector_idx)
{
    opnd_t Rm_elsz;

    /* Testing FCMLA   <Vd>.<Ts>, <Vn>.<Ts>, <Vm>.<Tb>[<imm1>], <imm2> */
    static const uint index_0_0[6] = { 0, 1, 1, 1, 0, 1 };
    static const uint rot_0_0[6] = { 0, 0, 90, 180, 180, 270 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *const expected_0_0[6] = {
        "fcmla  %d0 %d0 %d0 $0x00 $0x0000 $0x01 -> %d0",
        "fcmla  %d5 %d6 %d7 $0x01 $0x0000 $0x01 -> %d5",
        "fcmla  %d10 %d11 %d12 $0x01 $0x005a $0x01 -> %d10",
        "fcmla  %d16 %d17 %d18 $0x01 $0x00b4 $0x01 -> %d16",
        "fcmla  %d21 %d22 %d23 $0x00 $0x00b4 $0x01 -> %d21",
        "fcmla  %d31 %d31 %d31 $0x01 $0x010e $0x01 -> %d31",
    };
    TEST_LOOP(fcmla, fcmla_vector_idx, 6, expected_0_0[i],
              opnd_create_reg(Vdn_d_six_offset_0[i]),
              opnd_create_reg(Vdn_d_six_offset_1[i]),
              opnd_create_reg(Vdn_d_six_offset_2[i]),
              opnd_create_immed_uint(index_0_0[i], OPSZ_2b),
              opnd_create_immed_uint(rot_0_0[i], OPSZ_2), Rm_elsz);

    static const uint index_0_1[6] = { 0, 3, 0, 1, 1, 3 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *const expected_0_1[6] = {
        "fcmla  %q0 %q0 %q0 $0x00 $0x0000 $0x01 -> %q0",
        "fcmla  %q5 %q6 %q7 $0x03 $0x0000 $0x01 -> %q5",
        "fcmla  %q10 %q11 %q12 $0x00 $0x005a $0x01 -> %q10",
        "fcmla  %q16 %q17 %q18 $0x01 $0x00b4 $0x01 -> %q16",
        "fcmla  %q21 %q22 %q23 $0x01 $0x00b4 $0x01 -> %q21",
        "fcmla  %q31 %q31 %q31 $0x03 $0x010e $0x01 -> %q31",
    };
    TEST_LOOP(fcmla, fcmla_vector_idx, 6, expected_0_1[i],
              opnd_create_reg(Vdn_q_six_offset_0[i]),
              opnd_create_reg(Vdn_q_six_offset_1[i]),
              opnd_create_reg(Vdn_q_six_offset_2[i]),
              opnd_create_immed_uint(index_0_1[i], OPSZ_2b),
              opnd_create_immed_uint(rot_0_0[i], OPSZ_2), Rm_elsz);

    Rm_elsz = OPND_CREATE_SINGLE();
    const char *const expected_0_2[6] = {
        "fcmla  %q0 %q0 %q0 $0x00 $0x0000 $0x02 -> %q0",
        "fcmla  %q5 %q6 %q7 $0x01 $0x0000 $0x02 -> %q5",
        "fcmla  %q10 %q11 %q12 $0x01 $0x005a $0x02 -> %q10",
        "fcmla  %q16 %q17 %q18 $0x01 $0x00b4 $0x02 -> %q16",
        "fcmla  %q21 %q22 %q23 $0x00 $0x00b4 $0x02 -> %q21",
        "fcmla  %q31 %q31 %q31 $0x01 $0x010e $0x02 -> %q31",
    };
    TEST_LOOP(fcmla, fcmla_vector_idx, 6, expected_0_2[i],
              opnd_create_reg(Vdn_q_six_offset_0[i]),
              opnd_create_reg(Vdn_q_six_offset_1[i]),
              opnd_create_reg(Vdn_q_six_offset_2[i]),
              opnd_create_immed_uint(index_0_0[i], OPSZ_2b),
              opnd_create_immed_uint(rot_0_0[i], OPSZ_2), Rm_elsz);
}

TEST_INSTR(autda)
{
    /* Testing AUTDA   <Xd>, <Xn|SP> */
    const char *const expected_0_0[6] = {
        "autda  %x0 %x0 -> %x0",    "autda  %x5 %x6 -> %x5",
        "autda  %x10 %x11 -> %x10", "autda  %x15 %x16 -> %x15",
        "autda  %x20 %x21 -> %x20", "autda  %x30 %sp -> %x30",
    };
    TEST_LOOP(autda, autda, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg(Xn_six_offset_1_sp[i]));
}

TEST_INSTR(autdb)
{
    /* Testing AUTDB   <Xd>, <Xn|SP> */
    const char *const expected_0_0[6] = {
        "autdb  %x0 %x0 -> %x0",    "autdb  %x5 %x6 -> %x5",
        "autdb  %x10 %x11 -> %x10", "autdb  %x15 %x16 -> %x15",
        "autdb  %x20 %x21 -> %x20", "autdb  %x30 %sp -> %x30",
    };
    TEST_LOOP(autdb, autdb, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg(Xn_six_offset_1_sp[i]));
}

TEST_INSTR(autdza)
{
    /* Testing AUTDZA  <Xd> */
    const char *const expected_0_0[6] = {
        "autdza %x0 -> %x0",   "autdza %x5 -> %x5",   "autdza %x10 -> %x10",
        "autdza %x15 -> %x15", "autdza %x20 -> %x20", "autdza %x30 -> %x30",
    };
    TEST_LOOP(autdza, autdza, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]));
}

TEST_INSTR(autdzb)
{
    /* Testing AUTDZB  <Xd> */
    const char *const expected_0_0[6] = {
        "autdzb %x0 -> %x0",   "autdzb %x5 -> %x5",   "autdzb %x10 -> %x10",
        "autdzb %x15 -> %x15", "autdzb %x20 -> %x20", "autdzb %x30 -> %x30",
    };
    TEST_LOOP(autdzb, autdzb, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]));
}

TEST_INSTR(autia)
{
    /* Testing AUTIA   <Xd>, <Xn|SP> */
    const char *const expected_0_0[6] = {
        "autia  %x0 %x0 -> %x0",    "autia  %x5 %x6 -> %x5",
        "autia  %x10 %x11 -> %x10", "autia  %x15 %x16 -> %x15",
        "autia  %x20 %x21 -> %x20", "autia  %x30 %sp -> %x30",
    };
    TEST_LOOP(autia, autia, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg(Xn_six_offset_1_sp[i]));
}

TEST_INSTR(pauth_hints)
{
    TEST_NO_OPNDS(autia1716, autia1716, "autia1716 %x17 %x16 -> %x17");
    TEST_NO_OPNDS(autiasp, autiasp, "autiasp %x30 %sp -> %x30");
    TEST_NO_OPNDS(autiaz, autiaz, "autiaz %x30 -> %x30");
    TEST_NO_OPNDS(autib1716, autib1716, "autib1716 %x17 %x16 -> %x17");
    TEST_NO_OPNDS(autibsp, autibsp, "autibsp %x30 %sp -> %x30");
    TEST_NO_OPNDS(autibz, autibz, "autibz %x30 -> %x30");
}

TEST_INSTR(autib)
{
    /* Testing AUTIB   <Xd>, <Xn|SP> */
    const char *const expected_0_0[6] = {
        "autib  %x0 %x0 -> %x0",    "autib  %x5 %x6 -> %x5",
        "autib  %x10 %x11 -> %x10", "autib  %x15 %x16 -> %x15",
        "autib  %x20 %x21 -> %x20", "autib  %x30 %sp -> %x30",
    };
    TEST_LOOP(autib, autib, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg(Xn_six_offset_1_sp[i]));
}

TEST_INSTR(autiza)
{
    /* Testing AUTIZA  <Xd> */
    const char *const expected_0_0[6] = {
        "autiza %x0 -> %x0",   "autiza %x5 -> %x5",   "autiza %x10 -> %x10",
        "autiza %x15 -> %x15", "autiza %x20 -> %x20", "autiza %x30 -> %x30",
    };
    TEST_LOOP(autiza, autiza, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]));
}

TEST_INSTR(autizb)
{
    /* Testing AUTIZB  <Xd> */
    const char *const expected_0_0[6] = {
        "autizb %x0 -> %x0",   "autizb %x5 -> %x5",   "autizb %x10 -> %x10",
        "autizb %x15 -> %x15", "autizb %x20 -> %x20", "autizb %x30 -> %x30",
    };
    TEST_LOOP(autizb, autizb, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]));
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

    RUN_INSTR_TEST(fcadd_vector);
    RUN_INSTR_TEST(fcmla_vector);
    RUN_INSTR_TEST(fcmla_vector_idx);

    /* FEAT_PAuth */
    RUN_INSTR_TEST(autda);
    RUN_INSTR_TEST(autdb);
    RUN_INSTR_TEST(autdza);
    RUN_INSTR_TEST(autdzb);
    RUN_INSTR_TEST(pauth_hints);
    RUN_INSTR_TEST(autia);
    RUN_INSTR_TEST(autib);
    RUN_INSTR_TEST(autiza);
    RUN_INSTR_TEST(autizb);

    print("All v8.3 tests complete.");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
