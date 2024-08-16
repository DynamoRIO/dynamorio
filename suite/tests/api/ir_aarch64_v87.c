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

    print("All v8.7 tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
