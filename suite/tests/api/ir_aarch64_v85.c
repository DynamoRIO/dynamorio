/* **********************************************************
 * Copyright (c) 2023 - 2024 ARM Limited. All rights reserved.
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

TEST_INSTR(ldg)
{
    /* Testing LDG     <Xt>, [<Xn|SP>, #<simm>] */
    static const int imm9[6] = { -4096, -2704, -1344, 32, 1392, 4080 };

    TEST_LOOP_EXPECT(
        ldg, 6,
        INSTR_CREATE_ldg(dc, opnd_create_reg(Xn_six_offset_0[i]),
                         opnd_create_base_disp_aarch64(Xn_six_offset_1_sp[i], DR_REG_NULL,
                                                       DR_EXTEND_UXTX, 0, imm9[i], 0,
                                                       OPSZ_0)),
        {
            EXPECT_DISASSEMBLY(
                "ldg    %x0 -0x1000(%x0) -> %x0", "ldg    %x5 -0x0a90(%x6) -> %x5",
                "ldg    %x10 -0x0540(%x11) -> %x10", "ldg    %x15 +0x20(%x16) -> %x15",
                "ldg    %x20 +0x0570(%x21) -> %x20", "ldg    %x30 +0x0ff0(%sp) -> %x30");
            EXPECT_FALSE(instr_reads_memory(instr));
            EXPECT_FALSE(instr_writes_memory(instr));
        });
}

TEST_INSTR(st2g)
{
    static const int imm9[6] = { -4096, -2704, -1344, 32, 1392, 4080 };

    /* Testing ST2G    <Xt|SP>, [<Xn|SP>], #<simm> */
    TEST_LOOP_EXPECT(
        st2g, 6,
        INSTR_CREATE_st2g_post(
            dc,
            opnd_create_base_disp_aarch64(Xn_six_offset_0_sp[i], DR_REG_NULL,
                                          DR_EXTEND_UXTX, 0, 0, 0, OPSZ_0),
            opnd_create_reg(Xn_six_offset_1_sp[i]), OPND_CREATE_INT(imm9[i])),
        {
            EXPECT_DISASSEMBLY("st2g   %x0 %x0 $0xfffffffffffff000 -> (%x0) %x0",
                               "st2g   %x6 %x5 $0xfffffffffffff570 -> (%x5) %x5",
                               "st2g   %x11 %x10 $0xfffffffffffffac0 -> (%x10) %x10",
                               "st2g   %x16 %x15 $0x0000000000000020 -> (%x15) %x15",
                               "st2g   %x21 %x20 $0x0000000000000570 -> (%x20) %x20",
                               "st2g   %sp %sp $0x0000000000000ff0 -> (%sp) %sp");
            EXPECT_FALSE(instr_reads_memory(instr));
            EXPECT_FALSE(instr_writes_memory(instr));
        });

    /* Testing ST2G    <Xt|SP>, [<Xn|SP>, #<simm>]! */
    TEST_LOOP_EXPECT(
        st2g, 6,
        INSTR_CREATE_st2g_pre(
            dc,
            opnd_create_base_disp(Xn_six_offset_0_sp[i], DR_REG_NULL, 0, imm9[i], OPSZ_0),
            opnd_create_reg(Xn_six_offset_1_sp[i])),
        {
            EXPECT_DISASSEMBLY(
                "st2g   %x0 %x0 $0xfffffffffffff000 -> -0x1000(%x0) %x0",
                "st2g   %x6 %x5 $0xfffffffffffff570 -> -0x0a90(%x5) %x5",
                "st2g   %x11 %x10 $0xfffffffffffffac0 -> -0x0540(%x10) %x10",
                "st2g   %x16 %x15 $0x0000000000000020 -> +0x20(%x15) %x15",
                "st2g   %x21 %x20 $0x0000000000000570 -> +0x0570(%x20) %x20",
                "st2g   %sp %sp $0x0000000000000ff0 -> +0x0ff0(%sp) %sp");
            EXPECT_FALSE(instr_reads_memory(instr));
            EXPECT_FALSE(instr_writes_memory(instr));
        });

    /* Testing ST2G    <Xt|SP>, [<Xn|SP>, #<simm>] */
    TEST_LOOP_EXPECT(st2g, 6,
                     INSTR_CREATE_st2g_offset(dc,
                                              opnd_create_base_disp_aarch64(
                                                  Xn_six_offset_0_sp[i], DR_REG_NULL,
                                                  DR_EXTEND_UXTX, 0, imm9[i], 0, OPSZ_0),
                                              opnd_create_reg(Xn_six_offset_1_sp[i])),
                     {
                         EXPECT_DISASSEMBLY(
                             "st2g   %x0 -> -0x1000(%x0)", "st2g   %x6 -> -0x0a90(%x5)",
                             "st2g   %x11 -> -0x0540(%x10)", "st2g   %x16 -> +0x20(%x15)",
                             "st2g   %x21 -> +0x0570(%x20)",
                             "st2g   %sp -> +0x0ff0(%sp)");
                         EXPECT_FALSE(instr_reads_memory(instr));
                         EXPECT_FALSE(instr_writes_memory(instr));
                     });
}

