/* **********************************************************
 * Copyright (c) 2024 ARM Limited. All rights reserved.
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

TEST_INSTR(wfet)
{
    /* Testing WFET <Xt> */
    const char *const expected_0_0[6] = { "wfet   %x0",  "wfet   %x5",  "wfet   %x10",
                                          "wfet   %x15", "wfet   %x20", "wfet   %x30" };
    TEST_LOOP(wfet, wfet, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]));
}

TEST_INSTR(wfit)
{
    /* Testing WFIT <Xt> */
    const char *const expected_0_0[6] = { "wfit   %x0",  "wfit   %x5",  "wfit   %x10",
                                          "wfit   %x15", "wfit   %x20", "wfit   %x30" };
    TEST_LOOP(wfit, wfit, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0[i]));
}

TEST_INSTR(ld64b)
{
    /* Testing LD64B <Xt>, [<Xn|SP> {, #0}] */
    const char *const expected_0_0[6] = {
        "ld64b  (%x0)[64byte] -> %x6 %x7 %x8 %x9 %x10 %x11 %x12 %x13",
        "ld64b  (%x5)[64byte] -> %x10 %x11 %x12 %x13 %x14 %x15 %x16 %x17",
        "ld64b  (%x10)[64byte] -> %x22 %x23 %x24 %x25 %x26 %x27 %x28 %x29",
        "ld64b  (%x15)[64byte] -> %x4 %x5 %x6 %x7 %x8 %x9 %x10 %x11",
        "ld64b  (%x20)[64byte] -> %x12 %x13 %x14 %x15 %x16 %x17 %x18 %x19",
        "ld64b  (%x30)[64byte] -> %x8 %x9 %x10 %x11 %x12 %x13 %x14 %x15"
    };
    TEST_LOOP(ld64b, ld64b, 6, expected_0_0[i], opnd_create_reg(Xn_six_offset_0_8[i]),
              opnd_create_base_disp(Xn_six_offset_0[i], DR_REG_NULL, 0, 0, OPSZ_64));

    /* Check that invalid encodings fail. */
    instr =
        INSTR_CREATE_ld64b(dc, opnd_create_reg(DR_REG_X24), opnd_create_reg(DR_REG_X4));
    if (test_instr_encoding(
            dc, OP_ld64b, instr,
            "ld64b  (%x4)[64byte] -> %x24 %x25 %x26 %x27 %x28 %x29 %x30 %x31"))
        *psuccess = false;
    instr_destroy(dc, instr);

    instr =
        INSTR_CREATE_ld64b(dc, opnd_create_reg(DR_REG_X21), opnd_create_reg(DR_REG_X0));
    if (test_instr_encoding(
            dc, OP_ld64b, instr,
            "ld64b  (%x0)[64byte] -> %x21 %x22 %x23 %x24 %x25 %x26 %x27 %x28"))
        *psuccess = false;
    instr_destroy(dc, instr);
}

TEST_INSTR(st64b)
{
    /* ST64B <Xt>, [<Xn|SP> {, #0}] */
    const char *const expected_0_0[6] = {
        "st64b  %x6 %x7 %x8 %x9 %x10 %x11 %x12 %x13 -> (%x0)[64byte]",
        "st64b  %x10 %x11 %x12 %x13 %x14 %x15 %x16 %x17 -> (%x5)[64byte]",
        "st64b  %x22 %x23 %x24 %x25 %x26 %x27 %x28 %x29 -> (%x10)[64byte]",
        "st64b  %x4 %x5 %x6 %x7 %x8 %x9 %x10 %x11 -> (%x15)[64byte]",
        "st64b  %x12 %x13 %x14 %x15 %x16 %x17 %x18 %x19 -> (%x20)[64byte]",
        "st64b  %x8 %x9 %x10 %x11 %x12 %x13 %x14 %x15 -> (%x30)[64byte]"
    };
    TEST_LOOP(st64b, st64b, 6, expected_0_0[i],
              opnd_create_base_disp(Xn_six_offset_0[i], DR_REG_NULL, 0, 0, OPSZ_64),
              opnd_create_reg(Xn_six_offset_0_8[i]));

    /* Check that invalid encodings fail. */
    instr =
        INSTR_CREATE_st64b(dc, opnd_create_reg(DR_REG_X4), opnd_create_reg(DR_REG_X24));
    if (test_instr_encoding(
            dc, OP_st64b, instr,
            "st64b  %x24 %x25 %x26 %x27 %x28 %x29 %x30 %x31 -> (%x4)[64byte]"))
        *psuccess = false;
    instr_destroy(dc, instr);

    instr =
        INSTR_CREATE_st64b(dc, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_X21));
    if (test_instr_encoding(
            dc, OP_st64b, instr,
            "st64b  %x21 %x22 %x23 %x24 %x25 %x26 %x27 %x28 -> (%x0)[64byte]"))
        *psuccess = false;
    instr_destroy(dc, instr);
}

