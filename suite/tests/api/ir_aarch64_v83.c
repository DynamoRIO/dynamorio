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
    TEST_NO_OPNDS(pacia1716, pacia1716, "pacia1716 %x17 %x16 -> %x17");
    TEST_NO_OPNDS(paciasp, paciasp, "paciasp %x30 %sp -> %x30");
    TEST_NO_OPNDS(paciaz, paciaz, "paciaz %x30 -> %x30");
    TEST_NO_OPNDS(pacib1716, pacib1716, "pacib1716 %x17 %x16 -> %x17");
    TEST_NO_OPNDS(pacibsp, pacibsp, "pacibsp %x30 %sp -> %x30");
    TEST_NO_OPNDS(pacibz, pacibz, "pacibz %x30 -> %x30");
    TEST_NO_OPNDS(xpaclri, xpaclri, "xpaclri %x30 -> %x30");
    TEST_NO_OPNDS(eretaa, eretaa, "eretaa %x30 %sp");
    TEST_NO_OPNDS(eretab, eretab, "eretab %x30 %sp");
    TEST_NO_OPNDS(retaa, retaa, "retaa  %x30 %sp");
    TEST_NO_OPNDS(retab, retab, "retab  %x30 %sp");
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

TEST_INSTR(blraa)
{
    /* Testing BLRAA   <Xn>, <Xm|SP> */
    const char *const expected_0_0[6] = {
        "blraa  %x0 %x0 -> %x30",   "blraa  %x5 %x6 -> %x30",
        "blraa  %x10 %x11 -> %x30", "blraa  %x15 %x16 -> %x30",
        "blraa  %x20 %x21 -> %x30", "blraa  %x30 %sp -> %x30",
    };
    TEST_LOOP(blraa, blraa, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg(Xn_six_offset_1_sp[i]));
}

TEST_INSTR(blraaz)
{
    /* Testing BLRAAZ  <Xn> */
    const char *const expected_0_0[6] = {
        "blraaz %x0 -> %x30",  "blraaz %x5 -> %x30",  "blraaz %x10 -> %x30",
        "blraaz %x15 -> %x30", "blraaz %x20 -> %x30", "blraaz %x30 -> %x30",
    };
    TEST_LOOP(blraaz, blraaz, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]));
}

TEST_INSTR(blrab)
{
    /* Testing BLRAB   <Xn>, <Xm|SP> */
    const char *const expected_0_0[6] = {
        "blrab  %x0 %x0 -> %x30",   "blrab  %x5 %x6 -> %x30",
        "blrab  %x10 %x11 -> %x30", "blrab  %x15 %x16 -> %x30",
        "blrab  %x20 %x21 -> %x30", "blrab  %x30 %sp -> %x30",
    };
    TEST_LOOP(blrab, blrab, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg(Xn_six_offset_1_sp[i]));
}

TEST_INSTR(blrabz)
{
    /* Testing BLRABZ  <Xn> */
    const char *const expected_0_0[6] = {
        "blrabz %x0 -> %x30",  "blrabz %x5 -> %x30",  "blrabz %x10 -> %x30",
        "blrabz %x15 -> %x30", "blrabz %x20 -> %x30", "blrabz %x30 -> %x30",
    };
    TEST_LOOP(blrabz, blrabz, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]));
}

TEST_INSTR(braa)
{
    /* Testing BRAA    <Xn>, <Xm|SP> */
    const char *const expected_0_0[6] = {
        "braa   %x0 %x0",   "braa   %x5 %x6",   "braa   %x10 %x11",
        "braa   %x15 %x16", "braa   %x20 %x21", "braa   %x30 %sp",
    };
    TEST_LOOP(braa, braa, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg(Xn_six_offset_1_sp[i]));
}

TEST_INSTR(braaz)
{
    /* Testing BRAAZ   <Xn> */
    const char *const expected_0_0[6] = {
        "braaz  %x0",  "braaz  %x5",  "braaz  %x10",
        "braaz  %x15", "braaz  %x20", "braaz  %x30",
    };
    TEST_LOOP(braaz, braaz, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]));
}

