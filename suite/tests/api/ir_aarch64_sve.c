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

TEST_INSTR(add_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing ADD     <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P4, DR_REG_P5, DR_REG_P6 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_0[6] = {
        "add    %p0/m %z0.b %z0.b -> %z0.b",    "add    %p2/m %z5.b %z7.b -> %z5.b",
        "add    %p3/m %z10.b %z12.b -> %z10.b", "add    %p4/m %z15.b %z17.b -> %z15.b",
        "add    %p5/m %z20.b %z22.b -> %z20.b", "add    %p6/m %z30.b %z30.b -> %z30.b",
    };
    TEST_LOOP(add, add_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pg_0_0[i], true),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_1));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P4, DR_REG_P5, DR_REG_P6 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_1[6] = {
        "add    %p0/m %z0.h %z0.h -> %z0.h",    "add    %p2/m %z5.h %z7.h -> %z5.h",
        "add    %p3/m %z10.h %z12.h -> %z10.h", "add    %p4/m %z15.h %z17.h -> %z15.h",
        "add    %p5/m %z20.h %z22.h -> %z20.h", "add    %p6/m %z30.h %z30.h -> %z30.h",
    };
    TEST_LOOP(add, add_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_predicate_reg(Pg_0_1[i], true),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_2));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_0_2[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P4, DR_REG_P5, DR_REG_P6 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_2[6] = {
        "add    %p0/m %z0.s %z0.s -> %z0.s",    "add    %p2/m %z5.s %z7.s -> %z5.s",
        "add    %p3/m %z10.s %z12.s -> %z10.s", "add    %p4/m %z15.s %z17.s -> %z15.s",
        "add    %p5/m %z20.s %z22.s -> %z20.s", "add    %p6/m %z30.s %z30.s -> %z30.s",
    };
    TEST_LOOP(add, add_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_2[i], true),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_4));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_0_3[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P4, DR_REG_P5, DR_REG_P6 };
    reg_id_t Zm_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_3[6] = {
        "add    %p0/m %z0.d %z0.d -> %z0.d",    "add    %p2/m %z5.d %z7.d -> %z5.d",
        "add    %p3/m %z10.d %z12.d -> %z10.d", "add    %p4/m %z15.d %z17.d -> %z15.d",
        "add    %p5/m %z20.d %z22.d -> %z20.d", "add    %p6/m %z30.d %z30.d -> %z30.d",
    };
    TEST_LOOP(add, add_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(add_sve_shift)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing ADD     <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm>, <shift> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_0[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_0[6] = { 0, 0, 0, 0, 0, 0 };
    const char *expected_0_0[6] = {
        "add    %z0.b $0x00 lsl $0x00 -> %z0.b",
        "add    %z5.b $0x2b lsl $0x00 -> %z5.b",
        "add    %z10.b $0x56 lsl $0x00 -> %z10.b",
        "add    %z15.b $0x81 lsl $0x00 -> %z15.b",
        "add    %z20.b $0xab lsl $0x00 -> %z20.b",
        "add    %z30.b $0xff lsl $0x00 -> %z30.b",
    };
    TEST_LOOP(add, add_sve_shift, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_immed_uint(imm8_0_0[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_0[i], OPSZ_1b));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_1[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_1[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_1[6] = {
        "add    %z0.h $0x00 lsl $0x08 -> %z0.h",
        "add    %z5.h $0x2b lsl $0x08 -> %z5.h",
        "add    %z10.h $0x56 lsl $0x08 -> %z10.h",
        "add    %z15.h $0x81 lsl $0x08 -> %z15.h",
        "add    %z20.h $0xab lsl $0x00 -> %z20.h",
        "add    %z30.h $0xff lsl $0x00 -> %z30.h",
    };
    TEST_LOOP(add, add_sve_shift, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_immed_uint(imm8_0_1[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_1[i], OPSZ_1b));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_2[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_2[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_2[6] = {
        "add    %z0.s $0x00 lsl $0x08 -> %z0.s",
        "add    %z5.s $0x2b lsl $0x08 -> %z5.s",
        "add    %z10.s $0x56 lsl $0x08 -> %z10.s",
        "add    %z15.s $0x81 lsl $0x08 -> %z15.s",
        "add    %z20.s $0xab lsl $0x00 -> %z20.s",
        "add    %z30.s $0xff lsl $0x00 -> %z30.s",
    };
    TEST_LOOP(add, add_sve_shift, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_immed_uint(imm8_0_2[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_2[i], OPSZ_1b));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_3[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_3[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_3[6] = {
        "add    %z0.d $0x00 lsl $0x08 -> %z0.d",
        "add    %z5.d $0x2b lsl $0x08 -> %z5.d",
        "add    %z10.d $0x56 lsl $0x08 -> %z10.d",
        "add    %z15.d $0x81 lsl $0x08 -> %z15.d",
        "add    %z20.d $0xab lsl $0x00 -> %z20.d",
        "add    %z30.d $0xff lsl $0x00 -> %z30.d",
    };
    TEST_LOOP(add, add_sve_shift, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_immed_uint(imm8_0_3[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_3[i], OPSZ_1b));

    return success;
}

TEST_INSTR(add_sve)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing ADD     <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zd_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_0[6] = {
        "add    %z0.b %z0.b -> %z0.b",    "add    %z6.b %z7.b -> %z5.b",
        "add    %z11.b %z12.b -> %z10.b", "add    %z16.b %z17.b -> %z15.b",
        "add    %z21.b %z22.b -> %z20.b", "add    %z30.b %z30.b -> %z30.b",
    };
    TEST_LOOP(add, add_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zd_0_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_0_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_1));

    reg_id_t Zd_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_1[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_1[6] = {
        "add    %z0.h %z0.h -> %z0.h",    "add    %z6.h %z7.h -> %z5.h",
        "add    %z11.h %z12.h -> %z10.h", "add    %z16.h %z17.h -> %z15.h",
        "add    %z21.h %z22.h -> %z20.h", "add    %z30.h %z30.h -> %z30.h",
    };
    TEST_LOOP(add, add_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zd_0_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_0_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_2));

    reg_id_t Zd_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_2[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_2[6] = {
        "add    %z0.s %z0.s -> %z0.s",    "add    %z6.s %z7.s -> %z5.s",
        "add    %z11.s %z12.s -> %z10.s", "add    %z16.s %z17.s -> %z15.s",
        "add    %z21.s %z22.s -> %z20.s", "add    %z30.s %z30.s -> %z30.s",
    };
    TEST_LOOP(add, add_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zd_0_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_0_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_4));

    reg_id_t Zd_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_3[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_3[6] = {
        "add    %z0.d %z0.d -> %z0.d",    "add    %z6.d %z7.d -> %z5.d",
        "add    %z11.d %z12.d -> %z10.d", "add    %z16.d %z17.d -> %z15.d",
        "add    %z21.d %z22.d -> %z20.d", "add    %z30.d %z30.d -> %z30.d",
    };
    TEST_LOOP(add, add_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zd_0_3[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_0_3[i], OPSZ_8),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(zip2_vector)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing ZIP2    <Zd>.Q, <Zn>.Q, <Zm>.Q */
    reg_id_t Zd_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_0[6] = {
        "zip2   %z0.q %z0.q -> %z0.q",    "zip2   %z6.q %z7.q -> %z5.q",
        "zip2   %z11.q %z12.q -> %z10.q", "zip2   %z16.q %z17.q -> %z15.q",
        "zip2   %z21.q %z22.q -> %z20.q", "zip2   %z30.q %z30.q -> %z30.q",
    };
    TEST_LOOP(zip2, zip2_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zd_0_0[i], OPSZ_16),
              opnd_create_reg_element_vector(Zn_0_0[i], OPSZ_16),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_16));

    return success;
}

TEST_INSTR(movprfx_vector)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing MOVPRFX <Zd>, <Zn> */
    reg_id_t Zd_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    const char *expected_0_0[6] = {
        "movprfx %z0 -> %z0",   "movprfx %z6 -> %z5",   "movprfx %z11 -> %z10",
        "movprfx %z16 -> %z15", "movprfx %z21 -> %z20", "movprfx %z30 -> %z30",
    };
    TEST_LOOP(movprfx, movprfx_vector, 6, expected_0_0[i], opnd_create_reg(Zd_0_0[i]),
              opnd_create_reg(Zn_0_0[i]));

    return success;
}

TEST_INSTR(sqadd_sve_shift)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing SQADD   <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm>, <shift> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_0[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_0[6] = { 0, 0, 0, 0, 0, 0 };
    const char *expected_0_0[6] = {
        "sqadd  %z0.b $0x00 lsl $0x00 -> %z0.b",
        "sqadd  %z5.b $0x2b lsl $0x00 -> %z5.b",
        "sqadd  %z10.b $0x56 lsl $0x00 -> %z10.b",
        "sqadd  %z15.b $0x81 lsl $0x00 -> %z15.b",
        "sqadd  %z20.b $0xab lsl $0x00 -> %z20.b",
        "sqadd  %z30.b $0xff lsl $0x00 -> %z30.b",
    };
    TEST_LOOP(sqadd, sqadd_sve_shift, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_immed_uint(imm8_0_0[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_0[i], OPSZ_1b));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_1[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_1[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_1[6] = {
        "sqadd  %z0.h $0x00 lsl $0x08 -> %z0.h",
        "sqadd  %z5.h $0x2b lsl $0x08 -> %z5.h",
        "sqadd  %z10.h $0x56 lsl $0x08 -> %z10.h",
        "sqadd  %z15.h $0x81 lsl $0x08 -> %z15.h",
        "sqadd  %z20.h $0xab lsl $0x00 -> %z20.h",
        "sqadd  %z30.h $0xff lsl $0x00 -> %z30.h",
    };
    TEST_LOOP(sqadd, sqadd_sve_shift, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_immed_uint(imm8_0_1[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_1[i], OPSZ_1b));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_2[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_2[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_2[6] = {
        "sqadd  %z0.s $0x00 lsl $0x08 -> %z0.s",
        "sqadd  %z5.s $0x2b lsl $0x08 -> %z5.s",
        "sqadd  %z10.s $0x56 lsl $0x08 -> %z10.s",
        "sqadd  %z15.s $0x81 lsl $0x08 -> %z15.s",
        "sqadd  %z20.s $0xab lsl $0x00 -> %z20.s",
        "sqadd  %z30.s $0xff lsl $0x00 -> %z30.s",
    };
    TEST_LOOP(sqadd, sqadd_sve_shift, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_immed_uint(imm8_0_2[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_2[i], OPSZ_1b));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_3[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_3[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_3[6] = {
        "sqadd  %z0.d $0x00 lsl $0x08 -> %z0.d",
        "sqadd  %z5.d $0x2b lsl $0x08 -> %z5.d",
        "sqadd  %z10.d $0x56 lsl $0x08 -> %z10.d",
        "sqadd  %z15.d $0x81 lsl $0x08 -> %z15.d",
        "sqadd  %z20.d $0xab lsl $0x00 -> %z20.d",
        "sqadd  %z30.d $0xff lsl $0x00 -> %z30.d",
    };
    TEST_LOOP(sqadd, sqadd_sve_shift, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_immed_uint(imm8_0_3[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_3[i], OPSZ_1b));

    return success;
}

TEST_INSTR(sqadd_sve)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing SQADD   <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zd_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_0[6] = {
        "sqadd  %z0.b %z0.b -> %z0.b",    "sqadd  %z6.b %z7.b -> %z5.b",
        "sqadd  %z11.b %z12.b -> %z10.b", "sqadd  %z16.b %z17.b -> %z15.b",
        "sqadd  %z21.b %z22.b -> %z20.b", "sqadd  %z30.b %z30.b -> %z30.b",
    };
    TEST_LOOP(sqadd, sqadd_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zd_0_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_0_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_1));

    reg_id_t Zd_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_1[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_1[6] = {
        "sqadd  %z0.h %z0.h -> %z0.h",    "sqadd  %z6.h %z7.h -> %z5.h",
        "sqadd  %z11.h %z12.h -> %z10.h", "sqadd  %z16.h %z17.h -> %z15.h",
        "sqadd  %z21.h %z22.h -> %z20.h", "sqadd  %z30.h %z30.h -> %z30.h",
    };
    TEST_LOOP(sqadd, sqadd_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zd_0_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_0_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_2));

    reg_id_t Zd_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_2[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_2[6] = {
        "sqadd  %z0.s %z0.s -> %z0.s",    "sqadd  %z6.s %z7.s -> %z5.s",
        "sqadd  %z11.s %z12.s -> %z10.s", "sqadd  %z16.s %z17.s -> %z15.s",
        "sqadd  %z21.s %z22.s -> %z20.s", "sqadd  %z30.s %z30.s -> %z30.s",
    };
    TEST_LOOP(sqadd, sqadd_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zd_0_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_0_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_4));

    reg_id_t Zd_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_3[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_3[6] = {
        "sqadd  %z0.d %z0.d -> %z0.d",    "sqadd  %z6.d %z7.d -> %z5.d",
        "sqadd  %z11.d %z12.d -> %z10.d", "sqadd  %z16.d %z17.d -> %z15.d",
        "sqadd  %z21.d %z22.d -> %z20.d", "sqadd  %z30.d %z30.d -> %z30.d",
    };
    TEST_LOOP(sqadd, sqadd_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zd_0_3[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_0_3[i], OPSZ_8),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(sqsub_sve_shift)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing SQSUB   <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm>, <shift> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_0[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_0[6] = { 0, 0, 0, 0, 0, 0 };
    const char *expected_0_0[6] = {
        "sqsub  %z0.b $0x00 lsl $0x00 -> %z0.b",
        "sqsub  %z5.b $0x2b lsl $0x00 -> %z5.b",
        "sqsub  %z10.b $0x56 lsl $0x00 -> %z10.b",
        "sqsub  %z15.b $0x81 lsl $0x00 -> %z15.b",
        "sqsub  %z20.b $0xab lsl $0x00 -> %z20.b",
        "sqsub  %z30.b $0xff lsl $0x00 -> %z30.b",
    };
    TEST_LOOP(sqsub, sqsub_sve_shift, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_immed_uint(imm8_0_0[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_0[i], OPSZ_1b));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_1[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_1[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_1[6] = {
        "sqsub  %z0.h $0x00 lsl $0x08 -> %z0.h",
        "sqsub  %z5.h $0x2b lsl $0x08 -> %z5.h",
        "sqsub  %z10.h $0x56 lsl $0x08 -> %z10.h",
        "sqsub  %z15.h $0x81 lsl $0x08 -> %z15.h",
        "sqsub  %z20.h $0xab lsl $0x00 -> %z20.h",
        "sqsub  %z30.h $0xff lsl $0x00 -> %z30.h",
    };
    TEST_LOOP(sqsub, sqsub_sve_shift, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_immed_uint(imm8_0_1[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_1[i], OPSZ_1b));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_2[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_2[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_2[6] = {
        "sqsub  %z0.s $0x00 lsl $0x08 -> %z0.s",
        "sqsub  %z5.s $0x2b lsl $0x08 -> %z5.s",
        "sqsub  %z10.s $0x56 lsl $0x08 -> %z10.s",
        "sqsub  %z15.s $0x81 lsl $0x08 -> %z15.s",
        "sqsub  %z20.s $0xab lsl $0x00 -> %z20.s",
        "sqsub  %z30.s $0xff lsl $0x00 -> %z30.s",
    };
    TEST_LOOP(sqsub, sqsub_sve_shift, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_immed_uint(imm8_0_2[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_2[i], OPSZ_1b));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_3[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_3[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_3[6] = {
        "sqsub  %z0.d $0x00 lsl $0x08 -> %z0.d",
        "sqsub  %z5.d $0x2b lsl $0x08 -> %z5.d",
        "sqsub  %z10.d $0x56 lsl $0x08 -> %z10.d",
        "sqsub  %z15.d $0x81 lsl $0x08 -> %z15.d",
        "sqsub  %z20.d $0xab lsl $0x00 -> %z20.d",
        "sqsub  %z30.d $0xff lsl $0x00 -> %z30.d",
    };
    TEST_LOOP(sqsub, sqsub_sve_shift, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_immed_uint(imm8_0_3[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_3[i], OPSZ_1b));

    return success;
}

TEST_INSTR(sqsub_sve)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing SQSUB   <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zd_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_0[6] = {
        "sqsub  %z0.b %z0.b -> %z0.b",    "sqsub  %z6.b %z7.b -> %z5.b",
        "sqsub  %z11.b %z12.b -> %z10.b", "sqsub  %z16.b %z17.b -> %z15.b",
        "sqsub  %z21.b %z22.b -> %z20.b", "sqsub  %z30.b %z30.b -> %z30.b",
    };
    TEST_LOOP(sqsub, sqsub_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zd_0_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_0_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_1));

    reg_id_t Zd_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_1[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_1[6] = {
        "sqsub  %z0.h %z0.h -> %z0.h",    "sqsub  %z6.h %z7.h -> %z5.h",
        "sqsub  %z11.h %z12.h -> %z10.h", "sqsub  %z16.h %z17.h -> %z15.h",
        "sqsub  %z21.h %z22.h -> %z20.h", "sqsub  %z30.h %z30.h -> %z30.h",
    };
    TEST_LOOP(sqsub, sqsub_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zd_0_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_0_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_2));

    reg_id_t Zd_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_2[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_2[6] = {
        "sqsub  %z0.s %z0.s -> %z0.s",    "sqsub  %z6.s %z7.s -> %z5.s",
        "sqsub  %z11.s %z12.s -> %z10.s", "sqsub  %z16.s %z17.s -> %z15.s",
        "sqsub  %z21.s %z22.s -> %z20.s", "sqsub  %z30.s %z30.s -> %z30.s",
    };
    TEST_LOOP(sqsub, sqsub_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zd_0_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_0_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_4));

    reg_id_t Zd_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_3[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_3[6] = {
        "sqsub  %z0.d %z0.d -> %z0.d",    "sqsub  %z6.d %z7.d -> %z5.d",
        "sqsub  %z11.d %z12.d -> %z10.d", "sqsub  %z16.d %z17.d -> %z15.d",
        "sqsub  %z21.d %z22.d -> %z20.d", "sqsub  %z30.d %z30.d -> %z30.d",
    };
    TEST_LOOP(sqsub, sqsub_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zd_0_3[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_0_3[i], OPSZ_8),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(sub_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing SUB     <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P4, DR_REG_P5, DR_REG_P6 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_0[6] = {
        "sub    %p0/m %z0.b %z0.b -> %z0.b",    "sub    %p2/m %z5.b %z7.b -> %z5.b",
        "sub    %p3/m %z10.b %z12.b -> %z10.b", "sub    %p4/m %z15.b %z17.b -> %z15.b",
        "sub    %p5/m %z20.b %z22.b -> %z20.b", "sub    %p6/m %z30.b %z30.b -> %z30.b",
    };
    TEST_LOOP(sub, sub_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pg_0_0[i], true),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_1));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P4, DR_REG_P5, DR_REG_P6 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_1[6] = {
        "sub    %p0/m %z0.h %z0.h -> %z0.h",    "sub    %p2/m %z5.h %z7.h -> %z5.h",
        "sub    %p3/m %z10.h %z12.h -> %z10.h", "sub    %p4/m %z15.h %z17.h -> %z15.h",
        "sub    %p5/m %z20.h %z22.h -> %z20.h", "sub    %p6/m %z30.h %z30.h -> %z30.h",
    };
    TEST_LOOP(sub, sub_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_predicate_reg(Pg_0_1[i], true),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_2));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_0_2[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P4, DR_REG_P5, DR_REG_P6 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_2[6] = {
        "sub    %p0/m %z0.s %z0.s -> %z0.s",    "sub    %p2/m %z5.s %z7.s -> %z5.s",
        "sub    %p3/m %z10.s %z12.s -> %z10.s", "sub    %p4/m %z15.s %z17.s -> %z15.s",
        "sub    %p5/m %z20.s %z22.s -> %z20.s", "sub    %p6/m %z30.s %z30.s -> %z30.s",
    };
    TEST_LOOP(sub, sub_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_2[i], true),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_4));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_0_3[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P4, DR_REG_P5, DR_REG_P6 };
    reg_id_t Zm_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_3[6] = {
        "sub    %p0/m %z0.d %z0.d -> %z0.d",    "sub    %p2/m %z5.d %z7.d -> %z5.d",
        "sub    %p3/m %z10.d %z12.d -> %z10.d", "sub    %p4/m %z15.d %z17.d -> %z15.d",
        "sub    %p5/m %z20.d %z22.d -> %z20.d", "sub    %p6/m %z30.d %z30.d -> %z30.d",
    };
    TEST_LOOP(sub, sub_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(sub_sve_shift)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing SUB     <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm>, <shift> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_0[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_0[6] = { 0, 0, 0, 0, 0, 0 };
    const char *expected_0_0[6] = {
        "sub    %z0.b $0x00 lsl $0x00 -> %z0.b",
        "sub    %z5.b $0x2b lsl $0x00 -> %z5.b",
        "sub    %z10.b $0x56 lsl $0x00 -> %z10.b",
        "sub    %z15.b $0x81 lsl $0x00 -> %z15.b",
        "sub    %z20.b $0xab lsl $0x00 -> %z20.b",
        "sub    %z30.b $0xff lsl $0x00 -> %z30.b",
    };
    TEST_LOOP(sub, sub_sve_shift, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_immed_uint(imm8_0_0[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_0[i], OPSZ_1b));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_1[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_1[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_1[6] = {
        "sub    %z0.h $0x00 lsl $0x08 -> %z0.h",
        "sub    %z5.h $0x2b lsl $0x08 -> %z5.h",
        "sub    %z10.h $0x56 lsl $0x08 -> %z10.h",
        "sub    %z15.h $0x81 lsl $0x08 -> %z15.h",
        "sub    %z20.h $0xab lsl $0x00 -> %z20.h",
        "sub    %z30.h $0xff lsl $0x00 -> %z30.h",
    };
    TEST_LOOP(sub, sub_sve_shift, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_immed_uint(imm8_0_1[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_1[i], OPSZ_1b));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_2[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_2[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_2[6] = {
        "sub    %z0.s $0x00 lsl $0x08 -> %z0.s",
        "sub    %z5.s $0x2b lsl $0x08 -> %z5.s",
        "sub    %z10.s $0x56 lsl $0x08 -> %z10.s",
        "sub    %z15.s $0x81 lsl $0x08 -> %z15.s",
        "sub    %z20.s $0xab lsl $0x00 -> %z20.s",
        "sub    %z30.s $0xff lsl $0x00 -> %z30.s",
    };
    TEST_LOOP(sub, sub_sve_shift, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_immed_uint(imm8_0_2[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_2[i], OPSZ_1b));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_3[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_3[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_3[6] = {
        "sub    %z0.d $0x00 lsl $0x08 -> %z0.d",
        "sub    %z5.d $0x2b lsl $0x08 -> %z5.d",
        "sub    %z10.d $0x56 lsl $0x08 -> %z10.d",
        "sub    %z15.d $0x81 lsl $0x08 -> %z15.d",
        "sub    %z20.d $0xab lsl $0x00 -> %z20.d",
        "sub    %z30.d $0xff lsl $0x00 -> %z30.d",
    };
    TEST_LOOP(sub, sub_sve_shift, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_immed_uint(imm8_0_3[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_3[i], OPSZ_1b));

    return success;
}

TEST_INSTR(sub_sve)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing SUB     <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zd_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_0[6] = {
        "sub    %z0.b %z0.b -> %z0.b",    "sub    %z6.b %z7.b -> %z5.b",
        "sub    %z11.b %z12.b -> %z10.b", "sub    %z16.b %z17.b -> %z15.b",
        "sub    %z21.b %z22.b -> %z20.b", "sub    %z30.b %z30.b -> %z30.b",
    };
    TEST_LOOP(sub, sub_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zd_0_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_0_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_1));

    reg_id_t Zd_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_1[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_1[6] = {
        "sub    %z0.h %z0.h -> %z0.h",    "sub    %z6.h %z7.h -> %z5.h",
        "sub    %z11.h %z12.h -> %z10.h", "sub    %z16.h %z17.h -> %z15.h",
        "sub    %z21.h %z22.h -> %z20.h", "sub    %z30.h %z30.h -> %z30.h",
    };
    TEST_LOOP(sub, sub_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zd_0_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_0_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_2));

    reg_id_t Zd_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_2[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_2[6] = {
        "sub    %z0.s %z0.s -> %z0.s",    "sub    %z6.s %z7.s -> %z5.s",
        "sub    %z11.s %z12.s -> %z10.s", "sub    %z16.s %z17.s -> %z15.s",
        "sub    %z21.s %z22.s -> %z20.s", "sub    %z30.s %z30.s -> %z30.s",
    };
    TEST_LOOP(sub, sub_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zd_0_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_0_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_4));

    reg_id_t Zd_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_3[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_3[6] = {
        "sub    %z0.d %z0.d -> %z0.d",    "sub    %z6.d %z7.d -> %z5.d",
        "sub    %z11.d %z12.d -> %z10.d", "sub    %z16.d %z17.d -> %z15.d",
        "sub    %z21.d %z22.d -> %z20.d", "sub    %z30.d %z30.d -> %z30.d",
    };
    TEST_LOOP(sub, sub_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zd_0_3[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_0_3[i], OPSZ_8),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(subr_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing SUBR    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P4, DR_REG_P5, DR_REG_P6 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_0[6] = {
        "subr   %p0/m %z0.b %z0.b -> %z0.b",    "subr   %p2/m %z5.b %z7.b -> %z5.b",
        "subr   %p3/m %z10.b %z12.b -> %z10.b", "subr   %p4/m %z15.b %z17.b -> %z15.b",
        "subr   %p5/m %z20.b %z22.b -> %z20.b", "subr   %p6/m %z30.b %z30.b -> %z30.b",
    };
    TEST_LOOP(subr, subr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pg_0_0[i], true),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_1));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P4, DR_REG_P5, DR_REG_P6 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_1[6] = {
        "subr   %p0/m %z0.h %z0.h -> %z0.h",    "subr   %p2/m %z5.h %z7.h -> %z5.h",
        "subr   %p3/m %z10.h %z12.h -> %z10.h", "subr   %p4/m %z15.h %z17.h -> %z15.h",
        "subr   %p5/m %z20.h %z22.h -> %z20.h", "subr   %p6/m %z30.h %z30.h -> %z30.h",
    };
    TEST_LOOP(subr, subr_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_predicate_reg(Pg_0_1[i], true),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_2));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_0_2[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P4, DR_REG_P5, DR_REG_P6 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_2[6] = {
        "subr   %p0/m %z0.s %z0.s -> %z0.s",    "subr   %p2/m %z5.s %z7.s -> %z5.s",
        "subr   %p3/m %z10.s %z12.s -> %z10.s", "subr   %p4/m %z15.s %z17.s -> %z15.s",
        "subr   %p5/m %z20.s %z22.s -> %z20.s", "subr   %p6/m %z30.s %z30.s -> %z30.s",
    };
    TEST_LOOP(subr, subr_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_2[i], true),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_4));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_0_3[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P4, DR_REG_P5, DR_REG_P6 };
    reg_id_t Zm_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_3[6] = {
        "subr   %p0/m %z0.d %z0.d -> %z0.d",    "subr   %p2/m %z5.d %z7.d -> %z5.d",
        "subr   %p3/m %z10.d %z12.d -> %z10.d", "subr   %p4/m %z15.d %z17.d -> %z15.d",
        "subr   %p5/m %z20.d %z22.d -> %z20.d", "subr   %p6/m %z30.d %z30.d -> %z30.d",
    };
    TEST_LOOP(subr, subr_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(subr_sve_shift)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing SUBR    <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm>, <shift> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_0[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_0[6] = { 0, 0, 0, 0, 0, 0 };
    const char *expected_0_0[6] = {
        "subr   %z0.b $0x00 lsl $0x00 -> %z0.b",
        "subr   %z5.b $0x2b lsl $0x00 -> %z5.b",
        "subr   %z10.b $0x56 lsl $0x00 -> %z10.b",
        "subr   %z15.b $0x81 lsl $0x00 -> %z15.b",
        "subr   %z20.b $0xab lsl $0x00 -> %z20.b",
        "subr   %z30.b $0xff lsl $0x00 -> %z30.b",
    };
    TEST_LOOP(subr, subr_sve_shift, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_immed_uint(imm8_0_0[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_0[i], OPSZ_1b));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_1[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_1[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_1[6] = {
        "subr   %z0.h $0x00 lsl $0x08 -> %z0.h",
        "subr   %z5.h $0x2b lsl $0x08 -> %z5.h",
        "subr   %z10.h $0x56 lsl $0x08 -> %z10.h",
        "subr   %z15.h $0x81 lsl $0x08 -> %z15.h",
        "subr   %z20.h $0xab lsl $0x00 -> %z20.h",
        "subr   %z30.h $0xff lsl $0x00 -> %z30.h",
    };
    TEST_LOOP(subr, subr_sve_shift, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_immed_uint(imm8_0_1[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_1[i], OPSZ_1b));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_2[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_2[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_2[6] = {
        "subr   %z0.s $0x00 lsl $0x08 -> %z0.s",
        "subr   %z5.s $0x2b lsl $0x08 -> %z5.s",
        "subr   %z10.s $0x56 lsl $0x08 -> %z10.s",
        "subr   %z15.s $0x81 lsl $0x08 -> %z15.s",
        "subr   %z20.s $0xab lsl $0x00 -> %z20.s",
        "subr   %z30.s $0xff lsl $0x00 -> %z30.s",
    };
    TEST_LOOP(subr, subr_sve_shift, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_immed_uint(imm8_0_2[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_2[i], OPSZ_1b));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_3[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_3[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_3[6] = {
        "subr   %z0.d $0x00 lsl $0x08 -> %z0.d",
        "subr   %z5.d $0x2b lsl $0x08 -> %z5.d",
        "subr   %z10.d $0x56 lsl $0x08 -> %z10.d",
        "subr   %z15.d $0x81 lsl $0x08 -> %z15.d",
        "subr   %z20.d $0xab lsl $0x00 -> %z20.d",
        "subr   %z30.d $0xff lsl $0x00 -> %z30.d",
    };
    TEST_LOOP(subr, subr_sve_shift, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_immed_uint(imm8_0_3[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_3[i], OPSZ_1b));

    return success;
}

TEST_INSTR(uqadd_sve_shift)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing UQADD   <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm>, <shift> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_0[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_0[6] = { 0, 0, 0, 0, 0, 0 };
    const char *expected_0_0[6] = {
        "uqadd  %z0.b $0x00 lsl $0x00 -> %z0.b",
        "uqadd  %z5.b $0x2b lsl $0x00 -> %z5.b",
        "uqadd  %z10.b $0x56 lsl $0x00 -> %z10.b",
        "uqadd  %z15.b $0x81 lsl $0x00 -> %z15.b",
        "uqadd  %z20.b $0xab lsl $0x00 -> %z20.b",
        "uqadd  %z30.b $0xff lsl $0x00 -> %z30.b",
    };
    TEST_LOOP(uqadd, uqadd_sve_shift, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_immed_uint(imm8_0_0[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_0[i], OPSZ_1b));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_1[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_1[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_1[6] = {
        "uqadd  %z0.h $0x00 lsl $0x08 -> %z0.h",
        "uqadd  %z5.h $0x2b lsl $0x08 -> %z5.h",
        "uqadd  %z10.h $0x56 lsl $0x08 -> %z10.h",
        "uqadd  %z15.h $0x81 lsl $0x08 -> %z15.h",
        "uqadd  %z20.h $0xab lsl $0x00 -> %z20.h",
        "uqadd  %z30.h $0xff lsl $0x00 -> %z30.h",
    };
    TEST_LOOP(uqadd, uqadd_sve_shift, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_immed_uint(imm8_0_1[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_1[i], OPSZ_1b));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_2[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_2[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_2[6] = {
        "uqadd  %z0.s $0x00 lsl $0x08 -> %z0.s",
        "uqadd  %z5.s $0x2b lsl $0x08 -> %z5.s",
        "uqadd  %z10.s $0x56 lsl $0x08 -> %z10.s",
        "uqadd  %z15.s $0x81 lsl $0x08 -> %z15.s",
        "uqadd  %z20.s $0xab lsl $0x00 -> %z20.s",
        "uqadd  %z30.s $0xff lsl $0x00 -> %z30.s",
    };
    TEST_LOOP(uqadd, uqadd_sve_shift, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_immed_uint(imm8_0_2[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_2[i], OPSZ_1b));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_3[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_3[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_3[6] = {
        "uqadd  %z0.d $0x00 lsl $0x08 -> %z0.d",
        "uqadd  %z5.d $0x2b lsl $0x08 -> %z5.d",
        "uqadd  %z10.d $0x56 lsl $0x08 -> %z10.d",
        "uqadd  %z15.d $0x81 lsl $0x08 -> %z15.d",
        "uqadd  %z20.d $0xab lsl $0x00 -> %z20.d",
        "uqadd  %z30.d $0xff lsl $0x00 -> %z30.d",
    };
    TEST_LOOP(uqadd, uqadd_sve_shift, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_immed_uint(imm8_0_3[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_3[i], OPSZ_1b));

    return success;
}

TEST_INSTR(uqadd_sve)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing UQADD   <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zd_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_0[6] = {
        "uqadd  %z0.b %z0.b -> %z0.b",    "uqadd  %z6.b %z7.b -> %z5.b",
        "uqadd  %z11.b %z12.b -> %z10.b", "uqadd  %z16.b %z17.b -> %z15.b",
        "uqadd  %z21.b %z22.b -> %z20.b", "uqadd  %z30.b %z30.b -> %z30.b",
    };
    TEST_LOOP(uqadd, uqadd_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zd_0_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_0_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_1));

    reg_id_t Zd_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_1[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_1[6] = {
        "uqadd  %z0.h %z0.h -> %z0.h",    "uqadd  %z6.h %z7.h -> %z5.h",
        "uqadd  %z11.h %z12.h -> %z10.h", "uqadd  %z16.h %z17.h -> %z15.h",
        "uqadd  %z21.h %z22.h -> %z20.h", "uqadd  %z30.h %z30.h -> %z30.h",
    };
    TEST_LOOP(uqadd, uqadd_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zd_0_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_0_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_2));

    reg_id_t Zd_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_2[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_2[6] = {
        "uqadd  %z0.s %z0.s -> %z0.s",    "uqadd  %z6.s %z7.s -> %z5.s",
        "uqadd  %z11.s %z12.s -> %z10.s", "uqadd  %z16.s %z17.s -> %z15.s",
        "uqadd  %z21.s %z22.s -> %z20.s", "uqadd  %z30.s %z30.s -> %z30.s",
    };
    TEST_LOOP(uqadd, uqadd_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zd_0_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_0_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_4));

    reg_id_t Zd_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_3[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_3[6] = {
        "uqadd  %z0.d %z0.d -> %z0.d",    "uqadd  %z6.d %z7.d -> %z5.d",
        "uqadd  %z11.d %z12.d -> %z10.d", "uqadd  %z16.d %z17.d -> %z15.d",
        "uqadd  %z21.d %z22.d -> %z20.d", "uqadd  %z30.d %z30.d -> %z30.d",
    };
    TEST_LOOP(uqadd, uqadd_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zd_0_3[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_0_3[i], OPSZ_8),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(uqsub_sve_shift)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing UQSUB   <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm>, <shift> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_0[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_0[6] = { 0, 0, 0, 0, 0, 0 };
    const char *expected_0_0[6] = {
        "uqsub  %z0.b $0x00 lsl $0x00 -> %z0.b",
        "uqsub  %z5.b $0x2b lsl $0x00 -> %z5.b",
        "uqsub  %z10.b $0x56 lsl $0x00 -> %z10.b",
        "uqsub  %z15.b $0x81 lsl $0x00 -> %z15.b",
        "uqsub  %z20.b $0xab lsl $0x00 -> %z20.b",
        "uqsub  %z30.b $0xff lsl $0x00 -> %z30.b",
    };
    TEST_LOOP(uqsub, uqsub_sve_shift, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_immed_uint(imm8_0_0[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_0[i], OPSZ_1b));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_1[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_1[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_1[6] = {
        "uqsub  %z0.h $0x00 lsl $0x08 -> %z0.h",
        "uqsub  %z5.h $0x2b lsl $0x08 -> %z5.h",
        "uqsub  %z10.h $0x56 lsl $0x08 -> %z10.h",
        "uqsub  %z15.h $0x81 lsl $0x08 -> %z15.h",
        "uqsub  %z20.h $0xab lsl $0x00 -> %z20.h",
        "uqsub  %z30.h $0xff lsl $0x00 -> %z30.h",
    };
    TEST_LOOP(uqsub, uqsub_sve_shift, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_immed_uint(imm8_0_1[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_1[i], OPSZ_1b));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_2[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_2[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_2[6] = {
        "uqsub  %z0.s $0x00 lsl $0x08 -> %z0.s",
        "uqsub  %z5.s $0x2b lsl $0x08 -> %z5.s",
        "uqsub  %z10.s $0x56 lsl $0x08 -> %z10.s",
        "uqsub  %z15.s $0x81 lsl $0x08 -> %z15.s",
        "uqsub  %z20.s $0xab lsl $0x00 -> %z20.s",
        "uqsub  %z30.s $0xff lsl $0x00 -> %z30.s",
    };
    TEST_LOOP(uqsub, uqsub_sve_shift, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_immed_uint(imm8_0_2[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_2[i], OPSZ_1b));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    uint imm8_0_3[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_3[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_3[6] = {
        "uqsub  %z0.d $0x00 lsl $0x08 -> %z0.d",
        "uqsub  %z5.d $0x2b lsl $0x08 -> %z5.d",
        "uqsub  %z10.d $0x56 lsl $0x08 -> %z10.d",
        "uqsub  %z15.d $0x81 lsl $0x08 -> %z15.d",
        "uqsub  %z20.d $0xab lsl $0x00 -> %z20.d",
        "uqsub  %z30.d $0xff lsl $0x00 -> %z30.d",
    };
    TEST_LOOP(uqsub, uqsub_sve_shift, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_immed_uint(imm8_0_3[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_3[i], OPSZ_1b));

    return success;
}

TEST_INSTR(uqsub_sve)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing UQSUB   <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zd_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_0[6] = {
        "uqsub  %z0.b %z0.b -> %z0.b",    "uqsub  %z6.b %z7.b -> %z5.b",
        "uqsub  %z11.b %z12.b -> %z10.b", "uqsub  %z16.b %z17.b -> %z15.b",
        "uqsub  %z21.b %z22.b -> %z20.b", "uqsub  %z30.b %z30.b -> %z30.b",
    };
    TEST_LOOP(uqsub, uqsub_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zd_0_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_0_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_1));

    reg_id_t Zd_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_1[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_1[6] = {
        "uqsub  %z0.h %z0.h -> %z0.h",    "uqsub  %z6.h %z7.h -> %z5.h",
        "uqsub  %z11.h %z12.h -> %z10.h", "uqsub  %z16.h %z17.h -> %z15.h",
        "uqsub  %z21.h %z22.h -> %z20.h", "uqsub  %z30.h %z30.h -> %z30.h",
    };
    TEST_LOOP(uqsub, uqsub_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zd_0_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_0_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_2));

    reg_id_t Zd_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_2[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_2[6] = {
        "uqsub  %z0.s %z0.s -> %z0.s",    "uqsub  %z6.s %z7.s -> %z5.s",
        "uqsub  %z11.s %z12.s -> %z10.s", "uqsub  %z16.s %z17.s -> %z15.s",
        "uqsub  %z21.s %z22.s -> %z20.s", "uqsub  %z30.s %z30.s -> %z30.s",
    };
    TEST_LOOP(uqsub, uqsub_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zd_0_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_0_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_4));

    reg_id_t Zd_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_3[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    const char *expected_0_3[6] = {
        "uqsub  %z0.d %z0.d -> %z0.d",    "uqsub  %z6.d %z7.d -> %z5.d",
        "uqsub  %z11.d %z12.d -> %z10.d", "uqsub  %z16.d %z17.d -> %z15.d",
        "uqsub  %z21.d %z22.d -> %z20.d", "uqsub  %z30.d %z30.d -> %z30.d",
    };
    TEST_LOOP(uqsub, uqsub_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zd_0_3[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_0_3[i], OPSZ_8),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(cpy_sve_shift_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing CPY     <Zd>.<Ts>, <Pg>/Z, #<imm>, <shift> */
    reg_id_t Zd_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P3,  DR_REG_P6,
                           DR_REG_P8, DR_REG_P10, DR_REG_P14 };
    int imm8_0_0[6] = { -128, -84, -41, 2, 44, 127 };
    uint shift_0_0[6] = { 0, 0, 0, 0, 0, 0 };
    const char *expected_0_0[6] = {
        "cpy    %p0/z $0x80 lsl $0x00 -> %z0.b",
        "cpy    %p3/z $0xac lsl $0x00 -> %z5.b",
        "cpy    %p6/z $0xd7 lsl $0x00 -> %z10.b",
        "cpy    %p8/z $0x02 lsl $0x00 -> %z15.b",
        "cpy    %p10/z $0x2c lsl $0x00 -> %z20.b",
        "cpy    %p14/z $0x7f lsl $0x00 -> %z30.b",
    };
    TEST_LOOP(cpy, cpy_sve_shift_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zd_0_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pg_0_0[i], false),
              opnd_create_immed_int(imm8_0_0[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_0[i], OPSZ_1b));

    reg_id_t Zd_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P3,  DR_REG_P6,
                           DR_REG_P8, DR_REG_P10, DR_REG_P14 };
    int imm8_0_1[6] = { -128, -84, -41, 2, 44, 127 };
    uint shift_0_1[6] = { 8, 0, 0, 0, 8, 0 };
    const char *expected_0_1[6] = {
        "cpy    %p0/z $0x80 lsl $0x08 -> %z0.h",
        "cpy    %p3/z $0xac lsl $0x00 -> %z5.h",
        "cpy    %p6/z $0xd7 lsl $0x00 -> %z10.h",
        "cpy    %p8/z $0x02 lsl $0x00 -> %z15.h",
        "cpy    %p10/z $0x2c lsl $0x08 -> %z20.h",
        "cpy    %p14/z $0x7f lsl $0x00 -> %z30.h",
    };
    TEST_LOOP(cpy, cpy_sve_shift_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zd_0_1[i], OPSZ_2),
              opnd_create_predicate_reg(Pg_0_1[i], false),
              opnd_create_immed_int(imm8_0_1[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_1[i], OPSZ_1b));

    reg_id_t Zd_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_0_2[6] = { DR_REG_P0, DR_REG_P3,  DR_REG_P6,
                           DR_REG_P8, DR_REG_P10, DR_REG_P14 };
    int imm8_0_2[6] = { -128, -84, -41, 2, 44, 127 };
    uint shift_0_2[6] = { 8, 0, 0, 0, 8, 0 };
    const char *expected_0_2[6] = {
        "cpy    %p0/z $0x80 lsl $0x08 -> %z0.s",
        "cpy    %p3/z $0xac lsl $0x00 -> %z5.s",
        "cpy    %p6/z $0xd7 lsl $0x00 -> %z10.s",
        "cpy    %p8/z $0x02 lsl $0x00 -> %z15.s",
        "cpy    %p10/z $0x2c lsl $0x08 -> %z20.s",
        "cpy    %p14/z $0x7f lsl $0x00 -> %z30.s",
    };
    TEST_LOOP(cpy, cpy_sve_shift_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zd_0_2[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_2[i], false),
              opnd_create_immed_int(imm8_0_2[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_2[i], OPSZ_1b));

    reg_id_t Zd_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_0_3[6] = { DR_REG_P0, DR_REG_P3,  DR_REG_P6,
                           DR_REG_P8, DR_REG_P10, DR_REG_P14 };
    int imm8_0_3[6] = { -128, -84, -41, 2, 44, 127 };
    uint shift_0_3[6] = { 8, 0, 0, 0, 8, 0 };
    const char *expected_0_3[6] = {
        "cpy    %p0/z $0x80 lsl $0x08 -> %z0.d",
        "cpy    %p3/z $0xac lsl $0x00 -> %z5.d",
        "cpy    %p6/z $0xd7 lsl $0x00 -> %z10.d",
        "cpy    %p8/z $0x02 lsl $0x00 -> %z15.d",
        "cpy    %p10/z $0x2c lsl $0x08 -> %z20.d",
        "cpy    %p14/z $0x7f lsl $0x00 -> %z30.d",
    };
    TEST_LOOP(cpy, cpy_sve_shift_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zd_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], false),
              opnd_create_immed_int(imm8_0_3[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_3[i], OPSZ_1b));

    /* Testing CPY     <Zd>.<Ts>, <Pg>/M, #<imm>, <shift> */
    reg_id_t Zd_1_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_1_0[6] = { DR_REG_P0, DR_REG_P3,  DR_REG_P6,
                           DR_REG_P8, DR_REG_P10, DR_REG_P14 };
    int imm8_1_0[6] = { -128, -84, -41, 2, 44, 127 };
    uint shift_1_0[6] = { 0, 0, 0, 0, 0, 0 };
    const char *expected_1_0[6] = {
        "cpy    %p0/m $0x80 lsl $0x00 -> %z0.b",
        "cpy    %p3/m $0xac lsl $0x00 -> %z5.b",
        "cpy    %p6/m $0xd7 lsl $0x00 -> %z10.b",
        "cpy    %p8/m $0x02 lsl $0x00 -> %z15.b",
        "cpy    %p10/m $0x2c lsl $0x00 -> %z20.b",
        "cpy    %p14/m $0x7f lsl $0x00 -> %z30.b",
    };
    TEST_LOOP(cpy, cpy_sve_shift_pred, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zd_1_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pg_1_0[i], true),
              opnd_create_immed_int(imm8_1_0[i], OPSZ_1),
              opnd_create_immed_uint(shift_1_0[i], OPSZ_1b));

    reg_id_t Zd_1_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_1_1[6] = { DR_REG_P0, DR_REG_P3,  DR_REG_P6,
                           DR_REG_P8, DR_REG_P10, DR_REG_P14 };
    int imm8_1_1[6] = { -128, -84, -41, 2, 44, 127 };
    uint shift_1_1[6] = { 8, 0, 0, 0, 8, 0 };
    const char *expected_1_1[6] = {
        "cpy    %p0/m $0x80 lsl $0x08 -> %z0.h",
        "cpy    %p3/m $0xac lsl $0x00 -> %z5.h",
        "cpy    %p6/m $0xd7 lsl $0x00 -> %z10.h",
        "cpy    %p8/m $0x02 lsl $0x00 -> %z15.h",
        "cpy    %p10/m $0x2c lsl $0x08 -> %z20.h",
        "cpy    %p14/m $0x7f lsl $0x00 -> %z30.h",
    };
    TEST_LOOP(cpy, cpy_sve_shift_pred, 6, expected_1_1[i],
              opnd_create_reg_element_vector(Zd_1_1[i], OPSZ_2),
              opnd_create_predicate_reg(Pg_1_1[i], true),
              opnd_create_immed_int(imm8_1_1[i], OPSZ_1),
              opnd_create_immed_uint(shift_1_1[i], OPSZ_1b));

    reg_id_t Zd_1_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_1_2[6] = { DR_REG_P0, DR_REG_P3,  DR_REG_P6,
                           DR_REG_P8, DR_REG_P10, DR_REG_P14 };
    int imm8_1_2[6] = { -128, -84, -41, 2, 44, 127 };
    uint shift_1_2[6] = { 8, 0, 0, 0, 8, 0 };
    const char *expected_1_2[6] = {
        "cpy    %p0/m $0x80 lsl $0x08 -> %z0.s",
        "cpy    %p3/m $0xac lsl $0x00 -> %z5.s",
        "cpy    %p6/m $0xd7 lsl $0x00 -> %z10.s",
        "cpy    %p8/m $0x02 lsl $0x00 -> %z15.s",
        "cpy    %p10/m $0x2c lsl $0x08 -> %z20.s",
        "cpy    %p14/m $0x7f lsl $0x00 -> %z30.s",
    };
    TEST_LOOP(cpy, cpy_sve_shift_pred, 6, expected_1_2[i],
              opnd_create_reg_element_vector(Zd_1_2[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_1_2[i], true),
              opnd_create_immed_int(imm8_1_2[i], OPSZ_1),
              opnd_create_immed_uint(shift_1_2[i], OPSZ_1b));

    reg_id_t Zd_1_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Pg_1_3[6] = { DR_REG_P0, DR_REG_P3,  DR_REG_P6,
                           DR_REG_P8, DR_REG_P10, DR_REG_P14 };
    int imm8_1_3[6] = { -128, -84, -41, 2, 44, 127 };
    uint shift_1_3[6] = { 8, 0, 0, 0, 8, 0 };
    const char *expected_1_3[6] = {
        "cpy    %p0/m $0x80 lsl $0x08 -> %z0.d",
        "cpy    %p3/m $0xac lsl $0x00 -> %z5.d",
        "cpy    %p6/m $0xd7 lsl $0x00 -> %z10.d",
        "cpy    %p8/m $0x02 lsl $0x00 -> %z15.d",
        "cpy    %p10/m $0x2c lsl $0x08 -> %z20.d",
        "cpy    %p14/m $0x7f lsl $0x00 -> %z30.d",
    };
    TEST_LOOP(cpy, cpy_sve_shift_pred, 6, expected_1_3[i],
              opnd_create_reg_element_vector(Zd_1_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_1_3[i], true),
              opnd_create_immed_int(imm8_1_3[i], OPSZ_1),
              opnd_create_immed_uint(shift_1_3[i], OPSZ_1b));

    return success;
}

TEST_INSTR(ptest_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing PTEST   <Pg>, <Pn>.B */
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P5,
                           DR_REG_P7, DR_REG_P9, DR_REG_P14 };
    reg_id_t Pn_0_0[6] = { DR_REG_P0, DR_REG_P3,  DR_REG_P6,
                           DR_REG_P8, DR_REG_P10, DR_REG_P14 };
    const char *expected_0_0[6] = {
        "ptest  %p0 %p0.b", "ptest  %p2 %p3.b",  "ptest  %p5 %p6.b",
        "ptest  %p7 %p8.b", "ptest  %p9 %p10.b", "ptest  %p14 %p14.b",
    };
    TEST_LOOP(ptest, ptest_sve_pred, 6, expected_0_0[i], opnd_create_reg(Pg_0_0[i]),
              opnd_create_reg_element_vector(Pn_0_0[i], OPSZ_1));

    return success;
}

TEST_INSTR(mad_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing MAD     <Zdn>.<Ts>, <Pg>/M, <Zm>.<Ts>, <Za>.<Ts> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Za_0_0[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "mad    %p0/m %z0.b %z0.b %z0.b -> %z0.b",
        "mad    %p2/m %z5.b %z7.b %z8.b -> %z5.b",
        "mad    %p3/m %z10.b %z12.b %z13.b -> %z10.b",
        "mad    %p5/m %z16.b %z18.b %z19.b -> %z16.b",
        "mad    %p6/m %z21.b %z23.b %z24.b -> %z21.b",
        "mad    %p7/m %z31.b %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(mad, mad_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pg_0_0[i], true),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Za_0_0[i], OPSZ_1));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Za_0_1[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "mad    %p0/m %z0.h %z0.h %z0.h -> %z0.h",
        "mad    %p2/m %z5.h %z7.h %z8.h -> %z5.h",
        "mad    %p3/m %z10.h %z12.h %z13.h -> %z10.h",
        "mad    %p5/m %z16.h %z18.h %z19.h -> %z16.h",
        "mad    %p6/m %z21.h %z23.h %z24.h -> %z21.h",
        "mad    %p7/m %z31.h %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(mad, mad_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_predicate_reg(Pg_0_1[i], true),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Za_0_1[i], OPSZ_2));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_2[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Za_0_2[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_2[6] = {
        "mad    %p0/m %z0.s %z0.s %z0.s -> %z0.s",
        "mad    %p2/m %z5.s %z7.s %z8.s -> %z5.s",
        "mad    %p3/m %z10.s %z12.s %z13.s -> %z10.s",
        "mad    %p5/m %z16.s %z18.s %z19.s -> %z16.s",
        "mad    %p6/m %z21.s %z23.s %z24.s -> %z21.s",
        "mad    %p7/m %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(mad, mad_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_2[i], true),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Za_0_2[i], OPSZ_4));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_3[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Za_0_3[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_3[6] = {
        "mad    %p0/m %z0.d %z0.d %z0.d -> %z0.d",
        "mad    %p2/m %z5.d %z7.d %z8.d -> %z5.d",
        "mad    %p3/m %z10.d %z12.d %z13.d -> %z10.d",
        "mad    %p5/m %z16.d %z18.d %z19.d -> %z16.d",
        "mad    %p6/m %z21.d %z23.d %z24.d -> %z21.d",
        "mad    %p7/m %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(mad, mad_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8),
              opnd_create_reg_element_vector(Za_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(mla_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing MLA     <Zda>.<Ts>, <Pg>/M, <Zn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zda_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "mla    %p0/m %z0.b %z0.b %z0.b -> %z0.b",
        "mla    %p2/m %z7.b %z8.b %z5.b -> %z5.b",
        "mla    %p3/m %z12.b %z13.b %z10.b -> %z10.b",
        "mla    %p5/m %z18.b %z19.b %z16.b -> %z16.b",
        "mla    %p6/m %z23.b %z24.b %z21.b -> %z21.b",
        "mla    %p7/m %z31.b %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(mla, mla_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zda_0_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pg_0_0[i], true),
              opnd_create_reg_element_vector(Zn_0_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_1));

    reg_id_t Zda_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "mla    %p0/m %z0.h %z0.h %z0.h -> %z0.h",
        "mla    %p2/m %z7.h %z8.h %z5.h -> %z5.h",
        "mla    %p3/m %z12.h %z13.h %z10.h -> %z10.h",
        "mla    %p5/m %z18.h %z19.h %z16.h -> %z16.h",
        "mla    %p6/m %z23.h %z24.h %z21.h -> %z21.h",
        "mla    %p7/m %z31.h %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(mla, mla_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zda_0_1[i], OPSZ_2),
              opnd_create_predicate_reg(Pg_0_1[i], true),
              opnd_create_reg_element_vector(Zn_0_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_2));

    reg_id_t Zda_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_2[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_2[6] = {
        "mla    %p0/m %z0.s %z0.s %z0.s -> %z0.s",
        "mla    %p2/m %z7.s %z8.s %z5.s -> %z5.s",
        "mla    %p3/m %z12.s %z13.s %z10.s -> %z10.s",
        "mla    %p5/m %z18.s %z19.s %z16.s -> %z16.s",
        "mla    %p6/m %z23.s %z24.s %z21.s -> %z21.s",
        "mla    %p7/m %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(mla, mla_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zda_0_2[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_2[i], true),
              opnd_create_reg_element_vector(Zn_0_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_4));

    reg_id_t Zda_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_3[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Zm_0_3[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_3[6] = {
        "mla    %p0/m %z0.d %z0.d %z0.d -> %z0.d",
        "mla    %p2/m %z7.d %z8.d %z5.d -> %z5.d",
        "mla    %p3/m %z12.d %z13.d %z10.d -> %z10.d",
        "mla    %p5/m %z18.d %z19.d %z16.d -> %z16.d",
        "mla    %p6/m %z23.d %z24.d %z21.d -> %z21.d",
        "mla    %p7/m %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(mla, mla_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zda_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zn_0_3[i], OPSZ_8),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(mls_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing MLS     <Zda>.<Ts>, <Pg>/M, <Zn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zda_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "mls    %p0/m %z0.b %z0.b %z0.b -> %z0.b",
        "mls    %p2/m %z7.b %z8.b %z5.b -> %z5.b",
        "mls    %p3/m %z12.b %z13.b %z10.b -> %z10.b",
        "mls    %p5/m %z18.b %z19.b %z16.b -> %z16.b",
        "mls    %p6/m %z23.b %z24.b %z21.b -> %z21.b",
        "mls    %p7/m %z31.b %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(mls, mls_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zda_0_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pg_0_0[i], true),
              opnd_create_reg_element_vector(Zn_0_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_1));

    reg_id_t Zda_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "mls    %p0/m %z0.h %z0.h %z0.h -> %z0.h",
        "mls    %p2/m %z7.h %z8.h %z5.h -> %z5.h",
        "mls    %p3/m %z12.h %z13.h %z10.h -> %z10.h",
        "mls    %p5/m %z18.h %z19.h %z16.h -> %z16.h",
        "mls    %p6/m %z23.h %z24.h %z21.h -> %z21.h",
        "mls    %p7/m %z31.h %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(mls, mls_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zda_0_1[i], OPSZ_2),
              opnd_create_predicate_reg(Pg_0_1[i], true),
              opnd_create_reg_element_vector(Zn_0_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_2));

    reg_id_t Zda_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_2[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_2[6] = {
        "mls    %p0/m %z0.s %z0.s %z0.s -> %z0.s",
        "mls    %p2/m %z7.s %z8.s %z5.s -> %z5.s",
        "mls    %p3/m %z12.s %z13.s %z10.s -> %z10.s",
        "mls    %p5/m %z18.s %z19.s %z16.s -> %z16.s",
        "mls    %p6/m %z23.s %z24.s %z21.s -> %z21.s",
        "mls    %p7/m %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(mls, mls_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zda_0_2[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_2[i], true),
              opnd_create_reg_element_vector(Zn_0_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_4));

    reg_id_t Zda_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_3[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Zm_0_3[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_3[6] = {
        "mls    %p0/m %z0.d %z0.d %z0.d -> %z0.d",
        "mls    %p2/m %z7.d %z8.d %z5.d -> %z5.d",
        "mls    %p3/m %z12.d %z13.d %z10.d -> %z10.d",
        "mls    %p5/m %z18.d %z19.d %z16.d -> %z16.d",
        "mls    %p6/m %z23.d %z24.d %z21.d -> %z21.d",
        "mls    %p7/m %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(mls, mls_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zda_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zn_0_3[i], OPSZ_8),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(msb_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing MSB     <Zdn>.<Ts>, <Pg>/M, <Zm>.<Ts>, <Za>.<Ts> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Za_0_0[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "msb    %p0/m %z0.b %z0.b %z0.b -> %z0.b",
        "msb    %p2/m %z5.b %z7.b %z8.b -> %z5.b",
        "msb    %p3/m %z10.b %z12.b %z13.b -> %z10.b",
        "msb    %p5/m %z16.b %z18.b %z19.b -> %z16.b",
        "msb    %p6/m %z21.b %z23.b %z24.b -> %z21.b",
        "msb    %p7/m %z31.b %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(msb, msb_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pg_0_0[i], true),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Za_0_0[i], OPSZ_1));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Za_0_1[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "msb    %p0/m %z0.h %z0.h %z0.h -> %z0.h",
        "msb    %p2/m %z5.h %z7.h %z8.h -> %z5.h",
        "msb    %p3/m %z10.h %z12.h %z13.h -> %z10.h",
        "msb    %p5/m %z16.h %z18.h %z19.h -> %z16.h",
        "msb    %p6/m %z21.h %z23.h %z24.h -> %z21.h",
        "msb    %p7/m %z31.h %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(msb, msb_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_predicate_reg(Pg_0_1[i], true),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Za_0_1[i], OPSZ_2));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_2[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Za_0_2[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_2[6] = {
        "msb    %p0/m %z0.s %z0.s %z0.s -> %z0.s",
        "msb    %p2/m %z5.s %z7.s %z8.s -> %z5.s",
        "msb    %p3/m %z10.s %z12.s %z13.s -> %z10.s",
        "msb    %p5/m %z16.s %z18.s %z19.s -> %z16.s",
        "msb    %p6/m %z21.s %z23.s %z24.s -> %z21.s",
        "msb    %p7/m %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(msb, msb_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_2[i], true),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Za_0_2[i], OPSZ_4));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_3[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Za_0_3[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_3[6] = {
        "msb    %p0/m %z0.d %z0.d %z0.d -> %z0.d",
        "msb    %p2/m %z5.d %z7.d %z8.d -> %z5.d",
        "msb    %p3/m %z10.d %z12.d %z13.d -> %z10.d",
        "msb    %p5/m %z16.d %z18.d %z19.d -> %z16.d",
        "msb    %p6/m %z21.d %z23.d %z24.d -> %z21.d",
        "msb    %p7/m %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(msb, msb_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8),
              opnd_create_reg_element_vector(Za_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(mul_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing MUL     <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "mul    %p0/m %z0.b %z0.b -> %z0.b",    "mul    %p2/m %z5.b %z7.b -> %z5.b",
        "mul    %p3/m %z10.b %z12.b -> %z10.b", "mul    %p5/m %z16.b %z18.b -> %z16.b",
        "mul    %p6/m %z21.b %z23.b -> %z21.b", "mul    %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(mul, mul_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pg_0_0[i], true),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_1));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "mul    %p0/m %z0.h %z0.h -> %z0.h",    "mul    %p2/m %z5.h %z7.h -> %z5.h",
        "mul    %p3/m %z10.h %z12.h -> %z10.h", "mul    %p5/m %z16.h %z18.h -> %z16.h",
        "mul    %p6/m %z21.h %z23.h -> %z21.h", "mul    %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(mul, mul_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_predicate_reg(Pg_0_1[i], true),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_2));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_2[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_2[6] = {
        "mul    %p0/m %z0.s %z0.s -> %z0.s",    "mul    %p2/m %z5.s %z7.s -> %z5.s",
        "mul    %p3/m %z10.s %z12.s -> %z10.s", "mul    %p5/m %z16.s %z18.s -> %z16.s",
        "mul    %p6/m %z21.s %z23.s -> %z21.s", "mul    %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(mul, mul_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_2[i], true),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_4));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_3[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_3[6] = {
        "mul    %p0/m %z0.d %z0.d -> %z0.d",    "mul    %p2/m %z5.d %z7.d -> %z5.d",
        "mul    %p3/m %z10.d %z12.d -> %z10.d", "mul    %p5/m %z16.d %z18.d -> %z16.d",
        "mul    %p6/m %z21.d %z23.d -> %z21.d", "mul    %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(mul, mul_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(mul_sve)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing MUL     <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    int imm8_0_0[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_0[6] = {
        "mul    %z0.b $0x80 -> %z0.b",   "mul    %z5.b $0xab -> %z5.b",
        "mul    %z10.b $0xd6 -> %z10.b", "mul    %z16.b $0x01 -> %z16.b",
        "mul    %z21.b $0x2b -> %z21.b", "mul    %z31.b $0x7f -> %z31.b",
    };
    TEST_LOOP(mul, mul_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_immed_int(imm8_0_0[i], OPSZ_1));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    int imm8_0_1[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_1[6] = {
        "mul    %z0.h $0x80 -> %z0.h",   "mul    %z5.h $0xab -> %z5.h",
        "mul    %z10.h $0xd6 -> %z10.h", "mul    %z16.h $0x01 -> %z16.h",
        "mul    %z21.h $0x2b -> %z21.h", "mul    %z31.h $0x7f -> %z31.h",
    };
    TEST_LOOP(mul, mul_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_immed_int(imm8_0_1[i], OPSZ_1));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    int imm8_0_2[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_2[6] = {
        "mul    %z0.s $0x80 -> %z0.s",   "mul    %z5.s $0xab -> %z5.s",
        "mul    %z10.s $0xd6 -> %z10.s", "mul    %z16.s $0x01 -> %z16.s",
        "mul    %z21.s $0x2b -> %z21.s", "mul    %z31.s $0x7f -> %z31.s",
    };
    TEST_LOOP(mul, mul_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_immed_int(imm8_0_2[i], OPSZ_1));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    int imm8_0_3[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_3[6] = {
        "mul    %z0.d $0x80 -> %z0.d",   "mul    %z5.d $0xab -> %z5.d",
        "mul    %z10.d $0xd6 -> %z10.d", "mul    %z16.d $0x01 -> %z16.d",
        "mul    %z21.d $0x2b -> %z21.d", "mul    %z31.d $0x7f -> %z31.d",
    };
    TEST_LOOP(mul, mul_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_immed_int(imm8_0_3[i], OPSZ_1));

    return success;
}

TEST_INSTR(smulh_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing SMULH   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "smulh  %p0/m %z0.b %z0.b -> %z0.b",    "smulh  %p2/m %z5.b %z7.b -> %z5.b",
        "smulh  %p3/m %z10.b %z12.b -> %z10.b", "smulh  %p5/m %z16.b %z18.b -> %z16.b",
        "smulh  %p6/m %z21.b %z23.b -> %z21.b", "smulh  %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(smulh, smulh_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pg_0_0[i], true),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_1));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "smulh  %p0/m %z0.h %z0.h -> %z0.h",    "smulh  %p2/m %z5.h %z7.h -> %z5.h",
        "smulh  %p3/m %z10.h %z12.h -> %z10.h", "smulh  %p5/m %z16.h %z18.h -> %z16.h",
        "smulh  %p6/m %z21.h %z23.h -> %z21.h", "smulh  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(smulh, smulh_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_predicate_reg(Pg_0_1[i], true),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_2));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_2[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_2[6] = {
        "smulh  %p0/m %z0.s %z0.s -> %z0.s",    "smulh  %p2/m %z5.s %z7.s -> %z5.s",
        "smulh  %p3/m %z10.s %z12.s -> %z10.s", "smulh  %p5/m %z16.s %z18.s -> %z16.s",
        "smulh  %p6/m %z21.s %z23.s -> %z21.s", "smulh  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(smulh, smulh_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_2[i], true),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_4));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_3[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_3[6] = {
        "smulh  %p0/m %z0.d %z0.d -> %z0.d",    "smulh  %p2/m %z5.d %z7.d -> %z5.d",
        "smulh  %p3/m %z10.d %z12.d -> %z10.d", "smulh  %p5/m %z16.d %z18.d -> %z16.d",
        "smulh  %p6/m %z21.d %z23.d -> %z21.d", "smulh  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(smulh, smulh_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(umulh_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing UMULH   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "umulh  %p0/m %z0.b %z0.b -> %z0.b",    "umulh  %p2/m %z5.b %z7.b -> %z5.b",
        "umulh  %p3/m %z10.b %z12.b -> %z10.b", "umulh  %p5/m %z16.b %z18.b -> %z16.b",
        "umulh  %p6/m %z21.b %z23.b -> %z21.b", "umulh  %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(umulh, umulh_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pg_0_0[i], true),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_1));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "umulh  %p0/m %z0.h %z0.h -> %z0.h",    "umulh  %p2/m %z5.h %z7.h -> %z5.h",
        "umulh  %p3/m %z10.h %z12.h -> %z10.h", "umulh  %p5/m %z16.h %z18.h -> %z16.h",
        "umulh  %p6/m %z21.h %z23.h -> %z21.h", "umulh  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(umulh, umulh_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_predicate_reg(Pg_0_1[i], true),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_2));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_2[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_2[6] = {
        "umulh  %p0/m %z0.s %z0.s -> %z0.s",    "umulh  %p2/m %z5.s %z7.s -> %z5.s",
        "umulh  %p3/m %z10.s %z12.s -> %z10.s", "umulh  %p5/m %z16.s %z18.s -> %z16.s",
        "umulh  %p6/m %z21.s %z23.s -> %z21.s", "umulh  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(umulh, umulh_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_2[i], true),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_4));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_3[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_3[6] = {
        "umulh  %p0/m %z0.d %z0.d -> %z0.d",    "umulh  %p2/m %z5.d %z7.d -> %z5.d",
        "umulh  %p3/m %z10.d %z12.d -> %z10.d", "umulh  %p5/m %z16.d %z18.d -> %z16.d",
        "umulh  %p6/m %z21.d %z23.d -> %z21.d", "umulh  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(umulh, umulh_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
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

    RUN_INSTR_TEST(add_sve_pred);
    RUN_INSTR_TEST(add_sve_shift);
    RUN_INSTR_TEST(add_sve);

    RUN_INSTR_TEST(zip2_vector);

    RUN_INSTR_TEST(movprfx_vector);

    RUN_INSTR_TEST(sqadd_sve_shift);
    RUN_INSTR_TEST(sqadd_sve);
    RUN_INSTR_TEST(sqsub_sve_shift);
    RUN_INSTR_TEST(sqsub_sve);
    RUN_INSTR_TEST(sub_sve_pred);
    RUN_INSTR_TEST(sub_sve_shift);
    RUN_INSTR_TEST(sub_sve);
    RUN_INSTR_TEST(subr_sve_pred);
    RUN_INSTR_TEST(subr_sve_shift);
    RUN_INSTR_TEST(uqadd_sve_shift);
    RUN_INSTR_TEST(uqadd_sve);
    RUN_INSTR_TEST(uqsub_sve_shift);
    RUN_INSTR_TEST(uqsub_sve);

    RUN_INSTR_TEST(cpy_sve_shift_pred);
    RUN_INSTR_TEST(ptest_sve_pred);

    RUN_INSTR_TEST(mad_sve_pred);
    RUN_INSTR_TEST(mla_sve_pred);
    RUN_INSTR_TEST(mls_sve_pred);
    RUN_INSTR_TEST(msb_sve_pred);
    RUN_INSTR_TEST(mul_sve_pred);
    RUN_INSTR_TEST(mul_sve);
    RUN_INSTR_TEST(smulh_sve_pred);
    RUN_INSTR_TEST(umulh_sve_pred);

    print("All sve tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
