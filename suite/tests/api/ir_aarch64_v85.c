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

TEST_INSTR(bti)
{
    /* Testing BTI    #<imm> */
    const int imm[4] = { 0, 1, 2, 3 };

    const char *const expected_0_0[4] = { "bti    $0x00", "bti    $0x01", "bti    $0x02",
                                          "bti    $0x03" };
    TEST_LOOP(bti, bti, 4, expected_0_0[i], opnd_create_immed_uint(imm[i], OPSZ_3b));
}

TEST_INSTR(frint32x)
{

    /* Testing FRINT32X <Dd>, <Dn> */
    const char *const expected_0_0[6] = {
        "frint32x %d0 -> %d0",   "frint32x %d6 -> %d5",   "frint32x %d11 -> %d10",
        "frint32x %d17 -> %d16", "frint32x %d22 -> %d21", "frint32x %d31 -> %d31",
    };
    TEST_LOOP(frint32x, frint32x, 6, expected_0_0[i],
              opnd_create_reg(Vdn_d_six_offset_0[i]),
              opnd_create_reg(Vdn_d_six_offset_1[i]));

    /* Testing FRINT32X <Sd>, <Sn> */
    const char *const expected_1_0[6] = {
        "frint32x %s0 -> %s0",   "frint32x %s6 -> %s5",   "frint32x %s11 -> %s10",
        "frint32x %s17 -> %s16", "frint32x %s22 -> %s21", "frint32x %s31 -> %s31",
    };
    TEST_LOOP(frint32x, frint32x, 6, expected_1_0[i],
              opnd_create_reg(Vdn_s_six_offset_0[i]),
              opnd_create_reg(Vdn_s_six_offset_1[i]));
}