TEST_INSTR(brab)
{
    /* Testing BRAB    <Xn>, <Xm|SP> */
    const char *const expected_0_0[6] = {
        "brab   %x0 %x0",   "brab   %x5 %x6",   "brab   %x10 %x11",
        "brab   %x15 %x16", "brab   %x20 %x21", "brab   %x30 %sp",
    };
    TEST_LOOP(brab, brab, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg(Xn_six_offset_1_sp[i]));
}

TEST_INSTR(brabz)
{
    /* Testing BRABZ   <Xn> */
    const char *const expected_0_0[6] = {
        "brabz  %x0",  "brabz  %x5",  "brabz  %x10",
        "brabz  %x15", "brabz  %x20", "brabz  %x30",
    };
    TEST_LOOP(brabz, brabz, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]));
}

TEST_INSTR(pacda)
{

    /* Testing PACDA   <Xd>, <Xn|SP> */
    const char *const expected_0_0[6] = {
        "pacda  %x0 %x0 -> %x0",    "pacda  %x5 %x6 -> %x5",
        "pacda  %x10 %x11 -> %x10", "pacda  %x15 %x16 -> %x15",
        "pacda  %x20 %x21 -> %x20", "pacda  %x30 %sp -> %x30",
    };
    TEST_LOOP(pacda, pacda, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg(Xn_six_offset_1_sp[i]));
}

TEST_INSTR(pacdb)
{
    /* Testing PACDB   <Xd>, <Xn|SP> */
    const char *const expected_0_0[6] = {
        "pacdb  %x0 %x0 -> %x0",    "pacdb  %x5 %x6 -> %x5",
        "pacdb  %x10 %x11 -> %x10", "pacdb  %x15 %x16 -> %x15",
        "pacdb  %x20 %x21 -> %x20", "pacdb  %x30 %sp -> %x30",
    };
    TEST_LOOP(pacdb, pacdb, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg(Xn_six_offset_1_sp[i]));
}

TEST_INSTR(pacdza)
{
    /* Testing PACDZA  <Xd> */
    const char *const expected_0_0[6] = {
        "pacdza %x0 -> %x0",   "pacdza %x5 -> %x5",   "pacdza %x10 -> %x10",
        "pacdza %x15 -> %x15", "pacdza %x20 -> %x20", "pacdza %x30 -> %x30",
    };
    TEST_LOOP(pacdza, pacdza, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]));
}

TEST_INSTR(pacdzb)
{
    /* Testing PACDZB  <Xd> */
    const char *const expected_0_0[6] = {
        "pacdzb %x0 -> %x0",   "pacdzb %x5 -> %x5",   "pacdzb %x10 -> %x10",
        "pacdzb %x15 -> %x15", "pacdzb %x20 -> %x20", "pacdzb %x30 -> %x30",
    };
    TEST_LOOP(pacdzb, pacdzb, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]));
}

TEST_INSTR(pacga)
{
    /* Testing PACGA   <Xd>, <Xn>, <Xm|SP> */
    const char *const expected_0_0[6] = {
        "pacga  %x0 %x0 -> %x0",    "pacga  %x6 %x7 -> %x5",
        "pacga  %x11 %x12 -> %x10", "pacga  %x16 %x17 -> %x15",
        "pacga  %x21 %x22 -> %x20", "pacga  %x30 %sp -> %x30",
    };
    TEST_LOOP(pacga, pacga, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg(Xn_six_offset_1[i]),
              opnd_create_reg(Xn_six_offset_2_sp[i]));
}

TEST_INSTR(pacia)
{
    /* Testing PACIA   <Xd>, <Xn|SP> */
    const char *const expected_0_0[6] = {
        "pacia  %x0 %x0 -> %x0",    "pacia  %x5 %x6 -> %x5",
        "pacia  %x10 %x11 -> %x10", "pacia  %x15 %x16 -> %x15",
        "pacia  %x20 %x21 -> %x20", "pacia  %x30 %sp -> %x30",
    };
    TEST_LOOP(pacia, pacia, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg(Xn_six_offset_1_sp[i]));
}

