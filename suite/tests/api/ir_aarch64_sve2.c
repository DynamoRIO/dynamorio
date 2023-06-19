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

    print("All SVE2 tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