TEST_INSTR(stg)
{
    static const int imm9[6] = { -4096, -2704, -1344, 32, 1392, 4080 };

    /* Testing STG     <Xt|SP>, [<Xn|SP>], #<simm> */
    TEST_LOOP_EXPECT(
        stg, 6,
        INSTR_CREATE_stg_post(
            dc,
            opnd_create_base_disp_aarch64(Xn_six_offset_0_sp[i], DR_REG_NULL,
                                          DR_EXTEND_UXTX, 0, 0, 0, OPSZ_0),
            opnd_create_reg(Xn_six_offset_1_sp[i]), OPND_CREATE_INT(imm9[i])),
        {
            EXPECT_DISASSEMBLY("stg    %x0 %x0 $0xfffffffffffff000 -> (%x0) %x0",
                               "stg    %x6 %x5 $0xfffffffffffff570 -> (%x5) %x5",
                               "stg    %x11 %x10 $0xfffffffffffffac0 -> (%x10) %x10",
                               "stg    %x16 %x15 $0x0000000000000020 -> (%x15) %x15",
                               "stg    %x21 %x20 $0x0000000000000570 -> (%x20) %x20",
                               "stg    %sp %sp $0x0000000000000ff0 -> (%sp) %sp");
            EXPECT_FALSE(instr_reads_memory(instr));
            EXPECT_FALSE(instr_writes_memory(instr));
        });

    /* Testing STG     <Xt|SP>, [<Xn|SP>, #<simm>]! */
    TEST_LOOP_EXPECT(
        stg, 6,
        INSTR_CREATE_stg_pre(
            dc,
            opnd_create_base_disp(Xn_six_offset_0_sp[i], DR_REG_NULL, 0, imm9[i], OPSZ_0),
            opnd_create_reg(Xn_six_offset_1_sp[i])),
        {
            EXPECT_DISASSEMBLY(
                "stg    %x0 %x0 $0xfffffffffffff000 -> -0x1000(%x0) %x0",
                "stg    %x6 %x5 $0xfffffffffffff570 -> -0x0a90(%x5) %x5",
                "stg    %x11 %x10 $0xfffffffffffffac0 -> -0x0540(%x10) %x10",
                "stg    %x16 %x15 $0x0000000000000020 -> +0x20(%x15) %x15",
                "stg    %x21 %x20 $0x0000000000000570 -> +0x0570(%x20) %x20",
                "stg    %sp %sp $0x0000000000000ff0 -> +0x0ff0(%sp) %sp");
            EXPECT_FALSE(instr_reads_memory(instr));
            EXPECT_FALSE(instr_writes_memory(instr));
        });

    /* Testing STG     <Xt|SP>, [<Xn|SP>, #<simm>] */
    TEST_LOOP_EXPECT(stg, 6,
                     INSTR_CREATE_stg_offset(dc,
                                             opnd_create_base_disp_aarch64(
                                                 Xn_six_offset_0_sp[i], DR_REG_NULL,
                                                 DR_EXTEND_UXTX, 0, imm9[i], 0, OPSZ_0),
                                             opnd_create_reg(Xn_six_offset_1_sp[i])),
                     {
                         EXPECT_DISASSEMBLY(
                             "stg    %x0 -> -0x1000(%x0)", "stg    %x6 -> -0x0a90(%x5)",
                             "stg    %x11 -> -0x0540(%x10)", "stg    %x16 -> +0x20(%x15)",
                             "stg    %x21 -> +0x0570(%x20)",
                             "stg    %sp -> +0x0ff0(%sp)");
                         EXPECT_FALSE(instr_reads_memory(instr));
                         EXPECT_FALSE(instr_writes_memory(instr));
                     });
}

