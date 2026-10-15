/* **********************************************************
 * Copyright (c) 2015-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2022 ARM Limited. All rights reserved.
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

TEST_INSTR(sqrdmlsh_vector)
{

    opnd_t elsz;
    reg_id_t Rd_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rm_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_0[3] = {
        "sqrdmlsh %d0 %d0 %d0 $0x01 -> %d0",
        "sqrdmlsh %d10 %d10 %d10 $0x01 -> %d10",
        "sqrdmlsh %d31 %d31 %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sqrdmlsh_vector(dc, opnd_create_reg(Rd_0[i]),
                                             opnd_create_reg(Rn_0[i]),
                                             opnd_create_reg(Rm_0[i]), elsz);
        if (!test_instr_encoding(dc, OP_sqrdmlsh, instr, expected_0[i]))
            *psuccess = false;
        instr_destroy(dc, instr);
    }

    reg_id_t Rd_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rm_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    elsz = OPND_CREATE_SINGLE();
    const char *expected_1[3] = {
        "sqrdmlsh %d0 %d0 %d0 $0x02 -> %d0",
        "sqrdmlsh %d10 %d10 %d10 $0x02 -> %d10",
        "sqrdmlsh %d31 %d31 %d31 $0x02 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sqrdmlsh_vector(dc, opnd_create_reg(Rd_1[i]),
                                             opnd_create_reg(Rn_1[i]),
                                             opnd_create_reg(Rm_1[i]), elsz);
        if (!test_instr_encoding(dc, OP_sqrdmlsh, instr, expected_1[i]))
            *psuccess = false;
        instr_destroy(dc, instr);
    }

    reg_id_t Rd_2[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_2[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rm_2[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_2[3] = {
        "sqrdmlsh %q0 %q0 %q0 $0x01 -> %q0",
        "sqrdmlsh %q10 %q10 %q10 $0x01 -> %q10",
        "sqrdmlsh %q31 %q31 %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sqrdmlsh_vector(dc, opnd_create_reg(Rd_2[i]),
                                             opnd_create_reg(Rn_2[i]),
                                             opnd_create_reg(Rm_2[i]), elsz);
        if (!test_instr_encoding(dc, OP_sqrdmlsh, instr, expected_2[i]))
            *psuccess = false;
        instr_destroy(dc, instr);
    }

    reg_id_t Rd_3[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_3[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rm_3[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    elsz = OPND_CREATE_SINGLE();
    const char *expected_3[3] = {
        "sqrdmlsh %q0 %q0 %q0 $0x02 -> %q0",
        "sqrdmlsh %q10 %q10 %q10 $0x02 -> %q10",
        "sqrdmlsh %q31 %q31 %q31 $0x02 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sqrdmlsh_vector(dc, opnd_create_reg(Rd_3[i]),
                                             opnd_create_reg(Rn_3[i]),
                                             opnd_create_reg(Rm_3[i]), elsz);
        if (!test_instr_encoding(dc, OP_sqrdmlsh, instr, expected_3[i]))
            *psuccess = false;
        instr_destroy(dc, instr);
    }
}

TEST_INSTR(sqrdmlsh_scalar_idx)
{

    opnd_t elsz;
    reg_id_t Rd_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rm_0[3] = { DR_REG_Q0, DR_REG_Q5, DR_REG_Q15 };
    uint index_0[3] = { 0, 2, 7 };
    elsz = OPND_CREATE_HALF();
    const char *expected_0[3] = {
        "sqrdmlsh %h0 %h0 %q0 $0x00 $0x01 -> %h0",
        "sqrdmlsh %h10 %h10 %q5 $0x02 $0x01 -> %h10",
        "sqrdmlsh %h31 %h31 %q15 $0x07 $0x01 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sqrdmlsh_scalar_idx(
            dc, opnd_create_reg(Rd_0[i]), opnd_create_reg(Rn_0[i]),
            opnd_create_reg(Rm_0[i]), opnd_create_immed_uint(index_0[i], OPSZ_0), elsz);
        if (!test_instr_encoding(dc, OP_sqrdmlsh, instr, expected_0[i]))
            *psuccess = false;
        instr_destroy(dc, instr);
    }

    reg_id_t Rd_1[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rn_1[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rm_1[3] = { DR_REG_Q0, DR_REG_Q5, DR_REG_Q15 };
    uint index_1[3] = { 0, 1, 3 };
    elsz = OPND_CREATE_SINGLE();
    const char *expected_1[3] = {
        "sqrdmlsh %s0 %s0 %q0 $0x00 $0x02 -> %s0",
        "sqrdmlsh %s10 %s10 %q5 $0x01 $0x02 -> %s10",
        "sqrdmlsh %s31 %s31 %q15 $0x03 $0x02 -> %s31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sqrdmlsh_scalar_idx(
            dc, opnd_create_reg(Rd_1[i]), opnd_create_reg(Rn_1[i]),
            opnd_create_reg(Rm_1[i]), opnd_create_immed_uint(index_1[i], OPSZ_0), elsz);
        if (!test_instr_encoding(dc, OP_sqrdmlsh, instr, expected_1[i]))
            *psuccess = false;
        instr_destroy(dc, instr);
    }
}

TEST_INSTR(sqrdmlsh_scalar)
{

    reg_id_t Rd_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rm_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_0[3] = {
        "sqrdmlsh %h0 %h0 %h0 -> %h0",
        "sqrdmlsh %h10 %h10 %h10 -> %h10",
        "sqrdmlsh %h31 %h31 %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sqrdmlsh_scalar(dc, opnd_create_reg(Rd_0[i]),
                                             opnd_create_reg(Rn_0[i]),
                                             opnd_create_reg(Rm_0[i]));
        if (!test_instr_encoding(dc, OP_sqrdmlsh, instr, expected_0[i]))
            *psuccess = false;
        instr_destroy(dc, instr);
    }

    reg_id_t Rd_1[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rn_1[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rm_1[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    const char *expected_1[3] = {
        "sqrdmlsh %s0 %s0 %s0 -> %s0",
        "sqrdmlsh %s10 %s10 %s10 -> %s10",
        "sqrdmlsh %s31 %s31 %s31 -> %s31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sqrdmlsh_scalar(dc, opnd_create_reg(Rd_1[i]),
                                             opnd_create_reg(Rn_1[i]),
                                             opnd_create_reg(Rm_1[i]));
        if (!test_instr_encoding(dc, OP_sqrdmlsh, instr, expected_1[i]))
            *psuccess = false;
        instr_destroy(dc, instr);
    }
}

TEST_INSTR(ldlar)
{

    /* Testing LDLAR   <Wt>, [<Xn|SP>] */
    reg_id_t Rt_0_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_0_0[3] = { DR_REG_X0, DR_REG_X10, DR_REG_SP };
    const char *expected_0_0[3] = {
        "ldlar  (%x0)[4byte] -> %w0",
        "ldlar  (%x10)[4byte] -> %w10",
        "ldlar  (%sp)[4byte] -> %w30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_ldlar(
            dc, opnd_create_reg(Rt_0_0[i]),
            opnd_create_base_disp(Rn_0_0[i], DR_REG_NULL, 0, 0, OPSZ_4));
        if (!test_instr_encoding(dc, OP_ldlar, instr, expected_0_0[i]))
            *psuccess = false;
        instr_destroy(dc, instr);
    }

    /* Testing LDLAR   <Xt>, [<Xn|SP>] */
    reg_id_t Rt_1_0[3] = { DR_REG_X0, DR_REG_X10, DR_REG_X30 };
    reg_id_t Rn_1_0[3] = { DR_REG_X0, DR_REG_X10, DR_REG_SP };
    const char *expected_1_0[3] = {
        "ldlar  (%x0)[8byte] -> %x0",
        "ldlar  (%x10)[8byte] -> %x10",
        "ldlar  (%sp)[8byte] -> %x30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_ldlar(
            dc, opnd_create_reg(Rt_1_0[i]),
            opnd_create_base_disp(Rn_1_0[i], DR_REG_NULL, 0, 0, OPSZ_8));
        if (!test_instr_encoding(dc, OP_ldlar, instr, expected_1_0[i]))
            *psuccess = false;
        instr_destroy(dc, instr);
    }
}