TEST_INSTR(pacib)
{
    /* Testing PACIB   <Xd>, <Xn|SP> */
    const char *const expected_0_0[6] = {
        "pacib  %x0 %x0 -> %x0",    "pacib  %x5 %x6 -> %x5",
        "pacib  %x10 %x11 -> %x10", "pacib  %x15 %x16 -> %x15",
        "pacib  %x20 %x21 -> %x20", "pacib  %x30 %sp -> %x30",
    };
    TEST_LOOP(pacib, pacib, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg(Xn_six_offset_1_sp[i]));
}

TEST_INSTR(paciza)
{
    /* Testing PACIZA  <Xd> */
    const char *const expected_0_0[6] = {
        "paciza %x0 -> %x0",   "paciza %x5 -> %x5",   "paciza %x10 -> %x10",
        "paciza %x15 -> %x15", "paciza %x20 -> %x20", "paciza %x30 -> %x30",
    };
    TEST_LOOP(paciza, paciza, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]));
}

TEST_INSTR(pacizb)
{
    /* Testing PACIZB  <Xd> */
    const char *const expected_0_0[6] = {
        "pacizb %x0 -> %x0",   "pacizb %x5 -> %x5",   "pacizb %x10 -> %x10",
        "pacizb %x15 -> %x15", "pacizb %x20 -> %x20", "pacizb %x30 -> %x30",
    };
    TEST_LOOP(pacizb, pacizb, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]));
}

TEST_INSTR(ldraa)
{
    /* Testing LDRAA   <Xt>, [<Xn|SP>, #<simm>]! */
    static const int simm[6] = { -4096, -2720, -1352, 16, 1376, 4088 };
    const char *const expected_0_0[6] = {
        "ldraa  -0x1000(%x0)[8byte] %x0 $0xfffffffffffff000 -> %x0 %x0",
        "ldraa  -0x0aa0(%x6)[8byte] %x6 $0xfffffffffffff560 -> %x5 %x6",
        "ldraa  -0x0548(%x11)[8byte] %x11 $0xfffffffffffffab8 -> %x10 %x11",
        "ldraa  +0x10(%x16)[8byte] %x16 $0x0000000000000010 -> %x15 %x16",
        "ldraa  +0x0560(%x21)[8byte] %x21 $0x0000000000000560 -> %x20 %x21",
        "ldraa  +0x0ff8(%sp)[8byte] %sp $0x0000000000000ff8 -> %x30 %sp",
    };
    TEST_LOOP(
        ldraa, ldraa_imm, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
        opnd_create_reg(Xn_six_offset_1_sp[i]),
        opnd_create_base_disp(Xn_six_offset_1_sp[i], DR_REG_NULL, 0, simm[i], OPSZ_8),
        opnd_create_immed_uint(simm[i], OPSZ_PTR));

    /* Testing LDRAA   <Xt>, [<Xn|SP>, #<simm>] */
    const char *const expected_1_0[6] = {
        "ldraa  -0x1000(%x0)[8byte] -> %x0",   "ldraa  -0x0aa0(%x6)[8byte] -> %x5",
        "ldraa  -0x0548(%x11)[8byte] -> %x10", "ldraa  +0x10(%x16)[8byte] -> %x15",
        "ldraa  +0x0560(%x21)[8byte] -> %x20", "ldraa  +0x0ff8(%sp)[8byte] -> %x30",
    };
    TEST_LOOP(
        ldraa, ldraa, 6, expected_1_0[i], opnd_create_reg(Xn_six_offset_0[i]),
        opnd_create_base_disp(Xn_six_offset_1_sp[i], DR_REG_NULL, 0, simm[i], OPSZ_8));
}