TEST_INSTR(stz2g)
{
    static const int imm9[6] = { -4096, -2704, -1344, 32, 1392, 4080 };

    /* Testing STZ2G   <Xt|SP>, [<Xn|SP>], #<simm> */
    TEST_LOOP_EXPECT(
        stz2g, 6,
        INSTR_CREATE_stz2g_post(
            dc,
            opnd_create_base_disp_aarch64(Xn_six_offset_0_sp[i], DR_REG_NULL,
                                          DR_EXTEND_UXTX, 0, 0, 0, OPSZ_32),
            opnd_create_reg(Xn_six_offset_1_sp[i]), OPND_CREATE_INT(imm9[i])),
        {
            EXPECT_DISASSEMBLY(
                "stz2g  %x0 %x0 $0xfffffffffffff000 -> (%x0)[32byte] %x0",
                "stz2g  %x6 %x5 $0xfffffffffffff570 -> (%x5)[32byte] %x5",
                "stz2g  %x11 %x10 $0xfffffffffffffac0 -> (%x10)[32byte] %x10",
                "stz2g  %x16 %x15 $0x0000000000000020 -> (%x15)[32byte] %x15",
                "stz2g  %x21 %x20 $0x0000000000000570 -> (%x20)[32byte] %x20",
                "stz2g  %sp %sp $0x0000000000000ff0 -> (%sp)[32byte] %sp");
            EXPECT_FALSE(instr_reads_memory(instr));
            EXPECT_TRUE(instr_writes_memory(instr));
        });

    /* Testing STZ2G   <Xt|SP>, [<Xn|SP>, #<simm>]! */
    TEST_LOOP_EXPECT(
        stz2g, 6,
        INSTR_CREATE_stz2g_pre(dc,
                               opnd_create_base_disp(Xn_six_offset_0_sp[i], DR_REG_NULL,
                                                     0, imm9[i], OPSZ_32),
                               opnd_create_reg(Xn_six_offset_1_sp[i])),
        {
            EXPECT_DISASSEMBLY(
                "stz2g  %x0 %x0 $0xfffffffffffff000 -> -0x1000(%x0)[32byte] %x0",
                "stz2g  %x6 %x5 $0xfffffffffffff570 -> -0x0a90(%x5)[32byte] %x5",
                "stz2g  %x11 %x10 $0xfffffffffffffac0 -> -0x0540(%x10)[32byte] %x10",
                "stz2g  %x16 %x15 $0x0000000000000020 -> +0x20(%x15)[32byte] %x15",
                "stz2g  %x21 %x20 $0x0000000000000570 -> +0x0570(%x20)[32byte] %x20",
                "stz2g  %sp %sp $0x0000000000000ff0 -> +0x0ff0(%sp)[32byte] %sp");
            EXPECT_FALSE(instr_reads_memory(instr));
            EXPECT_TRUE(instr_writes_memory(instr));
        });

    /* Testing STZ2G   <Xt|SP>, [<Xn|SP>, #<simm>] */
    TEST_LOOP_EXPECT(
        stz2g, 6,
        INSTR_CREATE_stz2g_offset(
            dc,
            opnd_create_base_disp_aarch64(Xn_six_offset_0_sp[i], DR_REG_NULL,
                                          DR_EXTEND_UXTX, 0, imm9[i], 0, OPSZ_32),
            opnd_create_reg(Xn_six_offset_1_sp[i])),
        {
            EXPECT_DISASSEMBLY("stz2g  %x0 -> -0x1000(%x0)[32byte]",
                               "stz2g  %x6 -> -0x0a90(%x5)[32byte]",
                               "stz2g  %x11 -> -0x0540(%x10)[32byte]",
                               "stz2g  %x16 -> +0x20(%x15)[32byte]",
                               "stz2g  %x21 -> +0x0570(%x20)[32byte]",
                               "stz2g  %sp -> +0x0ff0(%sp)[32byte]");
            EXPECT_FALSE(instr_reads_memory(instr));
            EXPECT_TRUE(instr_writes_memory(instr));
        });
}