TEST_INSTR(ldlarb)
{

    /* Testing LDLARB  <Wt>, [<Xn|SP>] */
    reg_id_t Rt_0_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_0_0[3] = { DR_REG_X0, DR_REG_X10, DR_REG_SP };
    const char *expected_0_0[3] = {
        "ldlarb (%x0)[1byte] -> %w0",
        "ldlarb (%x10)[1byte] -> %w10",
        "ldlarb (%sp)[1byte] -> %w30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_ldlarb(
            dc, opnd_create_reg(Rt_0_0[i]),
            opnd_create_base_disp(Rn_0_0[i], DR_REG_NULL, 0, 0, OPSZ_1));
        if (!test_instr_encoding(dc, OP_ldlarb, instr, expected_0_0[i]))
            *psuccess = false;
        instr_destroy(dc, instr);
    }
}

TEST_INSTR(ldlarh)
{

    /* Testing LDLARH  <Wt>, [<Xn|SP>] */
    reg_id_t Rt_0_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_0_0[3] = { DR_REG_X0, DR_REG_X10, DR_REG_SP };
    const char *expected_0_0[3] = {
        "ldlarh (%x0)[2byte] -> %w0",
        "ldlarh (%x10)[2byte] -> %w10",
        "ldlarh (%sp)[2byte] -> %w30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_ldlarh(
            dc, opnd_create_reg(Rt_0_0[i]),
            opnd_create_base_disp(Rn_0_0[i], DR_REG_NULL, 0, 0, OPSZ_2));
        if (!test_instr_encoding(dc, OP_ldlarh, instr, expected_0_0[i]))
            *psuccess = false;
        instr_destroy(dc, instr);
    }
}

TEST_INSTR(stllr)
{

    /* Testing STLLR   <Wt>, [<Xn|SP>] */
    reg_id_t Rt_0_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_0_0[3] = { DR_REG_X0, DR_REG_X10, DR_REG_SP };
    const char *expected_0_0[3] = {
        "stllr  (%x0)[4byte] -> %w0",
        "stllr  (%x10)[4byte] -> %w10",
        "stllr  (%sp)[4byte] -> %w30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_stllr(
            dc, opnd_create_reg(Rt_0_0[i]),
            opnd_create_base_disp(Rn_0_0[i], DR_REG_NULL, 0, 0, OPSZ_4));
        if (!test_instr_encoding(dc, OP_stllr, instr, expected_0_0[i]))
            *psuccess = false;
        instr_destroy(dc, instr);
    }

    /* Testing STLLR   <Xt>, [<Xn|SP>] */
    reg_id_t Rt_1_0[3] = { DR_REG_X0, DR_REG_X10, DR_REG_X30 };
    reg_id_t Rn_1_0[3] = { DR_REG_X0, DR_REG_X10, DR_REG_SP };
    const char *expected_1_0[3] = {
        "stllr  (%x0)[8byte] -> %x0",
        "stllr  (%x10)[8byte] -> %x10",
        "stllr  (%sp)[8byte] -> %x30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_stllr(
            dc, opnd_create_reg(Rt_1_0[i]),
            opnd_create_base_disp(Rn_1_0[i], DR_REG_NULL, 0, 0, OPSZ_8));
        if (!test_instr_encoding(dc, OP_stllr, instr, expected_1_0[i]))
            *psuccess = false;
        instr_destroy(dc, instr);
    }
}

