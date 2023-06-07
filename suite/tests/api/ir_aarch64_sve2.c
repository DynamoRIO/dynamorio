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

    print("All SVE2 tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
