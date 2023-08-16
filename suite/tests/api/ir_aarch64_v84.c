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

TEST_INSTR(ldapur)
{
    /* Testing LDAPUR    <Wt>, [<Xn|SP>{, #<simm>}] */
    const int simm[6] = {
        -256, -86, -1, 0, 169, 255,
    };
    const char *const expected_0_0[6] = {
        "ldapur -0x0100(%x0)[4byte] -> %w0", "ldapur -0x56(%x6)[4byte] -> %w5",
        "ldapur -0x01(%x11)[4byte] -> %w10", "ldapur (%x16)[4byte] -> %w15",
        "ldapur +0xa9(%x21)[4byte] -> %w20", "ldapur +0xff(%sp)[4byte] -> %w30",
    };
    TEST_LOOP(
        ldapur, ldapur, 6, expected_0_0[i], opnd_create_reg(Wn_six_offset_0[i]),
        opnd_create_base_disp(Xn_six_offset_1_sp[i], DR_REG_NULL, 0, simm[i], OPSZ_4));

    /* Testing LDAPUR    <Xt>, [<Xn|SP>{, #<simm>}] */
    const char *const expected_0_1[6] = {
        "ldapur -0x0100(%x0)[8byte] -> %x0", "ldapur -0x56(%x6)[8byte] -> %x5",
        "ldapur -0x01(%x11)[8byte] -> %x10", "ldapur (%x16)[8byte] -> %x15",
        "ldapur +0xa9(%x21)[8byte] -> %x20", "ldapur +0xff(%sp)[8byte] -> %x30",
    };
    TEST_LOOP(
        ldapur, ldapur, 6, expected_0_1[i], opnd_create_reg(Xn_six_offset_0[i]),
        opnd_create_base_disp(Xn_six_offset_1_sp[i], DR_REG_NULL, 0, simm[i], OPSZ_8));
}

TEST_INSTR(ldapurb)
{
    /* Testing LDAPURB    <Wt>, [<Xn|SP>{, #<simm>}] */
    const int simm[6] = {
        -256, -86, -1, 0, 169, 255,
    };
    const char *const expected_0_0[6] = {
        "ldapurb -0x0100(%x0)[1byte] -> %w0", "ldapurb -0x56(%x6)[1byte] -> %w5",
        "ldapurb -0x01(%x11)[1byte] -> %w10", "ldapurb (%x16)[1byte] -> %w15",
        "ldapurb +0xa9(%x21)[1byte] -> %w20", "ldapurb +0xff(%sp)[1byte] -> %w30",
    };
    TEST_LOOP(
        ldapurb, ldapurb, 6, expected_0_0[i], opnd_create_reg(Wn_six_offset_0[i]),
        opnd_create_base_disp(Xn_six_offset_1_sp[i], DR_REG_NULL, 0, simm[i], OPSZ_1));
}

TEST_INSTR(ldapursb)
{
    /* Testing LDAPURSB  <Wt>, [<Xn|SP>{, #<simm>}] */
    const int simm[6] = {
        -256, -86, -1, 0, 169, 255,
    };
    const char *const expected_0_0[6] = {
        "ldapursb -0x0100(%x0)[1byte] -> %w0", "ldapursb -0x56(%x6)[1byte] -> %w5",
        "ldapursb -0x01(%x11)[1byte] -> %w10", "ldapursb (%x16)[1byte] -> %w15",
        "ldapursb +0xa9(%x21)[1byte] -> %w20", "ldapursb +0xff(%sp)[1byte] -> %w30",
    };
    TEST_LOOP(
        ldapursb, ldapursb, 6, expected_0_0[i], opnd_create_reg(Wn_six_offset_0[i]),
        opnd_create_base_disp(Xn_six_offset_1_sp[i], DR_REG_NULL, 0, simm[i], OPSZ_1));

    /* Testing LDAPURSB  <Xt>, [<Xn|SP>{, #<simm>}] */
    const char *const expected_0_1[6] = {
        "ldapursb -0x0100(%x0)[1byte] -> %x0", "ldapursb -0x56(%x6)[1byte] -> %x5",
        "ldapursb -0x01(%x11)[1byte] -> %x10", "ldapursb (%x16)[1byte] -> %x15",
        "ldapursb +0xa9(%x21)[1byte] -> %x20", "ldapursb +0xff(%sp)[1byte] -> %x30",
    };
    TEST_LOOP(
        ldapursb, ldapursb, 6, expected_0_1[i], opnd_create_reg(Xn_six_offset_0[i]),
        opnd_create_base_disp(Xn_six_offset_1_sp[i], DR_REG_NULL, 0, simm[i], OPSZ_1));
}