TEST_INSTR(stllrb)
{

    /* Testing STLLRB  <Wt>, [<Xn|SP>] */
    reg_id_t Rt_0_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_0_0[3] = { DR_REG_X0, DR_REG_X10, DR_REG_SP };
    const char *expected_0_0[3] = {
        "stllrb (%x0)[1byte] -> %w0",
        "stllrb (%x10)[1byte] -> %w10",
        "stllrb (%sp)[1byte] -> %w30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_stllrb(
            dc, opnd_create_reg(Rt_0_0[i]),
            opnd_create_base_disp(Rn_0_0[i], DR_REG_NULL, 0, 0, OPSZ_1));
        if (!test_instr_encoding(dc, OP_stllrb, instr, expected_0_0[i]))
            *psuccess = false;
        instr_destroy(dc, instr);
    }
}

TEST_INSTR(stllrh)
{

    /* Testing STLLRH  <Wt>, [<Xn|SP>] */
    reg_id_t Rt_0_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_0_0[3] = { DR_REG_X0, DR_REG_X10, DR_REG_SP };
    const char *expected_0_0[3] = {
        "stllrh (%x0)[2byte] -> %w0",
        "stllrh (%x10)[2byte] -> %w10",
        "stllrh (%sp)[2byte] -> %w30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_stllrh(
            dc, opnd_create_reg(Rt_0_0[i]),
            opnd_create_base_disp(Rn_0_0[i], DR_REG_NULL, 0, 0, OPSZ_2));
        if (!test_instr_encoding(dc, OP_stllrh, instr, expected_0_0[i]))
            *psuccess = false;
        instr_destroy(dc, instr);
    }
}

#define DEFINE_CAS_WORD_DOUBLE_TEST(name, mnemonic)                                \
    TEST_INSTR(name)                                                               \
    {                                                                              \
        /* Testing the word form. */                                               \
        TEST_LOOP_EXPECT(                                                          \
            name, 6,                                                               \
            INSTR_CREATE_##name(dc, opnd_create_reg(Wn_six_offset_0[i]),           \
                                opnd_create_reg(Wn_six_offset_1[i]),               \
                                opnd_create_base_disp(Xn_six_offset_2_sp[i],       \
                                                      DR_REG_NULL, 0, 0, OPSZ_4)), \
            {                                                                      \
                EXPECT_DISASSEMBLY(                                                \
                    mnemonic "%w0 %w0 (%x0)[4byte] -> %w0 (%x0)[4byte]",           \
                    mnemonic "%w5 %w6 (%x7)[4byte] -> %w5 (%x7)[4byte]",           \
                    mnemonic "%w10 %w11 (%x12)[4byte] -> %w10 (%x12)[4byte]",      \
                    mnemonic "%w15 %w16 (%x17)[4byte] -> %w15 (%x17)[4byte]",      \
                    mnemonic "%w20 %w21 (%x22)[4byte] -> %w20 (%x22)[4byte]",      \
                    mnemonic "%w30 %w30 (%sp)[4byte] -> %w30 (%sp)[4byte]");       \
                                                                                   \
                EXPECT_TRUE(instr_reads_memory(instr));                            \
                EXPECT_TRUE(instr_writes_memory(instr));                           \
            });                                                                    \
                                                                                   \
        /* Testing the doubleword form. */                                         \
        TEST_LOOP_EXPECT(                                                          \
            name, 6,                                                               \
            INSTR_CREATE_##name(dc, opnd_create_reg(Xn_six_offset_0[i]),           \
                                opnd_create_reg(Xn_six_offset_1[i]),               \
                                opnd_create_base_disp(Xn_six_offset_2_sp[i],       \
                                                      DR_REG_NULL, 0, 0, OPSZ_8)), \
            {                                                                      \
                EXPECT_DISASSEMBLY(                                                \
                    mnemonic "%x0 %x0 (%x0)[8byte] -> %x0 (%x0)[8byte]",           \
                    mnemonic "%x5 %x6 (%x7)[8byte] -> %x5 (%x7)[8byte]",           \
                    mnemonic "%x10 %x11 (%x12)[8byte] -> %x10 (%x12)[8byte]",      \
                    mnemonic "%x15 %x16 (%x17)[8byte] -> %x15 (%x17)[8byte]",      \
                    mnemonic "%x20 %x21 (%x22)[8byte] -> %x20 (%x22)[8byte]",      \
                    mnemonic "%x30 %x30 (%sp)[8byte] -> %x30 (%sp)[8byte]");       \
                                                                                   \
                EXPECT_TRUE(instr_reads_memory(instr));                            \
                EXPECT_TRUE(instr_writes_memory(instr));                           \
            });                                                                    \
    }