TEST_INSTR(frint32x_vector)
{
    /* Testing FRINT32X <Vd>.<Ts>, <Vn>.<Ts> */
    const char *const expected_0_0[6] = {
        "frint32x %d0.s -> %d0.s",   "frint32x %d6.s -> %d5.s",
        "frint32x %d11.s -> %d10.s", "frint32x %d17.s -> %d16.s",
        "frint32x %d22.s -> %d21.s", "frint32x %d31.s -> %d31.s",
    };
    TEST_LOOP(frint32x, frint32x_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Vdn_d_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Vdn_d_six_offset_1[i], OPSZ_4));

    const char *const expected_0_1[6] = {
        "frint32x %q0.s -> %q0.s",   "frint32x %q6.s -> %q5.s",
        "frint32x %q11.s -> %q10.s", "frint32x %q17.s -> %q16.s",
        "frint32x %q22.s -> %q21.s", "frint32x %q31.s -> %q31.s",
    };
    TEST_LOOP(frint32x, frint32x_vector, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Vdn_q_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Vdn_q_six_offset_1[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "frint32x %q0.d -> %q0.d",   "frint32x %q6.d -> %q5.d",
        "frint32x %q11.d -> %q10.d", "frint32x %q17.d -> %q16.d",
        "frint32x %q22.d -> %q21.d", "frint32x %q31.d -> %q31.d",
    };
    TEST_LOOP(frint32x, frint32x_vector, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Vdn_q_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Vdn_q_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(frint32z)
{
    /* Testing FRINT32Z <Dd>, <Dn> */
    const char *const expected_0_0[6] = {
        "frint32z %d0 -> %d0",   "frint32z %d6 -> %d5",   "frint32z %d11 -> %d10",
        "frint32z %d17 -> %d16", "frint32z %d22 -> %d21", "frint32z %d31 -> %d31",
    };
    TEST_LOOP(frint32z, frint32z, 6, expected_0_0[i],
              opnd_create_reg(Vdn_d_six_offset_0[i]),
              opnd_create_reg(Vdn_d_six_offset_1[i]));

    /* Testing FRINT32Z <Sd>, <Sn> */
    const char *const expected_1_0[6] = {
        "frint32z %s0 -> %s0",   "frint32z %s6 -> %s5",   "frint32z %s11 -> %s10",
        "frint32z %s17 -> %s16", "frint32z %s22 -> %s21", "frint32z %s31 -> %s31",
    };
    TEST_LOOP(frint32z, frint32z, 6, expected_1_0[i],
              opnd_create_reg(Vdn_s_six_offset_0[i]),
              opnd_create_reg(Vdn_s_six_offset_1[i]));
}

TEST_INSTR(frint32z_vector)
{
    /* Testing FRINT32Z <Vd>.<Ts>, <Vn>.<Ts> */
    const char *const expected_0_0[6] = {
        "frint32z %d0.s -> %d0.s",   "frint32z %d6.s -> %d5.s",
        "frint32z %d11.s -> %d10.s", "frint32z %d17.s -> %d16.s",
        "frint32z %d22.s -> %d21.s", "frint32z %d31.s -> %d31.s",
    };
    TEST_LOOP(frint32z, frint32z_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Vdn_d_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Vdn_d_six_offset_1[i], OPSZ_4));

    const char *const expected_0_1[6] = {
        "frint32z %q0.s -> %q0.s",   "frint32z %q6.s -> %q5.s",
        "frint32z %q11.s -> %q10.s", "frint32z %q17.s -> %q16.s",
        "frint32z %q22.s -> %q21.s", "frint32z %q31.s -> %q31.s",
    };
    TEST_LOOP(frint32z, frint32z_vector, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Vdn_q_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Vdn_q_six_offset_1[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "frint32z %q0.d -> %q0.d",   "frint32z %q6.d -> %q5.d",
        "frint32z %q11.d -> %q10.d", "frint32z %q17.d -> %q16.d",
        "frint32z %q22.d -> %q21.d", "frint32z %q31.d -> %q31.d",
    };
    TEST_LOOP(frint32z, frint32z_vector, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Vdn_q_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Vdn_q_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(frint64x)
{

    /* Testing FRINT64X <Dd>, <Dn> */
    const char *const expected_0_0[6] = {
        "frint64x %d0 -> %d0",   "frint64x %d6 -> %d5",   "frint64x %d11 -> %d10",
        "frint64x %d17 -> %d16", "frint64x %d22 -> %d21", "frint64x %d31 -> %d31",
    };
    TEST_LOOP(frint64x, frint64x, 6, expected_0_0[i],
              opnd_create_reg(Vdn_d_six_offset_0[i]),
              opnd_create_reg(Vdn_d_six_offset_1[i]));

    /* Testing FRINT64X <Sd>, <Sn> */
    const char *const expected_1_0[6] = {
        "frint64x %s0 -> %s0",   "frint64x %s6 -> %s5",   "frint64x %s11 -> %s10",
        "frint64x %s17 -> %s16", "frint64x %s22 -> %s21", "frint64x %s31 -> %s31",
    };
    TEST_LOOP(frint64x, frint64x, 6, expected_1_0[i],
              opnd_create_reg(Vdn_s_six_offset_0[i]),
              opnd_create_reg(Vdn_s_six_offset_1[i]));
}

TEST_INSTR(frint64x_vector)
{
    /* Testing FRINT64X <Vd>.<Ts>, <Vn>.<Ts> */
    const char *const expected_0_0[6] = {
        "frint64x %d0.s -> %d0.s",   "frint64x %d6.s -> %d5.s",
        "frint64x %d11.s -> %d10.s", "frint64x %d17.s -> %d16.s",
        "frint64x %d22.s -> %d21.s", "frint64x %d31.s -> %d31.s",
    };
    TEST_LOOP(frint64x, frint64x_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Vdn_d_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Vdn_d_six_offset_1[i], OPSZ_4));

    const char *const expected_0_1[6] = {
        "frint64x %q0.s -> %q0.s",   "frint64x %q6.s -> %q5.s",
        "frint64x %q11.s -> %q10.s", "frint64x %q17.s -> %q16.s",
        "frint64x %q22.s -> %q21.s", "frint64x %q31.s -> %q31.s",
    };
    TEST_LOOP(frint64x, frint64x_vector, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Vdn_q_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Vdn_q_six_offset_1[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "frint64x %q0.d -> %q0.d",   "frint64x %q6.d -> %q5.d",
        "frint64x %q11.d -> %q10.d", "frint64x %q17.d -> %q16.d",
        "frint64x %q22.d -> %q21.d", "frint64x %q31.d -> %q31.d",
    };
    TEST_LOOP(frint64x, frint64x_vector, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Vdn_q_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Vdn_q_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(frint64z)
{

    /* Testing FRINT64Z <Dd>, <Dn> */
    const char *const expected_0_0[6] = {
        "frint64z %d0 -> %d0",   "frint64z %d6 -> %d5",   "frint64z %d11 -> %d10",
        "frint64z %d17 -> %d16", "frint64z %d22 -> %d21", "frint64z %d31 -> %d31",
    };
    TEST_LOOP(frint64z, frint64z, 6, expected_0_0[i],
              opnd_create_reg(Vdn_d_six_offset_0[i]),
              opnd_create_reg(Vdn_d_six_offset_1[i]));

    /* Testing FRINT64Z <Sd>, <Sn> */
    const char *const expected_1_0[6] = {
        "frint64z %s0 -> %s0",   "frint64z %s6 -> %s5",   "frint64z %s11 -> %s10",
        "frint64z %s17 -> %s16", "frint64z %s22 -> %s21", "frint64z %s31 -> %s31",
    };
    TEST_LOOP(frint64z, frint64z, 6, expected_1_0[i],
              opnd_create_reg(Vdn_s_six_offset_0[i]),
              opnd_create_reg(Vdn_s_six_offset_1[i]));
}

TEST_INSTR(frint64z_vector)
{
    /* Testing FRINT64Z <Vd>.<Ts>, <Vn>.<Ts> */
    const char *const expected_0_0[6] = {
        "frint64z %d0.s -> %d0.s",   "frint64z %d6.s -> %d5.s",
        "frint64z %d11.s -> %d10.s", "frint64z %d17.s -> %d16.s",
        "frint64z %d22.s -> %d21.s", "frint64z %d31.s -> %d31.s",
    };
    TEST_LOOP(frint64z, frint64z_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Vdn_d_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Vdn_d_six_offset_1[i], OPSZ_4));

    const char *const expected_0_1[6] = {
        "frint64z %q0.s -> %q0.s",   "frint64z %q6.s -> %q5.s",
        "frint64z %q11.s -> %q10.s", "frint64z %q17.s -> %q16.s",
        "frint64z %q22.s -> %q21.s", "frint64z %q31.s -> %q31.s",
    };
    TEST_LOOP(frint64z, frint64z_vector, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Vdn_q_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Vdn_q_six_offset_1[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "frint64z %q0.d -> %q0.d",   "frint64z %q6.d -> %q5.d",
        "frint64z %q11.d -> %q10.d", "frint64z %q17.d -> %q16.d",
        "frint64z %q22.d -> %q21.d", "frint64z %q31.d -> %q31.d",
    };
    TEST_LOOP(frint64z, frint64z_vector, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Vdn_q_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Vdn_q_six_offset_1[i], OPSZ_8));
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

    RUN_INSTR_TEST(bti);

    RUN_INSTR_TEST(frint32x);
    RUN_INSTR_TEST(frint32x_vector);
    RUN_INSTR_TEST(frint32z);
    RUN_INSTR_TEST(frint32z_vector);
    RUN_INSTR_TEST(frint64x);
    RUN_INSTR_TEST(frint64x_vector);
    RUN_INSTR_TEST(frint64z);
    RUN_INSTR_TEST(frint64z_vector);

    print("All v8.5 tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
