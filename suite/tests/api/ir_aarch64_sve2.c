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

TEST_INSTR(aesd_sve)
{

    /* Testing AESD    <Zdn>.B, <Zdn>.B, <Zm>.B */
    const char *const expected_0_0[6] = {
        "aesd   %z0.b %z0.b -> %z0.b",    "aesd   %z5.b %z6.b -> %z5.b",
        "aesd   %z10.b %z11.b -> %z10.b", "aesd   %z16.b %z17.b -> %z16.b",
        "aesd   %z21.b %z22.b -> %z21.b", "aesd   %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(aesd, aesd_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1));
}

TEST_INSTR(aese_sve)
{

    /* Testing AESE    <Zdn>.B, <Zdn>.B, <Zm>.B */
    const char *const expected_0_0[6] = {
        "aese   %z0.b %z0.b -> %z0.b",    "aese   %z5.b %z6.b -> %z5.b",
        "aese   %z10.b %z11.b -> %z10.b", "aese   %z16.b %z17.b -> %z16.b",
        "aese   %z21.b %z22.b -> %z21.b", "aese   %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(aese, aese_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1));
}

TEST_INSTR(bcax_sve)
{

    /* Testing BCAX    <Zdn>.D, <Zdn>.D, <Zm>.D, <Zk>.D */
    const char *const expected_0_0[6] = {
        "bcax   %z0.d %z0.d %z0.d -> %z0.d",     "bcax   %z5.d %z6.d %z7.d -> %z5.d",
        "bcax   %z10.d %z11.d %z12.d -> %z10.d", "bcax   %z16.d %z17.d %z18.d -> %z16.d",
        "bcax   %z21.d %z22.d %z23.d -> %z21.d", "bcax   %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(bcax, bcax_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(bsl1n_sve)
{

    /* Testing BSL1N   <Zdn>.D, <Zdn>.D, <Zm>.D, <Zk>.D */
    const char *const expected_0_0[6] = {
        "bsl1n  %z0.d %z0.d %z0.d -> %z0.d",     "bsl1n  %z5.d %z6.d %z7.d -> %z5.d",
        "bsl1n  %z10.d %z11.d %z12.d -> %z10.d", "bsl1n  %z16.d %z17.d %z18.d -> %z16.d",
        "bsl1n  %z21.d %z22.d %z23.d -> %z21.d", "bsl1n  %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(bsl1n, bsl1n_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(bsl2n_sve)
{

    /* Testing BSL2N   <Zdn>.D, <Zdn>.D, <Zm>.D, <Zk>.D */
    const char *const expected_0_0[6] = {
        "bsl2n  %z0.d %z0.d %z0.d -> %z0.d",     "bsl2n  %z5.d %z6.d %z7.d -> %z5.d",
        "bsl2n  %z10.d %z11.d %z12.d -> %z10.d", "bsl2n  %z16.d %z17.d %z18.d -> %z16.d",
        "bsl2n  %z21.d %z22.d %z23.d -> %z21.d", "bsl2n  %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(bsl2n, bsl2n_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(bsl_sve)
{

    /* Testing BSL     <Zdn>.D, <Zdn>.D, <Zm>.D, <Zk>.D */
    const char *const expected_0_0[6] = {
        "bsl    %z0.d %z0.d %z0.d -> %z0.d",     "bsl    %z5.d %z6.d %z7.d -> %z5.d",
        "bsl    %z10.d %z11.d %z12.d -> %z10.d", "bsl    %z16.d %z17.d %z18.d -> %z16.d",
        "bsl    %z21.d %z22.d %z23.d -> %z21.d", "bsl    %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(bsl, bsl_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(eor3_sve)
{

    /* Testing EOR3    <Zdn>.D, <Zdn>.D, <Zm>.D, <Zk>.D */
    const char *const expected_0_0[6] = {
        "eor3   %z0.d %z0.d %z0.d -> %z0.d",     "eor3   %z5.d %z6.d %z7.d -> %z5.d",
        "eor3   %z10.d %z11.d %z12.d -> %z10.d", "eor3   %z16.d %z17.d %z18.d -> %z16.d",
        "eor3   %z21.d %z22.d %z23.d -> %z21.d", "eor3   %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(eor3, eor3_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(fmlalb_sve)
{

    /* Testing FMLALB  <Zda>.S, <Zn>.H, <Zm>.H */
    const char *const expected_0_0[6] = {
        "fmlalb %z0.s %z0.h %z0.h -> %z0.s",     "fmlalb %z5.s %z6.h %z7.h -> %z5.s",
        "fmlalb %z10.s %z11.h %z12.h -> %z10.s", "fmlalb %z16.s %z17.h %z18.h -> %z16.s",
        "fmlalb %z21.s %z22.h %z23.h -> %z21.s", "fmlalb %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(fmlalb, fmlalb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));
}

TEST_INSTR(fmlalt_sve)
{

    /* Testing FMLALT  <Zda>.S, <Zn>.H, <Zm>.H */
    const char *const expected_0_0[6] = {
        "fmlalt %z0.s %z0.h %z0.h -> %z0.s",     "fmlalt %z5.s %z6.h %z7.h -> %z5.s",
        "fmlalt %z10.s %z11.h %z12.h -> %z10.s", "fmlalt %z16.s %z17.h %z18.h -> %z16.s",
        "fmlalt %z21.s %z22.h %z23.h -> %z21.s", "fmlalt %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(fmlalt, fmlalt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));
}

TEST_INSTR(fmlslb_sve)
{

    /* Testing FMLSLB  <Zda>.S, <Zn>.H, <Zm>.H */
    const char *const expected_0_0[6] = {
        "fmlslb %z0.s %z0.h %z0.h -> %z0.s",     "fmlslb %z5.s %z6.h %z7.h -> %z5.s",
        "fmlslb %z10.s %z11.h %z12.h -> %z10.s", "fmlslb %z16.s %z17.h %z18.h -> %z16.s",
        "fmlslb %z21.s %z22.h %z23.h -> %z21.s", "fmlslb %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(fmlslb, fmlslb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));
}

TEST_INSTR(fmlslt_sve)
{

    /* Testing FMLSLT  <Zda>.S, <Zn>.H, <Zm>.H */
    const char *const expected_0_0[6] = {
        "fmlslt %z0.s %z0.h %z0.h -> %z0.s",     "fmlslt %z5.s %z6.h %z7.h -> %z5.s",
        "fmlslt %z10.s %z11.h %z12.h -> %z10.s", "fmlslt %z16.s %z17.h %z18.h -> %z16.s",
        "fmlslt %z21.s %z22.h %z23.h -> %z21.s", "fmlslt %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(fmlslt, fmlslt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));
}

TEST_INSTR(histseg_sve)
{

    /* Testing HISTSEG <Zd>.B, <Zn>.B, <Zm>.B */
    const char *const expected_0_0[6] = {
        "histseg %z0.b %z0.b -> %z0.b",    "histseg %z6.b %z7.b -> %z5.b",
        "histseg %z11.b %z12.b -> %z10.b", "histseg %z17.b %z18.b -> %z16.b",
        "histseg %z22.b %z23.b -> %z21.b", "histseg %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(histseg, histseg_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));
}

TEST_INSTR(nbsl_sve)
{

    /* Testing NBSL    <Zdn>.D, <Zdn>.D, <Zm>.D, <Zk>.D */
    const char *const expected_0_0[6] = {
        "nbsl   %z0.d %z0.d %z0.d -> %z0.d",     "nbsl   %z5.d %z6.d %z7.d -> %z5.d",
        "nbsl   %z10.d %z11.d %z12.d -> %z10.d", "nbsl   %z16.d %z17.d %z18.d -> %z16.d",
        "nbsl   %z21.d %z22.d %z23.d -> %z21.d", "nbsl   %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(nbsl, nbsl_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(pmul_sve)
{

    /* Testing PMUL    <Zd>.B, <Zn>.B, <Zm>.B */
    const char *const expected_0_0[6] = {
        "pmul   %z0.b %z0.b -> %z0.b",    "pmul   %z6.b %z7.b -> %z5.b",
        "pmul   %z11.b %z12.b -> %z10.b", "pmul   %z17.b %z18.b -> %z16.b",
        "pmul   %z22.b %z23.b -> %z21.b", "pmul   %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(pmul, pmul_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));
}


TEST_INSTR(rax1_sve)
{

    /* Testing RAX1    <Zd>.D, <Zn>.D, <Zm>.D */
    const char *const expected_0_0[6] = {
        "rax1   %z0.d %z0.d -> %z0.d",    "rax1   %z6.d %z7.d -> %z5.d",
        "rax1   %z11.d %z12.d -> %z10.d", "rax1   %z17.d %z18.d -> %z16.d",
        "rax1   %z22.d %z23.d -> %z21.d", "rax1   %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(rax1, rax1_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sm4e_sve)
{

    /* Testing SM4E    <Zdn>.S, <Zdn>.S, <Zm>.S */
    const char *const expected_0_0[6] = {
        "sm4e   %z0.s %z0.s -> %z0.s",    "sm4e   %z5.s %z6.s -> %z5.s",
        "sm4e   %z10.s %z11.s -> %z10.s", "sm4e   %z16.s %z17.s -> %z16.s",
        "sm4e   %z21.s %z22.s -> %z21.s", "sm4e   %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sm4e, sm4e_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4));
}

TEST_INSTR(sm4ekey_sve)
{

    /* Testing SM4EKEY <Zd>.S, <Zn>.S, <Zm>.S */
    const char *const expected_0_0[6] = {
        "sm4ekey %z0.s %z0.s -> %z0.s",    "sm4ekey %z6.s %z7.s -> %z5.s",
        "sm4ekey %z11.s %z12.s -> %z10.s", "sm4ekey %z17.s %z18.s -> %z16.s",
        "sm4ekey %z22.s %z23.s -> %z21.s", "sm4ekey %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sm4ekey, sm4ekey_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
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

    RUN_INSTR_TEST(aesd_sve);
    RUN_INSTR_TEST(aese_sve);
    RUN_INSTR_TEST(bcax_sve);
    RUN_INSTR_TEST(bsl1n_sve);
    RUN_INSTR_TEST(bsl2n_sve);
    RUN_INSTR_TEST(bsl_sve);
    RUN_INSTR_TEST(eor3_sve);
    RUN_INSTR_TEST(fmlalb_sve);
    RUN_INSTR_TEST(fmlalt_sve);
    RUN_INSTR_TEST(fmlslb_sve);
    RUN_INSTR_TEST(fmlslt_sve);
    RUN_INSTR_TEST(histseg_sve);
    RUN_INSTR_TEST(nbsl_sve);
    RUN_INSTR_TEST(pmul_sve);
    RUN_INSTR_TEST(rax1_sve);
    RUN_INSTR_TEST(sm4e_sve);
    RUN_INSTR_TEST(sm4ekey_sve);

    print("All SVE2 tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