#define DEFINE_CAS_BYTE_TEST(name, mnemonic)                                       \
    TEST_INSTR(name)                                                               \
    {                                                                              \
        /* Testing the byte form. */                                               \
        TEST_LOOP_EXPECT(                                                          \
            name, 6,                                                               \
            INSTR_CREATE_##name(dc, opnd_create_reg(Wn_six_offset_0[i]),           \
                                opnd_create_reg(Wn_six_offset_1[i]),               \
                                opnd_create_base_disp(Xn_six_offset_2_sp[i],       \
                                                      DR_REG_NULL, 0, 0, OPSZ_1)), \
            {                                                                      \
                EXPECT_DISASSEMBLY(                                                \
                    mnemonic "%w0 %w0 (%x0)[1byte] -> %w0 (%x0)[1byte]",           \
                    mnemonic "%w5 %w6 (%x7)[1byte] -> %w5 (%x7)[1byte]",           \
                    mnemonic "%w10 %w11 (%x12)[1byte] -> %w10 (%x12)[1byte]",      \
                    mnemonic "%w15 %w16 (%x17)[1byte] -> %w15 (%x17)[1byte]",      \
                    mnemonic "%w20 %w21 (%x22)[1byte] -> %w20 (%x22)[1byte]",      \
                    mnemonic "%w30 %w30 (%sp)[1byte] -> %w30 (%sp)[1byte]");       \
                                                                                   \
                EXPECT_TRUE(instr_reads_memory(instr));                            \
                EXPECT_TRUE(instr_writes_memory(instr));                           \
            });                                                                    \
    }

#define DEFINE_CAS_HALF_TEST(name, mnemonic)                                       \
    TEST_INSTR(name)                                                               \
    {                                                                              \
        /* Testing the halfword form. */                                           \
        TEST_LOOP_EXPECT(                                                          \
            name, 6,                                                               \
            INSTR_CREATE_##name(dc, opnd_create_reg(Wn_six_offset_0[i]),           \
                                opnd_create_reg(Wn_six_offset_1[i]),               \
                                opnd_create_base_disp(Xn_six_offset_2_sp[i],       \
                                                      DR_REG_NULL, 0, 0, OPSZ_2)), \
            {                                                                      \
                EXPECT_DISASSEMBLY(                                                \
                    mnemonic "%w0 %w0 (%x0)[2byte] -> %w0 (%x0)[2byte]",           \
                    mnemonic "%w5 %w6 (%x7)[2byte] -> %w5 (%x7)[2byte]",           \
                    mnemonic "%w10 %w11 (%x12)[2byte] -> %w10 (%x12)[2byte]",      \
                    mnemonic "%w15 %w16 (%x17)[2byte] -> %w15 (%x17)[2byte]",      \
                    mnemonic "%w20 %w21 (%x22)[2byte] -> %w20 (%x22)[2byte]",      \
                    mnemonic "%w30 %w30 (%sp)[2byte] -> %w30 (%sp)[2byte]");       \
                                                                                   \
                EXPECT_TRUE(instr_reads_memory(instr));                            \
                EXPECT_TRUE(instr_writes_memory(instr));                           \
            });                                                                    \
    }

DEFINE_CAS_WORD_DOUBLE_TEST(cas, "cas    ")
DEFINE_CAS_WORD_DOUBLE_TEST(casa, "casa   ")
DEFINE_CAS_BYTE_TEST(casab, "casab  ")
DEFINE_CAS_HALF_TEST(casah, "casah  ")
DEFINE_CAS_WORD_DOUBLE_TEST(casal, "casal  ")
DEFINE_CAS_BYTE_TEST(casalb, "casalb ")
DEFINE_CAS_HALF_TEST(casalh, "casalh ")
DEFINE_CAS_BYTE_TEST(casb, "casb   ")
DEFINE_CAS_HALF_TEST(cash, "cash   ")
DEFINE_CAS_WORD_DOUBLE_TEST(casl, "casl   ")
DEFINE_CAS_BYTE_TEST(caslb, "caslb  ")
DEFINE_CAS_HALF_TEST(caslh, "caslh  ")

#undef DEFINE_CAS_HALF_TEST
#undef DEFINE_CAS_BYTE_TEST
#undef DEFINE_CAS_WORD_DOUBLE_TEST