TEST_INSTR(ldapurh)
{
    /* Testing LDAPURH    <Wt>, [<Xn|SP>{, #<simm>}] */
    const int simm[6] = {
        -256, -86, -1, 0, 169, 255,
    };
    const char *const expected_0_0[6] = {
        "ldapurh -0x0100(%x0)[2byte] -> %w0", "ldapurh -0x56(%x6)[2byte] -> %w5",
        "ldapurh -0x01(%x11)[2byte] -> %w10", "ldapurh (%x16)[2byte] -> %w15",
        "ldapurh +0xa9(%x21)[2byte] -> %w20", "ldapurh +0xff(%sp)[2byte] -> %w30",
    };
    TEST_LOOP(
        ldapurh, ldapurh, 6, expected_0_0[i], opnd_create_reg(Wn_six_offset_0[i]),
        opnd_create_base_disp(Xn_six_offset_1_sp[i], DR_REG_NULL, 0, simm[i], OPSZ_2));
}

TEST_INSTR(ldapursh)
{
    /* Testing LDAPURSH  <Wt>, [<Xn|SP>{, #<simm>}] */
    const int simm[6] = {
        -256, -86, -1, 0, 169, 255,
    };
    const char *const expected_0_0[6] = {
        "ldapursh -0x0100(%x0)[2byte] -> %w0", "ldapursh -0x56(%x6)[2byte] -> %w5",
        "ldapursh -0x01(%x11)[2byte] -> %w10", "ldapursh (%x16)[2byte] -> %w15",
        "ldapursh +0xa9(%x21)[2byte] -> %w20", "ldapursh +0xff(%sp)[2byte] -> %w30",
    };
    TEST_LOOP(
        ldapursh, ldapursh, 6, expected_0_0[i], opnd_create_reg(Wn_six_offset_0[i]),
        opnd_create_base_disp(Xn_six_offset_1_sp[i], DR_REG_NULL, 0, simm[i], OPSZ_2));

    /* Testing LDAPURSH  <Xt>, [<Xn|SP>{, #<simm>}] */
    const char *const expected_0_1[6] = {
        "ldapursh -0x0100(%x0)[2byte] -> %x0", "ldapursh -0x56(%x6)[2byte] -> %x5",
        "ldapursh -0x01(%x11)[2byte] -> %x10", "ldapursh (%x16)[2byte] -> %x15",
        "ldapursh +0xa9(%x21)[2byte] -> %x20", "ldapursh +0xff(%sp)[2byte] -> %x30",
    };
    TEST_LOOP(
        ldapursh, ldapursh, 6, expected_0_1[i], opnd_create_reg(Xn_six_offset_0[i]),
        opnd_create_base_disp(Xn_six_offset_1_sp[i], DR_REG_NULL, 0, simm[i], OPSZ_2));
}

TEST_INSTR(ldapursw)
{
    /* Testing LDAPURSW    <Xt>, [<Xn|SP>{, #<simm>}] */
    const int simm[6] = {
        -256, -86, -1, 0, 169, 255,
    };
    const char *const expected_0_0[6] = {
        "ldapursw -0x0100(%x0)[4byte] -> %x0", "ldapursw -0x56(%x6)[4byte] -> %x5",
        "ldapursw -0x01(%x11)[4byte] -> %x10", "ldapursw (%x16)[4byte] -> %x15",
        "ldapursw +0xa9(%x21)[4byte] -> %x20", "ldapursw +0xff(%sp)[4byte] -> %x30",
    };
    TEST_LOOP(
        ldapursw, ldapursw, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
        opnd_create_base_disp(Xn_six_offset_1_sp[i], DR_REG_NULL, 0, simm[i], OPSZ_4));
}

TEST_INSTR(stlur)
{
    /* Testing STLUR    <Wt>, [<Xn|SP>{, #<simm>}] */
    const int simm[6] = {
        -256, -86, -1, 0, 169, 255,
    };
    const char *const expected_0_0[6] = {
        "stlur  %w0 -> -0x0100(%x0)[4byte]", "stlur  %w5 -> -0x56(%x6)[4byte]",
        "stlur  %w10 -> -0x01(%x11)[4byte]", "stlur  %w15 -> (%x16)[4byte]",
        "stlur  %w20 -> +0xa9(%x21)[4byte]", "stlur  %w30 -> +0xff(%sp)[4byte]",
    };
    TEST_LOOP(
        stlur, stlur, 6, expected_0_0[i], opnd_create_reg(Wn_six_offset_0[i]),
        opnd_create_base_disp(Xn_six_offset_1_sp[i], DR_REG_NULL, 0, simm[i], OPSZ_4));

    /* Testing STLUR    <Xt>, [<Xn|SP>{, #<simm>}] */
    const char *const expected_0_1[6] = {
        "stlur  %x0 -> -0x0100(%x0)[8byte]", "stlur  %x5 -> -0x56(%x6)[8byte]",
        "stlur  %x10 -> -0x01(%x11)[8byte]", "stlur  %x15 -> (%x16)[8byte]",
        "stlur  %x20 -> +0xa9(%x21)[8byte]", "stlur  %x30 -> +0xff(%sp)[8byte]",
    };
    TEST_LOOP(
        stlur, stlur, 6, expected_0_1[i], opnd_create_reg(Xn_six_offset_0[i]),
        opnd_create_base_disp(Xn_six_offset_1_sp[i], DR_REG_NULL, 0, simm[i], OPSZ_8));
}

