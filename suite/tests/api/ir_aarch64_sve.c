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

TEST_INSTR(add_vector)
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
    TEST_LOOP(add, sve_add_vector, 6, expected_0_0[i],
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
    TEST_LOOP(add, sve_add_vector, 6, expected_0_1[i],
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
    TEST_LOOP(add, sve_add_vector, 6, expected_0_2[i],
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
    TEST_LOOP(add, sve_add_vector, 6, expected_0_3[i],
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

    RUN_INSTR_TEST(add_vector);

    RUN_INSTR_TEST(zip2_vector);

    RUN_INSTR_TEST(movprfx_vector);

    print("All sve tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