TEST_INSTR(stzg)
{
    static const int imm9[6] = { -4096, -2704, -1344, 32, 1392, 4080 };

    /* Testing STZG    <Xt|SP>, [<Xn|SP>], #<simm> */
    TEST_LOOP_EXPECT(
        stzg, 6,
        INSTR_CREATE_stzg_post(
            dc,
            opnd_create_base_disp_aarch64(Xn_six_offset_0_sp[i], DR_REG_NULL,
                                          DR_EXTEND_UXTX, 0, 0, 0, OPSZ_16),
            opnd_create_reg(Xn_six_offset_1_sp[i]), OPND_CREATE_INT(imm9[i])),
        {
            EXPECT_DISASSEMBLY(
                "stzg   %x0 %x0 $0xfffffffffffff000 -> (%x0)[16byte] %x0",
                "stzg   %x6 %x5 $0xfffffffffffff570 -> (%x5)[16byte] %x5",
                "stzg   %x11 %x10 $0xfffffffffffffac0 -> (%x10)[16byte] %x10",
                "stzg   %x16 %x15 $0x0000000000000020 -> (%x15)[16byte] %x15",
                "stzg   %x21 %x20 $0x0000000000000570 -> (%x20)[16byte] %x20",
                "stzg   %sp %sp $0x0000000000000ff0 -> (%sp)[16byte] %sp");
            EXPECT_FALSE(instr_reads_memory(instr));
            EXPECT_TRUE(instr_writes_memory(instr));
        });

    /* Testing STZG    <Xt|SP>, [<Xn|SP>, #<simm>]! */
    TEST_LOOP_EXPECT(
        stzg, 6,
        INSTR_CREATE_stzg_pre(dc,
                              opnd_create_base_disp(Xn_six_offset_0_sp[i], DR_REG_NULL, 0,
                                                    imm9[i], OPSZ_16),
                              opnd_create_reg(Xn_six_offset_1_sp[i])),
        {
            EXPECT_DISASSEMBLY(
                "stzg   %x0 %x0 $0xfffffffffffff000 -> -0x1000(%x0)[16byte] %x0",
                "stzg   %x6 %x5 $0xfffffffffffff570 -> -0x0a90(%x5)[16byte] %x5",
                "stzg   %x11 %x10 $0xfffffffffffffac0 -> -0x0540(%x10)[16byte] %x10",
                "stzg   %x16 %x15 $0x0000000000000020 -> +0x20(%x15)[16byte] %x15",
                "stzg   %x21 %x20 $0x0000000000000570 -> +0x0570(%x20)[16byte] %x20",
                "stzg   %sp %sp $0x0000000000000ff0 -> +0x0ff0(%sp)[16byte] %sp");
            EXPECT_FALSE(instr_reads_memory(instr));
            EXPECT_TRUE(instr_writes_memory(instr));
        });

    /* Testing STZG    <Xt|SP>, [<Xn|SP>, #<simm>] */
    TEST_LOOP_EXPECT(stzg, 6,
                     INSTR_CREATE_stzg_offset(dc,
                                              opnd_create_base_disp_aarch64(
                                                  Xn_six_offset_0_sp[i], DR_REG_NULL,
                                                  DR_EXTEND_UXTX, 0, imm9[i], 0, OPSZ_16),
                                              opnd_create_reg(Xn_six_offset_1_sp[i])),
                     {
                         EXPECT_DISASSEMBLY("stzg   %x0 -> -0x1000(%x0)[16byte]",
                                            "stzg   %x6 -> -0x0a90(%x5)[16byte]",
                                            "stzg   %x11 -> -0x0540(%x10)[16byte]",
                                            "stzg   %x16 -> +0x20(%x15)[16byte]",
                                            "stzg   %x21 -> +0x0570(%x20)[16byte]",
                                            "stzg   %sp -> +0x0ff0(%sp)[16byte]");
                         EXPECT_FALSE(instr_reads_memory(instr));
                         EXPECT_TRUE(instr_writes_memory(instr));
                     });
}