#define DEFINE_CASP_WORD_DOUBLE_TEST(name, mnemonic)                                  \
    TEST_INSTR(name)                                                                  \
    {                                                                                 \
        const reg_id_t Ws1[6] = { DR_REG_W0,  DR_REG_W4,  DR_REG_W10,                 \
                                  DR_REG_W14, DR_REG_W20, DR_REG_W30 };               \
        const reg_id_t Ws2[6] = { DR_REG_W1,  DR_REG_W5,  DR_REG_W11,                 \
                                  DR_REG_W15, DR_REG_W21, DR_REG_WZR };               \
        const reg_id_t Wt1[6] = { DR_REG_W0,  DR_REG_W6,  DR_REG_W12,                 \
                                  DR_REG_W16, DR_REG_W22, DR_REG_W30 };               \
        const reg_id_t Wt2[6] = { DR_REG_W1,  DR_REG_W7,  DR_REG_W13,                 \
                                  DR_REG_W17, DR_REG_W23, DR_REG_WZR };               \
        const reg_id_t Xs1[6] = { DR_REG_X0,  DR_REG_X4,  DR_REG_X10,                 \
                                  DR_REG_X14, DR_REG_X20, DR_REG_X30 };               \
        const reg_id_t Xs2[6] = { DR_REG_X1,  DR_REG_X5,  DR_REG_X11,                 \
                                  DR_REG_X15, DR_REG_X21, DR_REG_XZR };               \
        const reg_id_t Xt1[6] = { DR_REG_X0,  DR_REG_X6,  DR_REG_X12,                 \
                                  DR_REG_X16, DR_REG_X22, DR_REG_X30 };               \
        const reg_id_t Xt2[6] = { DR_REG_X1,  DR_REG_X7,  DR_REG_X13,                 \
                                  DR_REG_X17, DR_REG_X23, DR_REG_XZR };               \
                                                                                      \
        /* Testing the word pair form. */                                             \
        TEST_LOOP_EXPECT(                                                             \
            name, 6,                                                                  \
            INSTR_CREATE_##name(dc, opnd_create_reg(Ws1[i]), opnd_create_reg(Ws2[i]), \
                                opnd_create_reg(Wt1[i]), opnd_create_reg(Wt2[i]),     \
                                opnd_create_base_disp(Xn_six_offset_2_sp[i],          \
                                                      DR_REG_NULL, 0, 0, OPSZ_8)),    \
            {                                                                         \
                EXPECT_DISASSEMBLY(                                                   \
                    mnemonic "%w0 %w1 %w0 %w1 (%x0)[8byte] -> %w0 %w1 "               \
                             "(%x0)[8byte]",                                          \
                    mnemonic "%w4 %w5 %w6 %w7 (%x7)[8byte] -> %w4 %w5 "               \
                             "(%x7)[8byte]",                                          \
                    mnemonic "%w10 %w11 %w12 %w13 (%x12)[8byte] -> %w10 %w11 "        \
                             "(%x12)[8byte]",                                         \
                    mnemonic "%w14 %w15 %w16 %w17 (%x17)[8byte] -> %w14 %w15 "        \
                             "(%x17)[8byte]",                                         \
                    mnemonic "%w20 %w21 %w22 %w23 (%x22)[8byte] -> %w20 %w21 "        \
                             "(%x22)[8byte]",                                         \
                    mnemonic "%w30 %wzr %w30 %wzr (%sp)[8byte] -> %w30 %wzr "         \
                             "(%sp)[8byte]");                                         \
                                                                                      \
                EXPECT_TRUE(instr_reads_memory(instr));                               \
                EXPECT_TRUE(instr_writes_memory(instr));                              \
            });                                                                       \
                                                                                      \
        /* Testing the doubleword pair form. */                                       \
        TEST_LOOP_EXPECT(                                                             \
            name, 6,                                                                  \
            INSTR_CREATE_##name(dc, opnd_create_reg(Xs1[i]), opnd_create_reg(Xs2[i]), \
                                opnd_create_reg(Xt1[i]), opnd_create_reg(Xt2[i]),     \
                                opnd_create_base_disp(Xn_six_offset_2_sp[i],          \
                                                      DR_REG_NULL, 0, 0, OPSZ_16)),   \
            {                                                                         \
                EXPECT_DISASSEMBLY(                                                   \
                    mnemonic "%x0 %x1 %x0 %x1 (%x0)[16byte] -> %x0 %x1 "              \
                             "(%x0)[16byte]",                                         \
                    mnemonic "%x4 %x5 %x6 %x7 (%x7)[16byte] -> %x4 %x5 "              \
                             "(%x7)[16byte]",                                         \
                    mnemonic "%x10 %x11 %x12 %x13 (%x12)[16byte] -> %x10 %x11 "       \
                             "(%x12)[16byte]",                                        \
                    mnemonic "%x14 %x15 %x16 %x17 (%x17)[16byte] -> %x14 %x15 "       \
                             "(%x17)[16byte]",                                        \
                    mnemonic "%x20 %x21 %x22 %x23 (%x22)[16byte] -> %x20 %x21 "       \
                             "(%x22)[16byte]",                                        \
                    mnemonic "%x30 %xzr %x30 %xzr (%sp)[16byte] -> %x30 %xzr "        \
                             "(%sp)[16byte]");                                        \
                                                                                      \
                EXPECT_TRUE(instr_reads_memory(instr));                               \
                EXPECT_TRUE(instr_writes_memory(instr));                              \
            });                                                                       \
    }

DEFINE_CASP_WORD_DOUBLE_TEST(casp, "casp   ")
DEFINE_CASP_WORD_DOUBLE_TEST(caspa, "caspa  ")
DEFINE_CASP_WORD_DOUBLE_TEST(caspal, "caspal ")
DEFINE_CASP_WORD_DOUBLE_TEST(caspl, "caspl  ")

#undef DEFINE_CASP_WORD_DOUBLE_TEST

#define DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(name, mnemonic)                              \
    TEST_INSTR(name)                                                                    \
    {                                                                                   \
        /* Testing the word form. */                                                    \
        TEST_LOOP_EXPECT(                                                               \
            name, 6,                                                                    \
            INSTR_CREATE_##name(dc, opnd_create_reg(Wn_six_offset_0[i]),                \
                                opnd_create_reg(Wn_six_offset_1[i]),                    \
                                opnd_create_base_disp(Xn_six_offset_2_sp[i],            \
                                                      DR_REG_NULL, 0, 0, OPSZ_4)),      \
            {                                                                           \
                EXPECT_DISASSEMBLY(mnemonic "%w0 (%x0)[4byte] -> %w0 (%x0)[4byte]",     \
                                   mnemonic "%w5 (%x7)[4byte] -> %w6 (%x7)[4byte]",     \
                                   mnemonic "%w10 (%x12)[4byte] -> %w11 (%x12)[4byte]", \
                                   mnemonic "%w15 (%x17)[4byte] -> %w16 (%x17)[4byte]", \
                                   mnemonic "%w20 (%x22)[4byte] -> %w21 (%x22)[4byte]", \
                                   mnemonic "%w30 (%sp)[4byte] -> %w30 (%sp)[4byte]");  \
                                                                                        \
                EXPECT_TRUE(instr_reads_memory(instr));                                 \
                EXPECT_TRUE(instr_writes_memory(instr));                                \
            });                                                                         \
                                                                                        \
        /* Testing the doubleword form. */                                              \
        TEST_LOOP_EXPECT(                                                               \
            name, 6,                                                                    \
            INSTR_CREATE_##name(dc, opnd_create_reg(Xn_six_offset_0[i]),                \
                                opnd_create_reg(Xn_six_offset_1[i]),                    \
                                opnd_create_base_disp(Xn_six_offset_2_sp[i],            \
                                                      DR_REG_NULL, 0, 0, OPSZ_8)),      \
            {                                                                           \
                EXPECT_DISASSEMBLY(mnemonic "%x0 (%x0)[8byte] -> %x0 (%x0)[8byte]",     \
                                   mnemonic "%x5 (%x7)[8byte] -> %x6 (%x7)[8byte]",     \
                                   mnemonic "%x10 (%x12)[8byte] -> %x11 (%x12)[8byte]", \
                                   mnemonic "%x15 (%x17)[8byte] -> %x16 (%x17)[8byte]", \
                                   mnemonic "%x20 (%x22)[8byte] -> %x21 (%x22)[8byte]", \
                                   mnemonic "%x30 (%sp)[8byte] -> %x30 (%sp)[8byte]");  \
                                                                                        \
                EXPECT_TRUE(instr_reads_memory(instr));                                 \
                EXPECT_TRUE(instr_writes_memory(instr));                                \
            });                                                                         \
    }

