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

TEST_INSTR(fexpa_sve)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing FEXPA   <Zd>.<Ts>, <Zn>.<Ts> */
    reg_id_t Zd_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "fexpa  %z0.h -> %z0.h",   "fexpa  %z6.h -> %z5.h",   "fexpa  %z11.h -> %z10.h",
        "fexpa  %z17.h -> %z16.h", "fexpa  %z22.h -> %z21.h", "fexpa  %z31.h -> %z31.h",
    };
    TEST_LOOP(fexpa, fexpa_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zd_0_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_0_0[i], OPSZ_2));

    reg_id_t Zd_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Zn_0_1[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "fexpa  %z0.s -> %z0.s",   "fexpa  %z6.s -> %z5.s",   "fexpa  %z11.s -> %z10.s",
        "fexpa  %z17.s -> %z16.s", "fexpa  %z22.s -> %z21.s", "fexpa  %z31.s -> %z31.s",
    };
    TEST_LOOP(fexpa, fexpa_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zd_0_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_0_1[i], OPSZ_4));

    reg_id_t Zd_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Zn_0_2[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z31 };
    const char *expected_0_2[6] = {
        "fexpa  %z0.d -> %z0.d",   "fexpa  %z6.d -> %z5.d",   "fexpa  %z11.d -> %z10.d",
        "fexpa  %z17.d -> %z16.d", "fexpa  %z22.d -> %z21.d", "fexpa  %z31.d -> %z31.d",
    };
    TEST_LOOP(fexpa, fexpa_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zd_0_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_0_2[i], OPSZ_8));

    return success;
}

TEST_INSTR(ftmad_sve)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing FTMAD   <Zdn>.<Ts>, <Zdn>.<Ts>, <Zm>.<Ts>, #<imm> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z31 };
    uint imm3_0_0[6] = { 0, 3, 4, 6, 7, 7 };
    const char *expected_0_0[6] = {
        "ftmad  %z0.h %z0.h $0x00 -> %z0.h",    "ftmad  %z5.h %z6.h $0x03 -> %z5.h",
        "ftmad  %z10.h %z11.h $0x04 -> %z10.h", "ftmad  %z16.h %z17.h $0x06 -> %z16.h",
        "ftmad  %z21.h %z22.h $0x07 -> %z21.h", "ftmad  %z31.h %z31.h $0x07 -> %z31.h",
    };
    TEST_LOOP(ftmad, ftmad_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z31 };
    uint imm3_0_1[6] = { 0, 3, 4, 6, 7, 7 };
    const char *expected_0_1[6] = {
        "ftmad  %z0.s %z0.s $0x00 -> %z0.s",    "ftmad  %z5.s %z6.s $0x03 -> %z5.s",
        "ftmad  %z10.s %z11.s $0x04 -> %z10.s", "ftmad  %z16.s %z17.s $0x06 -> %z16.s",
        "ftmad  %z21.s %z22.s $0x07 -> %z21.s", "ftmad  %z31.s %z31.s $0x07 -> %z31.s",
    };
    TEST_LOOP(ftmad, ftmad_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_3b));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z31 };
    uint imm3_0_2[6] = { 0, 3, 4, 6, 7, 7 };
    const char *expected_0_2[6] = {
        "ftmad  %z0.d %z0.d $0x00 -> %z0.d",    "ftmad  %z5.d %z6.d $0x03 -> %z5.d",
        "ftmad  %z10.d %z11.d $0x04 -> %z10.d", "ftmad  %z16.d %z17.d $0x06 -> %z16.d",
        "ftmad  %z21.d %z22.d $0x07 -> %z21.d", "ftmad  %z31.d %z31.d $0x07 -> %z31.d",
    };
    TEST_LOOP(ftmad, ftmad_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_3b));

    return success;
}

