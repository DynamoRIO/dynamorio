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
    }
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

    RUN_INSTR_TEST(sqrdmlsh_scalar);
    RUN_INSTR_TEST(sqrdmlsh_scalar_idx);
    RUN_INSTR_TEST(sqrdmlsh_vector);
    RUN_INSTR_TEST(ldlar);
    RUN_INSTR_TEST(ldlarb);
    RUN_INSTR_TEST(ldlarh);
    RUN_INSTR_TEST(stllr);
    RUN_INSTR_TEST(stllrb);
    RUN_INSTR_TEST(stllrh);

    print("All v8.1 tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