TEST_INSTR(stgp)
{
    static const int imm7[6] = { -1024, -640, -304, 48, 384, 1008 };

    /* Testing STGP    <Xt>, <Xt2>, [<Xn|SP>], #<simm> */
    TEST_LOOP_EXPECT(
        stgp, 6,
        INSTR_CREATE_stgp_post(
            dc,
            opnd_create_base_disp_aarch64(Xn_six_offset_0_sp[i], DR_REG_NULL,
                                          DR_EXTEND_UXTX, 0, 0, 0, OPSZ_16),
            opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]),
            OPND_CREATE_INT(imm7[i])),
        {
            EXPECT_DISASSEMBLY(
                "stgp   %x0 %x0 %x0 $0xfffffffffffffc00 -> (%x0)[16byte] %x0",
                "stgp   %x6 %x7 %x5 $0xfffffffffffffd80 -> (%x5)[16byte] %x5",
                "stgp   %x11 %x12 %x10 $0xfffffffffffffed0 -> (%x10)[16byte] %x10",
                "stgp   %x16 %x17 %x15 $0x0000000000000030 -> (%x15)[16byte] %x15",
                "stgp   %x21 %x22 %x20 $0x0000000000000180 -> (%x20)[16byte] %x20",
                "stgp   %x30 %x30 %sp $0x00000000000003f0 -> (%sp)[16byte] %sp");
            EXPECT_FALSE(instr_reads_memory(instr));
            EXPECT_TRUE(instr_writes_memory(instr));
        });

    /* Testing STGP    <Xt>, <Xt2>, [<Xn|SP>, #<simm>]! */
    TEST_LOOP_EXPECT(
        stgp, 6,
        INSTR_CREATE_stgp_pre(dc,
                              opnd_create_base_disp(Xn_six_offset_0_sp[i], DR_REG_NULL, 0,
                                                    imm7[i], OPSZ_16),
                              opnd_create_reg(Xn_six_offset_1[i]),
                              opnd_create_reg(Xn_six_offset_2[i])),
        {
            EXPECT_DISASSEMBLY(
                "stgp   %x0 %x0 %x0 $0xfffffffffffffc00 -> -0x0400(%x0)[16byte] %x0",
                "stgp   %x6 %x7 %x5 $0xfffffffffffffd80 -> -0x0280(%x5)[16byte] %x5",
                "stgp   %x11 %x12 %x10 $0xfffffffffffffed0 -> -0x0130(%x10)[16byte] %x10",
                "stgp   %x16 %x17 %x15 $0x0000000000000030 -> +0x30(%x15)[16byte] %x15",
                "stgp   %x21 %x22 %x20 $0x0000000000000180 -> +0x0180(%x20)[16byte] %x20",
                "stgp   %x30 %x30 %sp $0x00000000000003f0 -> +0x03f0(%sp)[16byte] %sp");
            EXPECT_FALSE(instr_reads_memory(instr));
            EXPECT_TRUE(instr_writes_memory(instr));
        });

    /* Testing STGP    <Xt>, <Xt2>, [<Xn|SP>, #<simm>] */
    TEST_LOOP_EXPECT(
        stgp, 6,
        INSTR_CREATE_stgp_offset(
            dc,
            opnd_create_base_disp_aarch64(Xn_six_offset_2_sp[i], DR_REG_NULL,
                                          DR_EXTEND_UXTX, 0, imm7[i], 0, OPSZ_16),
            opnd_create_reg(Xn_six_offset_0[i]), opnd_create_reg(Xn_six_offset_1[i])),
        {
            EXPECT_DISASSEMBLY("stgp   %x0 %x0 -> -0x0400(%x0)[16byte]",
                               "stgp   %x5 %x6 -> -0x0280(%x7)[16byte]",
                               "stgp   %x10 %x11 -> -0x0130(%x12)[16byte]",
                               "stgp   %x15 %x16 -> +0x30(%x17)[16byte]",
                               "stgp   %x20 %x21 -> +0x0180(%x22)[16byte]",
                               "stgp   %x30 %x30 -> +0x03f0(%sp)[16byte]");
            EXPECT_FALSE(instr_reads_memory(instr));
            EXPECT_TRUE(instr_writes_memory(instr));
        });
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