TEST_INSTR(stlurb)
{
    /* Testing STLURB   <Wt>, [<Xn|SP>{, #<simm>}] */
    const int simm[6] = {
        -256, -86, -1, 0, 169, 255,
    };
    const char *const expected_0_0[6] = {
        "stlurb %w0 -> -0x0100(%x0)[1byte]", "stlurb %w5 -> -0x56(%x6)[1byte]",
        "stlurb %w10 -> -0x01(%x11)[1byte]", "stlurb %w15 -> (%x16)[1byte]",
        "stlurb %w20 -> +0xa9(%x21)[1byte]", "stlurb %w30 -> +0xff(%sp)[1byte]",
    };
    TEST_LOOP(
        stlurb, stlurb, 6, expected_0_0[i], opnd_create_reg(Wn_six_offset_0[i]),
        opnd_create_base_disp(Xn_six_offset_1_sp[i], DR_REG_NULL, 0, simm[i], OPSZ_1));
}

TEST_INSTR(stlurh)
{
    /* Testing STLURH   <Wt>, [<Xn|SP>{, #<simm>}] */
    const int simm[6] = {
        -256, -86, -1, 0, 169, 255,
    };
    const char *const expected_0_0[6] = {
        "stlurh %w0 -> -0x0100(%x0)[2byte]", "stlurh %w5 -> -0x56(%x6)[2byte]",
        "stlurh %w10 -> -0x01(%x11)[2byte]", "stlurh %w15 -> (%x16)[2byte]",
        "stlurh %w20 -> +0xa9(%x21)[2byte]", "stlurh %w30 -> +0xff(%sp)[2byte]",
    };
    TEST_LOOP(
        stlurh, stlurh, 6, expected_0_0[i], opnd_create_reg(Wn_six_offset_0[i]),
        opnd_create_base_disp(Xn_six_offset_1_sp[i], DR_REG_NULL, 0, simm[i], OPSZ_2));
}

TEST_INSTR(cfinv)
{
    /* Testing CFINV */
    TEST_NO_OPNDS(cfinv, cfinv, "cfinv");
}

TEST_INSTR(rmif)
{
    /* Testing RMIF    <Xn>, #<imm1>, #<imm2> */
    static const uint imm6_0_0[6] = { 0, 11, 22, 33, 43, 63 };
    static const uint mask_0_0[6] = { 0, 4, 7, 10, 12, 15 };
    const char *const expected_0_0[6] = {
        "rmif   %x0 $0x00 $0x00",  "rmif   %x5 $0x0b $0x04",  "rmif   %x10 $0x16 $0x07",
        "rmif   %x15 $0x21 $0x0a", "rmif   %x20 $0x2b $0x0c", "rmif   %x30 $0x3f $0x0f",
    };
    TEST_LOOP(rmif, rmif, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_immed_uint(imm6_0_0[i], OPSZ_6b),
              opnd_create_immed_uint(mask_0_0[i], OPSZ_4b));
}

TEST_INSTR(setf16)
{
    /* Testing SETF16  <Wn> */
    const char *const expected_0_0[6] = {
        "setf16 %w0",  "setf16 %w5",  "setf16 %w10",
        "setf16 %w15", "setf16 %w20", "setf16 %w30",
    };
    TEST_LOOP(setf16, setf16, 6, expected_0_0[i], opnd_create_reg(Wn_six_offset_0[i]));
}

TEST_INSTR(setf8)
{
    /* Testing SETF8   <Wn> */
    const char *const expected_0_0[6] = {
        "setf8  %w0",  "setf8  %w5",  "setf8  %w10",
        "setf8  %w15", "setf8  %w20", "setf8  %w30",
    };
    TEST_LOOP(setf8, setf8, 6, expected_0_0[i], opnd_create_reg(Wn_six_offset_0[i]));
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

    /* ARMv8.4-RCPC */
    RUN_INSTR_TEST(ldapur);
    RUN_INSTR_TEST(ldapurb);
    RUN_INSTR_TEST(ldapursb);
    RUN_INSTR_TEST(ldapurh);
    RUN_INSTR_TEST(ldapursh);
    RUN_INSTR_TEST(ldapursw);
    RUN_INSTR_TEST(stlur);
    RUN_INSTR_TEST(stlurb);
    RUN_INSTR_TEST(stlurh);

    /* ARMv4-CondM */
    RUN_INSTR_TEST(cfinv);
    RUN_INSTR_TEST(rmif);
    RUN_INSTR_TEST(setf16);
    RUN_INSTR_TEST(setf8);

    print("All v8.4 tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
