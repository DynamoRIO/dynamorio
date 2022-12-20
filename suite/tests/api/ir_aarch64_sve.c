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
    /* Testing ADD     <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "add    %p0/m %z0.b %z0.b -> %z0.b",    "add    %p2/m %z5.b %z7.b -> %z5.b",
        "add    %p3/m %z10.b %z12.b -> %z10.b", "add    %p5/m %z16.b %z18.b -> %z16.b",
        "add    %p6/m %z21.b %z23.b -> %z21.b", "add    %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(add, add_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "add    %p0/m %z0.h %z0.h -> %z0.h",    "add    %p2/m %z5.h %z7.h -> %z5.h",
        "add    %p3/m %z10.h %z12.h -> %z10.h", "add    %p5/m %z16.h %z18.h -> %z16.h",
        "add    %p6/m %z21.h %z23.h -> %z21.h", "add    %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(add, add_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "add    %p0/m %z0.s %z0.s -> %z0.s",    "add    %p2/m %z5.s %z7.s -> %z5.s",
        "add    %p3/m %z10.s %z12.s -> %z10.s", "add    %p5/m %z16.s %z18.s -> %z16.s",
        "add    %p6/m %z21.s %z23.s -> %z21.s", "add    %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(add, add_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "add    %p0/m %z0.d %z0.d -> %z0.d",    "add    %p2/m %z5.d %z7.d -> %z5.d",
        "add    %p3/m %z10.d %z12.d -> %z10.d", "add    %p5/m %z16.d %z18.d -> %z16.d",
        "add    %p6/m %z21.d %z23.d -> %z21.d", "add    %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(add, add_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(add_sve_shift)
{
    /* Testing ADD     <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm>, <shift> */
    uint imm8_0_0[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_0[6] = { 0, 0, 0, 0, 0, 0 };
    const char *expected_0_0[6] = {
        "add    %z0.b $0x00 lsl $0x00 -> %z0.b",
        "add    %z5.b $0x2b lsl $0x00 -> %z5.b",
        "add    %z10.b $0x56 lsl $0x00 -> %z10.b",
        "add    %z16.b $0x81 lsl $0x00 -> %z16.b",
        "add    %z21.b $0xab lsl $0x00 -> %z21.b",
        "add    %z31.b $0xff lsl $0x00 -> %z31.b",
    };
    TEST_LOOP(add, add_sve_shift, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_immed_uint(imm8_0_0[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_0[i], OPSZ_1b));

    uint imm8_0_1[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_1[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_1[6] = {
        "add    %z0.h $0x00 lsl $0x08 -> %z0.h",
        "add    %z5.h $0x2b lsl $0x08 -> %z5.h",
        "add    %z10.h $0x56 lsl $0x08 -> %z10.h",
        "add    %z16.h $0x81 lsl $0x08 -> %z16.h",
        "add    %z21.h $0xab lsl $0x00 -> %z21.h",
        "add    %z31.h $0xff lsl $0x00 -> %z31.h",
    };
    TEST_LOOP(add, add_sve_shift, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_immed_uint(imm8_0_1[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_1[i], OPSZ_1b));

    uint imm8_0_2[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_2[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_2[6] = {
        "add    %z0.s $0x00 lsl $0x08 -> %z0.s",
        "add    %z5.s $0x2b lsl $0x08 -> %z5.s",
        "add    %z10.s $0x56 lsl $0x08 -> %z10.s",
        "add    %z16.s $0x81 lsl $0x08 -> %z16.s",
        "add    %z21.s $0xab lsl $0x00 -> %z21.s",
        "add    %z31.s $0xff lsl $0x00 -> %z31.s",
    };
    TEST_LOOP(add, add_sve_shift, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_immed_uint(imm8_0_2[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_2[i], OPSZ_1b));

    uint imm8_0_3[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_3[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_3[6] = {
        "add    %z0.d $0x00 lsl $0x08 -> %z0.d",
        "add    %z5.d $0x2b lsl $0x08 -> %z5.d",
        "add    %z10.d $0x56 lsl $0x08 -> %z10.d",
        "add    %z16.d $0x81 lsl $0x08 -> %z16.d",
        "add    %z21.d $0xab lsl $0x00 -> %z21.d",
        "add    %z31.d $0xff lsl $0x00 -> %z31.d",
    };
    TEST_LOOP(add, add_sve_shift, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_immed_uint(imm8_0_3[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_3[i], OPSZ_1b));
}

TEST_INSTR(add_sve)
{
    /* Testing ADD     <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "add    %z0.b %z0.b -> %z0.b",    "add    %z6.b %z7.b -> %z5.b",
        "add    %z11.b %z12.b -> %z10.b", "add    %z17.b %z18.b -> %z16.b",
        "add    %z22.b %z23.b -> %z21.b", "add    %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(add, add_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "add    %z0.h %z0.h -> %z0.h",    "add    %z6.h %z7.h -> %z5.h",
        "add    %z11.h %z12.h -> %z10.h", "add    %z17.h %z18.h -> %z16.h",
        "add    %z22.h %z23.h -> %z21.h", "add    %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(add, add_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "add    %z0.s %z0.s -> %z0.s",    "add    %z6.s %z7.s -> %z5.s",
        "add    %z11.s %z12.s -> %z10.s", "add    %z17.s %z18.s -> %z16.s",
        "add    %z22.s %z23.s -> %z21.s", "add    %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(add, add_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "add    %z0.d %z0.d -> %z0.d",    "add    %z6.d %z7.d -> %z5.d",
        "add    %z11.d %z12.d -> %z10.d", "add    %z17.d %z18.d -> %z16.d",
        "add    %z22.d %z23.d -> %z21.d", "add    %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(add, add_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(zip2_vector)
{
    /* Testing ZIP2    <Zd>.Q, <Zn>.Q, <Zm>.Q */
    const char *expected_0_0[6] = {
        "zip2   %z0.q %z0.q -> %z0.q",    "zip2   %z6.q %z7.q -> %z5.q",
        "zip2   %z11.q %z12.q -> %z10.q", "zip2   %z17.q %z18.q -> %z16.q",
        "zip2   %z22.q %z23.q -> %z21.q", "zip2   %z31.q %z31.q -> %z31.q",
    };
    TEST_LOOP(zip2, zip2_vector, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_16),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_16),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_16));
}

TEST_INSTR(movprfx_vector)
{
    /* Testing MOVPRFX <Zd>, <Zn> */
    const char *expected_0_0[6] = {
        "movprfx %z0 -> %z0",   "movprfx %z6 -> %z5",   "movprfx %z11 -> %z10",
        "movprfx %z17 -> %z16", "movprfx %z22 -> %z21", "movprfx %z31 -> %z31",
    };
    TEST_LOOP(movprfx, movprfx_vector, 6, expected_0_0[i],
              opnd_create_reg(Zn_six_offset_0[i]), opnd_create_reg(Zn_six_offset_1[i]));
}

TEST_INSTR(sqadd_sve_shift)
{
    /* Testing SQADD   <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm>, <shift> */
    uint imm8_0_0[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_0[6] = { 0, 0, 0, 0, 0, 0 };
    const char *expected_0_0[6] = {
        "sqadd  %z0.b $0x00 lsl $0x00 -> %z0.b",
        "sqadd  %z5.b $0x2b lsl $0x00 -> %z5.b",
        "sqadd  %z10.b $0x56 lsl $0x00 -> %z10.b",
        "sqadd  %z16.b $0x81 lsl $0x00 -> %z16.b",
        "sqadd  %z21.b $0xab lsl $0x00 -> %z21.b",
        "sqadd  %z31.b $0xff lsl $0x00 -> %z31.b",
    };
    TEST_LOOP(sqadd, sqadd_sve_shift, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_immed_uint(imm8_0_0[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_0[i], OPSZ_1b));

    uint imm8_0_1[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_1[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_1[6] = {
        "sqadd  %z0.h $0x00 lsl $0x08 -> %z0.h",
        "sqadd  %z5.h $0x2b lsl $0x08 -> %z5.h",
        "sqadd  %z10.h $0x56 lsl $0x08 -> %z10.h",
        "sqadd  %z16.h $0x81 lsl $0x08 -> %z16.h",
        "sqadd  %z21.h $0xab lsl $0x00 -> %z21.h",
        "sqadd  %z31.h $0xff lsl $0x00 -> %z31.h",
    };
    TEST_LOOP(sqadd, sqadd_sve_shift, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_immed_uint(imm8_0_1[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_1[i], OPSZ_1b));

    uint imm8_0_2[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_2[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_2[6] = {
        "sqadd  %z0.s $0x00 lsl $0x08 -> %z0.s",
        "sqadd  %z5.s $0x2b lsl $0x08 -> %z5.s",
        "sqadd  %z10.s $0x56 lsl $0x08 -> %z10.s",
        "sqadd  %z16.s $0x81 lsl $0x08 -> %z16.s",
        "sqadd  %z21.s $0xab lsl $0x00 -> %z21.s",
        "sqadd  %z31.s $0xff lsl $0x00 -> %z31.s",
    };
    TEST_LOOP(sqadd, sqadd_sve_shift, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_immed_uint(imm8_0_2[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_2[i], OPSZ_1b));

    uint imm8_0_3[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_3[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_3[6] = {
        "sqadd  %z0.d $0x00 lsl $0x08 -> %z0.d",
        "sqadd  %z5.d $0x2b lsl $0x08 -> %z5.d",
        "sqadd  %z10.d $0x56 lsl $0x08 -> %z10.d",
        "sqadd  %z16.d $0x81 lsl $0x08 -> %z16.d",
        "sqadd  %z21.d $0xab lsl $0x00 -> %z21.d",
        "sqadd  %z31.d $0xff lsl $0x00 -> %z31.d",
    };
    TEST_LOOP(sqadd, sqadd_sve_shift, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_immed_uint(imm8_0_3[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_3[i], OPSZ_1b));
}

TEST_INSTR(sqadd_sve)
{
    /* Testing SQADD   <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "sqadd  %z0.b %z0.b -> %z0.b",    "sqadd  %z6.b %z7.b -> %z5.b",
        "sqadd  %z11.b %z12.b -> %z10.b", "sqadd  %z17.b %z18.b -> %z16.b",
        "sqadd  %z22.b %z23.b -> %z21.b", "sqadd  %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(sqadd, sqadd_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "sqadd  %z0.h %z0.h -> %z0.h",    "sqadd  %z6.h %z7.h -> %z5.h",
        "sqadd  %z11.h %z12.h -> %z10.h", "sqadd  %z17.h %z18.h -> %z16.h",
        "sqadd  %z22.h %z23.h -> %z21.h", "sqadd  %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(sqadd, sqadd_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "sqadd  %z0.s %z0.s -> %z0.s",    "sqadd  %z6.s %z7.s -> %z5.s",
        "sqadd  %z11.s %z12.s -> %z10.s", "sqadd  %z17.s %z18.s -> %z16.s",
        "sqadd  %z22.s %z23.s -> %z21.s", "sqadd  %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sqadd, sqadd_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "sqadd  %z0.d %z0.d -> %z0.d",    "sqadd  %z6.d %z7.d -> %z5.d",
        "sqadd  %z11.d %z12.d -> %z10.d", "sqadd  %z17.d %z18.d -> %z16.d",
        "sqadd  %z22.d %z23.d -> %z21.d", "sqadd  %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sqadd, sqadd_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sqsub_sve_shift)
{
    /* Testing SQSUB   <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm>, <shift> */
    uint imm8_0_0[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_0[6] = { 0, 0, 0, 0, 0, 0 };
    const char *expected_0_0[6] = {
        "sqsub  %z0.b $0x00 lsl $0x00 -> %z0.b",
        "sqsub  %z5.b $0x2b lsl $0x00 -> %z5.b",
        "sqsub  %z10.b $0x56 lsl $0x00 -> %z10.b",
        "sqsub  %z16.b $0x81 lsl $0x00 -> %z16.b",
        "sqsub  %z21.b $0xab lsl $0x00 -> %z21.b",
        "sqsub  %z31.b $0xff lsl $0x00 -> %z31.b",
    };
    TEST_LOOP(sqsub, sqsub_sve_shift, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_immed_uint(imm8_0_0[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_0[i], OPSZ_1b));

    uint imm8_0_1[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_1[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_1[6] = {
        "sqsub  %z0.h $0x00 lsl $0x08 -> %z0.h",
        "sqsub  %z5.h $0x2b lsl $0x08 -> %z5.h",
        "sqsub  %z10.h $0x56 lsl $0x08 -> %z10.h",
        "sqsub  %z16.h $0x81 lsl $0x08 -> %z16.h",
        "sqsub  %z21.h $0xab lsl $0x00 -> %z21.h",
        "sqsub  %z31.h $0xff lsl $0x00 -> %z31.h",
    };
    TEST_LOOP(sqsub, sqsub_sve_shift, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_immed_uint(imm8_0_1[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_1[i], OPSZ_1b));

    uint imm8_0_2[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_2[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_2[6] = {
        "sqsub  %z0.s $0x00 lsl $0x08 -> %z0.s",
        "sqsub  %z5.s $0x2b lsl $0x08 -> %z5.s",
        "sqsub  %z10.s $0x56 lsl $0x08 -> %z10.s",
        "sqsub  %z16.s $0x81 lsl $0x08 -> %z16.s",
        "sqsub  %z21.s $0xab lsl $0x00 -> %z21.s",
        "sqsub  %z31.s $0xff lsl $0x00 -> %z31.s",
    };
    TEST_LOOP(sqsub, sqsub_sve_shift, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_immed_uint(imm8_0_2[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_2[i], OPSZ_1b));

    uint imm8_0_3[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_3[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_3[6] = {
        "sqsub  %z0.d $0x00 lsl $0x08 -> %z0.d",
        "sqsub  %z5.d $0x2b lsl $0x08 -> %z5.d",
        "sqsub  %z10.d $0x56 lsl $0x08 -> %z10.d",
        "sqsub  %z16.d $0x81 lsl $0x08 -> %z16.d",
        "sqsub  %z21.d $0xab lsl $0x00 -> %z21.d",
        "sqsub  %z31.d $0xff lsl $0x00 -> %z31.d",
    };
    TEST_LOOP(sqsub, sqsub_sve_shift, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_immed_uint(imm8_0_3[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_3[i], OPSZ_1b));
}

TEST_INSTR(sqsub_sve)
{
    /* Testing SQSUB   <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "sqsub  %z0.b %z0.b -> %z0.b",    "sqsub  %z6.b %z7.b -> %z5.b",
        "sqsub  %z11.b %z12.b -> %z10.b", "sqsub  %z17.b %z18.b -> %z16.b",
        "sqsub  %z22.b %z23.b -> %z21.b", "sqsub  %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(sqsub, sqsub_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "sqsub  %z0.h %z0.h -> %z0.h",    "sqsub  %z6.h %z7.h -> %z5.h",
        "sqsub  %z11.h %z12.h -> %z10.h", "sqsub  %z17.h %z18.h -> %z16.h",
        "sqsub  %z22.h %z23.h -> %z21.h", "sqsub  %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(sqsub, sqsub_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "sqsub  %z0.s %z0.s -> %z0.s",    "sqsub  %z6.s %z7.s -> %z5.s",
        "sqsub  %z11.s %z12.s -> %z10.s", "sqsub  %z17.s %z18.s -> %z16.s",
        "sqsub  %z22.s %z23.s -> %z21.s", "sqsub  %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sqsub, sqsub_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "sqsub  %z0.d %z0.d -> %z0.d",    "sqsub  %z6.d %z7.d -> %z5.d",
        "sqsub  %z11.d %z12.d -> %z10.d", "sqsub  %z17.d %z18.d -> %z16.d",
        "sqsub  %z22.d %z23.d -> %z21.d", "sqsub  %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sqsub, sqsub_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sub_sve_pred)
{
    /* Testing SUB     <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "sub    %p0/m %z0.b %z0.b -> %z0.b",    "sub    %p2/m %z5.b %z7.b -> %z5.b",
        "sub    %p3/m %z10.b %z12.b -> %z10.b", "sub    %p5/m %z16.b %z18.b -> %z16.b",
        "sub    %p6/m %z21.b %z23.b -> %z21.b", "sub    %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(sub, sub_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "sub    %p0/m %z0.h %z0.h -> %z0.h",    "sub    %p2/m %z5.h %z7.h -> %z5.h",
        "sub    %p3/m %z10.h %z12.h -> %z10.h", "sub    %p5/m %z16.h %z18.h -> %z16.h",
        "sub    %p6/m %z21.h %z23.h -> %z21.h", "sub    %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(sub, sub_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "sub    %p0/m %z0.s %z0.s -> %z0.s",    "sub    %p2/m %z5.s %z7.s -> %z5.s",
        "sub    %p3/m %z10.s %z12.s -> %z10.s", "sub    %p5/m %z16.s %z18.s -> %z16.s",
        "sub    %p6/m %z21.s %z23.s -> %z21.s", "sub    %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sub, sub_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "sub    %p0/m %z0.d %z0.d -> %z0.d",    "sub    %p2/m %z5.d %z7.d -> %z5.d",
        "sub    %p3/m %z10.d %z12.d -> %z10.d", "sub    %p5/m %z16.d %z18.d -> %z16.d",
        "sub    %p6/m %z21.d %z23.d -> %z21.d", "sub    %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sub, sub_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sub_sve_shift)
{
    /* Testing SUB     <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm>, <shift> */
    uint imm8_0_0[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_0[6] = { 0, 0, 0, 0, 0, 0 };
    const char *expected_0_0[6] = {
        "sub    %z0.b $0x00 lsl $0x00 -> %z0.b",
        "sub    %z5.b $0x2b lsl $0x00 -> %z5.b",
        "sub    %z10.b $0x56 lsl $0x00 -> %z10.b",
        "sub    %z16.b $0x81 lsl $0x00 -> %z16.b",
        "sub    %z21.b $0xab lsl $0x00 -> %z21.b",
        "sub    %z31.b $0xff lsl $0x00 -> %z31.b",
    };
    TEST_LOOP(sub, sub_sve_shift, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_immed_uint(imm8_0_0[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_0[i], OPSZ_1b));

    uint imm8_0_1[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_1[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_1[6] = {
        "sub    %z0.h $0x00 lsl $0x08 -> %z0.h",
        "sub    %z5.h $0x2b lsl $0x08 -> %z5.h",
        "sub    %z10.h $0x56 lsl $0x08 -> %z10.h",
        "sub    %z16.h $0x81 lsl $0x08 -> %z16.h",
        "sub    %z21.h $0xab lsl $0x00 -> %z21.h",
        "sub    %z31.h $0xff lsl $0x00 -> %z31.h",
    };
    TEST_LOOP(sub, sub_sve_shift, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_immed_uint(imm8_0_1[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_1[i], OPSZ_1b));

    uint imm8_0_2[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_2[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_2[6] = {
        "sub    %z0.s $0x00 lsl $0x08 -> %z0.s",
        "sub    %z5.s $0x2b lsl $0x08 -> %z5.s",
        "sub    %z10.s $0x56 lsl $0x08 -> %z10.s",
        "sub    %z16.s $0x81 lsl $0x08 -> %z16.s",
        "sub    %z21.s $0xab lsl $0x00 -> %z21.s",
        "sub    %z31.s $0xff lsl $0x00 -> %z31.s",
    };
    TEST_LOOP(sub, sub_sve_shift, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_immed_uint(imm8_0_2[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_2[i], OPSZ_1b));

    uint imm8_0_3[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_3[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_3[6] = {
        "sub    %z0.d $0x00 lsl $0x08 -> %z0.d",
        "sub    %z5.d $0x2b lsl $0x08 -> %z5.d",
        "sub    %z10.d $0x56 lsl $0x08 -> %z10.d",
        "sub    %z16.d $0x81 lsl $0x08 -> %z16.d",
        "sub    %z21.d $0xab lsl $0x00 -> %z21.d",
        "sub    %z31.d $0xff lsl $0x00 -> %z31.d",
    };
    TEST_LOOP(sub, sub_sve_shift, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_immed_uint(imm8_0_3[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_3[i], OPSZ_1b));
}

TEST_INSTR(sub_sve)
{
    /* Testing SUB     <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "sub    %z0.b %z0.b -> %z0.b",    "sub    %z6.b %z7.b -> %z5.b",
        "sub    %z11.b %z12.b -> %z10.b", "sub    %z17.b %z18.b -> %z16.b",
        "sub    %z22.b %z23.b -> %z21.b", "sub    %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(sub, sub_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "sub    %z0.h %z0.h -> %z0.h",    "sub    %z6.h %z7.h -> %z5.h",
        "sub    %z11.h %z12.h -> %z10.h", "sub    %z17.h %z18.h -> %z16.h",
        "sub    %z22.h %z23.h -> %z21.h", "sub    %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(sub, sub_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "sub    %z0.s %z0.s -> %z0.s",    "sub    %z6.s %z7.s -> %z5.s",
        "sub    %z11.s %z12.s -> %z10.s", "sub    %z17.s %z18.s -> %z16.s",
        "sub    %z22.s %z23.s -> %z21.s", "sub    %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sub, sub_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "sub    %z0.d %z0.d -> %z0.d",    "sub    %z6.d %z7.d -> %z5.d",
        "sub    %z11.d %z12.d -> %z10.d", "sub    %z17.d %z18.d -> %z16.d",
        "sub    %z22.d %z23.d -> %z21.d", "sub    %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sub, sub_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(subr_sve_pred)
{
    /* Testing SUBR    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "subr   %p0/m %z0.b %z0.b -> %z0.b",    "subr   %p2/m %z5.b %z7.b -> %z5.b",
        "subr   %p3/m %z10.b %z12.b -> %z10.b", "subr   %p5/m %z16.b %z18.b -> %z16.b",
        "subr   %p6/m %z21.b %z23.b -> %z21.b", "subr   %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(subr, subr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "subr   %p0/m %z0.h %z0.h -> %z0.h",    "subr   %p2/m %z5.h %z7.h -> %z5.h",
        "subr   %p3/m %z10.h %z12.h -> %z10.h", "subr   %p5/m %z16.h %z18.h -> %z16.h",
        "subr   %p6/m %z21.h %z23.h -> %z21.h", "subr   %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(subr, subr_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "subr   %p0/m %z0.s %z0.s -> %z0.s",    "subr   %p2/m %z5.s %z7.s -> %z5.s",
        "subr   %p3/m %z10.s %z12.s -> %z10.s", "subr   %p5/m %z16.s %z18.s -> %z16.s",
        "subr   %p6/m %z21.s %z23.s -> %z21.s", "subr   %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(subr, subr_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "subr   %p0/m %z0.d %z0.d -> %z0.d",    "subr   %p2/m %z5.d %z7.d -> %z5.d",
        "subr   %p3/m %z10.d %z12.d -> %z10.d", "subr   %p5/m %z16.d %z18.d -> %z16.d",
        "subr   %p6/m %z21.d %z23.d -> %z21.d", "subr   %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(subr, subr_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(subr_sve_shift)
{
    /* Testing SUBR    <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm>, <shift> */
    uint imm8_0_0[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_0[6] = { 0, 0, 0, 0, 0, 0 };
    const char *expected_0_0[6] = {
        "subr   %z0.b $0x00 lsl $0x00 -> %z0.b",
        "subr   %z5.b $0x2b lsl $0x00 -> %z5.b",
        "subr   %z10.b $0x56 lsl $0x00 -> %z10.b",
        "subr   %z16.b $0x81 lsl $0x00 -> %z16.b",
        "subr   %z21.b $0xab lsl $0x00 -> %z21.b",
        "subr   %z31.b $0xff lsl $0x00 -> %z31.b",
    };
    TEST_LOOP(subr, subr_sve_shift, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_immed_uint(imm8_0_0[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_0[i], OPSZ_1b));

    uint imm8_0_1[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_1[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_1[6] = {
        "subr   %z0.h $0x00 lsl $0x08 -> %z0.h",
        "subr   %z5.h $0x2b lsl $0x08 -> %z5.h",
        "subr   %z10.h $0x56 lsl $0x08 -> %z10.h",
        "subr   %z16.h $0x81 lsl $0x08 -> %z16.h",
        "subr   %z21.h $0xab lsl $0x00 -> %z21.h",
        "subr   %z31.h $0xff lsl $0x00 -> %z31.h",
    };
    TEST_LOOP(subr, subr_sve_shift, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_immed_uint(imm8_0_1[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_1[i], OPSZ_1b));

    uint imm8_0_2[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_2[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_2[6] = {
        "subr   %z0.s $0x00 lsl $0x08 -> %z0.s",
        "subr   %z5.s $0x2b lsl $0x08 -> %z5.s",
        "subr   %z10.s $0x56 lsl $0x08 -> %z10.s",
        "subr   %z16.s $0x81 lsl $0x08 -> %z16.s",
        "subr   %z21.s $0xab lsl $0x00 -> %z21.s",
        "subr   %z31.s $0xff lsl $0x00 -> %z31.s",
    };
    TEST_LOOP(subr, subr_sve_shift, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_immed_uint(imm8_0_2[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_2[i], OPSZ_1b));

    uint imm8_0_3[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_3[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_3[6] = {
        "subr   %z0.d $0x00 lsl $0x08 -> %z0.d",
        "subr   %z5.d $0x2b lsl $0x08 -> %z5.d",
        "subr   %z10.d $0x56 lsl $0x08 -> %z10.d",
        "subr   %z16.d $0x81 lsl $0x08 -> %z16.d",
        "subr   %z21.d $0xab lsl $0x00 -> %z21.d",
        "subr   %z31.d $0xff lsl $0x00 -> %z31.d",
    };
    TEST_LOOP(subr, subr_sve_shift, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_immed_uint(imm8_0_3[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_3[i], OPSZ_1b));
}

TEST_INSTR(uqadd_sve_shift)
{
    /* Testing UQADD   <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm>, <shift> */
    uint imm8_0_0[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_0[6] = { 0, 0, 0, 0, 0, 0 };
    const char *expected_0_0[6] = {
        "uqadd  %z0.b $0x00 lsl $0x00 -> %z0.b",
        "uqadd  %z5.b $0x2b lsl $0x00 -> %z5.b",
        "uqadd  %z10.b $0x56 lsl $0x00 -> %z10.b",
        "uqadd  %z16.b $0x81 lsl $0x00 -> %z16.b",
        "uqadd  %z21.b $0xab lsl $0x00 -> %z21.b",
        "uqadd  %z31.b $0xff lsl $0x00 -> %z31.b",
    };
    TEST_LOOP(uqadd, uqadd_sve_shift, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_immed_uint(imm8_0_0[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_0[i], OPSZ_1b));

    uint imm8_0_1[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_1[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_1[6] = {
        "uqadd  %z0.h $0x00 lsl $0x08 -> %z0.h",
        "uqadd  %z5.h $0x2b lsl $0x08 -> %z5.h",
        "uqadd  %z10.h $0x56 lsl $0x08 -> %z10.h",
        "uqadd  %z16.h $0x81 lsl $0x08 -> %z16.h",
        "uqadd  %z21.h $0xab lsl $0x00 -> %z21.h",
        "uqadd  %z31.h $0xff lsl $0x00 -> %z31.h",
    };
    TEST_LOOP(uqadd, uqadd_sve_shift, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_immed_uint(imm8_0_1[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_1[i], OPSZ_1b));

    uint imm8_0_2[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_2[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_2[6] = {
        "uqadd  %z0.s $0x00 lsl $0x08 -> %z0.s",
        "uqadd  %z5.s $0x2b lsl $0x08 -> %z5.s",
        "uqadd  %z10.s $0x56 lsl $0x08 -> %z10.s",
        "uqadd  %z16.s $0x81 lsl $0x08 -> %z16.s",
        "uqadd  %z21.s $0xab lsl $0x00 -> %z21.s",
        "uqadd  %z31.s $0xff lsl $0x00 -> %z31.s",
    };
    TEST_LOOP(uqadd, uqadd_sve_shift, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_immed_uint(imm8_0_2[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_2[i], OPSZ_1b));

    uint imm8_0_3[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_3[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_3[6] = {
        "uqadd  %z0.d $0x00 lsl $0x08 -> %z0.d",
        "uqadd  %z5.d $0x2b lsl $0x08 -> %z5.d",
        "uqadd  %z10.d $0x56 lsl $0x08 -> %z10.d",
        "uqadd  %z16.d $0x81 lsl $0x08 -> %z16.d",
        "uqadd  %z21.d $0xab lsl $0x00 -> %z21.d",
        "uqadd  %z31.d $0xff lsl $0x00 -> %z31.d",
    };
    TEST_LOOP(uqadd, uqadd_sve_shift, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_immed_uint(imm8_0_3[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_3[i], OPSZ_1b));
}

TEST_INSTR(uqadd_sve)
{
    /* Testing UQADD   <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "uqadd  %z0.b %z0.b -> %z0.b",    "uqadd  %z6.b %z7.b -> %z5.b",
        "uqadd  %z11.b %z12.b -> %z10.b", "uqadd  %z17.b %z18.b -> %z16.b",
        "uqadd  %z22.b %z23.b -> %z21.b", "uqadd  %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(uqadd, uqadd_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "uqadd  %z0.h %z0.h -> %z0.h",    "uqadd  %z6.h %z7.h -> %z5.h",
        "uqadd  %z11.h %z12.h -> %z10.h", "uqadd  %z17.h %z18.h -> %z16.h",
        "uqadd  %z22.h %z23.h -> %z21.h", "uqadd  %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(uqadd, uqadd_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "uqadd  %z0.s %z0.s -> %z0.s",    "uqadd  %z6.s %z7.s -> %z5.s",
        "uqadd  %z11.s %z12.s -> %z10.s", "uqadd  %z17.s %z18.s -> %z16.s",
        "uqadd  %z22.s %z23.s -> %z21.s", "uqadd  %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(uqadd, uqadd_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "uqadd  %z0.d %z0.d -> %z0.d",    "uqadd  %z6.d %z7.d -> %z5.d",
        "uqadd  %z11.d %z12.d -> %z10.d", "uqadd  %z17.d %z18.d -> %z16.d",
        "uqadd  %z22.d %z23.d -> %z21.d", "uqadd  %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(uqadd, uqadd_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(uqsub_sve_shift)
{
    /* Testing UQSUB   <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm>, <shift> */
    uint imm8_0_0[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_0[6] = { 0, 0, 0, 0, 0, 0 };
    const char *expected_0_0[6] = {
        "uqsub  %z0.b $0x00 lsl $0x00 -> %z0.b",
        "uqsub  %z5.b $0x2b lsl $0x00 -> %z5.b",
        "uqsub  %z10.b $0x56 lsl $0x00 -> %z10.b",
        "uqsub  %z16.b $0x81 lsl $0x00 -> %z16.b",
        "uqsub  %z21.b $0xab lsl $0x00 -> %z21.b",
        "uqsub  %z31.b $0xff lsl $0x00 -> %z31.b",
    };
    TEST_LOOP(uqsub, uqsub_sve_shift, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_immed_uint(imm8_0_0[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_0[i], OPSZ_1b));

    uint imm8_0_1[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_1[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_1[6] = {
        "uqsub  %z0.h $0x00 lsl $0x08 -> %z0.h",
        "uqsub  %z5.h $0x2b lsl $0x08 -> %z5.h",
        "uqsub  %z10.h $0x56 lsl $0x08 -> %z10.h",
        "uqsub  %z16.h $0x81 lsl $0x08 -> %z16.h",
        "uqsub  %z21.h $0xab lsl $0x00 -> %z21.h",
        "uqsub  %z31.h $0xff lsl $0x00 -> %z31.h",
    };
    TEST_LOOP(uqsub, uqsub_sve_shift, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_immed_uint(imm8_0_1[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_1[i], OPSZ_1b));

    uint imm8_0_2[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_2[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_2[6] = {
        "uqsub  %z0.s $0x00 lsl $0x08 -> %z0.s",
        "uqsub  %z5.s $0x2b lsl $0x08 -> %z5.s",
        "uqsub  %z10.s $0x56 lsl $0x08 -> %z10.s",
        "uqsub  %z16.s $0x81 lsl $0x08 -> %z16.s",
        "uqsub  %z21.s $0xab lsl $0x00 -> %z21.s",
        "uqsub  %z31.s $0xff lsl $0x00 -> %z31.s",
    };
    TEST_LOOP(uqsub, uqsub_sve_shift, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_immed_uint(imm8_0_2[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_2[i], OPSZ_1b));

    uint imm8_0_3[6] = { 0, 43, 86, 129, 171, 255 };
    uint shift_0_3[6] = { 8, 8, 8, 8, 0, 0 };
    const char *expected_0_3[6] = {
        "uqsub  %z0.d $0x00 lsl $0x08 -> %z0.d",
        "uqsub  %z5.d $0x2b lsl $0x08 -> %z5.d",
        "uqsub  %z10.d $0x56 lsl $0x08 -> %z10.d",
        "uqsub  %z16.d $0x81 lsl $0x08 -> %z16.d",
        "uqsub  %z21.d $0xab lsl $0x00 -> %z21.d",
        "uqsub  %z31.d $0xff lsl $0x00 -> %z31.d",
    };
    TEST_LOOP(uqsub, uqsub_sve_shift, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_immed_uint(imm8_0_3[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_3[i], OPSZ_1b));
}

TEST_INSTR(uqsub_sve)
{
    /* Testing UQSUB   <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "uqsub  %z0.b %z0.b -> %z0.b",    "uqsub  %z6.b %z7.b -> %z5.b",
        "uqsub  %z11.b %z12.b -> %z10.b", "uqsub  %z17.b %z18.b -> %z16.b",
        "uqsub  %z22.b %z23.b -> %z21.b", "uqsub  %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(uqsub, uqsub_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "uqsub  %z0.h %z0.h -> %z0.h",    "uqsub  %z6.h %z7.h -> %z5.h",
        "uqsub  %z11.h %z12.h -> %z10.h", "uqsub  %z17.h %z18.h -> %z16.h",
        "uqsub  %z22.h %z23.h -> %z21.h", "uqsub  %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(uqsub, uqsub_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "uqsub  %z0.s %z0.s -> %z0.s",    "uqsub  %z6.s %z7.s -> %z5.s",
        "uqsub  %z11.s %z12.s -> %z10.s", "uqsub  %z17.s %z18.s -> %z16.s",
        "uqsub  %z22.s %z23.s -> %z21.s", "uqsub  %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(uqsub, uqsub_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "uqsub  %z0.d %z0.d -> %z0.d",    "uqsub  %z6.d %z7.d -> %z5.d",
        "uqsub  %z11.d %z12.d -> %z10.d", "uqsub  %z17.d %z18.d -> %z16.d",
        "uqsub  %z22.d %z23.d -> %z21.d", "uqsub  %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(uqsub, uqsub_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(cpy_sve_shift_pred)
{
    /* Testing CPY     <Zd>.<Ts>, <Pg>/Z, #<imm>, <shift> */
    int imm8_0_0[6] = { -128, -84, -41, 2, 44, 127 };
    uint shift_0_0[6] = { 0, 0, 0, 0, 0, 0 };
    const char *expected_0_0[6] = {
        "cpy    %p0/z $0x80 lsl $0x00 -> %z0.b",
        "cpy    %p3/z $0xac lsl $0x00 -> %z5.b",
        "cpy    %p6/z $0xd7 lsl $0x00 -> %z10.b",
        "cpy    %p9/z $0x02 lsl $0x00 -> %z16.b",
        "cpy    %p11/z $0x2c lsl $0x00 -> %z21.b",
        "cpy    %p15/z $0x7f lsl $0x00 -> %z31.b",
    };
    TEST_LOOP(cpy, cpy_sve_shift_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false),
              opnd_create_immed_int(imm8_0_0[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_0[i], OPSZ_1b));

    int imm8_0_1[6] = { -128, -84, -41, 2, 44, 127 };
    uint shift_0_1[6] = { 8, 0, 0, 0, 8, 0 };
    const char *expected_0_1[6] = {
        "cpy    %p0/z $0x80 lsl $0x08 -> %z0.h",
        "cpy    %p3/z $0xac lsl $0x00 -> %z5.h",
        "cpy    %p6/z $0xd7 lsl $0x00 -> %z10.h",
        "cpy    %p9/z $0x02 lsl $0x00 -> %z16.h",
        "cpy    %p11/z $0x2c lsl $0x08 -> %z21.h",
        "cpy    %p15/z $0x7f lsl $0x00 -> %z31.h",
    };
    TEST_LOOP(cpy, cpy_sve_shift_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false),
              opnd_create_immed_int(imm8_0_1[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_1[i], OPSZ_1b));

    int imm8_0_2[6] = { -128, -84, -41, 2, 44, 127 };
    uint shift_0_2[6] = { 8, 0, 0, 0, 8, 0 };
    const char *expected_0_2[6] = {
        "cpy    %p0/z $0x80 lsl $0x08 -> %z0.s",
        "cpy    %p3/z $0xac lsl $0x00 -> %z5.s",
        "cpy    %p6/z $0xd7 lsl $0x00 -> %z10.s",
        "cpy    %p9/z $0x02 lsl $0x00 -> %z16.s",
        "cpy    %p11/z $0x2c lsl $0x08 -> %z21.s",
        "cpy    %p15/z $0x7f lsl $0x00 -> %z31.s",
    };
    TEST_LOOP(cpy, cpy_sve_shift_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false),
              opnd_create_immed_int(imm8_0_2[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_2[i], OPSZ_1b));

    int imm8_0_3[6] = { -128, -84, -41, 2, 44, 127 };
    uint shift_0_3[6] = { 8, 0, 0, 0, 8, 0 };
    const char *expected_0_3[6] = {
        "cpy    %p0/z $0x80 lsl $0x08 -> %z0.d",
        "cpy    %p3/z $0xac lsl $0x00 -> %z5.d",
        "cpy    %p6/z $0xd7 lsl $0x00 -> %z10.d",
        "cpy    %p9/z $0x02 lsl $0x00 -> %z16.d",
        "cpy    %p11/z $0x2c lsl $0x08 -> %z21.d",
        "cpy    %p15/z $0x7f lsl $0x00 -> %z31.d",
    };
    TEST_LOOP(cpy, cpy_sve_shift_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false),
              opnd_create_immed_int(imm8_0_3[i], OPSZ_1),
              opnd_create_immed_uint(shift_0_3[i], OPSZ_1b));

    /* Testing CPY     <Zd>.<Ts>, <Pg>/M, #<imm>, <shift> */
    int imm8_1_0[6] = { -128, -84, -41, 2, 44, 127 };
    uint shift_1_0[6] = { 0, 0, 0, 0, 0, 0 };
    const char *expected_1_0[6] = {
        "cpy    %p0/m $0x80 lsl $0x00 -> %z0.b",
        "cpy    %p3/m $0xac lsl $0x00 -> %z5.b",
        "cpy    %p6/m $0xd7 lsl $0x00 -> %z10.b",
        "cpy    %p9/m $0x02 lsl $0x00 -> %z16.b",
        "cpy    %p11/m $0x2c lsl $0x00 -> %z21.b",
        "cpy    %p15/m $0x7f lsl $0x00 -> %z31.b",
    };
    TEST_LOOP(cpy, cpy_sve_shift_pred, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_six_offset_1[i], true),
              opnd_create_immed_int(imm8_1_0[i], OPSZ_1),
              opnd_create_immed_uint(shift_1_0[i], OPSZ_1b));

    int imm8_1_1[6] = { -128, -84, -41, 2, 44, 127 };
    uint shift_1_1[6] = { 8, 0, 0, 0, 8, 0 };
    const char *expected_1_1[6] = {
        "cpy    %p0/m $0x80 lsl $0x08 -> %z0.h",
        "cpy    %p3/m $0xac lsl $0x00 -> %z5.h",
        "cpy    %p6/m $0xd7 lsl $0x00 -> %z10.h",
        "cpy    %p9/m $0x02 lsl $0x00 -> %z16.h",
        "cpy    %p11/m $0x2c lsl $0x08 -> %z21.h",
        "cpy    %p15/m $0x7f lsl $0x00 -> %z31.h",
    };
    TEST_LOOP(cpy, cpy_sve_shift_pred, 6, expected_1_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_six_offset_1[i], true),
              opnd_create_immed_int(imm8_1_1[i], OPSZ_1),
              opnd_create_immed_uint(shift_1_1[i], OPSZ_1b));

    int imm8_1_2[6] = { -128, -84, -41, 2, 44, 127 };
    uint shift_1_2[6] = { 8, 0, 0, 0, 8, 0 };
    const char *expected_1_2[6] = {
        "cpy    %p0/m $0x80 lsl $0x08 -> %z0.s",
        "cpy    %p3/m $0xac lsl $0x00 -> %z5.s",
        "cpy    %p6/m $0xd7 lsl $0x00 -> %z10.s",
        "cpy    %p9/m $0x02 lsl $0x00 -> %z16.s",
        "cpy    %p11/m $0x2c lsl $0x08 -> %z21.s",
        "cpy    %p15/m $0x7f lsl $0x00 -> %z31.s",
    };
    TEST_LOOP(cpy, cpy_sve_shift_pred, 6, expected_1_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_six_offset_1[i], true),
              opnd_create_immed_int(imm8_1_2[i], OPSZ_1),
              opnd_create_immed_uint(shift_1_2[i], OPSZ_1b));

    int imm8_1_3[6] = { -128, -84, -41, 2, 44, 127 };
    uint shift_1_3[6] = { 8, 0, 0, 0, 8, 0 };
    const char *expected_1_3[6] = {
        "cpy    %p0/m $0x80 lsl $0x08 -> %z0.d",
        "cpy    %p3/m $0xac lsl $0x00 -> %z5.d",
        "cpy    %p6/m $0xd7 lsl $0x00 -> %z10.d",
        "cpy    %p9/m $0x02 lsl $0x00 -> %z16.d",
        "cpy    %p11/m $0x2c lsl $0x08 -> %z21.d",
        "cpy    %p15/m $0x7f lsl $0x00 -> %z31.d",
    };
    TEST_LOOP(cpy, cpy_sve_shift_pred, 6, expected_1_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_six_offset_1[i], true),
              opnd_create_immed_int(imm8_1_3[i], OPSZ_1),
              opnd_create_immed_uint(shift_1_3[i], OPSZ_1b));
}

TEST_INSTR(ptest_sve_pred)
{
    /* Testing PTEST   <Pg>, <Pn>.B */
    const char *expected_0_0[6] = {
        "ptest  %p0 %p0.b", "ptest  %p2 %p3.b",   "ptest  %p5 %p6.b",
        "ptest  %p8 %p9.b", "ptest  %p10 %p11.b", "ptest  %p15 %p15.b",
    };
    TEST_LOOP(ptest, ptest_sve_pred, 6, expected_0_0[i],
              opnd_create_reg(Pn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_1));
}

TEST_INSTR(mad_sve_pred)
{
    /* Testing MAD     <Zdn>.<Ts>, <Pg>/M, <Zm>.<Ts>, <Za>.<Ts> */
    const char *expected_0_0[6] = {
        "mad    %p0/m %z0.b %z0.b %z0.b -> %z0.b",
        "mad    %p2/m %z5.b %z7.b %z8.b -> %z5.b",
        "mad    %p3/m %z10.b %z12.b %z13.b -> %z10.b",
        "mad    %p5/m %z16.b %z18.b %z19.b -> %z16.b",
        "mad    %p6/m %z21.b %z23.b %z24.b -> %z21.b",
        "mad    %p7/m %z31.b %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(mad, mad_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "mad    %p0/m %z0.h %z0.h %z0.h -> %z0.h",
        "mad    %p2/m %z5.h %z7.h %z8.h -> %z5.h",
        "mad    %p3/m %z10.h %z12.h %z13.h -> %z10.h",
        "mad    %p5/m %z16.h %z18.h %z19.h -> %z16.h",
        "mad    %p6/m %z21.h %z23.h %z24.h -> %z21.h",
        "mad    %p7/m %z31.h %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(mad, mad_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "mad    %p0/m %z0.s %z0.s %z0.s -> %z0.s",
        "mad    %p2/m %z5.s %z7.s %z8.s -> %z5.s",
        "mad    %p3/m %z10.s %z12.s %z13.s -> %z10.s",
        "mad    %p5/m %z16.s %z18.s %z19.s -> %z16.s",
        "mad    %p6/m %z21.s %z23.s %z24.s -> %z21.s",
        "mad    %p7/m %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(mad, mad_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "mad    %p0/m %z0.d %z0.d %z0.d -> %z0.d",
        "mad    %p2/m %z5.d %z7.d %z8.d -> %z5.d",
        "mad    %p3/m %z10.d %z12.d %z13.d -> %z10.d",
        "mad    %p5/m %z16.d %z18.d %z19.d -> %z16.d",
        "mad    %p6/m %z21.d %z23.d %z24.d -> %z21.d",
        "mad    %p7/m %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(mad, mad_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(mla_sve_pred)
{
    /* Testing MLA     <Zda>.<Ts>, <Pg>/M, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "mla    %p0/m %z0.b %z0.b %z0.b -> %z0.b",
        "mla    %p2/m %z7.b %z8.b %z5.b -> %z5.b",
        "mla    %p3/m %z12.b %z13.b %z10.b -> %z10.b",
        "mla    %p5/m %z18.b %z19.b %z16.b -> %z16.b",
        "mla    %p6/m %z23.b %z24.b %z21.b -> %z21.b",
        "mla    %p7/m %z31.b %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(mla, mla_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "mla    %p0/m %z0.h %z0.h %z0.h -> %z0.h",
        "mla    %p2/m %z7.h %z8.h %z5.h -> %z5.h",
        "mla    %p3/m %z12.h %z13.h %z10.h -> %z10.h",
        "mla    %p5/m %z18.h %z19.h %z16.h -> %z16.h",
        "mla    %p6/m %z23.h %z24.h %z21.h -> %z21.h",
        "mla    %p7/m %z31.h %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(mla, mla_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "mla    %p0/m %z0.s %z0.s %z0.s -> %z0.s",
        "mla    %p2/m %z7.s %z8.s %z5.s -> %z5.s",
        "mla    %p3/m %z12.s %z13.s %z10.s -> %z10.s",
        "mla    %p5/m %z18.s %z19.s %z16.s -> %z16.s",
        "mla    %p6/m %z23.s %z24.s %z21.s -> %z21.s",
        "mla    %p7/m %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(mla, mla_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "mla    %p0/m %z0.d %z0.d %z0.d -> %z0.d",
        "mla    %p2/m %z7.d %z8.d %z5.d -> %z5.d",
        "mla    %p3/m %z12.d %z13.d %z10.d -> %z10.d",
        "mla    %p5/m %z18.d %z19.d %z16.d -> %z16.d",
        "mla    %p6/m %z23.d %z24.d %z21.d -> %z21.d",
        "mla    %p7/m %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(mla, mla_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(mls_sve_pred)
{
    /* Testing MLS     <Zda>.<Ts>, <Pg>/M, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "mls    %p0/m %z0.b %z0.b %z0.b -> %z0.b",
        "mls    %p2/m %z7.b %z8.b %z5.b -> %z5.b",
        "mls    %p3/m %z12.b %z13.b %z10.b -> %z10.b",
        "mls    %p5/m %z18.b %z19.b %z16.b -> %z16.b",
        "mls    %p6/m %z23.b %z24.b %z21.b -> %z21.b",
        "mls    %p7/m %z31.b %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(mls, mls_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "mls    %p0/m %z0.h %z0.h %z0.h -> %z0.h",
        "mls    %p2/m %z7.h %z8.h %z5.h -> %z5.h",
        "mls    %p3/m %z12.h %z13.h %z10.h -> %z10.h",
        "mls    %p5/m %z18.h %z19.h %z16.h -> %z16.h",
        "mls    %p6/m %z23.h %z24.h %z21.h -> %z21.h",
        "mls    %p7/m %z31.h %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(mls, mls_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "mls    %p0/m %z0.s %z0.s %z0.s -> %z0.s",
        "mls    %p2/m %z7.s %z8.s %z5.s -> %z5.s",
        "mls    %p3/m %z12.s %z13.s %z10.s -> %z10.s",
        "mls    %p5/m %z18.s %z19.s %z16.s -> %z16.s",
        "mls    %p6/m %z23.s %z24.s %z21.s -> %z21.s",
        "mls    %p7/m %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(mls, mls_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "mls    %p0/m %z0.d %z0.d %z0.d -> %z0.d",
        "mls    %p2/m %z7.d %z8.d %z5.d -> %z5.d",
        "mls    %p3/m %z12.d %z13.d %z10.d -> %z10.d",
        "mls    %p5/m %z18.d %z19.d %z16.d -> %z16.d",
        "mls    %p6/m %z23.d %z24.d %z21.d -> %z21.d",
        "mls    %p7/m %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(mls, mls_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(msb_sve_pred)
{
    /* Testing MSB     <Zdn>.<Ts>, <Pg>/M, <Zm>.<Ts>, <Za>.<Ts> */
    const char *expected_0_0[6] = {
        "msb    %p0/m %z0.b %z0.b %z0.b -> %z0.b",
        "msb    %p2/m %z5.b %z7.b %z8.b -> %z5.b",
        "msb    %p3/m %z10.b %z12.b %z13.b -> %z10.b",
        "msb    %p5/m %z16.b %z18.b %z19.b -> %z16.b",
        "msb    %p6/m %z21.b %z23.b %z24.b -> %z21.b",
        "msb    %p7/m %z31.b %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(msb, msb_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "msb    %p0/m %z0.h %z0.h %z0.h -> %z0.h",
        "msb    %p2/m %z5.h %z7.h %z8.h -> %z5.h",
        "msb    %p3/m %z10.h %z12.h %z13.h -> %z10.h",
        "msb    %p5/m %z16.h %z18.h %z19.h -> %z16.h",
        "msb    %p6/m %z21.h %z23.h %z24.h -> %z21.h",
        "msb    %p7/m %z31.h %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(msb, msb_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "msb    %p0/m %z0.s %z0.s %z0.s -> %z0.s",
        "msb    %p2/m %z5.s %z7.s %z8.s -> %z5.s",
        "msb    %p3/m %z10.s %z12.s %z13.s -> %z10.s",
        "msb    %p5/m %z16.s %z18.s %z19.s -> %z16.s",
        "msb    %p6/m %z21.s %z23.s %z24.s -> %z21.s",
        "msb    %p7/m %z31.s %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(msb, msb_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "msb    %p0/m %z0.d %z0.d %z0.d -> %z0.d",
        "msb    %p2/m %z5.d %z7.d %z8.d -> %z5.d",
        "msb    %p3/m %z10.d %z12.d %z13.d -> %z10.d",
        "msb    %p5/m %z16.d %z18.d %z19.d -> %z16.d",
        "msb    %p6/m %z21.d %z23.d %z24.d -> %z21.d",
        "msb    %p7/m %z31.d %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(msb, msb_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(mul_sve_pred)
{
    /* Testing MUL     <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "mul    %p0/m %z0.b %z0.b -> %z0.b",    "mul    %p2/m %z5.b %z7.b -> %z5.b",
        "mul    %p3/m %z10.b %z12.b -> %z10.b", "mul    %p5/m %z16.b %z18.b -> %z16.b",
        "mul    %p6/m %z21.b %z23.b -> %z21.b", "mul    %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(mul, mul_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "mul    %p0/m %z0.h %z0.h -> %z0.h",    "mul    %p2/m %z5.h %z7.h -> %z5.h",
        "mul    %p3/m %z10.h %z12.h -> %z10.h", "mul    %p5/m %z16.h %z18.h -> %z16.h",
        "mul    %p6/m %z21.h %z23.h -> %z21.h", "mul    %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(mul, mul_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "mul    %p0/m %z0.s %z0.s -> %z0.s",    "mul    %p2/m %z5.s %z7.s -> %z5.s",
        "mul    %p3/m %z10.s %z12.s -> %z10.s", "mul    %p5/m %z16.s %z18.s -> %z16.s",
        "mul    %p6/m %z21.s %z23.s -> %z21.s", "mul    %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(mul, mul_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "mul    %p0/m %z0.d %z0.d -> %z0.d",    "mul    %p2/m %z5.d %z7.d -> %z5.d",
        "mul    %p3/m %z10.d %z12.d -> %z10.d", "mul    %p5/m %z16.d %z18.d -> %z16.d",
        "mul    %p6/m %z21.d %z23.d -> %z21.d", "mul    %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(mul, mul_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(mul_sve)
{
    /* Testing MUL     <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm> */
    int imm8_0_0[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_0[6] = {
        "mul    %z0.b $0x80 -> %z0.b",   "mul    %z5.b $0xab -> %z5.b",
        "mul    %z10.b $0xd6 -> %z10.b", "mul    %z16.b $0x01 -> %z16.b",
        "mul    %z21.b $0x2b -> %z21.b", "mul    %z31.b $0x7f -> %z31.b",
    };
    TEST_LOOP(mul, mul_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_immed_int(imm8_0_0[i], OPSZ_1));

    int imm8_0_1[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_1[6] = {
        "mul    %z0.h $0x80 -> %z0.h",   "mul    %z5.h $0xab -> %z5.h",
        "mul    %z10.h $0xd6 -> %z10.h", "mul    %z16.h $0x01 -> %z16.h",
        "mul    %z21.h $0x2b -> %z21.h", "mul    %z31.h $0x7f -> %z31.h",
    };
    TEST_LOOP(mul, mul_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_immed_int(imm8_0_1[i], OPSZ_1));

    int imm8_0_2[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_2[6] = {
        "mul    %z0.s $0x80 -> %z0.s",   "mul    %z5.s $0xab -> %z5.s",
        "mul    %z10.s $0xd6 -> %z10.s", "mul    %z16.s $0x01 -> %z16.s",
        "mul    %z21.s $0x2b -> %z21.s", "mul    %z31.s $0x7f -> %z31.s",
    };
    TEST_LOOP(mul, mul_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_immed_int(imm8_0_2[i], OPSZ_1));

    int imm8_0_3[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_3[6] = {
        "mul    %z0.d $0x80 -> %z0.d",   "mul    %z5.d $0xab -> %z5.d",
        "mul    %z10.d $0xd6 -> %z10.d", "mul    %z16.d $0x01 -> %z16.d",
        "mul    %z21.d $0x2b -> %z21.d", "mul    %z31.d $0x7f -> %z31.d",
    };
    TEST_LOOP(mul, mul_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_immed_int(imm8_0_3[i], OPSZ_1));
}

TEST_INSTR(smulh_sve_pred)
{
    /* Testing SMULH   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "smulh  %p0/m %z0.b %z0.b -> %z0.b",    "smulh  %p2/m %z5.b %z7.b -> %z5.b",
        "smulh  %p3/m %z10.b %z12.b -> %z10.b", "smulh  %p5/m %z16.b %z18.b -> %z16.b",
        "smulh  %p6/m %z21.b %z23.b -> %z21.b", "smulh  %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(smulh, smulh_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "smulh  %p0/m %z0.h %z0.h -> %z0.h",    "smulh  %p2/m %z5.h %z7.h -> %z5.h",
        "smulh  %p3/m %z10.h %z12.h -> %z10.h", "smulh  %p5/m %z16.h %z18.h -> %z16.h",
        "smulh  %p6/m %z21.h %z23.h -> %z21.h", "smulh  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(smulh, smulh_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "smulh  %p0/m %z0.s %z0.s -> %z0.s",    "smulh  %p2/m %z5.s %z7.s -> %z5.s",
        "smulh  %p3/m %z10.s %z12.s -> %z10.s", "smulh  %p5/m %z16.s %z18.s -> %z16.s",
        "smulh  %p6/m %z21.s %z23.s -> %z21.s", "smulh  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(smulh, smulh_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "smulh  %p0/m %z0.d %z0.d -> %z0.d",    "smulh  %p2/m %z5.d %z7.d -> %z5.d",
        "smulh  %p3/m %z10.d %z12.d -> %z10.d", "smulh  %p5/m %z16.d %z18.d -> %z16.d",
        "smulh  %p6/m %z21.d %z23.d -> %z21.d", "smulh  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(smulh, smulh_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(umulh_sve_pred)
{
    /* Testing UMULH   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "umulh  %p0/m %z0.b %z0.b -> %z0.b",    "umulh  %p2/m %z5.b %z7.b -> %z5.b",
        "umulh  %p3/m %z10.b %z12.b -> %z10.b", "umulh  %p5/m %z16.b %z18.b -> %z16.b",
        "umulh  %p6/m %z21.b %z23.b -> %z21.b", "umulh  %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(umulh, umulh_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "umulh  %p0/m %z0.h %z0.h -> %z0.h",    "umulh  %p2/m %z5.h %z7.h -> %z5.h",
        "umulh  %p3/m %z10.h %z12.h -> %z10.h", "umulh  %p5/m %z16.h %z18.h -> %z16.h",
        "umulh  %p6/m %z21.h %z23.h -> %z21.h", "umulh  %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(umulh, umulh_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "umulh  %p0/m %z0.s %z0.s -> %z0.s",    "umulh  %p2/m %z5.s %z7.s -> %z5.s",
        "umulh  %p3/m %z10.s %z12.s -> %z10.s", "umulh  %p5/m %z16.s %z18.s -> %z16.s",
        "umulh  %p6/m %z21.s %z23.s -> %z21.s", "umulh  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(umulh, umulh_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "umulh  %p0/m %z0.d %z0.d -> %z0.d",    "umulh  %p2/m %z5.d %z7.d -> %z5.d",
        "umulh  %p3/m %z10.d %z12.d -> %z10.d", "umulh  %p5/m %z16.d %z18.d -> %z16.d",
        "umulh  %p6/m %z21.d %z23.d -> %z21.d", "umulh  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(umulh, umulh_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(fexpa_sve)
{
    /* Testing FEXPA   <Zd>.<Ts>, <Zn>.<Ts> */
    const char *expected_0_0[6] = {
        "fexpa  %z0.h -> %z0.h",   "fexpa  %z6.h -> %z5.h",   "fexpa  %z11.h -> %z10.h",
        "fexpa  %z17.h -> %z16.h", "fexpa  %z22.h -> %z21.h", "fexpa  %z31.h -> %z31.h",
    };
    TEST_LOOP(fexpa, fexpa_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "fexpa  %z0.s -> %z0.s",   "fexpa  %z6.s -> %z5.s",   "fexpa  %z11.s -> %z10.s",
        "fexpa  %z17.s -> %z16.s", "fexpa  %z22.s -> %z21.s", "fexpa  %z31.s -> %z31.s",
    };
    TEST_LOOP(fexpa, fexpa_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "fexpa  %z0.d -> %z0.d",   "fexpa  %z6.d -> %z5.d",   "fexpa  %z11.d -> %z10.d",
        "fexpa  %z17.d -> %z16.d", "fexpa  %z22.d -> %z21.d", "fexpa  %z31.d -> %z31.d",
    };
    TEST_LOOP(fexpa, fexpa_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(ftmad_sve)
{
    /* Testing FTMAD   <Zdn>.<Ts>, <Zdn>.<Ts>, <Zm>.<Ts>, #<imm> */
    uint imm3_0_0[6] = { 0, 3, 4, 6, 7, 7 };
    const char *expected_0_0[6] = {
        "ftmad  %z0.h %z0.h $0x00 -> %z0.h",    "ftmad  %z5.h %z6.h $0x03 -> %z5.h",
        "ftmad  %z10.h %z11.h $0x04 -> %z10.h", "ftmad  %z16.h %z17.h $0x06 -> %z16.h",
        "ftmad  %z21.h %z22.h $0x07 -> %z21.h", "ftmad  %z31.h %z31.h $0x07 -> %z31.h",
    };
    TEST_LOOP(ftmad, ftmad_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_immed_uint(imm3_0_0[i], OPSZ_3b));

    uint imm3_0_1[6] = { 0, 3, 4, 6, 7, 7 };
    const char *expected_0_1[6] = {
        "ftmad  %z0.s %z0.s $0x00 -> %z0.s",    "ftmad  %z5.s %z6.s $0x03 -> %z5.s",
        "ftmad  %z10.s %z11.s $0x04 -> %z10.s", "ftmad  %z16.s %z17.s $0x06 -> %z16.s",
        "ftmad  %z21.s %z22.s $0x07 -> %z21.s", "ftmad  %z31.s %z31.s $0x07 -> %z31.s",
    };
    TEST_LOOP(ftmad, ftmad_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_immed_uint(imm3_0_1[i], OPSZ_3b));

    uint imm3_0_2[6] = { 0, 3, 4, 6, 7, 7 };
    const char *expected_0_2[6] = {
        "ftmad  %z0.d %z0.d $0x00 -> %z0.d",    "ftmad  %z5.d %z6.d $0x03 -> %z5.d",
        "ftmad  %z10.d %z11.d $0x04 -> %z10.d", "ftmad  %z16.d %z17.d $0x06 -> %z16.d",
        "ftmad  %z21.d %z22.d $0x07 -> %z21.d", "ftmad  %z31.d %z31.d $0x07 -> %z31.d",
    };
    TEST_LOOP(ftmad, ftmad_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_immed_uint(imm3_0_2[i], OPSZ_3b));
}

TEST_INSTR(ftsmul_sve)
{
    /* Testing FTSMUL  <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "ftsmul %z0.h %z0.h -> %z0.h",    "ftsmul %z6.h %z7.h -> %z5.h",
        "ftsmul %z11.h %z12.h -> %z10.h", "ftsmul %z17.h %z18.h -> %z16.h",
        "ftsmul %z22.h %z23.h -> %z21.h", "ftsmul %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(ftsmul, ftsmul_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "ftsmul %z0.s %z0.s -> %z0.s",    "ftsmul %z6.s %z7.s -> %z5.s",
        "ftsmul %z11.s %z12.s -> %z10.s", "ftsmul %z17.s %z18.s -> %z16.s",
        "ftsmul %z22.s %z23.s -> %z21.s", "ftsmul %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(ftsmul, ftsmul_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "ftsmul %z0.d %z0.d -> %z0.d",    "ftsmul %z6.d %z7.d -> %z5.d",
        "ftsmul %z11.d %z12.d -> %z10.d", "ftsmul %z17.d %z18.d -> %z16.d",
        "ftsmul %z22.d %z23.d -> %z21.d", "ftsmul %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(ftsmul, ftsmul_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(ftssel_sve)
{
    /* Testing FTSSEL  <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "ftssel %z0.h %z0.h -> %z0.h",    "ftssel %z6.h %z7.h -> %z5.h",
        "ftssel %z11.h %z12.h -> %z10.h", "ftssel %z17.h %z18.h -> %z16.h",
        "ftssel %z22.h %z23.h -> %z21.h", "ftssel %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(ftssel, ftssel_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "ftssel %z0.s %z0.s -> %z0.s",    "ftssel %z6.s %z7.s -> %z5.s",
        "ftssel %z11.s %z12.s -> %z10.s", "ftssel %z17.s %z18.s -> %z16.s",
        "ftssel %z22.s %z23.s -> %z21.s", "ftssel %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(ftssel, ftssel_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "ftssel %z0.d %z0.d -> %z0.d",    "ftssel %z6.d %z7.d -> %z5.d",
        "ftssel %z11.d %z12.d -> %z10.d", "ftssel %z17.d %z18.d -> %z16.d",
        "ftssel %z22.d %z23.d -> %z21.d", "ftssel %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(ftssel, ftssel_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(abs_sve_pred)
{
    /* Testing ABS     <Zd>.<Ts>, <Pg>/M, <Zn>.<Ts> */
    const char *expected_0_0[6] = {
        "abs    %p0/m %z0.b -> %z0.b",   "abs    %p2/m %z7.b -> %z5.b",
        "abs    %p3/m %z12.b -> %z10.b", "abs    %p5/m %z18.b -> %z16.b",
        "abs    %p6/m %z23.b -> %z21.b", "abs    %p7/m %z31.b -> %z31.b",
    };
    TEST_LOOP(abs, abs_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "abs    %p0/m %z0.h -> %z0.h",   "abs    %p2/m %z7.h -> %z5.h",
        "abs    %p3/m %z12.h -> %z10.h", "abs    %p5/m %z18.h -> %z16.h",
        "abs    %p6/m %z23.h -> %z21.h", "abs    %p7/m %z31.h -> %z31.h",
    };
    TEST_LOOP(abs, abs_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "abs    %p0/m %z0.s -> %z0.s",   "abs    %p2/m %z7.s -> %z5.s",
        "abs    %p3/m %z12.s -> %z10.s", "abs    %p5/m %z18.s -> %z16.s",
        "abs    %p6/m %z23.s -> %z21.s", "abs    %p7/m %z31.s -> %z31.s",
    };
    TEST_LOOP(abs, abs_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "abs    %p0/m %z0.d -> %z0.d",   "abs    %p2/m %z7.d -> %z5.d",
        "abs    %p3/m %z12.d -> %z10.d", "abs    %p5/m %z18.d -> %z16.d",
        "abs    %p6/m %z23.d -> %z21.d", "abs    %p7/m %z31.d -> %z31.d",
    };
    TEST_LOOP(abs, abs_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(cnot_sve_pred)
{
    /* Testing CNOT    <Zd>.<Ts>, <Pg>/M, <Zn>.<Ts> */
    const char *expected_0_0[6] = {
        "cnot   %p0/m %z0.b -> %z0.b",   "cnot   %p2/m %z7.b -> %z5.b",
        "cnot   %p3/m %z12.b -> %z10.b", "cnot   %p5/m %z18.b -> %z16.b",
        "cnot   %p6/m %z23.b -> %z21.b", "cnot   %p7/m %z31.b -> %z31.b",
    };
    TEST_LOOP(cnot, cnot_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "cnot   %p0/m %z0.h -> %z0.h",   "cnot   %p2/m %z7.h -> %z5.h",
        "cnot   %p3/m %z12.h -> %z10.h", "cnot   %p5/m %z18.h -> %z16.h",
        "cnot   %p6/m %z23.h -> %z21.h", "cnot   %p7/m %z31.h -> %z31.h",
    };
    TEST_LOOP(cnot, cnot_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "cnot   %p0/m %z0.s -> %z0.s",   "cnot   %p2/m %z7.s -> %z5.s",
        "cnot   %p3/m %z12.s -> %z10.s", "cnot   %p5/m %z18.s -> %z16.s",
        "cnot   %p6/m %z23.s -> %z21.s", "cnot   %p7/m %z31.s -> %z31.s",
    };
    TEST_LOOP(cnot, cnot_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "cnot   %p0/m %z0.d -> %z0.d",   "cnot   %p2/m %z7.d -> %z5.d",
        "cnot   %p3/m %z12.d -> %z10.d", "cnot   %p5/m %z18.d -> %z16.d",
        "cnot   %p6/m %z23.d -> %z21.d", "cnot   %p7/m %z31.d -> %z31.d",
    };
    TEST_LOOP(cnot, cnot_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(neg_sve_pred)
{
    /* Testing NEG     <Zd>.<Ts>, <Pg>/M, <Zn>.<Ts> */
    const char *expected_0_0[6] = {
        "neg    %p0/m %z0.b -> %z0.b",   "neg    %p2/m %z7.b -> %z5.b",
        "neg    %p3/m %z12.b -> %z10.b", "neg    %p5/m %z18.b -> %z16.b",
        "neg    %p6/m %z23.b -> %z21.b", "neg    %p7/m %z31.b -> %z31.b",
    };
    TEST_LOOP(neg, neg_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "neg    %p0/m %z0.h -> %z0.h",   "neg    %p2/m %z7.h -> %z5.h",
        "neg    %p3/m %z12.h -> %z10.h", "neg    %p5/m %z18.h -> %z16.h",
        "neg    %p6/m %z23.h -> %z21.h", "neg    %p7/m %z31.h -> %z31.h",
    };
    TEST_LOOP(neg, neg_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "neg    %p0/m %z0.s -> %z0.s",   "neg    %p2/m %z7.s -> %z5.s",
        "neg    %p3/m %z12.s -> %z10.s", "neg    %p5/m %z18.s -> %z16.s",
        "neg    %p6/m %z23.s -> %z21.s", "neg    %p7/m %z31.s -> %z31.s",
    };
    TEST_LOOP(neg, neg_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "neg    %p0/m %z0.d -> %z0.d",   "neg    %p2/m %z7.d -> %z5.d",
        "neg    %p3/m %z12.d -> %z10.d", "neg    %p5/m %z18.d -> %z16.d",
        "neg    %p6/m %z23.d -> %z21.d", "neg    %p7/m %z31.d -> %z31.d",
    };
    TEST_LOOP(neg, neg_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sabd_sve_pred)
{
    /* Testing SABD    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "sabd   %p0/m %z0.b %z0.b -> %z0.b",    "sabd   %p2/m %z5.b %z7.b -> %z5.b",
        "sabd   %p3/m %z10.b %z12.b -> %z10.b", "sabd   %p5/m %z16.b %z18.b -> %z16.b",
        "sabd   %p6/m %z21.b %z23.b -> %z21.b", "sabd   %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(sabd, sabd_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "sabd   %p0/m %z0.h %z0.h -> %z0.h",    "sabd   %p2/m %z5.h %z7.h -> %z5.h",
        "sabd   %p3/m %z10.h %z12.h -> %z10.h", "sabd   %p5/m %z16.h %z18.h -> %z16.h",
        "sabd   %p6/m %z21.h %z23.h -> %z21.h", "sabd   %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(sabd, sabd_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "sabd   %p0/m %z0.s %z0.s -> %z0.s",    "sabd   %p2/m %z5.s %z7.s -> %z5.s",
        "sabd   %p3/m %z10.s %z12.s -> %z10.s", "sabd   %p5/m %z16.s %z18.s -> %z16.s",
        "sabd   %p6/m %z21.s %z23.s -> %z21.s", "sabd   %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sabd, sabd_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "sabd   %p0/m %z0.d %z0.d -> %z0.d",    "sabd   %p2/m %z5.d %z7.d -> %z5.d",
        "sabd   %p3/m %z10.d %z12.d -> %z10.d", "sabd   %p5/m %z16.d %z18.d -> %z16.d",
        "sabd   %p6/m %z21.d %z23.d -> %z21.d", "sabd   %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sabd, sabd_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(smax_sve_pred)
{
    /* Testing SMAX    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "smax   %p0/m %z0.b %z0.b -> %z0.b",    "smax   %p2/m %z5.b %z7.b -> %z5.b",
        "smax   %p3/m %z10.b %z12.b -> %z10.b", "smax   %p5/m %z16.b %z18.b -> %z16.b",
        "smax   %p6/m %z21.b %z23.b -> %z21.b", "smax   %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(smax, smax_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "smax   %p0/m %z0.h %z0.h -> %z0.h",    "smax   %p2/m %z5.h %z7.h -> %z5.h",
        "smax   %p3/m %z10.h %z12.h -> %z10.h", "smax   %p5/m %z16.h %z18.h -> %z16.h",
        "smax   %p6/m %z21.h %z23.h -> %z21.h", "smax   %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(smax, smax_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "smax   %p0/m %z0.s %z0.s -> %z0.s",    "smax   %p2/m %z5.s %z7.s -> %z5.s",
        "smax   %p3/m %z10.s %z12.s -> %z10.s", "smax   %p5/m %z16.s %z18.s -> %z16.s",
        "smax   %p6/m %z21.s %z23.s -> %z21.s", "smax   %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(smax, smax_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "smax   %p0/m %z0.d %z0.d -> %z0.d",    "smax   %p2/m %z5.d %z7.d -> %z5.d",
        "smax   %p3/m %z10.d %z12.d -> %z10.d", "smax   %p5/m %z16.d %z18.d -> %z16.d",
        "smax   %p6/m %z21.d %z23.d -> %z21.d", "smax   %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(smax, smax_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(smax_sve)
{
    /* Testing SMAX    <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm> */
    int imm8_0_0[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_0[6] = {
        "smax   %z0.b $0x80 -> %z0.b",   "smax   %z5.b $0xab -> %z5.b",
        "smax   %z10.b $0xd6 -> %z10.b", "smax   %z16.b $0x01 -> %z16.b",
        "smax   %z21.b $0x2b -> %z21.b", "smax   %z31.b $0x7f -> %z31.b",
    };
    TEST_LOOP(smax, smax_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_immed_int(imm8_0_0[i], OPSZ_1));

    int imm8_0_1[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_1[6] = {
        "smax   %z0.h $0x80 -> %z0.h",   "smax   %z5.h $0xab -> %z5.h",
        "smax   %z10.h $0xd6 -> %z10.h", "smax   %z16.h $0x01 -> %z16.h",
        "smax   %z21.h $0x2b -> %z21.h", "smax   %z31.h $0x7f -> %z31.h",
    };
    TEST_LOOP(smax, smax_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_immed_int(imm8_0_1[i], OPSZ_1));

    int imm8_0_2[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_2[6] = {
        "smax   %z0.s $0x80 -> %z0.s",   "smax   %z5.s $0xab -> %z5.s",
        "smax   %z10.s $0xd6 -> %z10.s", "smax   %z16.s $0x01 -> %z16.s",
        "smax   %z21.s $0x2b -> %z21.s", "smax   %z31.s $0x7f -> %z31.s",
    };
    TEST_LOOP(smax, smax_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_immed_int(imm8_0_2[i], OPSZ_1));

    int imm8_0_3[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_3[6] = {
        "smax   %z0.d $0x80 -> %z0.d",   "smax   %z5.d $0xab -> %z5.d",
        "smax   %z10.d $0xd6 -> %z10.d", "smax   %z16.d $0x01 -> %z16.d",
        "smax   %z21.d $0x2b -> %z21.d", "smax   %z31.d $0x7f -> %z31.d",
    };
    TEST_LOOP(smax, smax_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_immed_int(imm8_0_3[i], OPSZ_1));
}

TEST_INSTR(smin_sve_pred)
{
    /* Testing SMIN    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "smin   %p0/m %z0.b %z0.b -> %z0.b",    "smin   %p2/m %z5.b %z7.b -> %z5.b",
        "smin   %p3/m %z10.b %z12.b -> %z10.b", "smin   %p5/m %z16.b %z18.b -> %z16.b",
        "smin   %p6/m %z21.b %z23.b -> %z21.b", "smin   %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(smin, smin_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "smin   %p0/m %z0.h %z0.h -> %z0.h",    "smin   %p2/m %z5.h %z7.h -> %z5.h",
        "smin   %p3/m %z10.h %z12.h -> %z10.h", "smin   %p5/m %z16.h %z18.h -> %z16.h",
        "smin   %p6/m %z21.h %z23.h -> %z21.h", "smin   %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(smin, smin_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "smin   %p0/m %z0.s %z0.s -> %z0.s",    "smin   %p2/m %z5.s %z7.s -> %z5.s",
        "smin   %p3/m %z10.s %z12.s -> %z10.s", "smin   %p5/m %z16.s %z18.s -> %z16.s",
        "smin   %p6/m %z21.s %z23.s -> %z21.s", "smin   %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(smin, smin_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "smin   %p0/m %z0.d %z0.d -> %z0.d",    "smin   %p2/m %z5.d %z7.d -> %z5.d",
        "smin   %p3/m %z10.d %z12.d -> %z10.d", "smin   %p5/m %z16.d %z18.d -> %z16.d",
        "smin   %p6/m %z21.d %z23.d -> %z21.d", "smin   %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(smin, smin_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(smin_sve)
{
    /* Testing SMIN    <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm> */
    int imm8_0_0[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_0[6] = {
        "smin   %z0.b $0x80 -> %z0.b",   "smin   %z5.b $0xab -> %z5.b",
        "smin   %z10.b $0xd6 -> %z10.b", "smin   %z16.b $0x01 -> %z16.b",
        "smin   %z21.b $0x2b -> %z21.b", "smin   %z31.b $0x7f -> %z31.b",
    };
    TEST_LOOP(smin, smin_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_immed_int(imm8_0_0[i], OPSZ_1));

    int imm8_0_1[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_1[6] = {
        "smin   %z0.h $0x80 -> %z0.h",   "smin   %z5.h $0xab -> %z5.h",
        "smin   %z10.h $0xd6 -> %z10.h", "smin   %z16.h $0x01 -> %z16.h",
        "smin   %z21.h $0x2b -> %z21.h", "smin   %z31.h $0x7f -> %z31.h",
    };
    TEST_LOOP(smin, smin_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_immed_int(imm8_0_1[i], OPSZ_1));

    int imm8_0_2[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_2[6] = {
        "smin   %z0.s $0x80 -> %z0.s",   "smin   %z5.s $0xab -> %z5.s",
        "smin   %z10.s $0xd6 -> %z10.s", "smin   %z16.s $0x01 -> %z16.s",
        "smin   %z21.s $0x2b -> %z21.s", "smin   %z31.s $0x7f -> %z31.s",
    };
    TEST_LOOP(smin, smin_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_immed_int(imm8_0_2[i], OPSZ_1));

    int imm8_0_3[6] = { -128, -85, -42, 1, 43, 127 };
    const char *expected_0_3[6] = {
        "smin   %z0.d $0x80 -> %z0.d",   "smin   %z5.d $0xab -> %z5.d",
        "smin   %z10.d $0xd6 -> %z10.d", "smin   %z16.d $0x01 -> %z16.d",
        "smin   %z21.d $0x2b -> %z21.d", "smin   %z31.d $0x7f -> %z31.d",
    };
    TEST_LOOP(smin, smin_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_immed_int(imm8_0_3[i], OPSZ_1));
}

TEST_INSTR(uabd_sve_pred)
{
    /* Testing UABD    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "uabd   %p0/m %z0.b %z0.b -> %z0.b",    "uabd   %p2/m %z5.b %z7.b -> %z5.b",
        "uabd   %p3/m %z10.b %z12.b -> %z10.b", "uabd   %p5/m %z16.b %z18.b -> %z16.b",
        "uabd   %p6/m %z21.b %z23.b -> %z21.b", "uabd   %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(uabd, uabd_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "uabd   %p0/m %z0.h %z0.h -> %z0.h",    "uabd   %p2/m %z5.h %z7.h -> %z5.h",
        "uabd   %p3/m %z10.h %z12.h -> %z10.h", "uabd   %p5/m %z16.h %z18.h -> %z16.h",
        "uabd   %p6/m %z21.h %z23.h -> %z21.h", "uabd   %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(uabd, uabd_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "uabd   %p0/m %z0.s %z0.s -> %z0.s",    "uabd   %p2/m %z5.s %z7.s -> %z5.s",
        "uabd   %p3/m %z10.s %z12.s -> %z10.s", "uabd   %p5/m %z16.s %z18.s -> %z16.s",
        "uabd   %p6/m %z21.s %z23.s -> %z21.s", "uabd   %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(uabd, uabd_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "uabd   %p0/m %z0.d %z0.d -> %z0.d",    "uabd   %p2/m %z5.d %z7.d -> %z5.d",
        "uabd   %p3/m %z10.d %z12.d -> %z10.d", "uabd   %p5/m %z16.d %z18.d -> %z16.d",
        "uabd   %p6/m %z21.d %z23.d -> %z21.d", "uabd   %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(uabd, uabd_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(facge_sve_pred)
{
    /* Testing FACGE   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "facge  %p0/z %z0.h %z0.h -> %p0.h",    "facge  %p2/z %z7.h %z8.h -> %p2.h",
        "facge  %p3/z %z12.h %z13.h -> %p5.h",  "facge  %p5/z %z18.h %z19.h -> %p8.h",
        "facge  %p6/z %z23.h %z24.h -> %p10.h", "facge  %p7/z %z31.h %z31.h -> %p15.h",
    };
    TEST_LOOP(facge, facge_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "facge  %p0/z %z0.s %z0.s -> %p0.s",    "facge  %p2/z %z7.s %z8.s -> %p2.s",
        "facge  %p3/z %z12.s %z13.s -> %p5.s",  "facge  %p5/z %z18.s %z19.s -> %p8.s",
        "facge  %p6/z %z23.s %z24.s -> %p10.s", "facge  %p7/z %z31.s %z31.s -> %p15.s",
    };
    TEST_LOOP(facge, facge_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "facge  %p0/z %z0.d %z0.d -> %p0.d",    "facge  %p2/z %z7.d %z8.d -> %p2.d",
        "facge  %p3/z %z12.d %z13.d -> %p5.d",  "facge  %p5/z %z18.d %z19.d -> %p8.d",
        "facge  %p6/z %z23.d %z24.d -> %p10.d", "facge  %p7/z %z31.d %z31.d -> %p15.d",
    };
    TEST_LOOP(facge, facge_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(facgt_sve_pred)
{
    /* Testing FACGT   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "facgt  %p0/z %z0.h %z0.h -> %p0.h",    "facgt  %p2/z %z7.h %z8.h -> %p2.h",
        "facgt  %p3/z %z12.h %z13.h -> %p5.h",  "facgt  %p5/z %z18.h %z19.h -> %p8.h",
        "facgt  %p6/z %z23.h %z24.h -> %p10.h", "facgt  %p7/z %z31.h %z31.h -> %p15.h",
    };
    TEST_LOOP(facgt, facgt_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "facgt  %p0/z %z0.s %z0.s -> %p0.s",    "facgt  %p2/z %z7.s %z8.s -> %p2.s",
        "facgt  %p3/z %z12.s %z13.s -> %p5.s",  "facgt  %p5/z %z18.s %z19.s -> %p8.s",
        "facgt  %p6/z %z23.s %z24.s -> %p10.s", "facgt  %p7/z %z31.s %z31.s -> %p15.s",
    };
    TEST_LOOP(facgt, facgt_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "facgt  %p0/z %z0.d %z0.d -> %p0.d",    "facgt  %p2/z %z7.d %z8.d -> %p2.d",
        "facgt  %p3/z %z12.d %z13.d -> %p5.d",  "facgt  %p5/z %z18.d %z19.d -> %p8.d",
        "facgt  %p6/z %z23.d %z24.d -> %p10.d", "facgt  %p7/z %z31.d %z31.d -> %p15.d",
    };
    TEST_LOOP(facgt, facgt_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(sdiv_sve_pred)
{
    /* Testing SDIV    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "sdiv   %p0/m %z0.s %z0.s -> %z0.s",    "sdiv   %p2/m %z5.s %z7.s -> %z5.s",
        "sdiv   %p3/m %z10.s %z12.s -> %z10.s", "sdiv   %p5/m %z16.s %z18.s -> %z16.s",
        "sdiv   %p6/m %z21.s %z23.s -> %z21.s", "sdiv   %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sdiv, sdiv_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_1[6] = {
        "sdiv   %p0/m %z0.d %z0.d -> %z0.d",    "sdiv   %p2/m %z5.d %z7.d -> %z5.d",
        "sdiv   %p3/m %z10.d %z12.d -> %z10.d", "sdiv   %p5/m %z16.d %z18.d -> %z16.d",
        "sdiv   %p6/m %z21.d %z23.d -> %z21.d", "sdiv   %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sdiv, sdiv_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sdivr_sve_pred)
{
    /* Testing SDIVR   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "sdivr  %p0/m %z0.s %z0.s -> %z0.s",    "sdivr  %p2/m %z5.s %z7.s -> %z5.s",
        "sdivr  %p3/m %z10.s %z12.s -> %z10.s", "sdivr  %p5/m %z16.s %z18.s -> %z16.s",
        "sdivr  %p6/m %z21.s %z23.s -> %z21.s", "sdivr  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sdivr, sdivr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_1[6] = {
        "sdivr  %p0/m %z0.d %z0.d -> %z0.d",    "sdivr  %p2/m %z5.d %z7.d -> %z5.d",
        "sdivr  %p3/m %z10.d %z12.d -> %z10.d", "sdivr  %p5/m %z16.d %z18.d -> %z16.d",
        "sdivr  %p6/m %z21.d %z23.d -> %z21.d", "sdivr  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sdivr, sdivr_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(udiv_sve_pred)
{
    /* Testing UDIV    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "udiv   %p0/m %z0.s %z0.s -> %z0.s",    "udiv   %p2/m %z5.s %z7.s -> %z5.s",
        "udiv   %p3/m %z10.s %z12.s -> %z10.s", "udiv   %p5/m %z16.s %z18.s -> %z16.s",
        "udiv   %p6/m %z21.s %z23.s -> %z21.s", "udiv   %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(udiv, udiv_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_1[6] = {
        "udiv   %p0/m %z0.d %z0.d -> %z0.d",    "udiv   %p2/m %z5.d %z7.d -> %z5.d",
        "udiv   %p3/m %z10.d %z12.d -> %z10.d", "udiv   %p5/m %z16.d %z18.d -> %z16.d",
        "udiv   %p6/m %z21.d %z23.d -> %z21.d", "udiv   %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(udiv, udiv_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(udivr_sve_pred)
{
    /* Testing UDIVR   <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "udivr  %p0/m %z0.s %z0.s -> %z0.s",    "udivr  %p2/m %z5.s %z7.s -> %z5.s",
        "udivr  %p3/m %z10.s %z12.s -> %z10.s", "udivr  %p5/m %z16.s %z18.s -> %z16.s",
        "udivr  %p6/m %z21.s %z23.s -> %z21.s", "udivr  %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(udivr, udivr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_1[6] = {
        "udivr  %p0/m %z0.d %z0.d -> %z0.d",    "udivr  %p2/m %z5.d %z7.d -> %z5.d",
        "udivr  %p3/m %z10.d %z12.d -> %z10.d", "udivr  %p5/m %z16.d %z18.d -> %z16.d",
        "udivr  %p6/m %z21.d %z23.d -> %z21.d", "udivr  %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(udivr, udivr_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(umax_sve_pred)
{
    /* Testing UMAX    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "umax   %p0/m %z0.b %z0.b -> %z0.b",    "umax   %p2/m %z5.b %z7.b -> %z5.b",
        "umax   %p3/m %z10.b %z12.b -> %z10.b", "umax   %p5/m %z16.b %z18.b -> %z16.b",
        "umax   %p6/m %z21.b %z23.b -> %z21.b", "umax   %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(umax, umax_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "umax   %p0/m %z0.h %z0.h -> %z0.h",    "umax   %p2/m %z5.h %z7.h -> %z5.h",
        "umax   %p3/m %z10.h %z12.h -> %z10.h", "umax   %p5/m %z16.h %z18.h -> %z16.h",
        "umax   %p6/m %z21.h %z23.h -> %z21.h", "umax   %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(umax, umax_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "umax   %p0/m %z0.s %z0.s -> %z0.s",    "umax   %p2/m %z5.s %z7.s -> %z5.s",
        "umax   %p3/m %z10.s %z12.s -> %z10.s", "umax   %p5/m %z16.s %z18.s -> %z16.s",
        "umax   %p6/m %z21.s %z23.s -> %z21.s", "umax   %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(umax, umax_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "umax   %p0/m %z0.d %z0.d -> %z0.d",    "umax   %p2/m %z5.d %z7.d -> %z5.d",
        "umax   %p3/m %z10.d %z12.d -> %z10.d", "umax   %p5/m %z16.d %z18.d -> %z16.d",
        "umax   %p6/m %z21.d %z23.d -> %z21.d", "umax   %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(umax, umax_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(umax_sve)
{
    /* Testing UMAX    <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm> */
    uint imm8_0_0[6] = { 0, 43, 86, 129, 171, 255 };
    const char *expected_0_0[6] = {
        "umax   %z0.b $0x00 -> %z0.b",   "umax   %z5.b $0x2b -> %z5.b",
        "umax   %z10.b $0x56 -> %z10.b", "umax   %z16.b $0x81 -> %z16.b",
        "umax   %z21.b $0xab -> %z21.b", "umax   %z31.b $0xff -> %z31.b",
    };
    TEST_LOOP(umax, umax_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_immed_uint(imm8_0_0[i], OPSZ_1));

    uint imm8_0_1[6] = { 0, 43, 86, 129, 171, 255 };
    const char *expected_0_1[6] = {
        "umax   %z0.h $0x00 -> %z0.h",   "umax   %z5.h $0x2b -> %z5.h",
        "umax   %z10.h $0x56 -> %z10.h", "umax   %z16.h $0x81 -> %z16.h",
        "umax   %z21.h $0xab -> %z21.h", "umax   %z31.h $0xff -> %z31.h",
    };
    TEST_LOOP(umax, umax_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_immed_uint(imm8_0_1[i], OPSZ_1));

    uint imm8_0_2[6] = { 0, 43, 86, 129, 171, 255 };
    const char *expected_0_2[6] = {
        "umax   %z0.s $0x00 -> %z0.s",   "umax   %z5.s $0x2b -> %z5.s",
        "umax   %z10.s $0x56 -> %z10.s", "umax   %z16.s $0x81 -> %z16.s",
        "umax   %z21.s $0xab -> %z21.s", "umax   %z31.s $0xff -> %z31.s",
    };
    TEST_LOOP(umax, umax_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_immed_uint(imm8_0_2[i], OPSZ_1));

    uint imm8_0_3[6] = { 0, 43, 86, 129, 171, 255 };
    const char *expected_0_3[6] = {
        "umax   %z0.d $0x00 -> %z0.d",   "umax   %z5.d $0x2b -> %z5.d",
        "umax   %z10.d $0x56 -> %z10.d", "umax   %z16.d $0x81 -> %z16.d",
        "umax   %z21.d $0xab -> %z21.d", "umax   %z31.d $0xff -> %z31.d",
    };
    TEST_LOOP(umax, umax_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_immed_uint(imm8_0_3[i], OPSZ_1));
}

TEST_INSTR(umin_sve_pred)
{
    /* Testing UMIN    <Zdn>.<Ts>, <Pg>/M, <Zdn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "umin   %p0/m %z0.b %z0.b -> %z0.b",    "umin   %p2/m %z5.b %z7.b -> %z5.b",
        "umin   %p3/m %z10.b %z12.b -> %z10.b", "umin   %p5/m %z16.b %z18.b -> %z16.b",
        "umin   %p6/m %z21.b %z23.b -> %z21.b", "umin   %p7/m %z31.b %z31.b -> %z31.b",
    };
    TEST_LOOP(umin, umin_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "umin   %p0/m %z0.h %z0.h -> %z0.h",    "umin   %p2/m %z5.h %z7.h -> %z5.h",
        "umin   %p3/m %z10.h %z12.h -> %z10.h", "umin   %p5/m %z16.h %z18.h -> %z16.h",
        "umin   %p6/m %z21.h %z23.h -> %z21.h", "umin   %p7/m %z31.h %z31.h -> %z31.h",
    };
    TEST_LOOP(umin, umin_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "umin   %p0/m %z0.s %z0.s -> %z0.s",    "umin   %p2/m %z5.s %z7.s -> %z5.s",
        "umin   %p3/m %z10.s %z12.s -> %z10.s", "umin   %p5/m %z16.s %z18.s -> %z16.s",
        "umin   %p6/m %z21.s %z23.s -> %z21.s", "umin   %p7/m %z31.s %z31.s -> %z31.s",
    };
    TEST_LOOP(umin, umin_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "umin   %p0/m %z0.d %z0.d -> %z0.d",    "umin   %p2/m %z5.d %z7.d -> %z5.d",
        "umin   %p3/m %z10.d %z12.d -> %z10.d", "umin   %p5/m %z16.d %z18.d -> %z16.d",
        "umin   %p6/m %z21.d %z23.d -> %z21.d", "umin   %p7/m %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(umin, umin_sve_pred, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(umin_sve)
{
    /* Testing UMIN    <Zdn>.<Ts>, <Zdn>.<Ts>, #<imm> */
    uint imm8_0_0[6] = { 0, 43, 86, 129, 171, 255 };
    const char *expected_0_0[6] = {
        "umin   %z0.b $0x00 -> %z0.b",   "umin   %z5.b $0x2b -> %z5.b",
        "umin   %z10.b $0x56 -> %z10.b", "umin   %z16.b $0x81 -> %z16.b",
        "umin   %z21.b $0xab -> %z21.b", "umin   %z31.b $0xff -> %z31.b",
    };
    TEST_LOOP(umin, umin_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_immed_uint(imm8_0_0[i], OPSZ_1));

    uint imm8_0_1[6] = { 0, 43, 86, 129, 171, 255 };
    const char *expected_0_1[6] = {
        "umin   %z0.h $0x00 -> %z0.h",   "umin   %z5.h $0x2b -> %z5.h",
        "umin   %z10.h $0x56 -> %z10.h", "umin   %z16.h $0x81 -> %z16.h",
        "umin   %z21.h $0xab -> %z21.h", "umin   %z31.h $0xff -> %z31.h",
    };
    TEST_LOOP(umin, umin_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_immed_uint(imm8_0_1[i], OPSZ_1));

    uint imm8_0_2[6] = { 0, 43, 86, 129, 171, 255 };
    const char *expected_0_2[6] = {
        "umin   %z0.s $0x00 -> %z0.s",   "umin   %z5.s $0x2b -> %z5.s",
        "umin   %z10.s $0x56 -> %z10.s", "umin   %z16.s $0x81 -> %z16.s",
        "umin   %z21.s $0xab -> %z21.s", "umin   %z31.s $0xff -> %z31.s",
    };
    TEST_LOOP(umin, umin_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_immed_uint(imm8_0_2[i], OPSZ_1));

    uint imm8_0_3[6] = { 0, 43, 86, 129, 171, 255 };
    const char *expected_0_3[6] = {
        "umin   %z0.d $0x00 -> %z0.d",   "umin   %z5.d $0x2b -> %z5.d",
        "umin   %z10.d $0x56 -> %z10.d", "umin   %z16.d $0x81 -> %z16.d",
        "umin   %z21.d $0xab -> %z21.d", "umin   %z31.d $0xff -> %z31.d",
    };
    TEST_LOOP(umin, umin_sve, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_immed_uint(imm8_0_3[i], OPSZ_1));
}

TEST_INSTR(sxtb_sve_pred)
{
    /* Testing SXTB    <Zd>.<Ts>, <Pg>/M, <Zn>.<Ts> */
    const char *expected_0_0[6] = {
        "sxtb   %p0/m %z0.h -> %z0.h",   "sxtb   %p2/m %z7.h -> %z5.h",
        "sxtb   %p3/m %z12.h -> %z10.h", "sxtb   %p5/m %z18.h -> %z16.h",
        "sxtb   %p6/m %z23.h -> %z21.h", "sxtb   %p7/m %z31.h -> %z31.h",
    };
    TEST_LOOP(sxtb, sxtb_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "sxtb   %p0/m %z0.s -> %z0.s",   "sxtb   %p2/m %z7.s -> %z5.s",
        "sxtb   %p3/m %z12.s -> %z10.s", "sxtb   %p5/m %z18.s -> %z16.s",
        "sxtb   %p6/m %z23.s -> %z21.s", "sxtb   %p7/m %z31.s -> %z31.s",
    };
    TEST_LOOP(sxtb, sxtb_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "sxtb   %p0/m %z0.d -> %z0.d",   "sxtb   %p2/m %z7.d -> %z5.d",
        "sxtb   %p3/m %z12.d -> %z10.d", "sxtb   %p5/m %z18.d -> %z16.d",
        "sxtb   %p6/m %z23.d -> %z21.d", "sxtb   %p7/m %z31.d -> %z31.d",
    };
    TEST_LOOP(sxtb, sxtb_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sxth_sve_pred)
{
    /* Testing SXTH    <Zd>.<Ts>, <Pg>/M, <Zn>.<Ts> */
    const char *expected_0_0[6] = {
        "sxth   %p0/m %z0.s -> %z0.s",   "sxth   %p2/m %z7.s -> %z5.s",
        "sxth   %p3/m %z12.s -> %z10.s", "sxth   %p5/m %z18.s -> %z16.s",
        "sxth   %p6/m %z23.s -> %z21.s", "sxth   %p7/m %z31.s -> %z31.s",
    };
    TEST_LOOP(sxth, sxth_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_1[6] = {
        "sxth   %p0/m %z0.d -> %z0.d",   "sxth   %p2/m %z7.d -> %z5.d",
        "sxth   %p3/m %z12.d -> %z10.d", "sxth   %p5/m %z18.d -> %z16.d",
        "sxth   %p6/m %z23.d -> %z21.d", "sxth   %p7/m %z31.d -> %z31.d",
    };
    TEST_LOOP(sxth, sxth_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(sxtw_sve_pred)
{
    /* Testing SXTW    <Zd>.D, <Pg>/M, <Zn>.D */
    const char *expected_0_0[6] = {
        "sxtw   %p0/m %z0.d -> %z0.d",   "sxtw   %p2/m %z7.d -> %z5.d",
        "sxtw   %p3/m %z12.d -> %z10.d", "sxtw   %p5/m %z18.d -> %z16.d",
        "sxtw   %p6/m %z23.d -> %z21.d", "sxtw   %p7/m %z31.d -> %z31.d",
    };
    TEST_LOOP(sxtw, sxtw_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(uxtb_sve_pred)
{
    /* Testing UXTB    <Zd>.<Ts>, <Pg>/M, <Zn>.<Ts> */
    const char *expected_0_0[6] = {
        "uxtb   %p0/m %z0.h -> %z0.h",   "uxtb   %p2/m %z7.h -> %z5.h",
        "uxtb   %p3/m %z12.h -> %z10.h", "uxtb   %p5/m %z18.h -> %z16.h",
        "uxtb   %p6/m %z23.h -> %z21.h", "uxtb   %p7/m %z31.h -> %z31.h",
    };
    TEST_LOOP(uxtb, uxtb_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "uxtb   %p0/m %z0.s -> %z0.s",   "uxtb   %p2/m %z7.s -> %z5.s",
        "uxtb   %p3/m %z12.s -> %z10.s", "uxtb   %p5/m %z18.s -> %z16.s",
        "uxtb   %p6/m %z23.s -> %z21.s", "uxtb   %p7/m %z31.s -> %z31.s",
    };
    TEST_LOOP(uxtb, uxtb_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "uxtb   %p0/m %z0.d -> %z0.d",   "uxtb   %p2/m %z7.d -> %z5.d",
        "uxtb   %p3/m %z12.d -> %z10.d", "uxtb   %p5/m %z18.d -> %z16.d",
        "uxtb   %p6/m %z23.d -> %z21.d", "uxtb   %p7/m %z31.d -> %z31.d",
    };
    TEST_LOOP(uxtb, uxtb_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(uxth_sve_pred)
{
    /* Testing UXTH    <Zd>.<Ts>, <Pg>/M, <Zn>.<Ts> */
    const char *expected_0_0[6] = {
        "uxth   %p0/m %z0.s -> %z0.s",   "uxth   %p2/m %z7.s -> %z5.s",
        "uxth   %p3/m %z12.s -> %z10.s", "uxth   %p5/m %z18.s -> %z16.s",
        "uxth   %p6/m %z23.s -> %z21.s", "uxth   %p7/m %z31.s -> %z31.s",
    };
    TEST_LOOP(uxth, uxth_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_1[6] = {
        "uxth   %p0/m %z0.d -> %z0.d",   "uxth   %p2/m %z7.d -> %z5.d",
        "uxth   %p3/m %z12.d -> %z10.d", "uxth   %p5/m %z18.d -> %z16.d",
        "uxth   %p6/m %z23.d -> %z21.d", "uxth   %p7/m %z31.d -> %z31.d",
    };
    TEST_LOOP(uxth, uxth_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(uxtw_sve_pred)
{
    /* Testing UXTW    <Zd>.D, <Pg>/M, <Zn>.D */
    const char *expected_0_0[6] = {
        "uxtw   %p0/m %z0.d -> %z0.d",   "uxtw   %p2/m %z7.d -> %z5.d",
        "uxtw   %p3/m %z12.d -> %z10.d", "uxtw   %p5/m %z18.d -> %z16.d",
        "uxtw   %p6/m %z23.d -> %z21.d", "uxtw   %p7/m %z31.d -> %z31.d",
    };
    TEST_LOOP(uxtw, uxtw_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(fcmeq_sve_zero_pred)
{
    /* Testing FCMEQ   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, #0.0 */
    const char *expected_0_0[6] = {
        "fcmeq  %p0/z %z0.h $0.000000 -> %p0.h",
        "fcmeq  %p2/z %z7.h $0.000000 -> %p2.h",
        "fcmeq  %p3/z %z12.h $0.000000 -> %p5.h",
        "fcmeq  %p5/z %z18.h $0.000000 -> %p8.h",
        "fcmeq  %p6/z %z23.h $0.000000 -> %p10.h",
        "fcmeq  %p7/z %z31.h $0.000000 -> %p15.h",
    };
    TEST_LOOP(fcmeq, fcmeq_sve_zero_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "fcmeq  %p0/z %z0.s $0.000000 -> %p0.s",
        "fcmeq  %p2/z %z7.s $0.000000 -> %p2.s",
        "fcmeq  %p3/z %z12.s $0.000000 -> %p5.s",
        "fcmeq  %p5/z %z18.s $0.000000 -> %p8.s",
        "fcmeq  %p6/z %z23.s $0.000000 -> %p10.s",
        "fcmeq  %p7/z %z31.s $0.000000 -> %p15.s",
    };
    TEST_LOOP(fcmeq, fcmeq_sve_zero_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "fcmeq  %p0/z %z0.d $0.000000 -> %p0.d",
        "fcmeq  %p2/z %z7.d $0.000000 -> %p2.d",
        "fcmeq  %p3/z %z12.d $0.000000 -> %p5.d",
        "fcmeq  %p5/z %z18.d $0.000000 -> %p8.d",
        "fcmeq  %p6/z %z23.d $0.000000 -> %p10.d",
        "fcmeq  %p7/z %z31.d $0.000000 -> %p15.d",
    };
    TEST_LOOP(fcmeq, fcmeq_sve_zero_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(fcmeq_sve_pred)
{
    /* Testing FCMEQ   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "fcmeq  %p0/z %z0.h %z0.h -> %p0.h",    "fcmeq  %p2/z %z7.h %z8.h -> %p2.h",
        "fcmeq  %p3/z %z12.h %z13.h -> %p5.h",  "fcmeq  %p5/z %z18.h %z19.h -> %p8.h",
        "fcmeq  %p6/z %z23.h %z24.h -> %p10.h", "fcmeq  %p7/z %z31.h %z31.h -> %p15.h",
    };
    TEST_LOOP(fcmeq, fcmeq_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "fcmeq  %p0/z %z0.s %z0.s -> %p0.s",    "fcmeq  %p2/z %z7.s %z8.s -> %p2.s",
        "fcmeq  %p3/z %z12.s %z13.s -> %p5.s",  "fcmeq  %p5/z %z18.s %z19.s -> %p8.s",
        "fcmeq  %p6/z %z23.s %z24.s -> %p10.s", "fcmeq  %p7/z %z31.s %z31.s -> %p15.s",
    };
    TEST_LOOP(fcmeq, fcmeq_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "fcmeq  %p0/z %z0.d %z0.d -> %p0.d",    "fcmeq  %p2/z %z7.d %z8.d -> %p2.d",
        "fcmeq  %p3/z %z12.d %z13.d -> %p5.d",  "fcmeq  %p5/z %z18.d %z19.d -> %p8.d",
        "fcmeq  %p6/z %z23.d %z24.d -> %p10.d", "fcmeq  %p7/z %z31.d %z31.d -> %p15.d",
    };
    TEST_LOOP(fcmeq, fcmeq_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(fcmge_sve_zero_pred)
{
    /* Testing FCMGE   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, #0.0 */
    const char *expected_0_0[6] = {
        "fcmge  %p0/z %z0.h $0.000000 -> %p0.h",
        "fcmge  %p2/z %z7.h $0.000000 -> %p2.h",
        "fcmge  %p3/z %z12.h $0.000000 -> %p5.h",
        "fcmge  %p5/z %z18.h $0.000000 -> %p8.h",
        "fcmge  %p6/z %z23.h $0.000000 -> %p10.h",
        "fcmge  %p7/z %z31.h $0.000000 -> %p15.h",
    };
    TEST_LOOP(fcmge, fcmge_sve_zero_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "fcmge  %p0/z %z0.s $0.000000 -> %p0.s",
        "fcmge  %p2/z %z7.s $0.000000 -> %p2.s",
        "fcmge  %p3/z %z12.s $0.000000 -> %p5.s",
        "fcmge  %p5/z %z18.s $0.000000 -> %p8.s",
        "fcmge  %p6/z %z23.s $0.000000 -> %p10.s",
        "fcmge  %p7/z %z31.s $0.000000 -> %p15.s",
    };
    TEST_LOOP(fcmge, fcmge_sve_zero_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "fcmge  %p0/z %z0.d $0.000000 -> %p0.d",
        "fcmge  %p2/z %z7.d $0.000000 -> %p2.d",
        "fcmge  %p3/z %z12.d $0.000000 -> %p5.d",
        "fcmge  %p5/z %z18.d $0.000000 -> %p8.d",
        "fcmge  %p6/z %z23.d $0.000000 -> %p10.d",
        "fcmge  %p7/z %z31.d $0.000000 -> %p15.d",
    };
    TEST_LOOP(fcmge, fcmge_sve_zero_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(fcmge_sve_pred)
{
    /* Testing FCMGE   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "fcmge  %p0/z %z0.h %z0.h -> %p0.h",    "fcmge  %p2/z %z7.h %z8.h -> %p2.h",
        "fcmge  %p3/z %z12.h %z13.h -> %p5.h",  "fcmge  %p5/z %z18.h %z19.h -> %p8.h",
        "fcmge  %p6/z %z23.h %z24.h -> %p10.h", "fcmge  %p7/z %z31.h %z31.h -> %p15.h",
    };
    TEST_LOOP(fcmge, fcmge_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "fcmge  %p0/z %z0.s %z0.s -> %p0.s",    "fcmge  %p2/z %z7.s %z8.s -> %p2.s",
        "fcmge  %p3/z %z12.s %z13.s -> %p5.s",  "fcmge  %p5/z %z18.s %z19.s -> %p8.s",
        "fcmge  %p6/z %z23.s %z24.s -> %p10.s", "fcmge  %p7/z %z31.s %z31.s -> %p15.s",
    };
    TEST_LOOP(fcmge, fcmge_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "fcmge  %p0/z %z0.d %z0.d -> %p0.d",    "fcmge  %p2/z %z7.d %z8.d -> %p2.d",
        "fcmge  %p3/z %z12.d %z13.d -> %p5.d",  "fcmge  %p5/z %z18.d %z19.d -> %p8.d",
        "fcmge  %p6/z %z23.d %z24.d -> %p10.d", "fcmge  %p7/z %z31.d %z31.d -> %p15.d",
    };
    TEST_LOOP(fcmge, fcmge_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(fcmgt_sve_zero_pred)
{
    /* Testing FCMGT   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, #0.0 */
    const char *expected_0_0[6] = {
        "fcmgt  %p0/z %z0.h $0.000000 -> %p0.h",
        "fcmgt  %p2/z %z7.h $0.000000 -> %p2.h",
        "fcmgt  %p3/z %z12.h $0.000000 -> %p5.h",
        "fcmgt  %p5/z %z18.h $0.000000 -> %p8.h",
        "fcmgt  %p6/z %z23.h $0.000000 -> %p10.h",
        "fcmgt  %p7/z %z31.h $0.000000 -> %p15.h",
    };
    TEST_LOOP(fcmgt, fcmgt_sve_zero_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "fcmgt  %p0/z %z0.s $0.000000 -> %p0.s",
        "fcmgt  %p2/z %z7.s $0.000000 -> %p2.s",
        "fcmgt  %p3/z %z12.s $0.000000 -> %p5.s",
        "fcmgt  %p5/z %z18.s $0.000000 -> %p8.s",
        "fcmgt  %p6/z %z23.s $0.000000 -> %p10.s",
        "fcmgt  %p7/z %z31.s $0.000000 -> %p15.s",
    };
    TEST_LOOP(fcmgt, fcmgt_sve_zero_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "fcmgt  %p0/z %z0.d $0.000000 -> %p0.d",
        "fcmgt  %p2/z %z7.d $0.000000 -> %p2.d",
        "fcmgt  %p3/z %z12.d $0.000000 -> %p5.d",
        "fcmgt  %p5/z %z18.d $0.000000 -> %p8.d",
        "fcmgt  %p6/z %z23.d $0.000000 -> %p10.d",
        "fcmgt  %p7/z %z31.d $0.000000 -> %p15.d",
    };
    TEST_LOOP(fcmgt, fcmgt_sve_zero_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(fcmgt_sve_pred)
{
    /* Testing FCMGT   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "fcmgt  %p0/z %z0.h %z0.h -> %p0.h",    "fcmgt  %p2/z %z7.h %z8.h -> %p2.h",
        "fcmgt  %p3/z %z12.h %z13.h -> %p5.h",  "fcmgt  %p5/z %z18.h %z19.h -> %p8.h",
        "fcmgt  %p6/z %z23.h %z24.h -> %p10.h", "fcmgt  %p7/z %z31.h %z31.h -> %p15.h",
    };
    TEST_LOOP(fcmgt, fcmgt_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "fcmgt  %p0/z %z0.s %z0.s -> %p0.s",    "fcmgt  %p2/z %z7.s %z8.s -> %p2.s",
        "fcmgt  %p3/z %z12.s %z13.s -> %p5.s",  "fcmgt  %p5/z %z18.s %z19.s -> %p8.s",
        "fcmgt  %p6/z %z23.s %z24.s -> %p10.s", "fcmgt  %p7/z %z31.s %z31.s -> %p15.s",
    };
    TEST_LOOP(fcmgt, fcmgt_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "fcmgt  %p0/z %z0.d %z0.d -> %p0.d",    "fcmgt  %p2/z %z7.d %z8.d -> %p2.d",
        "fcmgt  %p3/z %z12.d %z13.d -> %p5.d",  "fcmgt  %p5/z %z18.d %z19.d -> %p8.d",
        "fcmgt  %p6/z %z23.d %z24.d -> %p10.d", "fcmgt  %p7/z %z31.d %z31.d -> %p15.d",
    };
    TEST_LOOP(fcmgt, fcmgt_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(fcmle_sve_zero_pred)
{
    /* Testing FCMLE   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, #0.0 */
    const char *expected_0_0[6] = {
        "fcmle  %p0/z %z0.h $0.000000 -> %p0.h",
        "fcmle  %p2/z %z7.h $0.000000 -> %p2.h",
        "fcmle  %p3/z %z12.h $0.000000 -> %p5.h",
        "fcmle  %p5/z %z18.h $0.000000 -> %p8.h",
        "fcmle  %p6/z %z23.h $0.000000 -> %p10.h",
        "fcmle  %p7/z %z31.h $0.000000 -> %p15.h",
    };
    TEST_LOOP(fcmle, fcmle_sve_zero_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "fcmle  %p0/z %z0.s $0.000000 -> %p0.s",
        "fcmle  %p2/z %z7.s $0.000000 -> %p2.s",
        "fcmle  %p3/z %z12.s $0.000000 -> %p5.s",
        "fcmle  %p5/z %z18.s $0.000000 -> %p8.s",
        "fcmle  %p6/z %z23.s $0.000000 -> %p10.s",
        "fcmle  %p7/z %z31.s $0.000000 -> %p15.s",
    };
    TEST_LOOP(fcmle, fcmle_sve_zero_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "fcmle  %p0/z %z0.d $0.000000 -> %p0.d",
        "fcmle  %p2/z %z7.d $0.000000 -> %p2.d",
        "fcmle  %p3/z %z12.d $0.000000 -> %p5.d",
        "fcmle  %p5/z %z18.d $0.000000 -> %p8.d",
        "fcmle  %p6/z %z23.d $0.000000 -> %p10.d",
        "fcmle  %p7/z %z31.d $0.000000 -> %p15.d",
    };
    TEST_LOOP(fcmle, fcmle_sve_zero_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(fcmlt_sve_zero_pred)
{
    /* Testing FCMLT   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, #0.0 */
    const char *expected_0_0[6] = {
        "fcmlt  %p0/z %z0.h $0.000000 -> %p0.h",
        "fcmlt  %p2/z %z7.h $0.000000 -> %p2.h",
        "fcmlt  %p3/z %z12.h $0.000000 -> %p5.h",
        "fcmlt  %p5/z %z18.h $0.000000 -> %p8.h",
        "fcmlt  %p6/z %z23.h $0.000000 -> %p10.h",
        "fcmlt  %p7/z %z31.h $0.000000 -> %p15.h",
    };
    TEST_LOOP(fcmlt, fcmlt_sve_zero_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "fcmlt  %p0/z %z0.s $0.000000 -> %p0.s",
        "fcmlt  %p2/z %z7.s $0.000000 -> %p2.s",
        "fcmlt  %p3/z %z12.s $0.000000 -> %p5.s",
        "fcmlt  %p5/z %z18.s $0.000000 -> %p8.s",
        "fcmlt  %p6/z %z23.s $0.000000 -> %p10.s",
        "fcmlt  %p7/z %z31.s $0.000000 -> %p15.s",
    };
    TEST_LOOP(fcmlt, fcmlt_sve_zero_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "fcmlt  %p0/z %z0.d $0.000000 -> %p0.d",
        "fcmlt  %p2/z %z7.d $0.000000 -> %p2.d",
        "fcmlt  %p3/z %z12.d $0.000000 -> %p5.d",
        "fcmlt  %p5/z %z18.d $0.000000 -> %p8.d",
        "fcmlt  %p6/z %z23.d $0.000000 -> %p10.d",
        "fcmlt  %p7/z %z31.d $0.000000 -> %p15.d",
    };
    TEST_LOOP(fcmlt, fcmlt_sve_zero_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(fcmne_sve_zero_pred)
{
    /* Testing FCMNE   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, #0.0 */
    const char *expected_0_0[6] = {
        "fcmne  %p0/z %z0.h $0.000000 -> %p0.h",
        "fcmne  %p2/z %z7.h $0.000000 -> %p2.h",
        "fcmne  %p3/z %z12.h $0.000000 -> %p5.h",
        "fcmne  %p5/z %z18.h $0.000000 -> %p8.h",
        "fcmne  %p6/z %z23.h $0.000000 -> %p10.h",
        "fcmne  %p7/z %z31.h $0.000000 -> %p15.h",
    };
    TEST_LOOP(fcmne, fcmne_sve_zero_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "fcmne  %p0/z %z0.s $0.000000 -> %p0.s",
        "fcmne  %p2/z %z7.s $0.000000 -> %p2.s",
        "fcmne  %p3/z %z12.s $0.000000 -> %p5.s",
        "fcmne  %p5/z %z18.s $0.000000 -> %p8.s",
        "fcmne  %p6/z %z23.s $0.000000 -> %p10.s",
        "fcmne  %p7/z %z31.s $0.000000 -> %p15.s",
    };
    TEST_LOOP(fcmne, fcmne_sve_zero_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "fcmne  %p0/z %z0.d $0.000000 -> %p0.d",
        "fcmne  %p2/z %z7.d $0.000000 -> %p2.d",
        "fcmne  %p3/z %z12.d $0.000000 -> %p5.d",
        "fcmne  %p5/z %z18.d $0.000000 -> %p8.d",
        "fcmne  %p6/z %z23.d $0.000000 -> %p10.d",
        "fcmne  %p7/z %z31.d $0.000000 -> %p15.d",
    };
    TEST_LOOP(fcmne, fcmne_sve_zero_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(fcmne_sve_pred)
{
    /* Testing FCMNE   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "fcmne  %p0/z %z0.h %z0.h -> %p0.h",    "fcmne  %p2/z %z7.h %z8.h -> %p2.h",
        "fcmne  %p3/z %z12.h %z13.h -> %p5.h",  "fcmne  %p5/z %z18.h %z19.h -> %p8.h",
        "fcmne  %p6/z %z23.h %z24.h -> %p10.h", "fcmne  %p7/z %z31.h %z31.h -> %p15.h",
    };
    TEST_LOOP(fcmne, fcmne_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "fcmne  %p0/z %z0.s %z0.s -> %p0.s",    "fcmne  %p2/z %z7.s %z8.s -> %p2.s",
        "fcmne  %p3/z %z12.s %z13.s -> %p5.s",  "fcmne  %p5/z %z18.s %z19.s -> %p8.s",
        "fcmne  %p6/z %z23.s %z24.s -> %p10.s", "fcmne  %p7/z %z31.s %z31.s -> %p15.s",
    };
    TEST_LOOP(fcmne, fcmne_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "fcmne  %p0/z %z0.d %z0.d -> %p0.d",    "fcmne  %p2/z %z7.d %z8.d -> %p2.d",
        "fcmne  %p3/z %z12.d %z13.d -> %p5.d",  "fcmne  %p5/z %z18.d %z19.d -> %p8.d",
        "fcmne  %p6/z %z23.d %z24.d -> %p10.d", "fcmne  %p7/z %z31.d %z31.d -> %p15.d",
    };
    TEST_LOOP(fcmne, fcmne_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(fcmuo_sve_pred)
{
    /* Testing FCMUO   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_0_0[6] = {
        "fcmuo  %p0/z %z0.h %z0.h -> %p0.h",    "fcmuo  %p2/z %z7.h %z8.h -> %p2.h",
        "fcmuo  %p3/z %z12.h %z13.h -> %p5.h",  "fcmuo  %p5/z %z18.h %z19.h -> %p8.h",
        "fcmuo  %p6/z %z23.h %z24.h -> %p10.h", "fcmuo  %p7/z %z31.h %z31.h -> %p15.h",
    };
    TEST_LOOP(fcmuo, fcmuo_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "fcmuo  %p0/z %z0.s %z0.s -> %p0.s",    "fcmuo  %p2/z %z7.s %z8.s -> %p2.s",
        "fcmuo  %p3/z %z12.s %z13.s -> %p5.s",  "fcmuo  %p5/z %z18.s %z19.s -> %p8.s",
        "fcmuo  %p6/z %z23.s %z24.s -> %p10.s", "fcmuo  %p7/z %z31.s %z31.s -> %p15.s",
    };
    TEST_LOOP(fcmuo, fcmuo_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "fcmuo  %p0/z %z0.d %z0.d -> %p0.d",    "fcmuo  %p2/z %z7.d %z8.d -> %p2.d",
        "fcmuo  %p3/z %z12.d %z13.d -> %p5.d",  "fcmuo  %p5/z %z18.d %z19.d -> %p8.d",
        "fcmuo  %p6/z %z23.d %z24.d -> %p10.d", "fcmuo  %p7/z %z31.d %z31.d -> %p15.d",
    };
    TEST_LOOP(fcmuo, fcmuo_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(cmpeq_sve_pred_simm)
{

    /* Testing CMPEQ   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, #<imm> */
    int imm5_0_0[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_0[6] = {
        "cmpeq  %p0/z %z0.b $0xf0 -> %p0.b",   "cmpeq  %p2/z %z7.b $0xf8 -> %p2.b",
        "cmpeq  %p3/z %z12.b $0xfd -> %p5.b",  "cmpeq  %p5/z %z18.b $0x03 -> %p8.b",
        "cmpeq  %p6/z %z23.b $0x08 -> %p10.b", "cmpeq  %p7/z %z31.b $0x0f -> %p15.b",
    };
    TEST_LOOP(cmpeq, cmpeq_sve_pred_simm, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_immed_int(imm5_0_0[i], OPSZ_5b));

    int imm5_0_1[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_1[6] = {
        "cmpeq  %p0/z %z0.h $0xf0 -> %p0.h",   "cmpeq  %p2/z %z7.h $0xf8 -> %p2.h",
        "cmpeq  %p3/z %z12.h $0xfd -> %p5.h",  "cmpeq  %p5/z %z18.h $0x03 -> %p8.h",
        "cmpeq  %p6/z %z23.h $0x08 -> %p10.h", "cmpeq  %p7/z %z31.h $0x0f -> %p15.h",
    };
    TEST_LOOP(cmpeq, cmpeq_sve_pred_simm, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_immed_int(imm5_0_1[i], OPSZ_5b));

    int imm5_0_2[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_2[6] = {
        "cmpeq  %p0/z %z0.s $0xf0 -> %p0.s",   "cmpeq  %p2/z %z7.s $0xf8 -> %p2.s",
        "cmpeq  %p3/z %z12.s $0xfd -> %p5.s",  "cmpeq  %p5/z %z18.s $0x03 -> %p8.s",
        "cmpeq  %p6/z %z23.s $0x08 -> %p10.s", "cmpeq  %p7/z %z31.s $0x0f -> %p15.s",
    };
    TEST_LOOP(cmpeq, cmpeq_sve_pred_simm, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_immed_int(imm5_0_2[i], OPSZ_5b));

    int imm5_0_3[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_3[6] = {
        "cmpeq  %p0/z %z0.d $0xf0 -> %p0.d",   "cmpeq  %p2/z %z7.d $0xf8 -> %p2.d",
        "cmpeq  %p3/z %z12.d $0xfd -> %p5.d",  "cmpeq  %p5/z %z18.d $0x03 -> %p8.d",
        "cmpeq  %p6/z %z23.d $0x08 -> %p10.d", "cmpeq  %p7/z %z31.d $0x0f -> %p15.d",
    };
    TEST_LOOP(cmpeq, cmpeq_sve_pred_simm, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_immed_int(imm5_0_3[i], OPSZ_5b));
}

TEST_INSTR(cmpeq_sve_pred)
{

    /* Testing CMPEQ   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.D */
    const char *expected_0_0[6] = {
        "cmpeq  %p0/z %z0.b %z0.d -> %p0.b",    "cmpeq  %p2/z %z7.b %z8.d -> %p2.b",
        "cmpeq  %p3/z %z12.b %z13.d -> %p5.b",  "cmpeq  %p5/z %z18.b %z19.d -> %p8.b",
        "cmpeq  %p6/z %z23.b %z24.d -> %p10.b", "cmpeq  %p7/z %z31.b %z31.d -> %p15.b",
    };
    TEST_LOOP(cmpeq, cmpeq_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_1[6] = {
        "cmpeq  %p0/z %z0.h %z0.d -> %p0.h",    "cmpeq  %p2/z %z7.h %z8.d -> %p2.h",
        "cmpeq  %p3/z %z12.h %z13.d -> %p5.h",  "cmpeq  %p5/z %z18.h %z19.d -> %p8.h",
        "cmpeq  %p6/z %z23.h %z24.d -> %p10.h", "cmpeq  %p7/z %z31.h %z31.d -> %p15.h",
    };
    TEST_LOOP(cmpeq, cmpeq_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_2[6] = {
        "cmpeq  %p0/z %z0.s %z0.d -> %p0.s",    "cmpeq  %p2/z %z7.s %z8.d -> %p2.s",
        "cmpeq  %p3/z %z12.s %z13.d -> %p5.s",  "cmpeq  %p5/z %z18.s %z19.d -> %p8.s",
        "cmpeq  %p6/z %z23.s %z24.d -> %p10.s", "cmpeq  %p7/z %z31.s %z31.d -> %p15.s",
    };
    TEST_LOOP(cmpeq, cmpeq_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    /* Testing CMPEQ   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_1_0[6] = {
        "cmpeq  %p0/z %z0.b %z0.b -> %p0.b",    "cmpeq  %p2/z %z7.b %z8.b -> %p2.b",
        "cmpeq  %p3/z %z12.b %z13.b -> %p5.b",  "cmpeq  %p5/z %z18.b %z19.b -> %p8.b",
        "cmpeq  %p6/z %z23.b %z24.b -> %p10.b", "cmpeq  %p7/z %z31.b %z31.b -> %p15.b",
    };
    TEST_LOOP(cmpeq, cmpeq_sve_pred, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_1));

    const char *expected_1_1[6] = {
        "cmpeq  %p0/z %z0.h %z0.h -> %p0.h",    "cmpeq  %p2/z %z7.h %z8.h -> %p2.h",
        "cmpeq  %p3/z %z12.h %z13.h -> %p5.h",  "cmpeq  %p5/z %z18.h %z19.h -> %p8.h",
        "cmpeq  %p6/z %z23.h %z24.h -> %p10.h", "cmpeq  %p7/z %z31.h %z31.h -> %p15.h",
    };
    TEST_LOOP(cmpeq, cmpeq_sve_pred, 6, expected_1_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));

    const char *expected_1_2[6] = {
        "cmpeq  %p0/z %z0.s %z0.s -> %p0.s",    "cmpeq  %p2/z %z7.s %z8.s -> %p2.s",
        "cmpeq  %p3/z %z12.s %z13.s -> %p5.s",  "cmpeq  %p5/z %z18.s %z19.s -> %p8.s",
        "cmpeq  %p6/z %z23.s %z24.s -> %p10.s", "cmpeq  %p7/z %z31.s %z31.s -> %p15.s",
    };
    TEST_LOOP(cmpeq, cmpeq_sve_pred, 6, expected_1_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_4));

    const char *expected_1_3[6] = {
        "cmpeq  %p0/z %z0.d %z0.d -> %p0.d",    "cmpeq  %p2/z %z7.d %z8.d -> %p2.d",
        "cmpeq  %p3/z %z12.d %z13.d -> %p5.d",  "cmpeq  %p5/z %z18.d %z19.d -> %p8.d",
        "cmpeq  %p6/z %z23.d %z24.d -> %p10.d", "cmpeq  %p7/z %z31.d %z31.d -> %p15.d",
    };
    TEST_LOOP(cmpeq, cmpeq_sve_pred, 6, expected_1_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(cmpge_sve_pred_simm)
{

    /* Testing CMPGE   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, #<imm> */
    int imm5_0_0[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_0[6] = {
        "cmpge  %p0/z %z0.b $0xf0 -> %p0.b",   "cmpge  %p2/z %z7.b $0xf8 -> %p2.b",
        "cmpge  %p3/z %z12.b $0xfd -> %p5.b",  "cmpge  %p5/z %z18.b $0x03 -> %p8.b",
        "cmpge  %p6/z %z23.b $0x08 -> %p10.b", "cmpge  %p7/z %z31.b $0x0f -> %p15.b",
    };
    TEST_LOOP(cmpge, cmpge_sve_pred_simm, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_immed_int(imm5_0_0[i], OPSZ_5b));

    int imm5_0_1[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_1[6] = {
        "cmpge  %p0/z %z0.h $0xf0 -> %p0.h",   "cmpge  %p2/z %z7.h $0xf8 -> %p2.h",
        "cmpge  %p3/z %z12.h $0xfd -> %p5.h",  "cmpge  %p5/z %z18.h $0x03 -> %p8.h",
        "cmpge  %p6/z %z23.h $0x08 -> %p10.h", "cmpge  %p7/z %z31.h $0x0f -> %p15.h",
    };
    TEST_LOOP(cmpge, cmpge_sve_pred_simm, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_immed_int(imm5_0_1[i], OPSZ_5b));

    int imm5_0_2[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_2[6] = {
        "cmpge  %p0/z %z0.s $0xf0 -> %p0.s",   "cmpge  %p2/z %z7.s $0xf8 -> %p2.s",
        "cmpge  %p3/z %z12.s $0xfd -> %p5.s",  "cmpge  %p5/z %z18.s $0x03 -> %p8.s",
        "cmpge  %p6/z %z23.s $0x08 -> %p10.s", "cmpge  %p7/z %z31.s $0x0f -> %p15.s",
    };
    TEST_LOOP(cmpge, cmpge_sve_pred_simm, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_immed_int(imm5_0_2[i], OPSZ_5b));

    int imm5_0_3[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_3[6] = {
        "cmpge  %p0/z %z0.d $0xf0 -> %p0.d",   "cmpge  %p2/z %z7.d $0xf8 -> %p2.d",
        "cmpge  %p3/z %z12.d $0xfd -> %p5.d",  "cmpge  %p5/z %z18.d $0x03 -> %p8.d",
        "cmpge  %p6/z %z23.d $0x08 -> %p10.d", "cmpge  %p7/z %z31.d $0x0f -> %p15.d",
    };
    TEST_LOOP(cmpge, cmpge_sve_pred_simm, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_immed_int(imm5_0_3[i], OPSZ_5b));
}

TEST_INSTR(cmpge_sve_pred)
{

    /* Testing CMPGE   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.D */
    const char *expected_0_0[6] = {
        "cmpge  %p0/z %z0.b %z0.d -> %p0.b",    "cmpge  %p2/z %z7.b %z8.d -> %p2.b",
        "cmpge  %p3/z %z12.b %z13.d -> %p5.b",  "cmpge  %p5/z %z18.b %z19.d -> %p8.b",
        "cmpge  %p6/z %z23.b %z24.d -> %p10.b", "cmpge  %p7/z %z31.b %z31.d -> %p15.b",
    };
    TEST_LOOP(cmpge, cmpge_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_1[6] = {
        "cmpge  %p0/z %z0.h %z0.d -> %p0.h",    "cmpge  %p2/z %z7.h %z8.d -> %p2.h",
        "cmpge  %p3/z %z12.h %z13.d -> %p5.h",  "cmpge  %p5/z %z18.h %z19.d -> %p8.h",
        "cmpge  %p6/z %z23.h %z24.d -> %p10.h", "cmpge  %p7/z %z31.h %z31.d -> %p15.h",
    };
    TEST_LOOP(cmpge, cmpge_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_2[6] = {
        "cmpge  %p0/z %z0.s %z0.d -> %p0.s",    "cmpge  %p2/z %z7.s %z8.d -> %p2.s",
        "cmpge  %p3/z %z12.s %z13.d -> %p5.s",  "cmpge  %p5/z %z18.s %z19.d -> %p8.s",
        "cmpge  %p6/z %z23.s %z24.d -> %p10.s", "cmpge  %p7/z %z31.s %z31.d -> %p15.s",
    };
    TEST_LOOP(cmpge, cmpge_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    /* Testing CMPGE   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_1_0[6] = {
        "cmpge  %p0/z %z0.b %z0.b -> %p0.b",    "cmpge  %p2/z %z7.b %z8.b -> %p2.b",
        "cmpge  %p3/z %z12.b %z13.b -> %p5.b",  "cmpge  %p5/z %z18.b %z19.b -> %p8.b",
        "cmpge  %p6/z %z23.b %z24.b -> %p10.b", "cmpge  %p7/z %z31.b %z31.b -> %p15.b",
    };
    TEST_LOOP(cmpge, cmpge_sve_pred, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_1));

    const char *expected_1_1[6] = {
        "cmpge  %p0/z %z0.h %z0.h -> %p0.h",    "cmpge  %p2/z %z7.h %z8.h -> %p2.h",
        "cmpge  %p3/z %z12.h %z13.h -> %p5.h",  "cmpge  %p5/z %z18.h %z19.h -> %p8.h",
        "cmpge  %p6/z %z23.h %z24.h -> %p10.h", "cmpge  %p7/z %z31.h %z31.h -> %p15.h",
    };
    TEST_LOOP(cmpge, cmpge_sve_pred, 6, expected_1_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));

    const char *expected_1_2[6] = {
        "cmpge  %p0/z %z0.s %z0.s -> %p0.s",    "cmpge  %p2/z %z7.s %z8.s -> %p2.s",
        "cmpge  %p3/z %z12.s %z13.s -> %p5.s",  "cmpge  %p5/z %z18.s %z19.s -> %p8.s",
        "cmpge  %p6/z %z23.s %z24.s -> %p10.s", "cmpge  %p7/z %z31.s %z31.s -> %p15.s",
    };
    TEST_LOOP(cmpge, cmpge_sve_pred, 6, expected_1_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_4));

    const char *expected_1_3[6] = {
        "cmpge  %p0/z %z0.d %z0.d -> %p0.d",    "cmpge  %p2/z %z7.d %z8.d -> %p2.d",
        "cmpge  %p3/z %z12.d %z13.d -> %p5.d",  "cmpge  %p5/z %z18.d %z19.d -> %p8.d",
        "cmpge  %p6/z %z23.d %z24.d -> %p10.d", "cmpge  %p7/z %z31.d %z31.d -> %p15.d",
    };
    TEST_LOOP(cmpge, cmpge_sve_pred, 6, expected_1_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(cmpgt_sve_pred_simm)
{

    /* Testing CMPGT   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, #<imm> */
    int imm5_0_0[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_0[6] = {
        "cmpgt  %p0/z %z0.b $0xf0 -> %p0.b",   "cmpgt  %p2/z %z7.b $0xf8 -> %p2.b",
        "cmpgt  %p3/z %z12.b $0xfd -> %p5.b",  "cmpgt  %p5/z %z18.b $0x03 -> %p8.b",
        "cmpgt  %p6/z %z23.b $0x08 -> %p10.b", "cmpgt  %p7/z %z31.b $0x0f -> %p15.b",
    };
    TEST_LOOP(cmpgt, cmpgt_sve_pred_simm, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_immed_int(imm5_0_0[i], OPSZ_5b));

    int imm5_0_1[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_1[6] = {
        "cmpgt  %p0/z %z0.h $0xf0 -> %p0.h",   "cmpgt  %p2/z %z7.h $0xf8 -> %p2.h",
        "cmpgt  %p3/z %z12.h $0xfd -> %p5.h",  "cmpgt  %p5/z %z18.h $0x03 -> %p8.h",
        "cmpgt  %p6/z %z23.h $0x08 -> %p10.h", "cmpgt  %p7/z %z31.h $0x0f -> %p15.h",
    };
    TEST_LOOP(cmpgt, cmpgt_sve_pred_simm, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_immed_int(imm5_0_1[i], OPSZ_5b));

    int imm5_0_2[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_2[6] = {
        "cmpgt  %p0/z %z0.s $0xf0 -> %p0.s",   "cmpgt  %p2/z %z7.s $0xf8 -> %p2.s",
        "cmpgt  %p3/z %z12.s $0xfd -> %p5.s",  "cmpgt  %p5/z %z18.s $0x03 -> %p8.s",
        "cmpgt  %p6/z %z23.s $0x08 -> %p10.s", "cmpgt  %p7/z %z31.s $0x0f -> %p15.s",
    };
    TEST_LOOP(cmpgt, cmpgt_sve_pred_simm, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_immed_int(imm5_0_2[i], OPSZ_5b));

    int imm5_0_3[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_3[6] = {
        "cmpgt  %p0/z %z0.d $0xf0 -> %p0.d",   "cmpgt  %p2/z %z7.d $0xf8 -> %p2.d",
        "cmpgt  %p3/z %z12.d $0xfd -> %p5.d",  "cmpgt  %p5/z %z18.d $0x03 -> %p8.d",
        "cmpgt  %p6/z %z23.d $0x08 -> %p10.d", "cmpgt  %p7/z %z31.d $0x0f -> %p15.d",
    };
    TEST_LOOP(cmpgt, cmpgt_sve_pred_simm, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_immed_int(imm5_0_3[i], OPSZ_5b));
}

TEST_INSTR(cmpgt_sve_pred)
{

    /* Testing CMPGT   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.D */
    const char *expected_0_0[6] = {
        "cmpgt  %p0/z %z0.b %z0.d -> %p0.b",    "cmpgt  %p2/z %z7.b %z8.d -> %p2.b",
        "cmpgt  %p3/z %z12.b %z13.d -> %p5.b",  "cmpgt  %p5/z %z18.b %z19.d -> %p8.b",
        "cmpgt  %p6/z %z23.b %z24.d -> %p10.b", "cmpgt  %p7/z %z31.b %z31.d -> %p15.b",
    };
    TEST_LOOP(cmpgt, cmpgt_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_1[6] = {
        "cmpgt  %p0/z %z0.h %z0.d -> %p0.h",    "cmpgt  %p2/z %z7.h %z8.d -> %p2.h",
        "cmpgt  %p3/z %z12.h %z13.d -> %p5.h",  "cmpgt  %p5/z %z18.h %z19.d -> %p8.h",
        "cmpgt  %p6/z %z23.h %z24.d -> %p10.h", "cmpgt  %p7/z %z31.h %z31.d -> %p15.h",
    };
    TEST_LOOP(cmpgt, cmpgt_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_2[6] = {
        "cmpgt  %p0/z %z0.s %z0.d -> %p0.s",    "cmpgt  %p2/z %z7.s %z8.d -> %p2.s",
        "cmpgt  %p3/z %z12.s %z13.d -> %p5.s",  "cmpgt  %p5/z %z18.s %z19.d -> %p8.s",
        "cmpgt  %p6/z %z23.s %z24.d -> %p10.s", "cmpgt  %p7/z %z31.s %z31.d -> %p15.s",
    };
    TEST_LOOP(cmpgt, cmpgt_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    /* Testing CMPGT   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_1_0[6] = {
        "cmpgt  %p0/z %z0.b %z0.b -> %p0.b",    "cmpgt  %p2/z %z7.b %z8.b -> %p2.b",
        "cmpgt  %p3/z %z12.b %z13.b -> %p5.b",  "cmpgt  %p5/z %z18.b %z19.b -> %p8.b",
        "cmpgt  %p6/z %z23.b %z24.b -> %p10.b", "cmpgt  %p7/z %z31.b %z31.b -> %p15.b",
    };
    TEST_LOOP(cmpgt, cmpgt_sve_pred, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_1));

    const char *expected_1_1[6] = {
        "cmpgt  %p0/z %z0.h %z0.h -> %p0.h",    "cmpgt  %p2/z %z7.h %z8.h -> %p2.h",
        "cmpgt  %p3/z %z12.h %z13.h -> %p5.h",  "cmpgt  %p5/z %z18.h %z19.h -> %p8.h",
        "cmpgt  %p6/z %z23.h %z24.h -> %p10.h", "cmpgt  %p7/z %z31.h %z31.h -> %p15.h",
    };
    TEST_LOOP(cmpgt, cmpgt_sve_pred, 6, expected_1_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));

    const char *expected_1_2[6] = {
        "cmpgt  %p0/z %z0.s %z0.s -> %p0.s",    "cmpgt  %p2/z %z7.s %z8.s -> %p2.s",
        "cmpgt  %p3/z %z12.s %z13.s -> %p5.s",  "cmpgt  %p5/z %z18.s %z19.s -> %p8.s",
        "cmpgt  %p6/z %z23.s %z24.s -> %p10.s", "cmpgt  %p7/z %z31.s %z31.s -> %p15.s",
    };
    TEST_LOOP(cmpgt, cmpgt_sve_pred, 6, expected_1_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_4));

    const char *expected_1_3[6] = {
        "cmpgt  %p0/z %z0.d %z0.d -> %p0.d",    "cmpgt  %p2/z %z7.d %z8.d -> %p2.d",
        "cmpgt  %p3/z %z12.d %z13.d -> %p5.d",  "cmpgt  %p5/z %z18.d %z19.d -> %p8.d",
        "cmpgt  %p6/z %z23.d %z24.d -> %p10.d", "cmpgt  %p7/z %z31.d %z31.d -> %p15.d",
    };
    TEST_LOOP(cmpgt, cmpgt_sve_pred, 6, expected_1_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(cmphi_sve_pred_imm)
{

    /* Testing CMPHI   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, #<imm> */
    uint imm7_0_0[6] = { 0, 24, 45, 67, 88, 127 };
    const char *expected_0_0[6] = {
        "cmphi  %p0/z %z0.b $0x00 -> %p0.b",   "cmphi  %p2/z %z7.b $0x18 -> %p2.b",
        "cmphi  %p3/z %z12.b $0x2d -> %p5.b",  "cmphi  %p5/z %z18.b $0x43 -> %p8.b",
        "cmphi  %p6/z %z23.b $0x58 -> %p10.b", "cmphi  %p7/z %z31.b $0x7f -> %p15.b",
    };
    TEST_LOOP(cmphi, cmphi_sve_pred_imm, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_immed_uint(imm7_0_0[i], OPSZ_7b));

    uint imm7_0_1[6] = { 0, 24, 45, 67, 88, 127 };
    const char *expected_0_1[6] = {
        "cmphi  %p0/z %z0.h $0x00 -> %p0.h",   "cmphi  %p2/z %z7.h $0x18 -> %p2.h",
        "cmphi  %p3/z %z12.h $0x2d -> %p5.h",  "cmphi  %p5/z %z18.h $0x43 -> %p8.h",
        "cmphi  %p6/z %z23.h $0x58 -> %p10.h", "cmphi  %p7/z %z31.h $0x7f -> %p15.h",
    };
    TEST_LOOP(cmphi, cmphi_sve_pred_imm, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_immed_uint(imm7_0_1[i], OPSZ_7b));

    uint imm7_0_2[6] = { 0, 24, 45, 67, 88, 127 };
    const char *expected_0_2[6] = {
        "cmphi  %p0/z %z0.s $0x00 -> %p0.s",   "cmphi  %p2/z %z7.s $0x18 -> %p2.s",
        "cmphi  %p3/z %z12.s $0x2d -> %p5.s",  "cmphi  %p5/z %z18.s $0x43 -> %p8.s",
        "cmphi  %p6/z %z23.s $0x58 -> %p10.s", "cmphi  %p7/z %z31.s $0x7f -> %p15.s",
    };
    TEST_LOOP(cmphi, cmphi_sve_pred_imm, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_immed_uint(imm7_0_2[i], OPSZ_7b));

    uint imm7_0_3[6] = { 0, 24, 45, 67, 88, 127 };
    const char *expected_0_3[6] = {
        "cmphi  %p0/z %z0.d $0x00 -> %p0.d",   "cmphi  %p2/z %z7.d $0x18 -> %p2.d",
        "cmphi  %p3/z %z12.d $0x2d -> %p5.d",  "cmphi  %p5/z %z18.d $0x43 -> %p8.d",
        "cmphi  %p6/z %z23.d $0x58 -> %p10.d", "cmphi  %p7/z %z31.d $0x7f -> %p15.d",
    };
    TEST_LOOP(cmphi, cmphi_sve_pred_imm, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_immed_uint(imm7_0_3[i], OPSZ_7b));
}

TEST_INSTR(cmphi_sve_pred)
{

    /* Testing CMPHI   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.D */
    const char *expected_0_0[6] = {
        "cmphi  %p0/z %z0.b %z0.d -> %p0.b",    "cmphi  %p2/z %z7.b %z8.d -> %p2.b",
        "cmphi  %p3/z %z12.b %z13.d -> %p5.b",  "cmphi  %p5/z %z18.b %z19.d -> %p8.b",
        "cmphi  %p6/z %z23.b %z24.d -> %p10.b", "cmphi  %p7/z %z31.b %z31.d -> %p15.b",
    };
    TEST_LOOP(cmphi, cmphi_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_1[6] = {
        "cmphi  %p0/z %z0.h %z0.d -> %p0.h",    "cmphi  %p2/z %z7.h %z8.d -> %p2.h",
        "cmphi  %p3/z %z12.h %z13.d -> %p5.h",  "cmphi  %p5/z %z18.h %z19.d -> %p8.h",
        "cmphi  %p6/z %z23.h %z24.d -> %p10.h", "cmphi  %p7/z %z31.h %z31.d -> %p15.h",
    };
    TEST_LOOP(cmphi, cmphi_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_2[6] = {
        "cmphi  %p0/z %z0.s %z0.d -> %p0.s",    "cmphi  %p2/z %z7.s %z8.d -> %p2.s",
        "cmphi  %p3/z %z12.s %z13.d -> %p5.s",  "cmphi  %p5/z %z18.s %z19.d -> %p8.s",
        "cmphi  %p6/z %z23.s %z24.d -> %p10.s", "cmphi  %p7/z %z31.s %z31.d -> %p15.s",
    };
    TEST_LOOP(cmphi, cmphi_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    /* Testing CMPHI   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_1_0[6] = {
        "cmphi  %p0/z %z0.b %z0.b -> %p0.b",    "cmphi  %p2/z %z7.b %z8.b -> %p2.b",
        "cmphi  %p3/z %z12.b %z13.b -> %p5.b",  "cmphi  %p5/z %z18.b %z19.b -> %p8.b",
        "cmphi  %p6/z %z23.b %z24.b -> %p10.b", "cmphi  %p7/z %z31.b %z31.b -> %p15.b",
    };
    TEST_LOOP(cmphi, cmphi_sve_pred, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_1));

    const char *expected_1_1[6] = {
        "cmphi  %p0/z %z0.h %z0.h -> %p0.h",    "cmphi  %p2/z %z7.h %z8.h -> %p2.h",
        "cmphi  %p3/z %z12.h %z13.h -> %p5.h",  "cmphi  %p5/z %z18.h %z19.h -> %p8.h",
        "cmphi  %p6/z %z23.h %z24.h -> %p10.h", "cmphi  %p7/z %z31.h %z31.h -> %p15.h",
    };
    TEST_LOOP(cmphi, cmphi_sve_pred, 6, expected_1_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));

    const char *expected_1_2[6] = {
        "cmphi  %p0/z %z0.s %z0.s -> %p0.s",    "cmphi  %p2/z %z7.s %z8.s -> %p2.s",
        "cmphi  %p3/z %z12.s %z13.s -> %p5.s",  "cmphi  %p5/z %z18.s %z19.s -> %p8.s",
        "cmphi  %p6/z %z23.s %z24.s -> %p10.s", "cmphi  %p7/z %z31.s %z31.s -> %p15.s",
    };
    TEST_LOOP(cmphi, cmphi_sve_pred, 6, expected_1_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_4));

    const char *expected_1_3[6] = {
        "cmphi  %p0/z %z0.d %z0.d -> %p0.d",    "cmphi  %p2/z %z7.d %z8.d -> %p2.d",
        "cmphi  %p3/z %z12.d %z13.d -> %p5.d",  "cmphi  %p5/z %z18.d %z19.d -> %p8.d",
        "cmphi  %p6/z %z23.d %z24.d -> %p10.d", "cmphi  %p7/z %z31.d %z31.d -> %p15.d",
    };
    TEST_LOOP(cmphi, cmphi_sve_pred, 6, expected_1_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(cmphs_sve_pred_imm)
{

    /* Testing CMPHS   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, #<imm> */
    uint imm7_0_0[6] = { 0, 24, 45, 67, 88, 127 };
    const char *expected_0_0[6] = {
        "cmphs  %p0/z %z0.b $0x00 -> %p0.b",   "cmphs  %p2/z %z7.b $0x18 -> %p2.b",
        "cmphs  %p3/z %z12.b $0x2d -> %p5.b",  "cmphs  %p5/z %z18.b $0x43 -> %p8.b",
        "cmphs  %p6/z %z23.b $0x58 -> %p10.b", "cmphs  %p7/z %z31.b $0x7f -> %p15.b",
    };
    TEST_LOOP(cmphs, cmphs_sve_pred_imm, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_immed_uint(imm7_0_0[i], OPSZ_7b));

    uint imm7_0_1[6] = { 0, 24, 45, 67, 88, 127 };
    const char *expected_0_1[6] = {
        "cmphs  %p0/z %z0.h $0x00 -> %p0.h",   "cmphs  %p2/z %z7.h $0x18 -> %p2.h",
        "cmphs  %p3/z %z12.h $0x2d -> %p5.h",  "cmphs  %p5/z %z18.h $0x43 -> %p8.h",
        "cmphs  %p6/z %z23.h $0x58 -> %p10.h", "cmphs  %p7/z %z31.h $0x7f -> %p15.h",
    };
    TEST_LOOP(cmphs, cmphs_sve_pred_imm, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_immed_uint(imm7_0_1[i], OPSZ_7b));

    uint imm7_0_2[6] = { 0, 24, 45, 67, 88, 127 };
    const char *expected_0_2[6] = {
        "cmphs  %p0/z %z0.s $0x00 -> %p0.s",   "cmphs  %p2/z %z7.s $0x18 -> %p2.s",
        "cmphs  %p3/z %z12.s $0x2d -> %p5.s",  "cmphs  %p5/z %z18.s $0x43 -> %p8.s",
        "cmphs  %p6/z %z23.s $0x58 -> %p10.s", "cmphs  %p7/z %z31.s $0x7f -> %p15.s",
    };
    TEST_LOOP(cmphs, cmphs_sve_pred_imm, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_immed_uint(imm7_0_2[i], OPSZ_7b));

    uint imm7_0_3[6] = { 0, 24, 45, 67, 88, 127 };
    const char *expected_0_3[6] = {
        "cmphs  %p0/z %z0.d $0x00 -> %p0.d",   "cmphs  %p2/z %z7.d $0x18 -> %p2.d",
        "cmphs  %p3/z %z12.d $0x2d -> %p5.d",  "cmphs  %p5/z %z18.d $0x43 -> %p8.d",
        "cmphs  %p6/z %z23.d $0x58 -> %p10.d", "cmphs  %p7/z %z31.d $0x7f -> %p15.d",
    };
    TEST_LOOP(cmphs, cmphs_sve_pred_imm, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_immed_uint(imm7_0_3[i], OPSZ_7b));
}

TEST_INSTR(cmphs_sve_pred)
{

    /* Testing CMPHS   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.D */
    const char *expected_0_0[6] = {
        "cmphs  %p0/z %z0.b %z0.d -> %p0.b",    "cmphs  %p2/z %z7.b %z8.d -> %p2.b",
        "cmphs  %p3/z %z12.b %z13.d -> %p5.b",  "cmphs  %p5/z %z18.b %z19.d -> %p8.b",
        "cmphs  %p6/z %z23.b %z24.d -> %p10.b", "cmphs  %p7/z %z31.b %z31.d -> %p15.b",
    };
    TEST_LOOP(cmphs, cmphs_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_1[6] = {
        "cmphs  %p0/z %z0.h %z0.d -> %p0.h",    "cmphs  %p2/z %z7.h %z8.d -> %p2.h",
        "cmphs  %p3/z %z12.h %z13.d -> %p5.h",  "cmphs  %p5/z %z18.h %z19.d -> %p8.h",
        "cmphs  %p6/z %z23.h %z24.d -> %p10.h", "cmphs  %p7/z %z31.h %z31.d -> %p15.h",
    };
    TEST_LOOP(cmphs, cmphs_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_2[6] = {
        "cmphs  %p0/z %z0.s %z0.d -> %p0.s",    "cmphs  %p2/z %z7.s %z8.d -> %p2.s",
        "cmphs  %p3/z %z12.s %z13.d -> %p5.s",  "cmphs  %p5/z %z18.s %z19.d -> %p8.s",
        "cmphs  %p6/z %z23.s %z24.d -> %p10.s", "cmphs  %p7/z %z31.s %z31.d -> %p15.s",
    };
    TEST_LOOP(cmphs, cmphs_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    /* Testing CMPHS   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_1_0[6] = {
        "cmphs  %p0/z %z0.b %z0.b -> %p0.b",    "cmphs  %p2/z %z7.b %z8.b -> %p2.b",
        "cmphs  %p3/z %z12.b %z13.b -> %p5.b",  "cmphs  %p5/z %z18.b %z19.b -> %p8.b",
        "cmphs  %p6/z %z23.b %z24.b -> %p10.b", "cmphs  %p7/z %z31.b %z31.b -> %p15.b",
    };
    TEST_LOOP(cmphs, cmphs_sve_pred, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_1));

    const char *expected_1_1[6] = {
        "cmphs  %p0/z %z0.h %z0.h -> %p0.h",    "cmphs  %p2/z %z7.h %z8.h -> %p2.h",
        "cmphs  %p3/z %z12.h %z13.h -> %p5.h",  "cmphs  %p5/z %z18.h %z19.h -> %p8.h",
        "cmphs  %p6/z %z23.h %z24.h -> %p10.h", "cmphs  %p7/z %z31.h %z31.h -> %p15.h",
    };
    TEST_LOOP(cmphs, cmphs_sve_pred, 6, expected_1_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));

    const char *expected_1_2[6] = {
        "cmphs  %p0/z %z0.s %z0.s -> %p0.s",    "cmphs  %p2/z %z7.s %z8.s -> %p2.s",
        "cmphs  %p3/z %z12.s %z13.s -> %p5.s",  "cmphs  %p5/z %z18.s %z19.s -> %p8.s",
        "cmphs  %p6/z %z23.s %z24.s -> %p10.s", "cmphs  %p7/z %z31.s %z31.s -> %p15.s",
    };
    TEST_LOOP(cmphs, cmphs_sve_pred, 6, expected_1_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_4));

    const char *expected_1_3[6] = {
        "cmphs  %p0/z %z0.d %z0.d -> %p0.d",    "cmphs  %p2/z %z7.d %z8.d -> %p2.d",
        "cmphs  %p3/z %z12.d %z13.d -> %p5.d",  "cmphs  %p5/z %z18.d %z19.d -> %p8.d",
        "cmphs  %p6/z %z23.d %z24.d -> %p10.d", "cmphs  %p7/z %z31.d %z31.d -> %p15.d",
    };
    TEST_LOOP(cmphs, cmphs_sve_pred, 6, expected_1_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(cmple_sve_pred_simm)
{

    /* Testing CMPLE   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, #<imm> */
    int imm5_0_0[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_0[6] = {
        "cmple  %p0/z %z0.b $0xf0 -> %p0.b",   "cmple  %p2/z %z7.b $0xf8 -> %p2.b",
        "cmple  %p3/z %z12.b $0xfd -> %p5.b",  "cmple  %p5/z %z18.b $0x03 -> %p8.b",
        "cmple  %p6/z %z23.b $0x08 -> %p10.b", "cmple  %p7/z %z31.b $0x0f -> %p15.b",
    };
    TEST_LOOP(cmple, cmple_sve_pred_simm, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_immed_int(imm5_0_0[i], OPSZ_5b));

    int imm5_0_1[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_1[6] = {
        "cmple  %p0/z %z0.h $0xf0 -> %p0.h",   "cmple  %p2/z %z7.h $0xf8 -> %p2.h",
        "cmple  %p3/z %z12.h $0xfd -> %p5.h",  "cmple  %p5/z %z18.h $0x03 -> %p8.h",
        "cmple  %p6/z %z23.h $0x08 -> %p10.h", "cmple  %p7/z %z31.h $0x0f -> %p15.h",
    };
    TEST_LOOP(cmple, cmple_sve_pred_simm, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_immed_int(imm5_0_1[i], OPSZ_5b));

    int imm5_0_2[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_2[6] = {
        "cmple  %p0/z %z0.s $0xf0 -> %p0.s",   "cmple  %p2/z %z7.s $0xf8 -> %p2.s",
        "cmple  %p3/z %z12.s $0xfd -> %p5.s",  "cmple  %p5/z %z18.s $0x03 -> %p8.s",
        "cmple  %p6/z %z23.s $0x08 -> %p10.s", "cmple  %p7/z %z31.s $0x0f -> %p15.s",
    };
    TEST_LOOP(cmple, cmple_sve_pred_simm, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_immed_int(imm5_0_2[i], OPSZ_5b));

    int imm5_0_3[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_3[6] = {
        "cmple  %p0/z %z0.d $0xf0 -> %p0.d",   "cmple  %p2/z %z7.d $0xf8 -> %p2.d",
        "cmple  %p3/z %z12.d $0xfd -> %p5.d",  "cmple  %p5/z %z18.d $0x03 -> %p8.d",
        "cmple  %p6/z %z23.d $0x08 -> %p10.d", "cmple  %p7/z %z31.d $0x0f -> %p15.d",
    };
    TEST_LOOP(cmple, cmple_sve_pred_simm, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_immed_int(imm5_0_3[i], OPSZ_5b));
}

TEST_INSTR(cmple_sve_pred)
{

    /* Testing CMPLE   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.D */
    const char *expected_0_0[6] = {
        "cmple  %p0/z %z0.b %z0.d -> %p0.b",    "cmple  %p2/z %z7.b %z8.d -> %p2.b",
        "cmple  %p3/z %z12.b %z13.d -> %p5.b",  "cmple  %p5/z %z18.b %z19.d -> %p8.b",
        "cmple  %p6/z %z23.b %z24.d -> %p10.b", "cmple  %p7/z %z31.b %z31.d -> %p15.b",
    };
    TEST_LOOP(cmple, cmple_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_1[6] = {
        "cmple  %p0/z %z0.h %z0.d -> %p0.h",    "cmple  %p2/z %z7.h %z8.d -> %p2.h",
        "cmple  %p3/z %z12.h %z13.d -> %p5.h",  "cmple  %p5/z %z18.h %z19.d -> %p8.h",
        "cmple  %p6/z %z23.h %z24.d -> %p10.h", "cmple  %p7/z %z31.h %z31.d -> %p15.h",
    };
    TEST_LOOP(cmple, cmple_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_2[6] = {
        "cmple  %p0/z %z0.s %z0.d -> %p0.s",    "cmple  %p2/z %z7.s %z8.d -> %p2.s",
        "cmple  %p3/z %z12.s %z13.d -> %p5.s",  "cmple  %p5/z %z18.s %z19.d -> %p8.s",
        "cmple  %p6/z %z23.s %z24.d -> %p10.s", "cmple  %p7/z %z31.s %z31.d -> %p15.s",
    };
    TEST_LOOP(cmple, cmple_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(cmplo_sve_pred_imm)
{

    /* Testing CMPLO   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, #<imm> */
    uint imm7_0_0[6] = { 0, 24, 45, 67, 88, 127 };
    const char *expected_0_0[6] = {
        "cmplo  %p0/z %z0.b $0x00 -> %p0.b",   "cmplo  %p2/z %z7.b $0x18 -> %p2.b",
        "cmplo  %p3/z %z12.b $0x2d -> %p5.b",  "cmplo  %p5/z %z18.b $0x43 -> %p8.b",
        "cmplo  %p6/z %z23.b $0x58 -> %p10.b", "cmplo  %p7/z %z31.b $0x7f -> %p15.b",
    };
    TEST_LOOP(cmplo, cmplo_sve_pred_imm, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_immed_uint(imm7_0_0[i], OPSZ_7b));

    uint imm7_0_1[6] = { 0, 24, 45, 67, 88, 127 };
    const char *expected_0_1[6] = {
        "cmplo  %p0/z %z0.h $0x00 -> %p0.h",   "cmplo  %p2/z %z7.h $0x18 -> %p2.h",
        "cmplo  %p3/z %z12.h $0x2d -> %p5.h",  "cmplo  %p5/z %z18.h $0x43 -> %p8.h",
        "cmplo  %p6/z %z23.h $0x58 -> %p10.h", "cmplo  %p7/z %z31.h $0x7f -> %p15.h",
    };
    TEST_LOOP(cmplo, cmplo_sve_pred_imm, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_immed_uint(imm7_0_1[i], OPSZ_7b));

    uint imm7_0_2[6] = { 0, 24, 45, 67, 88, 127 };
    const char *expected_0_2[6] = {
        "cmplo  %p0/z %z0.s $0x00 -> %p0.s",   "cmplo  %p2/z %z7.s $0x18 -> %p2.s",
        "cmplo  %p3/z %z12.s $0x2d -> %p5.s",  "cmplo  %p5/z %z18.s $0x43 -> %p8.s",
        "cmplo  %p6/z %z23.s $0x58 -> %p10.s", "cmplo  %p7/z %z31.s $0x7f -> %p15.s",
    };
    TEST_LOOP(cmplo, cmplo_sve_pred_imm, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_immed_uint(imm7_0_2[i], OPSZ_7b));

    uint imm7_0_3[6] = { 0, 24, 45, 67, 88, 127 };
    const char *expected_0_3[6] = {
        "cmplo  %p0/z %z0.d $0x00 -> %p0.d",   "cmplo  %p2/z %z7.d $0x18 -> %p2.d",
        "cmplo  %p3/z %z12.d $0x2d -> %p5.d",  "cmplo  %p5/z %z18.d $0x43 -> %p8.d",
        "cmplo  %p6/z %z23.d $0x58 -> %p10.d", "cmplo  %p7/z %z31.d $0x7f -> %p15.d",
    };
    TEST_LOOP(cmplo, cmplo_sve_pred_imm, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_immed_uint(imm7_0_3[i], OPSZ_7b));
}

TEST_INSTR(cmplo_sve_pred)
{

    /* Testing CMPLO   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.D */
    const char *expected_0_0[6] = {
        "cmplo  %p0/z %z0.b %z0.d -> %p0.b",    "cmplo  %p2/z %z7.b %z8.d -> %p2.b",
        "cmplo  %p3/z %z12.b %z13.d -> %p5.b",  "cmplo  %p5/z %z18.b %z19.d -> %p8.b",
        "cmplo  %p6/z %z23.b %z24.d -> %p10.b", "cmplo  %p7/z %z31.b %z31.d -> %p15.b",
    };
    TEST_LOOP(cmplo, cmplo_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_1[6] = {
        "cmplo  %p0/z %z0.h %z0.d -> %p0.h",    "cmplo  %p2/z %z7.h %z8.d -> %p2.h",
        "cmplo  %p3/z %z12.h %z13.d -> %p5.h",  "cmplo  %p5/z %z18.h %z19.d -> %p8.h",
        "cmplo  %p6/z %z23.h %z24.d -> %p10.h", "cmplo  %p7/z %z31.h %z31.d -> %p15.h",
    };
    TEST_LOOP(cmplo, cmplo_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_2[6] = {
        "cmplo  %p0/z %z0.s %z0.d -> %p0.s",    "cmplo  %p2/z %z7.s %z8.d -> %p2.s",
        "cmplo  %p3/z %z12.s %z13.d -> %p5.s",  "cmplo  %p5/z %z18.s %z19.d -> %p8.s",
        "cmplo  %p6/z %z23.s %z24.d -> %p10.s", "cmplo  %p7/z %z31.s %z31.d -> %p15.s",
    };
    TEST_LOOP(cmplo, cmplo_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(cmpls_sve_pred_imm)
{

    /* Testing CMPLS   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, #<imm> */
    uint imm7_0_0[6] = { 0, 24, 45, 67, 88, 127 };
    const char *expected_0_0[6] = {
        "cmpls  %p0/z %z0.b $0x00 -> %p0.b",   "cmpls  %p2/z %z7.b $0x18 -> %p2.b",
        "cmpls  %p3/z %z12.b $0x2d -> %p5.b",  "cmpls  %p5/z %z18.b $0x43 -> %p8.b",
        "cmpls  %p6/z %z23.b $0x58 -> %p10.b", "cmpls  %p7/z %z31.b $0x7f -> %p15.b",
    };
    TEST_LOOP(cmpls, cmpls_sve_pred_imm, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_immed_uint(imm7_0_0[i], OPSZ_7b));

    uint imm7_0_1[6] = { 0, 24, 45, 67, 88, 127 };
    const char *expected_0_1[6] = {
        "cmpls  %p0/z %z0.h $0x00 -> %p0.h",   "cmpls  %p2/z %z7.h $0x18 -> %p2.h",
        "cmpls  %p3/z %z12.h $0x2d -> %p5.h",  "cmpls  %p5/z %z18.h $0x43 -> %p8.h",
        "cmpls  %p6/z %z23.h $0x58 -> %p10.h", "cmpls  %p7/z %z31.h $0x7f -> %p15.h",
    };
    TEST_LOOP(cmpls, cmpls_sve_pred_imm, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_immed_uint(imm7_0_1[i], OPSZ_7b));

    uint imm7_0_2[6] = { 0, 24, 45, 67, 88, 127 };
    const char *expected_0_2[6] = {
        "cmpls  %p0/z %z0.s $0x00 -> %p0.s",   "cmpls  %p2/z %z7.s $0x18 -> %p2.s",
        "cmpls  %p3/z %z12.s $0x2d -> %p5.s",  "cmpls  %p5/z %z18.s $0x43 -> %p8.s",
        "cmpls  %p6/z %z23.s $0x58 -> %p10.s", "cmpls  %p7/z %z31.s $0x7f -> %p15.s",
    };
    TEST_LOOP(cmpls, cmpls_sve_pred_imm, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_immed_uint(imm7_0_2[i], OPSZ_7b));

    uint imm7_0_3[6] = { 0, 24, 45, 67, 88, 127 };
    const char *expected_0_3[6] = {
        "cmpls  %p0/z %z0.d $0x00 -> %p0.d",   "cmpls  %p2/z %z7.d $0x18 -> %p2.d",
        "cmpls  %p3/z %z12.d $0x2d -> %p5.d",  "cmpls  %p5/z %z18.d $0x43 -> %p8.d",
        "cmpls  %p6/z %z23.d $0x58 -> %p10.d", "cmpls  %p7/z %z31.d $0x7f -> %p15.d",
    };
    TEST_LOOP(cmpls, cmpls_sve_pred_imm, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_immed_uint(imm7_0_3[i], OPSZ_7b));
}

TEST_INSTR(cmpls_sve_pred)
{

    /* Testing CMPLS   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.D */
    const char *expected_0_0[6] = {
        "cmpls  %p0/z %z0.b %z0.d -> %p0.b",    "cmpls  %p2/z %z7.b %z8.d -> %p2.b",
        "cmpls  %p3/z %z12.b %z13.d -> %p5.b",  "cmpls  %p5/z %z18.b %z19.d -> %p8.b",
        "cmpls  %p6/z %z23.b %z24.d -> %p10.b", "cmpls  %p7/z %z31.b %z31.d -> %p15.b",
    };
    TEST_LOOP(cmpls, cmpls_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_1[6] = {
        "cmpls  %p0/z %z0.h %z0.d -> %p0.h",    "cmpls  %p2/z %z7.h %z8.d -> %p2.h",
        "cmpls  %p3/z %z12.h %z13.d -> %p5.h",  "cmpls  %p5/z %z18.h %z19.d -> %p8.h",
        "cmpls  %p6/z %z23.h %z24.d -> %p10.h", "cmpls  %p7/z %z31.h %z31.d -> %p15.h",
    };
    TEST_LOOP(cmpls, cmpls_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_2[6] = {
        "cmpls  %p0/z %z0.s %z0.d -> %p0.s",    "cmpls  %p2/z %z7.s %z8.d -> %p2.s",
        "cmpls  %p3/z %z12.s %z13.d -> %p5.s",  "cmpls  %p5/z %z18.s %z19.d -> %p8.s",
        "cmpls  %p6/z %z23.s %z24.d -> %p10.s", "cmpls  %p7/z %z31.s %z31.d -> %p15.s",
    };
    TEST_LOOP(cmpls, cmpls_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(cmplt_sve_pred_simm)
{

    /* Testing CMPLT   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, #<imm> */
    int imm5_0_0[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_0[6] = {
        "cmplt  %p0/z %z0.b $0xf0 -> %p0.b",   "cmplt  %p2/z %z7.b $0xf8 -> %p2.b",
        "cmplt  %p3/z %z12.b $0xfd -> %p5.b",  "cmplt  %p5/z %z18.b $0x03 -> %p8.b",
        "cmplt  %p6/z %z23.b $0x08 -> %p10.b", "cmplt  %p7/z %z31.b $0x0f -> %p15.b",
    };
    TEST_LOOP(cmplt, cmplt_sve_pred_simm, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_immed_int(imm5_0_0[i], OPSZ_5b));

    int imm5_0_1[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_1[6] = {
        "cmplt  %p0/z %z0.h $0xf0 -> %p0.h",   "cmplt  %p2/z %z7.h $0xf8 -> %p2.h",
        "cmplt  %p3/z %z12.h $0xfd -> %p5.h",  "cmplt  %p5/z %z18.h $0x03 -> %p8.h",
        "cmplt  %p6/z %z23.h $0x08 -> %p10.h", "cmplt  %p7/z %z31.h $0x0f -> %p15.h",
    };
    TEST_LOOP(cmplt, cmplt_sve_pred_simm, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_immed_int(imm5_0_1[i], OPSZ_5b));

    int imm5_0_2[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_2[6] = {
        "cmplt  %p0/z %z0.s $0xf0 -> %p0.s",   "cmplt  %p2/z %z7.s $0xf8 -> %p2.s",
        "cmplt  %p3/z %z12.s $0xfd -> %p5.s",  "cmplt  %p5/z %z18.s $0x03 -> %p8.s",
        "cmplt  %p6/z %z23.s $0x08 -> %p10.s", "cmplt  %p7/z %z31.s $0x0f -> %p15.s",
    };
    TEST_LOOP(cmplt, cmplt_sve_pred_simm, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_immed_int(imm5_0_2[i], OPSZ_5b));

    int imm5_0_3[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_3[6] = {
        "cmplt  %p0/z %z0.d $0xf0 -> %p0.d",   "cmplt  %p2/z %z7.d $0xf8 -> %p2.d",
        "cmplt  %p3/z %z12.d $0xfd -> %p5.d",  "cmplt  %p5/z %z18.d $0x03 -> %p8.d",
        "cmplt  %p6/z %z23.d $0x08 -> %p10.d", "cmplt  %p7/z %z31.d $0x0f -> %p15.d",
    };
    TEST_LOOP(cmplt, cmplt_sve_pred_simm, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_immed_int(imm5_0_3[i], OPSZ_5b));
}

TEST_INSTR(cmplt_sve_pred)
{

    /* Testing CMPLT   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.D */
    const char *expected_0_0[6] = {
        "cmplt  %p0/z %z0.b %z0.d -> %p0.b",    "cmplt  %p2/z %z7.b %z8.d -> %p2.b",
        "cmplt  %p3/z %z12.b %z13.d -> %p5.b",  "cmplt  %p5/z %z18.b %z19.d -> %p8.b",
        "cmplt  %p6/z %z23.b %z24.d -> %p10.b", "cmplt  %p7/z %z31.b %z31.d -> %p15.b",
    };
    TEST_LOOP(cmplt, cmplt_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_1[6] = {
        "cmplt  %p0/z %z0.h %z0.d -> %p0.h",    "cmplt  %p2/z %z7.h %z8.d -> %p2.h",
        "cmplt  %p3/z %z12.h %z13.d -> %p5.h",  "cmplt  %p5/z %z18.h %z19.d -> %p8.h",
        "cmplt  %p6/z %z23.h %z24.d -> %p10.h", "cmplt  %p7/z %z31.h %z31.d -> %p15.h",
    };
    TEST_LOOP(cmplt, cmplt_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_2[6] = {
        "cmplt  %p0/z %z0.s %z0.d -> %p0.s",    "cmplt  %p2/z %z7.s %z8.d -> %p2.s",
        "cmplt  %p3/z %z12.s %z13.d -> %p5.s",  "cmplt  %p5/z %z18.s %z19.d -> %p8.s",
        "cmplt  %p6/z %z23.s %z24.d -> %p10.s", "cmplt  %p7/z %z31.s %z31.d -> %p15.s",
    };
    TEST_LOOP(cmplt, cmplt_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(cmpne_sve_pred_simm)
{

    /* Testing CMPNE   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, #<imm> */
    int imm5_0_0[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_0[6] = {
        "cmpne  %p0/z %z0.b $0xf0 -> %p0.b",   "cmpne  %p2/z %z7.b $0xf8 -> %p2.b",
        "cmpne  %p3/z %z12.b $0xfd -> %p5.b",  "cmpne  %p5/z %z18.b $0x03 -> %p8.b",
        "cmpne  %p6/z %z23.b $0x08 -> %p10.b", "cmpne  %p7/z %z31.b $0x0f -> %p15.b",
    };
    TEST_LOOP(cmpne, cmpne_sve_pred_simm, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_immed_int(imm5_0_0[i], OPSZ_5b));

    int imm5_0_1[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_1[6] = {
        "cmpne  %p0/z %z0.h $0xf0 -> %p0.h",   "cmpne  %p2/z %z7.h $0xf8 -> %p2.h",
        "cmpne  %p3/z %z12.h $0xfd -> %p5.h",  "cmpne  %p5/z %z18.h $0x03 -> %p8.h",
        "cmpne  %p6/z %z23.h $0x08 -> %p10.h", "cmpne  %p7/z %z31.h $0x0f -> %p15.h",
    };
    TEST_LOOP(cmpne, cmpne_sve_pred_simm, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_immed_int(imm5_0_1[i], OPSZ_5b));

    int imm5_0_2[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_2[6] = {
        "cmpne  %p0/z %z0.s $0xf0 -> %p0.s",   "cmpne  %p2/z %z7.s $0xf8 -> %p2.s",
        "cmpne  %p3/z %z12.s $0xfd -> %p5.s",  "cmpne  %p5/z %z18.s $0x03 -> %p8.s",
        "cmpne  %p6/z %z23.s $0x08 -> %p10.s", "cmpne  %p7/z %z31.s $0x0f -> %p15.s",
    };
    TEST_LOOP(cmpne, cmpne_sve_pred_simm, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_immed_int(imm5_0_2[i], OPSZ_5b));

    int imm5_0_3[6] = { -16, -8, -3, 3, 8, 15 };
    const char *expected_0_3[6] = {
        "cmpne  %p0/z %z0.d $0xf0 -> %p0.d",   "cmpne  %p2/z %z7.d $0xf8 -> %p2.d",
        "cmpne  %p3/z %z12.d $0xfd -> %p5.d",  "cmpne  %p5/z %z18.d $0x03 -> %p8.d",
        "cmpne  %p6/z %z23.d $0x08 -> %p10.d", "cmpne  %p7/z %z31.d $0x0f -> %p15.d",
    };
    TEST_LOOP(cmpne, cmpne_sve_pred_simm, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_immed_int(imm5_0_3[i], OPSZ_5b));
}

TEST_INSTR(cmpne_sve_pred)
{

    /* Testing CMPNE   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.D */
    const char *expected_0_0[6] = {
        "cmpne  %p0/z %z0.b %z0.d -> %p0.b",    "cmpne  %p2/z %z7.b %z8.d -> %p2.b",
        "cmpne  %p3/z %z12.b %z13.d -> %p5.b",  "cmpne  %p5/z %z18.b %z19.d -> %p8.b",
        "cmpne  %p6/z %z23.b %z24.d -> %p10.b", "cmpne  %p7/z %z31.b %z31.d -> %p15.b",
    };
    TEST_LOOP(cmpne, cmpne_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_1[6] = {
        "cmpne  %p0/z %z0.h %z0.d -> %p0.h",    "cmpne  %p2/z %z7.h %z8.d -> %p2.h",
        "cmpne  %p3/z %z12.h %z13.d -> %p5.h",  "cmpne  %p5/z %z18.h %z19.d -> %p8.h",
        "cmpne  %p6/z %z23.h %z24.d -> %p10.h", "cmpne  %p7/z %z31.h %z31.d -> %p15.h",
    };
    TEST_LOOP(cmpne, cmpne_sve_pred, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    const char *expected_0_2[6] = {
        "cmpne  %p0/z %z0.s %z0.d -> %p0.s",    "cmpne  %p2/z %z7.s %z8.d -> %p2.s",
        "cmpne  %p3/z %z12.s %z13.d -> %p5.s",  "cmpne  %p5/z %z18.s %z19.d -> %p8.s",
        "cmpne  %p6/z %z23.s %z24.d -> %p10.s", "cmpne  %p7/z %z31.s %z31.d -> %p15.s",
    };
    TEST_LOOP(cmpne, cmpne_sve_pred, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));

    /* Testing CMPNE   <Pd>.<Ts>, <Pg>/Z, <Zn>.<Ts>, <Zm>.<Ts> */
    const char *expected_1_0[6] = {
        "cmpne  %p0/z %z0.b %z0.b -> %p0.b",    "cmpne  %p2/z %z7.b %z8.b -> %p2.b",
        "cmpne  %p3/z %z12.b %z13.b -> %p5.b",  "cmpne  %p5/z %z18.b %z19.b -> %p8.b",
        "cmpne  %p6/z %z23.b %z24.b -> %p10.b", "cmpne  %p7/z %z31.b %z31.b -> %p15.b",
    };
    TEST_LOOP(cmpne, cmpne_sve_pred, 6, expected_1_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_1));

    const char *expected_1_1[6] = {
        "cmpne  %p0/z %z0.h %z0.h -> %p0.h",    "cmpne  %p2/z %z7.h %z8.h -> %p2.h",
        "cmpne  %p3/z %z12.h %z13.h -> %p5.h",  "cmpne  %p5/z %z18.h %z19.h -> %p8.h",
        "cmpne  %p6/z %z23.h %z24.h -> %p10.h", "cmpne  %p7/z %z31.h %z31.h -> %p15.h",
    };
    TEST_LOOP(cmpne, cmpne_sve_pred, 6, expected_1_1[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_2));

    const char *expected_1_2[6] = {
        "cmpne  %p0/z %z0.s %z0.s -> %p0.s",    "cmpne  %p2/z %z7.s %z8.s -> %p2.s",
        "cmpne  %p3/z %z12.s %z13.s -> %p5.s",  "cmpne  %p5/z %z18.s %z19.s -> %p8.s",
        "cmpne  %p6/z %z23.s %z24.s -> %p10.s", "cmpne  %p7/z %z31.s %z31.s -> %p15.s",
    };
    TEST_LOOP(cmpne, cmpne_sve_pred, 6, expected_1_2[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_4));

    const char *expected_1_3[6] = {
        "cmpne  %p0/z %z0.d %z0.d -> %p0.d",    "cmpne  %p2/z %z7.d %z8.d -> %p2.d",
        "cmpne  %p3/z %z12.d %z13.d -> %p5.d",  "cmpne  %p5/z %z18.d %z19.d -> %p8.d",
        "cmpne  %p6/z %z23.d %z24.d -> %p10.d", "cmpne  %p7/z %z31.d %z31.d -> %p15.d",
    };
    TEST_LOOP(cmpne, cmpne_sve_pred, 6, expected_1_3[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], false),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_3[i], OPSZ_8));
}

TEST_INSTR(rdffr_sve)
{
    /* Testing RDFFR   <Pd>.B */
    const char *expected_0_0[6] = {
        "rdffr   -> %p0.b", "rdffr   -> %p2.b",  "rdffr   -> %p5.b",
        "rdffr   -> %p8.b", "rdffr   -> %p10.b", "rdffr   -> %p15.b",
    };
    TEST_LOOP(rdffr, rdffr_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1));
}

TEST_INSTR(rdffr_sve_pred)
{
    /* Testing RDFFR   <Pd>.B, <Pg>/Z */
    const char *expected_0_0[6] = {
        "rdffr  %p0/z -> %p0.b", "rdffr  %p3/z -> %p2.b",   "rdffr  %p6/z -> %p5.b",
        "rdffr  %p9/z -> %p8.b", "rdffr  %p11/z -> %p10.b", "rdffr  %p15/z -> %p15.b",
    };
    TEST_LOOP(rdffr, rdffr_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false));
}

TEST_INSTR(rdffrs_sve_pred)
{
    /* Testing RDFFRS  <Pd>.B, <Pg>/Z */
    const char *expected_0_0[6] = {
        "rdffrs %p0/z -> %p0.b", "rdffrs %p3/z -> %p2.b",   "rdffrs %p6/z -> %p5.b",
        "rdffrs %p9/z -> %p8.b", "rdffrs %p11/z -> %p10.b", "rdffrs %p15/z -> %p15.b",
    };
    TEST_LOOP(rdffrs, rdffrs_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false));
}

TEST_INSTR(wrffr_sve)
{

    /* Testing WRFFR   <Pn>.B */
    const char *expected_0_0[6] = {
        "wrffr  %p0.b", "wrffr  %p2.b",  "wrffr  %p5.b",
        "wrffr  %p8.b", "wrffr  %p10.b", "wrffr  %p15.b",
    };
    TEST_LOOP(wrffr, wrffr_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1));
}

TEST_INSTR(setffr_sve)
{
    /* Testing SETFFR */
    TEST_NO_OPNDS(setffr, setffr_sve, "setffr");
}

TEST_INSTR(cntp_sve_pred)
{

    /* Testing CNTP    <Xd>, <Pg>, <Pn>.<Ts> */
    const char *expected_0_0[6] = {
        "cntp   %p0 %p0.b -> %x0",    "cntp   %p3 %p4.b -> %x5",
        "cntp   %p6 %p7.b -> %x10",   "cntp   %p9 %p10.b -> %x15",
        "cntp   %p11 %p12.b -> %x20", "cntp   %p15 %p15.b -> %x30",
    };
    TEST_LOOP(cntp, cntp_sve_pred, 6, expected_0_0[i],
              opnd_create_reg(Xn_six_offset_0[i]), opnd_create_reg(Pn_six_offset_1[i]),
              opnd_create_reg_element_vector(Pn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "cntp   %p0 %p0.h -> %x0",    "cntp   %p3 %p4.h -> %x5",
        "cntp   %p6 %p7.h -> %x10",   "cntp   %p9 %p10.h -> %x15",
        "cntp   %p11 %p12.h -> %x20", "cntp   %p15 %p15.h -> %x30",
    };
    TEST_LOOP(cntp, cntp_sve_pred, 6, expected_0_1[i],
              opnd_create_reg(Xn_six_offset_0[i]), opnd_create_reg(Pn_six_offset_1[i]),
              opnd_create_reg_element_vector(Pn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "cntp   %p0 %p0.s -> %x0",    "cntp   %p3 %p4.s -> %x5",
        "cntp   %p6 %p7.s -> %x10",   "cntp   %p9 %p10.s -> %x15",
        "cntp   %p11 %p12.s -> %x20", "cntp   %p15 %p15.s -> %x30",
    };
    TEST_LOOP(cntp, cntp_sve_pred, 6, expected_0_2[i],
              opnd_create_reg(Xn_six_offset_0[i]), opnd_create_reg(Pn_six_offset_1[i]),
              opnd_create_reg_element_vector(Pn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "cntp   %p0 %p0.d -> %x0",    "cntp   %p3 %p4.d -> %x5",
        "cntp   %p6 %p7.d -> %x10",   "cntp   %p9 %p10.d -> %x15",
        "cntp   %p11 %p12.d -> %x20", "cntp   %p15 %p15.d -> %x30",
    };
    TEST_LOOP(cntp, cntp_sve_pred, 6, expected_0_3[i],
              opnd_create_reg(Xn_six_offset_0[i]), opnd_create_reg(Pn_six_offset_1[i]),
              opnd_create_reg_element_vector(Pn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(decp_sve)
{

    /* Testing DECP    <Xdn>, <Pm>.<Ts> */
    const char *expected_0_0[6] = {
        "decp   %p0.b %x0 -> %x0",    "decp   %p3.b %x5 -> %x5",
        "decp   %p6.b %x10 -> %x10",  "decp   %p9.b %x15 -> %x15",
        "decp   %p11.b %x20 -> %x20", "decp   %p15.b %x30 -> %x30",
    };
    TEST_LOOP(decp, decp_sve, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "decp   %p0.h %x0 -> %x0",    "decp   %p3.h %x5 -> %x5",
        "decp   %p6.h %x10 -> %x10",  "decp   %p9.h %x15 -> %x15",
        "decp   %p11.h %x20 -> %x20", "decp   %p15.h %x30 -> %x30",
    };
    TEST_LOOP(decp, decp_sve, 6, expected_0_1[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "decp   %p0.s %x0 -> %x0",    "decp   %p3.s %x5 -> %x5",
        "decp   %p6.s %x10 -> %x10",  "decp   %p9.s %x15 -> %x15",
        "decp   %p11.s %x20 -> %x20", "decp   %p15.s %x30 -> %x30",
    };
    TEST_LOOP(decp, decp_sve, 6, expected_0_2[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "decp   %p0.d %x0 -> %x0",    "decp   %p3.d %x5 -> %x5",
        "decp   %p6.d %x10 -> %x10",  "decp   %p9.d %x15 -> %x15",
        "decp   %p11.d %x20 -> %x20", "decp   %p15.d %x30 -> %x30",
    };
    TEST_LOOP(decp, decp_sve, 6, expected_0_3[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(decp_sve_vector)
{

    /* Testing DECP    <Zdn>.<Ts>, <Pm>.<Ts> */
    const char *expected_0_0[6] = {
        "decp   %p0.h %z0.h -> %z0.h",    "decp   %p3.h %z5.h -> %z5.h",
        "decp   %p6.h %z10.h -> %z10.h",  "decp   %p9.h %z16.h -> %z16.h",
        "decp   %p11.h %z21.h -> %z21.h", "decp   %p15.h %z31.h -> %z31.h",
    };
    TEST_LOOP(decp, decp_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "decp   %p0.s %z0.s -> %z0.s",    "decp   %p3.s %z5.s -> %z5.s",
        "decp   %p6.s %z10.s -> %z10.s",  "decp   %p9.s %z16.s -> %z16.s",
        "decp   %p11.s %z21.s -> %z21.s", "decp   %p15.s %z31.s -> %z31.s",
    };
    TEST_LOOP(decp, decp_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "decp   %p0.d %z0.d -> %z0.d",    "decp   %p3.d %z5.d -> %z5.d",
        "decp   %p6.d %z10.d -> %z10.d",  "decp   %p9.d %z16.d -> %z16.d",
        "decp   %p11.d %z21.d -> %z21.d", "decp   %p15.d %z31.d -> %z31.d",
    };
    TEST_LOOP(decp, decp_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(incp_sve)
{

    /* Testing INCP    <Xdn>, <Pm>.<Ts> */
    const char *expected_0_0[6] = {
        "incp   %p0.b %x0 -> %x0",    "incp   %p3.b %x5 -> %x5",
        "incp   %p6.b %x10 -> %x10",  "incp   %p9.b %x15 -> %x15",
        "incp   %p11.b %x20 -> %x20", "incp   %p15.b %x30 -> %x30",
    };
    TEST_LOOP(incp, incp_sve, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "incp   %p0.h %x0 -> %x0",    "incp   %p3.h %x5 -> %x5",
        "incp   %p6.h %x10 -> %x10",  "incp   %p9.h %x15 -> %x15",
        "incp   %p11.h %x20 -> %x20", "incp   %p15.h %x30 -> %x30",
    };
    TEST_LOOP(incp, incp_sve, 6, expected_0_1[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "incp   %p0.s %x0 -> %x0",    "incp   %p3.s %x5 -> %x5",
        "incp   %p6.s %x10 -> %x10",  "incp   %p9.s %x15 -> %x15",
        "incp   %p11.s %x20 -> %x20", "incp   %p15.s %x30 -> %x30",
    };
    TEST_LOOP(incp, incp_sve, 6, expected_0_2[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "incp   %p0.d %x0 -> %x0",    "incp   %p3.d %x5 -> %x5",
        "incp   %p6.d %x10 -> %x10",  "incp   %p9.d %x15 -> %x15",
        "incp   %p11.d %x20 -> %x20", "incp   %p15.d %x30 -> %x30",
    };
    TEST_LOOP(incp, incp_sve, 6, expected_0_3[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(incp_sve_vector)
{

    /* Testing INCP    <Zdn>.<Ts>, <Pm>.<Ts> */
    const char *expected_0_0[6] = {
        "incp   %p0.h %z0.h -> %z0.h",    "incp   %p3.h %z5.h -> %z5.h",
        "incp   %p6.h %z10.h -> %z10.h",  "incp   %p9.h %z16.h -> %z16.h",
        "incp   %p11.h %z21.h -> %z21.h", "incp   %p15.h %z31.h -> %z31.h",
    };
    TEST_LOOP(incp, incp_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "incp   %p0.s %z0.s -> %z0.s",    "incp   %p3.s %z5.s -> %z5.s",
        "incp   %p6.s %z10.s -> %z10.s",  "incp   %p9.s %z16.s -> %z16.s",
        "incp   %p11.s %z21.s -> %z21.s", "incp   %p15.s %z31.s -> %z31.s",
    };
    TEST_LOOP(incp, incp_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "incp   %p0.d %z0.d -> %z0.d",    "incp   %p3.d %z5.d -> %z5.d",
        "incp   %p6.d %z10.d -> %z10.d",  "incp   %p9.d %z16.d -> %z16.d",
        "incp   %p11.d %z21.d -> %z21.d", "incp   %p15.d %z31.d -> %z31.d",
    };
    TEST_LOOP(incp, incp_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(sqdecp_sve)
{

    /* Testing SQDECP  <Xdn>, <Pm>.<Ts>, <Wdn> */
    const char *expected_0_0[6] = {
        "sqdecp %p0.b %w0 -> %x0",    "sqdecp %p3.b %w5 -> %x5",
        "sqdecp %p6.b %w10 -> %x10",  "sqdecp %p9.b %w15 -> %x15",
        "sqdecp %p11.b %w20 -> %x20", "sqdecp %p15.b %w30 -> %x30",
    };
    TEST_LOOP(sqdecp, sqdecp_sve_wide, 6, expected_0_0[i],
              opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "sqdecp %p0.h %w0 -> %x0",    "sqdecp %p3.h %w5 -> %x5",
        "sqdecp %p6.h %w10 -> %x10",  "sqdecp %p9.h %w15 -> %x15",
        "sqdecp %p11.h %w20 -> %x20", "sqdecp %p15.h %w30 -> %x30",
    };
    TEST_LOOP(sqdecp, sqdecp_sve_wide, 6, expected_0_1[i],
              opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "sqdecp %p0.s %w0 -> %x0",    "sqdecp %p3.s %w5 -> %x5",
        "sqdecp %p6.s %w10 -> %x10",  "sqdecp %p9.s %w15 -> %x15",
        "sqdecp %p11.s %w20 -> %x20", "sqdecp %p15.s %w30 -> %x30",
    };
    TEST_LOOP(sqdecp, sqdecp_sve_wide, 6, expected_0_2[i],
              opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "sqdecp %p0.d %w0 -> %x0",    "sqdecp %p3.d %w5 -> %x5",
        "sqdecp %p6.d %w10 -> %x10",  "sqdecp %p9.d %w15 -> %x15",
        "sqdecp %p11.d %w20 -> %x20", "sqdecp %p15.d %w30 -> %x30",
    };
    TEST_LOOP(sqdecp, sqdecp_sve_wide, 6, expected_0_3[i],
              opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_8));

    /* Testing SQDECP  <Xdn>, <Pm>.<Ts> */
    const char *expected_1_0[6] = {
        "sqdecp %p0.b %x0 -> %x0",    "sqdecp %p3.b %x5 -> %x5",
        "sqdecp %p6.b %x10 -> %x10",  "sqdecp %p9.b %x15 -> %x15",
        "sqdecp %p11.b %x20 -> %x20", "sqdecp %p15.b %x30 -> %x30",
    };
    TEST_LOOP(sqdecp, sqdecp_sve, 6, expected_1_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_1));

    const char *expected_1_1[6] = {
        "sqdecp %p0.h %x0 -> %x0",    "sqdecp %p3.h %x5 -> %x5",
        "sqdecp %p6.h %x10 -> %x10",  "sqdecp %p9.h %x15 -> %x15",
        "sqdecp %p11.h %x20 -> %x20", "sqdecp %p15.h %x30 -> %x30",
    };
    TEST_LOOP(sqdecp, sqdecp_sve, 6, expected_1_1[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_2));

    const char *expected_1_2[6] = {
        "sqdecp %p0.s %x0 -> %x0",    "sqdecp %p3.s %x5 -> %x5",
        "sqdecp %p6.s %x10 -> %x10",  "sqdecp %p9.s %x15 -> %x15",
        "sqdecp %p11.s %x20 -> %x20", "sqdecp %p15.s %x30 -> %x30",
    };
    TEST_LOOP(sqdecp, sqdecp_sve, 6, expected_1_2[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_4));

    const char *expected_1_3[6] = {
        "sqdecp %p0.d %x0 -> %x0",    "sqdecp %p3.d %x5 -> %x5",
        "sqdecp %p6.d %x10 -> %x10",  "sqdecp %p9.d %x15 -> %x15",
        "sqdecp %p11.d %x20 -> %x20", "sqdecp %p15.d %x30 -> %x30",
    };
    TEST_LOOP(sqdecp, sqdecp_sve, 6, expected_1_3[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(sqdecp_sve_vector)
{

    /* Testing SQDECP  <Zdn>.<Ts>, <Pm>.<Ts> */
    const char *expected_0_0[6] = {
        "sqdecp %p0.h %z0.h -> %z0.h",    "sqdecp %p3.h %z5.h -> %z5.h",
        "sqdecp %p6.h %z10.h -> %z10.h",  "sqdecp %p9.h %z16.h -> %z16.h",
        "sqdecp %p11.h %z21.h -> %z21.h", "sqdecp %p15.h %z31.h -> %z31.h",
    };
    TEST_LOOP(sqdecp, sqdecp_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "sqdecp %p0.s %z0.s -> %z0.s",    "sqdecp %p3.s %z5.s -> %z5.s",
        "sqdecp %p6.s %z10.s -> %z10.s",  "sqdecp %p9.s %z16.s -> %z16.s",
        "sqdecp %p11.s %z21.s -> %z21.s", "sqdecp %p15.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sqdecp, sqdecp_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "sqdecp %p0.d %z0.d -> %z0.d",    "sqdecp %p3.d %z5.d -> %z5.d",
        "sqdecp %p6.d %z10.d -> %z10.d",  "sqdecp %p9.d %z16.d -> %z16.d",
        "sqdecp %p11.d %z21.d -> %z21.d", "sqdecp %p15.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sqdecp, sqdecp_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(sqincp_sve)
{

    /* Testing SQINCP  <Xdn>, <Pm>.<Ts>, <Wdn> */
    const char *expected_0_0[6] = {
        "sqincp %p0.b %w0 -> %x0",    "sqincp %p3.b %w5 -> %x5",
        "sqincp %p6.b %w10 -> %x10",  "sqincp %p9.b %w15 -> %x15",
        "sqincp %p11.b %w20 -> %x20", "sqincp %p15.b %w30 -> %x30",
    };
    TEST_LOOP(sqincp, sqincp_sve_wide, 6, expected_0_0[i],
              opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "sqincp %p0.h %w0 -> %x0",    "sqincp %p3.h %w5 -> %x5",
        "sqincp %p6.h %w10 -> %x10",  "sqincp %p9.h %w15 -> %x15",
        "sqincp %p11.h %w20 -> %x20", "sqincp %p15.h %w30 -> %x30",
    };
    TEST_LOOP(sqincp, sqincp_sve_wide, 6, expected_0_1[i],
              opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "sqincp %p0.s %w0 -> %x0",    "sqincp %p3.s %w5 -> %x5",
        "sqincp %p6.s %w10 -> %x10",  "sqincp %p9.s %w15 -> %x15",
        "sqincp %p11.s %w20 -> %x20", "sqincp %p15.s %w30 -> %x30",
    };
    TEST_LOOP(sqincp, sqincp_sve_wide, 6, expected_0_2[i],
              opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "sqincp %p0.d %w0 -> %x0",    "sqincp %p3.d %w5 -> %x5",
        "sqincp %p6.d %w10 -> %x10",  "sqincp %p9.d %w15 -> %x15",
        "sqincp %p11.d %w20 -> %x20", "sqincp %p15.d %w30 -> %x30",
    };
    TEST_LOOP(sqincp, sqincp_sve_wide, 6, expected_0_3[i],
              opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_8));

    /* Testing SQINCP  <Xdn>, <Pm>.<Ts> */
    const char *expected_1_0[6] = {
        "sqincp %p0.b %x0 -> %x0",    "sqincp %p3.b %x5 -> %x5",
        "sqincp %p6.b %x10 -> %x10",  "sqincp %p9.b %x15 -> %x15",
        "sqincp %p11.b %x20 -> %x20", "sqincp %p15.b %x30 -> %x30",
    };
    TEST_LOOP(sqincp, sqincp_sve, 6, expected_1_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_1));

    const char *expected_1_1[6] = {
        "sqincp %p0.h %x0 -> %x0",    "sqincp %p3.h %x5 -> %x5",
        "sqincp %p6.h %x10 -> %x10",  "sqincp %p9.h %x15 -> %x15",
        "sqincp %p11.h %x20 -> %x20", "sqincp %p15.h %x30 -> %x30",
    };
    TEST_LOOP(sqincp, sqincp_sve, 6, expected_1_1[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_2));

    const char *expected_1_2[6] = {
        "sqincp %p0.s %x0 -> %x0",    "sqincp %p3.s %x5 -> %x5",
        "sqincp %p6.s %x10 -> %x10",  "sqincp %p9.s %x15 -> %x15",
        "sqincp %p11.s %x20 -> %x20", "sqincp %p15.s %x30 -> %x30",
    };
    TEST_LOOP(sqincp, sqincp_sve, 6, expected_1_2[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_4));

    const char *expected_1_3[6] = {
        "sqincp %p0.d %x0 -> %x0",    "sqincp %p3.d %x5 -> %x5",
        "sqincp %p6.d %x10 -> %x10",  "sqincp %p9.d %x15 -> %x15",
        "sqincp %p11.d %x20 -> %x20", "sqincp %p15.d %x30 -> %x30",
    };
    TEST_LOOP(sqincp, sqincp_sve, 6, expected_1_3[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(sqincp_sve_vector)
{

    /* Testing SQINCP  <Zdn>.<Ts>, <Pm>.<Ts> */
    const char *expected_0_0[6] = {
        "sqincp %p0.h %z0.h -> %z0.h",    "sqincp %p3.h %z5.h -> %z5.h",
        "sqincp %p6.h %z10.h -> %z10.h",  "sqincp %p9.h %z16.h -> %z16.h",
        "sqincp %p11.h %z21.h -> %z21.h", "sqincp %p15.h %z31.h -> %z31.h",
    };
    TEST_LOOP(sqincp, sqincp_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "sqincp %p0.s %z0.s -> %z0.s",    "sqincp %p3.s %z5.s -> %z5.s",
        "sqincp %p6.s %z10.s -> %z10.s",  "sqincp %p9.s %z16.s -> %z16.s",
        "sqincp %p11.s %z21.s -> %z21.s", "sqincp %p15.s %z31.s -> %z31.s",
    };
    TEST_LOOP(sqincp, sqincp_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "sqincp %p0.d %z0.d -> %z0.d",    "sqincp %p3.d %z5.d -> %z5.d",
        "sqincp %p6.d %z10.d -> %z10.d",  "sqincp %p9.d %z16.d -> %z16.d",
        "sqincp %p11.d %z21.d -> %z21.d", "sqincp %p15.d %z31.d -> %z31.d",
    };
    TEST_LOOP(sqincp, sqincp_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(uqdecp_sve)
{

    /* Testing UQDECP  <Wdn>, <Pm>.<Ts> */
    const char *expected_0_0[6] = {
        "uqdecp %p0.b %w0 -> %w0",    "uqdecp %p3.b %w5 -> %w5",
        "uqdecp %p6.b %w10 -> %w10",  "uqdecp %p9.b %w15 -> %w15",
        "uqdecp %p11.b %w20 -> %w20", "uqdecp %p15.b %w30 -> %w30",
    };
    TEST_LOOP(uqdecp, uqdecp_sve, 6, expected_0_0[i], opnd_create_reg(Wn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "uqdecp %p0.h %w0 -> %w0",    "uqdecp %p3.h %w5 -> %w5",
        "uqdecp %p6.h %w10 -> %w10",  "uqdecp %p9.h %w15 -> %w15",
        "uqdecp %p11.h %w20 -> %w20", "uqdecp %p15.h %w30 -> %w30",
    };
    TEST_LOOP(uqdecp, uqdecp_sve, 6, expected_0_1[i], opnd_create_reg(Wn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "uqdecp %p0.s %w0 -> %w0",    "uqdecp %p3.s %w5 -> %w5",
        "uqdecp %p6.s %w10 -> %w10",  "uqdecp %p9.s %w15 -> %w15",
        "uqdecp %p11.s %w20 -> %w20", "uqdecp %p15.s %w30 -> %w30",
    };
    TEST_LOOP(uqdecp, uqdecp_sve, 6, expected_0_2[i], opnd_create_reg(Wn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "uqdecp %p0.d %w0 -> %w0",    "uqdecp %p3.d %w5 -> %w5",
        "uqdecp %p6.d %w10 -> %w10",  "uqdecp %p9.d %w15 -> %w15",
        "uqdecp %p11.d %w20 -> %w20", "uqdecp %p15.d %w30 -> %w30",
    };
    TEST_LOOP(uqdecp, uqdecp_sve, 6, expected_0_3[i], opnd_create_reg(Wn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_8));

    /* Testing UQDECP  <Xdn>, <Pm>.<Ts> */
    const char *expected_1_0[6] = {
        "uqdecp %p0.b %x0 -> %x0",    "uqdecp %p3.b %x5 -> %x5",
        "uqdecp %p6.b %x10 -> %x10",  "uqdecp %p9.b %x15 -> %x15",
        "uqdecp %p11.b %x20 -> %x20", "uqdecp %p15.b %x30 -> %x30",
    };
    TEST_LOOP(uqdecp, uqdecp_sve, 6, expected_1_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_1));

    const char *expected_1_1[6] = {
        "uqdecp %p0.h %x0 -> %x0",    "uqdecp %p3.h %x5 -> %x5",
        "uqdecp %p6.h %x10 -> %x10",  "uqdecp %p9.h %x15 -> %x15",
        "uqdecp %p11.h %x20 -> %x20", "uqdecp %p15.h %x30 -> %x30",
    };
    TEST_LOOP(uqdecp, uqdecp_sve, 6, expected_1_1[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_2));

    const char *expected_1_2[6] = {
        "uqdecp %p0.s %x0 -> %x0",    "uqdecp %p3.s %x5 -> %x5",
        "uqdecp %p6.s %x10 -> %x10",  "uqdecp %p9.s %x15 -> %x15",
        "uqdecp %p11.s %x20 -> %x20", "uqdecp %p15.s %x30 -> %x30",
    };
    TEST_LOOP(uqdecp, uqdecp_sve, 6, expected_1_2[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_4));

    const char *expected_1_3[6] = {
        "uqdecp %p0.d %x0 -> %x0",    "uqdecp %p3.d %x5 -> %x5",
        "uqdecp %p6.d %x10 -> %x10",  "uqdecp %p9.d %x15 -> %x15",
        "uqdecp %p11.d %x20 -> %x20", "uqdecp %p15.d %x30 -> %x30",
    };
    TEST_LOOP(uqdecp, uqdecp_sve, 6, expected_1_3[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(uqdecp_sve_vector)
{

    /* Testing UQDECP  <Zdn>.<Ts>, <Pm>.<Ts> */
    const char *expected_0_0[6] = {
        "uqdecp %p0.h %z0.h -> %z0.h",    "uqdecp %p3.h %z5.h -> %z5.h",
        "uqdecp %p6.h %z10.h -> %z10.h",  "uqdecp %p9.h %z16.h -> %z16.h",
        "uqdecp %p11.h %z21.h -> %z21.h", "uqdecp %p15.h %z31.h -> %z31.h",
    };
    TEST_LOOP(uqdecp, uqdecp_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "uqdecp %p0.s %z0.s -> %z0.s",    "uqdecp %p3.s %z5.s -> %z5.s",
        "uqdecp %p6.s %z10.s -> %z10.s",  "uqdecp %p9.s %z16.s -> %z16.s",
        "uqdecp %p11.s %z21.s -> %z21.s", "uqdecp %p15.s %z31.s -> %z31.s",
    };
    TEST_LOOP(uqdecp, uqdecp_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "uqdecp %p0.d %z0.d -> %z0.d",    "uqdecp %p3.d %z5.d -> %z5.d",
        "uqdecp %p6.d %z10.d -> %z10.d",  "uqdecp %p9.d %z16.d -> %z16.d",
        "uqdecp %p11.d %z21.d -> %z21.d", "uqdecp %p15.d %z31.d -> %z31.d",
    };
    TEST_LOOP(uqdecp, uqdecp_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(uqincp_sve)
{

    /* Testing UQINCP  <Wdn>, <Pm>.<Ts> */
    const char *expected_0_0[6] = {
        "uqincp %p0.b %w0 -> %w0",    "uqincp %p3.b %w5 -> %w5",
        "uqincp %p6.b %w10 -> %w10",  "uqincp %p9.b %w15 -> %w15",
        "uqincp %p11.b %w20 -> %w20", "uqincp %p15.b %w30 -> %w30",
    };
    TEST_LOOP(uqincp, uqincp_sve, 6, expected_0_0[i], opnd_create_reg(Wn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "uqincp %p0.h %w0 -> %w0",    "uqincp %p3.h %w5 -> %w5",
        "uqincp %p6.h %w10 -> %w10",  "uqincp %p9.h %w15 -> %w15",
        "uqincp %p11.h %w20 -> %w20", "uqincp %p15.h %w30 -> %w30",
    };
    TEST_LOOP(uqincp, uqincp_sve, 6, expected_0_1[i], opnd_create_reg(Wn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "uqincp %p0.s %w0 -> %w0",    "uqincp %p3.s %w5 -> %w5",
        "uqincp %p6.s %w10 -> %w10",  "uqincp %p9.s %w15 -> %w15",
        "uqincp %p11.s %w20 -> %w20", "uqincp %p15.s %w30 -> %w30",
    };
    TEST_LOOP(uqincp, uqincp_sve, 6, expected_0_2[i], opnd_create_reg(Wn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "uqincp %p0.d %w0 -> %w0",    "uqincp %p3.d %w5 -> %w5",
        "uqincp %p6.d %w10 -> %w10",  "uqincp %p9.d %w15 -> %w15",
        "uqincp %p11.d %w20 -> %w20", "uqincp %p15.d %w30 -> %w30",
    };
    TEST_LOOP(uqincp, uqincp_sve, 6, expected_0_3[i], opnd_create_reg(Wn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_8));

    /* Testing UQINCP  <Xdn>, <Pm>.<Ts> */
    const char *expected_1_0[6] = {
        "uqincp %p0.b %x0 -> %x0",    "uqincp %p3.b %x5 -> %x5",
        "uqincp %p6.b %x10 -> %x10",  "uqincp %p9.b %x15 -> %x15",
        "uqincp %p11.b %x20 -> %x20", "uqincp %p15.b %x30 -> %x30",
    };
    TEST_LOOP(uqincp, uqincp_sve, 6, expected_1_0[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_1));

    const char *expected_1_1[6] = {
        "uqincp %p0.h %x0 -> %x0",    "uqincp %p3.h %x5 -> %x5",
        "uqincp %p6.h %x10 -> %x10",  "uqincp %p9.h %x15 -> %x15",
        "uqincp %p11.h %x20 -> %x20", "uqincp %p15.h %x30 -> %x30",
    };
    TEST_LOOP(uqincp, uqincp_sve, 6, expected_1_1[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_2));

    const char *expected_1_2[6] = {
        "uqincp %p0.s %x0 -> %x0",    "uqincp %p3.s %x5 -> %x5",
        "uqincp %p6.s %x10 -> %x10",  "uqincp %p9.s %x15 -> %x15",
        "uqincp %p11.s %x20 -> %x20", "uqincp %p15.s %x30 -> %x30",
    };
    TEST_LOOP(uqincp, uqincp_sve, 6, expected_1_2[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_4));

    const char *expected_1_3[6] = {
        "uqincp %p0.d %x0 -> %x0",    "uqincp %p3.d %x5 -> %x5",
        "uqincp %p6.d %x10 -> %x10",  "uqincp %p9.d %x15 -> %x15",
        "uqincp %p11.d %x20 -> %x20", "uqincp %p15.d %x30 -> %x30",
    };
    TEST_LOOP(uqincp, uqincp_sve, 6, expected_1_3[i], opnd_create_reg(Xn_six_offset_0[i]),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(uqincp_sve_vector)
{

    /* Testing UQINCP  <Zdn>.<Ts>, <Pm>.<Ts> */
    const char *expected_0_0[6] = {
        "uqincp %p0.h %z0.h -> %z0.h",    "uqincp %p3.h %z5.h -> %z5.h",
        "uqincp %p6.h %z10.h -> %z10.h",  "uqincp %p9.h %z16.h -> %z16.h",
        "uqincp %p11.h %z21.h -> %z21.h", "uqincp %p15.h %z31.h -> %z31.h",
    };
    TEST_LOOP(uqincp, uqincp_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_2));

    const char *expected_0_1[6] = {
        "uqincp %p0.s %z0.s -> %z0.s",    "uqincp %p3.s %z5.s -> %z5.s",
        "uqincp %p6.s %z10.s -> %z10.s",  "uqincp %p9.s %z16.s -> %z16.s",
        "uqincp %p11.s %z21.s -> %z21.s", "uqincp %p15.s %z31.s -> %z31.s",
    };
    TEST_LOOP(uqincp, uqincp_sve, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_4));

    const char *expected_0_2[6] = {
        "uqincp %p0.d %z0.d -> %z0.d",    "uqincp %p3.d %z5.d -> %z5.d",
        "uqincp %p6.d %z10.d -> %z10.d",  "uqincp %p9.d %z16.d -> %z16.d",
        "uqincp %p11.d %z21.d -> %z21.d", "uqincp %p15.d %z31.d -> %z31.d",
    };
    TEST_LOOP(uqincp, uqincp_sve, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Pn_six_offset_1[i], OPSZ_8));
}

TEST_INSTR(and_sve_imm)
{
    /* Testing AND     <Zdn>.<T>, <Zdn>.<T>, #<imm> */
    int imm13[6] = { 1, 4, 8, 16, 32, 63 };
    const char *expected_0[6] = {
        "and    %z0.b $0x01 -> %z0.b",   "and    %z5.b $0x04 -> %z5.b",
        "and    %z10.b $0x08 -> %z10.b", "and    %z16.b $0x10 -> %z16.b",
        "and    %z21.b $0x20 -> %z21.b", "and    %z31.b $0x3f -> %z31.b",
    };
    TEST_LOOP(and, and_sve_imm, 6, expected_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_immed_int(imm13[i], OPSZ_1));

    const char *expected_1[6] = {
        "and    %z0.h $0x0001 -> %z0.h",   "and    %z5.h $0x0004 -> %z5.h",
        "and    %z10.h $0x0008 -> %z10.h", "and    %z16.h $0x0010 -> %z16.h",
        "and    %z21.h $0x0020 -> %z21.h", "and    %z31.h $0x003f -> %z31.h",
    };
    TEST_LOOP(and, and_sve_imm, 6, expected_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_immed_int(imm13[i], OPSZ_2));

    const char *expected_2[6] = {
        "and    %z0.s $0x00000001 -> %z0.s",   "and    %z5.s $0x00000004 -> %z5.s",
        "and    %z10.s $0x00000008 -> %z10.s", "and    %z16.s $0x00000010 -> %z16.s",
        "and    %z21.s $0x00000020 -> %z21.s", "and    %z31.s $0x0000003f -> %z31.s",
    };
    TEST_LOOP(and, and_sve_imm, 6, expected_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_immed_int(imm13[i], OPSZ_4));

    const char *expected_3[6] = {
        "and    %z0.d $0x0000000000000001 -> %z0.d",
        "and    %z5.d $0x0000000000000004 -> %z5.d",
        "and    %z10.d $0x0000000000000008 -> %z10.d",
        "and    %z16.d $0x0000000000000010 -> %z16.d",
        "and    %z21.d $0x0000000000000020 -> %z21.d",
        "and    %z31.d $0x000000000000003f -> %z31.d",
    };
    TEST_LOOP(and, and_sve_imm, 6, expected_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_immed_int(imm13[i], OPSZ_8));
}

TEST_INSTR(bic_sve_imm)
{
    /* Testing BIC     <Zdn>.<T>, <Zdn>.<T>, #<imm> */
    int imm13[6] = { 1, 4, 8, 16, 32, 63 };
    const char *expected_0[6] = {
        "and    %z0.b $0xfe -> %z0.b",   "and    %z5.b $0xfb -> %z5.b",
        "and    %z10.b $0xf7 -> %z10.b", "and    %z16.b $0xef -> %z16.b",
        "and    %z21.b $0xdf -> %z21.b", "and    %z31.b $0xc0 -> %z31.b",
    };
    TEST_LOOP(and, bic_sve_imm, 6, expected_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_immed_int(imm13[i], OPSZ_1));

    const char *expected_1[6] = {
        "and    %z0.h $0xfffe -> %z0.h",   "and    %z5.h $0xfffb -> %z5.h",
        "and    %z10.h $0xfff7 -> %z10.h", "and    %z16.h $0xffef -> %z16.h",
        "and    %z21.h $0xffdf -> %z21.h", "and    %z31.h $0xffc0 -> %z31.h",
    };
    TEST_LOOP(and, bic_sve_imm, 6, expected_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_immed_int(imm13[i], OPSZ_2));

    const char *expected_2[6] = {
        "and    %z0.s $0xfffffffe -> %z0.s",   "and    %z5.s $0xfffffffb -> %z5.s",
        "and    %z10.s $0xfffffff7 -> %z10.s", "and    %z16.s $0xffffffef -> %z16.s",
        "and    %z21.s $0xffffffdf -> %z21.s", "and    %z31.s $0xffffffc0 -> %z31.s",
    };
    TEST_LOOP(and, bic_sve_imm, 6, expected_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_immed_int(imm13[i], OPSZ_4));

    const char *expected_3[6] = {
        "and    %z0.d $0xfffffffffffffffe -> %z0.d",
        "and    %z5.d $0xfffffffffffffffb -> %z5.d",
        "and    %z10.d $0xfffffffffffffff7 -> %z10.d",
        "and    %z16.d $0xffffffffffffffef -> %z16.d",
        "and    %z21.d $0xffffffffffffffdf -> %z21.d",
        "and    %z31.d $0xffffffffffffffc0 -> %z31.d",
    };
    TEST_LOOP(and, bic_sve_imm, 6, expected_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_immed_int(imm13[i], OPSZ_8));
}

TEST_INSTR(eor_sve_imm)
{
    /* Testing EOR     <Zdn>.<T>, <Zdn>.<T>, #<imm> */
    int imm13[6] = { 1, 4, 8, 16, 32, 63 };
    const char *expected_0[6] = {
        "eor    %z0.b $0x01 -> %z0.b",   "eor    %z5.b $0x04 -> %z5.b",
        "eor    %z10.b $0x08 -> %z10.b", "eor    %z16.b $0x10 -> %z16.b",
        "eor    %z21.b $0x20 -> %z21.b", "eor    %z31.b $0x3f -> %z31.b",
    };
    TEST_LOOP(eor, eor_sve_imm, 6, expected_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_immed_int(imm13[i], OPSZ_1));

    const char *expected_1[6] = {
        "eor    %z0.h $0x0001 -> %z0.h",   "eor    %z5.h $0x0004 -> %z5.h",
        "eor    %z10.h $0x0008 -> %z10.h", "eor    %z16.h $0x0010 -> %z16.h",
        "eor    %z21.h $0x0020 -> %z21.h", "eor    %z31.h $0x003f -> %z31.h",
    };
    TEST_LOOP(eor, eor_sve_imm, 6, expected_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_immed_int(imm13[i], OPSZ_2));

    const char *expected_2[6] = {
        "eor    %z0.s $0x00000001 -> %z0.s",   "eor    %z5.s $0x00000004 -> %z5.s",
        "eor    %z10.s $0x00000008 -> %z10.s", "eor    %z16.s $0x00000010 -> %z16.s",
        "eor    %z21.s $0x00000020 -> %z21.s", "eor    %z31.s $0x0000003f -> %z31.s",
    };
    TEST_LOOP(eor, eor_sve_imm, 6, expected_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_immed_int(imm13[i], OPSZ_4));

    const char *expected_3[6] = {
        "eor    %z0.d $0x0000000000000001 -> %z0.d",
        "eor    %z5.d $0x0000000000000004 -> %z5.d",
        "eor    %z10.d $0x0000000000000008 -> %z10.d",
        "eor    %z16.d $0x0000000000000010 -> %z16.d",
        "eor    %z21.d $0x0000000000000020 -> %z21.d",
        "eor    %z31.d $0x000000000000003f -> %z31.d",
    };
    TEST_LOOP(eor, eor_sve_imm, 6, expected_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_immed_int(imm13[i], OPSZ_8));
}

TEST_INSTR(orr_sve_imm)
{
    /* Testing ORR     <Zdn>.<T>, <Zdn>.<T>, #<imm> */
    int imm13[6] = { 1, 4, 8, 16, 32, 63 };
    const char *expected_0[6] = {
        "orr    %z0.b $0x01 -> %z0.b",   "orr    %z5.b $0x04 -> %z5.b",
        "orr    %z10.b $0x08 -> %z10.b", "orr    %z16.b $0x10 -> %z16.b",
        "orr    %z21.b $0x20 -> %z21.b", "orr    %z31.b $0x3f -> %z31.b",
    };
    TEST_LOOP(orr, orr_sve_imm, 6, expected_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_immed_int(imm13[i], OPSZ_1));

    const char *expected_1[6] = {
        "orr    %z0.h $0x0001 -> %z0.h",   "orr    %z5.h $0x0004 -> %z5.h",
        "orr    %z10.h $0x0008 -> %z10.h", "orr    %z16.h $0x0010 -> %z16.h",
        "orr    %z21.h $0x0020 -> %z21.h", "orr    %z31.h $0x003f -> %z31.h",
    };
    TEST_LOOP(orr, orr_sve_imm, 6, expected_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_immed_int(imm13[i], OPSZ_2));

    const char *expected_2[6] = {
        "orr    %z0.s $0x00000001 -> %z0.s",   "orr    %z5.s $0x00000004 -> %z5.s",
        "orr    %z10.s $0x00000008 -> %z10.s", "orr    %z16.s $0x00000010 -> %z16.s",
        "orr    %z21.s $0x00000020 -> %z21.s", "orr    %z31.s $0x0000003f -> %z31.s",
    };
    TEST_LOOP(orr, orr_sve_imm, 6, expected_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_immed_int(imm13[i], OPSZ_4));

    const char *expected_3[6] = {
        "orr    %z0.d $0x0000000000000001 -> %z0.d",
        "orr    %z5.d $0x0000000000000004 -> %z5.d",
        "orr    %z10.d $0x0000000000000008 -> %z10.d",
        "orr    %z16.d $0x0000000000000010 -> %z16.d",
        "orr    %z21.d $0x0000000000000020 -> %z21.d",
        "orr    %z31.d $0x000000000000003f -> %z31.d",
    };
    TEST_LOOP(orr, orr_sve_imm, 6, expected_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_immed_int(imm13[i], OPSZ_8));
}

TEST_INSTR(orn_sve_imm)
{
    /* Testing ORN     <Zdn>.<T>, <Zdn>.<T>, #<imm> */
    int imm13[6] = { 1, 4, 8, 16, 32, 63 };
    const char *expected_0[6] = {
        "orr    %z0.b $0xfe -> %z0.b",   "orr    %z5.b $0xfb -> %z5.b",
        "orr    %z10.b $0xf7 -> %z10.b", "orr    %z16.b $0xef -> %z16.b",
        "orr    %z21.b $0xdf -> %z21.b", "orr    %z31.b $0xc0 -> %z31.b",
    };
    TEST_LOOP(orr, orn_sve_imm, 6, expected_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_immed_int(imm13[i], OPSZ_1));

    const char *expected_1[6] = {
        "orr    %z0.h $0xfffe -> %z0.h",   "orr    %z5.h $0xfffb -> %z5.h",
        "orr    %z10.h $0xfff7 -> %z10.h", "orr    %z16.h $0xffef -> %z16.h",
        "orr    %z21.h $0xffdf -> %z21.h", "orr    %z31.h $0xffc0 -> %z31.h",
    };
    TEST_LOOP(orr, orn_sve_imm, 6, expected_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_immed_int(imm13[i], OPSZ_2));

    const char *expected_2[6] = {
        "orr    %z0.s $0xfffffffe -> %z0.s",   "orr    %z5.s $0xfffffffb -> %z5.s",
        "orr    %z10.s $0xfffffff7 -> %z10.s", "orr    %z16.s $0xffffffef -> %z16.s",
        "orr    %z21.s $0xffffffdf -> %z21.s", "orr    %z31.s $0xffffffc0 -> %z31.s",
    };
    TEST_LOOP(orr, orn_sve_imm, 6, expected_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_immed_int(imm13[i], OPSZ_4));

    const char *expected_3[6] = {
        "orr    %z0.d $0xfffffffffffffffe -> %z0.d",
        "orr    %z5.d $0xfffffffffffffffb -> %z5.d",
        "orr    %z10.d $0xfffffffffffffff7 -> %z10.d",
        "orr    %z16.d $0xffffffffffffffef -> %z16.d",
        "orr    %z21.d $0xffffffffffffffdf -> %z21.d",
        "orr    %z31.d $0xffffffffffffffc0 -> %z31.d",
    };
    TEST_LOOP(orr, orn_sve_imm, 6, expected_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_immed_int(imm13[i], OPSZ_8));
}

TEST_INSTR(and_sve_pred_b)
{

    /* Testing AND     <Pd>.B, <Pg>/Z, <Pn>.B, <Pm>.B */
    reg_id_t Pm_0_0[6] = { DR_REG_P0,  DR_REG_P5,  DR_REG_P8,
                           DR_REG_P11, DR_REG_P13, DR_REG_P15 };
    const char *expected_0_0[6] = {
        "and    %p0/z %p0.b %p0.b -> %p0.b",     "and    %p3/z %p4.b %p5.b -> %p2.b",
        "and    %p6/z %p7.b %p8.b -> %p5.b",     "and    %p9/z %p10.b %p11.b -> %p8.b",
        "and    %p11/z %p12.b %p13.b -> %p10.b", "and    %p15/z %p15.b %p15.b -> %p15.b",
    };
    TEST_LOOP(and, and_sve_pred_b, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false),
              opnd_create_reg_element_vector(Pn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Pm_0_0[i], OPSZ_1));
}

TEST_INSTR(and_sve)
{

    /* Testing AND     <Zd>.D, <Zn>.D, <Zm>.D */
    const char *expected_0_0[6] = {
        "and    %z0.d %z0.d -> %z0.d",    "and    %z6.d %z7.d -> %z5.d",
        "and    %z11.d %z12.d -> %z10.d", "and    %z17.d %z18.d -> %z16.d",
        "and    %z22.d %z23.d -> %z21.d", "and    %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(and, and_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(ands_sve_pred)
{

    /* Testing ANDS    <Pd>.B, <Pg>/Z, <Pn>.B, <Pm>.B */
    reg_id_t Pm_0_0[6] = { DR_REG_P0,  DR_REG_P5,  DR_REG_P8,
                           DR_REG_P11, DR_REG_P13, DR_REG_P15 };
    const char *expected_0_0[6] = {
        "ands   %p0/z %p0.b %p0.b -> %p0.b",     "ands   %p3/z %p4.b %p5.b -> %p2.b",
        "ands   %p6/z %p7.b %p8.b -> %p5.b",     "ands   %p9/z %p10.b %p11.b -> %p8.b",
        "ands   %p11/z %p12.b %p13.b -> %p10.b", "ands   %p15/z %p15.b %p15.b -> %p15.b",
    };
    TEST_LOOP(ands, ands_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false),
              opnd_create_reg_element_vector(Pn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Pm_0_0[i], OPSZ_1));
}

TEST_INSTR(bic_sve_pred_b)
{

    /* Testing BIC     <Pd>.B, <Pg>/Z, <Pn>.B, <Pm>.B */
    reg_id_t Pm_0_0[6] = { DR_REG_P0,  DR_REG_P5,  DR_REG_P8,
                           DR_REG_P11, DR_REG_P13, DR_REG_P15 };
    const char *expected_0_0[6] = {
        "bic    %p0/z %p0.b %p0.b -> %p0.b",     "bic    %p3/z %p4.b %p5.b -> %p2.b",
        "bic    %p6/z %p7.b %p8.b -> %p5.b",     "bic    %p9/z %p10.b %p11.b -> %p8.b",
        "bic    %p11/z %p12.b %p13.b -> %p10.b", "bic    %p15/z %p15.b %p15.b -> %p15.b",
    };
    TEST_LOOP(bic, bic_sve_pred_b, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false),
              opnd_create_reg_element_vector(Pn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Pm_0_0[i], OPSZ_1));
}

TEST_INSTR(bic_sve)
{

    /* Testing BIC     <Zd>.D, <Zn>.D, <Zm>.D */
    const char *expected_0_0[6] = {
        "bic    %z0.d %z0.d -> %z0.d",    "bic    %z6.d %z7.d -> %z5.d",
        "bic    %z11.d %z12.d -> %z10.d", "bic    %z17.d %z18.d -> %z16.d",
        "bic    %z22.d %z23.d -> %z21.d", "bic    %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(bic, bic_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(bics_sve_pred)
{

    /* Testing BICS    <Pd>.B, <Pg>/Z, <Pn>.B, <Pm>.B */
    reg_id_t Pm_0_0[6] = { DR_REG_P0,  DR_REG_P5,  DR_REG_P8,
                           DR_REG_P11, DR_REG_P13, DR_REG_P15 };
    const char *expected_0_0[6] = {
        "bics   %p0/z %p0.b %p0.b -> %p0.b",     "bics   %p3/z %p4.b %p5.b -> %p2.b",
        "bics   %p6/z %p7.b %p8.b -> %p5.b",     "bics   %p9/z %p10.b %p11.b -> %p8.b",
        "bics   %p11/z %p12.b %p13.b -> %p10.b", "bics   %p15/z %p15.b %p15.b -> %p15.b",
    };
    TEST_LOOP(bics, bics_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false),
              opnd_create_reg_element_vector(Pn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Pm_0_0[i], OPSZ_1));
}

TEST_INSTR(eor_sve_pred_b)
{

    /* Testing EOR     <Pd>.B, <Pg>/Z, <Pn>.B, <Pm>.B */
    reg_id_t Pm_0_0[6] = { DR_REG_P0,  DR_REG_P5,  DR_REG_P8,
                           DR_REG_P11, DR_REG_P13, DR_REG_P15 };
    const char *expected_0_0[6] = {
        "eor    %p0/z %p0.b %p0.b -> %p0.b",     "eor    %p3/z %p4.b %p5.b -> %p2.b",
        "eor    %p6/z %p7.b %p8.b -> %p5.b",     "eor    %p9/z %p10.b %p11.b -> %p8.b",
        "eor    %p11/z %p12.b %p13.b -> %p10.b", "eor    %p15/z %p15.b %p15.b -> %p15.b",
    };
    TEST_LOOP(eor, eor_sve_pred_b, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false),
              opnd_create_reg_element_vector(Pn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Pm_0_0[i], OPSZ_1));
}

TEST_INSTR(eor_sve)
{

    /* Testing EOR     <Zd>.D, <Zn>.D, <Zm>.D */
    const char *expected_0_0[6] = {
        "eor    %z0.d %z0.d -> %z0.d",    "eor    %z6.d %z7.d -> %z5.d",
        "eor    %z11.d %z12.d -> %z10.d", "eor    %z17.d %z18.d -> %z16.d",
        "eor    %z22.d %z23.d -> %z21.d", "eor    %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(eor, eor_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(eors_sve_pred)
{

    /* Testing EORS    <Pd>.B, <Pg>/Z, <Pn>.B, <Pm>.B */
    reg_id_t Pm_0_0[6] = { DR_REG_P0,  DR_REG_P5,  DR_REG_P8,
                           DR_REG_P11, DR_REG_P13, DR_REG_P15 };
    const char *expected_0_0[6] = {
        "eors   %p0/z %p0.b %p0.b -> %p0.b",     "eors   %p3/z %p4.b %p5.b -> %p2.b",
        "eors   %p6/z %p7.b %p8.b -> %p5.b",     "eors   %p9/z %p10.b %p11.b -> %p8.b",
        "eors   %p11/z %p12.b %p13.b -> %p10.b", "eors   %p15/z %p15.b %p15.b -> %p15.b",
    };
    TEST_LOOP(eors, eors_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false),
              opnd_create_reg_element_vector(Pn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Pm_0_0[i], OPSZ_1));
}

TEST_INSTR(nand_sve_pred)
{

    /* Testing NAND    <Pd>.B, <Pg>/Z, <Pn>.B, <Pm>.B */
    reg_id_t Pm_0_0[6] = { DR_REG_P0,  DR_REG_P5,  DR_REG_P8,
                           DR_REG_P11, DR_REG_P13, DR_REG_P15 };
    const char *expected_0_0[6] = {
        "nand   %p0/z %p0.b %p0.b -> %p0.b",     "nand   %p3/z %p4.b %p5.b -> %p2.b",
        "nand   %p6/z %p7.b %p8.b -> %p5.b",     "nand   %p9/z %p10.b %p11.b -> %p8.b",
        "nand   %p11/z %p12.b %p13.b -> %p10.b", "nand   %p15/z %p15.b %p15.b -> %p15.b",
    };
    TEST_LOOP(nand, nand_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false),
              opnd_create_reg_element_vector(Pn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Pm_0_0[i], OPSZ_1));
}

TEST_INSTR(nands_sve_pred)
{

    /* Testing NANDS   <Pd>.B, <Pg>/Z, <Pn>.B, <Pm>.B */
    reg_id_t Pm_0_0[6] = { DR_REG_P0,  DR_REG_P5,  DR_REG_P8,
                           DR_REG_P11, DR_REG_P13, DR_REG_P15 };
    const char *expected_0_0[6] = {
        "nands  %p0/z %p0.b %p0.b -> %p0.b",     "nands  %p3/z %p4.b %p5.b -> %p2.b",
        "nands  %p6/z %p7.b %p8.b -> %p5.b",     "nands  %p9/z %p10.b %p11.b -> %p8.b",
        "nands  %p11/z %p12.b %p13.b -> %p10.b", "nands  %p15/z %p15.b %p15.b -> %p15.b",
    };
    TEST_LOOP(nands, nands_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false),
              opnd_create_reg_element_vector(Pn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Pm_0_0[i], OPSZ_1));
}

TEST_INSTR(nor_sve_pred)
{

    /* Testing NOR     <Pd>.B, <Pg>/Z, <Pn>.B, <Pm>.B */
    reg_id_t Pm_0_0[6] = { DR_REG_P0,  DR_REG_P5,  DR_REG_P8,
                           DR_REG_P11, DR_REG_P13, DR_REG_P15 };
    const char *expected_0_0[6] = {
        "nor    %p0/z %p0.b %p0.b -> %p0.b",     "nor    %p3/z %p4.b %p5.b -> %p2.b",
        "nor    %p6/z %p7.b %p8.b -> %p5.b",     "nor    %p9/z %p10.b %p11.b -> %p8.b",
        "nor    %p11/z %p12.b %p13.b -> %p10.b", "nor    %p15/z %p15.b %p15.b -> %p15.b",
    };
    TEST_LOOP(nor, nor_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false),
              opnd_create_reg_element_vector(Pn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Pm_0_0[i], OPSZ_1));
}

TEST_INSTR(nors_sve_pred)
{

    /* Testing NORS    <Pd>.B, <Pg>/Z, <Pn>.B, <Pm>.B */
    reg_id_t Pm_0_0[6] = { DR_REG_P0,  DR_REG_P5,  DR_REG_P8,
                           DR_REG_P11, DR_REG_P13, DR_REG_P15 };
    const char *expected_0_0[6] = {
        "nors   %p0/z %p0.b %p0.b -> %p0.b",     "nors   %p3/z %p4.b %p5.b -> %p2.b",
        "nors   %p6/z %p7.b %p8.b -> %p5.b",     "nors   %p9/z %p10.b %p11.b -> %p8.b",
        "nors   %p11/z %p12.b %p13.b -> %p10.b", "nors   %p15/z %p15.b %p15.b -> %p15.b",
    };
    TEST_LOOP(nors, nors_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false),
              opnd_create_reg_element_vector(Pn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Pm_0_0[i], OPSZ_1));
}

TEST_INSTR(not_sve_pred_vec)
{

    /* Testing NOT     <Zd>.<Ts>, <Pg>/M, <Zn>.<Ts> */
    const char *expected_0_0[6] = {
        "not    %p0/m %z0.b -> %z0.b",   "not    %p2/m %z7.b -> %z5.b",
        "not    %p3/m %z12.b -> %z10.b", "not    %p5/m %z18.b -> %z16.b",
        "not    %p6/m %z23.b -> %z21.b", "not    %p7/m %z31.b -> %z31.b",
    };
    TEST_LOOP(not, not_sve_pred_vec, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_1));

    const char *expected_0_1[6] = {
        "not    %p0/m %z0.h -> %z0.h",   "not    %p2/m %z7.h -> %z5.h",
        "not    %p3/m %z12.h -> %z10.h", "not    %p5/m %z18.h -> %z16.h",
        "not    %p6/m %z23.h -> %z21.h", "not    %p7/m %z31.h -> %z31.h",
    };
    TEST_LOOP(not, not_sve_pred_vec, 6, expected_0_1[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_2),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_2));

    const char *expected_0_2[6] = {
        "not    %p0/m %z0.s -> %z0.s",   "not    %p2/m %z7.s -> %z5.s",
        "not    %p3/m %z12.s -> %z10.s", "not    %p5/m %z18.s -> %z16.s",
        "not    %p6/m %z23.s -> %z21.s", "not    %p7/m %z31.s -> %z31.s",
    };
    TEST_LOOP(not, not_sve_pred_vec, 6, expected_0_2[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_4),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_4));

    const char *expected_0_3[6] = {
        "not    %p0/m %z0.d -> %z0.d",   "not    %p2/m %z7.d -> %z5.d",
        "not    %p3/m %z12.d -> %z10.d", "not    %p5/m %z18.d -> %z16.d",
        "not    %p6/m %z23.d -> %z21.d", "not    %p7/m %z31.d -> %z31.d",
    };
    TEST_LOOP(not, not_sve_pred_vec, 6, expected_0_3[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_predicate_reg(Pn_half_six_offset_0[i], true),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(orn_sve_pred)
{

    /* Testing ORN     <Pd>.B, <Pg>/Z, <Pn>.B, <Pm>.B */
    reg_id_t Pm_0_0[6] = { DR_REG_P0,  DR_REG_P5,  DR_REG_P8,
                           DR_REG_P11, DR_REG_P13, DR_REG_P15 };
    const char *expected_0_0[6] = {
        "orn    %p0/z %p0.b %p0.b -> %p0.b",     "orn    %p3/z %p4.b %p5.b -> %p2.b",
        "orn    %p6/z %p7.b %p8.b -> %p5.b",     "orn    %p9/z %p10.b %p11.b -> %p8.b",
        "orn    %p11/z %p12.b %p13.b -> %p10.b", "orn    %p15/z %p15.b %p15.b -> %p15.b",
    };
    TEST_LOOP(orn, orn_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false),
              opnd_create_reg_element_vector(Pn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Pm_0_0[i], OPSZ_1));
}

TEST_INSTR(orns_sve_pred)
{

    /* Testing ORNS    <Pd>.B, <Pg>/Z, <Pn>.B, <Pm>.B */
    reg_id_t Pm_0_0[6] = { DR_REG_P0,  DR_REG_P5,  DR_REG_P8,
                           DR_REG_P11, DR_REG_P13, DR_REG_P15 };
    const char *expected_0_0[6] = {
        "orns   %p0/z %p0.b %p0.b -> %p0.b",     "orns   %p3/z %p4.b %p5.b -> %p2.b",
        "orns   %p6/z %p7.b %p8.b -> %p5.b",     "orns   %p9/z %p10.b %p11.b -> %p8.b",
        "orns   %p11/z %p12.b %p13.b -> %p10.b", "orns   %p15/z %p15.b %p15.b -> %p15.b",
    };
    TEST_LOOP(orns, orns_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false),
              opnd_create_reg_element_vector(Pn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Pm_0_0[i], OPSZ_1));
}

TEST_INSTR(orr_sve_pred_b)
{

    /* Testing ORR     <Pd>.B, <Pg>/Z, <Pn>.B, <Pm>.B */
    reg_id_t Pm_0_0[6] = { DR_REG_P0,  DR_REG_P5,  DR_REG_P8,
                           DR_REG_P11, DR_REG_P13, DR_REG_P15 };
    const char *expected_0_0[6] = {
        "orr    %p0/z %p0.b %p0.b -> %p0.b",     "orr    %p3/z %p4.b %p5.b -> %p2.b",
        "orr    %p6/z %p7.b %p8.b -> %p5.b",     "orr    %p9/z %p10.b %p11.b -> %p8.b",
        "orr    %p11/z %p12.b %p13.b -> %p10.b", "orr    %p15/z %p15.b %p15.b -> %p15.b",
    };
    TEST_LOOP(orr, orr_sve_pred_b, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false),
              opnd_create_reg_element_vector(Pn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Pm_0_0[i], OPSZ_1));
}

TEST_INSTR(orr_sve)
{

    /* Testing ORR     <Zd>.D, <Zn>.D, <Zm>.D */
    const char *expected_0_0[6] = {
        "orr    %z0.d %z0.d -> %z0.d",    "orr    %z6.d %z7.d -> %z5.d",
        "orr    %z11.d %z12.d -> %z10.d", "orr    %z17.d %z18.d -> %z16.d",
        "orr    %z22.d %z23.d -> %z21.d", "orr    %z31.d %z31.d -> %z31.d",
    };
    TEST_LOOP(orr, orr_sve, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Zn_six_offset_0[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_1[i], OPSZ_8),
              opnd_create_reg_element_vector(Zn_six_offset_2[i], OPSZ_8));
}

TEST_INSTR(orrs_sve_pred)
{

    /* Testing ORRS    <Pd>.B, <Pg>/Z, <Pn>.B, <Pm>.B */
    reg_id_t Pm_0_0[6] = { DR_REG_P0,  DR_REG_P5,  DR_REG_P8,
                           DR_REG_P11, DR_REG_P13, DR_REG_P15 };
    const char *expected_0_0[6] = {
        "orrs   %p0/z %p0.b %p0.b -> %p0.b",     "orrs   %p3/z %p4.b %p5.b -> %p2.b",
        "orrs   %p6/z %p7.b %p8.b -> %p5.b",     "orrs   %p9/z %p10.b %p11.b -> %p8.b",
        "orrs   %p11/z %p12.b %p13.b -> %p10.b", "orrs   %p15/z %p15.b %p15.b -> %p15.b",
    };
    TEST_LOOP(orrs, orrs_sve_pred, 6, expected_0_0[i],
              opnd_create_reg_element_vector(Pn_six_offset_0[i], OPSZ_1),
              opnd_create_predicate_reg(Pn_six_offset_1[i], false),
              opnd_create_reg_element_vector(Pn_six_offset_2[i], OPSZ_1),
              opnd_create_reg_element_vector(Pm_0_0[i], OPSZ_1));
}

TEST_INSTR(str)
{
    /* Testing STR <Zt>, [<Xn|SP>{, #<simm>, MUL VL}] */
    int simm_0[6] = { 0, 255, -256, 127, -128, -1 };
    const char *expected_0[6] = {
        "str    %z0 -> (%x0)[32byte]",          "str    %z5 -> +0xff(%x5)[32byte]",
        "str    %z10 -> -0x0100(%x10)[32byte]", "str    %z16 -> +0x7f(%x16)[32byte]",
        "str    %z21 -> -0x80(%x21)[32byte]",   "str    %z31 -> -0x01(%x30)[32byte]"
    };
    TEST_LOOP(str, str, 6, expected_0[i],
              opnd_create_base_disp_aarch64(Xn_six_offset_0[i], DR_REG_NULL, 0, false,
                                            simm_0[i], 0, OPSZ_32),
              opnd_create_reg(Zn_six_offset_0[i]));

    /* STR <Pt>, [<Xn|SP>{, #<simm>, MUL VL}] */
    int simm_1[6] = { 0, 255, -256, 127, -128, -1 };
    const char *expected_1[6] = {
        "str    %p0 -> (%x0)[32byte]",         "str    %p2 -> +0xff(%x5)[32byte]",
        "str    %p5 -> -0x0100(%x10)[32byte]", "str    %p8 -> +0x7f(%x16)[32byte]",
        "str    %p10 -> -0x80(%x21)[32byte]",  "str    %p15 -> -0x01(%x30)[32byte]"
    };
    TEST_LOOP(str, str, 6, expected_1[i],
              opnd_create_base_disp_aarch64(Xn_six_offset_0[i], DR_REG_NULL, 0, false,
                                            simm_1[i], 0, OPSZ_32),
              opnd_create_reg(Pn_six_offset_0[i]));
}

TEST_INSTR(ldr)
{
    /* Testing STR <Zt>, [<Xn|SP>{, #<simm>, MUL VL}] */
    int simm_0[6] = { 0, 255, -256, 127, -128, -1 };
    const char *expected_0[6] = {
        "ldr    (%x0)[32byte] -> %z0",          "ldr    +0xff(%x6)[32byte] -> %z6",
        "ldr    -0x0100(%x11)[32byte] -> %z11", "ldr    +0x7f(%x17)[32byte] -> %z17",
        "ldr    -0x80(%x22)[32byte] -> %z22",   "ldr    -0x01(%x30)[32byte] -> %z31",
    };
    TEST_LOOP(ldr, ldr, 6, expected_0[i], opnd_create_reg(Zn_six_offset_1[i]),
              opnd_create_base_disp_aarch64(Xn_six_offset_1[i], DR_REG_NULL, 0, false,
                                            simm_0[i], 0, OPSZ_32));

    /* LDR <Pt>, [<Xn|SP>{, #<simm>, MUL VL}] */
    int simm_1[6] = { 0, 255, -256, 127, -128, -1 };
    const char *expected_1[6] = {
        "ldr    (%x0)[32byte] -> %p0",         "ldr    +0xff(%x6)[32byte] -> %p3",
        "ldr    -0x0100(%x11)[32byte] -> %p6", "ldr    +0x7f(%x17)[32byte] -> %p9",
        "ldr    -0x80(%x22)[32byte] -> %p11",  "ldr    -0x01(%x30)[32byte] -> %p15",
    };
    TEST_LOOP(ldr, ldr, 6, expected_1[i], opnd_create_reg(Pn_six_offset_1[i]),
              opnd_create_base_disp_aarch64(Xn_six_offset_1[i], DR_REG_NULL, 0, false,
                                            simm_1[i], 0, OPSZ_32));
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

    RUN_INSTR_TEST(sxtb_sve_pred);
    RUN_INSTR_TEST(sxth_sve_pred);
    RUN_INSTR_TEST(sxtw_sve_pred);
    RUN_INSTR_TEST(uxtb_sve_pred);
    RUN_INSTR_TEST(uxth_sve_pred);
    RUN_INSTR_TEST(uxtw_sve_pred);

    RUN_INSTR_TEST(fcmeq_sve_zero_pred);
    RUN_INSTR_TEST(fcmeq_sve_pred);
    RUN_INSTR_TEST(fcmge_sve_zero_pred);
    RUN_INSTR_TEST(fcmge_sve_pred);
    RUN_INSTR_TEST(fcmgt_sve_zero_pred);
    RUN_INSTR_TEST(fcmgt_sve_pred);
    RUN_INSTR_TEST(fcmle_sve_zero_pred);
    RUN_INSTR_TEST(fcmlt_sve_zero_pred);
    RUN_INSTR_TEST(fcmne_sve_zero_pred);
    RUN_INSTR_TEST(fcmne_sve_pred);
    RUN_INSTR_TEST(fcmuo_sve_pred);

    RUN_INSTR_TEST(cmpeq_sve_pred_simm);
    RUN_INSTR_TEST(cmpeq_sve_pred);
    RUN_INSTR_TEST(cmpge_sve_pred_simm);
    RUN_INSTR_TEST(cmpge_sve_pred);
    RUN_INSTR_TEST(cmpgt_sve_pred_simm);
    RUN_INSTR_TEST(cmpgt_sve_pred);
    RUN_INSTR_TEST(cmphi_sve_pred_imm);
    RUN_INSTR_TEST(cmphi_sve_pred);
    RUN_INSTR_TEST(cmphs_sve_pred_imm);
    RUN_INSTR_TEST(cmphs_sve_pred);
    RUN_INSTR_TEST(cmple_sve_pred_simm);
    RUN_INSTR_TEST(cmple_sve_pred);
    RUN_INSTR_TEST(cmplo_sve_pred_imm);
    RUN_INSTR_TEST(cmplo_sve_pred);
    RUN_INSTR_TEST(cmpls_sve_pred_imm);
    RUN_INSTR_TEST(cmpls_sve_pred);
    RUN_INSTR_TEST(cmplt_sve_pred_simm);
    RUN_INSTR_TEST(cmplt_sve_pred);
    RUN_INSTR_TEST(cmpne_sve_pred_simm);
    RUN_INSTR_TEST(cmpne_sve_pred);

    RUN_INSTR_TEST(rdffr_sve_pred);
    RUN_INSTR_TEST(rdffr_sve);
    RUN_INSTR_TEST(rdffrs_sve_pred);
    RUN_INSTR_TEST(setffr_sve);
    RUN_INSTR_TEST(wrffr_sve);

    RUN_INSTR_TEST(cntp_sve_pred);
    RUN_INSTR_TEST(decp_sve);
    RUN_INSTR_TEST(decp_sve);
    RUN_INSTR_TEST(incp_sve);
    RUN_INSTR_TEST(incp_sve);
    RUN_INSTR_TEST(sqdecp_sve);
    RUN_INSTR_TEST(sqdecp_sve);
    RUN_INSTR_TEST(sqincp_sve);
    RUN_INSTR_TEST(sqincp_sve);
    RUN_INSTR_TEST(uqdecp_sve);
    RUN_INSTR_TEST(uqdecp_sve);
    RUN_INSTR_TEST(uqincp_sve);
    RUN_INSTR_TEST(uqincp_sve);

    RUN_INSTR_TEST(and_sve_imm);
    RUN_INSTR_TEST(bic_sve_imm);
    RUN_INSTR_TEST(eor_sve_imm);
    RUN_INSTR_TEST(orr_sve_imm);
    RUN_INSTR_TEST(orn_sve_imm);

    RUN_INSTR_TEST(and_sve_pred_b);
    RUN_INSTR_TEST(and_sve);
    RUN_INSTR_TEST(ands_sve_pred);
    RUN_INSTR_TEST(bic_sve_pred_b);
    RUN_INSTR_TEST(bic_sve);
    RUN_INSTR_TEST(bics_sve_pred);
    RUN_INSTR_TEST(eor_sve_pred_b);
    RUN_INSTR_TEST(eor_sve);
    RUN_INSTR_TEST(eors_sve_pred);
    RUN_INSTR_TEST(nand_sve_pred);
    RUN_INSTR_TEST(nands_sve_pred);
    RUN_INSTR_TEST(nor_sve_pred);
    RUN_INSTR_TEST(nors_sve_pred);
    RUN_INSTR_TEST(not_sve_pred_vec);
    RUN_INSTR_TEST(orn_sve_pred);
    RUN_INSTR_TEST(orns_sve_pred);
    RUN_INSTR_TEST(orr_sve_pred_b);
    RUN_INSTR_TEST(orr_sve);
    RUN_INSTR_TEST(orrs_sve_pred);

    RUN_INSTR_TEST(str);
    RUN_INSTR_TEST(ldr);

    print("All sve tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
