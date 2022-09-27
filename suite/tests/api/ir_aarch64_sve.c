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
    opnd_t Zm_elsz;

    /* Testing ADD     <Zd>.<Ts>, <Zn>.<Ts>, <Zm>.<Ts> */
    reg_id_t Zd_0_0[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_0[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_0[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    Zm_elsz = OPND_CREATE_BYTE();
    const char *expected_0_0[6] = {
        "add    %z0 %z0 $0x00 -> %z0",    "add    %z6 %z7 $0x00 -> %z5",
        "add    %z11 %z12 $0x00 -> %z10", "add    %z16 %z17 $0x00 -> %z15",
        "add    %z21 %z22 $0x00 -> %z20", "add    %z30 %z30 $0x00 -> %z30",
    };
    TEST_LOOP(add, add_vector, 6, expected_0_0[i], opnd_create_reg(Zd_0_0[i]),
              opnd_create_reg(Zn_0_0[i]), opnd_create_reg(Zm_0_0[i]), Zm_elsz);

    reg_id_t Zd_0_1[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_1[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_1[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    Zm_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[6] = {
        "add    %z0 %z0 $0x01 -> %z0",    "add    %z6 %z7 $0x01 -> %z5",
        "add    %z11 %z12 $0x01 -> %z10", "add    %z16 %z17 $0x01 -> %z15",
        "add    %z21 %z22 $0x01 -> %z20", "add    %z30 %z30 $0x01 -> %z30",
    };
    TEST_LOOP(add, add_vector, 6, expected_0_1[i], opnd_create_reg(Zd_0_1[i]),
              opnd_create_reg(Zn_0_1[i]), opnd_create_reg(Zm_0_1[i]), Zm_elsz);

    reg_id_t Zd_0_2[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_2[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_2[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    Zm_elsz = OPND_CREATE_SINGLE();
    const char *expected_0_2[6] = {
        "add    %z0 %z0 $0x02 -> %z0",    "add    %z6 %z7 $0x02 -> %z5",
        "add    %z11 %z12 $0x02 -> %z10", "add    %z16 %z17 $0x02 -> %z15",
        "add    %z21 %z22 $0x02 -> %z20", "add    %z30 %z30 $0x02 -> %z30",
    };
    TEST_LOOP(add, add_vector, 6, expected_0_2[i], opnd_create_reg(Zd_0_2[i]),
              opnd_create_reg(Zn_0_2[i]), opnd_create_reg(Zm_0_2[i]), Zm_elsz);

    reg_id_t Zd_0_3[6] = { DR_REG_Z0,  DR_REG_Z5,  DR_REG_Z10,
                           DR_REG_Z15, DR_REG_Z20, DR_REG_Z30 };
    reg_id_t Zn_0_3[6] = { DR_REG_Z0,  DR_REG_Z6,  DR_REG_Z11,
                           DR_REG_Z16, DR_REG_Z21, DR_REG_Z30 };
    reg_id_t Zm_0_3[6] = { DR_REG_Z0,  DR_REG_Z7,  DR_REG_Z12,
                           DR_REG_Z17, DR_REG_Z22, DR_REG_Z30 };
    Zm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_0_3[6] = {
        "add    %z0 %z0 $0x03 -> %z0",    "add    %z6 %z7 $0x03 -> %z5",
        "add    %z11 %z12 $0x03 -> %z10", "add    %z16 %z17 $0x03 -> %z15",
        "add    %z21 %z22 $0x03 -> %z20", "add    %z30 %z30 $0x03 -> %z30",
    };
    TEST_LOOP(add, add_vector, 6, expected_0_3[i], opnd_create_reg(Zd_0_3[i]),
              opnd_create_reg(Zn_0_3[i]), opnd_create_reg(Zm_0_3[i]), Zm_elsz);

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

    print("All sve tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