TEST_INSTR(ftsmul_sve)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing FTSMUL  <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zd_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z31 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "ftsmul %z0.h %z0.h -> %z0.h",    "ftsmul %z6.h %z7.h -> %z5.h",
        "ftsmul %z11.h %z12.h -> %z10.h", "ftsmul %z17.h %z18.h -> %z16.h",
        "ftsmul %z22.h %z23.h -> %z21.h", "ftsmul %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(ftsmul, ftsmul_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zd_0_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_0_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_2));

    reg_id_t Zd_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Zn_0_1[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z31 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "ftsmul %z0.s %z0.s -> %z0.s",    "ftsmul %z6.s %z7.s -> %z5.s",
        "ftsmul %z11.s %z12.s -> %z10.s", "ftsmul %z17.s %z18.s -> %z16.s",
        "ftsmul %z22.s %z23.s -> %z21.s", "ftsmul %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(ftsmul, ftsmul_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zd_0_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_0_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_4));

    reg_id_t Zd_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Zn_0_2[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z31 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_2[6] = {
        "ftsmul %z0.d %z0.d -> %z0.d",    "ftsmul %z6.d %z7.d -> %z5.d",
        "ftsmul %z11.d %z12.d -> %z10.d", "ftsmul %z17.d %z18.d -> %z16.d",
        "ftsmul %z22.d %z23.d -> %z21.d", "ftsmul %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(ftsmul, ftsmul_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zd_0_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_0_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_8));

    return success;
}

TEST_INSTR(ftssel_sve)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing FTSSEL  <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zd_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z31 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "ftssel %z0.h %z0.h -> %z0.h",    "ftssel %z6.h %z7.h -> %z5.h",
        "ftssel %z11.h %z12.h -> %z10.h", "ftssel %z17.h %z18.h -> %z16.h",
        "ftssel %z22.h %z23.h -> %z21.h", "ftssel %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(ftssel, ftssel_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zd_0_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_0_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_2));

    reg_id_t Zd_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Zn_0_1[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z31 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "ftssel %z0.s %z0.s -> %z0.s",    "ftssel %z6.s %z7.s -> %z5.s",
        "ftssel %z11.s %z12.s -> %z10.s", "ftssel %z17.s %z18.s -> %z16.s",
        "ftssel %z22.s %z23.s -> %z21.s", "ftssel %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(ftssel, ftssel_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zd_0_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_0_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_4));

    reg_id_t Zd_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Zn_0_2[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z31 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_2[6] = {
        "ftssel %z0.d %z0.d -> %z0.d",    "ftssel %z6.d %z7.d -> %z5.d",
        "ftssel %z11.d %z12.d -> %z10.d", "ftssel %z17.d %z18.d -> %z16.d",
        "ftssel %z22.d %z23.d -> %z21.d", "ftssel %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(ftssel, ftssel_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zd_0_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_0_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_8));

    return success;
}

TEST_INSTR(abs_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing ABS     <Zd>.<Ts>, <Pg>/M, <Zn>.<Ts> */
    reg_id_t Zd_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "abs    %p0/m %z0.b -> %z0.b",   "abs    %p2/m %z7.b -> %z5.b",
        "abs    %p3/m %z12.b -> %z10.b", "abs    %p5/m %z18.b -> %z16.b",
        "abs    %p6/m %z23.b -> %z21.b", "abs    %p7/m %z31.b -> %z31.b",
    };
    TEST_LOOP(abs, abs_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zd_0_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pg_0_0[i], true),
              opnd_create_reg_element_vector(Zn_0_0[i], OPSZ_1));

    reg_id_t Zd_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "abs    %p0/m %z0.h -> %z0.h",   "abs    %p2/m %z7.h -> %z5.h",
        "abs    %p3/m %z12.h -> %z10.h", "abs    %p5/m %z18.h -> %z16.h",
        "abs    %p6/m %z23.h -> %z21.h", "abs    %p7/m %z31.h -> %z31.h",
    };
    TEST_LOOP(abs, abs_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zd_0_1[i], OPSZ_2),
              opnd_create_predicate_reg(Pg_0_1[i], true),
              opnd_create_reg_element_vector(Zn_0_1[i], OPSZ_2));

    reg_id_t Zd_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_2[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_2[6] = {
        "abs    %p0/m %z0.s -> %z0.s",   "abs    %p2/m %z7.s -> %z5.s",
        "abs    %p3/m %z12.s -> %z10.s", "abs    %p5/m %z18.s -> %z16.s",
        "abs    %p6/m %z23.s -> %z21.s", "abs    %p7/m %z31.s -> %z31.s",
    };
    TEST_LOOP(abs, abs_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zd_0_2[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_2[i], true),
              opnd_create_reg_element_vector(Zn_0_2[i], OPSZ_4));

    reg_id_t Zd_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_3[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_3[6] = {
        "abs    %p0/m %z0.d -> %z0.d",   "abs    %p2/m %z7.d -> %z5.d",
        "abs    %p3/m %z12.d -> %z10.d", "abs    %p5/m %z18.d -> %z16.d",
        "abs    %p6/m %z23.d -> %z21.d", "abs    %p7/m %z31.d -> %z31.d",
    };
    TEST_LOOP(abs, abs_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zd_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zn_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(cnot_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing CNOT    <Zd>.<Ts>, <Pg>/M, <Zn>.<Ts> */
    reg_id_t Zd_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "cnot   %p0/m %z0.b -> %z0.b",   "cnot   %p2/m %z7.b -> %z5.b",
        "cnot   %p3/m %z12.b -> %z10.b", "cnot   %p5/m %z18.b -> %z16.b",
        "cnot   %p6/m %z23.b -> %z21.b", "cnot   %p7/m %z31.b -> %z31.b",
    };
    TEST_LOOP(cnot, cnot_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zd_0_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pg_0_0[i], true),
              opnd_create_reg_element_vector(Zn_0_0[i], OPSZ_1));

    reg_id_t Zd_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "cnot   %p0/m %z0.h -> %z0.h",   "cnot   %p2/m %z7.h -> %z5.h",
        "cnot   %p3/m %z12.h -> %z10.h", "cnot   %p5/m %z18.h -> %z16.h",
        "cnot   %p6/m %z23.h -> %z21.h", "cnot   %p7/m %z31.h -> %z31.h",
    };
    TEST_LOOP(cnot, cnot_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zd_0_1[i], OPSZ_2),
              opnd_create_predicate_reg(Pg_0_1[i], true),
              opnd_create_reg_element_vector(Zn_0_1[i], OPSZ_2));

    reg_id_t Zd_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_2[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_2[6] = {
        "cnot   %p0/m %z0.s -> %z0.s",   "cnot   %p2/m %z7.s -> %z5.s",
        "cnot   %p3/m %z12.s -> %z10.s", "cnot   %p5/m %z18.s -> %z16.s",
        "cnot   %p6/m %z23.s -> %z21.s", "cnot   %p7/m %z31.s -> %z31.s",
    };
    TEST_LOOP(cnot, cnot_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zd_0_2[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_2[i], true),
              opnd_create_reg_element_vector(Zn_0_2[i], OPSZ_4));

    reg_id_t Zd_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_3[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_3[6] = {
        "cnot   %p0/m %z0.d -> %z0.d",   "cnot   %p2/m %z7.d -> %z5.d",
        "cnot   %p3/m %z12.d -> %z10.d", "cnot   %p5/m %z18.d -> %z16.d",
        "cnot   %p6/m %z23.d -> %z21.d", "cnot   %p7/m %z31.d -> %z31.d",
    };
    TEST_LOOP(cnot, cnot_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zd_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zn_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(neg_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing NEG     <Zd>.<Ts>, <Pg>/M, <Zn>.<Ts> */
    reg_id_t Zd_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "neg    %p0/m %z0.b -> %z0.b",   "neg    %p2/m %z7.b -> %z5.b",
        "neg    %p3/m %z12.b -> %z10.b", "neg    %p5/m %z18.b -> %z16.b",
        "neg    %p6/m %z23.b -> %z21.b", "neg    %p7/m %z31.b -> %z31.b",
    };
    TEST_LOOP(neg, neg_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zd_0_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pg_0_0[i], true),
              opnd_create_reg_element_vector(Zn_0_0[i], OPSZ_1));

    reg_id_t Zd_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "neg    %p0/m %z0.h -> %z0.h",   "neg    %p2/m %z7.h -> %z5.h",
        "neg    %p3/m %z12.h -> %z10.h", "neg    %p5/m %z18.h -> %z16.h",
        "neg    %p6/m %z23.h -> %z21.h", "neg    %p7/m %z31.h -> %z31.h",
    };
    TEST_LOOP(neg, neg_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zd_0_1[i], OPSZ_2),
              opnd_create_predicate_reg(Pg_0_1[i], true),
              opnd_create_reg_element_vector(Zn_0_1[i], OPSZ_2));

    reg_id_t Zd_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_2[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_2[6] = {
        "neg    %p0/m %z0.s -> %z0.s",   "neg    %p2/m %z7.s -> %z5.s",
        "neg    %p3/m %z12.s -> %z10.s", "neg    %p5/m %z18.s -> %z16.s",
        "neg    %p6/m %z23.s -> %z21.s", "neg    %p7/m %z31.s -> %z31.s",
    };
    TEST_LOOP(neg, neg_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zd_0_2[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_2[i], true),
              opnd_create_reg_element_vector(Zn_0_2[i], OPSZ_4));

    reg_id_t Zd_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_3[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_3[6] = {
        "neg    %p0/m %z0.d -> %z0.d",   "neg    %p2/m %z7.d -> %z5.d",
        "neg    %p3/m %z12.d -> %z10.d", "neg    %p5/m %z18.d -> %z16.d",
        "neg    %p6/m %z23.d -> %z21.d", "neg    %p7/m %z31.d -> %z31.d",
    };
    TEST_LOOP(neg, neg_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zd_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zn_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(sabd_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing SABD    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "sabd   %p0/m %z0.b %z0.b -> %z0.b",    "sabd   %p2/m %z5.b %z7.b -> %z5.b",
        "sabd   %p3/m %z10.b %z12.b -> %z10.b", "sabd   %p5/m %z16.b %z18.b -> %z16.b",
        "sabd   %p6/m %z21.b %z23.b -> %z21.b", "sabd   %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(sabd, sabd_sve_pred, 6, expected_0_0[i],
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
        "sabd   %p0/m %z0.h %z0.h -> %z0.h",    "sabd   %p2/m %z5.h %z7.h -> %z5.h",
        "sabd   %p3/m %z10.h %z12.h -> %z10.h", "sabd   %p5/m %z16.h %z18.h -> %z16.h",
        "sabd   %p6/m %z21.h %z23.h -> %z21.h", "sabd   %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(sabd, sabd_sve_pred, 6, expected_0_1[i],
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
        "sabd   %p0/m %z0.s %z0.s -> %z0.s",    "sabd   %p2/m %z5.s %z7.s -> %z5.s",
        "sabd   %p3/m %z10.s %z12.s -> %z10.s", "sabd   %p5/m %z16.s %z18.s -> %z16.s",
        "sabd   %p6/m %z21.s %z23.s -> %z21.s", "sabd   %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sabd, sabd_sve_pred, 6, expected_0_2[i],
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
        "sabd   %p0/m %z0.d %z0.d -> %z0.d",    "sabd   %p2/m %z5.d %z7.d -> %z5.d",
        "sabd   %p3/m %z10.d %z12.d -> %z10.d", "sabd   %p5/m %z16.d %z18.d -> %z16.d",
        "sabd   %p6/m %z21.d %z23.d -> %z21.d", "sabd   %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sabd, sabd_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(smax_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing SMAX    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "smax   %p0/m %z0.b %z0.b -> %z0.b",    "smax   %p2/m %z5.b %z7.b -> %z5.b",
        "smax   %p3/m %z10.b %z12.b -> %z10.b", "smax   %p5/m %z16.b %z18.b -> %z16.b",
        "smax   %p6/m %z21.b %z23.b -> %z21.b", "smax   %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(smax, smax_sve_pred, 6, expected_0_0[i],
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
        "smax   %p0/m %z0.h %z0.h -> %z0.h",    "smax   %p2/m %z5.h %z7.h -> %z5.h",
        "smax   %p3/m %z10.h %z12.h -> %z10.h", "smax   %p5/m %z16.h %z18.h -> %z16.h",
        "smax   %p6/m %z21.h %z23.h -> %z21.h", "smax   %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(smax, smax_sve_pred, 6, expected_0_1[i],
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
        "smax   %p0/m %z0.s %z0.s -> %z0.s",    "smax   %p2/m %z5.s %z7.s -> %z5.s",
        "smax   %p3/m %z10.s %z12.s -> %z10.s", "smax   %p5/m %z16.s %z18.s -> %z16.s",
        "smax   %p6/m %z21.s %z23.s -> %z21.s", "smax   %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(smax, smax_sve_pred, 6, expected_0_2[i],
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
        "smax   %p0/m %z0.d %z0.d -> %z0.d",    "smax   %p2/m %z5.d %z7.d -> %z5.d",
        "smax   %p3/m %z10.d %z12.d -> %z10.d", "smax   %p5/m %z16.d %z18.d -> %z16.d",
        "smax   %p6/m %z21.d %z23.d -> %z21.d", "smax   %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(smax, smax_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(smax_sve)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing SMAX    <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    int imm8_0_0[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_0[6] = {
        "smax   %z0.b $0x80 -> %z0.b",   "smax   %z5.b $0xab -> %z5.b",
        "smax   %z10.b $0xd6 -> %z10.b", "smax   %z16.b $0x01 -> %z16.b",
        "smax   %z21.b $0x2b -> %z21.b", "smax   %z31.b $0x7f -> %z31.b",
    };
    TEST_LOOP(smax, smax_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_immed_int(imm8_0_0[i], OPSZ_1));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    int imm8_0_1[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_1[6] = {
        "smax   %z0.h $0x80 -> %z0.h",   "smax   %z5.h $0xab -> %z5.h",
        "smax   %z10.h $0xd6 -> %z10.h", "smax   %z16.h $0x01 -> %z16.h",
        "smax   %z21.h $0x2b -> %z21.h", "smax   %z31.h $0x7f -> %z31.h",
    };
    TEST_LOOP(smax, smax_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_immed_int(imm8_0_1[i], OPSZ_1));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    int imm8_0_2[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_2[6] = {
        "smax   %z0.s $0x80 -> %z0.s",   "smax   %z5.s $0xab -> %z5.s",
        "smax   %z10.s $0xd6 -> %z10.s", "smax   %z16.s $0x01 -> %z16.s",
        "smax   %z21.s $0x2b -> %z21.s", "smax   %z31.s $0x7f -> %z31.s",
    };
    TEST_LOOP(smax, smax_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_immed_int(imm8_0_2[i], OPSZ_1));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    int imm8_0_3[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_3[6] = {
        "smax   %z0.d $0x80 -> %z0.d",   "smax   %z5.d $0xab -> %z5.d",
        "smax   %z10.d $0xd6 -> %z10.d", "smax   %z16.d $0x01 -> %z16.d",
        "smax   %z21.d $0x2b -> %z21.d", "smax   %z31.d $0x7f -> %z31.d",
    };
    TEST_LOOP(smax, smax_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_immed_int(imm8_0_3[i], OPSZ_1));

    return success;
}

TEST_INSTR(smin_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing SMIN    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "smin   %p0/m %z0.b %z0.b -> %z0.b",    "smin   %p2/m %z5.b %z7.b -> %z5.b",
        "smin   %p3/m %z10.b %z12.b -> %z10.b", "smin   %p5/m %z16.b %z18.b -> %z16.b",
        "smin   %p6/m %z21.b %z23.b -> %z21.b", "smin   %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(smin, smin_sve_pred, 6, expected_0_0[i],
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
        "smin   %p0/m %z0.h %z0.h -> %z0.h",    "smin   %p2/m %z5.h %z7.h -> %z5.h",
        "smin   %p3/m %z10.h %z12.h -> %z10.h", "smin   %p5/m %z16.h %z18.h -> %z16.h",
        "smin   %p6/m %z21.h %z23.h -> %z21.h", "smin   %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(smin, smin_sve_pred, 6, expected_0_1[i],
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
        "smin   %p0/m %z0.s %z0.s -> %z0.s",    "smin   %p2/m %z5.s %z7.s -> %z5.s",
        "smin   %p3/m %z10.s %z12.s -> %z10.s", "smin   %p5/m %z16.s %z18.s -> %z16.s",
        "smin   %p6/m %z21.s %z23.s -> %z21.s", "smin   %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(smin, smin_sve_pred, 6, expected_0_2[i],
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
        "smin   %p0/m %z0.d %z0.d -> %z0.d",    "smin   %p2/m %z5.d %z7.d -> %z5.d",
        "smin   %p3/m %z10.d %z12.d -> %z10.d", "smin   %p5/m %z16.d %z18.d -> %z16.d",
        "smin   %p6/m %z21.d %z23.d -> %z21.d", "smin   %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(smin, smin_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(smin_sve)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing SMIN    <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    int imm8_0_0[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_0[6] = {
        "smin   %z0.b $0x80 -> %z0.b",   "smin   %z5.b $0xab -> %z5.b",
        "smin   %z10.b $0xd6 -> %z10.b", "smin   %z16.b $0x01 -> %z16.b",
        "smin   %z21.b $0x2b -> %z21.b", "smin   %z31.b $0x7f -> %z31.b",
    };
    TEST_LOOP(smin, smin_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_immed_int(imm8_0_0[i], OPSZ_1));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    int imm8_0_1[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_1[6] = {
        "smin   %z0.h $0x80 -> %z0.h",   "smin   %z5.h $0xab -> %z5.h",
        "smin   %z10.h $0xd6 -> %z10.h", "smin   %z16.h $0x01 -> %z16.h",
        "smin   %z21.h $0x2b -> %z21.h", "smin   %z31.h $0x7f -> %z31.h",
    };
    TEST_LOOP(smin, smin_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_immed_int(imm8_0_1[i], OPSZ_1));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    int imm8_0_2[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_2[6] = {
        "smin   %z0.s $0x80 -> %z0.s",   "smin   %z5.s $0xab -> %z5.s",
        "smin   %z10.s $0xd6 -> %z10.s", "smin   %z16.s $0x01 -> %z16.s",
        "smin   %z21.s $0x2b -> %z21.s", "smin   %z31.s $0x7f -> %z31.s",
    };
    TEST_LOOP(smin, smin_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_immed_int(imm8_0_2[i], OPSZ_1));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    int imm8_0_3[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_3[6] = {
        "smin   %z0.d $0x80 -> %z0.d",   "smin   %z5.d $0xab -> %z5.d",
        "smin   %z10.d $0xd6 -> %z10.d", "smin   %z16.d $0x01 -> %z16.d",
        "smin   %z21.d $0x2b -> %z21.d", "smin   %z31.d $0x7f -> %z31.d",
    };
    TEST_LOOP(smin, smin_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_immed_int(imm8_0_3[i], OPSZ_1));

    return success;
}

TEST_INSTR(uabd_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing UABD    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "uabd   %p0/m %z0.b %z0.b -> %z0.b",    "uabd   %p2/m %z5.b %z7.b -> %z5.b",
        "uabd   %p3/m %z10.b %z12.b -> %z10.b", "uabd   %p5/m %z16.b %z18.b -> %z16.b",
        "uabd   %p6/m %z21.b %z23.b -> %z21.b", "uabd   %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(uabd, uabd_sve_pred, 6, expected_0_0[i],
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
        "uabd   %p0/m %z0.h %z0.h -> %z0.h",    "uabd   %p2/m %z5.h %z7.h -> %z5.h",
        "uabd   %p3/m %z10.h %z12.h -> %z10.h", "uabd   %p5/m %z16.h %z18.h -> %z16.h",
        "uabd   %p6/m %z21.h %z23.h -> %z21.h", "uabd   %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(uabd, uabd_sve_pred, 6, expected_0_1[i],
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
        "uabd   %p0/m %z0.s %z0.s -> %z0.s",    "uabd   %p2/m %z5.s %z7.s -> %z5.s",
        "uabd   %p3/m %z10.s %z12.s -> %z10.s", "uabd   %p5/m %z16.s %z18.s -> %z16.s",
        "uabd   %p6/m %z21.s %z23.s -> %z21.s", "uabd   %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(uabd, uabd_sve_pred, 6, expected_0_2[i],
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
        "uabd   %p0/m %z0.d %z0.d -> %z0.d",    "uabd   %p2/m %z5.d %z7.d -> %z5.d",
        "uabd   %p3/m %z10.d %z12.d -> %z10.d", "uabd   %p5/m %z16.d %z18.d -> %z16.d",
        "uabd   %p6/m %z21.d %z23.d -> %z21.d", "uabd   %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(uabd, uabd_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(facge_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing FACGE   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Pd_0_0[6] = { DR_REG_P0, DR_REG_P2,  DR_REG_P5,
                           DR_REG_P8, DR_REG_P10, DR_REG_P15 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "facge  %p0/z %z0.h %z0.h -> %p0.h",    "facge  %p2/z %z7.h %z8.h -> %p2.h",
        "facge  %p3/z %z12.h %z13.h -> %p5.h",  "facge  %p5/z %z18.h %z19.h -> %p8.h",
        "facge  %p6/z %z23.h %z24.h -> %p10.h", "facge  %p7/z %z31.h %z31.h -> %p15.h",
    };
    TEST_LOOP(facge, facge_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pd_0_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pg_0_0[i], false),
              opnd_create_reg_element_vector(Zn_0_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_2));

    reg_id_t Pd_0_1[6] = { DR_REG_P0, DR_REG_P2,  DR_REG_P5,
                           DR_REG_P8, DR_REG_P10, DR_REG_P15 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "facge  %p0/z %z0.s %z0.s -> %p0.s",    "facge  %p2/z %z7.s %z8.s -> %p2.s",
        "facge  %p3/z %z12.s %z13.s -> %p5.s",  "facge  %p5/z %z18.s %z19.s -> %p8.s",
        "facge  %p6/z %z23.s %z24.s -> %p10.s", "facge  %p7/z %z31.s %z31.s -> %p15.s",
    };
    TEST_LOOP(facge, facge_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pd_0_1[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_1[i], false),
              opnd_create_reg_element_vector(Zn_0_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_4));

    reg_id_t Pd_0_2[6] = { DR_REG_P0, DR_REG_P2,  DR_REG_P5,
                           DR_REG_P8, DR_REG_P10, DR_REG_P15 };
    reg_id_t Pg_0_2[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_2[6] = {
        "facge  %p0/z %z0.d %z0.d -> %p0.d",    "facge  %p2/z %z7.d %z8.d -> %p2.d",
        "facge  %p3/z %z12.d %z13.d -> %p5.d",  "facge  %p5/z %z18.d %z19.d -> %p8.d",
        "facge  %p6/z %z23.d %z24.d -> %p10.d", "facge  %p7/z %z31.d %z31.d -> %p15.d",
    };
    TEST_LOOP(facge, facge_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pd_0_2[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_2[i], false),
              opnd_create_reg_element_vector(Zn_0_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_8));

    return success;
}

TEST_INSTR(facgt_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing FACGT   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Pd_0_0[6] = { DR_REG_P0, DR_REG_P2,  DR_REG_P5,
                           DR_REG_P8, DR_REG_P10, DR_REG_P15 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "facgt  %p0/z %z0.h %z0.h -> %p0.h",    "facgt  %p2/z %z7.h %z8.h -> %p2.h",
        "facgt  %p3/z %z12.h %z13.h -> %p5.h",  "facgt  %p5/z %z18.h %z19.h -> %p8.h",
        "facgt  %p6/z %z23.h %z24.h -> %p10.h", "facgt  %p7/z %z31.h %z31.h -> %p15.h",
    };
    TEST_LOOP(facgt, facgt_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pd_0_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pg_0_0[i], false),
              opnd_create_reg_element_vector(Zn_0_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_2));

    reg_id_t Pd_0_1[6] = { DR_REG_P0, DR_REG_P2,  DR_REG_P5,
                           DR_REG_P8, DR_REG_P10, DR_REG_P15 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "facgt  %p0/z %z0.s %z0.s -> %p0.s",    "facgt  %p2/z %z7.s %z8.s -> %p2.s",
        "facgt  %p3/z %z12.s %z13.s -> %p5.s",  "facgt  %p5/z %z18.s %z19.s -> %p8.s",
        "facgt  %p6/z %z23.s %z24.s -> %p10.s", "facgt  %p7/z %z31.s %z31.s -> %p15.s",
    };
    TEST_LOOP(facgt, facgt_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pd_0_1[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_1[i], false),
              opnd_create_reg_element_vector(Zn_0_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_4));

    reg_id_t Pd_0_2[6] = { DR_REG_P0, DR_REG_P2,  DR_REG_P5,
                           DR_REG_P8, DR_REG_P10, DR_REG_P15 };
    reg_id_t Pg_0_2[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zn_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z8,  DR_REG_Z13,
                           DR_REG_Z19, DR_REG_Z24, DR_REG_Z31 };
    const char *expected_0_2[6] = {
        "facgt  %p0/z %z0.d %z0.d -> %p0.d",    "facgt  %p2/z %z7.d %z8.d -> %p2.d",
        "facgt  %p3/z %z12.d %z13.d -> %p5.d",  "facgt  %p5/z %z18.d %z19.d -> %p8.d",
        "facgt  %p6/z %z23.d %z24.d -> %p10.d", "facgt  %p7/z %z31.d %z31.d -> %p15.d",
    };
    TEST_LOOP(facgt, facgt_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pd_0_2[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_2[i], false),
              opnd_create_reg_element_vector(Zn_0_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zm_0_2[i], OPSZ_8));

    return success;
}

TEST_INSTR(sdiv_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing SDIV    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "sdiv   %p0/m %z0.s %z0.s -> %z0.s",    "sdiv   %p2/m %z5.s %z7.s -> %z5.s",
        "sdiv   %p3/m %z10.s %z12.s -> %z10.s", "sdiv   %p5/m %z16.s %z18.s -> %z16.s",
        "sdiv   %p6/m %z21.s %z23.s -> %z21.s", "sdiv   %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sdiv, sdiv_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_0[i], true),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "sdiv   %p0/m %z0.d %z0.d -> %z0.d",    "sdiv   %p2/m %z5.d %z7.d -> %z5.d",
        "sdiv   %p3/m %z10.d %z12.d -> %z10.d", "sdiv   %p5/m %z16.d %z18.d -> %z16.d",
        "sdiv   %p6/m %z21.d %z23.d -> %z21.d", "sdiv   %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sdiv, sdiv_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_1[i], true),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_8));

    return success;
}

TEST_INSTR(sdivr_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing SDIVR   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "sdivr  %p0/m %z0.s %z0.s -> %z0.s",    "sdivr  %p2/m %z5.s %z7.s -> %z5.s",
        "sdivr  %p3/m %z10.s %z12.s -> %z10.s", "sdivr  %p5/m %z16.s %z18.s -> %z16.s",
        "sdivr  %p6/m %z21.s %z23.s -> %z21.s", "sdivr  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sdivr, sdivr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_0[i], true),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "sdivr  %p0/m %z0.d %z0.d -> %z0.d",    "sdivr  %p2/m %z5.d %z7.d -> %z5.d",
        "sdivr  %p3/m %z10.d %z12.d -> %z10.d", "sdivr  %p5/m %z16.d %z18.d -> %z16.d",
        "sdivr  %p6/m %z21.d %z23.d -> %z21.d", "sdivr  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sdivr, sdivr_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_1[i], true),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_8));

    return success;
}

TEST_INSTR(udiv_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing UDIV    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "udiv   %p0/m %z0.s %z0.s -> %z0.s",    "udiv   %p2/m %z5.s %z7.s -> %z5.s",
        "udiv   %p3/m %z10.s %z12.s -> %z10.s", "udiv   %p5/m %z16.s %z18.s -> %z16.s",
        "udiv   %p6/m %z21.s %z23.s -> %z21.s", "udiv   %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(udiv, udiv_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_0[i], true),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "udiv   %p0/m %z0.d %z0.d -> %z0.d",    "udiv   %p2/m %z5.d %z7.d -> %z5.d",
        "udiv   %p3/m %z10.d %z12.d -> %z10.d", "udiv   %p5/m %z16.d %z18.d -> %z16.d",
        "udiv   %p6/m %z21.d %z23.d -> %z21.d", "udiv   %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(udiv, udiv_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_1[i], true),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_8));

    return success;
}

TEST_INSTR(udivr_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing UDIVR   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "udivr  %p0/m %z0.s %z0.s -> %z0.s",    "udivr  %p2/m %z5.s %z7.s -> %z5.s",
        "udivr  %p3/m %z10.s %z12.s -> %z10.s", "udivr  %p5/m %z16.s %z18.s -> %z16.s",
        "udivr  %p6/m %z21.s %z23.s -> %z21.s", "udivr  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(udivr, udivr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pg_0_0[i], true),
              opnd_create_reg_element_vector(Zm_0_0[i], OPSZ_4));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_1[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_1[6] = {
        "udivr  %p0/m %z0.d %z0.d -> %z0.d",    "udivr  %p2/m %z5.d %z7.d -> %z5.d",
        "udivr  %p3/m %z10.d %z12.d -> %z10.d", "udivr  %p5/m %z16.d %z18.d -> %z16.d",
        "udivr  %p6/m %z21.d %z23.d -> %z21.d", "udivr  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(udivr, udivr_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_1[i], true),
              opnd_create_reg_element_vector(Zm_0_1[i], OPSZ_8));

    return success;
}

TEST_INSTR(umax_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing UMAX    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "umax   %p0/m %z0.b %z0.b -> %z0.b",    "umax   %p2/m %z5.b %z7.b -> %z5.b",
        "umax   %p3/m %z10.b %z12.b -> %z10.b", "umax   %p5/m %z16.b %z18.b -> %z16.b",
        "umax   %p6/m %z21.b %z23.b -> %z21.b", "umax   %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(umax, umax_sve_pred, 6, expected_0_0[i],
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
        "umax   %p0/m %z0.h %z0.h -> %z0.h",    "umax   %p2/m %z5.h %z7.h -> %z5.h",
        "umax   %p3/m %z10.h %z12.h -> %z10.h", "umax   %p5/m %z16.h %z18.h -> %z16.h",
        "umax   %p6/m %z21.h %z23.h -> %z21.h", "umax   %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(umax, umax_sve_pred, 6, expected_0_1[i],
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
        "umax   %p0/m %z0.s %z0.s -> %z0.s",    "umax   %p2/m %z5.s %z7.s -> %z5.s",
        "umax   %p3/m %z10.s %z12.s -> %z10.s", "umax   %p5/m %z16.s %z18.s -> %z16.s",
        "umax   %p6/m %z21.s %z23.s -> %z21.s", "umax   %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(umax, umax_sve_pred, 6, expected_0_2[i],
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
        "umax   %p0/m %z0.d %z0.d -> %z0.d",    "umax   %p2/m %z5.d %z7.d -> %z5.d",
        "umax   %p3/m %z10.d %z12.d -> %z10.d", "umax   %p5/m %z16.d %z18.d -> %z16.d",
        "umax   %p6/m %z21.d %z23.d -> %z21.d", "umax   %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(umax, umax_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(umax_sve)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing UMAX    <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    uint imm8_0_0[6] = { 0, 43, 86, 129, 171, 255 };
    const char *expected_0_0[6] = {
        "umax   %z0.b $0x00 -> %z0.b",   "umax   %z5.b $0x2b -> %z5.b",
        "umax   %z10.b $0x56 -> %z10.b", "umax   %z16.b $0x81 -> %z16.b",
        "umax   %z21.b $0xab -> %z21.b", "umax   %z31.b $0xff -> %z31.b",
    };
    TEST_LOOP(umax, umax_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_immed_uint(imm8_0_0[i], OPSZ_1));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    uint imm8_0_1[6] = { 0, 43, 86, 129, 171, 255 };
    const char *expected_0_1[6] = {
        "umax   %z0.h $0x00 -> %z0.h",   "umax   %z5.h $0x2b -> %z5.h",
        "umax   %z10.h $0x56 -> %z10.h", "umax   %z16.h $0x81 -> %z16.h",
        "umax   %z21.h $0xab -> %z21.h", "umax   %z31.h $0xff -> %z31.h",
    };
    TEST_LOOP(umax, umax_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_immed_uint(imm8_0_1[i], OPSZ_1));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    uint imm8_0_2[6] = { 0, 43, 86, 129, 171, 255 };
    const char *expected_0_2[6] = {
        "umax   %z0.s $0x00 -> %z0.s",   "umax   %z5.s $0x2b -> %z5.s",
        "umax   %z10.s $0x56 -> %z10.s", "umax   %z16.s $0x81 -> %z16.s",
        "umax   %z21.s $0xab -> %z21.s", "umax   %z31.s $0xff -> %z31.s",
    };
    TEST_LOOP(umax, umax_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_immed_uint(imm8_0_2[i], OPSZ_1));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    uint imm8_0_3[6] = { 0, 43, 86, 129, 171, 255 };
    const char *expected_0_3[6] = {
        "umax   %z0.d $0x00 -> %z0.d",   "umax   %z5.d $0x2b -> %z5.d",
        "umax   %z10.d $0x56 -> %z10.d", "umax   %z16.d $0x81 -> %z16.d",
        "umax   %z21.d $0xab -> %z21.d", "umax   %z31.d $0xff -> %z31.d",
    };
    TEST_LOOP(umax, umax_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_immed_uint(imm8_0_3[i], OPSZ_1));

    return success;
}

TEST_INSTR(umin_sve_pred)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing UMIN    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    reg_id_t Pg_0_0[6] = { DR_REG_P0, DR_REG_P2, DR_REG_P3,
                           DR_REG_P5, DR_REG_P6, DR_REG_P7 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z18, DR_REG_Z23, DR_REG_Z31 };
    const char *expected_0_0[6] = {
        "umin   %p0/m %z0.b %z0.b -> %z0.b",    "umin   %p2/m %z5.b %z7.b -> %z5.b",
        "umin   %p3/m %z10.b %z12.b -> %z10.b", "umin   %p5/m %z16.b %z18.b -> %z16.b",
        "umin   %p6/m %z21.b %z23.b -> %z21.b", "umin   %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(umin, umin_sve_pred, 6, expected_0_0[i],
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
        "umin   %p0/m %z0.h %z0.h -> %z0.h",    "umin   %p2/m %z5.h %z7.h -> %z5.h",
        "umin   %p3/m %z10.h %z12.h -> %z10.h", "umin   %p5/m %z16.h %z18.h -> %z16.h",
        "umin   %p6/m %z21.h %z23.h -> %z21.h", "umin   %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(umin, umin_sve_pred, 6, expected_0_1[i],
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
        "umin   %p0/m %z0.s %z0.s -> %z0.s",    "umin   %p2/m %z5.s %z7.s -> %z5.s",
        "umin   %p3/m %z10.s %z12.s -> %z10.s", "umin   %p5/m %z16.s %z18.s -> %z16.s",
        "umin   %p6/m %z21.s %z23.s -> %z21.s", "umin   %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(umin, umin_sve_pred, 6, expected_0_2[i],
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
        "umin   %p0/m %z0.d %z0.d -> %z0.d",    "umin   %p2/m %z5.d %z7.d -> %z5.d",
        "umin   %p3/m %z10.d %z12.d -> %z10.d", "umin   %p5/m %z16.d %z18.d -> %z16.d",
        "umin   %p6/m %z21.d %z23.d -> %z21.d", "umin   %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(umin, umin_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_predicate_reg(Pg_0_3[i], true),
              opnd_create_reg_element_vector(Zm_0_3[i], OPSZ_8));

    return success;
}

TEST_INSTR(umin_sve)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* Testing UMIN    <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm> */
    reg_id_t Zdn_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    uint imm8_0_0[6] = { 0, 43, 86, 129, 171, 255 };
    const char *expected_0_0[6] = {
        "umin   %z0.b $0x00 -> %z0.b",   "umin   %z5.b $0x2b -> %z5.b",
        "umin   %z10.b $0x56 -> %z10.b", "umin   %z16.b $0x81 -> %z16.b",
        "umin   %z21.b $0xab -> %z21.b", "umin   %z31.b $0xff -> %z31.b",
    };
    TEST_LOOP(umin, umin_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zdn_0_0[i], OPSZ_1),
              opnd_create_immed_uint(imm8_0_0[i], OPSZ_1));

    reg_id_t Zdn_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    uint imm8_0_1[6] = { 0, 43, 86, 129, 171, 255 };
    const char *expected_0_1[6] = {
        "umin   %z0.h $0x00 -> %z0.h",   "umin   %z5.h $0x2b -> %z5.h",
        "umin   %z10.h $0x56 -> %z10.h", "umin   %z16.h $0x81 -> %z16.h",
        "umin   %z21.h $0xab -> %z21.h", "umin   %z31.h $0xff -> %z31.h",
    };
    TEST_LOOP(umin, umin_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zdn_0_1[i], OPSZ_2),
              opnd_create_immed_uint(imm8_0_1[i], OPSZ_1));

    reg_id_t Zdn_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    uint imm8_0_2[6] = { 0, 43, 86, 129, 171, 255 };
    const char *expected_0_2[6] = {
        "umin   %z0.s $0x00 -> %z0.s",   "umin   %z5.s $0x2b -> %z5.s",
        "umin   %z10.s $0x56 -> %z10.s", "umin   %z16.s $0x81 -> %z16.s",
        "umin   %z21.s $0xab -> %z21.s", "umin   %z31.s $0xff -> %z31.s",
    };
    TEST_LOOP(umin, umin_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zdn_0_2[i], OPSZ_4),
              opnd_create_immed_uint(imm8_0_2[i], OPSZ_1));

    reg_id_t Zdn_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                            DR_REG_Z16, DR_REG_Z21, DR_REG_Z31 };
    uint imm8_0_3[6] = { 0, 43, 86, 129, 171, 255 };
    const char *expected_0_3[6] = {
        "umin   %z0.d $0x00 -> %z0.d",   "umin   %z5.d $0x2b -> %z5.d",
        "umin   %z10.d $0x56 -> %z10.d", "umin   %z16.d $0x81 -> %z16.d",
        "umin   %z21.d $0xab -> %z21.d", "umin   %z31.d $0xff -> %z31.d",
    };
    TEST_LOOP(umin, umin_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zdn_0_3[i], OPSZ_8),
              opnd_create_immed_uint(imm8_0_3[i], OPSZ_1));

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

    RUN_INSTR_TEST(fexpa_sve);
    RUN_INSTR_TEST(ftmad_sve);
    RUN_INSTR_TEST(ftsmul_sve);
    RUN_INSTR_TEST(ftssel_sve);

    RUN_INSTR_TEST(abs_sve_pred);
    RUN_INSTR_TEST(cnot_sve_pred);
    RUN_INSTR_TEST(neg_sve_pred);
    RUN_INSTR_TEST(sabd_sve_pred);
    RUN_INSTR_TEST(smax_sve_pred);
    RUN_INSTR_TEST(smax_sve);
    RUN_INSTR_TEST(smin_sve_pred);
    RUN_INSTR_TEST(smin_sve);
    RUN_INSTR_TEST(uabd_sve_pred);

    RUN_INSTR_TEST(facge_sve_pred);
    RUN_INSTR_TEST(facgt_sve_pred);

    RUN_INSTR_TEST(sdiv_sve_pred);
    RUN_INSTR_TEST(sdivr_sve_pred);
    RUN_INSTR_TEST(udiv_sve_pred);
    RUN_INSTR_TEST(udivr_sve_pred);
    RUN_INSTR_TEST(umax_sve_pred);
    RUN_INSTR_TEST(umax_sve);
    RUN_INSTR_TEST(umin_sve_pred);
    RUN_INSTR_TEST(umin_sve);

    print("All sve tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