TEST_INSTR(ldrab)
{
    /* Testing LDRAB   <Xt>, [<Xn|SP>, #<simm>]! */
    static const int simm[6] = { -4096, -2720, -1352, 16, 1376, 4088 };
    const char *const expected_0_0[6] = {
        "ldrab  -0x1000(%x0)[8byte] %x0 $0xfffffffffffff000 -> %x0 %x0",
        "ldrab  -0x0aa0(%x6)[8byte] %x6 $0xfffffffffffff560 -> %x5 %x6",
        "ldrab  -0x0548(%x11)[8byte] %x11 $0xfffffffffffffab8 -> %x10 %x11",
        "ldrab  +0x10(%x16)[8byte] %x16 $0x0000000000000010 -> %x15 %x16",
        "ldrab  +0x0560(%x21)[8byte] %x21 $0x0000000000000560 -> %x20 %x21",
        "ldrab  +0x0ff8(%sp)[8byte] %sp $0x0000000000000ff8 -> %x30 %sp",
    };
    TEST_LOOP(
        ldrab, ldrab_imm, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
        opnd_create_reg(Xn_six_offset_1_sp[i]),
        opnd_create_base_disp(Xn_six_offset_1_sp[i], DR_REG_NULL, 0, simm[i], OPSZ_8),
        opnd_create_immed_uint(simm[i], OPSZ_PTR));

    /* Testing LDRAB   <Xt>, [<Xn|SP>, #<simm>] */
    const char *const expected_1_0[6] = {
        "ldrab  -0x1000(%x0)[8byte] -> %x0",   "ldrab  -0x0aa0(%x6)[8byte] -> %x5",
        "ldrab  -0x0548(%x11)[8byte] -> %x10", "ldrab  +0x10(%x16)[8byte] -> %x15",
        "ldrab  +0x0560(%x21)[8byte] -> %x20", "ldrab  +0x0ff8(%sp)[8byte] -> %x30",
    };
    TEST_LOOP(
        ldrab, ldrab, 6, expected_1_0[i], opnd_create_reg(Xn_six_offset_0[i]),
        opnd_create_base_disp(Xn_six_offset_1_sp[i], DR_REG_NULL, 0, simm[i], OPSZ_8));
}

TEST_INSTR(xpacd)
{
    /* Testing XPACD   <Xd> */
    const char *const expected_0_0[6] = {
        "xpacd  %x0 -> %x0",   "xpacd  %x5 -> %x5",   "xpacd  %x10 -> %x10",
        "xpacd  %x15 -> %x15", "xpacd  %x20 -> %x20", "xpacd  %x30 -> %x30",
    };
    TEST_LOOP(xpacd, xpacd, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]));
}

TEST_INSTR(xpaci)
{
    /* Testing XPACI   <Xd> */
    const char *const expected_0_0[6] = {
        "xpaci  %x0 -> %x0",   "xpaci  %x5 -> %x5",   "xpaci  %x10 -> %x10",
        "xpaci  %x15 -> %x15", "xpaci  %x20 -> %x20", "xpaci  %x30 -> %x30",
    };
    TEST_LOOP(xpaci, xpaci, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]));
}

TEST_INSTR(fjcvtzs)
{
    /* Testing FJCVTZS <Wd>, <Dn> */
    const char *const expected_0_0[6] = {
        "fjcvtzs %d0 -> %w0",   "fjcvtzs %d6 -> %w5",   "fjcvtzs %d11 -> %w10",
        "fjcvtzs %d17 -> %w15", "fjcvtzs %d22 -> %w20", "fjcvtzs %d31 -> %w30",
    };
    TEST_LOOP(fjcvtzs, fjcvtzs, 6, expected_0_0[i], opnd_create_reg(Wn_six_offset_0[i]),
              opnd_create_reg(Vdn_d_six_offset_1[i]));
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
    RUN_INSTR_TEST(blraa);
    RUN_INSTR_TEST(blraaz);
    RUN_INSTR_TEST(blrab);
    RUN_INSTR_TEST(blrabz);
    RUN_INSTR_TEST(braa);
    RUN_INSTR_TEST(braaz);
    RUN_INSTR_TEST(brab);
    RUN_INSTR_TEST(brabz);
    RUN_INSTR_TEST(pacda);
    RUN_INSTR_TEST(pacdb);
    RUN_INSTR_TEST(pacdza);
    RUN_INSTR_TEST(pacdzb);
    RUN_INSTR_TEST(pacga);
    RUN_INSTR_TEST(pacia);
    RUN_INSTR_TEST(pacib);
    RUN_INSTR_TEST(paciza);
    RUN_INSTR_TEST(pacizb);
    RUN_INSTR_TEST(ldraa);
    RUN_INSTR_TEST(ldrab);
    RUN_INSTR_TEST(xpacd);
    RUN_INSTR_TEST(xpaci);

    /* FEAT_JSCVT */
    RUN_INSTR_TEST(fjcvtzs);

    print("All v8.3 tests complete.");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