TEST_INSTR(st64bv)
{
    /* ST64BV <Xs>, <Xt>, [<Xn|SP>] */
    const char *const expected_0_0[6] = {
        "st64bv %x6 %x7 %x8 %x9 %x10 %x11 %x12 %x13 -> (%x0)[64byte] %x20",
        "st64bv %x10 %x11 %x12 %x13 %x14 %x15 %x16 %x17 -> (%x5)[64byte] %x1",
        "st64bv %x22 %x23 %x24 %x25 %x26 %x27 %x28 %x29 -> (%x10)[64byte] %x30",
        "st64bv %x4 %x5 %x6 %x7 %x8 %x9 %x10 %x11 -> (%x15)[64byte] %x21",
        "st64bv %x12 %x13 %x14 %x15 %x16 %x17 %x18 %x19 -> (%x20)[64byte] %x0",
        "st64bv %x8 %x9 %x10 %x11 %x12 %x13 %x14 %x15 -> (%x30)[64byte] %x3"
    };
    TEST_LOOP(st64bv, st64bv, 6, expected_0_0[i],
              opnd_create_base_disp(Xn_six_offset_0[i], DR_REG_NULL, 0, 0, OPSZ_64),
              opnd_create_reg(Xn_six_offset_4[i]), opnd_create_reg(Xn_six_offset_0_8[i]));

    /* Check that invalid encodings fail. */
    instr = INSTR_CREATE_st64bv(dc, opnd_create_reg(DR_REG_X4),
                                opnd_create_reg(DR_REG_X6), opnd_create_reg(DR_REG_X24));
    if (test_instr_encoding(
            dc, OP_st64bv, instr,
            "st64bv  %x24 %x25 %x26 %x27 %x28 %x29 %x30 %x31 -> (%x4)[64byte] %x6"))
        *psuccess = false;
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_st64bv(dc, opnd_create_reg(DR_REG_X0),
                                opnd_create_reg(DR_REG_X4), opnd_create_reg(DR_REG_X21));
    if (test_instr_encoding(
            dc, OP_st64bv, instr,
            "st64bv  %x21 %x22 %x23 %x24 %x25 %x26 %x27 %x28 -> (%x0)[64byte] %x4"))
        *psuccess = false;
    instr_destroy(dc, instr);
}

TEST_INSTR(st64bv0)
{
    /* ST64BV0 <Xs>, <Xt>, [<Xn|SP>] */
    const char *const expected_0_0[6] = {
        "st64bv0 %x6 %x7 %x8 %x9 %x10 %x11 %x12 %x13 -> (%x0)[64byte] %x20",
        "st64bv0 %x10 %x11 %x12 %x13 %x14 %x15 %x16 %x17 -> (%x5)[64byte] %x1",
        "st64bv0 %x22 %x23 %x24 %x25 %x26 %x27 %x28 %x29 -> (%x10)[64byte] %x30",
        "st64bv0 %x4 %x5 %x6 %x7 %x8 %x9 %x10 %x11 -> (%x15)[64byte] %x21",
        "st64bv0 %x12 %x13 %x14 %x15 %x16 %x17 %x18 %x19 -> (%x20)[64byte] %x0",
        "st64bv0 %x8 %x9 %x10 %x11 %x12 %x13 %x14 %x15 -> (%x30)[64byte] %x3"
    };
    TEST_LOOP(st64bv0, st64bv0, 6, expected_0_0[i],
              opnd_create_base_disp(Xn_six_offset_0[i], DR_REG_NULL, 0, 0, OPSZ_64),
              opnd_create_reg(Xn_six_offset_4[i]), opnd_create_reg(Xn_six_offset_0_8[i]));

    /* Check that invalid encodings fail. */
    instr = INSTR_CREATE_st64bv0(dc, opnd_create_reg(DR_REG_X4),
                                 opnd_create_reg(DR_REG_X6), opnd_create_reg(DR_REG_X24));
    if (test_instr_encoding(
            dc, OP_st64bv0, instr,
            "st64bv0  %x24 %x25 %x26 %x27 %x28 %x29 %x30 %x31 -> (%x4)[64byte] %x6"))
        *psuccess = false;
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_st64bv0(dc, opnd_create_reg(DR_REG_X0),
                                 opnd_create_reg(DR_REG_X4), opnd_create_reg(DR_REG_X21));
    if (test_instr_encoding(
            dc, OP_st64bv0, instr,
            "st64bv0  %x21 %x22 %x23 %x24 %x25 %x26 %x27 %x28 -> (%x0)[64byte] %x4"))
        *psuccess = false;
    instr_destroy(dc, instr);
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

    /* FEAT_WFxT */
    RUN_INSTR_TEST(wfet);
    RUN_INSTR_TEST(wfit);
    RUN_INSTR_TEST(ld64b);
    RUN_INSTR_TEST(st64b);
    RUN_INSTR_TEST(st64bv);
    RUN_INSTR_TEST(st64bv0);

    print("All v8.7 tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
