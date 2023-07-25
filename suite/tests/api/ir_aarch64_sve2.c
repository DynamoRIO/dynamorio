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

TEST_INSTR(adclb_sve)
{

    /* Testing ADCLB   <Zda>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "adclb  %z0.s %z0.s %z0.s -> %z0.s",     "adclb  %z5.s %z6.s %z7.s -> %z5.s",
        "adclb  %z10.s %z11.s %z12.s -> %z10.s", "adclb  %z16.s %z17.s %z18.s -> %z16.s",
        "adclb  %z21.s %z22.s %z23.s -> %z21.s", "adclb  %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(adclb, adclb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_1[6] = {
        "adclb  %z0.d %z0.d %z0.d -> %z0.d",     "adclb  %z5.d %z6.d %z7.d -> %z5.d",
        "adclb  %z10.d %z11.d %z12.d -> %z10.d", "adclb  %z16.d %z17.d %z18.d -> %z16.d",
        "adclb  %z21.d %z22.d %z23.d -> %z21.d", "adclb  %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(adclb, adclb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(adclt_sve)
{

    /* Testing ADCLT   <Zda>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "adclt  %z0.s %z0.s %z0.s -> %z0.s",     "adclt  %z5.s %z6.s %z7.s -> %z5.s",
        "adclt  %z10.s %z11.s %z12.s -> %z10.s", "adclt  %z16.s %z17.s %z18.s -> %z16.s",
        "adclt  %z21.s %z22.s %z23.s -> %z21.s", "adclt  %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(adclt, adclt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_1[6] = {
        "adclt  %z0.d %z0.d %z0.d -> %z0.d",     "adclt  %z5.d %z6.d %z7.d -> %z5.d",
        "adclt  %z10.d %z11.d %z12.d -> %z10.d", "adclt  %z16.d %z17.d %z18.d -> %z16.d",
        "adclt  %z21.d %z22.d %z23.d -> %z21.d", "adclt  %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(adclt, adclt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(bdep_sve)
{

    /* Testing BDEP    <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "bdep   %z0.b %z0.b -> %z0.b",    "bdep   %z6.b %z7.b -> %z5.b",
        "bdep   %z11.b %z12.b -> %z10.b", "bdep   %z17.b %z18.b -> %z16.b",
        "bdep   %z22.b %z23.b -> %z21.b", "bdep   %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(bdep, bdep_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "bdep   %z0.h %z0.h -> %z0.h",    "bdep   %z6.h %z7.h -> %z5.h",
        "bdep   %z11.h %z12.h -> %z10.h", "bdep   %z17.h %z18.h -> %z16.h",
        "bdep   %z22.h %z23.h -> %z21.h", "bdep   %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(bdep, bdep_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "bdep   %z0.s %z0.s -> %z0.s",    "bdep   %z6.s %z7.s -> %z5.s",
        "bdep   %z11.s %z12.s -> %z10.s", "bdep   %z17.s %z18.s -> %z16.s",
        "bdep   %z22.s %z23.s -> %z21.s", "bdep   %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(bdep, bdep_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "bdep   %z0.d %z0.d -> %z0.d",    "bdep   %z6.d %z7.d -> %z5.d",
        "bdep   %z11.d %z12.d -> %z10.d", "bdep   %z17.d %z18.d -> %z16.d",
        "bdep   %z22.d %z23.d -> %z21.d", "bdep   %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(bdep, bdep_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(bext_sve)
{

    /* Testing BEXT    <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "bext   %z0.b %z0.b -> %z0.b",    "bext   %z6.b %z7.b -> %z5.b",
        "bext   %z11.b %z12.b -> %z10.b", "bext   %z17.b %z18.b -> %z16.b",
        "bext   %z22.b %z23.b -> %z21.b", "bext   %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(bext, bext_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "bext   %z0.h %z0.h -> %z0.h",    "bext   %z6.h %z7.h -> %z5.h",
        "bext   %z11.h %z12.h -> %z10.h", "bext   %z17.h %z18.h -> %z16.h",
        "bext   %z22.h %z23.h -> %z21.h", "bext   %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(bext, bext_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "bext   %z0.s %z0.s -> %z0.s",    "bext   %z6.s %z7.s -> %z5.s",
        "bext   %z11.s %z12.s -> %z10.s", "bext   %z17.s %z18.s -> %z16.s",
        "bext   %z22.s %z23.s -> %z21.s", "bext   %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(bext, bext_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "bext   %z0.d %z0.d -> %z0.d",    "bext   %z6.d %z7.d -> %z5.d",
        "bext   %z11.d %z12.d -> %z10.d", "bext   %z17.d %z18.d -> %z16.d",
        "bext   %z22.d %z23.d -> %z21.d", "bext   %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(bext, bext_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(bgrp_sve)
{

    /* Testing BGRP    <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "bgrp   %z0.b %z0.b -> %z0.b",    "bgrp   %z6.b %z7.b -> %z5.b",
        "bgrp   %z11.b %z12.b -> %z10.b", "bgrp   %z17.b %z18.b -> %z16.b",
        "bgrp   %z22.b %z23.b -> %z21.b", "bgrp   %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(bgrp, bgrp_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "bgrp   %z0.h %z0.h -> %z0.h",    "bgrp   %z6.h %z7.h -> %z5.h",
        "bgrp   %z11.h %z12.h -> %z10.h", "bgrp   %z17.h %z18.h -> %z16.h",
        "bgrp   %z22.h %z23.h -> %z21.h", "bgrp   %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(bgrp, bgrp_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "bgrp   %z0.s %z0.s -> %z0.s",    "bgrp   %z6.s %z7.s -> %z5.s",
        "bgrp   %z11.s %z12.s -> %z10.s", "bgrp   %z17.s %z18.s -> %z16.s",
        "bgrp   %z22.s %z23.s -> %z21.s", "bgrp   %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(bgrp, bgrp_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "bgrp   %z0.d %z0.d -> %z0.d",    "bgrp   %z6.d %z7.d -> %z5.d",
        "bgrp   %z11.d %z12.d -> %z10.d", "bgrp   %z17.d %z18.d -> %z16.d",
        "bgrp   %z22.d %z23.d -> %z21.d", "bgrp   %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(bgrp, bgrp_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(eorbt_sve)
{

    /* Testing EORBT   <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "eorbt  %z0.b %z0.b %z0.b -> %z0.b",     "eorbt  %z5.b %z6.b %z7.b -> %z5.b",
        "eorbt  %z10.b %z11.b %z12.b -> %z10.b", "eorbt  %z16.b %z17.b %z18.b -> %z16.b",
        "eorbt  %z21.b %z22.b %z23.b -> %z21.b", "eorbt  %z31.b %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(eorbt, eorbt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "eorbt  %z0.h %z0.h %z0.h -> %z0.h",     "eorbt  %z5.h %z6.h %z7.h -> %z5.h",
        "eorbt  %z10.h %z11.h %z12.h -> %z10.h", "eorbt  %z16.h %z17.h %z18.h -> %z16.h",
        "eorbt  %z21.h %z22.h %z23.h -> %z21.h", "eorbt  %z31.h %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(eorbt, eorbt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "eorbt  %z0.s %z0.s %z0.s -> %z0.s",     "eorbt  %z5.s %z6.s %z7.s -> %z5.s",
        "eorbt  %z10.s %z11.s %z12.s -> %z10.s", "eorbt  %z16.s %z17.s %z18.s -> %z16.s",
        "eorbt  %z21.s %z22.s %z23.s -> %z21.s", "eorbt  %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(eorbt, eorbt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "eorbt  %z0.d %z0.d %z0.d -> %z0.d",     "eorbt  %z5.d %z6.d %z7.d -> %z5.d",
        "eorbt  %z10.d %z11.d %z12.d -> %z10.d", "eorbt  %z16.d %z17.d %z18.d -> %z16.d",
        "eorbt  %z21.d %z22.d %z23.d -> %z21.d", "eorbt  %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(eorbt, eorbt_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(eortb_sve)
{

    /* Testing EORTB   <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "eortb  %z0.b %z0.b %z0.b -> %z0.b",     "eortb  %z5.b %z6.b %z7.b -> %z5.b",
        "eortb  %z10.b %z11.b %z12.b -> %z10.b", "eortb  %z16.b %z17.b %z18.b -> %z16.b",
        "eortb  %z21.b %z22.b %z23.b -> %z21.b", "eortb  %z31.b %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(eortb, eortb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "eortb  %z0.h %z0.h %z0.h -> %z0.h",     "eortb  %z5.h %z6.h %z7.h -> %z5.h",
        "eortb  %z10.h %z11.h %z12.h -> %z10.h", "eortb  %z16.h %z17.h %z18.h -> %z16.h",
        "eortb  %z21.h %z22.h %z23.h -> %z21.h", "eortb  %z31.h %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(eortb, eortb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "eortb  %z0.s %z0.s %z0.s -> %z0.s",     "eortb  %z5.s %z6.s %z7.s -> %z5.s",
        "eortb  %z10.s %z11.s %z12.s -> %z10.s", "eortb  %z16.s %z17.s %z18.s -> %z16.s",
        "eortb  %z21.s %z22.s %z23.s -> %z21.s", "eortb  %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(eortb, eortb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "eortb  %z0.d %z0.d %z0.d -> %z0.d",     "eortb  %z5.d %z6.d %z7.d -> %z5.d",
        "eortb  %z10.d %z11.d %z12.d -> %z10.d", "eortb  %z16.d %z17.d %z18.d -> %z16.d",
        "eortb  %z21.d %z22.d %z23.d -> %z21.d", "eortb  %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(eortb, eortb_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(saba_sve)
{

    /* Testing SABA    <Zda>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "saba   %z0.b %z0.b %z0.b -> %z0.b",     "saba   %z5.b %z6.b %z7.b -> %z5.b",
        "saba   %z10.b %z11.b %z12.b -> %z10.b", "saba   %z16.b %z17.b %z18.b -> %z16.b",
        "saba   %z21.b %z22.b %z23.b -> %z21.b", "saba   %z31.b %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(saba, saba_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "saba   %z0.h %z0.h %z0.h -> %z0.h",     "saba   %z5.h %z6.h %z7.h -> %z5.h",
        "saba   %z10.h %z11.h %z12.h -> %z10.h", "saba   %z16.h %z17.h %z18.h -> %z16.h",
        "saba   %z21.h %z22.h %z23.h -> %z21.h", "saba   %z31.h %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(saba, saba_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "saba   %z0.s %z0.s %z0.s -> %z0.s",     "saba   %z5.s %z6.s %z7.s -> %z5.s",
        "saba   %z10.s %z11.s %z12.s -> %z10.s", "saba   %z16.s %z17.s %z18.s -> %z16.s",
        "saba   %z21.s %z22.s %z23.s -> %z21.s", "saba   %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(saba, saba_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "saba   %z0.d %z0.d %z0.d -> %z0.d",     "saba   %z5.d %z6.d %z7.d -> %z5.d",
        "saba   %z10.d %z11.d %z12.d -> %z10.d", "saba   %z16.d %z17.d %z18.d -> %z16.d",
        "saba   %z21.d %z22.d %z23.d -> %z21.d", "saba   %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(saba, saba_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sbclb_sve)
{

    /* Testing SBCLB   <Zda>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "sbclb  %z0.s %z0.s %z0.s -> %z0.s",     "sbclb  %z5.s %z6.s %z7.s -> %z5.s",
        "sbclb  %z10.s %z11.s %z12.s -> %z10.s", "sbclb  %z16.s %z17.s %z18.s -> %z16.s",
        "sbclb  %z21.s %z22.s %z23.s -> %z21.s", "sbclb  %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sbclb, sbclb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_1[6] = {
        "sbclb  %z0.d %z0.d %z0.d -> %z0.d",     "sbclb  %z5.d %z6.d %z7.d -> %z5.d",
        "sbclb  %z10.d %z11.d %z12.d -> %z10.d", "sbclb  %z16.d %z17.d %z18.d -> %z16.d",
        "sbclb  %z21.d %z22.d %z23.d -> %z21.d", "sbclb  %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sbclb, sbclb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sbclt_sve)
{

    /* Testing SBCLT   <Zda>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "sbclt  %z0.s %z0.s %z0.s -> %z0.s",     "sbclt  %z5.s %z6.s %z7.s -> %z5.s",
        "sbclt  %z10.s %z11.s %z12.s -> %z10.s", "sbclt  %z16.s %z17.s %z18.s -> %z16.s",
        "sbclt  %z21.s %z22.s %z23.s -> %z21.s", "sbclt  %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sbclt, sbclt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_1[6] = {
        "sbclt  %z0.d %z0.d %z0.d -> %z0.d",     "sbclt  %z5.d %z6.d %z7.d -> %z5.d",
        "sbclt  %z10.d %z11.d %z12.d -> %z10.d", "sbclt  %z16.d %z17.d %z18.d -> %z16.d",
        "sbclt  %z21.d %z22.d %z23.d -> %z21.d", "sbclt  %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sbclt, sbclt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sqdmulh_sve)
{

    /* Testing SQDMULH <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "sqdmulh %z0.b %z0.b -> %z0.b",    "sqdmulh %z6.b %z7.b -> %z5.b",
        "sqdmulh %z11.b %z12.b -> %z10.b", "sqdmulh %z17.b %z18.b -> %z16.b",
        "sqdmulh %z22.b %z23.b -> %z21.b", "sqdmulh %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(sqdmulh, sqdmulh_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqdmulh %z0.h %z0.h -> %z0.h",    "sqdmulh %z6.h %z7.h -> %z5.h",
        "sqdmulh %z11.h %z12.h -> %z10.h", "sqdmulh %z17.h %z18.h -> %z16.h",
        "sqdmulh %z22.h %z23.h -> %z21.h", "sqdmulh %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(sqdmulh, sqdmulh_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqdmulh %z0.s %z0.s -> %z0.s",    "sqdmulh %z6.s %z7.s -> %z5.s",
        "sqdmulh %z11.s %z12.s -> %z10.s", "sqdmulh %z17.s %z18.s -> %z16.s",
        "sqdmulh %z22.s %z23.s -> %z21.s", "sqdmulh %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sqdmulh, sqdmulh_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "sqdmulh %z0.d %z0.d -> %z0.d",    "sqdmulh %z6.d %z7.d -> %z5.d",
        "sqdmulh %z11.d %z12.d -> %z10.d", "sqdmulh %z17.d %z18.d -> %z16.d",
        "sqdmulh %z22.d %z23.d -> %z21.d", "sqdmulh %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sqdmulh, sqdmulh_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sqrdmlah_sve)
{

    /* Testing SQRDMLAH <Zda>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "sqrdmlah %z0.b %z0.b %z0.b -> %z0.b",
        "sqrdmlah %z5.b %z6.b %z7.b -> %z5.b",
        "sqrdmlah %z10.b %z11.b %z12.b -> %z10.b",
        "sqrdmlah %z16.b %z17.b %z18.b -> %z16.b",
        "sqrdmlah %z21.b %z22.b %z23.b -> %z21.b",
        "sqrdmlah %z31.b %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(sqrdmlah, sqrdmlah_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqrdmlah %z0.h %z0.h %z0.h -> %z0.h",
        "sqrdmlah %z5.h %z6.h %z7.h -> %z5.h",
        "sqrdmlah %z10.h %z11.h %z12.h -> %z10.h",
        "sqrdmlah %z16.h %z17.h %z18.h -> %z16.h",
        "sqrdmlah %z21.h %z22.h %z23.h -> %z21.h",
        "sqrdmlah %z31.h %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(sqrdmlah, sqrdmlah_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqrdmlah %z0.s %z0.s %z0.s -> %z0.s",
        "sqrdmlah %z5.s %z6.s %z7.s -> %z5.s",
        "sqrdmlah %z10.s %z11.s %z12.s -> %z10.s",
        "sqrdmlah %z16.s %z17.s %z18.s -> %z16.s",
        "sqrdmlah %z21.s %z22.s %z23.s -> %z21.s",
        "sqrdmlah %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sqrdmlah, sqrdmlah_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "sqrdmlah %z0.d %z0.d %z0.d -> %z0.d",
        "sqrdmlah %z5.d %z6.d %z7.d -> %z5.d",
        "sqrdmlah %z10.d %z11.d %z12.d -> %z10.d",
        "sqrdmlah %z16.d %z17.d %z18.d -> %z16.d",
        "sqrdmlah %z21.d %z22.d %z23.d -> %z21.d",
        "sqrdmlah %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sqrdmlah, sqrdmlah_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sqrdmlsh_sve)
{

    /* Testing SQRDMLSH <Zda>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "sqrdmlsh %z0.b %z0.b %z0.b -> %z0.b",
        "sqrdmlsh %z5.b %z6.b %z7.b -> %z5.b",
        "sqrdmlsh %z10.b %z11.b %z12.b -> %z10.b",
        "sqrdmlsh %z16.b %z17.b %z18.b -> %z16.b",
        "sqrdmlsh %z21.b %z22.b %z23.b -> %z21.b",
        "sqrdmlsh %z31.b %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(sqrdmlsh, sqrdmlsh_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqrdmlsh %z0.h %z0.h %z0.h -> %z0.h",
        "sqrdmlsh %z5.h %z6.h %z7.h -> %z5.h",
        "sqrdmlsh %z10.h %z11.h %z12.h -> %z10.h",
        "sqrdmlsh %z16.h %z17.h %z18.h -> %z16.h",
        "sqrdmlsh %z21.h %z22.h %z23.h -> %z21.h",
        "sqrdmlsh %z31.h %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(sqrdmlsh, sqrdmlsh_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqrdmlsh %z0.s %z0.s %z0.s -> %z0.s",
        "sqrdmlsh %z5.s %z6.s %z7.s -> %z5.s",
        "sqrdmlsh %z10.s %z11.s %z12.s -> %z10.s",
        "sqrdmlsh %z16.s %z17.s %z18.s -> %z16.s",
        "sqrdmlsh %z21.s %z22.s %z23.s -> %z21.s",
        "sqrdmlsh %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sqrdmlsh, sqrdmlsh_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "sqrdmlsh %z0.d %z0.d %z0.d -> %z0.d",
        "sqrdmlsh %z5.d %z6.d %z7.d -> %z5.d",
        "sqrdmlsh %z10.d %z11.d %z12.d -> %z10.d",
        "sqrdmlsh %z16.d %z17.d %z18.d -> %z16.d",
        "sqrdmlsh %z21.d %z22.d %z23.d -> %z21.d",
        "sqrdmlsh %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sqrdmlsh, sqrdmlsh_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sqrdmulh_sve)
{

    /* Testing SQRDMULH <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "sqrdmulh %z0.b %z0.b -> %z0.b",    "sqrdmulh %z6.b %z7.b -> %z5.b",
        "sqrdmulh %z11.b %z12.b -> %z10.b", "sqrdmulh %z17.b %z18.b -> %z16.b",
        "sqrdmulh %z22.b %z23.b -> %z21.b", "sqrdmulh %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(sqrdmulh, sqrdmulh_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqrdmulh %z0.h %z0.h -> %z0.h",    "sqrdmulh %z6.h %z7.h -> %z5.h",
        "sqrdmulh %z11.h %z12.h -> %z10.h", "sqrdmulh %z17.h %z18.h -> %z16.h",
        "sqrdmulh %z22.h %z23.h -> %z21.h", "sqrdmulh %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(sqrdmulh, sqrdmulh_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqrdmulh %z0.s %z0.s -> %z0.s",    "sqrdmulh %z6.s %z7.s -> %z5.s",
        "sqrdmulh %z11.s %z12.s -> %z10.s", "sqrdmulh %z17.s %z18.s -> %z16.s",
        "sqrdmulh %z22.s %z23.s -> %z21.s", "sqrdmulh %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sqrdmulh, sqrdmulh_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "sqrdmulh %z0.d %z0.d -> %z0.d",    "sqrdmulh %z6.d %z7.d -> %z5.d",
        "sqrdmulh %z11.d %z12.d -> %z10.d", "sqrdmulh %z17.d %z18.d -> %z16.d",
        "sqrdmulh %z22.d %z23.d -> %z21.d", "sqrdmulh %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sqrdmulh, sqrdmulh_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(tbx_sve)
{

    /* Testing TBX     <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "tbx    %z0.b %z0.b %z0.b -> %z0.b",     "tbx    %z5.b %z6.b %z7.b -> %z5.b",
        "tbx    %z10.b %z11.b %z12.b -> %z10.b", "tbx    %z16.b %z17.b %z18.b -> %z16.b",
        "tbx    %z21.b %z22.b %z23.b -> %z21.b", "tbx    %z31.b %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(tbx, tbx_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "tbx    %z0.h %z0.h %z0.h -> %z0.h",     "tbx    %z5.h %z6.h %z7.h -> %z5.h",
        "tbx    %z10.h %z11.h %z12.h -> %z10.h", "tbx    %z16.h %z17.h %z18.h -> %z16.h",
        "tbx    %z21.h %z22.h %z23.h -> %z21.h", "tbx    %z31.h %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(tbx, tbx_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "tbx    %z0.s %z0.s %z0.s -> %z0.s",     "tbx    %z5.s %z6.s %z7.s -> %z5.s",
        "tbx    %z10.s %z11.s %z12.s -> %z10.s", "tbx    %z16.s %z17.s %z18.s -> %z16.s",
        "tbx    %z21.s %z22.s %z23.s -> %z21.s", "tbx    %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(tbx, tbx_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "tbx    %z0.d %z0.d %z0.d -> %z0.d",     "tbx    %z5.d %z6.d %z7.d -> %z5.d",
        "tbx    %z10.d %z11.d %z12.d -> %z10.d", "tbx    %z16.d %z17.d %z18.d -> %z16.d",
        "tbx    %z21.d %z22.d %z23.d -> %z21.d", "tbx    %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(tbx, tbx_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(uaba_sve)
{

    /* Testing UABA    <Zda>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "uaba   %z0.b %z0.b %z0.b -> %z0.b",     "uaba   %z5.b %z6.b %z7.b -> %z5.b",
        "uaba   %z10.b %z11.b %z12.b -> %z10.b", "uaba   %z16.b %z17.b %z18.b -> %z16.b",
        "uaba   %z21.b %z22.b %z23.b -> %z21.b", "uaba   %z31.b %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(uaba, uaba_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uaba   %z0.h %z0.h %z0.h -> %z0.h",     "uaba   %z5.h %z6.h %z7.h -> %z5.h",
        "uaba   %z10.h %z11.h %z12.h -> %z10.h", "uaba   %z16.h %z17.h %z18.h -> %z16.h",
        "uaba   %z21.h %z22.h %z23.h -> %z21.h", "uaba   %z31.h %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(uaba, uaba_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uaba   %z0.s %z0.s %z0.s -> %z0.s",     "uaba   %z5.s %z6.s %z7.s -> %z5.s",
        "uaba   %z10.s %z11.s %z12.s -> %z10.s", "uaba   %z16.s %z17.s %z18.s -> %z16.s",
        "uaba   %z21.s %z22.s %z23.s -> %z21.s", "uaba   %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(uaba, uaba_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "uaba   %z0.d %z0.d %z0.d -> %z0.d",     "uaba   %z5.d %z6.d %z7.d -> %z5.d",
        "uaba   %z10.d %z11.d %z12.d -> %z10.d", "uaba   %z16.d %z17.d %z18.d -> %z16.d",
        "uaba   %z21.d %z22.d %z23.d -> %z21.d", "uaba   %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(uaba, uaba_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(addhnb_sve)
{

    /* Testing ADDHNB  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "addhnb %z0.h %z0.h -> %z0.b",    "addhnb %z6.h %z7.h -> %z5.b",
        "addhnb %z11.h %z12.h -> %z10.b", "addhnb %z17.h %z18.h -> %z16.b",
        "addhnb %z22.h %z23.h -> %z21.b", "addhnb %z31.h %z31.h -> %z31.b",
    };
    TEST_LOOP(addhnb, addhnb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "addhnb %z0.s %z0.s -> %z0.h",    "addhnb %z6.s %z7.s -> %z5.h",
        "addhnb %z11.s %z12.s -> %z10.h", "addhnb %z17.s %z18.s -> %z16.h",
        "addhnb %z22.s %z23.s -> %z21.h", "addhnb %z31.s %z31.s -> %z31.h",
    };
    TEST_LOOP(addhnb, addhnb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "addhnb %z0.d %z0.d -> %z0.s",    "addhnb %z6.d %z7.d -> %z5.s",
        "addhnb %z11.d %z12.d -> %z10.s", "addhnb %z17.d %z18.d -> %z16.s",
        "addhnb %z22.d %z23.d -> %z21.s", "addhnb %z31.d %z31.d -> %z31.s",
    };
    TEST_LOOP(addhnb, addhnb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(addhnt_sve)
{

    /* Testing ADDHNT  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "addhnt %z0.b %z0.h %z0.h -> %z0.b",     "addhnt %z5.b %z6.h %z7.h -> %z5.b",
        "addhnt %z10.b %z11.h %z12.h -> %z10.b", "addhnt %z16.b %z17.h %z18.h -> %z16.b",
        "addhnt %z21.b %z22.h %z23.h -> %z21.b", "addhnt %z31.b %z31.h %z31.h -> %z31.b",
    };
    TEST_LOOP(addhnt, addhnt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "addhnt %z0.h %z0.s %z0.s -> %z0.h",     "addhnt %z5.h %z6.s %z7.s -> %z5.h",
        "addhnt %z10.h %z11.s %z12.s -> %z10.h", "addhnt %z16.h %z17.s %z18.s -> %z16.h",
        "addhnt %z21.h %z22.s %z23.s -> %z21.h", "addhnt %z31.h %z31.s %z31.s -> %z31.h",
    };
    TEST_LOOP(addhnt, addhnt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "addhnt %z0.s %z0.d %z0.d -> %z0.s",     "addhnt %z5.s %z6.d %z7.d -> %z5.s",
        "addhnt %z10.s %z11.d %z12.d -> %z10.s", "addhnt %z16.s %z17.d %z18.d -> %z16.s",
        "addhnt %z21.s %z22.d %z23.d -> %z21.s", "addhnt %z31.s %z31.d %z31.d -> %z31.s",
    };
    TEST_LOOP(addhnt, addhnt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(pmullb_sve)
{

    /* Testing PMULLB  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "pmullb %z0.b %z0.b -> %z0.h",    "pmullb %z6.b %z7.b -> %z5.h",
        "pmullb %z11.b %z12.b -> %z10.h", "pmullb %z17.b %z18.b -> %z16.h",
        "pmullb %z22.b %z23.b -> %z21.h", "pmullb %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(pmullb, pmullb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "pmullb %z0.s %z0.s -> %z0.d",    "pmullb %z6.s %z7.s -> %z5.d",
        "pmullb %z11.s %z12.s -> %z10.d", "pmullb %z17.s %z18.s -> %z16.d",
        "pmullb %z22.s %z23.s -> %z21.d", "pmullb %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(pmullb, pmullb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(pmullt_sve)
{

    /* Testing PMULLT  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "pmullt %z0.b %z0.b -> %z0.h",    "pmullt %z6.b %z7.b -> %z5.h",
        "pmullt %z11.b %z12.b -> %z10.h", "pmullt %z17.b %z18.b -> %z16.h",
        "pmullt %z22.b %z23.b -> %z21.h", "pmullt %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(pmullt, pmullt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "pmullt %z0.s %z0.s -> %z0.d",    "pmullt %z6.s %z7.s -> %z5.d",
        "pmullt %z11.s %z12.s -> %z10.d", "pmullt %z17.s %z18.s -> %z16.d",
        "pmullt %z22.s %z23.s -> %z21.d", "pmullt %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(pmullt, pmullt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(raddhnb_sve)
{

    /* Testing RADDHNB <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "raddhnb %z0.h %z0.h -> %z0.b",    "raddhnb %z6.h %z7.h -> %z5.b",
        "raddhnb %z11.h %z12.h -> %z10.b", "raddhnb %z17.h %z18.h -> %z16.b",
        "raddhnb %z22.h %z23.h -> %z21.b", "raddhnb %z31.h %z31.h -> %z31.b",
    };
    TEST_LOOP(raddhnb, raddhnb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "raddhnb %z0.s %z0.s -> %z0.h",    "raddhnb %z6.s %z7.s -> %z5.h",
        "raddhnb %z11.s %z12.s -> %z10.h", "raddhnb %z17.s %z18.s -> %z16.h",
        "raddhnb %z22.s %z23.s -> %z21.h", "raddhnb %z31.s %z31.s -> %z31.h",
    };
    TEST_LOOP(raddhnb, raddhnb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "raddhnb %z0.d %z0.d -> %z0.s",    "raddhnb %z6.d %z7.d -> %z5.s",
        "raddhnb %z11.d %z12.d -> %z10.s", "raddhnb %z17.d %z18.d -> %z16.s",
        "raddhnb %z22.d %z23.d -> %z21.s", "raddhnb %z31.d %z31.d -> %z31.s",
    };
    TEST_LOOP(raddhnb, raddhnb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(raddhnt_sve)
{

    /* Testing RADDHNT <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "raddhnt %z0.b %z0.h %z0.h -> %z0.b",
        "raddhnt %z5.b %z6.h %z7.h -> %z5.b",
        "raddhnt %z10.b %z11.h %z12.h -> %z10.b",
        "raddhnt %z16.b %z17.h %z18.h -> %z16.b",
        "raddhnt %z21.b %z22.h %z23.h -> %z21.b",
        "raddhnt %z31.b %z31.h %z31.h -> %z31.b",
    };
    TEST_LOOP(raddhnt, raddhnt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "raddhnt %z0.h %z0.s %z0.s -> %z0.h",
        "raddhnt %z5.h %z6.s %z7.s -> %z5.h",
        "raddhnt %z10.h %z11.s %z12.s -> %z10.h",
        "raddhnt %z16.h %z17.s %z18.s -> %z16.h",
        "raddhnt %z21.h %z22.s %z23.s -> %z21.h",
        "raddhnt %z31.h %z31.s %z31.s -> %z31.h",
    };
    TEST_LOOP(raddhnt, raddhnt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "raddhnt %z0.s %z0.d %z0.d -> %z0.s",
        "raddhnt %z5.s %z6.d %z7.d -> %z5.s",
        "raddhnt %z10.s %z11.d %z12.d -> %z10.s",
        "raddhnt %z16.s %z17.d %z18.d -> %z16.s",
        "raddhnt %z21.s %z22.d %z23.d -> %z21.s",
        "raddhnt %z31.s %z31.d %z31.d -> %z31.s",
    };
    TEST_LOOP(raddhnt, raddhnt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(rsubhnb_sve)
{

    /* Testing RSUBHNB <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "rsubhnb %z0.h %z0.h -> %z0.b",    "rsubhnb %z6.h %z7.h -> %z5.b",
        "rsubhnb %z11.h %z12.h -> %z10.b", "rsubhnb %z17.h %z18.h -> %z16.b",
        "rsubhnb %z22.h %z23.h -> %z21.b", "rsubhnb %z31.h %z31.h -> %z31.b",
    };
    TEST_LOOP(rsubhnb, rsubhnb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "rsubhnb %z0.s %z0.s -> %z0.h",    "rsubhnb %z6.s %z7.s -> %z5.h",
        "rsubhnb %z11.s %z12.s -> %z10.h", "rsubhnb %z17.s %z18.s -> %z16.h",
        "rsubhnb %z22.s %z23.s -> %z21.h", "rsubhnb %z31.s %z31.s -> %z31.h",
    };
    TEST_LOOP(rsubhnb, rsubhnb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "rsubhnb %z0.d %z0.d -> %z0.s",    "rsubhnb %z6.d %z7.d -> %z5.s",
        "rsubhnb %z11.d %z12.d -> %z10.s", "rsubhnb %z17.d %z18.d -> %z16.s",
        "rsubhnb %z22.d %z23.d -> %z21.s", "rsubhnb %z31.d %z31.d -> %z31.s",
    };
    TEST_LOOP(rsubhnb, rsubhnb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(rsubhnt_sve)
{

    /* Testing RSUBHNT <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "rsubhnt %z0.b %z0.h %z0.h -> %z0.b",
        "rsubhnt %z5.b %z6.h %z7.h -> %z5.b",
        "rsubhnt %z10.b %z11.h %z12.h -> %z10.b",
        "rsubhnt %z16.b %z17.h %z18.h -> %z16.b",
        "rsubhnt %z21.b %z22.h %z23.h -> %z21.b",
        "rsubhnt %z31.b %z31.h %z31.h -> %z31.b",
    };
    TEST_LOOP(rsubhnt, rsubhnt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "rsubhnt %z0.h %z0.s %z0.s -> %z0.h",
        "rsubhnt %z5.h %z6.s %z7.s -> %z5.h",
        "rsubhnt %z10.h %z11.s %z12.s -> %z10.h",
        "rsubhnt %z16.h %z17.s %z18.s -> %z16.h",
        "rsubhnt %z21.h %z22.s %z23.s -> %z21.h",
        "rsubhnt %z31.h %z31.s %z31.s -> %z31.h",
    };
    TEST_LOOP(rsubhnt, rsubhnt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "rsubhnt %z0.s %z0.d %z0.d -> %z0.s",
        "rsubhnt %z5.s %z6.d %z7.d -> %z5.s",
        "rsubhnt %z10.s %z11.d %z12.d -> %z10.s",
        "rsubhnt %z16.s %z17.d %z18.d -> %z16.s",
        "rsubhnt %z21.s %z22.d %z23.d -> %z21.s",
        "rsubhnt %z31.s %z31.d %z31.d -> %z31.s",
    };
    TEST_LOOP(rsubhnt, rsubhnt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sabalb_sve)
{

    /* Testing SABALB  <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "sabalb %z0.h %z0.b %z0.b -> %z0.h",     "sabalb %z5.h %z6.b %z7.b -> %z5.h",
        "sabalb %z10.h %z11.b %z12.b -> %z10.h", "sabalb %z16.h %z17.b %z18.b -> %z16.h",
        "sabalb %z21.h %z22.b %z23.b -> %z21.h", "sabalb %z31.h %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(sabalb, sabalb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sabalb %z0.s %z0.h %z0.h -> %z0.s",     "sabalb %z5.s %z6.h %z7.h -> %z5.s",
        "sabalb %z10.s %z11.h %z12.h -> %z10.s", "sabalb %z16.s %z17.h %z18.h -> %z16.s",
        "sabalb %z21.s %z22.h %z23.h -> %z21.s", "sabalb %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(sabalb, sabalb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sabalb %z0.d %z0.s %z0.s -> %z0.d",     "sabalb %z5.d %z6.s %z7.s -> %z5.d",
        "sabalb %z10.d %z11.s %z12.s -> %z10.d", "sabalb %z16.d %z17.s %z18.s -> %z16.d",
        "sabalb %z21.d %z22.s %z23.s -> %z21.d", "sabalb %z31.d %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(sabalb, sabalb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(sabalt_sve)
{

    /* Testing SABALT  <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "sabalt %z0.h %z0.b %z0.b -> %z0.h",     "sabalt %z5.h %z6.b %z7.b -> %z5.h",
        "sabalt %z10.h %z11.b %z12.b -> %z10.h", "sabalt %z16.h %z17.b %z18.b -> %z16.h",
        "sabalt %z21.h %z22.b %z23.b -> %z21.h", "sabalt %z31.h %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(sabalt, sabalt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sabalt %z0.s %z0.h %z0.h -> %z0.s",     "sabalt %z5.s %z6.h %z7.h -> %z5.s",
        "sabalt %z10.s %z11.h %z12.h -> %z10.s", "sabalt %z16.s %z17.h %z18.h -> %z16.s",
        "sabalt %z21.s %z22.h %z23.h -> %z21.s", "sabalt %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(sabalt, sabalt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sabalt %z0.d %z0.s %z0.s -> %z0.d",     "sabalt %z5.d %z6.s %z7.s -> %z5.d",
        "sabalt %z10.d %z11.s %z12.s -> %z10.d", "sabalt %z16.d %z17.s %z18.s -> %z16.d",
        "sabalt %z21.d %z22.s %z23.s -> %z21.d", "sabalt %z31.d %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(sabalt, sabalt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(sabdlb_sve)
{

    /* Testing SABDLB  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "sabdlb %z0.b %z0.b -> %z0.h",    "sabdlb %z6.b %z7.b -> %z5.h",
        "sabdlb %z11.b %z12.b -> %z10.h", "sabdlb %z17.b %z18.b -> %z16.h",
        "sabdlb %z22.b %z23.b -> %z21.h", "sabdlb %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(sabdlb, sabdlb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sabdlb %z0.h %z0.h -> %z0.s",    "sabdlb %z6.h %z7.h -> %z5.s",
        "sabdlb %z11.h %z12.h -> %z10.s", "sabdlb %z17.h %z18.h -> %z16.s",
        "sabdlb %z22.h %z23.h -> %z21.s", "sabdlb %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(sabdlb, sabdlb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sabdlb %z0.s %z0.s -> %z0.d",    "sabdlb %z6.s %z7.s -> %z5.d",
        "sabdlb %z11.s %z12.s -> %z10.d", "sabdlb %z17.s %z18.s -> %z16.d",
        "sabdlb %z22.s %z23.s -> %z21.d", "sabdlb %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(sabdlb, sabdlb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(sabdlt_sve)
{

    /* Testing SABDLT  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "sabdlt %z0.b %z0.b -> %z0.h",    "sabdlt %z6.b %z7.b -> %z5.h",
        "sabdlt %z11.b %z12.b -> %z10.h", "sabdlt %z17.b %z18.b -> %z16.h",
        "sabdlt %z22.b %z23.b -> %z21.h", "sabdlt %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(sabdlt, sabdlt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sabdlt %z0.h %z0.h -> %z0.s",    "sabdlt %z6.h %z7.h -> %z5.s",
        "sabdlt %z11.h %z12.h -> %z10.s", "sabdlt %z17.h %z18.h -> %z16.s",
        "sabdlt %z22.h %z23.h -> %z21.s", "sabdlt %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(sabdlt, sabdlt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sabdlt %z0.s %z0.s -> %z0.d",    "sabdlt %z6.s %z7.s -> %z5.d",
        "sabdlt %z11.s %z12.s -> %z10.d", "sabdlt %z17.s %z18.s -> %z16.d",
        "sabdlt %z22.s %z23.s -> %z21.d", "sabdlt %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(sabdlt, sabdlt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(saddlb_sve)
{

    /* Testing SADDLB  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "saddlb %z0.b %z0.b -> %z0.h",    "saddlb %z6.b %z7.b -> %z5.h",
        "saddlb %z11.b %z12.b -> %z10.h", "saddlb %z17.b %z18.b -> %z16.h",
        "saddlb %z22.b %z23.b -> %z21.h", "saddlb %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(saddlb, saddlb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "saddlb %z0.h %z0.h -> %z0.s",    "saddlb %z6.h %z7.h -> %z5.s",
        "saddlb %z11.h %z12.h -> %z10.s", "saddlb %z17.h %z18.h -> %z16.s",
        "saddlb %z22.h %z23.h -> %z21.s", "saddlb %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(saddlb, saddlb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "saddlb %z0.s %z0.s -> %z0.d",    "saddlb %z6.s %z7.s -> %z5.d",
        "saddlb %z11.s %z12.s -> %z10.d", "saddlb %z17.s %z18.s -> %z16.d",
        "saddlb %z22.s %z23.s -> %z21.d", "saddlb %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(saddlb, saddlb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(saddlbt_sve)
{

    /* Testing SADDLBT <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "saddlbt %z0.b %z0.b -> %z0.h",    "saddlbt %z6.b %z7.b -> %z5.h",
        "saddlbt %z11.b %z12.b -> %z10.h", "saddlbt %z17.b %z18.b -> %z16.h",
        "saddlbt %z22.b %z23.b -> %z21.h", "saddlbt %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(saddlbt, saddlbt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "saddlbt %z0.h %z0.h -> %z0.s",    "saddlbt %z6.h %z7.h -> %z5.s",
        "saddlbt %z11.h %z12.h -> %z10.s", "saddlbt %z17.h %z18.h -> %z16.s",
        "saddlbt %z22.h %z23.h -> %z21.s", "saddlbt %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(saddlbt, saddlbt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "saddlbt %z0.s %z0.s -> %z0.d",    "saddlbt %z6.s %z7.s -> %z5.d",
        "saddlbt %z11.s %z12.s -> %z10.d", "saddlbt %z17.s %z18.s -> %z16.d",
        "saddlbt %z22.s %z23.s -> %z21.d", "saddlbt %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(saddlbt, saddlbt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(saddlt_sve)
{

    /* Testing SADDLT  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "saddlt %z0.b %z0.b -> %z0.h",    "saddlt %z6.b %z7.b -> %z5.h",
        "saddlt %z11.b %z12.b -> %z10.h", "saddlt %z17.b %z18.b -> %z16.h",
        "saddlt %z22.b %z23.b -> %z21.h", "saddlt %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(saddlt, saddlt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "saddlt %z0.h %z0.h -> %z0.s",    "saddlt %z6.h %z7.h -> %z5.s",
        "saddlt %z11.h %z12.h -> %z10.s", "saddlt %z17.h %z18.h -> %z16.s",
        "saddlt %z22.h %z23.h -> %z21.s", "saddlt %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(saddlt, saddlt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "saddlt %z0.s %z0.s -> %z0.d",    "saddlt %z6.s %z7.s -> %z5.d",
        "saddlt %z11.s %z12.s -> %z10.d", "saddlt %z17.s %z18.s -> %z16.d",
        "saddlt %z22.s %z23.s -> %z21.d", "saddlt %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(saddlt, saddlt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(saddwb_sve)
{

    /* Testing SADDWB  <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "saddwb %z0.h %z0.b -> %z0.h",    "saddwb %z6.h %z7.b -> %z5.h",
        "saddwb %z11.h %z12.b -> %z10.h", "saddwb %z17.h %z18.b -> %z16.h",
        "saddwb %z22.h %z23.b -> %z21.h", "saddwb %z31.h %z31.b -> %z31.h",
    };
    TEST_LOOP(saddwb, saddwb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "saddwb %z0.s %z0.h -> %z0.s",    "saddwb %z6.s %z7.h -> %z5.s",
        "saddwb %z11.s %z12.h -> %z10.s", "saddwb %z17.s %z18.h -> %z16.s",
        "saddwb %z22.s %z23.h -> %z21.s", "saddwb %z31.s %z31.h -> %z31.s",
    };
    TEST_LOOP(saddwb, saddwb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "saddwb %z0.d %z0.s -> %z0.d",    "saddwb %z6.d %z7.s -> %z5.d",
        "saddwb %z11.d %z12.s -> %z10.d", "saddwb %z17.d %z18.s -> %z16.d",
        "saddwb %z22.d %z23.s -> %z21.d", "saddwb %z31.d %z31.s -> %z31.d",
    };
    TEST_LOOP(saddwb, saddwb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(saddwt_sve)
{

    /* Testing SADDWT  <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "saddwt %z0.h %z0.b -> %z0.h",    "saddwt %z6.h %z7.b -> %z5.h",
        "saddwt %z11.h %z12.b -> %z10.h", "saddwt %z17.h %z18.b -> %z16.h",
        "saddwt %z22.h %z23.b -> %z21.h", "saddwt %z31.h %z31.b -> %z31.h",
    };
    TEST_LOOP(saddwt, saddwt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "saddwt %z0.s %z0.h -> %z0.s",    "saddwt %z6.s %z7.h -> %z5.s",
        "saddwt %z11.s %z12.h -> %z10.s", "saddwt %z17.s %z18.h -> %z16.s",
        "saddwt %z22.s %z23.h -> %z21.s", "saddwt %z31.s %z31.h -> %z31.s",
    };
    TEST_LOOP(saddwt, saddwt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "saddwt %z0.d %z0.s -> %z0.d",    "saddwt %z6.d %z7.s -> %z5.d",
        "saddwt %z11.d %z12.s -> %z10.d", "saddwt %z17.d %z18.s -> %z16.d",
        "saddwt %z22.d %z23.s -> %z21.d", "saddwt %z31.d %z31.s -> %z31.d",
    };
    TEST_LOOP(saddwt, saddwt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(smlalb_sve)
{

    /* Testing SMLALB  <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "smlalb %z0.h %z0.b %z0.b -> %z0.h",     "smlalb %z5.h %z6.b %z7.b -> %z5.h",
        "smlalb %z10.h %z11.b %z12.b -> %z10.h", "smlalb %z16.h %z17.b %z18.b -> %z16.h",
        "smlalb %z21.h %z22.b %z23.b -> %z21.h", "smlalb %z31.h %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(smlalb, smlalb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "smlalb %z0.s %z0.h %z0.h -> %z0.s",     "smlalb %z5.s %z6.h %z7.h -> %z5.s",
        "smlalb %z10.s %z11.h %z12.h -> %z10.s", "smlalb %z16.s %z17.h %z18.h -> %z16.s",
        "smlalb %z21.s %z22.h %z23.h -> %z21.s", "smlalb %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(smlalb, smlalb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "smlalb %z0.d %z0.s %z0.s -> %z0.d",     "smlalb %z5.d %z6.s %z7.s -> %z5.d",
        "smlalb %z10.d %z11.s %z12.s -> %z10.d", "smlalb %z16.d %z17.s %z18.s -> %z16.d",
        "smlalb %z21.d %z22.s %z23.s -> %z21.d", "smlalb %z31.d %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(smlalb, smlalb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(smlalt_sve)
{

    /* Testing SMLALT  <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "smlalt %z0.h %z0.b %z0.b -> %z0.h",     "smlalt %z5.h %z6.b %z7.b -> %z5.h",
        "smlalt %z10.h %z11.b %z12.b -> %z10.h", "smlalt %z16.h %z17.b %z18.b -> %z16.h",
        "smlalt %z21.h %z22.b %z23.b -> %z21.h", "smlalt %z31.h %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(smlalt, smlalt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "smlalt %z0.s %z0.h %z0.h -> %z0.s",     "smlalt %z5.s %z6.h %z7.h -> %z5.s",
        "smlalt %z10.s %z11.h %z12.h -> %z10.s", "smlalt %z16.s %z17.h %z18.h -> %z16.s",
        "smlalt %z21.s %z22.h %z23.h -> %z21.s", "smlalt %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(smlalt, smlalt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "smlalt %z0.d %z0.s %z0.s -> %z0.d",     "smlalt %z5.d %z6.s %z7.s -> %z5.d",
        "smlalt %z10.d %z11.s %z12.s -> %z10.d", "smlalt %z16.d %z17.s %z18.s -> %z16.d",
        "smlalt %z21.d %z22.s %z23.s -> %z21.d", "smlalt %z31.d %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(smlalt, smlalt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(smlslb_sve)
{

    /* Testing SMLSLB  <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "smlslb %z0.h %z0.b %z0.b -> %z0.h",     "smlslb %z5.h %z6.b %z7.b -> %z5.h",
        "smlslb %z10.h %z11.b %z12.b -> %z10.h", "smlslb %z16.h %z17.b %z18.b -> %z16.h",
        "smlslb %z21.h %z22.b %z23.b -> %z21.h", "smlslb %z31.h %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(smlslb, smlslb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "smlslb %z0.s %z0.h %z0.h -> %z0.s",     "smlslb %z5.s %z6.h %z7.h -> %z5.s",
        "smlslb %z10.s %z11.h %z12.h -> %z10.s", "smlslb %z16.s %z17.h %z18.h -> %z16.s",
        "smlslb %z21.s %z22.h %z23.h -> %z21.s", "smlslb %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(smlslb, smlslb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "smlslb %z0.d %z0.s %z0.s -> %z0.d",     "smlslb %z5.d %z6.s %z7.s -> %z5.d",
        "smlslb %z10.d %z11.s %z12.s -> %z10.d", "smlslb %z16.d %z17.s %z18.s -> %z16.d",
        "smlslb %z21.d %z22.s %z23.s -> %z21.d", "smlslb %z31.d %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(smlslb, smlslb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(smlslt_sve)
{

    /* Testing SMLSLT  <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "smlslt %z0.h %z0.b %z0.b -> %z0.h",     "smlslt %z5.h %z6.b %z7.b -> %z5.h",
        "smlslt %z10.h %z11.b %z12.b -> %z10.h", "smlslt %z16.h %z17.b %z18.b -> %z16.h",
        "smlslt %z21.h %z22.b %z23.b -> %z21.h", "smlslt %z31.h %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(smlslt, smlslt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "smlslt %z0.s %z0.h %z0.h -> %z0.s",     "smlslt %z5.s %z6.h %z7.h -> %z5.s",
        "smlslt %z10.s %z11.h %z12.h -> %z10.s", "smlslt %z16.s %z17.h %z18.h -> %z16.s",
        "smlslt %z21.s %z22.h %z23.h -> %z21.s", "smlslt %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(smlslt, smlslt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "smlslt %z0.d %z0.s %z0.s -> %z0.d",     "smlslt %z5.d %z6.s %z7.s -> %z5.d",
        "smlslt %z10.d %z11.s %z12.s -> %z10.d", "smlslt %z16.d %z17.s %z18.s -> %z16.d",
        "smlslt %z21.d %z22.s %z23.s -> %z21.d", "smlslt %z31.d %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(smlslt, smlslt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(smullb_sve)
{

    /* Testing SMULLB  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "smullb %z0.b %z0.b -> %z0.h",    "smullb %z6.b %z7.b -> %z5.h",
        "smullb %z11.b %z12.b -> %z10.h", "smullb %z17.b %z18.b -> %z16.h",
        "smullb %z22.b %z23.b -> %z21.h", "smullb %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(smullb, smullb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "smullb %z0.h %z0.h -> %z0.s",    "smullb %z6.h %z7.h -> %z5.s",
        "smullb %z11.h %z12.h -> %z10.s", "smullb %z17.h %z18.h -> %z16.s",
        "smullb %z22.h %z23.h -> %z21.s", "smullb %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(smullb, smullb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "smullb %z0.s %z0.s -> %z0.d",    "smullb %z6.s %z7.s -> %z5.d",
        "smullb %z11.s %z12.s -> %z10.d", "smullb %z17.s %z18.s -> %z16.d",
        "smullb %z22.s %z23.s -> %z21.d", "smullb %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(smullb, smullb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(smullt_sve)
{

    /* Testing SMULLT  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "smullt %z0.b %z0.b -> %z0.h",    "smullt %z6.b %z7.b -> %z5.h",
        "smullt %z11.b %z12.b -> %z10.h", "smullt %z17.b %z18.b -> %z16.h",
        "smullt %z22.b %z23.b -> %z21.h", "smullt %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(smullt, smullt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "smullt %z0.h %z0.h -> %z0.s",    "smullt %z6.h %z7.h -> %z5.s",
        "smullt %z11.h %z12.h -> %z10.s", "smullt %z17.h %z18.h -> %z16.s",
        "smullt %z22.h %z23.h -> %z21.s", "smullt %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(smullt, smullt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "smullt %z0.s %z0.s -> %z0.d",    "smullt %z6.s %z7.s -> %z5.d",
        "smullt %z11.s %z12.s -> %z10.d", "smullt %z17.s %z18.s -> %z16.d",
        "smullt %z22.s %z23.s -> %z21.d", "smullt %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(smullt, smullt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(sqdmlalb_sve)
{

    /* Testing SQDMLALB <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "sqdmlalb %z0.h %z0.b %z0.b -> %z0.h",
        "sqdmlalb %z5.h %z6.b %z7.b -> %z5.h",
        "sqdmlalb %z10.h %z11.b %z12.b -> %z10.h",
        "sqdmlalb %z16.h %z17.b %z18.b -> %z16.h",
        "sqdmlalb %z21.h %z22.b %z23.b -> %z21.h",
        "sqdmlalb %z31.h %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(sqdmlalb, sqdmlalb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqdmlalb %z0.s %z0.h %z0.h -> %z0.s",
        "sqdmlalb %z5.s %z6.h %z7.h -> %z5.s",
        "sqdmlalb %z10.s %z11.h %z12.h -> %z10.s",
        "sqdmlalb %z16.s %z17.h %z18.h -> %z16.s",
        "sqdmlalb %z21.s %z22.h %z23.h -> %z21.s",
        "sqdmlalb %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(sqdmlalb, sqdmlalb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqdmlalb %z0.d %z0.s %z0.s -> %z0.d",
        "sqdmlalb %z5.d %z6.s %z7.s -> %z5.d",
        "sqdmlalb %z10.d %z11.s %z12.s -> %z10.d",
        "sqdmlalb %z16.d %z17.s %z18.s -> %z16.d",
        "sqdmlalb %z21.d %z22.s %z23.s -> %z21.d",
        "sqdmlalb %z31.d %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(sqdmlalb, sqdmlalb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(sqdmlalbt_sve)
{

    /* Testing SQDMLALBT <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "sqdmlalbt %z0.h %z0.b %z0.b -> %z0.h",
        "sqdmlalbt %z5.h %z6.b %z7.b -> %z5.h",
        "sqdmlalbt %z10.h %z11.b %z12.b -> %z10.h",
        "sqdmlalbt %z16.h %z17.b %z18.b -> %z16.h",
        "sqdmlalbt %z21.h %z22.b %z23.b -> %z21.h",
        "sqdmlalbt %z31.h %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(sqdmlalbt, sqdmlalbt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqdmlalbt %z0.s %z0.h %z0.h -> %z0.s",
        "sqdmlalbt %z5.s %z6.h %z7.h -> %z5.s",
        "sqdmlalbt %z10.s %z11.h %z12.h -> %z10.s",
        "sqdmlalbt %z16.s %z17.h %z18.h -> %z16.s",
        "sqdmlalbt %z21.s %z22.h %z23.h -> %z21.s",
        "sqdmlalbt %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(sqdmlalbt, sqdmlalbt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqdmlalbt %z0.d %z0.s %z0.s -> %z0.d",
        "sqdmlalbt %z5.d %z6.s %z7.s -> %z5.d",
        "sqdmlalbt %z10.d %z11.s %z12.s -> %z10.d",
        "sqdmlalbt %z16.d %z17.s %z18.s -> %z16.d",
        "sqdmlalbt %z21.d %z22.s %z23.s -> %z21.d",
        "sqdmlalbt %z31.d %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(sqdmlalbt, sqdmlalbt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(sqdmlalt_sve)
{

    /* Testing SQDMLALT <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "sqdmlalt %z0.h %z0.b %z0.b -> %z0.h",
        "sqdmlalt %z5.h %z6.b %z7.b -> %z5.h",
        "sqdmlalt %z10.h %z11.b %z12.b -> %z10.h",
        "sqdmlalt %z16.h %z17.b %z18.b -> %z16.h",
        "sqdmlalt %z21.h %z22.b %z23.b -> %z21.h",
        "sqdmlalt %z31.h %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(sqdmlalt, sqdmlalt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqdmlalt %z0.s %z0.h %z0.h -> %z0.s",
        "sqdmlalt %z5.s %z6.h %z7.h -> %z5.s",
        "sqdmlalt %z10.s %z11.h %z12.h -> %z10.s",
        "sqdmlalt %z16.s %z17.h %z18.h -> %z16.s",
        "sqdmlalt %z21.s %z22.h %z23.h -> %z21.s",
        "sqdmlalt %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(sqdmlalt, sqdmlalt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqdmlalt %z0.d %z0.s %z0.s -> %z0.d",
        "sqdmlalt %z5.d %z6.s %z7.s -> %z5.d",
        "sqdmlalt %z10.d %z11.s %z12.s -> %z10.d",
        "sqdmlalt %z16.d %z17.s %z18.s -> %z16.d",
        "sqdmlalt %z21.d %z22.s %z23.s -> %z21.d",
        "sqdmlalt %z31.d %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(sqdmlalt, sqdmlalt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(sqdmlslb_sve)
{

    /* Testing SQDMLSLB <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "sqdmlslb %z0.h %z0.b %z0.b -> %z0.h",
        "sqdmlslb %z5.h %z6.b %z7.b -> %z5.h",
        "sqdmlslb %z10.h %z11.b %z12.b -> %z10.h",
        "sqdmlslb %z16.h %z17.b %z18.b -> %z16.h",
        "sqdmlslb %z21.h %z22.b %z23.b -> %z21.h",
        "sqdmlslb %z31.h %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(sqdmlslb, sqdmlslb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqdmlslb %z0.s %z0.h %z0.h -> %z0.s",
        "sqdmlslb %z5.s %z6.h %z7.h -> %z5.s",
        "sqdmlslb %z10.s %z11.h %z12.h -> %z10.s",
        "sqdmlslb %z16.s %z17.h %z18.h -> %z16.s",
        "sqdmlslb %z21.s %z22.h %z23.h -> %z21.s",
        "sqdmlslb %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(sqdmlslb, sqdmlslb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqdmlslb %z0.d %z0.s %z0.s -> %z0.d",
        "sqdmlslb %z5.d %z6.s %z7.s -> %z5.d",
        "sqdmlslb %z10.d %z11.s %z12.s -> %z10.d",
        "sqdmlslb %z16.d %z17.s %z18.s -> %z16.d",
        "sqdmlslb %z21.d %z22.s %z23.s -> %z21.d",
        "sqdmlslb %z31.d %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(sqdmlslb, sqdmlslb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(sqdmlslbt_sve)
{

    /* Testing SQDMLSLBT <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "sqdmlslbt %z0.h %z0.b %z0.b -> %z0.h",
        "sqdmlslbt %z5.h %z6.b %z7.b -> %z5.h",
        "sqdmlslbt %z10.h %z11.b %z12.b -> %z10.h",
        "sqdmlslbt %z16.h %z17.b %z18.b -> %z16.h",
        "sqdmlslbt %z21.h %z22.b %z23.b -> %z21.h",
        "sqdmlslbt %z31.h %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(sqdmlslbt, sqdmlslbt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqdmlslbt %z0.s %z0.h %z0.h -> %z0.s",
        "sqdmlslbt %z5.s %z6.h %z7.h -> %z5.s",
        "sqdmlslbt %z10.s %z11.h %z12.h -> %z10.s",
        "sqdmlslbt %z16.s %z17.h %z18.h -> %z16.s",
        "sqdmlslbt %z21.s %z22.h %z23.h -> %z21.s",
        "sqdmlslbt %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(sqdmlslbt, sqdmlslbt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqdmlslbt %z0.d %z0.s %z0.s -> %z0.d",
        "sqdmlslbt %z5.d %z6.s %z7.s -> %z5.d",
        "sqdmlslbt %z10.d %z11.s %z12.s -> %z10.d",
        "sqdmlslbt %z16.d %z17.s %z18.s -> %z16.d",
        "sqdmlslbt %z21.d %z22.s %z23.s -> %z21.d",
        "sqdmlslbt %z31.d %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(sqdmlslbt, sqdmlslbt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(sqdmlslt_sve)
{

    /* Testing SQDMLSLT <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "sqdmlslt %z0.h %z0.b %z0.b -> %z0.h",
        "sqdmlslt %z5.h %z6.b %z7.b -> %z5.h",
        "sqdmlslt %z10.h %z11.b %z12.b -> %z10.h",
        "sqdmlslt %z16.h %z17.b %z18.b -> %z16.h",
        "sqdmlslt %z21.h %z22.b %z23.b -> %z21.h",
        "sqdmlslt %z31.h %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(sqdmlslt, sqdmlslt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqdmlslt %z0.s %z0.h %z0.h -> %z0.s",
        "sqdmlslt %z5.s %z6.h %z7.h -> %z5.s",
        "sqdmlslt %z10.s %z11.h %z12.h -> %z10.s",
        "sqdmlslt %z16.s %z17.h %z18.h -> %z16.s",
        "sqdmlslt %z21.s %z22.h %z23.h -> %z21.s",
        "sqdmlslt %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(sqdmlslt, sqdmlslt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqdmlslt %z0.d %z0.s %z0.s -> %z0.d",
        "sqdmlslt %z5.d %z6.s %z7.s -> %z5.d",
        "sqdmlslt %z10.d %z11.s %z12.s -> %z10.d",
        "sqdmlslt %z16.d %z17.s %z18.s -> %z16.d",
        "sqdmlslt %z21.d %z22.s %z23.s -> %z21.d",
        "sqdmlslt %z31.d %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(sqdmlslt, sqdmlslt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(sqdmullb_sve)
{

    /* Testing SQDMULLB <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "sqdmullb %z0.b %z0.b -> %z0.h",    "sqdmullb %z6.b %z7.b -> %z5.h",
        "sqdmullb %z11.b %z12.b -> %z10.h", "sqdmullb %z17.b %z18.b -> %z16.h",
        "sqdmullb %z22.b %z23.b -> %z21.h", "sqdmullb %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(sqdmullb, sqdmullb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqdmullb %z0.h %z0.h -> %z0.s",    "sqdmullb %z6.h %z7.h -> %z5.s",
        "sqdmullb %z11.h %z12.h -> %z10.s", "sqdmullb %z17.h %z18.h -> %z16.s",
        "sqdmullb %z22.h %z23.h -> %z21.s", "sqdmullb %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(sqdmullb, sqdmullb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqdmullb %z0.s %z0.s -> %z0.d",    "sqdmullb %z6.s %z7.s -> %z5.d",
        "sqdmullb %z11.s %z12.s -> %z10.d", "sqdmullb %z17.s %z18.s -> %z16.d",
        "sqdmullb %z22.s %z23.s -> %z21.d", "sqdmullb %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(sqdmullb, sqdmullb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(sqdmullt_sve)
{

    /* Testing SQDMULLT <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "sqdmullt %z0.b %z0.b -> %z0.h",    "sqdmullt %z6.b %z7.b -> %z5.h",
        "sqdmullt %z11.b %z12.b -> %z10.h", "sqdmullt %z17.b %z18.b -> %z16.h",
        "sqdmullt %z22.b %z23.b -> %z21.h", "sqdmullt %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(sqdmullt, sqdmullt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqdmullt %z0.h %z0.h -> %z0.s",    "sqdmullt %z6.h %z7.h -> %z5.s",
        "sqdmullt %z11.h %z12.h -> %z10.s", "sqdmullt %z17.h %z18.h -> %z16.s",
        "sqdmullt %z22.h %z23.h -> %z21.s", "sqdmullt %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(sqdmullt, sqdmullt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqdmullt %z0.s %z0.s -> %z0.d",    "sqdmullt %z6.s %z7.s -> %z5.d",
        "sqdmullt %z11.s %z12.s -> %z10.d", "sqdmullt %z17.s %z18.s -> %z16.d",
        "sqdmullt %z22.s %z23.s -> %z21.d", "sqdmullt %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(sqdmullt, sqdmullt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(ssublb_sve)
{

    /* Testing SSUBLB  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "ssublb %z0.b %z0.b -> %z0.h",    "ssublb %z6.b %z7.b -> %z5.h",
        "ssublb %z11.b %z12.b -> %z10.h", "ssublb %z17.b %z18.b -> %z16.h",
        "ssublb %z22.b %z23.b -> %z21.h", "ssublb %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(ssublb, ssublb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "ssublb %z0.h %z0.h -> %z0.s",    "ssublb %z6.h %z7.h -> %z5.s",
        "ssublb %z11.h %z12.h -> %z10.s", "ssublb %z17.h %z18.h -> %z16.s",
        "ssublb %z22.h %z23.h -> %z21.s", "ssublb %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(ssublb, ssublb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "ssublb %z0.s %z0.s -> %z0.d",    "ssublb %z6.s %z7.s -> %z5.d",
        "ssublb %z11.s %z12.s -> %z10.d", "ssublb %z17.s %z18.s -> %z16.d",
        "ssublb %z22.s %z23.s -> %z21.d", "ssublb %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(ssublb, ssublb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(ssublbt_sve)
{

    /* Testing SSUBLBT <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "ssublbt %z0.b %z0.b -> %z0.h",    "ssublbt %z6.b %z7.b -> %z5.h",
        "ssublbt %z11.b %z12.b -> %z10.h", "ssublbt %z17.b %z18.b -> %z16.h",
        "ssublbt %z22.b %z23.b -> %z21.h", "ssublbt %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(ssublbt, ssublbt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "ssublbt %z0.h %z0.h -> %z0.s",    "ssublbt %z6.h %z7.h -> %z5.s",
        "ssublbt %z11.h %z12.h -> %z10.s", "ssublbt %z17.h %z18.h -> %z16.s",
        "ssublbt %z22.h %z23.h -> %z21.s", "ssublbt %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(ssublbt, ssublbt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "ssublbt %z0.s %z0.s -> %z0.d",    "ssublbt %z6.s %z7.s -> %z5.d",
        "ssublbt %z11.s %z12.s -> %z10.d", "ssublbt %z17.s %z18.s -> %z16.d",
        "ssublbt %z22.s %z23.s -> %z21.d", "ssublbt %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(ssublbt, ssublbt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(ssublt_sve)
{

    /* Testing SSUBLT  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "ssublt %z0.b %z0.b -> %z0.h",    "ssublt %z6.b %z7.b -> %z5.h",
        "ssublt %z11.b %z12.b -> %z10.h", "ssublt %z17.b %z18.b -> %z16.h",
        "ssublt %z22.b %z23.b -> %z21.h", "ssublt %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(ssublt, ssublt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "ssublt %z0.h %z0.h -> %z0.s",    "ssublt %z6.h %z7.h -> %z5.s",
        "ssublt %z11.h %z12.h -> %z10.s", "ssublt %z17.h %z18.h -> %z16.s",
        "ssublt %z22.h %z23.h -> %z21.s", "ssublt %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(ssublt, ssublt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "ssublt %z0.s %z0.s -> %z0.d",    "ssublt %z6.s %z7.s -> %z5.d",
        "ssublt %z11.s %z12.s -> %z10.d", "ssublt %z17.s %z18.s -> %z16.d",
        "ssublt %z22.s %z23.s -> %z21.d", "ssublt %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(ssublt, ssublt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(ssubltb_sve)
{

    /* Testing SSUBLTB <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "ssubltb %z0.b %z0.b -> %z0.h",    "ssubltb %z6.b %z7.b -> %z5.h",
        "ssubltb %z11.b %z12.b -> %z10.h", "ssubltb %z17.b %z18.b -> %z16.h",
        "ssubltb %z22.b %z23.b -> %z21.h", "ssubltb %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(ssubltb, ssubltb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "ssubltb %z0.h %z0.h -> %z0.s",    "ssubltb %z6.h %z7.h -> %z5.s",
        "ssubltb %z11.h %z12.h -> %z10.s", "ssubltb %z17.h %z18.h -> %z16.s",
        "ssubltb %z22.h %z23.h -> %z21.s", "ssubltb %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(ssubltb, ssubltb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "ssubltb %z0.s %z0.s -> %z0.d",    "ssubltb %z6.s %z7.s -> %z5.d",
        "ssubltb %z11.s %z12.s -> %z10.d", "ssubltb %z17.s %z18.s -> %z16.d",
        "ssubltb %z22.s %z23.s -> %z21.d", "ssubltb %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(ssubltb, ssubltb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(ssubwb_sve)
{

    /* Testing SSUBWB  <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "ssubwb %z0.h %z0.b -> %z0.h",    "ssubwb %z6.h %z7.b -> %z5.h",
        "ssubwb %z11.h %z12.b -> %z10.h", "ssubwb %z17.h %z18.b -> %z16.h",
        "ssubwb %z22.h %z23.b -> %z21.h", "ssubwb %z31.h %z31.b -> %z31.h",
    };
    TEST_LOOP(ssubwb, ssubwb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "ssubwb %z0.s %z0.h -> %z0.s",    "ssubwb %z6.s %z7.h -> %z5.s",
        "ssubwb %z11.s %z12.h -> %z10.s", "ssubwb %z17.s %z18.h -> %z16.s",
        "ssubwb %z22.s %z23.h -> %z21.s", "ssubwb %z31.s %z31.h -> %z31.s",
    };
    TEST_LOOP(ssubwb, ssubwb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "ssubwb %z0.d %z0.s -> %z0.d",    "ssubwb %z6.d %z7.s -> %z5.d",
        "ssubwb %z11.d %z12.s -> %z10.d", "ssubwb %z17.d %z18.s -> %z16.d",
        "ssubwb %z22.d %z23.s -> %z21.d", "ssubwb %z31.d %z31.s -> %z31.d",
    };
    TEST_LOOP(ssubwb, ssubwb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(ssubwt_sve)
{

    /* Testing SSUBWT  <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "ssubwt %z0.h %z0.b -> %z0.h",    "ssubwt %z6.h %z7.b -> %z5.h",
        "ssubwt %z11.h %z12.b -> %z10.h", "ssubwt %z17.h %z18.b -> %z16.h",
        "ssubwt %z22.h %z23.b -> %z21.h", "ssubwt %z31.h %z31.b -> %z31.h",
    };
    TEST_LOOP(ssubwt, ssubwt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "ssubwt %z0.s %z0.h -> %z0.s",    "ssubwt %z6.s %z7.h -> %z5.s",
        "ssubwt %z11.s %z12.h -> %z10.s", "ssubwt %z17.s %z18.h -> %z16.s",
        "ssubwt %z22.s %z23.h -> %z21.s", "ssubwt %z31.s %z31.h -> %z31.s",
    };
    TEST_LOOP(ssubwt, ssubwt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "ssubwt %z0.d %z0.s -> %z0.d",    "ssubwt %z6.d %z7.s -> %z5.d",
        "ssubwt %z11.d %z12.s -> %z10.d", "ssubwt %z17.d %z18.s -> %z16.d",
        "ssubwt %z22.d %z23.s -> %z21.d", "ssubwt %z31.d %z31.s -> %z31.d",
    };
    TEST_LOOP(ssubwt, ssubwt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(subhnb_sve)
{

    /* Testing SUBHNB  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "subhnb %z0.h %z0.h -> %z0.b",    "subhnb %z6.h %z7.h -> %z5.b",
        "subhnb %z11.h %z12.h -> %z10.b", "subhnb %z17.h %z18.h -> %z16.b",
        "subhnb %z22.h %z23.h -> %z21.b", "subhnb %z31.h %z31.h -> %z31.b",
    };
    TEST_LOOP(subhnb, subhnb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "subhnb %z0.s %z0.s -> %z0.h",    "subhnb %z6.s %z7.s -> %z5.h",
        "subhnb %z11.s %z12.s -> %z10.h", "subhnb %z17.s %z18.s -> %z16.h",
        "subhnb %z22.s %z23.s -> %z21.h", "subhnb %z31.s %z31.s -> %z31.h",
    };
    TEST_LOOP(subhnb, subhnb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "subhnb %z0.d %z0.d -> %z0.s",    "subhnb %z6.d %z7.d -> %z5.s",
        "subhnb %z11.d %z12.d -> %z10.s", "subhnb %z17.d %z18.d -> %z16.s",
        "subhnb %z22.d %z23.d -> %z21.s", "subhnb %z31.d %z31.d -> %z31.s",
    };
    TEST_LOOP(subhnb, subhnb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(subhnt_sve)
{

    /* Testing SUBHNT  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "subhnt %z0.b %z0.h %z0.h -> %z0.b",     "subhnt %z5.b %z6.h %z7.h -> %z5.b",
        "subhnt %z10.b %z11.h %z12.h -> %z10.b", "subhnt %z16.b %z17.h %z18.h -> %z16.b",
        "subhnt %z21.b %z22.h %z23.h -> %z21.b", "subhnt %z31.b %z31.h %z31.h -> %z31.b",
    };
    TEST_LOOP(subhnt, subhnt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "subhnt %z0.h %z0.s %z0.s -> %z0.h",     "subhnt %z5.h %z6.s %z7.s -> %z5.h",
        "subhnt %z10.h %z11.s %z12.s -> %z10.h", "subhnt %z16.h %z17.s %z18.s -> %z16.h",
        "subhnt %z21.h %z22.s %z23.s -> %z21.h", "subhnt %z31.h %z31.s %z31.s -> %z31.h",
    };
    TEST_LOOP(subhnt, subhnt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "subhnt %z0.s %z0.d %z0.d -> %z0.s",     "subhnt %z5.s %z6.d %z7.d -> %z5.s",
        "subhnt %z10.s %z11.d %z12.d -> %z10.s", "subhnt %z16.s %z17.d %z18.d -> %z16.s",
        "subhnt %z21.s %z22.d %z23.d -> %z21.s", "subhnt %z31.s %z31.d %z31.d -> %z31.s",
    };
    TEST_LOOP(subhnt, subhnt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(uabalb_sve)
{

    /* Testing UABALB  <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "uabalb %z0.h %z0.b %z0.b -> %z0.h",     "uabalb %z5.h %z6.b %z7.b -> %z5.h",
        "uabalb %z10.h %z11.b %z12.b -> %z10.h", "uabalb %z16.h %z17.b %z18.b -> %z16.h",
        "uabalb %z21.h %z22.b %z23.b -> %z21.h", "uabalb %z31.h %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(uabalb, uabalb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uabalb %z0.s %z0.h %z0.h -> %z0.s",     "uabalb %z5.s %z6.h %z7.h -> %z5.s",
        "uabalb %z10.s %z11.h %z12.h -> %z10.s", "uabalb %z16.s %z17.h %z18.h -> %z16.s",
        "uabalb %z21.s %z22.h %z23.h -> %z21.s", "uabalb %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(uabalb, uabalb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uabalb %z0.d %z0.s %z0.s -> %z0.d",     "uabalb %z5.d %z6.s %z7.s -> %z5.d",
        "uabalb %z10.d %z11.s %z12.s -> %z10.d", "uabalb %z16.d %z17.s %z18.s -> %z16.d",
        "uabalb %z21.d %z22.s %z23.s -> %z21.d", "uabalb %z31.d %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(uabalb, uabalb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(uabalt_sve)
{

    /* Testing UABALT  <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "uabalt %z0.h %z0.b %z0.b -> %z0.h",     "uabalt %z5.h %z6.b %z7.b -> %z5.h",
        "uabalt %z10.h %z11.b %z12.b -> %z10.h", "uabalt %z16.h %z17.b %z18.b -> %z16.h",
        "uabalt %z21.h %z22.b %z23.b -> %z21.h", "uabalt %z31.h %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(uabalt, uabalt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uabalt %z0.s %z0.h %z0.h -> %z0.s",     "uabalt %z5.s %z6.h %z7.h -> %z5.s",
        "uabalt %z10.s %z11.h %z12.h -> %z10.s", "uabalt %z16.s %z17.h %z18.h -> %z16.s",
        "uabalt %z21.s %z22.h %z23.h -> %z21.s", "uabalt %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(uabalt, uabalt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uabalt %z0.d %z0.s %z0.s -> %z0.d",     "uabalt %z5.d %z6.s %z7.s -> %z5.d",
        "uabalt %z10.d %z11.s %z12.s -> %z10.d", "uabalt %z16.d %z17.s %z18.s -> %z16.d",
        "uabalt %z21.d %z22.s %z23.s -> %z21.d", "uabalt %z31.d %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(uabalt, uabalt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(uabdlb_sve)
{

    /* Testing UABDLB  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "uabdlb %z0.b %z0.b -> %z0.h",    "uabdlb %z6.b %z7.b -> %z5.h",
        "uabdlb %z11.b %z12.b -> %z10.h", "uabdlb %z17.b %z18.b -> %z16.h",
        "uabdlb %z22.b %z23.b -> %z21.h", "uabdlb %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(uabdlb, uabdlb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uabdlb %z0.h %z0.h -> %z0.s",    "uabdlb %z6.h %z7.h -> %z5.s",
        "uabdlb %z11.h %z12.h -> %z10.s", "uabdlb %z17.h %z18.h -> %z16.s",
        "uabdlb %z22.h %z23.h -> %z21.s", "uabdlb %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(uabdlb, uabdlb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uabdlb %z0.s %z0.s -> %z0.d",    "uabdlb %z6.s %z7.s -> %z5.d",
        "uabdlb %z11.s %z12.s -> %z10.d", "uabdlb %z17.s %z18.s -> %z16.d",
        "uabdlb %z22.s %z23.s -> %z21.d", "uabdlb %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(uabdlb, uabdlb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(uabdlt_sve)
{

    /* Testing UABDLT  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "uabdlt %z0.b %z0.b -> %z0.h",    "uabdlt %z6.b %z7.b -> %z5.h",
        "uabdlt %z11.b %z12.b -> %z10.h", "uabdlt %z17.b %z18.b -> %z16.h",
        "uabdlt %z22.b %z23.b -> %z21.h", "uabdlt %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(uabdlt, uabdlt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uabdlt %z0.h %z0.h -> %z0.s",    "uabdlt %z6.h %z7.h -> %z5.s",
        "uabdlt %z11.h %z12.h -> %z10.s", "uabdlt %z17.h %z18.h -> %z16.s",
        "uabdlt %z22.h %z23.h -> %z21.s", "uabdlt %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(uabdlt, uabdlt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uabdlt %z0.s %z0.s -> %z0.d",    "uabdlt %z6.s %z7.s -> %z5.d",
        "uabdlt %z11.s %z12.s -> %z10.d", "uabdlt %z17.s %z18.s -> %z16.d",
        "uabdlt %z22.s %z23.s -> %z21.d", "uabdlt %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(uabdlt, uabdlt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(uaddlb_sve)
{

    /* Testing UADDLB  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "uaddlb %z0.b %z0.b -> %z0.h",    "uaddlb %z6.b %z7.b -> %z5.h",
        "uaddlb %z11.b %z12.b -> %z10.h", "uaddlb %z17.b %z18.b -> %z16.h",
        "uaddlb %z22.b %z23.b -> %z21.h", "uaddlb %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(uaddlb, uaddlb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uaddlb %z0.h %z0.h -> %z0.s",    "uaddlb %z6.h %z7.h -> %z5.s",
        "uaddlb %z11.h %z12.h -> %z10.s", "uaddlb %z17.h %z18.h -> %z16.s",
        "uaddlb %z22.h %z23.h -> %z21.s", "uaddlb %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(uaddlb, uaddlb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uaddlb %z0.s %z0.s -> %z0.d",    "uaddlb %z6.s %z7.s -> %z5.d",
        "uaddlb %z11.s %z12.s -> %z10.d", "uaddlb %z17.s %z18.s -> %z16.d",
        "uaddlb %z22.s %z23.s -> %z21.d", "uaddlb %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(uaddlb, uaddlb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(uaddlt_sve)
{

    /* Testing UADDLT  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "uaddlt %z0.b %z0.b -> %z0.h",    "uaddlt %z6.b %z7.b -> %z5.h",
        "uaddlt %z11.b %z12.b -> %z10.h", "uaddlt %z17.b %z18.b -> %z16.h",
        "uaddlt %z22.b %z23.b -> %z21.h", "uaddlt %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(uaddlt, uaddlt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uaddlt %z0.h %z0.h -> %z0.s",    "uaddlt %z6.h %z7.h -> %z5.s",
        "uaddlt %z11.h %z12.h -> %z10.s", "uaddlt %z17.h %z18.h -> %z16.s",
        "uaddlt %z22.h %z23.h -> %z21.s", "uaddlt %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(uaddlt, uaddlt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uaddlt %z0.s %z0.s -> %z0.d",    "uaddlt %z6.s %z7.s -> %z5.d",
        "uaddlt %z11.s %z12.s -> %z10.d", "uaddlt %z17.s %z18.s -> %z16.d",
        "uaddlt %z22.s %z23.s -> %z21.d", "uaddlt %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(uaddlt, uaddlt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(uaddwb_sve)
{

    /* Testing UADDWB  <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "uaddwb %z0.h %z0.b -> %z0.h",    "uaddwb %z6.h %z7.b -> %z5.h",
        "uaddwb %z11.h %z12.b -> %z10.h", "uaddwb %z17.h %z18.b -> %z16.h",
        "uaddwb %z22.h %z23.b -> %z21.h", "uaddwb %z31.h %z31.b -> %z31.h",
    };
    TEST_LOOP(uaddwb, uaddwb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uaddwb %z0.s %z0.h -> %z0.s",    "uaddwb %z6.s %z7.h -> %z5.s",
        "uaddwb %z11.s %z12.h -> %z10.s", "uaddwb %z17.s %z18.h -> %z16.s",
        "uaddwb %z22.s %z23.h -> %z21.s", "uaddwb %z31.s %z31.h -> %z31.s",
    };
    TEST_LOOP(uaddwb, uaddwb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uaddwb %z0.d %z0.s -> %z0.d",    "uaddwb %z6.d %z7.s -> %z5.d",
        "uaddwb %z11.d %z12.s -> %z10.d", "uaddwb %z17.d %z18.s -> %z16.d",
        "uaddwb %z22.d %z23.s -> %z21.d", "uaddwb %z31.d %z31.s -> %z31.d",
    };
    TEST_LOOP(uaddwb, uaddwb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(uaddwt_sve)
{

    /* Testing UADDWT  <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "uaddwt %z0.h %z0.b -> %z0.h",    "uaddwt %z6.h %z7.b -> %z5.h",
        "uaddwt %z11.h %z12.b -> %z10.h", "uaddwt %z17.h %z18.b -> %z16.h",
        "uaddwt %z22.h %z23.b -> %z21.h", "uaddwt %z31.h %z31.b -> %z31.h",
    };
    TEST_LOOP(uaddwt, uaddwt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uaddwt %z0.s %z0.h -> %z0.s",    "uaddwt %z6.s %z7.h -> %z5.s",
        "uaddwt %z11.s %z12.h -> %z10.s", "uaddwt %z17.s %z18.h -> %z16.s",
        "uaddwt %z22.s %z23.h -> %z21.s", "uaddwt %z31.s %z31.h -> %z31.s",
    };
    TEST_LOOP(uaddwt, uaddwt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uaddwt %z0.d %z0.s -> %z0.d",    "uaddwt %z6.d %z7.s -> %z5.d",
        "uaddwt %z11.d %z12.s -> %z10.d", "uaddwt %z17.d %z18.s -> %z16.d",
        "uaddwt %z22.d %z23.s -> %z21.d", "uaddwt %z31.d %z31.s -> %z31.d",
    };
    TEST_LOOP(uaddwt, uaddwt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(umlalb_sve)
{

    /* Testing UMLALB  <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "umlalb %z0.h %z0.b %z0.b -> %z0.h",     "umlalb %z5.h %z6.b %z7.b -> %z5.h",
        "umlalb %z10.h %z11.b %z12.b -> %z10.h", "umlalb %z16.h %z17.b %z18.b -> %z16.h",
        "umlalb %z21.h %z22.b %z23.b -> %z21.h", "umlalb %z31.h %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(umlalb, umlalb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "umlalb %z0.s %z0.h %z0.h -> %z0.s",     "umlalb %z5.s %z6.h %z7.h -> %z5.s",
        "umlalb %z10.s %z11.h %z12.h -> %z10.s", "umlalb %z16.s %z17.h %z18.h -> %z16.s",
        "umlalb %z21.s %z22.h %z23.h -> %z21.s", "umlalb %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(umlalb, umlalb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "umlalb %z0.d %z0.s %z0.s -> %z0.d",     "umlalb %z5.d %z6.s %z7.s -> %z5.d",
        "umlalb %z10.d %z11.s %z12.s -> %z10.d", "umlalb %z16.d %z17.s %z18.s -> %z16.d",
        "umlalb %z21.d %z22.s %z23.s -> %z21.d", "umlalb %z31.d %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(umlalb, umlalb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(umlalt_sve)
{

    /* Testing UMLALT  <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "umlalt %z0.h %z0.b %z0.b -> %z0.h",     "umlalt %z5.h %z6.b %z7.b -> %z5.h",
        "umlalt %z10.h %z11.b %z12.b -> %z10.h", "umlalt %z16.h %z17.b %z18.b -> %z16.h",
        "umlalt %z21.h %z22.b %z23.b -> %z21.h", "umlalt %z31.h %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(umlalt, umlalt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "umlalt %z0.s %z0.h %z0.h -> %z0.s",     "umlalt %z5.s %z6.h %z7.h -> %z5.s",
        "umlalt %z10.s %z11.h %z12.h -> %z10.s", "umlalt %z16.s %z17.h %z18.h -> %z16.s",
        "umlalt %z21.s %z22.h %z23.h -> %z21.s", "umlalt %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(umlalt, umlalt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "umlalt %z0.d %z0.s %z0.s -> %z0.d",     "umlalt %z5.d %z6.s %z7.s -> %z5.d",
        "umlalt %z10.d %z11.s %z12.s -> %z10.d", "umlalt %z16.d %z17.s %z18.s -> %z16.d",
        "umlalt %z21.d %z22.s %z23.s -> %z21.d", "umlalt %z31.d %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(umlalt, umlalt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(umlslb_sve)
{

    /* Testing UMLSLB  <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "umlslb %z0.h %z0.b %z0.b -> %z0.h",     "umlslb %z5.h %z6.b %z7.b -> %z5.h",
        "umlslb %z10.h %z11.b %z12.b -> %z10.h", "umlslb %z16.h %z17.b %z18.b -> %z16.h",
        "umlslb %z21.h %z22.b %z23.b -> %z21.h", "umlslb %z31.h %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(umlslb, umlslb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "umlslb %z0.s %z0.h %z0.h -> %z0.s",     "umlslb %z5.s %z6.h %z7.h -> %z5.s",
        "umlslb %z10.s %z11.h %z12.h -> %z10.s", "umlslb %z16.s %z17.h %z18.h -> %z16.s",
        "umlslb %z21.s %z22.h %z23.h -> %z21.s", "umlslb %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(umlslb, umlslb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "umlslb %z0.d %z0.s %z0.s -> %z0.d",     "umlslb %z5.d %z6.s %z7.s -> %z5.d",
        "umlslb %z10.d %z11.s %z12.s -> %z10.d", "umlslb %z16.d %z17.s %z18.s -> %z16.d",
        "umlslb %z21.d %z22.s %z23.s -> %z21.d", "umlslb %z31.d %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(umlslb, umlslb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(umlslt_sve)
{

    /* Testing UMLSLT  <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "umlslt %z0.h %z0.b %z0.b -> %z0.h",     "umlslt %z5.h %z6.b %z7.b -> %z5.h",
        "umlslt %z10.h %z11.b %z12.b -> %z10.h", "umlslt %z16.h %z17.b %z18.b -> %z16.h",
        "umlslt %z21.h %z22.b %z23.b -> %z21.h", "umlslt %z31.h %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(umlslt, umlslt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "umlslt %z0.s %z0.h %z0.h -> %z0.s",     "umlslt %z5.s %z6.h %z7.h -> %z5.s",
        "umlslt %z10.s %z11.h %z12.h -> %z10.s", "umlslt %z16.s %z17.h %z18.h -> %z16.s",
        "umlslt %z21.s %z22.h %z23.h -> %z21.s", "umlslt %z31.s %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(umlslt, umlslt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "umlslt %z0.d %z0.s %z0.s -> %z0.d",     "umlslt %z5.d %z6.s %z7.s -> %z5.d",
        "umlslt %z10.d %z11.s %z12.s -> %z10.d", "umlslt %z16.d %z17.s %z18.s -> %z16.d",
        "umlslt %z21.d %z22.s %z23.s -> %z21.d", "umlslt %z31.d %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(umlslt, umlslt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(umullb_sve)
{

    /* Testing UMULLB  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "umullb %z0.b %z0.b -> %z0.h",    "umullb %z6.b %z7.b -> %z5.h",
        "umullb %z11.b %z12.b -> %z10.h", "umullb %z17.b %z18.b -> %z16.h",
        "umullb %z22.b %z23.b -> %z21.h", "umullb %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(umullb, umullb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "umullb %z0.h %z0.h -> %z0.s",    "umullb %z6.h %z7.h -> %z5.s",
        "umullb %z11.h %z12.h -> %z10.s", "umullb %z17.h %z18.h -> %z16.s",
        "umullb %z22.h %z23.h -> %z21.s", "umullb %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(umullb, umullb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "umullb %z0.s %z0.s -> %z0.d",    "umullb %z6.s %z7.s -> %z5.d",
        "umullb %z11.s %z12.s -> %z10.d", "umullb %z17.s %z18.s -> %z16.d",
        "umullb %z22.s %z23.s -> %z21.d", "umullb %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(umullb, umullb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(umullt_sve)
{

    /* Testing UMULLT  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "umullt %z0.b %z0.b -> %z0.h",    "umullt %z6.b %z7.b -> %z5.h",
        "umullt %z11.b %z12.b -> %z10.h", "umullt %z17.b %z18.b -> %z16.h",
        "umullt %z22.b %z23.b -> %z21.h", "umullt %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(umullt, umullt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "umullt %z0.h %z0.h -> %z0.s",    "umullt %z6.h %z7.h -> %z5.s",
        "umullt %z11.h %z12.h -> %z10.s", "umullt %z17.h %z18.h -> %z16.s",
        "umullt %z22.h %z23.h -> %z21.s", "umullt %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(umullt, umullt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "umullt %z0.s %z0.s -> %z0.d",    "umullt %z6.s %z7.s -> %z5.d",
        "umullt %z11.s %z12.s -> %z10.d", "umullt %z17.s %z18.s -> %z16.d",
        "umullt %z22.s %z23.s -> %z21.d", "umullt %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(umullt, umullt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(usublb_sve)
{

    /* Testing USUBLB  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "usublb %z0.b %z0.b -> %z0.h",    "usublb %z6.b %z7.b -> %z5.h",
        "usublb %z11.b %z12.b -> %z10.h", "usublb %z17.b %z18.b -> %z16.h",
        "usublb %z22.b %z23.b -> %z21.h", "usublb %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(usublb, usublb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "usublb %z0.h %z0.h -> %z0.s",    "usublb %z6.h %z7.h -> %z5.s",
        "usublb %z11.h %z12.h -> %z10.s", "usublb %z17.h %z18.h -> %z16.s",
        "usublb %z22.h %z23.h -> %z21.s", "usublb %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(usublb, usublb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "usublb %z0.s %z0.s -> %z0.d",    "usublb %z6.s %z7.s -> %z5.d",
        "usublb %z11.s %z12.s -> %z10.d", "usublb %z17.s %z18.s -> %z16.d",
        "usublb %z22.s %z23.s -> %z21.d", "usublb %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(usublb, usublb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(usublt_sve)
{

    /* Testing USUBLT  <Zd>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "usublt %z0.b %z0.b -> %z0.h",    "usublt %z6.b %z7.b -> %z5.h",
        "usublt %z11.b %z12.b -> %z10.h", "usublt %z17.b %z18.b -> %z16.h",
        "usublt %z22.b %z23.b -> %z21.h", "usublt %z31.b %z31.b -> %z31.h",
    };
    TEST_LOOP(usublt, usublt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "usublt %z0.h %z0.h -> %z0.s",    "usublt %z6.h %z7.h -> %z5.s",
        "usublt %z11.h %z12.h -> %z10.s", "usublt %z17.h %z18.h -> %z16.s",
        "usublt %z22.h %z23.h -> %z21.s", "usublt %z31.h %z31.h -> %z31.s",
    };
    TEST_LOOP(usublt, usublt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "usublt %z0.s %z0.s -> %z0.d",    "usublt %z6.s %z7.s -> %z5.d",
        "usublt %z11.s %z12.s -> %z10.d", "usublt %z17.s %z18.s -> %z16.d",
        "usublt %z22.s %z23.s -> %z21.d", "usublt %z31.s %z31.s -> %z31.d",
    };
    TEST_LOOP(usublt, usublt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(usubwb_sve)
{

    /* Testing USUBWB  <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "usubwb %z0.h %z0.b -> %z0.h",    "usubwb %z6.h %z7.b -> %z5.h",
        "usubwb %z11.h %z12.b -> %z10.h", "usubwb %z17.h %z18.b -> %z16.h",
        "usubwb %z22.h %z23.b -> %z21.h", "usubwb %z31.h %z31.b -> %z31.h",
    };
    TEST_LOOP(usubwb, usubwb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "usubwb %z0.s %z0.h -> %z0.s",    "usubwb %z6.s %z7.h -> %z5.s",
        "usubwb %z11.s %z12.h -> %z10.s", "usubwb %z17.s %z18.h -> %z16.s",
        "usubwb %z22.s %z23.h -> %z21.s", "usubwb %z31.s %z31.h -> %z31.s",
    };
    TEST_LOOP(usubwb, usubwb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "usubwb %z0.d %z0.s -> %z0.d",    "usubwb %z6.d %z7.s -> %z5.d",
        "usubwb %z11.d %z12.s -> %z10.d", "usubwb %z17.d %z18.s -> %z16.d",
        "usubwb %z22.d %z23.s -> %z21.d", "usubwb %z31.d %z31.s -> %z31.d",
    };
    TEST_LOOP(usubwb, usubwb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(usubwt_sve)
{

    /* Testing USUBWT  <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Tb> */
    const char *const expected_0_0[6] = {
        "usubwt %z0.h %z0.b -> %z0.h",    "usubwt %z6.h %z7.b -> %z5.h",
        "usubwt %z11.h %z12.b -> %z10.h", "usubwt %z17.h %z18.b -> %z16.h",
        "usubwt %z22.h %z23.b -> %z21.h", "usubwt %z31.h %z31.b -> %z31.h",
    };
    TEST_LOOP(usubwt, usubwt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "usubwt %z0.s %z0.h -> %z0.s",    "usubwt %z6.s %z7.h -> %z5.s",
        "usubwt %z11.s %z12.h -> %z10.s", "usubwt %z17.s %z18.h -> %z16.s",
        "usubwt %z22.s %z23.h -> %z21.s", "usubwt %z31.s %z31.h -> %z31.s",
    };
    TEST_LOOP(usubwt, usubwt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "usubwt %z0.d %z0.s -> %z0.d",    "usubwt %z6.d %z7.s -> %z5.d",
        "usubwt %z11.d %z12.s -> %z10.d", "usubwt %z17.d %z18.s -> %z16.d",
        "usubwt %z22.d %z23.s -> %z21.d", "usubwt %z31.d %z31.s -> %z31.d",
    };
    TEST_LOOP(usubwt, usubwt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(aesimc_sve)
{

    /* Testing AESIMC  <Zdn>.B, <Zdn>.B */
    const char *const expected_0_0[6] = {
        "aesimc %z0.b -> %z0.b",   "aesimc %z5.b -> %z5.b",   "aesimc %z10.b -> %z10.b",
        "aesimc %z16.b -> %z16.b", "aesimc %z21.b -> %z21.b", "aesimc %z31.b -> %z31.b",
    };
    TEST_LOOP(aesimc, aesimc_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1));
}

TEST_INSTR(aesmc_sve)
{

    /* Testing AESMC   <Zdn>.B, <Zdn>.B */
    const char *const expected_0_0[6] = {
        "aesmc  %z0.b -> %z0.b",   "aesmc  %z5.b -> %z5.b",   "aesmc  %z10.b -> %z10.b",
        "aesmc  %z16.b -> %z16.b", "aesmc  %z21.b -> %z21.b", "aesmc  %z31.b -> %z31.b",
    };
    TEST_LOOP(aesmc, aesmc_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1));
}

TEST_INSTR(sqxtnb_sve)
{

    /* Testing SQXTNB  <Zd>.<Ts>, <Zn>.<Tb> */
    const char *const expected_0_0[6] = {
        "sqxtnb %z0.h -> %z0.b",   "sqxtnb %z6.h -> %z5.b",   "sqxtnb %z11.h -> %z10.b",
        "sqxtnb %z17.h -> %z16.b", "sqxtnb %z22.h -> %z21.b", "sqxtnb %z31.h -> %z31.b",
    };
    TEST_LOOP(sqxtnb, sqxtnb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "sqxtnb %z0.s -> %z0.h",   "sqxtnb %z6.s -> %z5.h",   "sqxtnb %z11.s -> %z10.h",
        "sqxtnb %z17.s -> %z16.h", "sqxtnb %z22.s -> %z21.h", "sqxtnb %z31.s -> %z31.h",
    };
    TEST_LOOP(sqxtnb, sqxtnb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "sqxtnb %z0.d -> %z0.s",   "sqxtnb %z6.d -> %z5.s",   "sqxtnb %z11.d -> %z10.s",
        "sqxtnb %z17.d -> %z16.s", "sqxtnb %z22.d -> %z21.s", "sqxtnb %z31.d -> %z31.s",
    };
    TEST_LOOP(sqxtnb, sqxtnb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(sqxtnt_sve)
{

    /* Testing SQXTNT  <Zd>.<Ts>, <Zn>.<Tb> */
    const char *const expected_0_0[6] = {
        "sqxtnt %z0.b %z0.h -> %z0.b",    "sqxtnt %z5.b %z6.h -> %z5.b",
        "sqxtnt %z10.b %z11.h -> %z10.b", "sqxtnt %z16.b %z17.h -> %z16.b",
        "sqxtnt %z21.b %z22.h -> %z21.b", "sqxtnt %z31.b %z31.h -> %z31.b",
    };
    TEST_LOOP(sqxtnt, sqxtnt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "sqxtnt %z0.h %z0.s -> %z0.h",    "sqxtnt %z5.h %z6.s -> %z5.h",
        "sqxtnt %z10.h %z11.s -> %z10.h", "sqxtnt %z16.h %z17.s -> %z16.h",
        "sqxtnt %z21.h %z22.s -> %z21.h", "sqxtnt %z31.h %z31.s -> %z31.h",
    };
    TEST_LOOP(sqxtnt, sqxtnt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "sqxtnt %z0.s %z0.d -> %z0.s",    "sqxtnt %z5.s %z6.d -> %z5.s",
        "sqxtnt %z10.s %z11.d -> %z10.s", "sqxtnt %z16.s %z17.d -> %z16.s",
        "sqxtnt %z21.s %z22.d -> %z21.s", "sqxtnt %z31.s %z31.d -> %z31.s",
    };
    TEST_LOOP(sqxtnt, sqxtnt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(sqxtunb_sve)
{

    /* Testing SQXTUNB <Zd>.<Ts>, <Zn>.<Tb> */
    const char *const expected_0_0[6] = {
        "sqxtunb %z0.h -> %z0.b",   "sqxtunb %z6.h -> %z5.b",
        "sqxtunb %z11.h -> %z10.b", "sqxtunb %z17.h -> %z16.b",
        "sqxtunb %z22.h -> %z21.b", "sqxtunb %z31.h -> %z31.b",
    };
    TEST_LOOP(sqxtunb, sqxtunb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "sqxtunb %z0.s -> %z0.h",   "sqxtunb %z6.s -> %z5.h",
        "sqxtunb %z11.s -> %z10.h", "sqxtunb %z17.s -> %z16.h",
        "sqxtunb %z22.s -> %z21.h", "sqxtunb %z31.s -> %z31.h",
    };
    TEST_LOOP(sqxtunb, sqxtunb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "sqxtunb %z0.d -> %z0.s",   "sqxtunb %z6.d -> %z5.s",
        "sqxtunb %z11.d -> %z10.s", "sqxtunb %z17.d -> %z16.s",
        "sqxtunb %z22.d -> %z21.s", "sqxtunb %z31.d -> %z31.s",
    };
    TEST_LOOP(sqxtunb, sqxtunb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(sqxtunt_sve)
{

    /* Testing SQXTUNT <Zd>.<Ts>, <Zn>.<Tb> */
    const char *const expected_0_0[6] = {
        "sqxtunt %z0.b %z0.h -> %z0.b",    "sqxtunt %z5.b %z6.h -> %z5.b",
        "sqxtunt %z10.b %z11.h -> %z10.b", "sqxtunt %z16.b %z17.h -> %z16.b",
        "sqxtunt %z21.b %z22.h -> %z21.b", "sqxtunt %z31.b %z31.h -> %z31.b",
    };
    TEST_LOOP(sqxtunt, sqxtunt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "sqxtunt %z0.h %z0.s -> %z0.h",    "sqxtunt %z5.h %z6.s -> %z5.h",
        "sqxtunt %z10.h %z11.s -> %z10.h", "sqxtunt %z16.h %z17.s -> %z16.h",
        "sqxtunt %z21.h %z22.s -> %z21.h", "sqxtunt %z31.h %z31.s -> %z31.h",
    };
    TEST_LOOP(sqxtunt, sqxtunt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "sqxtunt %z0.s %z0.d -> %z0.s",    "sqxtunt %z5.s %z6.d -> %z5.s",
        "sqxtunt %z10.s %z11.d -> %z10.s", "sqxtunt %z16.s %z17.d -> %z16.s",
        "sqxtunt %z21.s %z22.d -> %z21.s", "sqxtunt %z31.s %z31.d -> %z31.s",
    };
    TEST_LOOP(sqxtunt, sqxtunt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(uqxtnb_sve)
{

    /* Testing UQXTNB  <Zd>.<Ts>, <Zn>.<Tb> */
    const char *const expected_0_0[6] = {
        "uqxtnb %z0.h -> %z0.b",   "uqxtnb %z6.h -> %z5.b",   "uqxtnb %z11.h -> %z10.b",
        "uqxtnb %z17.h -> %z16.b", "uqxtnb %z22.h -> %z21.b", "uqxtnb %z31.h -> %z31.b",
    };
    TEST_LOOP(uqxtnb, uqxtnb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "uqxtnb %z0.s -> %z0.h",   "uqxtnb %z6.s -> %z5.h",   "uqxtnb %z11.s -> %z10.h",
        "uqxtnb %z17.s -> %z16.h", "uqxtnb %z22.s -> %z21.h", "uqxtnb %z31.s -> %z31.h",
    };
    TEST_LOOP(uqxtnb, uqxtnb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "uqxtnb %z0.d -> %z0.s",   "uqxtnb %z6.d -> %z5.s",   "uqxtnb %z11.d -> %z10.s",
        "uqxtnb %z17.d -> %z16.s", "uqxtnb %z22.d -> %z21.s", "uqxtnb %z31.d -> %z31.s",
    };
    TEST_LOOP(uqxtnb, uqxtnb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(uqxtnt_sve)
{

    /* Testing UQXTNT  <Zd>.<Ts>, <Zn>.<Tb> */
    const char *const expected_0_0[6] = {
        "uqxtnt %z0.b %z0.h -> %z0.b",    "uqxtnt %z5.b %z6.h -> %z5.b",
        "uqxtnt %z10.b %z11.h -> %z10.b", "uqxtnt %z16.b %z17.h -> %z16.b",
        "uqxtnt %z21.b %z22.h -> %z21.b", "uqxtnt %z31.b %z31.h -> %z31.b",
    };
    TEST_LOOP(uqxtnt, uqxtnt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "uqxtnt %z0.h %z0.s -> %z0.h",    "uqxtnt %z5.h %z6.s -> %z5.h",
        "uqxtnt %z10.h %z11.s -> %z10.h", "uqxtnt %z16.h %z17.s -> %z16.h",
        "uqxtnt %z21.h %z22.s -> %z21.h", "uqxtnt %z31.h %z31.s -> %z31.h",
    };
    TEST_LOOP(uqxtnt, uqxtnt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "uqxtnt %z0.s %z0.d -> %z0.s",    "uqxtnt %z5.s %z6.d -> %z5.s",
        "uqxtnt %z10.s %z11.d -> %z10.s", "uqxtnt %z16.s %z17.d -> %z16.s",
        "uqxtnt %z21.s %z22.d -> %z21.s", "uqxtnt %z31.s %z31.d -> %z31.s",
    };
    TEST_LOOP(uqxtnt, uqxtnt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(fmlalb_sve_idx)
{

    /* Testing FMLALB  <Zda>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_0_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_0_0[6] = {
        "fmlalb %z0.s %z0.h %z0.h $0x00 -> %z0.s",
        "fmlalb %z5.s %z6.h %z3.h $0x04 -> %z5.s",
        "fmlalb %z10.s %z11.h %z4.h $0x05 -> %z10.s",
        "fmlalb %z16.s %z17.h %z6.h $0x07 -> %z16.s",
        "fmlalb %z21.s %z22.h %z7.h $0x00 -> %z21.s",
        "fmlalb %z31.s %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(fmlalb, fmlalb_sve_idx, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_0_0[i], OPSZ_3b));
}

TEST_INSTR(fmlalt_sve_idx)
{

    /* Testing FMLALT  <Zda>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_0_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_0_0[6] = {
        "fmlalt %z0.s %z0.h %z0.h $0x00 -> %z0.s",
        "fmlalt %z5.s %z6.h %z3.h $0x04 -> %z5.s",
        "fmlalt %z10.s %z11.h %z4.h $0x05 -> %z10.s",
        "fmlalt %z16.s %z17.h %z6.h $0x07 -> %z16.s",
        "fmlalt %z21.s %z22.h %z7.h $0x00 -> %z21.s",
        "fmlalt %z31.s %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(fmlalt, fmlalt_sve_idx, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_0_0[i], OPSZ_3b));
}

TEST_INSTR(fmlslb_sve_idx)
{

    /* Testing FMLSLB  <Zda>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_0_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_0_0[6] = {
        "fmlslb %z0.s %z0.h %z0.h $0x00 -> %z0.s",
        "fmlslb %z5.s %z6.h %z3.h $0x04 -> %z5.s",
        "fmlslb %z10.s %z11.h %z4.h $0x05 -> %z10.s",
        "fmlslb %z16.s %z17.h %z6.h $0x07 -> %z16.s",
        "fmlslb %z21.s %z22.h %z7.h $0x00 -> %z21.s",
        "fmlslb %z31.s %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(fmlslb, fmlslb_sve_idx, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_0_0[i], OPSZ_3b));
}

TEST_INSTR(fmlslt_sve_idx)
{

    /* Testing FMLSLT  <Zda>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_0_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_0_0[6] = {
        "fmlslt %z0.s %z0.h %z0.h $0x00 -> %z0.s",
        "fmlslt %z5.s %z6.h %z3.h $0x04 -> %z5.s",
        "fmlslt %z10.s %z11.h %z4.h $0x05 -> %z10.s",
        "fmlslt %z16.s %z17.h %z6.h $0x07 -> %z16.s",
        "fmlslt %z21.s %z22.h %z7.h $0x00 -> %z21.s",
        "fmlslt %z31.s %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(fmlslt, fmlslt_sve_idx, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_0_0[i], OPSZ_3b));
}

TEST_INSTR(smlalb_sve_idx_vector)
{

    /* Testing SMLALB  <Zda>.D, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i2_0_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_0_0[6] = {
        "smlalb %z0.d %z0.s %z0.s $0x00 -> %z0.d",
        "smlalb %z5.d %z6.s %z4.s $0x03 -> %z5.d",
        "smlalb %z10.d %z11.s %z7.s $0x00 -> %z10.d",
        "smlalb %z16.d %z17.s %z10.s $0x01 -> %z16.d",
        "smlalb %z21.d %z22.s %z12.s $0x01 -> %z21.d",
        "smlalb %z31.d %z31.s %z15.s $0x03 -> %z31.d",
    };
    TEST_LOOP(smlalb, smlalb_sve_idx_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_0_0[i], OPSZ_2b));

    /* Testing SMLALB  <Zda>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_1_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_1_0[6] = {
        "smlalb %z0.s %z0.h %z0.h $0x00 -> %z0.s",
        "smlalb %z5.s %z6.h %z3.h $0x04 -> %z5.s",
        "smlalb %z10.s %z11.h %z4.h $0x05 -> %z10.s",
        "smlalb %z16.s %z17.h %z6.h $0x07 -> %z16.s",
        "smlalb %z21.s %z22.h %z7.h $0x00 -> %z21.s",
        "smlalb %z31.s %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(smlalb, smlalb_sve_idx_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_1_0[i], OPSZ_3b));
}

TEST_INSTR(smlalt_sve_idx_vector)
{

    /* Testing SMLALT  <Zda>.D, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i2_0_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_0_0[6] = {
        "smlalt %z0.d %z0.s %z0.s $0x00 -> %z0.d",
        "smlalt %z5.d %z6.s %z4.s $0x03 -> %z5.d",
        "smlalt %z10.d %z11.s %z7.s $0x00 -> %z10.d",
        "smlalt %z16.d %z17.s %z10.s $0x01 -> %z16.d",
        "smlalt %z21.d %z22.s %z12.s $0x01 -> %z21.d",
        "smlalt %z31.d %z31.s %z15.s $0x03 -> %z31.d",
    };
    TEST_LOOP(smlalt, smlalt_sve_idx_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_0_0[i], OPSZ_2b));

    /* Testing SMLALT  <Zda>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_1_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_1_0[6] = {
        "smlalt %z0.s %z0.h %z0.h $0x00 -> %z0.s",
        "smlalt %z5.s %z6.h %z3.h $0x04 -> %z5.s",
        "smlalt %z10.s %z11.h %z4.h $0x05 -> %z10.s",
        "smlalt %z16.s %z17.h %z6.h $0x07 -> %z16.s",
        "smlalt %z21.s %z22.h %z7.h $0x00 -> %z21.s",
        "smlalt %z31.s %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(smlalt, smlalt_sve_idx_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_1_0[i], OPSZ_3b));
}

TEST_INSTR(smlslb_sve_idx_vector)
{

    /* Testing SMLSLB  <Zda>.D, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i2_0_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_0_0[6] = {
        "smlslb %z0.d %z0.s %z0.s $0x00 -> %z0.d",
        "smlslb %z5.d %z6.s %z4.s $0x03 -> %z5.d",
        "smlslb %z10.d %z11.s %z7.s $0x00 -> %z10.d",
        "smlslb %z16.d %z17.s %z10.s $0x01 -> %z16.d",
        "smlslb %z21.d %z22.s %z12.s $0x01 -> %z21.d",
        "smlslb %z31.d %z31.s %z15.s $0x03 -> %z31.d",
    };
    TEST_LOOP(smlslb, smlslb_sve_idx_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_0_0[i], OPSZ_2b));

    /* Testing SMLSLB  <Zda>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_1_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_1_0[6] = {
        "smlslb %z0.s %z0.h %z0.h $0x00 -> %z0.s",
        "smlslb %z5.s %z6.h %z3.h $0x04 -> %z5.s",
        "smlslb %z10.s %z11.h %z4.h $0x05 -> %z10.s",
        "smlslb %z16.s %z17.h %z6.h $0x07 -> %z16.s",
        "smlslb %z21.s %z22.h %z7.h $0x00 -> %z21.s",
        "smlslb %z31.s %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(smlslb, smlslb_sve_idx_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_1_0[i], OPSZ_3b));
}

TEST_INSTR(smlslt_sve_idx_vector)
{

    /* Testing SMLSLT  <Zda>.D, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i2_0_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_0_0[6] = {
        "smlslt %z0.d %z0.s %z0.s $0x00 -> %z0.d",
        "smlslt %z5.d %z6.s %z4.s $0x03 -> %z5.d",
        "smlslt %z10.d %z11.s %z7.s $0x00 -> %z10.d",
        "smlslt %z16.d %z17.s %z10.s $0x01 -> %z16.d",
        "smlslt %z21.d %z22.s %z12.s $0x01 -> %z21.d",
        "smlslt %z31.d %z31.s %z15.s $0x03 -> %z31.d",
    };
    TEST_LOOP(smlslt, smlslt_sve_idx_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_0_0[i], OPSZ_2b));

    /* Testing SMLSLT  <Zda>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_1_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_1_0[6] = {
        "smlslt %z0.s %z0.h %z0.h $0x00 -> %z0.s",
        "smlslt %z5.s %z6.h %z3.h $0x04 -> %z5.s",
        "smlslt %z10.s %z11.h %z4.h $0x05 -> %z10.s",
        "smlslt %z16.s %z17.h %z6.h $0x07 -> %z16.s",
        "smlslt %z21.s %z22.h %z7.h $0x00 -> %z21.s",
        "smlslt %z31.s %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(smlslt, smlslt_sve_idx_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_1_0[i], OPSZ_3b));
}

TEST_INSTR(smullb_sve_idx_vector)
{

    /* Testing SMULLB  <Zd>.D, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i2_0_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_0_0[6] = {
        "smullb %z0.s %z0.s $0x00 -> %z0.d",    "smullb %z6.s %z4.s $0x03 -> %z5.d",
        "smullb %z11.s %z7.s $0x00 -> %z10.d",  "smullb %z17.s %z10.s $0x01 -> %z16.d",
        "smullb %z22.s %z12.s $0x01 -> %z21.d", "smullb %z31.s %z15.s $0x03 -> %z31.d",
    };
    TEST_LOOP(smullb, smullb_sve_idx_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_0_0[i], OPSZ_2b));

    /* Testing SMULLB  <Zd>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_1_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_1_0[6] = {
        "smullb %z0.h %z0.h $0x00 -> %z0.s",   "smullb %z6.h %z3.h $0x04 -> %z5.s",
        "smullb %z11.h %z4.h $0x05 -> %z10.s", "smullb %z17.h %z6.h $0x07 -> %z16.s",
        "smullb %z22.h %z7.h $0x00 -> %z21.s", "smullb %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(smullb, smullb_sve_idx_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_1_0[i], OPSZ_3b));
}

TEST_INSTR(smullt_sve_idx_vector)
{

    /* Testing SMULLT  <Zd>.D, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i2_0_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_0_0[6] = {
        "smullt %z0.s %z0.s $0x00 -> %z0.d",    "smullt %z6.s %z4.s $0x03 -> %z5.d",
        "smullt %z11.s %z7.s $0x00 -> %z10.d",  "smullt %z17.s %z10.s $0x01 -> %z16.d",
        "smullt %z22.s %z12.s $0x01 -> %z21.d", "smullt %z31.s %z15.s $0x03 -> %z31.d",
    };
    TEST_LOOP(smullt, smullt_sve_idx_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_0_0[i], OPSZ_2b));

    /* Testing SMULLT  <Zd>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_1_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_1_0[6] = {
        "smullt %z0.h %z0.h $0x00 -> %z0.s",   "smullt %z6.h %z3.h $0x04 -> %z5.s",
        "smullt %z11.h %z4.h $0x05 -> %z10.s", "smullt %z17.h %z6.h $0x07 -> %z16.s",
        "smullt %z22.h %z7.h $0x00 -> %z21.s", "smullt %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(smullt, smullt_sve_idx_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_1_0[i], OPSZ_3b));
}

TEST_INSTR(sqdmlalb_sve_idx_vector)
{

    /* Testing SQDMLALB <Zda>.D, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i2_0_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_0_0[6] = {
        "sqdmlalb %z0.d %z0.s %z0.s $0x00 -> %z0.d",
        "sqdmlalb %z5.d %z6.s %z4.s $0x03 -> %z5.d",
        "sqdmlalb %z10.d %z11.s %z7.s $0x00 -> %z10.d",
        "sqdmlalb %z16.d %z17.s %z10.s $0x01 -> %z16.d",
        "sqdmlalb %z21.d %z22.s %z12.s $0x01 -> %z21.d",
        "sqdmlalb %z31.d %z31.s %z15.s $0x03 -> %z31.d",
    };
    TEST_LOOP(sqdmlalb, sqdmlalb_sve_idx_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_0_0[i], OPSZ_2b));

    /* Testing SQDMLALB <Zda>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_1_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_1_0[6] = {
        "sqdmlalb %z0.s %z0.h %z0.h $0x00 -> %z0.s",
        "sqdmlalb %z5.s %z6.h %z3.h $0x04 -> %z5.s",
        "sqdmlalb %z10.s %z11.h %z4.h $0x05 -> %z10.s",
        "sqdmlalb %z16.s %z17.h %z6.h $0x07 -> %z16.s",
        "sqdmlalb %z21.s %z22.h %z7.h $0x00 -> %z21.s",
        "sqdmlalb %z31.s %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(sqdmlalb, sqdmlalb_sve_idx_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_1_0[i], OPSZ_3b));
}

TEST_INSTR(sqdmlalt_sve_idx_vector)
{

    /* Testing SQDMLALT <Zda>.D, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i2_0_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_0_0[6] = {
        "sqdmlalt %z0.d %z0.s %z0.s $0x00 -> %z0.d",
        "sqdmlalt %z5.d %z6.s %z4.s $0x03 -> %z5.d",
        "sqdmlalt %z10.d %z11.s %z7.s $0x00 -> %z10.d",
        "sqdmlalt %z16.d %z17.s %z10.s $0x01 -> %z16.d",
        "sqdmlalt %z21.d %z22.s %z12.s $0x01 -> %z21.d",
        "sqdmlalt %z31.d %z31.s %z15.s $0x03 -> %z31.d",
    };
    TEST_LOOP(sqdmlalt, sqdmlalt_sve_idx_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_0_0[i], OPSZ_2b));

    /* Testing SQDMLALT <Zda>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_1_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_1_0[6] = {
        "sqdmlalt %z0.s %z0.h %z0.h $0x00 -> %z0.s",
        "sqdmlalt %z5.s %z6.h %z3.h $0x04 -> %z5.s",
        "sqdmlalt %z10.s %z11.h %z4.h $0x05 -> %z10.s",
        "sqdmlalt %z16.s %z17.h %z6.h $0x07 -> %z16.s",
        "sqdmlalt %z21.s %z22.h %z7.h $0x00 -> %z21.s",
        "sqdmlalt %z31.s %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(sqdmlalt, sqdmlalt_sve_idx_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_1_0[i], OPSZ_3b));
}

TEST_INSTR(sqdmlslb_sve_idx_vector)
{

    /* Testing SQDMLSLB <Zda>.D, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i2_0_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_0_0[6] = {
        "sqdmlslb %z0.d %z0.s %z0.s $0x00 -> %z0.d",
        "sqdmlslb %z5.d %z6.s %z4.s $0x03 -> %z5.d",
        "sqdmlslb %z10.d %z11.s %z7.s $0x00 -> %z10.d",
        "sqdmlslb %z16.d %z17.s %z10.s $0x01 -> %z16.d",
        "sqdmlslb %z21.d %z22.s %z12.s $0x01 -> %z21.d",
        "sqdmlslb %z31.d %z31.s %z15.s $0x03 -> %z31.d",
    };
    TEST_LOOP(sqdmlslb, sqdmlslb_sve_idx_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_0_0[i], OPSZ_2b));

    /* Testing SQDMLSLB <Zda>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_1_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_1_0[6] = {
        "sqdmlslb %z0.s %z0.h %z0.h $0x00 -> %z0.s",
        "sqdmlslb %z5.s %z6.h %z3.h $0x04 -> %z5.s",
        "sqdmlslb %z10.s %z11.h %z4.h $0x05 -> %z10.s",
        "sqdmlslb %z16.s %z17.h %z6.h $0x07 -> %z16.s",
        "sqdmlslb %z21.s %z22.h %z7.h $0x00 -> %z21.s",
        "sqdmlslb %z31.s %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(sqdmlslb, sqdmlslb_sve_idx_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_1_0[i], OPSZ_3b));
}

TEST_INSTR(sqdmlslt_sve_idx_vector)
{

    /* Testing SQDMLSLT <Zda>.D, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i2_0_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_0_0[6] = {
        "sqdmlslt %z0.d %z0.s %z0.s $0x00 -> %z0.d",
        "sqdmlslt %z5.d %z6.s %z4.s $0x03 -> %z5.d",
        "sqdmlslt %z10.d %z11.s %z7.s $0x00 -> %z10.d",
        "sqdmlslt %z16.d %z17.s %z10.s $0x01 -> %z16.d",
        "sqdmlslt %z21.d %z22.s %z12.s $0x01 -> %z21.d",
        "sqdmlslt %z31.d %z31.s %z15.s $0x03 -> %z31.d",
    };
    TEST_LOOP(sqdmlslt, sqdmlslt_sve_idx_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_0_0[i], OPSZ_2b));

    /* Testing SQDMLSLT <Zda>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_1_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_1_0[6] = {
        "sqdmlslt %z0.s %z0.h %z0.h $0x00 -> %z0.s",
        "sqdmlslt %z5.s %z6.h %z3.h $0x04 -> %z5.s",
        "sqdmlslt %z10.s %z11.h %z4.h $0x05 -> %z10.s",
        "sqdmlslt %z16.s %z17.h %z6.h $0x07 -> %z16.s",
        "sqdmlslt %z21.s %z22.h %z7.h $0x00 -> %z21.s",
        "sqdmlslt %z31.s %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(sqdmlslt, sqdmlslt_sve_idx_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_1_0[i], OPSZ_3b));
}

TEST_INSTR(sqdmulh_sve_idx)
{

    /* Testing SQDMULH <Zd>.D, <Zn>.D, <Zm>.D[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i1_0_0[6] = { 0, 1, 1, 1, 0, 1 };
    const char *const expected_0_0[6] = {
        "sqdmulh %z0.d %z0.d $0x00 -> %z0.d",    "sqdmulh %z6.d %z4.d $0x01 -> %z5.d",
        "sqdmulh %z11.d %z7.d $0x01 -> %z10.d",  "sqdmulh %z17.d %z10.d $0x01 -> %z16.d",
        "sqdmulh %z22.d %z12.d $0x00 -> %z21.d", "sqdmulh %z31.d %z15.d $0x01 -> %z31.d",
    };
    TEST_LOOP(sqdmulh, sqdmulh_sve_idx, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_8),
              opnd_create_immed_uint(i1_0_0[i], OPSZ_1b));

    /* Testing SQDMULH <Zd>.S, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i2_1_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_1_0[6] = {
        "sqdmulh %z0.s %z0.s $0x00 -> %z0.s",   "sqdmulh %z6.s %z3.s $0x03 -> %z5.s",
        "sqdmulh %z11.s %z4.s $0x00 -> %z10.s", "sqdmulh %z17.s %z6.s $0x01 -> %z16.s",
        "sqdmulh %z22.s %z7.s $0x01 -> %z21.s", "sqdmulh %z31.s %z7.s $0x03 -> %z31.s",
    };
    TEST_LOOP(sqdmulh, sqdmulh_sve_idx, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_1_0[i], OPSZ_2b));

    /* Testing SQDMULH <Zd>.H, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_2_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_2_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_2_0[6] = {
        "sqdmulh %z0.h %z0.h $0x00 -> %z0.h",   "sqdmulh %z6.h %z3.h $0x04 -> %z5.h",
        "sqdmulh %z11.h %z4.h $0x05 -> %z10.h", "sqdmulh %z17.h %z6.h $0x07 -> %z16.h",
        "sqdmulh %z22.h %z7.h $0x00 -> %z21.h", "sqdmulh %z31.h %z7.h $0x07 -> %z31.h",
    };
    TEST_LOOP(sqdmulh, sqdmulh_sve_idx, 6, expected_2_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_2_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_2_0[i], OPSZ_3b));
}

TEST_INSTR(sqdmullb_sve_idx_vector)
{

    /* Testing SQDMULLB <Zd>.D, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i2_0_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_0_0[6] = {
        "sqdmullb %z0.s %z0.s $0x00 -> %z0.d",
        "sqdmullb %z6.s %z4.s $0x03 -> %z5.d",
        "sqdmullb %z11.s %z7.s $0x00 -> %z10.d",
        "sqdmullb %z17.s %z10.s $0x01 -> %z16.d",
        "sqdmullb %z22.s %z12.s $0x01 -> %z21.d",
        "sqdmullb %z31.s %z15.s $0x03 -> %z31.d",
    };
    TEST_LOOP(sqdmullb, sqdmullb_sve_idx_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_0_0[i], OPSZ_2b));

    /* Testing SQDMULLB <Zd>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_1_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_1_0[6] = {
        "sqdmullb %z0.h %z0.h $0x00 -> %z0.s",   "sqdmullb %z6.h %z3.h $0x04 -> %z5.s",
        "sqdmullb %z11.h %z4.h $0x05 -> %z10.s", "sqdmullb %z17.h %z6.h $0x07 -> %z16.s",
        "sqdmullb %z22.h %z7.h $0x00 -> %z21.s", "sqdmullb %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(sqdmullb, sqdmullb_sve_idx_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_1_0[i], OPSZ_3b));
}

TEST_INSTR(sqdmullt_sve_idx_vector)
{

    /* Testing SQDMULLT <Zd>.D, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i2_0_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_0_0[6] = {
        "sqdmullt %z0.s %z0.s $0x00 -> %z0.d",
        "sqdmullt %z6.s %z4.s $0x03 -> %z5.d",
        "sqdmullt %z11.s %z7.s $0x00 -> %z10.d",
        "sqdmullt %z17.s %z10.s $0x01 -> %z16.d",
        "sqdmullt %z22.s %z12.s $0x01 -> %z21.d",
        "sqdmullt %z31.s %z15.s $0x03 -> %z31.d",
    };
    TEST_LOOP(sqdmullt, sqdmullt_sve_idx_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_0_0[i], OPSZ_2b));

    /* Testing SQDMULLT <Zd>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_1_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_1_0[6] = {
        "sqdmullt %z0.h %z0.h $0x00 -> %z0.s",   "sqdmullt %z6.h %z3.h $0x04 -> %z5.s",
        "sqdmullt %z11.h %z4.h $0x05 -> %z10.s", "sqdmullt %z17.h %z6.h $0x07 -> %z16.s",
        "sqdmullt %z22.h %z7.h $0x00 -> %z21.s", "sqdmullt %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(sqdmullt, sqdmullt_sve_idx_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_1_0[i], OPSZ_3b));
}

TEST_INSTR(sqrdmlah_sve_idx)
{

    /* Testing SQRDMLAH <Zda>.D, <Zn>.D, <Zm>.D[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i1_0_0[6] = { 0, 1, 1, 1, 0, 1 };
    const char *const expected_0_0[6] = {
        "sqrdmlah %z0.d %z0.d %z0.d $0x00 -> %z0.d",
        "sqrdmlah %z5.d %z6.d %z4.d $0x01 -> %z5.d",
        "sqrdmlah %z10.d %z11.d %z7.d $0x01 -> %z10.d",
        "sqrdmlah %z16.d %z17.d %z10.d $0x01 -> %z16.d",
        "sqrdmlah %z21.d %z22.d %z12.d $0x00 -> %z21.d",
        "sqrdmlah %z31.d %z31.d %z15.d $0x01 -> %z31.d",
    };
    TEST_LOOP(sqrdmlah, sqrdmlah_sve_idx, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_8),
              opnd_create_immed_uint(i1_0_0[i], OPSZ_1b));

    /* Testing SQRDMLAH <Zda>.S, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i2_1_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_1_0[6] = {
        "sqrdmlah %z0.s %z0.s %z0.s $0x00 -> %z0.s",
        "sqrdmlah %z5.s %z6.s %z3.s $0x03 -> %z5.s",
        "sqrdmlah %z10.s %z11.s %z4.s $0x00 -> %z10.s",
        "sqrdmlah %z16.s %z17.s %z6.s $0x01 -> %z16.s",
        "sqrdmlah %z21.s %z22.s %z7.s $0x01 -> %z21.s",
        "sqrdmlah %z31.s %z31.s %z7.s $0x03 -> %z31.s",
    };
    TEST_LOOP(sqrdmlah, sqrdmlah_sve_idx, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_1_0[i], OPSZ_2b));

    /* Testing SQRDMLAH <Zda>.H, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_2_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_2_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_2_0[6] = {
        "sqrdmlah %z0.h %z0.h %z0.h $0x00 -> %z0.h",
        "sqrdmlah %z5.h %z6.h %z3.h $0x04 -> %z5.h",
        "sqrdmlah %z10.h %z11.h %z4.h $0x05 -> %z10.h",
        "sqrdmlah %z16.h %z17.h %z6.h $0x07 -> %z16.h",
        "sqrdmlah %z21.h %z22.h %z7.h $0x00 -> %z21.h",
        "sqrdmlah %z31.h %z31.h %z7.h $0x07 -> %z31.h",
    };
    TEST_LOOP(sqrdmlah, sqrdmlah_sve_idx, 6, expected_2_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_2_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_2_0[i], OPSZ_3b));
}

TEST_INSTR(sqrdmlsh_sve_idx)
{

    /* Testing SQRDMLSH <Zda>.D, <Zn>.D, <Zm>.D[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i1_0_0[6] = { 0, 1, 1, 1, 0, 1 };
    const char *const expected_0_0[6] = {
        "sqrdmlsh %z0.d %z0.d %z0.d $0x00 -> %z0.d",
        "sqrdmlsh %z5.d %z6.d %z4.d $0x01 -> %z5.d",
        "sqrdmlsh %z10.d %z11.d %z7.d $0x01 -> %z10.d",
        "sqrdmlsh %z16.d %z17.d %z10.d $0x01 -> %z16.d",
        "sqrdmlsh %z21.d %z22.d %z12.d $0x00 -> %z21.d",
        "sqrdmlsh %z31.d %z31.d %z15.d $0x01 -> %z31.d",
    };
    TEST_LOOP(sqrdmlsh, sqrdmlsh_sve_idx, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_8),
              opnd_create_immed_uint(i1_0_0[i], OPSZ_1b));

    /* Testing SQRDMLSH <Zda>.S, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i2_1_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_1_0[6] = {
        "sqrdmlsh %z0.s %z0.s %z0.s $0x00 -> %z0.s",
        "sqrdmlsh %z5.s %z6.s %z3.s $0x03 -> %z5.s",
        "sqrdmlsh %z10.s %z11.s %z4.s $0x00 -> %z10.s",
        "sqrdmlsh %z16.s %z17.s %z6.s $0x01 -> %z16.s",
        "sqrdmlsh %z21.s %z22.s %z7.s $0x01 -> %z21.s",
        "sqrdmlsh %z31.s %z31.s %z7.s $0x03 -> %z31.s",
    };
    TEST_LOOP(sqrdmlsh, sqrdmlsh_sve_idx, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_1_0[i], OPSZ_2b));

    /* Testing SQRDMLSH <Zda>.H, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_2_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_2_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_2_0[6] = {
        "sqrdmlsh %z0.h %z0.h %z0.h $0x00 -> %z0.h",
        "sqrdmlsh %z5.h %z6.h %z3.h $0x04 -> %z5.h",
        "sqrdmlsh %z10.h %z11.h %z4.h $0x05 -> %z10.h",
        "sqrdmlsh %z16.h %z17.h %z6.h $0x07 -> %z16.h",
        "sqrdmlsh %z21.h %z22.h %z7.h $0x00 -> %z21.h",
        "sqrdmlsh %z31.h %z31.h %z7.h $0x07 -> %z31.h",
    };
    TEST_LOOP(sqrdmlsh, sqrdmlsh_sve_idx, 6, expected_2_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_2_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_2_0[i], OPSZ_3b));
}

TEST_INSTR(sqrdmulh_sve_idx)
{

    /* Testing SQRDMULH <Zd>.D, <Zn>.D, <Zm>.D[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i1_0_0[6] = { 0, 1, 1, 1, 0, 1 };
    const char *const expected_0_0[6] = {
        "sqrdmulh %z0.d %z0.d $0x00 -> %z0.d",
        "sqrdmulh %z6.d %z4.d $0x01 -> %z5.d",
        "sqrdmulh %z11.d %z7.d $0x01 -> %z10.d",
        "sqrdmulh %z17.d %z10.d $0x01 -> %z16.d",
        "sqrdmulh %z22.d %z12.d $0x00 -> %z21.d",
        "sqrdmulh %z31.d %z15.d $0x01 -> %z31.d",
    };
    TEST_LOOP(sqrdmulh, sqrdmulh_sve_idx, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_8),
              opnd_create_immed_uint(i1_0_0[i], OPSZ_1b));

    /* Testing SQRDMULH <Zd>.S, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i2_1_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_1_0[6] = {
        "sqrdmulh %z0.s %z0.s $0x00 -> %z0.s",   "sqrdmulh %z6.s %z3.s $0x03 -> %z5.s",
        "sqrdmulh %z11.s %z4.s $0x00 -> %z10.s", "sqrdmulh %z17.s %z6.s $0x01 -> %z16.s",
        "sqrdmulh %z22.s %z7.s $0x01 -> %z21.s", "sqrdmulh %z31.s %z7.s $0x03 -> %z31.s",
    };
    TEST_LOOP(sqrdmulh, sqrdmulh_sve_idx, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_1_0[i], OPSZ_2b));

    /* Testing SQRDMULH <Zd>.H, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_2_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_2_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_2_0[6] = {
        "sqrdmulh %z0.h %z0.h $0x00 -> %z0.h",   "sqrdmulh %z6.h %z3.h $0x04 -> %z5.h",
        "sqrdmulh %z11.h %z4.h $0x05 -> %z10.h", "sqrdmulh %z17.h %z6.h $0x07 -> %z16.h",
        "sqrdmulh %z22.h %z7.h $0x00 -> %z21.h", "sqrdmulh %z31.h %z7.h $0x07 -> %z31.h",
    };
    TEST_LOOP(sqrdmulh, sqrdmulh_sve_idx, 6, expected_2_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_2_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_2_0[i], OPSZ_3b));
}

TEST_INSTR(umlalb_sve_idx_vector)
{

    /* Testing UMLALB  <Zda>.D, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i2_0_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_0_0[6] = {
        "umlalb %z0.d %z0.s %z0.s $0x00 -> %z0.d",
        "umlalb %z5.d %z6.s %z4.s $0x03 -> %z5.d",
        "umlalb %z10.d %z11.s %z7.s $0x00 -> %z10.d",
        "umlalb %z16.d %z17.s %z10.s $0x01 -> %z16.d",
        "umlalb %z21.d %z22.s %z12.s $0x01 -> %z21.d",
        "umlalb %z31.d %z31.s %z15.s $0x03 -> %z31.d",
    };
    TEST_LOOP(umlalb, umlalb_sve_idx_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_0_0[i], OPSZ_2b));

    /* Testing UMLALB  <Zda>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_1_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_1_0[6] = {
        "umlalb %z0.s %z0.h %z0.h $0x00 -> %z0.s",
        "umlalb %z5.s %z6.h %z3.h $0x04 -> %z5.s",
        "umlalb %z10.s %z11.h %z4.h $0x05 -> %z10.s",
        "umlalb %z16.s %z17.h %z6.h $0x07 -> %z16.s",
        "umlalb %z21.s %z22.h %z7.h $0x00 -> %z21.s",
        "umlalb %z31.s %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(umlalb, umlalb_sve_idx_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_1_0[i], OPSZ_3b));
}

TEST_INSTR(umlalt_sve_idx_vector)
{

    /* Testing UMLALT  <Zda>.D, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i2_0_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_0_0[6] = {
        "umlalt %z0.d %z0.s %z0.s $0x00 -> %z0.d",
        "umlalt %z5.d %z6.s %z4.s $0x03 -> %z5.d",
        "umlalt %z10.d %z11.s %z7.s $0x00 -> %z10.d",
        "umlalt %z16.d %z17.s %z10.s $0x01 -> %z16.d",
        "umlalt %z21.d %z22.s %z12.s $0x01 -> %z21.d",
        "umlalt %z31.d %z31.s %z15.s $0x03 -> %z31.d",
    };
    TEST_LOOP(umlalt, umlalt_sve_idx_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_0_0[i], OPSZ_2b));

    /* Testing UMLALT  <Zda>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_1_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_1_0[6] = {
        "umlalt %z0.s %z0.h %z0.h $0x00 -> %z0.s",
        "umlalt %z5.s %z6.h %z3.h $0x04 -> %z5.s",
        "umlalt %z10.s %z11.h %z4.h $0x05 -> %z10.s",
        "umlalt %z16.s %z17.h %z6.h $0x07 -> %z16.s",
        "umlalt %z21.s %z22.h %z7.h $0x00 -> %z21.s",
        "umlalt %z31.s %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(umlalt, umlalt_sve_idx_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_1_0[i], OPSZ_3b));
}

TEST_INSTR(umlslb_sve_idx_vector)
{

    /* Testing UMLSLB  <Zda>.D, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i2_0_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_0_0[6] = {
        "umlslb %z0.d %z0.s %z0.s $0x00 -> %z0.d",
        "umlslb %z5.d %z6.s %z4.s $0x03 -> %z5.d",
        "umlslb %z10.d %z11.s %z7.s $0x00 -> %z10.d",
        "umlslb %z16.d %z17.s %z10.s $0x01 -> %z16.d",
        "umlslb %z21.d %z22.s %z12.s $0x01 -> %z21.d",
        "umlslb %z31.d %z31.s %z15.s $0x03 -> %z31.d",
    };
    TEST_LOOP(umlslb, umlslb_sve_idx_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_0_0[i], OPSZ_2b));

    /* Testing UMLSLB  <Zda>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_1_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_1_0[6] = {
        "umlslb %z0.s %z0.h %z0.h $0x00 -> %z0.s",
        "umlslb %z5.s %z6.h %z3.h $0x04 -> %z5.s",
        "umlslb %z10.s %z11.h %z4.h $0x05 -> %z10.s",
        "umlslb %z16.s %z17.h %z6.h $0x07 -> %z16.s",
        "umlslb %z21.s %z22.h %z7.h $0x00 -> %z21.s",
        "umlslb %z31.s %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(umlslb, umlslb_sve_idx_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_1_0[i], OPSZ_3b));
}

TEST_INSTR(umlslt_sve_idx_vector)
{

    /* Testing UMLSLT  <Zda>.D, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i2_0_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_0_0[6] = {
        "umlslt %z0.d %z0.s %z0.s $0x00 -> %z0.d",
        "umlslt %z5.d %z6.s %z4.s $0x03 -> %z5.d",
        "umlslt %z10.d %z11.s %z7.s $0x00 -> %z10.d",
        "umlslt %z16.d %z17.s %z10.s $0x01 -> %z16.d",
        "umlslt %z21.d %z22.s %z12.s $0x01 -> %z21.d",
        "umlslt %z31.d %z31.s %z15.s $0x03 -> %z31.d",
    };
    TEST_LOOP(umlslt, umlslt_sve_idx_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_0_0[i], OPSZ_2b));

    /* Testing UMLSLT  <Zda>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_1_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_1_0[6] = {
        "umlslt %z0.s %z0.h %z0.h $0x00 -> %z0.s",
        "umlslt %z5.s %z6.h %z3.h $0x04 -> %z5.s",
        "umlslt %z10.s %z11.h %z4.h $0x05 -> %z10.s",
        "umlslt %z16.s %z17.h %z6.h $0x07 -> %z16.s",
        "umlslt %z21.s %z22.h %z7.h $0x00 -> %z21.s",
        "umlslt %z31.s %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(umlslt, umlslt_sve_idx_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_1_0[i], OPSZ_3b));
}

TEST_INSTR(umullb_sve_idx_vector)
{

    /* Testing UMULLB  <Zd>.D, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i2_0_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_0_0[6] = {
        "umullb %z0.s %z0.s $0x00 -> %z0.d",    "umullb %z6.s %z4.s $0x03 -> %z5.d",
        "umullb %z11.s %z7.s $0x00 -> %z10.d",  "umullb %z17.s %z10.s $0x01 -> %z16.d",
        "umullb %z22.s %z12.s $0x01 -> %z21.d", "umullb %z31.s %z15.s $0x03 -> %z31.d",
    };
    TEST_LOOP(umullb, umullb_sve_idx_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_0_0[i], OPSZ_2b));

    /* Testing UMULLB  <Zd>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_1_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_1_0[6] = {
        "umullb %z0.h %z0.h $0x00 -> %z0.s",   "umullb %z6.h %z3.h $0x04 -> %z5.s",
        "umullb %z11.h %z4.h $0x05 -> %z10.s", "umullb %z17.h %z6.h $0x07 -> %z16.s",
        "umullb %z22.h %z7.h $0x00 -> %z21.s", "umullb %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(umullb, umullb_sve_idx_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_1_0[i], OPSZ_3b));
}

TEST_INSTR(umullt_sve_idx_vector)
{

    /* Testing UMULLT  <Zd>.D, <Zn>.S, <Zm>.S[<index>] */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i2_0_0[6] = { 0, 3, 0, 1, 1, 3 };
    const char *const expected_0_0[6] = {
        "umullt %z0.s %z0.s $0x00 -> %z0.d",    "umullt %z6.s %z4.s $0x03 -> %z5.d",
        "umullt %z11.s %z7.s $0x00 -> %z10.d",  "umullt %z17.s %z10.s $0x01 -> %z16.d",
        "umullt %z22.s %z12.s $0x01 -> %z21.d", "umullt %z31.s %z15.s $0x03 -> %z31.d",
    };
    TEST_LOOP(umullt, umullt_sve_idx_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i2_0_0[i], OPSZ_2b));

    /* Testing UMULLT  <Zd>.S, <Zn>.H, <Zm>.H[<index>] */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i3_1_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *const expected_1_0[6] = {
        "umullt %z0.h %z0.h $0x00 -> %z0.s",   "umullt %z6.h %z3.h $0x04 -> %z5.s",
        "umullt %z11.h %z4.h $0x05 -> %z10.s", "umullt %z17.h %z6.h $0x07 -> %z16.s",
        "umullt %z22.h %z7.h $0x00 -> %z21.s", "umullt %z31.h %z7.h $0x07 -> %z31.s",
    };
    TEST_LOOP(umullt, umullt_sve_idx_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i3_1_0[i], OPSZ_3b));
}

TEST_INSTR(addp_sve_pred)
{

    /* Testing ADDP    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "addp   %p0/m %z0.b %z0.b -> %z0.b",    "addp   %p2/m %z5.b %z7.b -> %z5.b",
        "addp   %p3/m %z10.b %z12.b -> %z10.b", "addp   %p5/m %z16.b %z18.b -> %z16.b",
        "addp   %p6/m %z21.b %z23.b -> %z21.b", "addp   %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(addp, addp_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "addp   %p0/m %z0.h %z0.h -> %z0.h",    "addp   %p2/m %z5.h %z7.h -> %z5.h",
        "addp   %p3/m %z10.h %z12.h -> %z10.h", "addp   %p5/m %z16.h %z18.h -> %z16.h",
        "addp   %p6/m %z21.h %z23.h -> %z21.h", "addp   %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(addp, addp_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "addp   %p0/m %z0.s %z0.s -> %z0.s",    "addp   %p2/m %z5.s %z7.s -> %z5.s",
        "addp   %p3/m %z10.s %z12.s -> %z10.s", "addp   %p5/m %z16.s %z18.s -> %z16.s",
        "addp   %p6/m %z21.s %z23.s -> %z21.s", "addp   %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(addp, addp_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "addp   %p0/m %z0.d %z0.d -> %z0.d",    "addp   %p2/m %z5.d %z7.d -> %z5.d",
        "addp   %p3/m %z10.d %z12.d -> %z10.d", "addp   %p5/m %z16.d %z18.d -> %z16.d",
        "addp   %p6/m %z21.d %z23.d -> %z21.d", "addp   %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(addp, addp_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(faddp_sve_pred)
{

    /* Testing FADDP   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "faddp  %p0/m %z0.h %z0.h -> %z0.h",    "faddp  %p2/m %z5.h %z7.h -> %z5.h",
        "faddp  %p3/m %z10.h %z12.h -> %z10.h", "faddp  %p5/m %z16.h %z18.h -> %z16.h",
        "faddp  %p6/m %z21.h %z23.h -> %z21.h", "faddp  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(faddp, faddp_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "faddp  %p0/m %z0.s %z0.s -> %z0.s",    "faddp  %p2/m %z5.s %z7.s -> %z5.s",
        "faddp  %p3/m %z10.s %z12.s -> %z10.s", "faddp  %p5/m %z16.s %z18.s -> %z16.s",
        "faddp  %p6/m %z21.s %z23.s -> %z21.s", "faddp  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(faddp, faddp_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "faddp  %p0/m %z0.d %z0.d -> %z0.d",    "faddp  %p2/m %z5.d %z7.d -> %z5.d",
        "faddp  %p3/m %z10.d %z12.d -> %z10.d", "faddp  %p5/m %z16.d %z18.d -> %z16.d",
        "faddp  %p6/m %z21.d %z23.d -> %z21.d", "faddp  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(faddp, faddp_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(fmaxnmp_sve_pred)
{

    /* Testing FMAXNMP <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "fmaxnmp %p0/m %z0.h %z0.h -> %z0.h",    "fmaxnmp %p2/m %z5.h %z7.h -> %z5.h",
        "fmaxnmp %p3/m %z10.h %z12.h -> %z10.h", "fmaxnmp %p5/m %z16.h %z18.h -> %z16.h",
        "fmaxnmp %p6/m %z21.h %z23.h -> %z21.h", "fmaxnmp %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(fmaxnmp, fmaxnmp_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "fmaxnmp %p0/m %z0.s %z0.s -> %z0.s",    "fmaxnmp %p2/m %z5.s %z7.s -> %z5.s",
        "fmaxnmp %p3/m %z10.s %z12.s -> %z10.s", "fmaxnmp %p5/m %z16.s %z18.s -> %z16.s",
        "fmaxnmp %p6/m %z21.s %z23.s -> %z21.s", "fmaxnmp %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(fmaxnmp, fmaxnmp_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "fmaxnmp %p0/m %z0.d %z0.d -> %z0.d",    "fmaxnmp %p2/m %z5.d %z7.d -> %z5.d",
        "fmaxnmp %p3/m %z10.d %z12.d -> %z10.d", "fmaxnmp %p5/m %z16.d %z18.d -> %z16.d",
        "fmaxnmp %p6/m %z21.d %z23.d -> %z21.d", "fmaxnmp %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(fmaxnmp, fmaxnmp_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(fmaxp_sve_pred)
{

    /* Testing FMAXP   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "fmaxp  %p0/m %z0.h %z0.h -> %z0.h",    "fmaxp  %p2/m %z5.h %z7.h -> %z5.h",
        "fmaxp  %p3/m %z10.h %z12.h -> %z10.h", "fmaxp  %p5/m %z16.h %z18.h -> %z16.h",
        "fmaxp  %p6/m %z21.h %z23.h -> %z21.h", "fmaxp  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(fmaxp, fmaxp_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "fmaxp  %p0/m %z0.s %z0.s -> %z0.s",    "fmaxp  %p2/m %z5.s %z7.s -> %z5.s",
        "fmaxp  %p3/m %z10.s %z12.s -> %z10.s", "fmaxp  %p5/m %z16.s %z18.s -> %z16.s",
        "fmaxp  %p6/m %z21.s %z23.s -> %z21.s", "fmaxp  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(fmaxp, fmaxp_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "fmaxp  %p0/m %z0.d %z0.d -> %z0.d",    "fmaxp  %p2/m %z5.d %z7.d -> %z5.d",
        "fmaxp  %p3/m %z10.d %z12.d -> %z10.d", "fmaxp  %p5/m %z16.d %z18.d -> %z16.d",
        "fmaxp  %p6/m %z21.d %z23.d -> %z21.d", "fmaxp  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(fmaxp, fmaxp_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(fminnmp_sve_pred)
{

    /* Testing FMINNMP <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "fminnmp %p0/m %z0.h %z0.h -> %z0.h",    "fminnmp %p2/m %z5.h %z7.h -> %z5.h",
        "fminnmp %p3/m %z10.h %z12.h -> %z10.h", "fminnmp %p5/m %z16.h %z18.h -> %z16.h",
        "fminnmp %p6/m %z21.h %z23.h -> %z21.h", "fminnmp %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(fminnmp, fminnmp_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "fminnmp %p0/m %z0.s %z0.s -> %z0.s",    "fminnmp %p2/m %z5.s %z7.s -> %z5.s",
        "fminnmp %p3/m %z10.s %z12.s -> %z10.s", "fminnmp %p5/m %z16.s %z18.s -> %z16.s",
        "fminnmp %p6/m %z21.s %z23.s -> %z21.s", "fminnmp %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(fminnmp, fminnmp_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "fminnmp %p0/m %z0.d %z0.d -> %z0.d",    "fminnmp %p2/m %z5.d %z7.d -> %z5.d",
        "fminnmp %p3/m %z10.d %z12.d -> %z10.d", "fminnmp %p5/m %z16.d %z18.d -> %z16.d",
        "fminnmp %p6/m %z21.d %z23.d -> %z21.d", "fminnmp %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(fminnmp, fminnmp_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(fminp_sve_pred)
{

    /* Testing FMINP   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "fminp  %p0/m %z0.h %z0.h -> %z0.h",    "fminp  %p2/m %z5.h %z7.h -> %z5.h",
        "fminp  %p3/m %z10.h %z12.h -> %z10.h", "fminp  %p5/m %z16.h %z18.h -> %z16.h",
        "fminp  %p6/m %z21.h %z23.h -> %z21.h", "fminp  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(fminp, fminp_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "fminp  %p0/m %z0.s %z0.s -> %z0.s",    "fminp  %p2/m %z5.s %z7.s -> %z5.s",
        "fminp  %p3/m %z10.s %z12.s -> %z10.s", "fminp  %p5/m %z16.s %z18.s -> %z16.s",
        "fminp  %p6/m %z21.s %z23.s -> %z21.s", "fminp  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(fminp, fminp_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "fminp  %p0/m %z0.d %z0.d -> %z0.d",    "fminp  %p2/m %z5.d %z7.d -> %z5.d",
        "fminp  %p3/m %z10.d %z12.d -> %z10.d", "fminp  %p5/m %z16.d %z18.d -> %z16.d",
        "fminp  %p6/m %z21.d %z23.d -> %z21.d", "fminp  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(fminp, fminp_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(histcnt_sve_pred)
{

    /* Testing HISTCNT <Zd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "histcnt %p0/z %z0.s %z0.s -> %z0.s",    "histcnt %p2/z %z7.s %z8.s -> %z5.s",
        "histcnt %p3/z %z12.s %z13.s -> %z10.s", "histcnt %p5/z %z18.s %z19.s -> %z16.s",
        "histcnt %p6/z %z23.s %z24.s -> %z21.s", "histcnt %p7/z %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(histcnt, histcnt_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_4));

    const char *const expected_0_1[6] = {
        "histcnt %p0/z %z0.d %z0.d -> %z0.d",    "histcnt %p2/z %z7.d %z8.d -> %z5.d",
        "histcnt %p3/z %z12.d %z13.d -> %z10.d", "histcnt %p5/z %z18.d %z19.d -> %z16.d",
        "histcnt %p6/z %z23.d %z24.d -> %z21.d", "histcnt %p7/z %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(histcnt, histcnt_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(shadd_sve_pred)
{

    /* Testing SHADD   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "shadd  %p0/m %z0.b %z0.b -> %z0.b",    "shadd  %p2/m %z5.b %z7.b -> %z5.b",
        "shadd  %p3/m %z10.b %z12.b -> %z10.b", "shadd  %p5/m %z16.b %z18.b -> %z16.b",
        "shadd  %p6/m %z21.b %z23.b -> %z21.b", "shadd  %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(shadd, shadd_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "shadd  %p0/m %z0.h %z0.h -> %z0.h",    "shadd  %p2/m %z5.h %z7.h -> %z5.h",
        "shadd  %p3/m %z10.h %z12.h -> %z10.h", "shadd  %p5/m %z16.h %z18.h -> %z16.h",
        "shadd  %p6/m %z21.h %z23.h -> %z21.h", "shadd  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(shadd, shadd_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "shadd  %p0/m %z0.s %z0.s -> %z0.s",    "shadd  %p2/m %z5.s %z7.s -> %z5.s",
        "shadd  %p3/m %z10.s %z12.s -> %z10.s", "shadd  %p5/m %z16.s %z18.s -> %z16.s",
        "shadd  %p6/m %z21.s %z23.s -> %z21.s", "shadd  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(shadd, shadd_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "shadd  %p0/m %z0.d %z0.d -> %z0.d",    "shadd  %p2/m %z5.d %z7.d -> %z5.d",
        "shadd  %p3/m %z10.d %z12.d -> %z10.d", "shadd  %p5/m %z16.d %z18.d -> %z16.d",
        "shadd  %p6/m %z21.d %z23.d -> %z21.d", "shadd  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(shadd, shadd_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(shsub_sve_pred)
{

    /* Testing SHSUB   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "shsub  %p0/m %z0.b %z0.b -> %z0.b",    "shsub  %p2/m %z5.b %z7.b -> %z5.b",
        "shsub  %p3/m %z10.b %z12.b -> %z10.b", "shsub  %p5/m %z16.b %z18.b -> %z16.b",
        "shsub  %p6/m %z21.b %z23.b -> %z21.b", "shsub  %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(shsub, shsub_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "shsub  %p0/m %z0.h %z0.h -> %z0.h",    "shsub  %p2/m %z5.h %z7.h -> %z5.h",
        "shsub  %p3/m %z10.h %z12.h -> %z10.h", "shsub  %p5/m %z16.h %z18.h -> %z16.h",
        "shsub  %p6/m %z21.h %z23.h -> %z21.h", "shsub  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(shsub, shsub_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "shsub  %p0/m %z0.s %z0.s -> %z0.s",    "shsub  %p2/m %z5.s %z7.s -> %z5.s",
        "shsub  %p3/m %z10.s %z12.s -> %z10.s", "shsub  %p5/m %z16.s %z18.s -> %z16.s",
        "shsub  %p6/m %z21.s %z23.s -> %z21.s", "shsub  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(shsub, shsub_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "shsub  %p0/m %z0.d %z0.d -> %z0.d",    "shsub  %p2/m %z5.d %z7.d -> %z5.d",
        "shsub  %p3/m %z10.d %z12.d -> %z10.d", "shsub  %p5/m %z16.d %z18.d -> %z16.d",
        "shsub  %p6/m %z21.d %z23.d -> %z21.d", "shsub  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(shsub, shsub_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(shsubr_sve_pred)
{

    /* Testing SHSUBR  <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "shsubr %p0/m %z0.b %z0.b -> %z0.b",    "shsubr %p2/m %z5.b %z7.b -> %z5.b",
        "shsubr %p3/m %z10.b %z12.b -> %z10.b", "shsubr %p5/m %z16.b %z18.b -> %z16.b",
        "shsubr %p6/m %z21.b %z23.b -> %z21.b", "shsubr %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(shsubr, shsubr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "shsubr %p0/m %z0.h %z0.h -> %z0.h",    "shsubr %p2/m %z5.h %z7.h -> %z5.h",
        "shsubr %p3/m %z10.h %z12.h -> %z10.h", "shsubr %p5/m %z16.h %z18.h -> %z16.h",
        "shsubr %p6/m %z21.h %z23.h -> %z21.h", "shsubr %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(shsubr, shsubr_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "shsubr %p0/m %z0.s %z0.s -> %z0.s",    "shsubr %p2/m %z5.s %z7.s -> %z5.s",
        "shsubr %p3/m %z10.s %z12.s -> %z10.s", "shsubr %p5/m %z16.s %z18.s -> %z16.s",
        "shsubr %p6/m %z21.s %z23.s -> %z21.s", "shsubr %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(shsubr, shsubr_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "shsubr %p0/m %z0.d %z0.d -> %z0.d",    "shsubr %p2/m %z5.d %z7.d -> %z5.d",
        "shsubr %p3/m %z10.d %z12.d -> %z10.d", "shsubr %p5/m %z16.d %z18.d -> %z16.d",
        "shsubr %p6/m %z21.d %z23.d -> %z21.d", "shsubr %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(shsubr, shsubr_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(smaxp_sve_pred)
{

    /* Testing SMAXP   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "smaxp  %p0/m %z0.b %z0.b -> %z0.b",    "smaxp  %p2/m %z5.b %z7.b -> %z5.b",
        "smaxp  %p3/m %z10.b %z12.b -> %z10.b", "smaxp  %p5/m %z16.b %z18.b -> %z16.b",
        "smaxp  %p6/m %z21.b %z23.b -> %z21.b", "smaxp  %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(smaxp, smaxp_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "smaxp  %p0/m %z0.h %z0.h -> %z0.h",    "smaxp  %p2/m %z5.h %z7.h -> %z5.h",
        "smaxp  %p3/m %z10.h %z12.h -> %z10.h", "smaxp  %p5/m %z16.h %z18.h -> %z16.h",
        "smaxp  %p6/m %z21.h %z23.h -> %z21.h", "smaxp  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(smaxp, smaxp_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "smaxp  %p0/m %z0.s %z0.s -> %z0.s",    "smaxp  %p2/m %z5.s %z7.s -> %z5.s",
        "smaxp  %p3/m %z10.s %z12.s -> %z10.s", "smaxp  %p5/m %z16.s %z18.s -> %z16.s",
        "smaxp  %p6/m %z21.s %z23.s -> %z21.s", "smaxp  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(smaxp, smaxp_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "smaxp  %p0/m %z0.d %z0.d -> %z0.d",    "smaxp  %p2/m %z5.d %z7.d -> %z5.d",
        "smaxp  %p3/m %z10.d %z12.d -> %z10.d", "smaxp  %p5/m %z16.d %z18.d -> %z16.d",
        "smaxp  %p6/m %z21.d %z23.d -> %z21.d", "smaxp  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(smaxp, smaxp_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sminp_sve_pred)
{

    /* Testing SMINP   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "sminp  %p0/m %z0.b %z0.b -> %z0.b",    "sminp  %p2/m %z5.b %z7.b -> %z5.b",
        "sminp  %p3/m %z10.b %z12.b -> %z10.b", "sminp  %p5/m %z16.b %z18.b -> %z16.b",
        "sminp  %p6/m %z21.b %z23.b -> %z21.b", "sminp  %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(sminp, sminp_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sminp  %p0/m %z0.h %z0.h -> %z0.h",    "sminp  %p2/m %z5.h %z7.h -> %z5.h",
        "sminp  %p3/m %z10.h %z12.h -> %z10.h", "sminp  %p5/m %z16.h %z18.h -> %z16.h",
        "sminp  %p6/m %z21.h %z23.h -> %z21.h", "sminp  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(sminp, sminp_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sminp  %p0/m %z0.s %z0.s -> %z0.s",    "sminp  %p2/m %z5.s %z7.s -> %z5.s",
        "sminp  %p3/m %z10.s %z12.s -> %z10.s", "sminp  %p5/m %z16.s %z18.s -> %z16.s",
        "sminp  %p6/m %z21.s %z23.s -> %z21.s", "sminp  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sminp, sminp_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "sminp  %p0/m %z0.d %z0.d -> %z0.d",    "sminp  %p2/m %z5.d %z7.d -> %z5.d",
        "sminp  %p3/m %z10.d %z12.d -> %z10.d", "sminp  %p5/m %z16.d %z18.d -> %z16.d",
        "sminp  %p6/m %z21.d %z23.d -> %z21.d", "sminp  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sminp, sminp_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sqrshl_sve_pred)
{

    /* Testing SQRSHL  <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "sqrshl %p0/m %z0.b %z0.b -> %z0.b",    "sqrshl %p2/m %z5.b %z7.b -> %z5.b",
        "sqrshl %p3/m %z10.b %z12.b -> %z10.b", "sqrshl %p5/m %z16.b %z18.b -> %z16.b",
        "sqrshl %p6/m %z21.b %z23.b -> %z21.b", "sqrshl %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(sqrshl, sqrshl_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqrshl %p0/m %z0.h %z0.h -> %z0.h",    "sqrshl %p2/m %z5.h %z7.h -> %z5.h",
        "sqrshl %p3/m %z10.h %z12.h -> %z10.h", "sqrshl %p5/m %z16.h %z18.h -> %z16.h",
        "sqrshl %p6/m %z21.h %z23.h -> %z21.h", "sqrshl %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(sqrshl, sqrshl_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqrshl %p0/m %z0.s %z0.s -> %z0.s",    "sqrshl %p2/m %z5.s %z7.s -> %z5.s",
        "sqrshl %p3/m %z10.s %z12.s -> %z10.s", "sqrshl %p5/m %z16.s %z18.s -> %z16.s",
        "sqrshl %p6/m %z21.s %z23.s -> %z21.s", "sqrshl %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sqrshl, sqrshl_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "sqrshl %p0/m %z0.d %z0.d -> %z0.d",    "sqrshl %p2/m %z5.d %z7.d -> %z5.d",
        "sqrshl %p3/m %z10.d %z12.d -> %z10.d", "sqrshl %p5/m %z16.d %z18.d -> %z16.d",
        "sqrshl %p6/m %z21.d %z23.d -> %z21.d", "sqrshl %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sqrshl, sqrshl_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sqrshlr_sve_pred)
{

    /* Testing SQRSHLR <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "sqrshlr %p0/m %z0.b %z0.b -> %z0.b",    "sqrshlr %p2/m %z5.b %z7.b -> %z5.b",
        "sqrshlr %p3/m %z10.b %z12.b -> %z10.b", "sqrshlr %p5/m %z16.b %z18.b -> %z16.b",
        "sqrshlr %p6/m %z21.b %z23.b -> %z21.b", "sqrshlr %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(sqrshlr, sqrshlr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqrshlr %p0/m %z0.h %z0.h -> %z0.h",    "sqrshlr %p2/m %z5.h %z7.h -> %z5.h",
        "sqrshlr %p3/m %z10.h %z12.h -> %z10.h", "sqrshlr %p5/m %z16.h %z18.h -> %z16.h",
        "sqrshlr %p6/m %z21.h %z23.h -> %z21.h", "sqrshlr %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(sqrshlr, sqrshlr_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqrshlr %p0/m %z0.s %z0.s -> %z0.s",    "sqrshlr %p2/m %z5.s %z7.s -> %z5.s",
        "sqrshlr %p3/m %z10.s %z12.s -> %z10.s", "sqrshlr %p5/m %z16.s %z18.s -> %z16.s",
        "sqrshlr %p6/m %z21.s %z23.s -> %z21.s", "sqrshlr %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sqrshlr, sqrshlr_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "sqrshlr %p0/m %z0.d %z0.d -> %z0.d",    "sqrshlr %p2/m %z5.d %z7.d -> %z5.d",
        "sqrshlr %p3/m %z10.d %z12.d -> %z10.d", "sqrshlr %p5/m %z16.d %z18.d -> %z16.d",
        "sqrshlr %p6/m %z21.d %z23.d -> %z21.d", "sqrshlr %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sqrshlr, sqrshlr_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sqshl_sve_pred)
{

    /* Testing SQSHL   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "sqshl  %p0/m %z0.b %z0.b -> %z0.b",    "sqshl  %p2/m %z5.b %z7.b -> %z5.b",
        "sqshl  %p3/m %z10.b %z12.b -> %z10.b", "sqshl  %p5/m %z16.b %z18.b -> %z16.b",
        "sqshl  %p6/m %z21.b %z23.b -> %z21.b", "sqshl  %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(sqshl, sqshl_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqshl  %p0/m %z0.h %z0.h -> %z0.h",    "sqshl  %p2/m %z5.h %z7.h -> %z5.h",
        "sqshl  %p3/m %z10.h %z12.h -> %z10.h", "sqshl  %p5/m %z16.h %z18.h -> %z16.h",
        "sqshl  %p6/m %z21.h %z23.h -> %z21.h", "sqshl  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(sqshl, sqshl_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqshl  %p0/m %z0.s %z0.s -> %z0.s",    "sqshl  %p2/m %z5.s %z7.s -> %z5.s",
        "sqshl  %p3/m %z10.s %z12.s -> %z10.s", "sqshl  %p5/m %z16.s %z18.s -> %z16.s",
        "sqshl  %p6/m %z21.s %z23.s -> %z21.s", "sqshl  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sqshl, sqshl_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "sqshl  %p0/m %z0.d %z0.d -> %z0.d",    "sqshl  %p2/m %z5.d %z7.d -> %z5.d",
        "sqshl  %p3/m %z10.d %z12.d -> %z10.d", "sqshl  %p5/m %z16.d %z18.d -> %z16.d",
        "sqshl  %p6/m %z21.d %z23.d -> %z21.d", "sqshl  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sqshl, sqshl_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));

    /* Testing SQSHL   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, #<const> */
    static const uint imm3_1_0[6] = { 0, 3, 4, 6, 7, 7 };
    const char *const expected_1_0[6] = {
        "sqshl  %p0/m %z0.b $0x00 -> %z0.b",   "sqshl  %p2/m %z5.b $0x03 -> %z5.b",
        "sqshl  %p3/m %z10.b $0x04 -> %z10.b", "sqshl  %p5/m %z16.b $0x06 -> %z16.b",
        "sqshl  %p6/m %z21.b $0x07 -> %z21.b", "sqshl  %p7/m %z31.b $0x07 -> %z31.b",
    };
    TEST_LOOP(sqshl, sqshl_sve_pred, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_1_0[i], OPSZ_3b));

    static const uint imm3_1_1[6] = { 0, 4, 7, 10, 12, 15 };
    const char *const expected_1_1[6] = {
        "sqshl  %p0/m %z0.h $0x00 -> %z0.h",   "sqshl  %p2/m %z5.h $0x04 -> %z5.h",
        "sqshl  %p3/m %z10.h $0x07 -> %z10.h", "sqshl  %p5/m %z16.h $0x0a -> %z16.h",
        "sqshl  %p6/m %z21.h $0x0c -> %z21.h", "sqshl  %p7/m %z31.h $0x0f -> %z31.h",
    };
    TEST_LOOP(sqshl, sqshl_sve_pred, 6, expected_1_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_1_1[i], OPSZ_4b));

    static const uint imm3_1_2[6] = { 0, 7, 12, 18, 23, 31 };
    const char *const expected_1_2[6] = {
        "sqshl  %p0/m %z0.s $0x00 -> %z0.s",   "sqshl  %p2/m %z5.s $0x07 -> %z5.s",
        "sqshl  %p3/m %z10.s $0x0c -> %z10.s", "sqshl  %p5/m %z16.s $0x12 -> %z16.s",
        "sqshl  %p6/m %z21.s $0x17 -> %z21.s", "sqshl  %p7/m %z31.s $0x1f -> %z31.s",
    };
    TEST_LOOP(sqshl, sqshl_sve_pred, 6, expected_1_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_1_2[i], OPSZ_5b));

    static const uint imm3_1_3[6] = { 0, 12, 23, 34, 44, 63 };
    const char *const expected_1_3[6] = {
        "sqshl  %p0/m %z0.d $0x00 -> %z0.d",   "sqshl  %p2/m %z5.d $0x0c -> %z5.d",
        "sqshl  %p3/m %z10.d $0x17 -> %z10.d", "sqshl  %p5/m %z16.d $0x22 -> %z16.d",
        "sqshl  %p6/m %z21.d $0x2c -> %z21.d", "sqshl  %p7/m %z31.d $0x3f -> %z31.d",
    };
    TEST_LOOP(sqshl, sqshl_sve_pred, 6, expected_1_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_1_3[i], OPSZ_6b));
}

TEST_INSTR(sqshlr_sve_pred)
{

    /* Testing SQSHLR  <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "sqshlr %p0/m %z0.b %z0.b -> %z0.b",    "sqshlr %p2/m %z5.b %z7.b -> %z5.b",
        "sqshlr %p3/m %z10.b %z12.b -> %z10.b", "sqshlr %p5/m %z16.b %z18.b -> %z16.b",
        "sqshlr %p6/m %z21.b %z23.b -> %z21.b", "sqshlr %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(sqshlr, sqshlr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqshlr %p0/m %z0.h %z0.h -> %z0.h",    "sqshlr %p2/m %z5.h %z7.h -> %z5.h",
        "sqshlr %p3/m %z10.h %z12.h -> %z10.h", "sqshlr %p5/m %z16.h %z18.h -> %z16.h",
        "sqshlr %p6/m %z21.h %z23.h -> %z21.h", "sqshlr %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(sqshlr, sqshlr_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqshlr %p0/m %z0.s %z0.s -> %z0.s",    "sqshlr %p2/m %z5.s %z7.s -> %z5.s",
        "sqshlr %p3/m %z10.s %z12.s -> %z10.s", "sqshlr %p5/m %z16.s %z18.s -> %z16.s",
        "sqshlr %p6/m %z21.s %z23.s -> %z21.s", "sqshlr %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sqshlr, sqshlr_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "sqshlr %p0/m %z0.d %z0.d -> %z0.d",    "sqshlr %p2/m %z5.d %z7.d -> %z5.d",
        "sqshlr %p3/m %z10.d %z12.d -> %z10.d", "sqshlr %p5/m %z16.d %z18.d -> %z16.d",
        "sqshlr %p6/m %z21.d %z23.d -> %z21.d", "sqshlr %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sqshlr, sqshlr_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sqsubr_sve_pred)
{

    /* Testing SQSUBR  <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "sqsubr %p0/m %z0.b %z0.b -> %z0.b",    "sqsubr %p2/m %z5.b %z7.b -> %z5.b",
        "sqsubr %p3/m %z10.b %z12.b -> %z10.b", "sqsubr %p5/m %z16.b %z18.b -> %z16.b",
        "sqsubr %p6/m %z21.b %z23.b -> %z21.b", "sqsubr %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(sqsubr, sqsubr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqsubr %p0/m %z0.h %z0.h -> %z0.h",    "sqsubr %p2/m %z5.h %z7.h -> %z5.h",
        "sqsubr %p3/m %z10.h %z12.h -> %z10.h", "sqsubr %p5/m %z16.h %z18.h -> %z16.h",
        "sqsubr %p6/m %z21.h %z23.h -> %z21.h", "sqsubr %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(sqsubr, sqsubr_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqsubr %p0/m %z0.s %z0.s -> %z0.s",    "sqsubr %p2/m %z5.s %z7.s -> %z5.s",
        "sqsubr %p3/m %z10.s %z12.s -> %z10.s", "sqsubr %p5/m %z16.s %z18.s -> %z16.s",
        "sqsubr %p6/m %z21.s %z23.s -> %z21.s", "sqsubr %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sqsubr, sqsubr_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "sqsubr %p0/m %z0.d %z0.d -> %z0.d",    "sqsubr %p2/m %z5.d %z7.d -> %z5.d",
        "sqsubr %p3/m %z10.d %z12.d -> %z10.d", "sqsubr %p5/m %z16.d %z18.d -> %z16.d",
        "sqsubr %p6/m %z21.d %z23.d -> %z21.d", "sqsubr %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sqsubr, sqsubr_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(srhadd_sve_pred)
{

    /* Testing SRHADD  <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "srhadd %p0/m %z0.b %z0.b -> %z0.b",    "srhadd %p2/m %z5.b %z7.b -> %z5.b",
        "srhadd %p3/m %z10.b %z12.b -> %z10.b", "srhadd %p5/m %z16.b %z18.b -> %z16.b",
        "srhadd %p6/m %z21.b %z23.b -> %z21.b", "srhadd %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(srhadd, srhadd_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "srhadd %p0/m %z0.h %z0.h -> %z0.h",    "srhadd %p2/m %z5.h %z7.h -> %z5.h",
        "srhadd %p3/m %z10.h %z12.h -> %z10.h", "srhadd %p5/m %z16.h %z18.h -> %z16.h",
        "srhadd %p6/m %z21.h %z23.h -> %z21.h", "srhadd %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(srhadd, srhadd_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "srhadd %p0/m %z0.s %z0.s -> %z0.s",    "srhadd %p2/m %z5.s %z7.s -> %z5.s",
        "srhadd %p3/m %z10.s %z12.s -> %z10.s", "srhadd %p5/m %z16.s %z18.s -> %z16.s",
        "srhadd %p6/m %z21.s %z23.s -> %z21.s", "srhadd %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(srhadd, srhadd_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "srhadd %p0/m %z0.d %z0.d -> %z0.d",    "srhadd %p2/m %z5.d %z7.d -> %z5.d",
        "srhadd %p3/m %z10.d %z12.d -> %z10.d", "srhadd %p5/m %z16.d %z18.d -> %z16.d",
        "srhadd %p6/m %z21.d %z23.d -> %z21.d", "srhadd %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(srhadd, srhadd_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(srshl_sve_pred)
{

    /* Testing SRSHL   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "srshl  %p0/m %z0.b %z0.b -> %z0.b",    "srshl  %p2/m %z5.b %z7.b -> %z5.b",
        "srshl  %p3/m %z10.b %z12.b -> %z10.b", "srshl  %p5/m %z16.b %z18.b -> %z16.b",
        "srshl  %p6/m %z21.b %z23.b -> %z21.b", "srshl  %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(srshl, srshl_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "srshl  %p0/m %z0.h %z0.h -> %z0.h",    "srshl  %p2/m %z5.h %z7.h -> %z5.h",
        "srshl  %p3/m %z10.h %z12.h -> %z10.h", "srshl  %p5/m %z16.h %z18.h -> %z16.h",
        "srshl  %p6/m %z21.h %z23.h -> %z21.h", "srshl  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(srshl, srshl_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "srshl  %p0/m %z0.s %z0.s -> %z0.s",    "srshl  %p2/m %z5.s %z7.s -> %z5.s",
        "srshl  %p3/m %z10.s %z12.s -> %z10.s", "srshl  %p5/m %z16.s %z18.s -> %z16.s",
        "srshl  %p6/m %z21.s %z23.s -> %z21.s", "srshl  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(srshl, srshl_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "srshl  %p0/m %z0.d %z0.d -> %z0.d",    "srshl  %p2/m %z5.d %z7.d -> %z5.d",
        "srshl  %p3/m %z10.d %z12.d -> %z10.d", "srshl  %p5/m %z16.d %z18.d -> %z16.d",
        "srshl  %p6/m %z21.d %z23.d -> %z21.d", "srshl  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(srshl, srshl_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(srshlr_sve_pred)
{

    /* Testing SRSHLR  <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "srshlr %p0/m %z0.b %z0.b -> %z0.b",    "srshlr %p2/m %z5.b %z7.b -> %z5.b",
        "srshlr %p3/m %z10.b %z12.b -> %z10.b", "srshlr %p5/m %z16.b %z18.b -> %z16.b",
        "srshlr %p6/m %z21.b %z23.b -> %z21.b", "srshlr %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(srshlr, srshlr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "srshlr %p0/m %z0.h %z0.h -> %z0.h",    "srshlr %p2/m %z5.h %z7.h -> %z5.h",
        "srshlr %p3/m %z10.h %z12.h -> %z10.h", "srshlr %p5/m %z16.h %z18.h -> %z16.h",
        "srshlr %p6/m %z21.h %z23.h -> %z21.h", "srshlr %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(srshlr, srshlr_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "srshlr %p0/m %z0.s %z0.s -> %z0.s",    "srshlr %p2/m %z5.s %z7.s -> %z5.s",
        "srshlr %p3/m %z10.s %z12.s -> %z10.s", "srshlr %p5/m %z16.s %z18.s -> %z16.s",
        "srshlr %p6/m %z21.s %z23.s -> %z21.s", "srshlr %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(srshlr, srshlr_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "srshlr %p0/m %z0.d %z0.d -> %z0.d",    "srshlr %p2/m %z5.d %z7.d -> %z5.d",
        "srshlr %p3/m %z10.d %z12.d -> %z10.d", "srshlr %p5/m %z16.d %z18.d -> %z16.d",
        "srshlr %p6/m %z21.d %z23.d -> %z21.d", "srshlr %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(srshlr, srshlr_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(suqadd_sve_pred)
{

    /* Testing SUQADD  <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "suqadd %p0/m %z0.b %z0.b -> %z0.b",    "suqadd %p2/m %z5.b %z7.b -> %z5.b",
        "suqadd %p3/m %z10.b %z12.b -> %z10.b", "suqadd %p5/m %z16.b %z18.b -> %z16.b",
        "suqadd %p6/m %z21.b %z23.b -> %z21.b", "suqadd %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(suqadd, suqadd_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "suqadd %p0/m %z0.h %z0.h -> %z0.h",    "suqadd %p2/m %z5.h %z7.h -> %z5.h",
        "suqadd %p3/m %z10.h %z12.h -> %z10.h", "suqadd %p5/m %z16.h %z18.h -> %z16.h",
        "suqadd %p6/m %z21.h %z23.h -> %z21.h", "suqadd %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(suqadd, suqadd_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "suqadd %p0/m %z0.s %z0.s -> %z0.s",    "suqadd %p2/m %z5.s %z7.s -> %z5.s",
        "suqadd %p3/m %z10.s %z12.s -> %z10.s", "suqadd %p5/m %z16.s %z18.s -> %z16.s",
        "suqadd %p6/m %z21.s %z23.s -> %z21.s", "suqadd %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(suqadd, suqadd_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "suqadd %p0/m %z0.d %z0.d -> %z0.d",    "suqadd %p2/m %z5.d %z7.d -> %z5.d",
        "suqadd %p3/m %z10.d %z12.d -> %z10.d", "suqadd %p5/m %z16.d %z18.d -> %z16.d",
        "suqadd %p6/m %z21.d %z23.d -> %z21.d", "suqadd %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(suqadd, suqadd_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(uhadd_sve_pred)
{

    /* Testing UHADD   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "uhadd  %p0/m %z0.b %z0.b -> %z0.b",    "uhadd  %p2/m %z5.b %z7.b -> %z5.b",
        "uhadd  %p3/m %z10.b %z12.b -> %z10.b", "uhadd  %p5/m %z16.b %z18.b -> %z16.b",
        "uhadd  %p6/m %z21.b %z23.b -> %z21.b", "uhadd  %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(uhadd, uhadd_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uhadd  %p0/m %z0.h %z0.h -> %z0.h",    "uhadd  %p2/m %z5.h %z7.h -> %z5.h",
        "uhadd  %p3/m %z10.h %z12.h -> %z10.h", "uhadd  %p5/m %z16.h %z18.h -> %z16.h",
        "uhadd  %p6/m %z21.h %z23.h -> %z21.h", "uhadd  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(uhadd, uhadd_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uhadd  %p0/m %z0.s %z0.s -> %z0.s",    "uhadd  %p2/m %z5.s %z7.s -> %z5.s",
        "uhadd  %p3/m %z10.s %z12.s -> %z10.s", "uhadd  %p5/m %z16.s %z18.s -> %z16.s",
        "uhadd  %p6/m %z21.s %z23.s -> %z21.s", "uhadd  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(uhadd, uhadd_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "uhadd  %p0/m %z0.d %z0.d -> %z0.d",    "uhadd  %p2/m %z5.d %z7.d -> %z5.d",
        "uhadd  %p3/m %z10.d %z12.d -> %z10.d", "uhadd  %p5/m %z16.d %z18.d -> %z16.d",
        "uhadd  %p6/m %z21.d %z23.d -> %z21.d", "uhadd  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(uhadd, uhadd_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(uhsub_sve_pred)
{

    /* Testing UHSUB   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "uhsub  %p0/m %z0.b %z0.b -> %z0.b",    "uhsub  %p2/m %z5.b %z7.b -> %z5.b",
        "uhsub  %p3/m %z10.b %z12.b -> %z10.b", "uhsub  %p5/m %z16.b %z18.b -> %z16.b",
        "uhsub  %p6/m %z21.b %z23.b -> %z21.b", "uhsub  %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(uhsub, uhsub_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uhsub  %p0/m %z0.h %z0.h -> %z0.h",    "uhsub  %p2/m %z5.h %z7.h -> %z5.h",
        "uhsub  %p3/m %z10.h %z12.h -> %z10.h", "uhsub  %p5/m %z16.h %z18.h -> %z16.h",
        "uhsub  %p6/m %z21.h %z23.h -> %z21.h", "uhsub  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(uhsub, uhsub_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uhsub  %p0/m %z0.s %z0.s -> %z0.s",    "uhsub  %p2/m %z5.s %z7.s -> %z5.s",
        "uhsub  %p3/m %z10.s %z12.s -> %z10.s", "uhsub  %p5/m %z16.s %z18.s -> %z16.s",
        "uhsub  %p6/m %z21.s %z23.s -> %z21.s", "uhsub  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(uhsub, uhsub_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "uhsub  %p0/m %z0.d %z0.d -> %z0.d",    "uhsub  %p2/m %z5.d %z7.d -> %z5.d",
        "uhsub  %p3/m %z10.d %z12.d -> %z10.d", "uhsub  %p5/m %z16.d %z18.d -> %z16.d",
        "uhsub  %p6/m %z21.d %z23.d -> %z21.d", "uhsub  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(uhsub, uhsub_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(uhsubr_sve_pred)
{

    /* Testing UHSUBR  <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "uhsubr %p0/m %z0.b %z0.b -> %z0.b",    "uhsubr %p2/m %z5.b %z7.b -> %z5.b",
        "uhsubr %p3/m %z10.b %z12.b -> %z10.b", "uhsubr %p5/m %z16.b %z18.b -> %z16.b",
        "uhsubr %p6/m %z21.b %z23.b -> %z21.b", "uhsubr %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(uhsubr, uhsubr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uhsubr %p0/m %z0.h %z0.h -> %z0.h",    "uhsubr %p2/m %z5.h %z7.h -> %z5.h",
        "uhsubr %p3/m %z10.h %z12.h -> %z10.h", "uhsubr %p5/m %z16.h %z18.h -> %z16.h",
        "uhsubr %p6/m %z21.h %z23.h -> %z21.h", "uhsubr %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(uhsubr, uhsubr_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uhsubr %p0/m %z0.s %z0.s -> %z0.s",    "uhsubr %p2/m %z5.s %z7.s -> %z5.s",
        "uhsubr %p3/m %z10.s %z12.s -> %z10.s", "uhsubr %p5/m %z16.s %z18.s -> %z16.s",
        "uhsubr %p6/m %z21.s %z23.s -> %z21.s", "uhsubr %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(uhsubr, uhsubr_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "uhsubr %p0/m %z0.d %z0.d -> %z0.d",    "uhsubr %p2/m %z5.d %z7.d -> %z5.d",
        "uhsubr %p3/m %z10.d %z12.d -> %z10.d", "uhsubr %p5/m %z16.d %z18.d -> %z16.d",
        "uhsubr %p6/m %z21.d %z23.d -> %z21.d", "uhsubr %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(uhsubr, uhsubr_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(umaxp_sve_pred)
{

    /* Testing UMAXP   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "umaxp  %p0/m %z0.b %z0.b -> %z0.b",    "umaxp  %p2/m %z5.b %z7.b -> %z5.b",
        "umaxp  %p3/m %z10.b %z12.b -> %z10.b", "umaxp  %p5/m %z16.b %z18.b -> %z16.b",
        "umaxp  %p6/m %z21.b %z23.b -> %z21.b", "umaxp  %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(umaxp, umaxp_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "umaxp  %p0/m %z0.h %z0.h -> %z0.h",    "umaxp  %p2/m %z5.h %z7.h -> %z5.h",
        "umaxp  %p3/m %z10.h %z12.h -> %z10.h", "umaxp  %p5/m %z16.h %z18.h -> %z16.h",
        "umaxp  %p6/m %z21.h %z23.h -> %z21.h", "umaxp  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(umaxp, umaxp_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "umaxp  %p0/m %z0.s %z0.s -> %z0.s",    "umaxp  %p2/m %z5.s %z7.s -> %z5.s",
        "umaxp  %p3/m %z10.s %z12.s -> %z10.s", "umaxp  %p5/m %z16.s %z18.s -> %z16.s",
        "umaxp  %p6/m %z21.s %z23.s -> %z21.s", "umaxp  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(umaxp, umaxp_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "umaxp  %p0/m %z0.d %z0.d -> %z0.d",    "umaxp  %p2/m %z5.d %z7.d -> %z5.d",
        "umaxp  %p3/m %z10.d %z12.d -> %z10.d", "umaxp  %p5/m %z16.d %z18.d -> %z16.d",
        "umaxp  %p6/m %z21.d %z23.d -> %z21.d", "umaxp  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(umaxp, umaxp_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(uminp_sve_pred)
{

    /* Testing UMINP   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "uminp  %p0/m %z0.b %z0.b -> %z0.b",    "uminp  %p2/m %z5.b %z7.b -> %z5.b",
        "uminp  %p3/m %z10.b %z12.b -> %z10.b", "uminp  %p5/m %z16.b %z18.b -> %z16.b",
        "uminp  %p6/m %z21.b %z23.b -> %z21.b", "uminp  %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(uminp, uminp_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uminp  %p0/m %z0.h %z0.h -> %z0.h",    "uminp  %p2/m %z5.h %z7.h -> %z5.h",
        "uminp  %p3/m %z10.h %z12.h -> %z10.h", "uminp  %p5/m %z16.h %z18.h -> %z16.h",
        "uminp  %p6/m %z21.h %z23.h -> %z21.h", "uminp  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(uminp, uminp_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uminp  %p0/m %z0.s %z0.s -> %z0.s",    "uminp  %p2/m %z5.s %z7.s -> %z5.s",
        "uminp  %p3/m %z10.s %z12.s -> %z10.s", "uminp  %p5/m %z16.s %z18.s -> %z16.s",
        "uminp  %p6/m %z21.s %z23.s -> %z21.s", "uminp  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(uminp, uminp_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "uminp  %p0/m %z0.d %z0.d -> %z0.d",    "uminp  %p2/m %z5.d %z7.d -> %z5.d",
        "uminp  %p3/m %z10.d %z12.d -> %z10.d", "uminp  %p5/m %z16.d %z18.d -> %z16.d",
        "uminp  %p6/m %z21.d %z23.d -> %z21.d", "uminp  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(uminp, uminp_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(uqrshl_sve_pred)
{

    /* Testing UQRSHL  <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "uqrshl %p0/m %z0.b %z0.b -> %z0.b",    "uqrshl %p2/m %z5.b %z7.b -> %z5.b",
        "uqrshl %p3/m %z10.b %z12.b -> %z10.b", "uqrshl %p5/m %z16.b %z18.b -> %z16.b",
        "uqrshl %p6/m %z21.b %z23.b -> %z21.b", "uqrshl %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(uqrshl, uqrshl_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uqrshl %p0/m %z0.h %z0.h -> %z0.h",    "uqrshl %p2/m %z5.h %z7.h -> %z5.h",
        "uqrshl %p3/m %z10.h %z12.h -> %z10.h", "uqrshl %p5/m %z16.h %z18.h -> %z16.h",
        "uqrshl %p6/m %z21.h %z23.h -> %z21.h", "uqrshl %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(uqrshl, uqrshl_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uqrshl %p0/m %z0.s %z0.s -> %z0.s",    "uqrshl %p2/m %z5.s %z7.s -> %z5.s",
        "uqrshl %p3/m %z10.s %z12.s -> %z10.s", "uqrshl %p5/m %z16.s %z18.s -> %z16.s",
        "uqrshl %p6/m %z21.s %z23.s -> %z21.s", "uqrshl %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(uqrshl, uqrshl_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "uqrshl %p0/m %z0.d %z0.d -> %z0.d",    "uqrshl %p2/m %z5.d %z7.d -> %z5.d",
        "uqrshl %p3/m %z10.d %z12.d -> %z10.d", "uqrshl %p5/m %z16.d %z18.d -> %z16.d",
        "uqrshl %p6/m %z21.d %z23.d -> %z21.d", "uqrshl %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(uqrshl, uqrshl_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(uqrshlr_sve_pred)
{

    /* Testing UQRSHLR <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "uqrshlr %p0/m %z0.b %z0.b -> %z0.b",    "uqrshlr %p2/m %z5.b %z7.b -> %z5.b",
        "uqrshlr %p3/m %z10.b %z12.b -> %z10.b", "uqrshlr %p5/m %z16.b %z18.b -> %z16.b",
        "uqrshlr %p6/m %z21.b %z23.b -> %z21.b", "uqrshlr %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(uqrshlr, uqrshlr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uqrshlr %p0/m %z0.h %z0.h -> %z0.h",    "uqrshlr %p2/m %z5.h %z7.h -> %z5.h",
        "uqrshlr %p3/m %z10.h %z12.h -> %z10.h", "uqrshlr %p5/m %z16.h %z18.h -> %z16.h",
        "uqrshlr %p6/m %z21.h %z23.h -> %z21.h", "uqrshlr %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(uqrshlr, uqrshlr_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uqrshlr %p0/m %z0.s %z0.s -> %z0.s",    "uqrshlr %p2/m %z5.s %z7.s -> %z5.s",
        "uqrshlr %p3/m %z10.s %z12.s -> %z10.s", "uqrshlr %p5/m %z16.s %z18.s -> %z16.s",
        "uqrshlr %p6/m %z21.s %z23.s -> %z21.s", "uqrshlr %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(uqrshlr, uqrshlr_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "uqrshlr %p0/m %z0.d %z0.d -> %z0.d",    "uqrshlr %p2/m %z5.d %z7.d -> %z5.d",
        "uqrshlr %p3/m %z10.d %z12.d -> %z10.d", "uqrshlr %p5/m %z16.d %z18.d -> %z16.d",
        "uqrshlr %p6/m %z21.d %z23.d -> %z21.d", "uqrshlr %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(uqrshlr, uqrshlr_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(uqshl_sve_pred)
{

    /* Testing UQSHL   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "uqshl  %p0/m %z0.b %z0.b -> %z0.b",    "uqshl  %p2/m %z5.b %z7.b -> %z5.b",
        "uqshl  %p3/m %z10.b %z12.b -> %z10.b", "uqshl  %p5/m %z16.b %z18.b -> %z16.b",
        "uqshl  %p6/m %z21.b %z23.b -> %z21.b", "uqshl  %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(uqshl, uqshl_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uqshl  %p0/m %z0.h %z0.h -> %z0.h",    "uqshl  %p2/m %z5.h %z7.h -> %z5.h",
        "uqshl  %p3/m %z10.h %z12.h -> %z10.h", "uqshl  %p5/m %z16.h %z18.h -> %z16.h",
        "uqshl  %p6/m %z21.h %z23.h -> %z21.h", "uqshl  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(uqshl, uqshl_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uqshl  %p0/m %z0.s %z0.s -> %z0.s",    "uqshl  %p2/m %z5.s %z7.s -> %z5.s",
        "uqshl  %p3/m %z10.s %z12.s -> %z10.s", "uqshl  %p5/m %z16.s %z18.s -> %z16.s",
        "uqshl  %p6/m %z21.s %z23.s -> %z21.s", "uqshl  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(uqshl, uqshl_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "uqshl  %p0/m %z0.d %z0.d -> %z0.d",    "uqshl  %p2/m %z5.d %z7.d -> %z5.d",
        "uqshl  %p3/m %z10.d %z12.d -> %z10.d", "uqshl  %p5/m %z16.d %z18.d -> %z16.d",
        "uqshl  %p6/m %z21.d %z23.d -> %z21.d", "uqshl  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(uqshl, uqshl_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));

    /* Testing UQSHL   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, #<const> */
    static const uint imm3_1_0[6] = { 0, 3, 4, 6, 7, 7 };
    const char *const expected_1_0[6] = {
        "uqshl  %p0/m %z0.b $0x00 -> %z0.b",   "uqshl  %p2/m %z5.b $0x03 -> %z5.b",
        "uqshl  %p3/m %z10.b $0x04 -> %z10.b", "uqshl  %p5/m %z16.b $0x06 -> %z16.b",
        "uqshl  %p6/m %z21.b $0x07 -> %z21.b", "uqshl  %p7/m %z31.b $0x07 -> %z31.b",
    };
    TEST_LOOP(uqshl, uqshl_sve_pred, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_1_0[i], OPSZ_3b));

    static const uint imm3_1_1[6] = { 0, 4, 7, 10, 12, 15 };
    const char *const expected_1_1[6] = {
        "uqshl  %p0/m %z0.h $0x00 -> %z0.h",   "uqshl  %p2/m %z5.h $0x04 -> %z5.h",
        "uqshl  %p3/m %z10.h $0x07 -> %z10.h", "uqshl  %p5/m %z16.h $0x0a -> %z16.h",
        "uqshl  %p6/m %z21.h $0x0c -> %z21.h", "uqshl  %p7/m %z31.h $0x0f -> %z31.h",
    };
    TEST_LOOP(uqshl, uqshl_sve_pred, 6, expected_1_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_1_1[i], OPSZ_4b));

    static const uint imm3_1_2[6] = { 0, 7, 12, 18, 23, 31 };
    const char *const expected_1_2[6] = {
        "uqshl  %p0/m %z0.s $0x00 -> %z0.s",   "uqshl  %p2/m %z5.s $0x07 -> %z5.s",
        "uqshl  %p3/m %z10.s $0x0c -> %z10.s", "uqshl  %p5/m %z16.s $0x12 -> %z16.s",
        "uqshl  %p6/m %z21.s $0x17 -> %z21.s", "uqshl  %p7/m %z31.s $0x1f -> %z31.s",
    };
    TEST_LOOP(uqshl, uqshl_sve_pred, 6, expected_1_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_1_2[i], OPSZ_5b));

    static const uint imm3_1_3[6] = { 0, 12, 23, 34, 44, 63 };
    const char *const expected_1_3[6] = {
        "uqshl  %p0/m %z0.d $0x00 -> %z0.d",   "uqshl  %p2/m %z5.d $0x0c -> %z5.d",
        "uqshl  %p3/m %z10.d $0x17 -> %z10.d", "uqshl  %p5/m %z16.d $0x22 -> %z16.d",
        "uqshl  %p6/m %z21.d $0x2c -> %z21.d", "uqshl  %p7/m %z31.d $0x3f -> %z31.d",
    };
    TEST_LOOP(uqshl, uqshl_sve_pred, 6, expected_1_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_1_3[i], OPSZ_6b));
}

TEST_INSTR(uqshlr_sve_pred)
{

    /* Testing UQSHLR  <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "uqshlr %p0/m %z0.b %z0.b -> %z0.b",    "uqshlr %p2/m %z5.b %z7.b -> %z5.b",
        "uqshlr %p3/m %z10.b %z12.b -> %z10.b", "uqshlr %p5/m %z16.b %z18.b -> %z16.b",
        "uqshlr %p6/m %z21.b %z23.b -> %z21.b", "uqshlr %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(uqshlr, uqshlr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uqshlr %p0/m %z0.h %z0.h -> %z0.h",    "uqshlr %p2/m %z5.h %z7.h -> %z5.h",
        "uqshlr %p3/m %z10.h %z12.h -> %z10.h", "uqshlr %p5/m %z16.h %z18.h -> %z16.h",
        "uqshlr %p6/m %z21.h %z23.h -> %z21.h", "uqshlr %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(uqshlr, uqshlr_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uqshlr %p0/m %z0.s %z0.s -> %z0.s",    "uqshlr %p2/m %z5.s %z7.s -> %z5.s",
        "uqshlr %p3/m %z10.s %z12.s -> %z10.s", "uqshlr %p5/m %z16.s %z18.s -> %z16.s",
        "uqshlr %p6/m %z21.s %z23.s -> %z21.s", "uqshlr %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(uqshlr, uqshlr_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "uqshlr %p0/m %z0.d %z0.d -> %z0.d",    "uqshlr %p2/m %z5.d %z7.d -> %z5.d",
        "uqshlr %p3/m %z10.d %z12.d -> %z10.d", "uqshlr %p5/m %z16.d %z18.d -> %z16.d",
        "uqshlr %p6/m %z21.d %z23.d -> %z21.d", "uqshlr %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(uqshlr, uqshlr_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(uqsubr_sve_pred)
{

    /* Testing UQSUBR  <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "uqsubr %p0/m %z0.b %z0.b -> %z0.b",    "uqsubr %p2/m %z5.b %z7.b -> %z5.b",
        "uqsubr %p3/m %z10.b %z12.b -> %z10.b", "uqsubr %p5/m %z16.b %z18.b -> %z16.b",
        "uqsubr %p6/m %z21.b %z23.b -> %z21.b", "uqsubr %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(uqsubr, uqsubr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uqsubr %p0/m %z0.h %z0.h -> %z0.h",    "uqsubr %p2/m %z5.h %z7.h -> %z5.h",
        "uqsubr %p3/m %z10.h %z12.h -> %z10.h", "uqsubr %p5/m %z16.h %z18.h -> %z16.h",
        "uqsubr %p6/m %z21.h %z23.h -> %z21.h", "uqsubr %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(uqsubr, uqsubr_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uqsubr %p0/m %z0.s %z0.s -> %z0.s",    "uqsubr %p2/m %z5.s %z7.s -> %z5.s",
        "uqsubr %p3/m %z10.s %z12.s -> %z10.s", "uqsubr %p5/m %z16.s %z18.s -> %z16.s",
        "uqsubr %p6/m %z21.s %z23.s -> %z21.s", "uqsubr %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(uqsubr, uqsubr_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "uqsubr %p0/m %z0.d %z0.d -> %z0.d",    "uqsubr %p2/m %z5.d %z7.d -> %z5.d",
        "uqsubr %p3/m %z10.d %z12.d -> %z10.d", "uqsubr %p5/m %z16.d %z18.d -> %z16.d",
        "uqsubr %p6/m %z21.d %z23.d -> %z21.d", "uqsubr %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(uqsubr, uqsubr_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(urhadd_sve_pred)
{

    /* Testing URHADD  <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "urhadd %p0/m %z0.b %z0.b -> %z0.b",    "urhadd %p2/m %z5.b %z7.b -> %z5.b",
        "urhadd %p3/m %z10.b %z12.b -> %z10.b", "urhadd %p5/m %z16.b %z18.b -> %z16.b",
        "urhadd %p6/m %z21.b %z23.b -> %z21.b", "urhadd %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(urhadd, urhadd_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "urhadd %p0/m %z0.h %z0.h -> %z0.h",    "urhadd %p2/m %z5.h %z7.h -> %z5.h",
        "urhadd %p3/m %z10.h %z12.h -> %z10.h", "urhadd %p5/m %z16.h %z18.h -> %z16.h",
        "urhadd %p6/m %z21.h %z23.h -> %z21.h", "urhadd %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(urhadd, urhadd_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "urhadd %p0/m %z0.s %z0.s -> %z0.s",    "urhadd %p2/m %z5.s %z7.s -> %z5.s",
        "urhadd %p3/m %z10.s %z12.s -> %z10.s", "urhadd %p5/m %z16.s %z18.s -> %z16.s",
        "urhadd %p6/m %z21.s %z23.s -> %z21.s", "urhadd %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(urhadd, urhadd_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "urhadd %p0/m %z0.d %z0.d -> %z0.d",    "urhadd %p2/m %z5.d %z7.d -> %z5.d",
        "urhadd %p3/m %z10.d %z12.d -> %z10.d", "urhadd %p5/m %z16.d %z18.d -> %z16.d",
        "urhadd %p6/m %z21.d %z23.d -> %z21.d", "urhadd %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(urhadd, urhadd_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(urshl_sve_pred)
{

    /* Testing URSHL   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "urshl  %p0/m %z0.b %z0.b -> %z0.b",    "urshl  %p2/m %z5.b %z7.b -> %z5.b",
        "urshl  %p3/m %z10.b %z12.b -> %z10.b", "urshl  %p5/m %z16.b %z18.b -> %z16.b",
        "urshl  %p6/m %z21.b %z23.b -> %z21.b", "urshl  %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(urshl, urshl_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "urshl  %p0/m %z0.h %z0.h -> %z0.h",    "urshl  %p2/m %z5.h %z7.h -> %z5.h",
        "urshl  %p3/m %z10.h %z12.h -> %z10.h", "urshl  %p5/m %z16.h %z18.h -> %z16.h",
        "urshl  %p6/m %z21.h %z23.h -> %z21.h", "urshl  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(urshl, urshl_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "urshl  %p0/m %z0.s %z0.s -> %z0.s",    "urshl  %p2/m %z5.s %z7.s -> %z5.s",
        "urshl  %p3/m %z10.s %z12.s -> %z10.s", "urshl  %p5/m %z16.s %z18.s -> %z16.s",
        "urshl  %p6/m %z21.s %z23.s -> %z21.s", "urshl  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(urshl, urshl_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "urshl  %p0/m %z0.d %z0.d -> %z0.d",    "urshl  %p2/m %z5.d %z7.d -> %z5.d",
        "urshl  %p3/m %z10.d %z12.d -> %z10.d", "urshl  %p5/m %z16.d %z18.d -> %z16.d",
        "urshl  %p6/m %z21.d %z23.d -> %z21.d", "urshl  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(urshl, urshl_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(urshlr_sve_pred)
{

    /* Testing URSHLR  <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "urshlr %p0/m %z0.b %z0.b -> %z0.b",    "urshlr %p2/m %z5.b %z7.b -> %z5.b",
        "urshlr %p3/m %z10.b %z12.b -> %z10.b", "urshlr %p5/m %z16.b %z18.b -> %z16.b",
        "urshlr %p6/m %z21.b %z23.b -> %z21.b", "urshlr %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(urshlr, urshlr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "urshlr %p0/m %z0.h %z0.h -> %z0.h",    "urshlr %p2/m %z5.h %z7.h -> %z5.h",
        "urshlr %p3/m %z10.h %z12.h -> %z10.h", "urshlr %p5/m %z16.h %z18.h -> %z16.h",
        "urshlr %p6/m %z21.h %z23.h -> %z21.h", "urshlr %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(urshlr, urshlr_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "urshlr %p0/m %z0.s %z0.s -> %z0.s",    "urshlr %p2/m %z5.s %z7.s -> %z5.s",
        "urshlr %p3/m %z10.s %z12.s -> %z10.s", "urshlr %p5/m %z16.s %z18.s -> %z16.s",
        "urshlr %p6/m %z21.s %z23.s -> %z21.s", "urshlr %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(urshlr, urshlr_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "urshlr %p0/m %z0.d %z0.d -> %z0.d",    "urshlr %p2/m %z5.d %z7.d -> %z5.d",
        "urshlr %p3/m %z10.d %z12.d -> %z10.d", "urshlr %p5/m %z16.d %z18.d -> %z16.d",
        "urshlr %p6/m %z21.d %z23.d -> %z21.d", "urshlr %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(urshlr, urshlr_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(usqadd_sve_pred)
{

    /* Testing USQADD  <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "usqadd %p0/m %z0.b %z0.b -> %z0.b",    "usqadd %p2/m %z5.b %z7.b -> %z5.b",
        "usqadd %p3/m %z10.b %z12.b -> %z10.b", "usqadd %p5/m %z16.b %z18.b -> %z16.b",
        "usqadd %p6/m %z21.b %z23.b -> %z21.b", "usqadd %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(usqadd, usqadd_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "usqadd %p0/m %z0.h %z0.h -> %z0.h",    "usqadd %p2/m %z5.h %z7.h -> %z5.h",
        "usqadd %p3/m %z10.h %z12.h -> %z10.h", "usqadd %p5/m %z16.h %z18.h -> %z16.h",
        "usqadd %p6/m %z21.h %z23.h -> %z21.h", "usqadd %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(usqadd, usqadd_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "usqadd %p0/m %z0.s %z0.s -> %z0.s",    "usqadd %p2/m %z5.s %z7.s -> %z5.s",
        "usqadd %p3/m %z10.s %z12.s -> %z10.s", "usqadd %p5/m %z16.s %z18.s -> %z16.s",
        "usqadd %p6/m %z21.s %z23.s -> %z21.s", "usqadd %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(usqadd, usqadd_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "usqadd %p0/m %z0.d %z0.d -> %z0.d",    "usqadd %p2/m %z5.d %z7.d -> %z5.d",
        "usqadd %p3/m %z10.d %z12.d -> %z10.d", "usqadd %p5/m %z16.d %z18.d -> %z16.d",
        "usqadd %p6/m %z21.d %z23.d -> %z21.d", "usqadd %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(usqadd, usqadd_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(fcvtlt_sve_pred)
{

    /* Testing FCVTLT  <Zd>.S, <Pg>/M, <Zn>.H */
    const char *const expected_0_0[6] = {
        "fcvtlt %p0/m %z0.h -> %z0.s",   "fcvtlt %p2/m %z7.h -> %z5.s",
        "fcvtlt %p3/m %z12.h -> %z10.s", "fcvtlt %p5/m %z18.h -> %z16.s",
        "fcvtlt %p6/m %z23.h -> %z21.s", "fcvtlt %p7/m %z31.h -> %z31.s",
    };
    TEST_LOOP(fcvtlt, fcvtlt_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    /* Testing FCVTLT  <Zd>.D, <Pg>/M, <Zn>.S */
    const char *const expected_1_0[6] = {
        "fcvtlt %p0/m %z0.s -> %z0.d",   "fcvtlt %p2/m %z7.s -> %z5.d",
        "fcvtlt %p3/m %z12.s -> %z10.d", "fcvtlt %p5/m %z18.s -> %z16.d",
        "fcvtlt %p6/m %z23.s -> %z21.d", "fcvtlt %p7/m %z31.s -> %z31.d",
    };
    TEST_LOOP(fcvtlt, fcvtlt_sve_pred, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(fcvtnt_sve_pred)
{

    /* Testing FCVTNT  <Zd>.S, <Pg>/M, <Zn>.D */
    const char *const expected_0_0[6] = {
        "fcvtnt %z0.s %p0/m %z0.d -> %z0.s",    "fcvtnt %z5.s %p2/m %z7.d -> %z5.s",
        "fcvtnt %z10.s %p3/m %z12.d -> %z10.s", "fcvtnt %z16.s %p5/m %z18.d -> %z16.s",
        "fcvtnt %z21.s %p6/m %z23.d -> %z21.s", "fcvtnt %z31.s %p7/m %z31.d -> %z31.s",
    };
    TEST_LOOP(fcvtnt, fcvtnt_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));

    /* Testing FCVTNT  <Zd>.H, <Pg>/M, <Zn>.S */
    const char *const expected_1_0[6] = {
        "fcvtnt %z0.h %p0/m %z0.s -> %z0.h",    "fcvtnt %z5.h %p2/m %z7.s -> %z5.h",
        "fcvtnt %z10.h %p3/m %z12.s -> %z10.h", "fcvtnt %z16.h %p5/m %z18.s -> %z16.h",
        "fcvtnt %z21.h %p6/m %z23.s -> %z21.h", "fcvtnt %z31.h %p7/m %z31.s -> %z31.h",
    };
    TEST_LOOP(fcvtnt, fcvtnt_sve_pred, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(fcvtx_sve_pred)
{

    /* Testing FCVTX   <Zd>.S, <Pg>/M, <Zn>.D */
    const char *const expected_0_0[6] = {
        "fcvtx  %p0/m %z0.d -> %z0.s",   "fcvtx  %p2/m %z7.d -> %z5.s",
        "fcvtx  %p3/m %z12.d -> %z10.s", "fcvtx  %p5/m %z18.d -> %z16.s",
        "fcvtx  %p6/m %z23.d -> %z21.s", "fcvtx  %p7/m %z31.d -> %z31.s",
    };
    TEST_LOOP(fcvtx, fcvtx_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(fcvtxnt_sve_pred)
{

    /* Testing FCVTXNT <Zd>.S, <Pg>/M, <Zn>.D */
    const char *const expected_0_0[6] = {
        "fcvtxnt %z0.s %p0/m %z0.d -> %z0.s",    "fcvtxnt %z5.s %p2/m %z7.d -> %z5.s",
        "fcvtxnt %z10.s %p3/m %z12.d -> %z10.s", "fcvtxnt %z16.s %p5/m %z18.d -> %z16.s",
        "fcvtxnt %z21.s %p6/m %z23.d -> %z21.s", "fcvtxnt %z31.s %p7/m %z31.d -> %z31.s",
    };
    TEST_LOOP(fcvtxnt, fcvtxnt_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(flogb_sve_pred)
{

    /* Testing FLOGB   <Zd>.<Ts>, <Pg>/M, <Zn>.<Ts> */
    const char *const expected_0_0[6] = {
        "flogb  %p0/m %z0.h -> %z0.h",   "flogb  %p2/m %z7.h -> %z5.h",
        "flogb  %p3/m %z12.h -> %z10.h", "flogb  %p5/m %z18.h -> %z16.h",
        "flogb  %p6/m %z23.h -> %z21.h", "flogb  %p7/m %z31.h -> %z31.h",
    };
    TEST_LOOP(flogb, flogb_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_1[6] = {
        "flogb  %p0/m %z0.s -> %z0.s",   "flogb  %p2/m %z7.s -> %z5.s",
        "flogb  %p3/m %z12.s -> %z10.s", "flogb  %p5/m %z18.s -> %z16.s",
        "flogb  %p6/m %z23.s -> %z21.s", "flogb  %p7/m %z31.s -> %z31.s",
    };
    TEST_LOOP(flogb, flogb_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_2[6] = {
        "flogb  %p0/m %z0.d -> %z0.d",   "flogb  %p2/m %z7.d -> %z5.d",
        "flogb  %p3/m %z12.d -> %z10.d", "flogb  %p5/m %z18.d -> %z16.d",
        "flogb  %p6/m %z23.d -> %z21.d", "flogb  %p7/m %z31.d -> %z31.d",
    };
    TEST_LOOP(flogb, flogb_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sadalp_sve_pred)
{

    /* Testing SADALP  <Zda>.<Ts>, <Pg>/M, <Zn>.<Tb> */
    const char *const expected_0_0[6] = {
        "sadalp %z0.h %p0/m %z0.b -> %z0.h",    "sadalp %z5.h %p2/m %z7.b -> %z5.h",
        "sadalp %z10.h %p3/m %z12.b -> %z10.h", "sadalp %z16.h %p5/m %z18.b -> %z16.h",
        "sadalp %z21.h %p6/m %z23.b -> %z21.h", "sadalp %z31.h %p7/m %z31.b -> %z31.h",
    };
    TEST_LOOP(sadalp, sadalp_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sadalp %z0.s %p0/m %z0.h -> %z0.s",    "sadalp %z5.s %p2/m %z7.h -> %z5.s",
        "sadalp %z10.s %p3/m %z12.h -> %z10.s", "sadalp %z16.s %p5/m %z18.h -> %z16.s",
        "sadalp %z21.s %p6/m %z23.h -> %z21.s", "sadalp %z31.s %p7/m %z31.h -> %z31.s",
    };
    TEST_LOOP(sadalp, sadalp_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sadalp %z0.d %p0/m %z0.s -> %z0.d",    "sadalp %z5.d %p2/m %z7.s -> %z5.d",
        "sadalp %z10.d %p3/m %z12.s -> %z10.d", "sadalp %z16.d %p5/m %z18.s -> %z16.d",
        "sadalp %z21.d %p6/m %z23.s -> %z21.d", "sadalp %z31.d %p7/m %z31.s -> %z31.d",
    };
    TEST_LOOP(sadalp, sadalp_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(sqabs_sve_pred)
{

    /* Testing SQABS   <Zd>.<Ts>, <Pg>/M, <Zn>.<Ts> */
    const char *const expected_0_0[6] = {
        "sqabs  %p0/m %z0.b -> %z0.b",   "sqabs  %p2/m %z7.b -> %z5.b",
        "sqabs  %p3/m %z12.b -> %z10.b", "sqabs  %p5/m %z18.b -> %z16.b",
        "sqabs  %p6/m %z23.b -> %z21.b", "sqabs  %p7/m %z31.b -> %z31.b",
    };
    TEST_LOOP(sqabs, sqabs_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqabs  %p0/m %z0.h -> %z0.h",   "sqabs  %p2/m %z7.h -> %z5.h",
        "sqabs  %p3/m %z12.h -> %z10.h", "sqabs  %p5/m %z18.h -> %z16.h",
        "sqabs  %p6/m %z23.h -> %z21.h", "sqabs  %p7/m %z31.h -> %z31.h",
    };
    TEST_LOOP(sqabs, sqabs_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqabs  %p0/m %z0.s -> %z0.s",   "sqabs  %p2/m %z7.s -> %z5.s",
        "sqabs  %p3/m %z12.s -> %z10.s", "sqabs  %p5/m %z18.s -> %z16.s",
        "sqabs  %p6/m %z23.s -> %z21.s", "sqabs  %p7/m %z31.s -> %z31.s",
    };
    TEST_LOOP(sqabs, sqabs_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "sqabs  %p0/m %z0.d -> %z0.d",   "sqabs  %p2/m %z7.d -> %z5.d",
        "sqabs  %p3/m %z12.d -> %z10.d", "sqabs  %p5/m %z18.d -> %z16.d",
        "sqabs  %p6/m %z23.d -> %z21.d", "sqabs  %p7/m %z31.d -> %z31.d",
    };
    TEST_LOOP(sqabs, sqabs_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sqneg_sve_pred)
{

    /* Testing SQNEG   <Zd>.<Ts>, <Pg>/M, <Zn>.<Ts> */
    const char *const expected_0_0[6] = {
        "sqneg  %p0/m %z0.b -> %z0.b",   "sqneg  %p2/m %z7.b -> %z5.b",
        "sqneg  %p3/m %z12.b -> %z10.b", "sqneg  %p5/m %z18.b -> %z16.b",
        "sqneg  %p6/m %z23.b -> %z21.b", "sqneg  %p7/m %z31.b -> %z31.b",
    };
    TEST_LOOP(sqneg, sqneg_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "sqneg  %p0/m %z0.h -> %z0.h",   "sqneg  %p2/m %z7.h -> %z5.h",
        "sqneg  %p3/m %z12.h -> %z10.h", "sqneg  %p5/m %z18.h -> %z16.h",
        "sqneg  %p6/m %z23.h -> %z21.h", "sqneg  %p7/m %z31.h -> %z31.h",
    };
    TEST_LOOP(sqneg, sqneg_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "sqneg  %p0/m %z0.s -> %z0.s",   "sqneg  %p2/m %z7.s -> %z5.s",
        "sqneg  %p3/m %z12.s -> %z10.s", "sqneg  %p5/m %z18.s -> %z16.s",
        "sqneg  %p6/m %z23.s -> %z21.s", "sqneg  %p7/m %z31.s -> %z31.s",
    };
    TEST_LOOP(sqneg, sqneg_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *const expected_0_3[6] = {
        "sqneg  %p0/m %z0.d -> %z0.d",   "sqneg  %p2/m %z7.d -> %z5.d",
        "sqneg  %p3/m %z12.d -> %z10.d", "sqneg  %p5/m %z18.d -> %z16.d",
        "sqneg  %p6/m %z23.d -> %z21.d", "sqneg  %p7/m %z31.d -> %z31.d",
    };
    TEST_LOOP(sqneg, sqneg_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(uadalp_sve_pred)
{

    /* Testing UADALP  <Zda>.<Ts>, <Pg>/M, <Zn>.<Tb> */
    const char *const expected_0_0[6] = {
        "uadalp %z0.h %p0/m %z0.b -> %z0.h",    "uadalp %z5.h %p2/m %z7.b -> %z5.h",
        "uadalp %z10.h %p3/m %z12.b -> %z10.h", "uadalp %z16.h %p5/m %z18.b -> %z16.h",
        "uadalp %z21.h %p6/m %z23.b -> %z21.h", "uadalp %z31.h %p7/m %z31.b -> %z31.h",
    };
    TEST_LOOP(uadalp, uadalp_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "uadalp %z0.s %p0/m %z0.h -> %z0.s",    "uadalp %z5.s %p2/m %z7.h -> %z5.s",
        "uadalp %z10.s %p3/m %z12.h -> %z10.s", "uadalp %z16.s %p5/m %z18.h -> %z16.s",
        "uadalp %z21.s %p6/m %z23.h -> %z21.s", "uadalp %z31.s %p7/m %z31.h -> %z31.s",
    };
    TEST_LOOP(uadalp, uadalp_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *const expected_0_2[6] = {
        "uadalp %z0.d %p0/m %z0.s -> %z0.d",    "uadalp %z5.d %p2/m %z7.s -> %z5.d",
        "uadalp %z10.d %p3/m %z12.s -> %z10.d", "uadalp %z16.d %p5/m %z18.s -> %z16.d",
        "uadalp %z21.d %p6/m %z23.s -> %z21.d", "uadalp %z31.d %p7/m %z31.s -> %z31.d",
    };
    TEST_LOOP(uadalp, uadalp_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(cadd_sve)
{

    /* Testing CADD    <Zdn>.<Ts>, <Zdn>.<Ts>, <Zm>.<Ts>, <const> */
    static const uint rot_0_0[6] = { 90, 90, 90, 90, 270, 270 };
    const char *const expected_0_0[6] = {
        "cadd   %z0.b %z0.b $0x005a -> %z0.b",
        "cadd   %z5.b %z6.b $0x005a -> %z5.b",
        "cadd   %z10.b %z11.b $0x005a -> %z10.b",
        "cadd   %z16.b %z17.b $0x005a -> %z16.b",
        "cadd   %z21.b %z22.b $0x010e -> %z21.b",
        "cadd   %z31.b %z31.b $0x010e -> %z31.b",
    };
    TEST_LOOP(cadd, cadd_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_immed_uint(rot_0_0[i], OPSZ_2));

    static const uint rot_0_1[6] = { 90, 90, 90, 90, 270, 270 };
    const char *const expected_0_1[6] = {
        "cadd   %z0.h %z0.h $0x005a -> %z0.h",
        "cadd   %z5.h %z6.h $0x005a -> %z5.h",
        "cadd   %z10.h %z11.h $0x005a -> %z10.h",
        "cadd   %z16.h %z17.h $0x005a -> %z16.h",
        "cadd   %z21.h %z22.h $0x010e -> %z21.h",
        "cadd   %z31.h %z31.h $0x010e -> %z31.h",
    };
    TEST_LOOP(cadd, cadd_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(rot_0_1[i], OPSZ_2));

    static const uint rot_0_2[6] = { 90, 90, 90, 90, 270, 270 };
    const char *const expected_0_2[6] = {
        "cadd   %z0.s %z0.s $0x005a -> %z0.s",
        "cadd   %z5.s %z6.s $0x005a -> %z5.s",
        "cadd   %z10.s %z11.s $0x005a -> %z10.s",
        "cadd   %z16.s %z17.s $0x005a -> %z16.s",
        "cadd   %z21.s %z22.s $0x010e -> %z21.s",
        "cadd   %z31.s %z31.s $0x010e -> %z31.s",
    };
    TEST_LOOP(cadd, cadd_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(rot_0_2[i], OPSZ_2));

    static const uint rot_0_3[6] = { 90, 90, 90, 90, 270, 270 };
    const char *const expected_0_3[6] = {
        "cadd   %z0.d %z0.d $0x005a -> %z0.d",
        "cadd   %z5.d %z6.d $0x005a -> %z5.d",
        "cadd   %z10.d %z11.d $0x005a -> %z10.d",
        "cadd   %z16.d %z17.d $0x005a -> %z16.d",
        "cadd   %z21.d %z22.d $0x010e -> %z21.d",
        "cadd   %z31.d %z31.d $0x010e -> %z31.d",
    };
    TEST_LOOP(cadd, cadd_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(rot_0_3[i], OPSZ_2));
}

TEST_INSTR(cdot_sve)
{

    /* Testing CDOT    <Zda>.<Ts>, <Zn>.<Tb>, <Zm>.<Tb>, <const> */
    static const uint rot_0_0[6] = { 0, 270, 0, 90, 90, 270 };
    const char *const expected_0_0[6] = {
        "cdot   %z0.s %z0.b %z0.b $0x0000 -> %z0.s",
        "cdot   %z5.s %z6.b %z7.b $0x010e -> %z5.s",
        "cdot   %z10.s %z11.b %z12.b $0x0000 -> %z10.s",
        "cdot   %z16.s %z17.b %z18.b $0x005a -> %z16.s",
        "cdot   %z21.s %z22.b %z23.b $0x005a -> %z21.s",
        "cdot   %z31.s %z31.b %z31.b $0x010e -> %z31.s",
    };
    TEST_LOOP(cdot, cdot_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_immed_uint(rot_0_0[i], OPSZ_2));

    static const uint rot_0_1[6] = { 0, 270, 0, 90, 90, 270 };
    const char *const expected_0_1[6] = {
        "cdot   %z0.d %z0.h %z0.h $0x0000 -> %z0.d",
        "cdot   %z5.d %z6.h %z7.h $0x010e -> %z5.d",
        "cdot   %z10.d %z11.h %z12.h $0x0000 -> %z10.d",
        "cdot   %z16.d %z17.h %z18.h $0x005a -> %z16.d",
        "cdot   %z21.d %z22.h %z23.h $0x005a -> %z21.d",
        "cdot   %z31.d %z31.h %z31.h $0x010e -> %z31.d",
    };
    TEST_LOOP(cdot, cdot_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_immed_uint(rot_0_1[i], OPSZ_2));
}

TEST_INSTR(cmla_sve)
{

    /* Testing CMLA    <Zda>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts>, <const> */
    static const uint rot_0_0[6] = { 0, 270, 0, 90, 90, 270 };
    const char *const expected_0_0[6] = {
        "cmla   %z0.b %z0.b %z0.b $0x0000 -> %z0.b",
        "cmla   %z5.b %z6.b %z7.b $0x010e -> %z5.b",
        "cmla   %z10.b %z11.b %z12.b $0x0000 -> %z10.b",
        "cmla   %z16.b %z17.b %z18.b $0x005a -> %z16.b",
        "cmla   %z21.b %z22.b %z23.b $0x005a -> %z21.b",
        "cmla   %z31.b %z31.b %z31.b $0x010e -> %z31.b",
    };
    TEST_LOOP(cmla, cmla_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_immed_uint(rot_0_0[i], OPSZ_2));

    static const uint rot_0_1[6] = { 0, 270, 0, 90, 90, 270 };
    const char *const expected_0_1[6] = {
        "cmla   %z0.h %z0.h %z0.h $0x0000 -> %z0.h",
        "cmla   %z5.h %z6.h %z7.h $0x010e -> %z5.h",
        "cmla   %z10.h %z11.h %z12.h $0x0000 -> %z10.h",
        "cmla   %z16.h %z17.h %z18.h $0x005a -> %z16.h",
        "cmla   %z21.h %z22.h %z23.h $0x005a -> %z21.h",
        "cmla   %z31.h %z31.h %z31.h $0x010e -> %z31.h",
    };
    TEST_LOOP(cmla, cmla_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_immed_uint(rot_0_1[i], OPSZ_2));

    static const uint rot_0_2[6] = { 0, 270, 0, 90, 90, 270 };
    const char *const expected_0_2[6] = {
        "cmla   %z0.s %z0.s %z0.s $0x0000 -> %z0.s",
        "cmla   %z5.s %z6.s %z7.s $0x010e -> %z5.s",
        "cmla   %z10.s %z11.s %z12.s $0x0000 -> %z10.s",
        "cmla   %z16.s %z17.s %z18.s $0x005a -> %z16.s",
        "cmla   %z21.s %z22.s %z23.s $0x005a -> %z21.s",
        "cmla   %z31.s %z31.s %z31.s $0x010e -> %z31.s",
    };
    TEST_LOOP(cmla, cmla_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_immed_uint(rot_0_2[i], OPSZ_2));

    static const uint rot_0_3[6] = { 0, 270, 0, 90, 90, 270 };
    const char *const expected_0_3[6] = {
        "cmla   %z0.d %z0.d %z0.d $0x0000 -> %z0.d",
        "cmla   %z5.d %z6.d %z7.d $0x010e -> %z5.d",
        "cmla   %z10.d %z11.d %z12.d $0x0000 -> %z10.d",
        "cmla   %z16.d %z17.d %z18.d $0x005a -> %z16.d",
        "cmla   %z21.d %z22.d %z23.d $0x005a -> %z21.d",
        "cmla   %z31.d %z31.d %z31.d $0x010e -> %z31.d",
    };
    TEST_LOOP(cmla, cmla_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_immed_uint(rot_0_3[i], OPSZ_2));
}

TEST_INSTR(rshrnb_sve)
{

    /* Testing RSHRNB  <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "rshrnb %z0.h $0x01 -> %z0.b",   "rshrnb %z6.h $0x04 -> %z5.b",
        "rshrnb %z11.h $0x05 -> %z10.b", "rshrnb %z17.h $0x07 -> %z16.b",
        "rshrnb %z22.h $0x08 -> %z21.b", "rshrnb %z31.h $0x08 -> %z31.b",
    };
    TEST_LOOP(rshrnb, rshrnb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "rshrnb %z0.s $0x01 -> %z0.h",   "rshrnb %z6.s $0x05 -> %z5.h",
        "rshrnb %z11.s $0x08 -> %z10.h", "rshrnb %z17.s $0x0b -> %z16.h",
        "rshrnb %z22.s $0x0d -> %z21.h", "rshrnb %z31.s $0x10 -> %z31.h",
    };
    TEST_LOOP(rshrnb, rshrnb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "rshrnb %z0.d $0x01 -> %z0.s",   "rshrnb %z6.d $0x08 -> %z5.s",
        "rshrnb %z11.d $0x0d -> %z10.s", "rshrnb %z17.d $0x13 -> %z16.s",
        "rshrnb %z22.d $0x18 -> %z21.s", "rshrnb %z31.d $0x20 -> %z31.s",
    };
    TEST_LOOP(rshrnb, rshrnb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(rshrnt_sve)
{

    /* Testing RSHRNT  <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "rshrnt %z0.b %z0.h $0x01 -> %z0.b",    "rshrnt %z5.b %z6.h $0x04 -> %z5.b",
        "rshrnt %z10.b %z11.h $0x05 -> %z10.b", "rshrnt %z16.b %z17.h $0x07 -> %z16.b",
        "rshrnt %z21.b %z22.h $0x08 -> %z21.b", "rshrnt %z31.b %z31.h $0x08 -> %z31.b",
    };
    TEST_LOOP(rshrnt, rshrnt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "rshrnt %z0.h %z0.s $0x01 -> %z0.h",    "rshrnt %z5.h %z6.s $0x05 -> %z5.h",
        "rshrnt %z10.h %z11.s $0x08 -> %z10.h", "rshrnt %z16.h %z17.s $0x0b -> %z16.h",
        "rshrnt %z21.h %z22.s $0x0d -> %z21.h", "rshrnt %z31.h %z31.s $0x10 -> %z31.h",
    };
    TEST_LOOP(rshrnt, rshrnt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "rshrnt %z0.s %z0.d $0x01 -> %z0.s",    "rshrnt %z5.s %z6.d $0x08 -> %z5.s",
        "rshrnt %z10.s %z11.d $0x0d -> %z10.s", "rshrnt %z16.s %z17.d $0x13 -> %z16.s",
        "rshrnt %z21.s %z22.d $0x18 -> %z21.s", "rshrnt %z31.s %z31.d $0x20 -> %z31.s",
    };
    TEST_LOOP(rshrnt, rshrnt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(shrnb_sve)
{

    /* Testing SHRNB   <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "shrnb  %z0.h $0x01 -> %z0.b",   "shrnb  %z6.h $0x04 -> %z5.b",
        "shrnb  %z11.h $0x05 -> %z10.b", "shrnb  %z17.h $0x07 -> %z16.b",
        "shrnb  %z22.h $0x08 -> %z21.b", "shrnb  %z31.h $0x08 -> %z31.b",
    };
    TEST_LOOP(shrnb, shrnb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "shrnb  %z0.s $0x01 -> %z0.h",   "shrnb  %z6.s $0x05 -> %z5.h",
        "shrnb  %z11.s $0x08 -> %z10.h", "shrnb  %z17.s $0x0b -> %z16.h",
        "shrnb  %z22.s $0x0d -> %z21.h", "shrnb  %z31.s $0x10 -> %z31.h",
    };
    TEST_LOOP(shrnb, shrnb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "shrnb  %z0.d $0x01 -> %z0.s",   "shrnb  %z6.d $0x08 -> %z5.s",
        "shrnb  %z11.d $0x0d -> %z10.s", "shrnb  %z17.d $0x13 -> %z16.s",
        "shrnb  %z22.d $0x18 -> %z21.s", "shrnb  %z31.d $0x20 -> %z31.s",
    };
    TEST_LOOP(shrnb, shrnb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(shrnt_sve)
{

    /* Testing SHRNT   <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "shrnt  %z0.b %z0.h $0x01 -> %z0.b",    "shrnt  %z5.b %z6.h $0x04 -> %z5.b",
        "shrnt  %z10.b %z11.h $0x05 -> %z10.b", "shrnt  %z16.b %z17.h $0x07 -> %z16.b",
        "shrnt  %z21.b %z22.h $0x08 -> %z21.b", "shrnt  %z31.b %z31.h $0x08 -> %z31.b",
    };
    TEST_LOOP(shrnt, shrnt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "shrnt  %z0.h %z0.s $0x01 -> %z0.h",    "shrnt  %z5.h %z6.s $0x05 -> %z5.h",
        "shrnt  %z10.h %z11.s $0x08 -> %z10.h", "shrnt  %z16.h %z17.s $0x0b -> %z16.h",
        "shrnt  %z21.h %z22.s $0x0d -> %z21.h", "shrnt  %z31.h %z31.s $0x10 -> %z31.h",
    };
    TEST_LOOP(shrnt, shrnt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "shrnt  %z0.s %z0.d $0x01 -> %z0.s",    "shrnt  %z5.s %z6.d $0x08 -> %z5.s",
        "shrnt  %z10.s %z11.d $0x0d -> %z10.s", "shrnt  %z16.s %z17.d $0x13 -> %z16.s",
        "shrnt  %z21.s %z22.d $0x18 -> %z21.s", "shrnt  %z31.s %z31.d $0x20 -> %z31.s",
    };
    TEST_LOOP(shrnt, shrnt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(sli_sve)
{

    /* Testing SLI     <Zd>.<Ts>, <Zn>.<Ts>, #<const> */
    static const uint imm3_0_0[6] = { 0, 3, 4, 6, 7, 7 };
    const char *const expected_0_0[6] = {
        "sli    %z0.b %z0.b $0x00 -> %z0.b",    "sli    %z5.b %z6.b $0x03 -> %z5.b",
        "sli    %z10.b %z11.b $0x04 -> %z10.b", "sli    %z16.b %z17.b $0x06 -> %z16.b",
        "sli    %z21.b %z22.b $0x07 -> %z21.b", "sli    %z31.b %z31.b $0x07 -> %z31.b",
    };
    TEST_LOOP(sli, sli_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 0, 4, 7, 10, 12, 15 };
    const char *const expected_0_1[6] = {
        "sli    %z0.h %z0.h $0x00 -> %z0.h",    "sli    %z5.h %z6.h $0x04 -> %z5.h",
        "sli    %z10.h %z11.h $0x07 -> %z10.h", "sli    %z16.h %z17.h $0x0a -> %z16.h",
        "sli    %z21.h %z22.h $0x0c -> %z21.h", "sli    %z31.h %z31.h $0x0f -> %z31.h",
    };
    TEST_LOOP(sli, sli_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 0, 7, 12, 18, 23, 31 };
    const char *const expected_0_2[6] = {
        "sli    %z0.s %z0.s $0x00 -> %z0.s",    "sli    %z5.s %z6.s $0x07 -> %z5.s",
        "sli    %z10.s %z11.s $0x0c -> %z10.s", "sli    %z16.s %z17.s $0x12 -> %z16.s",
        "sli    %z21.s %z22.s $0x17 -> %z21.s", "sli    %z31.s %z31.s $0x1f -> %z31.s",
    };
    TEST_LOOP(sli, sli_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));

    static const uint imm3_0_3[6] = { 0, 12, 23, 34, 44, 63 };
    const char *const expected_0_3[6] = {
        "sli    %z0.d %z0.d $0x00 -> %z0.d",    "sli    %z5.d %z6.d $0x0c -> %z5.d",
        "sli    %z10.d %z11.d $0x17 -> %z10.d", "sli    %z16.d %z17.d $0x22 -> %z16.d",
        "sli    %z21.d %z22.d $0x2c -> %z21.d", "sli    %z31.d %z31.d $0x3f -> %z31.d",
    };
    TEST_LOOP(sli, sli_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_3[i], OPSZ_6b));
}

TEST_INSTR(sqcadd_sve)
{

    /* Testing SQCADD  <Zdn>.<Ts>, <Zdn>.<Ts>, <Zm>.<Ts>, <const> */
    static const uint rot_0_0[6] = { 90, 90, 90, 90, 270, 270 };
    const char *const expected_0_0[6] = {
        "sqcadd %z0.b %z0.b $0x005a -> %z0.b",
        "sqcadd %z5.b %z6.b $0x005a -> %z5.b",
        "sqcadd %z10.b %z11.b $0x005a -> %z10.b",
        "sqcadd %z16.b %z17.b $0x005a -> %z16.b",
        "sqcadd %z21.b %z22.b $0x010e -> %z21.b",
        "sqcadd %z31.b %z31.b $0x010e -> %z31.b",
    };
    TEST_LOOP(sqcadd, sqcadd_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_immed_uint(rot_0_0[i], OPSZ_2));

    static const uint rot_0_1[6] = { 90, 90, 90, 90, 270, 270 };
    const char *const expected_0_1[6] = {
        "sqcadd %z0.h %z0.h $0x005a -> %z0.h",
        "sqcadd %z5.h %z6.h $0x005a -> %z5.h",
        "sqcadd %z10.h %z11.h $0x005a -> %z10.h",
        "sqcadd %z16.h %z17.h $0x005a -> %z16.h",
        "sqcadd %z21.h %z22.h $0x010e -> %z21.h",
        "sqcadd %z31.h %z31.h $0x010e -> %z31.h",
    };
    TEST_LOOP(sqcadd, sqcadd_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(rot_0_1[i], OPSZ_2));

    static const uint rot_0_2[6] = { 90, 90, 90, 90, 270, 270 };
    const char *const expected_0_2[6] = {
        "sqcadd %z0.s %z0.s $0x005a -> %z0.s",
        "sqcadd %z5.s %z6.s $0x005a -> %z5.s",
        "sqcadd %z10.s %z11.s $0x005a -> %z10.s",
        "sqcadd %z16.s %z17.s $0x005a -> %z16.s",
        "sqcadd %z21.s %z22.s $0x010e -> %z21.s",
        "sqcadd %z31.s %z31.s $0x010e -> %z31.s",
    };
    TEST_LOOP(sqcadd, sqcadd_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(rot_0_2[i], OPSZ_2));

    static const uint rot_0_3[6] = { 90, 90, 90, 90, 270, 270 };
    const char *const expected_0_3[6] = {
        "sqcadd %z0.d %z0.d $0x005a -> %z0.d",
        "sqcadd %z5.d %z6.d $0x005a -> %z5.d",
        "sqcadd %z10.d %z11.d $0x005a -> %z10.d",
        "sqcadd %z16.d %z17.d $0x005a -> %z16.d",
        "sqcadd %z21.d %z22.d $0x010e -> %z21.d",
        "sqcadd %z31.d %z31.d $0x010e -> %z31.d",
    };
    TEST_LOOP(sqcadd, sqcadd_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(rot_0_3[i], OPSZ_2));
}

TEST_INSTR(sqrdcmlah_sve)
{

    /* Testing SQRDCMLAH <Zda>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts>, <const> */
    static const uint rot_0_0[6] = { 0, 270, 0, 90, 90, 270 };
    const char *const expected_0_0[6] = {
        "sqrdcmlah %z0.b %z0.b %z0.b $0x0000 -> %z0.b",
        "sqrdcmlah %z5.b %z6.b %z7.b $0x010e -> %z5.b",
        "sqrdcmlah %z10.b %z11.b %z12.b $0x0000 -> %z10.b",
        "sqrdcmlah %z16.b %z17.b %z18.b $0x005a -> %z16.b",
        "sqrdcmlah %z21.b %z22.b %z23.b $0x005a -> %z21.b",
        "sqrdcmlah %z31.b %z31.b %z31.b $0x010e -> %z31.b",
    };
    TEST_LOOP(sqrdcmlah, sqrdcmlah_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_immed_uint(rot_0_0[i], OPSZ_2));

    static const uint rot_0_1[6] = { 0, 270, 0, 90, 90, 270 };
    const char *const expected_0_1[6] = {
        "sqrdcmlah %z0.h %z0.h %z0.h $0x0000 -> %z0.h",
        "sqrdcmlah %z5.h %z6.h %z7.h $0x010e -> %z5.h",
        "sqrdcmlah %z10.h %z11.h %z12.h $0x0000 -> %z10.h",
        "sqrdcmlah %z16.h %z17.h %z18.h $0x005a -> %z16.h",
        "sqrdcmlah %z21.h %z22.h %z23.h $0x005a -> %z21.h",
        "sqrdcmlah %z31.h %z31.h %z31.h $0x010e -> %z31.h",
    };
    TEST_LOOP(sqrdcmlah, sqrdcmlah_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_immed_uint(rot_0_1[i], OPSZ_2));

    static const uint rot_0_2[6] = { 0, 270, 0, 90, 90, 270 };
    const char *const expected_0_2[6] = {
        "sqrdcmlah %z0.s %z0.s %z0.s $0x0000 -> %z0.s",
        "sqrdcmlah %z5.s %z6.s %z7.s $0x010e -> %z5.s",
        "sqrdcmlah %z10.s %z11.s %z12.s $0x0000 -> %z10.s",
        "sqrdcmlah %z16.s %z17.s %z18.s $0x005a -> %z16.s",
        "sqrdcmlah %z21.s %z22.s %z23.s $0x005a -> %z21.s",
        "sqrdcmlah %z31.s %z31.s %z31.s $0x010e -> %z31.s",
    };
    TEST_LOOP(sqrdcmlah, sqrdcmlah_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_immed_uint(rot_0_2[i], OPSZ_2));

    static const uint rot_0_3[6] = { 0, 270, 0, 90, 90, 270 };
    const char *const expected_0_3[6] = {
        "sqrdcmlah %z0.d %z0.d %z0.d $0x0000 -> %z0.d",
        "sqrdcmlah %z5.d %z6.d %z7.d $0x010e -> %z5.d",
        "sqrdcmlah %z10.d %z11.d %z12.d $0x0000 -> %z10.d",
        "sqrdcmlah %z16.d %z17.d %z18.d $0x005a -> %z16.d",
        "sqrdcmlah %z21.d %z22.d %z23.d $0x005a -> %z21.d",
        "sqrdcmlah %z31.d %z31.d %z31.d $0x010e -> %z31.d",
    };
    TEST_LOOP(sqrdcmlah, sqrdcmlah_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_immed_uint(rot_0_3[i], OPSZ_2));
}

TEST_INSTR(sqrshrnb_sve)
{

    /* Testing SQRSHRNB <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "sqrshrnb %z0.h $0x01 -> %z0.b",   "sqrshrnb %z6.h $0x04 -> %z5.b",
        "sqrshrnb %z11.h $0x05 -> %z10.b", "sqrshrnb %z17.h $0x07 -> %z16.b",
        "sqrshrnb %z22.h $0x08 -> %z21.b", "sqrshrnb %z31.h $0x08 -> %z31.b",
    };
    TEST_LOOP(sqrshrnb, sqrshrnb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "sqrshrnb %z0.s $0x01 -> %z0.h",   "sqrshrnb %z6.s $0x05 -> %z5.h",
        "sqrshrnb %z11.s $0x08 -> %z10.h", "sqrshrnb %z17.s $0x0b -> %z16.h",
        "sqrshrnb %z22.s $0x0d -> %z21.h", "sqrshrnb %z31.s $0x10 -> %z31.h",
    };
    TEST_LOOP(sqrshrnb, sqrshrnb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "sqrshrnb %z0.d $0x01 -> %z0.s",   "sqrshrnb %z6.d $0x08 -> %z5.s",
        "sqrshrnb %z11.d $0x0d -> %z10.s", "sqrshrnb %z17.d $0x13 -> %z16.s",
        "sqrshrnb %z22.d $0x18 -> %z21.s", "sqrshrnb %z31.d $0x20 -> %z31.s",
    };
    TEST_LOOP(sqrshrnb, sqrshrnb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(sqrshrnt_sve)
{

    /* Testing SQRSHRNT <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "sqrshrnt %z0.b %z0.h $0x01 -> %z0.b",
        "sqrshrnt %z5.b %z6.h $0x04 -> %z5.b",
        "sqrshrnt %z10.b %z11.h $0x05 -> %z10.b",
        "sqrshrnt %z16.b %z17.h $0x07 -> %z16.b",
        "sqrshrnt %z21.b %z22.h $0x08 -> %z21.b",
        "sqrshrnt %z31.b %z31.h $0x08 -> %z31.b",
    };
    TEST_LOOP(sqrshrnt, sqrshrnt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "sqrshrnt %z0.h %z0.s $0x01 -> %z0.h",
        "sqrshrnt %z5.h %z6.s $0x05 -> %z5.h",
        "sqrshrnt %z10.h %z11.s $0x08 -> %z10.h",
        "sqrshrnt %z16.h %z17.s $0x0b -> %z16.h",
        "sqrshrnt %z21.h %z22.s $0x0d -> %z21.h",
        "sqrshrnt %z31.h %z31.s $0x10 -> %z31.h",
    };
    TEST_LOOP(sqrshrnt, sqrshrnt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "sqrshrnt %z0.s %z0.d $0x01 -> %z0.s",
        "sqrshrnt %z5.s %z6.d $0x08 -> %z5.s",
        "sqrshrnt %z10.s %z11.d $0x0d -> %z10.s",
        "sqrshrnt %z16.s %z17.d $0x13 -> %z16.s",
        "sqrshrnt %z21.s %z22.d $0x18 -> %z21.s",
        "sqrshrnt %z31.s %z31.d $0x20 -> %z31.s",
    };
    TEST_LOOP(sqrshrnt, sqrshrnt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(sqrshrunb_sve)
{

    /* Testing SQRSHRUNB <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "sqrshrunb %z0.h $0x01 -> %z0.b",   "sqrshrunb %z6.h $0x04 -> %z5.b",
        "sqrshrunb %z11.h $0x05 -> %z10.b", "sqrshrunb %z17.h $0x07 -> %z16.b",
        "sqrshrunb %z22.h $0x08 -> %z21.b", "sqrshrunb %z31.h $0x08 -> %z31.b",
    };
    TEST_LOOP(sqrshrunb, sqrshrunb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "sqrshrunb %z0.s $0x01 -> %z0.h",   "sqrshrunb %z6.s $0x05 -> %z5.h",
        "sqrshrunb %z11.s $0x08 -> %z10.h", "sqrshrunb %z17.s $0x0b -> %z16.h",
        "sqrshrunb %z22.s $0x0d -> %z21.h", "sqrshrunb %z31.s $0x10 -> %z31.h",
    };
    TEST_LOOP(sqrshrunb, sqrshrunb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "sqrshrunb %z0.d $0x01 -> %z0.s",   "sqrshrunb %z6.d $0x08 -> %z5.s",
        "sqrshrunb %z11.d $0x0d -> %z10.s", "sqrshrunb %z17.d $0x13 -> %z16.s",
        "sqrshrunb %z22.d $0x18 -> %z21.s", "sqrshrunb %z31.d $0x20 -> %z31.s",
    };
    TEST_LOOP(sqrshrunb, sqrshrunb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(sqrshrunt_sve)
{

    /* Testing SQRSHRUNT <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "sqrshrunt %z0.b %z0.h $0x01 -> %z0.b",
        "sqrshrunt %z5.b %z6.h $0x04 -> %z5.b",
        "sqrshrunt %z10.b %z11.h $0x05 -> %z10.b",
        "sqrshrunt %z16.b %z17.h $0x07 -> %z16.b",
        "sqrshrunt %z21.b %z22.h $0x08 -> %z21.b",
        "sqrshrunt %z31.b %z31.h $0x08 -> %z31.b",
    };
    TEST_LOOP(sqrshrunt, sqrshrunt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "sqrshrunt %z0.h %z0.s $0x01 -> %z0.h",
        "sqrshrunt %z5.h %z6.s $0x05 -> %z5.h",
        "sqrshrunt %z10.h %z11.s $0x08 -> %z10.h",
        "sqrshrunt %z16.h %z17.s $0x0b -> %z16.h",
        "sqrshrunt %z21.h %z22.s $0x0d -> %z21.h",
        "sqrshrunt %z31.h %z31.s $0x10 -> %z31.h",
    };
    TEST_LOOP(sqrshrunt, sqrshrunt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "sqrshrunt %z0.s %z0.d $0x01 -> %z0.s",
        "sqrshrunt %z5.s %z6.d $0x08 -> %z5.s",
        "sqrshrunt %z10.s %z11.d $0x0d -> %z10.s",
        "sqrshrunt %z16.s %z17.d $0x13 -> %z16.s",
        "sqrshrunt %z21.s %z22.d $0x18 -> %z21.s",
        "sqrshrunt %z31.s %z31.d $0x20 -> %z31.s",
    };
    TEST_LOOP(sqrshrunt, sqrshrunt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(sqshlu_sve_pred)
{

    /* Testing SQSHLU  <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, #<const> */
    static const uint imm3_0_0[6] = { 0, 3, 4, 6, 7, 7 };
    const char *const expected_0_0[6] = {
        "sqshlu %p0/m %z0.b $0x00 -> %z0.b",   "sqshlu %p2/m %z5.b $0x03 -> %z5.b",
        "sqshlu %p3/m %z10.b $0x04 -> %z10.b", "sqshlu %p5/m %z16.b $0x06 -> %z16.b",
        "sqshlu %p6/m %z21.b $0x07 -> %z21.b", "sqshlu %p7/m %z31.b $0x07 -> %z31.b",
    };
    TEST_LOOP(sqshlu, sqshlu_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 0, 4, 7, 10, 12, 15 };
    const char *const expected_0_1[6] = {
        "sqshlu %p0/m %z0.h $0x00 -> %z0.h",   "sqshlu %p2/m %z5.h $0x04 -> %z5.h",
        "sqshlu %p3/m %z10.h $0x07 -> %z10.h", "sqshlu %p5/m %z16.h $0x0a -> %z16.h",
        "sqshlu %p6/m %z21.h $0x0c -> %z21.h", "sqshlu %p7/m %z31.h $0x0f -> %z31.h",
    };
    TEST_LOOP(sqshlu, sqshlu_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 0, 7, 12, 18, 23, 31 };
    const char *const expected_0_2[6] = {
        "sqshlu %p0/m %z0.s $0x00 -> %z0.s",   "sqshlu %p2/m %z5.s $0x07 -> %z5.s",
        "sqshlu %p3/m %z10.s $0x0c -> %z10.s", "sqshlu %p5/m %z16.s $0x12 -> %z16.s",
        "sqshlu %p6/m %z21.s $0x17 -> %z21.s", "sqshlu %p7/m %z31.s $0x1f -> %z31.s",
    };
    TEST_LOOP(sqshlu, sqshlu_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));

    static const uint imm3_0_3[6] = { 0, 12, 23, 34, 44, 63 };
    const char *const expected_0_3[6] = {
        "sqshlu %p0/m %z0.d $0x00 -> %z0.d",   "sqshlu %p2/m %z5.d $0x0c -> %z5.d",
        "sqshlu %p3/m %z10.d $0x17 -> %z10.d", "sqshlu %p5/m %z16.d $0x22 -> %z16.d",
        "sqshlu %p6/m %z21.d $0x2c -> %z21.d", "sqshlu %p7/m %z31.d $0x3f -> %z31.d",
    };
    TEST_LOOP(sqshlu, sqshlu_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_0_3[i], OPSZ_6b));
}

TEST_INSTR(sqshrnb_sve)
{

    /* Testing SQSHRNB <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "sqshrnb %z0.h $0x01 -> %z0.b",   "sqshrnb %z6.h $0x04 -> %z5.b",
        "sqshrnb %z11.h $0x05 -> %z10.b", "sqshrnb %z17.h $0x07 -> %z16.b",
        "sqshrnb %z22.h $0x08 -> %z21.b", "sqshrnb %z31.h $0x08 -> %z31.b",
    };
    TEST_LOOP(sqshrnb, sqshrnb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "sqshrnb %z0.s $0x01 -> %z0.h",   "sqshrnb %z6.s $0x05 -> %z5.h",
        "sqshrnb %z11.s $0x08 -> %z10.h", "sqshrnb %z17.s $0x0b -> %z16.h",
        "sqshrnb %z22.s $0x0d -> %z21.h", "sqshrnb %z31.s $0x10 -> %z31.h",
    };
    TEST_LOOP(sqshrnb, sqshrnb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "sqshrnb %z0.d $0x01 -> %z0.s",   "sqshrnb %z6.d $0x08 -> %z5.s",
        "sqshrnb %z11.d $0x0d -> %z10.s", "sqshrnb %z17.d $0x13 -> %z16.s",
        "sqshrnb %z22.d $0x18 -> %z21.s", "sqshrnb %z31.d $0x20 -> %z31.s",
    };
    TEST_LOOP(sqshrnb, sqshrnb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(sqshrnt_sve)
{

    /* Testing SQSHRNT <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "sqshrnt %z0.b %z0.h $0x01 -> %z0.b",    "sqshrnt %z5.b %z6.h $0x04 -> %z5.b",
        "sqshrnt %z10.b %z11.h $0x05 -> %z10.b", "sqshrnt %z16.b %z17.h $0x07 -> %z16.b",
        "sqshrnt %z21.b %z22.h $0x08 -> %z21.b", "sqshrnt %z31.b %z31.h $0x08 -> %z31.b",
    };
    TEST_LOOP(sqshrnt, sqshrnt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "sqshrnt %z0.h %z0.s $0x01 -> %z0.h",    "sqshrnt %z5.h %z6.s $0x05 -> %z5.h",
        "sqshrnt %z10.h %z11.s $0x08 -> %z10.h", "sqshrnt %z16.h %z17.s $0x0b -> %z16.h",
        "sqshrnt %z21.h %z22.s $0x0d -> %z21.h", "sqshrnt %z31.h %z31.s $0x10 -> %z31.h",
    };
    TEST_LOOP(sqshrnt, sqshrnt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "sqshrnt %z0.s %z0.d $0x01 -> %z0.s",    "sqshrnt %z5.s %z6.d $0x08 -> %z5.s",
        "sqshrnt %z10.s %z11.d $0x0d -> %z10.s", "sqshrnt %z16.s %z17.d $0x13 -> %z16.s",
        "sqshrnt %z21.s %z22.d $0x18 -> %z21.s", "sqshrnt %z31.s %z31.d $0x20 -> %z31.s",
    };
    TEST_LOOP(sqshrnt, sqshrnt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(sqshrunb_sve)
{

    /* Testing SQSHRUNB <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "sqshrunb %z0.h $0x01 -> %z0.b",   "sqshrunb %z6.h $0x04 -> %z5.b",
        "sqshrunb %z11.h $0x05 -> %z10.b", "sqshrunb %z17.h $0x07 -> %z16.b",
        "sqshrunb %z22.h $0x08 -> %z21.b", "sqshrunb %z31.h $0x08 -> %z31.b",
    };
    TEST_LOOP(sqshrunb, sqshrunb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "sqshrunb %z0.s $0x01 -> %z0.h",   "sqshrunb %z6.s $0x05 -> %z5.h",
        "sqshrunb %z11.s $0x08 -> %z10.h", "sqshrunb %z17.s $0x0b -> %z16.h",
        "sqshrunb %z22.s $0x0d -> %z21.h", "sqshrunb %z31.s $0x10 -> %z31.h",
    };
    TEST_LOOP(sqshrunb, sqshrunb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "sqshrunb %z0.d $0x01 -> %z0.s",   "sqshrunb %z6.d $0x08 -> %z5.s",
        "sqshrunb %z11.d $0x0d -> %z10.s", "sqshrunb %z17.d $0x13 -> %z16.s",
        "sqshrunb %z22.d $0x18 -> %z21.s", "sqshrunb %z31.d $0x20 -> %z31.s",
    };
    TEST_LOOP(sqshrunb, sqshrunb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(sqshrunt_sve)
{

    /* Testing SQSHRUNT <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "sqshrunt %z0.b %z0.h $0x01 -> %z0.b",
        "sqshrunt %z5.b %z6.h $0x04 -> %z5.b",
        "sqshrunt %z10.b %z11.h $0x05 -> %z10.b",
        "sqshrunt %z16.b %z17.h $0x07 -> %z16.b",
        "sqshrunt %z21.b %z22.h $0x08 -> %z21.b",
        "sqshrunt %z31.b %z31.h $0x08 -> %z31.b",
    };
    TEST_LOOP(sqshrunt, sqshrunt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "sqshrunt %z0.h %z0.s $0x01 -> %z0.h",
        "sqshrunt %z5.h %z6.s $0x05 -> %z5.h",
        "sqshrunt %z10.h %z11.s $0x08 -> %z10.h",
        "sqshrunt %z16.h %z17.s $0x0b -> %z16.h",
        "sqshrunt %z21.h %z22.s $0x0d -> %z21.h",
        "sqshrunt %z31.h %z31.s $0x10 -> %z31.h",
    };
    TEST_LOOP(sqshrunt, sqshrunt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "sqshrunt %z0.s %z0.d $0x01 -> %z0.s",
        "sqshrunt %z5.s %z6.d $0x08 -> %z5.s",
        "sqshrunt %z10.s %z11.d $0x0d -> %z10.s",
        "sqshrunt %z16.s %z17.d $0x13 -> %z16.s",
        "sqshrunt %z21.s %z22.d $0x18 -> %z21.s",
        "sqshrunt %z31.s %z31.d $0x20 -> %z31.s",
    };
    TEST_LOOP(sqshrunt, sqshrunt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(sri_sve)
{

    /* Testing SRI     <Zd>.<Ts>, <Zn>.<Ts>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "sri    %z0.b %z0.b $0x01 -> %z0.b",    "sri    %z5.b %z6.b $0x04 -> %z5.b",
        "sri    %z10.b %z11.b $0x05 -> %z10.b", "sri    %z16.b %z17.b $0x07 -> %z16.b",
        "sri    %z21.b %z22.b $0x08 -> %z21.b", "sri    %z31.b %z31.b $0x08 -> %z31.b",
    };
    TEST_LOOP(sri, sri_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "sri    %z0.h %z0.h $0x01 -> %z0.h",    "sri    %z5.h %z6.h $0x05 -> %z5.h",
        "sri    %z10.h %z11.h $0x08 -> %z10.h", "sri    %z16.h %z17.h $0x0b -> %z16.h",
        "sri    %z21.h %z22.h $0x0d -> %z21.h", "sri    %z31.h %z31.h $0x10 -> %z31.h",
    };
    TEST_LOOP(sri, sri_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "sri    %z0.s %z0.s $0x01 -> %z0.s",    "sri    %z5.s %z6.s $0x08 -> %z5.s",
        "sri    %z10.s %z11.s $0x0d -> %z10.s", "sri    %z16.s %z17.s $0x13 -> %z16.s",
        "sri    %z21.s %z22.s $0x18 -> %z21.s", "sri    %z31.s %z31.s $0x20 -> %z31.s",
    };
    TEST_LOOP(sri, sri_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));

    static const uint imm3_0_3[6] = { 1, 13, 24, 35, 45, 64 };
    const char *const expected_0_3[6] = {
        "sri    %z0.d %z0.d $0x01 -> %z0.d",    "sri    %z5.d %z6.d $0x0d -> %z5.d",
        "sri    %z10.d %z11.d $0x18 -> %z10.d", "sri    %z16.d %z17.d $0x23 -> %z16.d",
        "sri    %z21.d %z22.d $0x2d -> %z21.d", "sri    %z31.d %z31.d $0x40 -> %z31.d",
    };
    TEST_LOOP(sri, sri_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_3[i], OPSZ_6b));
}

TEST_INSTR(srshr_sve_pred)
{

    /* Testing SRSHR   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "srshr  %p0/m %z0.b $0x01 -> %z0.b",   "srshr  %p2/m %z5.b $0x04 -> %z5.b",
        "srshr  %p3/m %z10.b $0x05 -> %z10.b", "srshr  %p5/m %z16.b $0x07 -> %z16.b",
        "srshr  %p6/m %z21.b $0x08 -> %z21.b", "srshr  %p7/m %z31.b $0x08 -> %z31.b",
    };
    TEST_LOOP(srshr, srshr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "srshr  %p0/m %z0.h $0x01 -> %z0.h",   "srshr  %p2/m %z5.h $0x05 -> %z5.h",
        "srshr  %p3/m %z10.h $0x08 -> %z10.h", "srshr  %p5/m %z16.h $0x0b -> %z16.h",
        "srshr  %p6/m %z21.h $0x0d -> %z21.h", "srshr  %p7/m %z31.h $0x10 -> %z31.h",
    };
    TEST_LOOP(srshr, srshr_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "srshr  %p0/m %z0.s $0x01 -> %z0.s",   "srshr  %p2/m %z5.s $0x08 -> %z5.s",
        "srshr  %p3/m %z10.s $0x0d -> %z10.s", "srshr  %p5/m %z16.s $0x13 -> %z16.s",
        "srshr  %p6/m %z21.s $0x18 -> %z21.s", "srshr  %p7/m %z31.s $0x20 -> %z31.s",
    };
    TEST_LOOP(srshr, srshr_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));

    static const uint imm3_0_3[6] = { 1, 13, 24, 35, 45, 64 };
    const char *const expected_0_3[6] = {
        "srshr  %p0/m %z0.d $0x01 -> %z0.d",   "srshr  %p2/m %z5.d $0x0d -> %z5.d",
        "srshr  %p3/m %z10.d $0x18 -> %z10.d", "srshr  %p5/m %z16.d $0x23 -> %z16.d",
        "srshr  %p6/m %z21.d $0x2d -> %z21.d", "srshr  %p7/m %z31.d $0x40 -> %z31.d",
    };
    TEST_LOOP(srshr, srshr_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_0_3[i], OPSZ_6b));
}

TEST_INSTR(srsra_sve)
{

    /* Testing SRSRA   <Zda>.<Ts>, <Zn>.<Ts>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "srsra  %z0.b %z0.b $0x01 -> %z0.b",    "srsra  %z5.b %z6.b $0x04 -> %z5.b",
        "srsra  %z10.b %z11.b $0x05 -> %z10.b", "srsra  %z16.b %z17.b $0x07 -> %z16.b",
        "srsra  %z21.b %z22.b $0x08 -> %z21.b", "srsra  %z31.b %z31.b $0x08 -> %z31.b",
    };
    TEST_LOOP(srsra, srsra_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "srsra  %z0.h %z0.h $0x01 -> %z0.h",    "srsra  %z5.h %z6.h $0x05 -> %z5.h",
        "srsra  %z10.h %z11.h $0x08 -> %z10.h", "srsra  %z16.h %z17.h $0x0b -> %z16.h",
        "srsra  %z21.h %z22.h $0x0d -> %z21.h", "srsra  %z31.h %z31.h $0x10 -> %z31.h",
    };
    TEST_LOOP(srsra, srsra_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "srsra  %z0.s %z0.s $0x01 -> %z0.s",    "srsra  %z5.s %z6.s $0x08 -> %z5.s",
        "srsra  %z10.s %z11.s $0x0d -> %z10.s", "srsra  %z16.s %z17.s $0x13 -> %z16.s",
        "srsra  %z21.s %z22.s $0x18 -> %z21.s", "srsra  %z31.s %z31.s $0x20 -> %z31.s",
    };
    TEST_LOOP(srsra, srsra_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));

    static const uint imm3_0_3[6] = { 1, 13, 24, 35, 45, 64 };
    const char *const expected_0_3[6] = {
        "srsra  %z0.d %z0.d $0x01 -> %z0.d",    "srsra  %z5.d %z6.d $0x0d -> %z5.d",
        "srsra  %z10.d %z11.d $0x18 -> %z10.d", "srsra  %z16.d %z17.d $0x23 -> %z16.d",
        "srsra  %z21.d %z22.d $0x2d -> %z21.d", "srsra  %z31.d %z31.d $0x40 -> %z31.d",
    };
    TEST_LOOP(srsra, srsra_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_3[i], OPSZ_6b));
}

TEST_INSTR(sshllb_sve)
{

    /* Testing SSHLLB  <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 0, 3, 4, 6, 7, 7 };
    const char *const expected_0_0[6] = {
        "sshllb %z0.b $0x00 -> %z0.h",   "sshllb %z6.b $0x03 -> %z5.h",
        "sshllb %z11.b $0x04 -> %z10.h", "sshllb %z17.b $0x06 -> %z16.h",
        "sshllb %z22.b $0x07 -> %z21.h", "sshllb %z31.b $0x07 -> %z31.h",
    };
    TEST_LOOP(sshllb, sshllb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 0, 4, 7, 10, 12, 15 };
    const char *const expected_0_1[6] = {
        "sshllb %z0.h $0x00 -> %z0.s",   "sshllb %z6.h $0x04 -> %z5.s",
        "sshllb %z11.h $0x07 -> %z10.s", "sshllb %z17.h $0x0a -> %z16.s",
        "sshllb %z22.h $0x0c -> %z21.s", "sshllb %z31.h $0x0f -> %z31.s",
    };
    TEST_LOOP(sshllb, sshllb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 0, 7, 12, 18, 23, 31 };
    const char *const expected_0_2[6] = {
        "sshllb %z0.s $0x00 -> %z0.d",   "sshllb %z6.s $0x07 -> %z5.d",
        "sshllb %z11.s $0x0c -> %z10.d", "sshllb %z17.s $0x12 -> %z16.d",
        "sshllb %z22.s $0x17 -> %z21.d", "sshllb %z31.s $0x1f -> %z31.d",
    };
    TEST_LOOP(sshllb, sshllb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(sshllt_sve)
{

    /* Testing SSHLLT  <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 0, 3, 4, 6, 7, 7 };
    const char *const expected_0_0[6] = {
        "sshllt %z0.b $0x00 -> %z0.h",   "sshllt %z6.b $0x03 -> %z5.h",
        "sshllt %z11.b $0x04 -> %z10.h", "sshllt %z17.b $0x06 -> %z16.h",
        "sshllt %z22.b $0x07 -> %z21.h", "sshllt %z31.b $0x07 -> %z31.h",
    };
    TEST_LOOP(sshllt, sshllt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 0, 4, 7, 10, 12, 15 };
    const char *const expected_0_1[6] = {
        "sshllt %z0.h $0x00 -> %z0.s",   "sshllt %z6.h $0x04 -> %z5.s",
        "sshllt %z11.h $0x07 -> %z10.s", "sshllt %z17.h $0x0a -> %z16.s",
        "sshllt %z22.h $0x0c -> %z21.s", "sshllt %z31.h $0x0f -> %z31.s",
    };
    TEST_LOOP(sshllt, sshllt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 0, 7, 12, 18, 23, 31 };
    const char *const expected_0_2[6] = {
        "sshllt %z0.s $0x00 -> %z0.d",   "sshllt %z6.s $0x07 -> %z5.d",
        "sshllt %z11.s $0x0c -> %z10.d", "sshllt %z17.s $0x12 -> %z16.d",
        "sshllt %z22.s $0x17 -> %z21.d", "sshllt %z31.s $0x1f -> %z31.d",
    };
    TEST_LOOP(sshllt, sshllt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(ssra_sve)
{

    /* Testing SSRA    <Zda>.<Ts>, <Zn>.<Ts>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "ssra   %z0.b %z0.b $0x01 -> %z0.b",    "ssra   %z5.b %z6.b $0x04 -> %z5.b",
        "ssra   %z10.b %z11.b $0x05 -> %z10.b", "ssra   %z16.b %z17.b $0x07 -> %z16.b",
        "ssra   %z21.b %z22.b $0x08 -> %z21.b", "ssra   %z31.b %z31.b $0x08 -> %z31.b",
    };
    TEST_LOOP(ssra, ssra_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "ssra   %z0.h %z0.h $0x01 -> %z0.h",    "ssra   %z5.h %z6.h $0x05 -> %z5.h",
        "ssra   %z10.h %z11.h $0x08 -> %z10.h", "ssra   %z16.h %z17.h $0x0b -> %z16.h",
        "ssra   %z21.h %z22.h $0x0d -> %z21.h", "ssra   %z31.h %z31.h $0x10 -> %z31.h",
    };
    TEST_LOOP(ssra, ssra_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "ssra   %z0.s %z0.s $0x01 -> %z0.s",    "ssra   %z5.s %z6.s $0x08 -> %z5.s",
        "ssra   %z10.s %z11.s $0x0d -> %z10.s", "ssra   %z16.s %z17.s $0x13 -> %z16.s",
        "ssra   %z21.s %z22.s $0x18 -> %z21.s", "ssra   %z31.s %z31.s $0x20 -> %z31.s",
    };
    TEST_LOOP(ssra, ssra_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));

    static const uint imm3_0_3[6] = { 1, 13, 24, 35, 45, 64 };
    const char *const expected_0_3[6] = {
        "ssra   %z0.d %z0.d $0x01 -> %z0.d",    "ssra   %z5.d %z6.d $0x0d -> %z5.d",
        "ssra   %z10.d %z11.d $0x18 -> %z10.d", "ssra   %z16.d %z17.d $0x23 -> %z16.d",
        "ssra   %z21.d %z22.d $0x2d -> %z21.d", "ssra   %z31.d %z31.d $0x40 -> %z31.d",
    };
    TEST_LOOP(ssra, ssra_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_3[i], OPSZ_6b));
}

TEST_INSTR(uqrshrnb_sve)
{

    /* Testing UQRSHRNB <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "uqrshrnb %z0.h $0x01 -> %z0.b",   "uqrshrnb %z6.h $0x04 -> %z5.b",
        "uqrshrnb %z11.h $0x05 -> %z10.b", "uqrshrnb %z17.h $0x07 -> %z16.b",
        "uqrshrnb %z22.h $0x08 -> %z21.b", "uqrshrnb %z31.h $0x08 -> %z31.b",
    };
    TEST_LOOP(uqrshrnb, uqrshrnb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "uqrshrnb %z0.s $0x01 -> %z0.h",   "uqrshrnb %z6.s $0x05 -> %z5.h",
        "uqrshrnb %z11.s $0x08 -> %z10.h", "uqrshrnb %z17.s $0x0b -> %z16.h",
        "uqrshrnb %z22.s $0x0d -> %z21.h", "uqrshrnb %z31.s $0x10 -> %z31.h",
    };
    TEST_LOOP(uqrshrnb, uqrshrnb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "uqrshrnb %z0.d $0x01 -> %z0.s",   "uqrshrnb %z6.d $0x08 -> %z5.s",
        "uqrshrnb %z11.d $0x0d -> %z10.s", "uqrshrnb %z17.d $0x13 -> %z16.s",
        "uqrshrnb %z22.d $0x18 -> %z21.s", "uqrshrnb %z31.d $0x20 -> %z31.s",
    };
    TEST_LOOP(uqrshrnb, uqrshrnb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(uqrshrnt_sve)
{

    /* Testing UQRSHRNT <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "uqrshrnt %z0.b %z0.h $0x01 -> %z0.b",
        "uqrshrnt %z5.b %z6.h $0x04 -> %z5.b",
        "uqrshrnt %z10.b %z11.h $0x05 -> %z10.b",
        "uqrshrnt %z16.b %z17.h $0x07 -> %z16.b",
        "uqrshrnt %z21.b %z22.h $0x08 -> %z21.b",
        "uqrshrnt %z31.b %z31.h $0x08 -> %z31.b",
    };
    TEST_LOOP(uqrshrnt, uqrshrnt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "uqrshrnt %z0.h %z0.s $0x01 -> %z0.h",
        "uqrshrnt %z5.h %z6.s $0x05 -> %z5.h",
        "uqrshrnt %z10.h %z11.s $0x08 -> %z10.h",
        "uqrshrnt %z16.h %z17.s $0x0b -> %z16.h",
        "uqrshrnt %z21.h %z22.s $0x0d -> %z21.h",
        "uqrshrnt %z31.h %z31.s $0x10 -> %z31.h",
    };
    TEST_LOOP(uqrshrnt, uqrshrnt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "uqrshrnt %z0.s %z0.d $0x01 -> %z0.s",
        "uqrshrnt %z5.s %z6.d $0x08 -> %z5.s",
        "uqrshrnt %z10.s %z11.d $0x0d -> %z10.s",
        "uqrshrnt %z16.s %z17.d $0x13 -> %z16.s",
        "uqrshrnt %z21.s %z22.d $0x18 -> %z21.s",
        "uqrshrnt %z31.s %z31.d $0x20 -> %z31.s",
    };
    TEST_LOOP(uqrshrnt, uqrshrnt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(uqshrnb_sve)
{

    /* Testing UQSHRNB <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "uqshrnb %z0.h $0x01 -> %z0.b",   "uqshrnb %z6.h $0x04 -> %z5.b",
        "uqshrnb %z11.h $0x05 -> %z10.b", "uqshrnb %z17.h $0x07 -> %z16.b",
        "uqshrnb %z22.h $0x08 -> %z21.b", "uqshrnb %z31.h $0x08 -> %z31.b",
    };
    TEST_LOOP(uqshrnb, uqshrnb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "uqshrnb %z0.s $0x01 -> %z0.h",   "uqshrnb %z6.s $0x05 -> %z5.h",
        "uqshrnb %z11.s $0x08 -> %z10.h", "uqshrnb %z17.s $0x0b -> %z16.h",
        "uqshrnb %z22.s $0x0d -> %z21.h", "uqshrnb %z31.s $0x10 -> %z31.h",
    };
    TEST_LOOP(uqshrnb, uqshrnb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "uqshrnb %z0.d $0x01 -> %z0.s",   "uqshrnb %z6.d $0x08 -> %z5.s",
        "uqshrnb %z11.d $0x0d -> %z10.s", "uqshrnb %z17.d $0x13 -> %z16.s",
        "uqshrnb %z22.d $0x18 -> %z21.s", "uqshrnb %z31.d $0x20 -> %z31.s",
    };
    TEST_LOOP(uqshrnb, uqshrnb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(uqshrnt_sve)
{

    /* Testing UQSHRNT <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "uqshrnt %z0.b %z0.h $0x01 -> %z0.b",    "uqshrnt %z5.b %z6.h $0x04 -> %z5.b",
        "uqshrnt %z10.b %z11.h $0x05 -> %z10.b", "uqshrnt %z16.b %z17.h $0x07 -> %z16.b",
        "uqshrnt %z21.b %z22.h $0x08 -> %z21.b", "uqshrnt %z31.b %z31.h $0x08 -> %z31.b",
    };
    TEST_LOOP(uqshrnt, uqshrnt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "uqshrnt %z0.h %z0.s $0x01 -> %z0.h",    "uqshrnt %z5.h %z6.s $0x05 -> %z5.h",
        "uqshrnt %z10.h %z11.s $0x08 -> %z10.h", "uqshrnt %z16.h %z17.s $0x0b -> %z16.h",
        "uqshrnt %z21.h %z22.s $0x0d -> %z21.h", "uqshrnt %z31.h %z31.s $0x10 -> %z31.h",
    };
    TEST_LOOP(uqshrnt, uqshrnt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "uqshrnt %z0.s %z0.d $0x01 -> %z0.s",    "uqshrnt %z5.s %z6.d $0x08 -> %z5.s",
        "uqshrnt %z10.s %z11.d $0x0d -> %z10.s", "uqshrnt %z16.s %z17.d $0x13 -> %z16.s",
        "uqshrnt %z21.s %z22.d $0x18 -> %z21.s", "uqshrnt %z31.s %z31.d $0x20 -> %z31.s",
    };
    TEST_LOOP(uqshrnt, uqshrnt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(urshr_sve_pred)
{

    /* Testing URSHR   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "urshr  %p0/m %z0.b $0x01 -> %z0.b",   "urshr  %p2/m %z5.b $0x04 -> %z5.b",
        "urshr  %p3/m %z10.b $0x05 -> %z10.b", "urshr  %p5/m %z16.b $0x07 -> %z16.b",
        "urshr  %p6/m %z21.b $0x08 -> %z21.b", "urshr  %p7/m %z31.b $0x08 -> %z31.b",
    };
    TEST_LOOP(urshr, urshr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "urshr  %p0/m %z0.h $0x01 -> %z0.h",   "urshr  %p2/m %z5.h $0x05 -> %z5.h",
        "urshr  %p3/m %z10.h $0x08 -> %z10.h", "urshr  %p5/m %z16.h $0x0b -> %z16.h",
        "urshr  %p6/m %z21.h $0x0d -> %z21.h", "urshr  %p7/m %z31.h $0x10 -> %z31.h",
    };
    TEST_LOOP(urshr, urshr_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "urshr  %p0/m %z0.s $0x01 -> %z0.s",   "urshr  %p2/m %z5.s $0x08 -> %z5.s",
        "urshr  %p3/m %z10.s $0x0d -> %z10.s", "urshr  %p5/m %z16.s $0x13 -> %z16.s",
        "urshr  %p6/m %z21.s $0x18 -> %z21.s", "urshr  %p7/m %z31.s $0x20 -> %z31.s",
    };
    TEST_LOOP(urshr, urshr_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));

    static const uint imm3_0_3[6] = { 1, 13, 24, 35, 45, 64 };
    const char *const expected_0_3[6] = {
        "urshr  %p0/m %z0.d $0x01 -> %z0.d",   "urshr  %p2/m %z5.d $0x0d -> %z5.d",
        "urshr  %p3/m %z10.d $0x18 -> %z10.d", "urshr  %p5/m %z16.d $0x23 -> %z16.d",
        "urshr  %p6/m %z21.d $0x2d -> %z21.d", "urshr  %p7/m %z31.d $0x40 -> %z31.d",
    };
    TEST_LOOP(urshr, urshr_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_immed_uint(imm3_0_3[i], OPSZ_6b));
}

TEST_INSTR(ursra_sve)
{

    /* Testing URSRA   <Zda>.<Ts>, <Zn>.<Ts>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "ursra  %z0.b %z0.b $0x01 -> %z0.b",    "ursra  %z5.b %z6.b $0x04 -> %z5.b",
        "ursra  %z10.b %z11.b $0x05 -> %z10.b", "ursra  %z16.b %z17.b $0x07 -> %z16.b",
        "ursra  %z21.b %z22.b $0x08 -> %z21.b", "ursra  %z31.b %z31.b $0x08 -> %z31.b",
    };
    TEST_LOOP(ursra, ursra_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "ursra  %z0.h %z0.h $0x01 -> %z0.h",    "ursra  %z5.h %z6.h $0x05 -> %z5.h",
        "ursra  %z10.h %z11.h $0x08 -> %z10.h", "ursra  %z16.h %z17.h $0x0b -> %z16.h",
        "ursra  %z21.h %z22.h $0x0d -> %z21.h", "ursra  %z31.h %z31.h $0x10 -> %z31.h",
    };
    TEST_LOOP(ursra, ursra_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "ursra  %z0.s %z0.s $0x01 -> %z0.s",    "ursra  %z5.s %z6.s $0x08 -> %z5.s",
        "ursra  %z10.s %z11.s $0x0d -> %z10.s", "ursra  %z16.s %z17.s $0x13 -> %z16.s",
        "ursra  %z21.s %z22.s $0x18 -> %z21.s", "ursra  %z31.s %z31.s $0x20 -> %z31.s",
    };
    TEST_LOOP(ursra, ursra_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));

    static const uint imm3_0_3[6] = { 1, 13, 24, 35, 45, 64 };
    const char *const expected_0_3[6] = {
        "ursra  %z0.d %z0.d $0x01 -> %z0.d",    "ursra  %z5.d %z6.d $0x0d -> %z5.d",
        "ursra  %z10.d %z11.d $0x18 -> %z10.d", "ursra  %z16.d %z17.d $0x23 -> %z16.d",
        "ursra  %z21.d %z22.d $0x2d -> %z21.d", "ursra  %z31.d %z31.d $0x40 -> %z31.d",
    };
    TEST_LOOP(ursra, ursra_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_3[i], OPSZ_6b));
}

TEST_INSTR(ushllb_sve)
{

    /* Testing USHLLB  <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 0, 3, 4, 6, 7, 7 };
    const char *const expected_0_0[6] = {
        "ushllb %z0.b $0x00 -> %z0.h",   "ushllb %z6.b $0x03 -> %z5.h",
        "ushllb %z11.b $0x04 -> %z10.h", "ushllb %z17.b $0x06 -> %z16.h",
        "ushllb %z22.b $0x07 -> %z21.h", "ushllb %z31.b $0x07 -> %z31.h",
    };
    TEST_LOOP(ushllb, ushllb_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 0, 4, 7, 10, 12, 15 };
    const char *const expected_0_1[6] = {
        "ushllb %z0.h $0x00 -> %z0.s",   "ushllb %z6.h $0x04 -> %z5.s",
        "ushllb %z11.h $0x07 -> %z10.s", "ushllb %z17.h $0x0a -> %z16.s",
        "ushllb %z22.h $0x0c -> %z21.s", "ushllb %z31.h $0x0f -> %z31.s",
    };
    TEST_LOOP(ushllb, ushllb_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 0, 7, 12, 18, 23, 31 };
    const char *const expected_0_2[6] = {
        "ushllb %z0.s $0x00 -> %z0.d",   "ushllb %z6.s $0x07 -> %z5.d",
        "ushllb %z11.s $0x0c -> %z10.d", "ushllb %z17.s $0x12 -> %z16.d",
        "ushllb %z22.s $0x17 -> %z21.d", "ushllb %z31.s $0x1f -> %z31.d",
    };
    TEST_LOOP(ushllb, ushllb_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(ushllt_sve)
{

    /* Testing USHLLT  <Zd>.<Ts>, <Zn>.<Tb>, #<const> */
    static const uint imm3_0_0[6] = { 0, 3, 4, 6, 7, 7 };
    const char *const expected_0_0[6] = {
        "ushllt %z0.b $0x00 -> %z0.h",   "ushllt %z6.b $0x03 -> %z5.h",
        "ushllt %z11.b $0x04 -> %z10.h", "ushllt %z17.b $0x06 -> %z16.h",
        "ushllt %z22.b $0x07 -> %z21.h", "ushllt %z31.b $0x07 -> %z31.h",
    };
    TEST_LOOP(ushllt, ushllt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 0, 4, 7, 10, 12, 15 };
    const char *const expected_0_1[6] = {
        "ushllt %z0.h $0x00 -> %z0.s",   "ushllt %z6.h $0x04 -> %z5.s",
        "ushllt %z11.h $0x07 -> %z10.s", "ushllt %z17.h $0x0a -> %z16.s",
        "ushllt %z22.h $0x0c -> %z21.s", "ushllt %z31.h $0x0f -> %z31.s",
    };
    TEST_LOOP(ushllt, ushllt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 0, 7, 12, 18, 23, 31 };
    const char *const expected_0_2[6] = {
        "ushllt %z0.s $0x00 -> %z0.d",   "ushllt %z6.s $0x07 -> %z5.d",
        "ushllt %z11.s $0x0c -> %z10.d", "ushllt %z17.s $0x12 -> %z16.d",
        "ushllt %z22.s $0x17 -> %z21.d", "ushllt %z31.s $0x1f -> %z31.d",
    };
    TEST_LOOP(ushllt, ushllt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));
}

TEST_INSTR(usra_sve)
{

    /* Testing USRA    <Zda>.<Ts>, <Zn>.<Ts>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "usra   %z0.b %z0.b $0x01 -> %z0.b",    "usra   %z5.b %z6.b $0x04 -> %z5.b",
        "usra   %z10.b %z11.b $0x05 -> %z10.b", "usra   %z16.b %z17.b $0x07 -> %z16.b",
        "usra   %z21.b %z22.b $0x08 -> %z21.b", "usra   %z31.b %z31.b $0x08 -> %z31.b",
    };
    TEST_LOOP(usra, usra_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "usra   %z0.h %z0.h $0x01 -> %z0.h",    "usra   %z5.h %z6.h $0x05 -> %z5.h",
        "usra   %z10.h %z11.h $0x08 -> %z10.h", "usra   %z16.h %z17.h $0x0b -> %z16.h",
        "usra   %z21.h %z22.h $0x0d -> %z21.h", "usra   %z31.h %z31.h $0x10 -> %z31.h",
    };
    TEST_LOOP(usra, usra_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "usra   %z0.s %z0.s $0x01 -> %z0.s",    "usra   %z5.s %z6.s $0x08 -> %z5.s",
        "usra   %z10.s %z11.s $0x0d -> %z10.s", "usra   %z16.s %z17.s $0x13 -> %z16.s",
        "usra   %z21.s %z22.s $0x18 -> %z21.s", "usra   %z31.s %z31.s $0x20 -> %z31.s",
    };
    TEST_LOOP(usra, usra_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));

    static const uint imm3_0_3[6] = { 1, 13, 24, 35, 45, 64 };
    const char *const expected_0_3[6] = {
        "usra   %z0.d %z0.d $0x01 -> %z0.d",    "usra   %z5.d %z6.d $0x0d -> %z5.d",
        "usra   %z10.d %z11.d $0x18 -> %z10.d", "usra   %z16.d %z17.d $0x23 -> %z16.d",
        "usra   %z21.d %z22.d $0x2d -> %z21.d", "usra   %z31.d %z31.d $0x40 -> %z31.d",
    };
    TEST_LOOP(usra, usra_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_3[i], OPSZ_6b));
}

TEST_INSTR(xar_sve)
{

    /* Testing XAR     <Zdn>.<Ts>, <Zdn>.<Ts>, <Zm>.<Ts>, #<const> */
    static const uint imm3_0_0[6] = { 1, 4, 5, 7, 8, 8 };
    const char *const expected_0_0[6] = {
        "xar    %z0.b %z0.b $0x01 -> %z0.b",    "xar    %z5.b %z6.b $0x04 -> %z5.b",
        "xar    %z10.b %z11.b $0x05 -> %z10.b", "xar    %z16.b %z17.b $0x07 -> %z16.b",
        "xar    %z21.b %z22.b $0x08 -> %z21.b", "xar    %z31.b %z31.b $0x08 -> %z31.b",
    };
    TEST_LOOP(xar, xar_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    static const uint imm3_0_1[6] = { 1, 5, 8, 11, 13, 16 };
    const char *const expected_0_1[6] = {
        "xar    %z0.h %z0.h $0x01 -> %z0.h",    "xar    %z5.h %z6.h $0x05 -> %z5.h",
        "xar    %z10.h %z11.h $0x08 -> %z10.h", "xar    %z16.h %z17.h $0x0b -> %z16.h",
        "xar    %z21.h %z22.h $0x0d -> %z21.h", "xar    %z31.h %z31.h $0x10 -> %z31.h",
    };
    TEST_LOOP(xar, xar_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_4b));

    static const uint imm3_0_2[6] = { 1, 8, 13, 19, 24, 32 };
    const char *const expected_0_2[6] = {
        "xar    %z0.s %z0.s $0x01 -> %z0.s",    "xar    %z5.s %z6.s $0x08 -> %z5.s",
        "xar    %z10.s %z11.s $0x0d -> %z10.s", "xar    %z16.s %z17.s $0x13 -> %z16.s",
        "xar    %z21.s %z22.s $0x18 -> %z21.s", "xar    %z31.s %z31.s $0x20 -> %z31.s",
    };
    TEST_LOOP(xar, xar_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_5b));

    static const uint imm3_0_3[6] = { 1, 13, 24, 35, 45, 64 };
    const char *const expected_0_3[6] = {
        "xar    %z0.d %z0.d $0x01 -> %z0.d",    "xar    %z5.d %z6.d $0x0d -> %z5.d",
        "xar    %z10.d %z11.d $0x18 -> %z10.d", "xar    %z16.d %z17.d $0x23 -> %z16.d",
        "xar    %z21.d %z22.d $0x2d -> %z21.d", "xar    %z31.d %z31.d $0x40 -> %z31.d",
    };
    TEST_LOOP(xar, xar_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_3[i], OPSZ_6b));
}

TEST_INSTR(ldnt1sb_sve_pred)
{

    /* Testing LDNT1SB { <Zt>.D }, <Pg>/Z, [<Zn>.D{, <Xm>}] */
    const char *const expected_0_0[6] = {
        "ldnt1sb (%z0.d,%x0)[4byte] %p0/z -> %z0.d",
        "ldnt1sb (%z7.d,%x8)[4byte] %p2/z -> %z5.d",
        "ldnt1sb (%z12.d,%x13)[4byte] %p3/z -> %z10.d",
        "ldnt1sb (%z18.d,%x18)[4byte] %p5/z -> %z16.d",
        "ldnt1sb (%z23.d,%x23)[4byte] %p6/z -> %z21.d",
        "ldnt1sb (%z31.d,%x30)[4byte] %p7/z -> %z31.d",
    };
    TEST_LOOP(ldnt1sb, ldnt1sb_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_vector_base_disp_aarch64(Zn_six_offset_2[i], Xn_six_offset_3[i],
                                                   OPSZ_8, DR_EXTEND_UXTX, 0, 0, 0,
                                                   OPSZ_4, 0));

    /* Testing LDNT1SB { <Zt>.S }, <Pg>/Z, [<Zn>.S{, <Xm>}] */
    const char *const expected_1_0[6] = {
        "ldnt1sb (%z0.s,%x0)[8byte] %p0/z -> %z0.s",
        "ldnt1sb (%z7.s,%x8)[8byte] %p2/z -> %z5.s",
        "ldnt1sb (%z12.s,%x13)[8byte] %p3/z -> %z10.s",
        "ldnt1sb (%z18.s,%x18)[8byte] %p5/z -> %z16.s",
        "ldnt1sb (%z23.s,%x23)[8byte] %p6/z -> %z21.s",
        "ldnt1sb (%z31.s,%x30)[8byte] %p7/z -> %z31.s",
    };
    TEST_LOOP(ldnt1sb, ldnt1sb_sve_pred, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_vector_base_disp_aarch64(Zn_six_offset_2[i], Xn_six_offset_3[i],
                                                   OPSZ_4, DR_EXTEND_UXTX, 0, 0, 0,
                                                   OPSZ_8, 0));
}

TEST_INSTR(ldnt1sh_sve_pred)
{

    /* Testing LDNT1SH { <Zt>.D }, <Pg>/Z, [<Zn>.D{, <Xm>}] */
    const char *const expected_0_0[6] = {
        "ldnt1sh (%z0.d,%x0)[8byte] %p0/z -> %z0.d",
        "ldnt1sh (%z7.d,%x8)[8byte] %p2/z -> %z5.d",
        "ldnt1sh (%z12.d,%x13)[8byte] %p3/z -> %z10.d",
        "ldnt1sh (%z18.d,%x18)[8byte] %p5/z -> %z16.d",
        "ldnt1sh (%z23.d,%x23)[8byte] %p6/z -> %z21.d",
        "ldnt1sh (%z31.d,%x30)[8byte] %p7/z -> %z31.d",
    };
    TEST_LOOP(ldnt1sh, ldnt1sh_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_vector_base_disp_aarch64(Zn_six_offset_2[i], Xn_six_offset_3[i],
                                                   OPSZ_8, DR_EXTEND_UXTX, 0, 0, 0,
                                                   OPSZ_8, 0));

    /* Testing LDNT1SH { <Zt>.S }, <Pg>/Z, [<Zn>.S{, <Xm>}] */
    const char *const expected_1_0[6] = {
        "ldnt1sh (%z0.s,%x0)[16byte] %p0/z -> %z0.s",
        "ldnt1sh (%z7.s,%x8)[16byte] %p2/z -> %z5.s",
        "ldnt1sh (%z12.s,%x13)[16byte] %p3/z -> %z10.s",
        "ldnt1sh (%z18.s,%x18)[16byte] %p5/z -> %z16.s",
        "ldnt1sh (%z23.s,%x23)[16byte] %p6/z -> %z21.s",
        "ldnt1sh (%z31.s,%x30)[16byte] %p7/z -> %z31.s",
    };
    TEST_LOOP(ldnt1sh, ldnt1sh_sve_pred, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_vector_base_disp_aarch64(Zn_six_offset_2[i], Xn_six_offset_3[i],
                                                   OPSZ_4, DR_EXTEND_UXTX, 0, 0, 0,
                                                   OPSZ_16, 0));
}

TEST_INSTR(ldnt1sw_sve_pred)
{

    /* Testing LDNT1SW { <Zt>.D }, <Pg>/Z, [<Zn>.D{, <Xm>}] */
    const char *const expected_0_0[6] = {
        "ldnt1sw (%z0.d,%x0)[16byte] %p0/z -> %z0.d",
        "ldnt1sw (%z7.d,%x8)[16byte] %p2/z -> %z5.d",
        "ldnt1sw (%z12.d,%x13)[16byte] %p3/z -> %z10.d",
        "ldnt1sw (%z18.d,%x18)[16byte] %p5/z -> %z16.d",
        "ldnt1sw (%z23.d,%x23)[16byte] %p6/z -> %z21.d",
        "ldnt1sw (%z31.d,%x30)[16byte] %p7/z -> %z31.d",
    };
    TEST_LOOP(ldnt1sw, ldnt1sw_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_vector_base_disp_aarch64(Zn_six_offset_2[i], Xn_six_offset_3[i],
                                                   OPSZ_8, DR_EXTEND_UXTX, 0, 0, 0,
                                                   OPSZ_16, 0));
}

TEST_INSTR(cdot_sve_idx_imm_vector)
{

    /* Testing CDOT    <Zda>.D, <Zn>.H, <Zm>.H[<imm>], <const> */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i1_0_0[6] = { 0, 1, 1, 1, 0, 1 };
    static const uint rot_0_0[6] = { 0, 0, 90, 180, 180, 270 };
    const char *const expected_0_0[6] = {
        "cdot   %z0.d %z0.h %z0.h $0x00 $0x0000 -> %z0.d",
        "cdot   %z5.d %z6.h %z4.h $0x01 $0x0000 -> %z5.d",
        "cdot   %z10.d %z11.h %z7.h $0x01 $0x005a -> %z10.d",
        "cdot   %z16.d %z17.h %z10.h $0x01 $0x00b4 -> %z16.d",
        "cdot   %z21.d %z22.h %z12.h $0x00 $0x00b4 -> %z21.d",
        "cdot   %z31.d %z31.h %z15.h $0x01 $0x010e -> %z31.d",
    };
    TEST_LOOP(cdot, cdot_sve_idx_imm_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_2),
              opnd_create_immed_uint(i1_0_0[i], OPSZ_1b),
              opnd_create_immed_uint(rot_0_0[i], OPSZ_2));

    /* Testing CDOT    <Zda>.S, <Zn>.B, <Zm>.B[<imm>], <const> */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i2_1_0[6] = { 0, 3, 0, 1, 1, 3 };
    static const uint rot_1_0[6] = { 0, 0, 90, 180, 180, 270 };
    const char *const expected_1_0[6] = {
        "cdot   %z0.s %z0.b %z0.b $0x00 $0x0000 -> %z0.s",
        "cdot   %z5.s %z6.b %z3.b $0x03 $0x0000 -> %z5.s",
        "cdot   %z10.s %z11.b %z4.b $0x00 $0x005a -> %z10.s",
        "cdot   %z16.s %z17.b %z6.b $0x01 $0x00b4 -> %z16.s",
        "cdot   %z21.s %z22.b %z7.b $0x01 $0x00b4 -> %z21.s",
        "cdot   %z31.s %z31.b %z7.b $0x03 $0x010e -> %z31.s",
    };
    TEST_LOOP(cdot, cdot_sve_idx_imm_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_1),
              opnd_create_immed_uint(i2_1_0[i], OPSZ_2b),
              opnd_create_immed_uint(rot_1_0[i], OPSZ_2));
}

TEST_INSTR(cmla_sve_idx_imm_vector)
{

    /* Testing CMLA    <Zda>.S, <Zn>.S, <Zm>.S[<imm>], <const> */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i1_0_0[6] = { 0, 1, 1, 1, 0, 1 };
    static const uint rot_0_0[6] = { 0, 0, 90, 180, 180, 270 };
    const char *const expected_0_0[6] = {
        "cmla   %z0.s %z0.s %z0.s $0x00 $0x0000 -> %z0.s",
        "cmla   %z5.s %z6.s %z4.s $0x01 $0x0000 -> %z5.s",
        "cmla   %z10.s %z11.s %z7.s $0x01 $0x005a -> %z10.s",
        "cmla   %z16.s %z17.s %z10.s $0x01 $0x00b4 -> %z16.s",
        "cmla   %z21.s %z22.s %z12.s $0x00 $0x00b4 -> %z21.s",
        "cmla   %z31.s %z31.s %z15.s $0x01 $0x010e -> %z31.s",
    };
    TEST_LOOP(cmla, cmla_sve_idx_imm_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i1_0_0[i], OPSZ_1b),
              opnd_create_immed_uint(rot_0_0[i], OPSZ_2));

    /* Testing CMLA    <Zda>.H, <Zn>.H, <Zm>.H[<imm>], <const> */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i2_1_0[6] = { 0, 3, 0, 1, 1, 3 };
    static const uint rot_1_0[6] = { 0, 0, 90, 180, 180, 270 };
    const char *const expected_1_0[6] = {
        "cmla   %z0.h %z0.h %z0.h $0x00 $0x0000 -> %z0.h",
        "cmla   %z5.h %z6.h %z3.h $0x03 $0x0000 -> %z5.h",
        "cmla   %z10.h %z11.h %z4.h $0x00 $0x005a -> %z10.h",
        "cmla   %z16.h %z17.h %z6.h $0x01 $0x00b4 -> %z16.h",
        "cmla   %z21.h %z22.h %z7.h $0x01 $0x00b4 -> %z21.h",
        "cmla   %z31.h %z31.h %z7.h $0x03 $0x010e -> %z31.h",
    };
    TEST_LOOP(cmla, cmla_sve_idx_imm_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i2_1_0[i], OPSZ_2b),
              opnd_create_immed_uint(rot_1_0[i], OPSZ_2));
}

TEST_INSTR(sqrdcmlah_sve_idx_imm_vector)
{

    /* Testing SQRDCMLAH <Zda>.S, <Zn>.S, <Zm>.S[<imm>], <const> */
    static const reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z4,  DR_REG_Z7,
                                        DR_REG_Z10, DR_REG_Z12, DR_REG_Z15 };
    static const uint i1_0_0[6] = { 0, 1, 1, 1, 0, 1 };
    static const uint rot_0_0[6] = { 0, 0, 90, 180, 180, 270 };
    const char *const expected_0_0[6] = {
        "sqrdcmlah %z0.s %z0.s %z0.s $0x00 $0x0000 -> %z0.s",
        "sqrdcmlah %z5.s %z6.s %z4.s $0x01 $0x0000 -> %z5.s",
        "sqrdcmlah %z10.s %z11.s %z7.s $0x01 $0x005a -> %z10.s",
        "sqrdcmlah %z16.s %z17.s %z10.s $0x01 $0x00b4 -> %z16.s",
        "sqrdcmlah %z21.s %z22.s %z12.s $0x00 $0x00b4 -> %z21.s",
        "sqrdcmlah %z31.s %z31.s %z15.s $0x01 $0x010e -> %z31.s",
    };
    TEST_LOOP(sqrdcmlah, sqrdcmlah_sve_idx_imm_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4),
              opnd_create_immed_uint(i1_0_0[i], OPSZ_1b),
              opnd_create_immed_uint(rot_0_0[i], OPSZ_2));

    /* Testing SQRDCMLAH <Zda>.H, <Zn>.H, <Zm>.H[<imm>], <const> */
    static const reg_id_t Zm_1_0[6] = { DR_REG_Z0, DR_REG_Z3, DR_REG_Z4,
                                        DR_REG_Z6, DR_REG_Z7, DR_REG_Z7 };
    static const uint i2_1_0[6] = { 0, 3, 0, 1, 1, 3 };
    static const uint rot_1_0[6] = { 0, 0, 90, 180, 180, 270 };
    const char *const expected_1_0[6] = {
        "sqrdcmlah %z0.h %z0.h %z0.h $0x00 $0x0000 -> %z0.h",
        "sqrdcmlah %z5.h %z6.h %z3.h $0x03 $0x0000 -> %z5.h",
        "sqrdcmlah %z10.h %z11.h %z4.h $0x00 $0x005a -> %z10.h",
        "sqrdcmlah %z16.h %z17.h %z6.h $0x01 $0x00b4 -> %z16.h",
        "sqrdcmlah %z21.h %z22.h %z7.h $0x01 $0x00b4 -> %z21.h",
        "sqrdcmlah %z31.h %z31.h %z7.h $0x03 $0x010e -> %z31.h",
    };
    TEST_LOOP(sqrdcmlah, sqrdcmlah_sve_idx_imm_vector, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_1_0[i], OPSZ_2),
              opnd_create_immed_uint(i2_1_0[i], OPSZ_2b),
              opnd_create_immed_uint(rot_1_0[i], OPSZ_2));
}

TEST_INSTR(match_sve_pred)
{

    /* Testing MATCH   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "match  %p0/z %z0.b %z0.b -> %p0.b",    "match  %p2/z %z7.b %z8.b -> %p2.b",
        "match  %p3/z %z12.b %z13.b -> %p5.b",  "match  %p5/z %z18.b %z19.b -> %p8.b",
        "match  %p6/z %z23.b %z24.b -> %p10.b", "match  %p7/z %z31.b %z31.b -> %p15.b",
    };
    TEST_LOOP(match, match_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "match  %p0/z %z0.h %z0.h -> %p0.h",    "match  %p2/z %z7.h %z8.h -> %p2.h",
        "match  %p3/z %z12.h %z13.h -> %p5.h",  "match  %p5/z %z18.h %z19.h -> %p8.h",
        "match  %p6/z %z23.h %z24.h -> %p10.h", "match  %p7/z %z31.h %z31.h -> %p15.h",
    };
    TEST_LOOP(match, match_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));
}

TEST_INSTR(nmatch_sve_pred)
{

    /* Testing NMATCH  <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *const expected_0_0[6] = {
        "nmatch %p0/z %z0.b %z0.b -> %p0.b",    "nmatch %p2/z %z7.b %z8.b -> %p2.b",
        "nmatch %p3/z %z12.b %z13.b -> %p5.b",  "nmatch %p5/z %z18.b %z19.b -> %p8.b",
        "nmatch %p6/z %z23.b %z24.b -> %p10.b", "nmatch %p7/z %z31.b %z31.b -> %p15.b",
    };
    TEST_LOOP(nmatch, nmatch_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_1));

    const char *const expected_0_1[6] = {
        "nmatch %p0/z %z0.h %z0.h -> %p0.h",    "nmatch %p2/z %z7.h %z8.h -> %p2.h",
        "nmatch %p3/z %z12.h %z13.h -> %p5.h",  "nmatch %p5/z %z18.h %z19.h -> %p8.h",
        "nmatch %p6/z %z23.h %z24.h -> %p10.h", "nmatch %p7/z %z31.h %z31.h -> %p15.h",
    };
    TEST_LOOP(nmatch, nmatch_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));
}

TEST_INSTR(urecpe_sve_pred)
{

    /* Testing URECPE  <Zd>.S, <Pg>/M, <Zn>.S */
    const char *const expected_0_0[6] = {
        "urecpe %p0/m %z0.s -> %z0.s",   "urecpe %p2/m %z7.s -> %z5.s",
        "urecpe %p3/m %z12.s -> %z10.s", "urecpe %p5/m %z18.s -> %z16.s",
        "urecpe %p6/m %z23.s -> %z21.s", "urecpe %p7/m %z31.s -> %z31.s",
    };
    TEST_LOOP(urecpe, urecpe_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(ursqrte_sve_pred)
{

    /* Testing URSQRTE <Zd>.S, <Pg>/M, <Zn>.S */
    const char *const expected_0_0[6] = {
        "ursqrte %p0/m %z0.s -> %z0.s",   "ursqrte %p2/m %z7.s -> %z5.s",
        "ursqrte %p3/m %z12.s -> %z10.s", "ursqrte %p5/m %z18.s -> %z16.s",
        "ursqrte %p6/m %z23.s -> %z21.s", "ursqrte %p7/m %z31.s -> %z31.s",
    };
    TEST_LOOP(ursqrte, ursqrte_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));
}

TEST_INSTR(whilege_sve)
{

    /* Testing WHILEGE <Pd>.<Ts>, <R><n>, <R><m> */
    const char *const expected_0_0[6] = {
        "whilege %w0 %w0 -> %p0.b",    "whilege %w6 %w7 -> %p2.b",
        "whilege %w11 %w12 -> %p5.b",  "whilege %w16 %w17 -> %p8.b",
        "whilege %w21 %w22 -> %p10.b", "whilege %w30 %w30 -> %p15.b",
    };
    TEST_LOOP(whilege, whilege_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_reg(Wn_six_offset_1[i]), opnd_create_reg(Wn_six_offset_2[i]));

    const char *const expected_0_1[6] = {
        "whilege %x0 %x0 -> %p0.b",    "whilege %x6 %x7 -> %p2.b",
        "whilege %x11 %x12 -> %p5.b",  "whilege %x16 %x17 -> %p8.b",
        "whilege %x21 %x22 -> %p10.b", "whilege %x30 %x30 -> %p15.b",
    };
    TEST_LOOP(whilege, whilege_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    const char *const expected_0_2[6] = {
        "whilege %w0 %w0 -> %p0.h",    "whilege %w6 %w7 -> %p2.h",
        "whilege %w11 %w12 -> %p5.h",  "whilege %w16 %w17 -> %p8.h",
        "whilege %w21 %w22 -> %p10.h", "whilege %w30 %w30 -> %p15.h",
    };
    TEST_LOOP(whilege, whilege_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_reg(Wn_six_offset_1[i]), opnd_create_reg(Wn_six_offset_2[i]));

    const char *const expected_0_3[6] = {
        "whilege %x0 %x0 -> %p0.h",    "whilege %x6 %x7 -> %p2.h",
        "whilege %x11 %x12 -> %p5.h",  "whilege %x16 %x17 -> %p8.h",
        "whilege %x21 %x22 -> %p10.h", "whilege %x30 %x30 -> %p15.h",
    };
    TEST_LOOP(whilege, whilege_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    const char *const expected_0_4[6] = {
        "whilege %w0 %w0 -> %p0.s",    "whilege %w6 %w7 -> %p2.s",
        "whilege %w11 %w12 -> %p5.s",  "whilege %w16 %w17 -> %p8.s",
        "whilege %w21 %w22 -> %p10.s", "whilege %w30 %w30 -> %p15.s",
    };
    TEST_LOOP(whilege, whilege_sve, 6, expected_0_4[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_reg(Wn_six_offset_1[i]), opnd_create_reg(Wn_six_offset_2[i]));

    const char *const expected_0_5[6] = {
        "whilege %x0 %x0 -> %p0.s",    "whilege %x6 %x7 -> %p2.s",
        "whilege %x11 %x12 -> %p5.s",  "whilege %x16 %x17 -> %p8.s",
        "whilege %x21 %x22 -> %p10.s", "whilege %x30 %x30 -> %p15.s",
    };
    TEST_LOOP(whilege, whilege_sve, 6, expected_0_5[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    const char *const expected_0_6[6] = {
        "whilege %w0 %w0 -> %p0.d",    "whilege %w6 %w7 -> %p2.d",
        "whilege %w11 %w12 -> %p5.d",  "whilege %w16 %w17 -> %p8.d",
        "whilege %w21 %w22 -> %p10.d", "whilege %w30 %w30 -> %p15.d",
    };
    TEST_LOOP(whilege, whilege_sve, 6, expected_0_6[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_reg(Wn_six_offset_1[i]), opnd_create_reg(Wn_six_offset_2[i]));

    const char *const expected_0_7[6] = {
        "whilege %x0 %x0 -> %p0.d",    "whilege %x6 %x7 -> %p2.d",
        "whilege %x11 %x12 -> %p5.d",  "whilege %x16 %x17 -> %p8.d",
        "whilege %x21 %x22 -> %p10.d", "whilege %x30 %x30 -> %p15.d",
    };
    TEST_LOOP(whilege, whilege_sve, 6, expected_0_7[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));
}

TEST_INSTR(whilegt_sve)
{

    /* Testing WHILEGT <Pd>.<Ts>, <R><n>, <R><m> */
    const char *const expected_0_0[6] = {
        "whilegt %w0 %w0 -> %p0.b",    "whilegt %w6 %w7 -> %p2.b",
        "whilegt %w11 %w12 -> %p5.b",  "whilegt %w16 %w17 -> %p8.b",
        "whilegt %w21 %w22 -> %p10.b", "whilegt %w30 %w30 -> %p15.b",
    };
    TEST_LOOP(whilegt, whilegt_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_reg(Wn_six_offset_1[i]), opnd_create_reg(Wn_six_offset_2[i]));

    const char *const expected_0_1[6] = {
        "whilegt %x0 %x0 -> %p0.b",    "whilegt %x6 %x7 -> %p2.b",
        "whilegt %x11 %x12 -> %p5.b",  "whilegt %x16 %x17 -> %p8.b",
        "whilegt %x21 %x22 -> %p10.b", "whilegt %x30 %x30 -> %p15.b",
    };
    TEST_LOOP(whilegt, whilegt_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    const char *const expected_0_2[6] = {
        "whilegt %w0 %w0 -> %p0.h",    "whilegt %w6 %w7 -> %p2.h",
        "whilegt %w11 %w12 -> %p5.h",  "whilegt %w16 %w17 -> %p8.h",
        "whilegt %w21 %w22 -> %p10.h", "whilegt %w30 %w30 -> %p15.h",
    };
    TEST_LOOP(whilegt, whilegt_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_reg(Wn_six_offset_1[i]), opnd_create_reg(Wn_six_offset_2[i]));

    const char *const expected_0_3[6] = {
        "whilegt %x0 %x0 -> %p0.h",    "whilegt %x6 %x7 -> %p2.h",
        "whilegt %x11 %x12 -> %p5.h",  "whilegt %x16 %x17 -> %p8.h",
        "whilegt %x21 %x22 -> %p10.h", "whilegt %x30 %x30 -> %p15.h",
    };
    TEST_LOOP(whilegt, whilegt_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    const char *const expected_0_4[6] = {
        "whilegt %w0 %w0 -> %p0.s",    "whilegt %w6 %w7 -> %p2.s",
        "whilegt %w11 %w12 -> %p5.s",  "whilegt %w16 %w17 -> %p8.s",
        "whilegt %w21 %w22 -> %p10.s", "whilegt %w30 %w30 -> %p15.s",
    };
    TEST_LOOP(whilegt, whilegt_sve, 6, expected_0_4[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_reg(Wn_six_offset_1[i]), opnd_create_reg(Wn_six_offset_2[i]));

    const char *const expected_0_5[6] = {
        "whilegt %x0 %x0 -> %p0.s",    "whilegt %x6 %x7 -> %p2.s",
        "whilegt %x11 %x12 -> %p5.s",  "whilegt %x16 %x17 -> %p8.s",
        "whilegt %x21 %x22 -> %p10.s", "whilegt %x30 %x30 -> %p15.s",
    };
    TEST_LOOP(whilegt, whilegt_sve, 6, expected_0_5[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    const char *const expected_0_6[6] = {
        "whilegt %w0 %w0 -> %p0.d",    "whilegt %w6 %w7 -> %p2.d",
        "whilegt %w11 %w12 -> %p5.d",  "whilegt %w16 %w17 -> %p8.d",
        "whilegt %w21 %w22 -> %p10.d", "whilegt %w30 %w30 -> %p15.d",
    };
    TEST_LOOP(whilegt, whilegt_sve, 6, expected_0_6[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_reg(Wn_six_offset_1[i]), opnd_create_reg(Wn_six_offset_2[i]));

    const char *const expected_0_7[6] = {
        "whilegt %x0 %x0 -> %p0.d",    "whilegt %x6 %x7 -> %p2.d",
        "whilegt %x11 %x12 -> %p5.d",  "whilegt %x16 %x17 -> %p8.d",
        "whilegt %x21 %x22 -> %p10.d", "whilegt %x30 %x30 -> %p15.d",
    };
    TEST_LOOP(whilegt, whilegt_sve, 6, expected_0_7[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));
}

TEST_INSTR(whilehi_sve)
{

    /* Testing WHILEHI <Pd>.<Ts>, <R><n>, <R><m> */
    const char *const expected_0_0[6] = {
        "whilehi %w0 %w0 -> %p0.b",    "whilehi %w6 %w7 -> %p2.b",
        "whilehi %w11 %w12 -> %p5.b",  "whilehi %w16 %w17 -> %p8.b",
        "whilehi %w21 %w22 -> %p10.b", "whilehi %w30 %w30 -> %p15.b",
    };
    TEST_LOOP(whilehi, whilehi_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_reg(Wn_six_offset_1[i]), opnd_create_reg(Wn_six_offset_2[i]));

    const char *const expected_0_1[6] = {
        "whilehi %x0 %x0 -> %p0.b",    "whilehi %x6 %x7 -> %p2.b",
        "whilehi %x11 %x12 -> %p5.b",  "whilehi %x16 %x17 -> %p8.b",
        "whilehi %x21 %x22 -> %p10.b", "whilehi %x30 %x30 -> %p15.b",
    };
    TEST_LOOP(whilehi, whilehi_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    const char *const expected_0_2[6] = {
        "whilehi %w0 %w0 -> %p0.h",    "whilehi %w6 %w7 -> %p2.h",
        "whilehi %w11 %w12 -> %p5.h",  "whilehi %w16 %w17 -> %p8.h",
        "whilehi %w21 %w22 -> %p10.h", "whilehi %w30 %w30 -> %p15.h",
    };
    TEST_LOOP(whilehi, whilehi_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_reg(Wn_six_offset_1[i]), opnd_create_reg(Wn_six_offset_2[i]));

    const char *const expected_0_3[6] = {
        "whilehi %x0 %x0 -> %p0.h",    "whilehi %x6 %x7 -> %p2.h",
        "whilehi %x11 %x12 -> %p5.h",  "whilehi %x16 %x17 -> %p8.h",
        "whilehi %x21 %x22 -> %p10.h", "whilehi %x30 %x30 -> %p15.h",
    };
    TEST_LOOP(whilehi, whilehi_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    const char *const expected_0_4[6] = {
        "whilehi %w0 %w0 -> %p0.s",    "whilehi %w6 %w7 -> %p2.s",
        "whilehi %w11 %w12 -> %p5.s",  "whilehi %w16 %w17 -> %p8.s",
        "whilehi %w21 %w22 -> %p10.s", "whilehi %w30 %w30 -> %p15.s",
    };
    TEST_LOOP(whilehi, whilehi_sve, 6, expected_0_4[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_reg(Wn_six_offset_1[i]), opnd_create_reg(Wn_six_offset_2[i]));

    const char *const expected_0_5[6] = {
        "whilehi %x0 %x0 -> %p0.s",    "whilehi %x6 %x7 -> %p2.s",
        "whilehi %x11 %x12 -> %p5.s",  "whilehi %x16 %x17 -> %p8.s",
        "whilehi %x21 %x22 -> %p10.s", "whilehi %x30 %x30 -> %p15.s",
    };
    TEST_LOOP(whilehi, whilehi_sve, 6, expected_0_5[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    const char *const expected_0_6[6] = {
        "whilehi %w0 %w0 -> %p0.d",    "whilehi %w6 %w7 -> %p2.d",
        "whilehi %w11 %w12 -> %p5.d",  "whilehi %w16 %w17 -> %p8.d",
        "whilehi %w21 %w22 -> %p10.d", "whilehi %w30 %w30 -> %p15.d",
    };
    TEST_LOOP(whilehi, whilehi_sve, 6, expected_0_6[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_reg(Wn_six_offset_1[i]), opnd_create_reg(Wn_six_offset_2[i]));

    const char *const expected_0_7[6] = {
        "whilehi %x0 %x0 -> %p0.d",    "whilehi %x6 %x7 -> %p2.d",
        "whilehi %x11 %x12 -> %p5.d",  "whilehi %x16 %x17 -> %p8.d",
        "whilehi %x21 %x22 -> %p10.d", "whilehi %x30 %x30 -> %p15.d",
    };
    TEST_LOOP(whilehi, whilehi_sve, 6, expected_0_7[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));
}

TEST_INSTR(whilehs_sve)
{

    /* Testing WHILEHS <Pd>.<Ts>, <R><n>, <R><m> */
    const char *const expected_0_0[6] = {
        "whilehs %w0 %w0 -> %p0.b",    "whilehs %w6 %w7 -> %p2.b",
        "whilehs %w11 %w12 -> %p5.b",  "whilehs %w16 %w17 -> %p8.b",
        "whilehs %w21 %w22 -> %p10.b", "whilehs %w30 %w30 -> %p15.b",
    };
    TEST_LOOP(whilehs, whilehs_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_reg(Wn_six_offset_1[i]), opnd_create_reg(Wn_six_offset_2[i]));

    const char *const expected_0_1[6] = {
        "whilehs %x0 %x0 -> %p0.b",    "whilehs %x6 %x7 -> %p2.b",
        "whilehs %x11 %x12 -> %p5.b",  "whilehs %x16 %x17 -> %p8.b",
        "whilehs %x21 %x22 -> %p10.b", "whilehs %x30 %x30 -> %p15.b",
    };
    TEST_LOOP(whilehs, whilehs_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    const char *const expected_0_2[6] = {
        "whilehs %w0 %w0 -> %p0.h",    "whilehs %w6 %w7 -> %p2.h",
        "whilehs %w11 %w12 -> %p5.h",  "whilehs %w16 %w17 -> %p8.h",
        "whilehs %w21 %w22 -> %p10.h", "whilehs %w30 %w30 -> %p15.h",
    };
    TEST_LOOP(whilehs, whilehs_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_reg(Wn_six_offset_1[i]), opnd_create_reg(Wn_six_offset_2[i]));

    const char *const expected_0_3[6] = {
        "whilehs %x0 %x0 -> %p0.h",    "whilehs %x6 %x7 -> %p2.h",
        "whilehs %x11 %x12 -> %p5.h",  "whilehs %x16 %x17 -> %p8.h",
        "whilehs %x21 %x22 -> %p10.h", "whilehs %x30 %x30 -> %p15.h",
    };
    TEST_LOOP(whilehs, whilehs_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    const char *const expected_0_4[6] = {
        "whilehs %w0 %w0 -> %p0.s",    "whilehs %w6 %w7 -> %p2.s",
        "whilehs %w11 %w12 -> %p5.s",  "whilehs %w16 %w17 -> %p8.s",
        "whilehs %w21 %w22 -> %p10.s", "whilehs %w30 %w30 -> %p15.s",
    };
    TEST_LOOP(whilehs, whilehs_sve, 6, expected_0_4[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_reg(Wn_six_offset_1[i]), opnd_create_reg(Wn_six_offset_2[i]));

    const char *const expected_0_5[6] = {
        "whilehs %x0 %x0 -> %p0.s",    "whilehs %x6 %x7 -> %p2.s",
        "whilehs %x11 %x12 -> %p5.s",  "whilehs %x16 %x17 -> %p8.s",
        "whilehs %x21 %x22 -> %p10.s", "whilehs %x30 %x30 -> %p15.s",
    };
    TEST_LOOP(whilehs, whilehs_sve, 6, expected_0_5[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    const char *const expected_0_6[6] = {
        "whilehs %w0 %w0 -> %p0.d",    "whilehs %w6 %w7 -> %p2.d",
        "whilehs %w11 %w12 -> %p5.d",  "whilehs %w16 %w17 -> %p8.d",
        "whilehs %w21 %w22 -> %p10.d", "whilehs %w30 %w30 -> %p15.d",
    };
    TEST_LOOP(whilehs, whilehs_sve, 6, expected_0_6[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_reg(Wn_six_offset_1[i]), opnd_create_reg(Wn_six_offset_2[i]));

    const char *const expected_0_7[6] = {
        "whilehs %x0 %x0 -> %p0.d",    "whilehs %x6 %x7 -> %p2.d",
        "whilehs %x11 %x12 -> %p5.d",  "whilehs %x16 %x17 -> %p8.d",
        "whilehs %x21 %x22 -> %p10.d", "whilehs %x30 %x30 -> %p15.d",
    };
    TEST_LOOP(whilehs, whilehs_sve, 6, expected_0_7[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));
}

TEST_INSTR(whilerw_sve)
{

    /* Testing WHILERW <Pd>.<Ts>, <Xn>, <Xm> */
    const char *const expected_0_0[6] = {
        "whilerw %x0 %x0 -> %p0.b",    "whilerw %x6 %x7 -> %p2.b",
        "whilerw %x11 %x12 -> %p5.b",  "whilerw %x16 %x17 -> %p8.b",
        "whilerw %x21 %x22 -> %p10.b", "whilerw %x30 %x30 -> %p15.b",
    };
    TEST_LOOP(whilerw, whilerw_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    const char *const expected_0_1[6] = {
        "whilerw %x0 %x0 -> %p0.h",    "whilerw %x6 %x7 -> %p2.h",
        "whilerw %x11 %x12 -> %p5.h",  "whilerw %x16 %x17 -> %p8.h",
        "whilerw %x21 %x22 -> %p10.h", "whilerw %x30 %x30 -> %p15.h",
    };
    TEST_LOOP(whilerw, whilerw_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    const char *const expected_0_2[6] = {
        "whilerw %x0 %x0 -> %p0.s",    "whilerw %x6 %x7 -> %p2.s",
        "whilerw %x11 %x12 -> %p5.s",  "whilerw %x16 %x17 -> %p8.s",
        "whilerw %x21 %x22 -> %p10.s", "whilerw %x30 %x30 -> %p15.s",
    };
    TEST_LOOP(whilerw, whilerw_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    const char *const expected_0_3[6] = {
        "whilerw %x0 %x0 -> %p0.d",    "whilerw %x6 %x7 -> %p2.d",
        "whilerw %x11 %x12 -> %p5.d",  "whilerw %x16 %x17 -> %p8.d",
        "whilerw %x21 %x22 -> %p10.d", "whilerw %x30 %x30 -> %p15.d",
    };
    TEST_LOOP(whilerw, whilerw_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));
}

TEST_INSTR(whilewr_sve)
{

    /* Testing WHILEWR <Pd>.<Ts>, <Xn>, <Xm> */
    const char *const expected_0_0[6] = {
        "whilewr %x0 %x0 -> %p0.b",    "whilewr %x6 %x7 -> %p2.b",
        "whilewr %x11 %x12 -> %p5.b",  "whilewr %x16 %x17 -> %p8.b",
        "whilewr %x21 %x22 -> %p10.b", "whilewr %x30 %x30 -> %p15.b",
    };
    TEST_LOOP(whilewr, whilewr_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    const char *const expected_0_1[6] = {
        "whilewr %x0 %x0 -> %p0.h",    "whilewr %x6 %x7 -> %p2.h",
        "whilewr %x11 %x12 -> %p5.h",  "whilewr %x16 %x17 -> %p8.h",
        "whilewr %x21 %x22 -> %p10.h", "whilewr %x30 %x30 -> %p15.h",
    };
    TEST_LOOP(whilewr, whilewr_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    const char *const expected_0_2[6] = {
        "whilewr %x0 %x0 -> %p0.s",    "whilewr %x6 %x7 -> %p2.s",
        "whilewr %x11 %x12 -> %p5.s",  "whilewr %x16 %x17 -> %p8.s",
        "whilewr %x21 %x22 -> %p10.s", "whilewr %x30 %x30 -> %p15.s",
    };
    TEST_LOOP(whilewr, whilewr_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));

    const char *const expected_0_3[6] = {
        "whilewr %x0 %x0 -> %p0.d",    "whilewr %x6 %x7 -> %p2.d",
        "whilewr %x11 %x12 -> %p5.d",  "whilewr %x16 %x17 -> %p8.d",
        "whilewr %x21 %x22 -> %p10.d", "whilewr %x30 %x30 -> %p15.d",
    };
    TEST_LOOP(whilewr, whilewr_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_reg(Xn_six_offset_1[i]), opnd_create_reg(Xn_six_offset_2[i]));
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

    RUN_INSTR_TEST(adclb_sve);
    RUN_INSTR_TEST(adclt_sve);
    RUN_INSTR_TEST(bdep_sve);
    RUN_INSTR_TEST(bext_sve);
    RUN_INSTR_TEST(bgrp_sve);
    RUN_INSTR_TEST(eorbt_sve);
    RUN_INSTR_TEST(eortb_sve);
    RUN_INSTR_TEST(saba_sve);
    RUN_INSTR_TEST(sbclb_sve);
    RUN_INSTR_TEST(sbclt_sve);
    RUN_INSTR_TEST(sqdmulh_sve);
    RUN_INSTR_TEST(sqrdmlah_sve);
    RUN_INSTR_TEST(sqrdmlsh_sve);
    RUN_INSTR_TEST(sqrdmulh_sve);
    RUN_INSTR_TEST(tbx_sve);
    RUN_INSTR_TEST(uaba_sve);

    RUN_INSTR_TEST(addhnb_sve);
    RUN_INSTR_TEST(addhnt_sve);
    RUN_INSTR_TEST(pmullb_sve);
    RUN_INSTR_TEST(pmullt_sve);
    RUN_INSTR_TEST(raddhnb_sve);
    RUN_INSTR_TEST(raddhnt_sve);
    RUN_INSTR_TEST(rsubhnb_sve);
    RUN_INSTR_TEST(rsubhnt_sve);
    RUN_INSTR_TEST(sabalb_sve);
    RUN_INSTR_TEST(sabalt_sve);
    RUN_INSTR_TEST(sabdlb_sve);
    RUN_INSTR_TEST(sabdlt_sve);
    RUN_INSTR_TEST(saddlb_sve);
    RUN_INSTR_TEST(saddlbt_sve);
    RUN_INSTR_TEST(saddlt_sve);
    RUN_INSTR_TEST(saddwb_sve);
    RUN_INSTR_TEST(saddwt_sve);
    RUN_INSTR_TEST(smlalb_sve);
    RUN_INSTR_TEST(smlalt_sve);
    RUN_INSTR_TEST(smlslb_sve);
    RUN_INSTR_TEST(smlslt_sve);
    RUN_INSTR_TEST(smullb_sve);
    RUN_INSTR_TEST(smullt_sve);
    RUN_INSTR_TEST(sqdmlalb_sve);
    RUN_INSTR_TEST(sqdmlalbt_sve);
    RUN_INSTR_TEST(sqdmlalt_sve);
    RUN_INSTR_TEST(sqdmlslb_sve);
    RUN_INSTR_TEST(sqdmlslbt_sve);
    RUN_INSTR_TEST(sqdmlslt_sve);
    RUN_INSTR_TEST(sqdmullb_sve);
    RUN_INSTR_TEST(sqdmullt_sve);
    RUN_INSTR_TEST(ssublb_sve);
    RUN_INSTR_TEST(ssublbt_sve);
    RUN_INSTR_TEST(ssublt_sve);
    RUN_INSTR_TEST(ssubltb_sve);
    RUN_INSTR_TEST(ssubwb_sve);
    RUN_INSTR_TEST(ssubwt_sve);
    RUN_INSTR_TEST(subhnb_sve);
    RUN_INSTR_TEST(subhnt_sve);
    RUN_INSTR_TEST(uabalb_sve);
    RUN_INSTR_TEST(uabalt_sve);
    RUN_INSTR_TEST(uabdlb_sve);
    RUN_INSTR_TEST(uabdlt_sve);
    RUN_INSTR_TEST(uaddlb_sve);
    RUN_INSTR_TEST(uaddlt_sve);
    RUN_INSTR_TEST(uaddwb_sve);
    RUN_INSTR_TEST(uaddwt_sve);
    RUN_INSTR_TEST(umlalb_sve);
    RUN_INSTR_TEST(umlalt_sve);
    RUN_INSTR_TEST(umlslb_sve);
    RUN_INSTR_TEST(umlslt_sve);
    RUN_INSTR_TEST(umullb_sve);
    RUN_INSTR_TEST(umullt_sve);
    RUN_INSTR_TEST(usublb_sve);
    RUN_INSTR_TEST(usublt_sve);
    RUN_INSTR_TEST(usubwb_sve);
    RUN_INSTR_TEST(usubwt_sve);

    RUN_INSTR_TEST(aesimc_sve);
    RUN_INSTR_TEST(aesmc_sve);
    RUN_INSTR_TEST(sqxtnb_sve);
    RUN_INSTR_TEST(sqxtnt_sve);
    RUN_INSTR_TEST(sqxtunb_sve);
    RUN_INSTR_TEST(sqxtunt_sve);
    RUN_INSTR_TEST(uqxtnb_sve);
    RUN_INSTR_TEST(uqxtnt_sve);

    RUN_INSTR_TEST(fmlalb_sve_idx);
    RUN_INSTR_TEST(fmlalt_sve_idx);
    RUN_INSTR_TEST(fmlslb_sve_idx);
    RUN_INSTR_TEST(fmlslt_sve_idx);
    RUN_INSTR_TEST(smlalb_sve_idx_vector);
    RUN_INSTR_TEST(smlalt_sve_idx_vector);
    RUN_INSTR_TEST(smlslb_sve_idx_vector);
    RUN_INSTR_TEST(smlslt_sve_idx_vector);
    RUN_INSTR_TEST(smullb_sve_idx_vector);
    RUN_INSTR_TEST(smullt_sve_idx_vector);
    RUN_INSTR_TEST(sqdmlalb_sve_idx_vector);
    RUN_INSTR_TEST(sqdmlalt_sve_idx_vector);
    RUN_INSTR_TEST(sqdmlslb_sve_idx_vector);
    RUN_INSTR_TEST(sqdmlslt_sve_idx_vector);
    RUN_INSTR_TEST(sqdmulh_sve_idx);
    RUN_INSTR_TEST(sqdmullb_sve_idx_vector);
    RUN_INSTR_TEST(sqdmullt_sve_idx_vector);
    RUN_INSTR_TEST(sqrdmlah_sve_idx);
    RUN_INSTR_TEST(sqrdmlsh_sve_idx);
    RUN_INSTR_TEST(sqrdmulh_sve_idx);
    RUN_INSTR_TEST(umlalb_sve_idx_vector);
    RUN_INSTR_TEST(umlalt_sve_idx_vector);
    RUN_INSTR_TEST(umlslb_sve_idx_vector);
    RUN_INSTR_TEST(umlslt_sve_idx_vector);
    RUN_INSTR_TEST(umullb_sve_idx_vector);
    RUN_INSTR_TEST(umullt_sve_idx_vector);

    RUN_INSTR_TEST(addp_sve_pred);
    RUN_INSTR_TEST(faddp_sve_pred);
    RUN_INSTR_TEST(fmaxnmp_sve_pred);
    RUN_INSTR_TEST(fmaxp_sve_pred);
    RUN_INSTR_TEST(fminnmp_sve_pred);
    RUN_INSTR_TEST(fminp_sve_pred);
    RUN_INSTR_TEST(histcnt_sve_pred);
    RUN_INSTR_TEST(shadd_sve_pred);
    RUN_INSTR_TEST(shsub_sve_pred);
    RUN_INSTR_TEST(shsubr_sve_pred);
    RUN_INSTR_TEST(smaxp_sve_pred);
    RUN_INSTR_TEST(sminp_sve_pred);
    RUN_INSTR_TEST(sqrshl_sve_pred);
    RUN_INSTR_TEST(sqrshlr_sve_pred);
    RUN_INSTR_TEST(sqshl_sve_pred);
    RUN_INSTR_TEST(sqshlr_sve_pred);
    RUN_INSTR_TEST(sqsubr_sve_pred);
    RUN_INSTR_TEST(srhadd_sve_pred);
    RUN_INSTR_TEST(srshl_sve_pred);
    RUN_INSTR_TEST(srshlr_sve_pred);
    RUN_INSTR_TEST(suqadd_sve_pred);
    RUN_INSTR_TEST(uhadd_sve_pred);
    RUN_INSTR_TEST(uhsub_sve_pred);
    RUN_INSTR_TEST(uhsubr_sve_pred);
    RUN_INSTR_TEST(umaxp_sve_pred);
    RUN_INSTR_TEST(uminp_sve_pred);
    RUN_INSTR_TEST(uqrshl_sve_pred);
    RUN_INSTR_TEST(uqrshlr_sve_pred);
    RUN_INSTR_TEST(uqshl_sve_pred);
    RUN_INSTR_TEST(uqshlr_sve_pred);
    RUN_INSTR_TEST(uqsubr_sve_pred);
    RUN_INSTR_TEST(urhadd_sve_pred);
    RUN_INSTR_TEST(urshl_sve_pred);
    RUN_INSTR_TEST(urshlr_sve_pred);
    RUN_INSTR_TEST(usqadd_sve_pred);

    RUN_INSTR_TEST(fcvtlt_sve_pred);
    RUN_INSTR_TEST(fcvtnt_sve_pred);
    RUN_INSTR_TEST(fcvtx_sve_pred);
    RUN_INSTR_TEST(fcvtxnt_sve_pred);
    RUN_INSTR_TEST(flogb_sve_pred);
    RUN_INSTR_TEST(sadalp_sve_pred);
    RUN_INSTR_TEST(sqabs_sve_pred);
    RUN_INSTR_TEST(sqneg_sve_pred);
    RUN_INSTR_TEST(uadalp_sve_pred);

    RUN_INSTR_TEST(cadd_sve);
    RUN_INSTR_TEST(cdot_sve);
    RUN_INSTR_TEST(cmla_sve);
    RUN_INSTR_TEST(rshrnb_sve);
    RUN_INSTR_TEST(rshrnt_sve);
    RUN_INSTR_TEST(shrnb_sve);
    RUN_INSTR_TEST(shrnt_sve);
    RUN_INSTR_TEST(sli_sve);
    RUN_INSTR_TEST(sqcadd_sve);
    RUN_INSTR_TEST(sqrdcmlah_sve);
    RUN_INSTR_TEST(sqrshrnb_sve);
    RUN_INSTR_TEST(sqrshrnt_sve);
    RUN_INSTR_TEST(sqrshrunb_sve);
    RUN_INSTR_TEST(sqrshrunt_sve);
    RUN_INSTR_TEST(sqshlu_sve_pred);
    RUN_INSTR_TEST(sqshrnb_sve);
    RUN_INSTR_TEST(sqshrnt_sve);
    RUN_INSTR_TEST(sqshrunb_sve);
    RUN_INSTR_TEST(sqshrunt_sve);
    RUN_INSTR_TEST(sri_sve);
    RUN_INSTR_TEST(srshr_sve_pred);
    RUN_INSTR_TEST(srsra_sve);
    RUN_INSTR_TEST(sshllb_sve);
    RUN_INSTR_TEST(sshllt_sve);
    RUN_INSTR_TEST(ssra_sve);
    RUN_INSTR_TEST(uqrshrnb_sve);
    RUN_INSTR_TEST(uqrshrnt_sve);
    RUN_INSTR_TEST(uqshrnb_sve);
    RUN_INSTR_TEST(uqshrnt_sve);
    RUN_INSTR_TEST(urshr_sve_pred);
    RUN_INSTR_TEST(ursra_sve);
    RUN_INSTR_TEST(ushllb_sve);
    RUN_INSTR_TEST(ushllt_sve);
    RUN_INSTR_TEST(usra_sve);
    RUN_INSTR_TEST(xar_sve);

    RUN_INSTR_TEST(ldnt1sb_sve_pred);
    RUN_INSTR_TEST(ldnt1sh_sve_pred);
    RUN_INSTR_TEST(ldnt1sw_sve_pred);

    RUN_INSTR_TEST(cdot_sve_idx_imm_vector);
    RUN_INSTR_TEST(cmla_sve_idx_imm_vector);
    RUN_INSTR_TEST(sqrdcmlah_sve_idx_imm_vector);

    RUN_INSTR_TEST(match_sve_pred);
    RUN_INSTR_TEST(nmatch_sve_pred);
    RUN_INSTR_TEST(urecpe_sve_pred);
    RUN_INSTR_TEST(ursqrte_sve_pred);
    RUN_INSTR_TEST(whilege_sve);
    RUN_INSTR_TEST(whilegt_sve);
    RUN_INSTR_TEST(whilehi_sve);
    RUN_INSTR_TEST(whilehs_sve);
    RUN_INSTR_TEST(whilerw_sve);
    RUN_INSTR_TEST(whilewr_sve);

    print("All SVE2 tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