#define DEFINE_LSE_ATOMIC_BYTE_TEST(name, mnemonic)                                     \
    TEST_INSTR(name)                                                                    \
    {                                                                                   \
        /* Testing the byte form. */                                                    \
        TEST_LOOP_EXPECT(                                                               \
            name, 6,                                                                    \
            INSTR_CREATE_##name(dc, opnd_create_reg(Wn_six_offset_0[i]),                \
                                opnd_create_reg(Wn_six_offset_1[i]),                    \
                                opnd_create_base_disp(Xn_six_offset_2_sp[i],            \
                                                      DR_REG_NULL, 0, 0, OPSZ_1)),      \
            {                                                                           \
                EXPECT_DISASSEMBLY(mnemonic "%w0 (%x0)[1byte] -> %w0 (%x0)[1byte]",     \
                                   mnemonic "%w5 (%x7)[1byte] -> %w6 (%x7)[1byte]",     \
                                   mnemonic "%w10 (%x12)[1byte] -> %w11 (%x12)[1byte]", \
                                   mnemonic "%w15 (%x17)[1byte] -> %w16 (%x17)[1byte]", \
                                   mnemonic "%w20 (%x22)[1byte] -> %w21 (%x22)[1byte]", \
                                   mnemonic "%w30 (%sp)[1byte] -> %w30 (%sp)[1byte]");  \
                                                                                        \
                EXPECT_TRUE(instr_reads_memory(instr));                                 \
                EXPECT_TRUE(instr_writes_memory(instr));                                \
            });                                                                         \
    }

#define DEFINE_LSE_ATOMIC_HALF_TEST(name, mnemonic)                                     \
    TEST_INSTR(name)                                                                    \
    {                                                                                   \
        /* Testing the halfword form. */                                                \
        TEST_LOOP_EXPECT(                                                               \
            name, 6,                                                                    \
            INSTR_CREATE_##name(dc, opnd_create_reg(Wn_six_offset_0[i]),                \
                                opnd_create_reg(Wn_six_offset_1[i]),                    \
                                opnd_create_base_disp(Xn_six_offset_2_sp[i],            \
                                                      DR_REG_NULL, 0, 0, OPSZ_2)),      \
            {                                                                           \
                EXPECT_DISASSEMBLY(mnemonic "%w0 (%x0)[2byte] -> %w0 (%x0)[2byte]",     \
                                   mnemonic "%w5 (%x7)[2byte] -> %w6 (%x7)[2byte]",     \
                                   mnemonic "%w10 (%x12)[2byte] -> %w11 (%x12)[2byte]", \
                                   mnemonic "%w15 (%x17)[2byte] -> %w16 (%x17)[2byte]", \
                                   mnemonic "%w20 (%x22)[2byte] -> %w21 (%x22)[2byte]", \
                                   mnemonic "%w30 (%sp)[2byte] -> %w30 (%sp)[2byte]");  \
                                                                                        \
                EXPECT_TRUE(instr_reads_memory(instr));                                 \
                EXPECT_TRUE(instr_writes_memory(instr));                                \
            });                                                                         \
    }

DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldadd, "ldadd  ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldadda, "ldadda ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldaddab, "ldaddab ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldaddah, "ldaddah ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldaddal, "ldaddal ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldaddalb, "ldaddalb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldaddalh, "ldaddalh ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldaddb, "ldaddb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldaddh, "ldaddh ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldaddl, "ldaddl ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldaddlb, "ldaddlb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldaddlh, "ldaddlh ")

DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldclr, "ldclr  ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldclra, "ldclra ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldclrab, "ldclrab ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldclrah, "ldclrah ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldclral, "ldclral ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldclralb, "ldclralb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldclralh, "ldclralh ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldclrb, "ldclrb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldclrh, "ldclrh ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldclrl, "ldclrl ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldclrlb, "ldclrlb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldclrlh, "ldclrlh ")

DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldeor, "ldeor  ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldeora, "ldeora ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldeorab, "ldeorab ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldeorah, "ldeorah ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldeoral, "ldeoral ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldeoralb, "ldeoralb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldeoralh, "ldeoralh ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldeorb, "ldeorb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldeorh, "ldeorh ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldeorl, "ldeorl ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldeorlb, "ldeorlb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldeorlh, "ldeorlh ")

DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldset, "ldset  ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldseta, "ldseta ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldsetab, "ldsetab ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldsetah, "ldsetah ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldsetal, "ldsetal ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldsetalb, "ldsetalb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldsetalh, "ldsetalh ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldsetb, "ldsetb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldseth, "ldseth ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldsetl, "ldsetl ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldsetlb, "ldsetlb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldsetlh, "ldsetlh ")

DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldsmax, "ldsmax ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldsmaxa, "ldsmaxa ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldsmaxab, "ldsmaxab ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldsmaxah, "ldsmaxah ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldsmaxal, "ldsmaxal ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldsmaxalb, "ldsmaxalb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldsmaxalh, "ldsmaxalh ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldsmaxb, "ldsmaxb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldsmaxh, "ldsmaxh ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldsmaxl, "ldsmaxl ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldsmaxlb, "ldsmaxlb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldsmaxlh, "ldsmaxlh ")

DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldsmin, "ldsmin ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldsmina, "ldsmina ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldsminab, "ldsminab ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldsminah, "ldsminah ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldsminal, "ldsminal ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldsminalb, "ldsminalb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldsminalh, "ldsminalh ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldsminb, "ldsminb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldsminh, "ldsminh ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldsminl, "ldsminl ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldsminlb, "ldsminlb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldsminlh, "ldsminlh ")

DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldumax, "ldumax ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldumaxa, "ldumaxa ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldumaxab, "ldumaxab ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldumaxah, "ldumaxah ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldumaxal, "ldumaxal ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldumaxalb, "ldumaxalb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldumaxalh, "ldumaxalh ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldumaxb, "ldumaxb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldumaxh, "ldumaxh ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldumaxl, "ldumaxl ")
DEFINE_LSE_ATOMIC_BYTE_TEST(ldumaxlb, "ldumaxlb ")
DEFINE_LSE_ATOMIC_HALF_TEST(ldumaxlh, "ldumaxlh ")

DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldumin, "ldumin ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(ldumina, "ldumina ")
DEFINE_LSE_ATOMIC_BYTE_TEST(lduminab, "lduminab ")
DEFINE_LSE_ATOMIC_HALF_TEST(lduminah, "lduminah ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(lduminal, "lduminal ")
DEFINE_LSE_ATOMIC_BYTE_TEST(lduminalb, "lduminalb ")
DEFINE_LSE_ATOMIC_HALF_TEST(lduminalh, "lduminalh ")
DEFINE_LSE_ATOMIC_BYTE_TEST(lduminb, "lduminb ")
DEFINE_LSE_ATOMIC_HALF_TEST(lduminh, "lduminh ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(lduminl, "lduminl ")
DEFINE_LSE_ATOMIC_BYTE_TEST(lduminlb, "lduminlb ")
DEFINE_LSE_ATOMIC_HALF_TEST(lduminlh, "lduminlh ")

DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(swp, "swp    ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(swpa, "swpa   ")
DEFINE_LSE_ATOMIC_BYTE_TEST(swpab, "swpab  ")
DEFINE_LSE_ATOMIC_HALF_TEST(swpah, "swpah  ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(swpal, "swpal  ")
DEFINE_LSE_ATOMIC_BYTE_TEST(swpalb, "swpalb ")
DEFINE_LSE_ATOMIC_HALF_TEST(swpalh, "swpalh ")
DEFINE_LSE_ATOMIC_BYTE_TEST(swpb, "swpb   ")
DEFINE_LSE_ATOMIC_HALF_TEST(swph, "swph   ")
DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST(swpl, "swpl   ")
DEFINE_LSE_ATOMIC_BYTE_TEST(swplb, "swplb  ")
DEFINE_LSE_ATOMIC_HALF_TEST(swplh, "swplh  ")

#undef DEFINE_LSE_ATOMIC_HALF_TEST
#undef DEFINE_LSE_ATOMIC_BYTE_TEST
#undef DEFINE_LSE_ATOMIC_WORD_DOUBLE_TEST

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

    RUN_INSTR_TEST(sqrdmlsh_scalar);
    RUN_INSTR_TEST(sqrdmlsh_scalar_idx);
    RUN_INSTR_TEST(sqrdmlsh_vector);
    RUN_INSTR_TEST(ldlar);
    RUN_INSTR_TEST(ldlarb);
    RUN_INSTR_TEST(ldlarh);
    RUN_INSTR_TEST(stllr);
    RUN_INSTR_TEST(stllrb);
    RUN_INSTR_TEST(stllrh);

    /* LSE */
    RUN_INSTR_TEST(cas);
    RUN_INSTR_TEST(casa);
    RUN_INSTR_TEST(casab);
    RUN_INSTR_TEST(casah);
    RUN_INSTR_TEST(casal);
    RUN_INSTR_TEST(casalb);
    RUN_INSTR_TEST(casalh);
    RUN_INSTR_TEST(casb);
    RUN_INSTR_TEST(cash);
    RUN_INSTR_TEST(casl);
    RUN_INSTR_TEST(caslb);
    RUN_INSTR_TEST(caslh);
    RUN_INSTR_TEST(casp);
    RUN_INSTR_TEST(caspa);
    RUN_INSTR_TEST(caspal);
    RUN_INSTR_TEST(caspl);
    RUN_INSTR_TEST(ldadd);
    RUN_INSTR_TEST(ldadda);
    RUN_INSTR_TEST(ldaddab);
    RUN_INSTR_TEST(ldaddah);
    RUN_INSTR_TEST(ldaddal);
    RUN_INSTR_TEST(ldaddalb);
    RUN_INSTR_TEST(ldaddalh);
    RUN_INSTR_TEST(ldaddb);
    RUN_INSTR_TEST(ldaddh);
    RUN_INSTR_TEST(ldaddl);
    RUN_INSTR_TEST(ldaddlb);
    RUN_INSTR_TEST(ldaddlh);
    RUN_INSTR_TEST(ldclr);
    RUN_INSTR_TEST(ldclra);
    RUN_INSTR_TEST(ldclrab);
    RUN_INSTR_TEST(ldclrah);
    RUN_INSTR_TEST(ldclral);
    RUN_INSTR_TEST(ldclralb);
    RUN_INSTR_TEST(ldclralh);
    RUN_INSTR_TEST(ldclrb);
    RUN_INSTR_TEST(ldclrh);
    RUN_INSTR_TEST(ldclrl);
    RUN_INSTR_TEST(ldclrlb);
    RUN_INSTR_TEST(ldclrlh);
    RUN_INSTR_TEST(ldeor);
    RUN_INSTR_TEST(ldeora);
    RUN_INSTR_TEST(ldeorab);
    RUN_INSTR_TEST(ldeorah);
    RUN_INSTR_TEST(ldeoral);
    RUN_INSTR_TEST(ldeoralb);
    RUN_INSTR_TEST(ldeoralh);
    RUN_INSTR_TEST(ldeorb);
    RUN_INSTR_TEST(ldeorh);
    RUN_INSTR_TEST(ldeorl);
    RUN_INSTR_TEST(ldeorlb);
    RUN_INSTR_TEST(ldeorlh);
    RUN_INSTR_TEST(ldset);
    RUN_INSTR_TEST(ldseta);
    RUN_INSTR_TEST(ldsetab);
    RUN_INSTR_TEST(ldsetah);
    RUN_INSTR_TEST(ldsetal);
    RUN_INSTR_TEST(ldsetalb);
    RUN_INSTR_TEST(ldsetalh);
    RUN_INSTR_TEST(ldsetb);
    RUN_INSTR_TEST(ldseth);
    RUN_INSTR_TEST(ldsetl);
    RUN_INSTR_TEST(ldsetlb);
    RUN_INSTR_TEST(ldsetlh);
    RUN_INSTR_TEST(ldsmax);
    RUN_INSTR_TEST(ldsmaxa);
    RUN_INSTR_TEST(ldsmaxab);
    RUN_INSTR_TEST(ldsmaxah);
    RUN_INSTR_TEST(ldsmaxal);
    RUN_INSTR_TEST(ldsmaxalb);
    RUN_INSTR_TEST(ldsmaxalh);
    RUN_INSTR_TEST(ldsmaxb);
    RUN_INSTR_TEST(ldsmaxh);
    RUN_INSTR_TEST(ldsmaxl);
    RUN_INSTR_TEST(ldsmaxlb);
    RUN_INSTR_TEST(ldsmaxlh);
    RUN_INSTR_TEST(ldsmin);
    RUN_INSTR_TEST(ldsmina);
    RUN_INSTR_TEST(ldsminab);
    RUN_INSTR_TEST(ldsminah);
    RUN_INSTR_TEST(ldsminal);
    RUN_INSTR_TEST(ldsminalb);
    RUN_INSTR_TEST(ldsminalh);
    RUN_INSTR_TEST(ldsminb);
    RUN_INSTR_TEST(ldsminh);
    RUN_INSTR_TEST(ldsminl);
    RUN_INSTR_TEST(ldsminlb);
    RUN_INSTR_TEST(ldsminlh);
    RUN_INSTR_TEST(ldumax);
    RUN_INSTR_TEST(ldumaxa);
    RUN_INSTR_TEST(ldumaxab);
    RUN_INSTR_TEST(ldumaxah);
    RUN_INSTR_TEST(ldumaxal);
    RUN_INSTR_TEST(ldumaxalb);
    RUN_INSTR_TEST(ldumaxalh);
    RUN_INSTR_TEST(ldumaxb);
    RUN_INSTR_TEST(ldumaxh);
    RUN_INSTR_TEST(ldumaxl);
    RUN_INSTR_TEST(ldumaxlb);
    RUN_INSTR_TEST(ldumaxlh);
    RUN_INSTR_TEST(ldumin);
    RUN_INSTR_TEST(ldumina);
    RUN_INSTR_TEST(lduminab);
    RUN_INSTR_TEST(lduminah);
    RUN_INSTR_TEST(lduminal);
    RUN_INSTR_TEST(lduminalb);
    RUN_INSTR_TEST(lduminalh);
    RUN_INSTR_TEST(lduminb);
    RUN_INSTR_TEST(lduminh);
    RUN_INSTR_TEST(lduminl);
    RUN_INSTR_TEST(lduminlb);
    RUN_INSTR_TEST(lduminlh);
    RUN_INSTR_TEST(swp);
    RUN_INSTR_TEST(swpa);
    RUN_INSTR_TEST(swpab);
    RUN_INSTR_TEST(swpah);
    RUN_INSTR_TEST(swpal);
    RUN_INSTR_TEST(swpalb);
    RUN_INSTR_TEST(swpalh);
    RUN_INSTR_TEST(swpb);
    RUN_INSTR_TEST(swph);
    RUN_INSTR_TEST(swpl);
    RUN_INSTR_TEST(swplb);
    RUN_INSTR_TEST(swplh);

    print("All v8.1 tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