TEST_INSTR(stgm)
{
    /* Testing STGM    <Xt|SP>, [<Xn|SP>] */
    TEST_LOOP_EXPECT(
        stgm, 6,
        INSTR_CREATE_stgm(dc,
                          opnd_create_base_disp_aarch64(Xn_six_offset_0_sp[i],
                                                        DR_REG_NULL, DR_EXTEND_UXTX, 0, 0,
                                                        0, OPSZ_0),
                          opnd_create_reg(Xn_six_offset_1[i])),
        {
            EXPECT_DISASSEMBLY("stgm   %x0 -> (%x0)", "stgm   %x6 -> (%x5)",
                               "stgm   %x11 -> (%x10)", "stgm   %x16 -> (%x15)",
                               "stgm   %x21 -> (%x20)", "stgm   %x30 -> (%sp)");
            EXPECT_FALSE(instr_reads_memory(instr));
            EXPECT_FALSE(instr_writes_memory(instr));
        });
}

TEST_INSTR(stzgm)
{
    /* Testing STZGM   <Xt|SP>, [<Xn|SP>] */
    TEST_LOOP_EXPECT(stzgm, 6,
                     INSTR_CREATE_stzgm(
                         dc,
                         opnd_create_base_disp_aarch64(Xn_six_offset_0_sp[i], DR_REG_NULL,
                                                       DR_EXTEND_UXTX, 0, 0, 0, OPSZ_16),
                         opnd_create_reg(Xn_six_offset_1[i])),
                     {
                         EXPECT_DISASSEMBLY("stzgm  %x0 -> (%x0)[16byte]",
                                            "stzgm  %x6 -> (%x5)[16byte]",
                                            "stzgm  %x11 -> (%x10)[16byte]",
                                            "stzgm  %x16 -> (%x15)[16byte]",
                                            "stzgm  %x21 -> (%x20)[16byte]",
                                            "stzgm  %x30 -> (%sp)[16byte]", );
                         EXPECT_FALSE(instr_reads_memory(instr));
                         EXPECT_TRUE(instr_writes_memory(instr));
                     });
}

TEST_INSTR(ldgm)
{
    /* Testing LDGM    <Xt>, [<Xn|SP>] */
    TEST_LOOP_EXPECT(
        ldgm, 6,
        INSTR_CREATE_ldgm(dc, opnd_create_reg(Xn_six_offset_0[i]),
                          opnd_create_base_disp_aarch64(Xn_six_offset_1_sp[i],
                                                        DR_REG_NULL, DR_EXTEND_UXTX, 0, 0,
                                                        0, OPSZ_0)),
        {
            EXPECT_DISASSEMBLY("ldgm   (%x0) -> %x0", "ldgm   (%x6) -> %x5",
                               "ldgm   (%x11) -> %x10", "ldgm   (%x16) -> %x15",
                               "ldgm   (%x21) -> %x20", "ldgm   (%sp) -> %x30");
            EXPECT_FALSE(instr_reads_memory(instr));
            EXPECT_FALSE(instr_writes_memory(instr));
        });
}

TEST_INSTR(axflag)
{
    /* Testing AXFLAG */
    TEST_LOOP_EXPECT(axflag, 1, INSTR_CREATE_axflag(dc), {
        EXPECT_DISASSEMBLY("axflag");
        EXPECT_TRUE(
            TEST(EFLAGS_READ_NZCV, instr_get_arith_flags(instr, DR_QUERY_INCLUDE_ALL)));
        EXPECT_TRUE(
            TEST(EFLAGS_WRITE_NZCV, instr_get_arith_flags(instr, DR_QUERY_INCLUDE_ALL)));
    });
}

TEST_INSTR(xaflag)
{
    /* Testing XAFLAG */
    TEST_LOOP_EXPECT(xaflag, 1, INSTR_CREATE_xaflag(dc), {
        EXPECT_DISASSEMBLY("xaflag");
        EXPECT_TRUE(
            TEST(EFLAGS_READ_NZCV, instr_get_arith_flags(instr, DR_QUERY_INCLUDE_ALL)));
        EXPECT_TRUE(
            TEST(EFLAGS_WRITE_NZCV, instr_get_arith_flags(instr, DR_QUERY_INCLUDE_ALL)));
    });
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

    /* FEAT_MTE */
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

    /* FEAT_MTE2 */
    RUN_INSTR_TEST(stgm);
    RUN_INSTR_TEST(stzgm);
    RUN_INSTR_TEST(ldgm);

    /* FEAT_FlagM2 */
    RUN_INSTR_TEST(axflag);
    RUN_INSTR_TEST(xaflag);

    print("All v8.5 tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
