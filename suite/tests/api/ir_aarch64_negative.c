/* **********************************************************
 * Copyright (c) 2020 Google, Inc. All rights reserved.
 * Copyright (c) 2018 Arm Limited. All rights reserved.
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

/* This file contains tests to ensure we fail to encode invalid IR instructions. */

/* Define DR_FAST_IR to verify that everything compiles when we call the inline
 * versions of these routines.
 */
#ifndef STANDALONE_DECODER
#    define DR_FAST_IR 1
#endif

/* Uses the DR CLIENT_INTERFACE API, using DR as a standalone library, rather than
 * being a client library working with DR on a target program.
 */

#include "configure.h"
#include "dr_api.h"
#include "tools.h"

#ifdef STANDALONE_DECODER
#    define ASSERT(x)                                                                 \
        ((void)((!(x)) ? (fprintf(stderr, "ASSERT FAILURE (standalone): %s:%d: %s\n", \
                                  __FILE__, __LINE__, #x),                            \
                          abort(), 0)                                                 \
                       : 0))
#else
#    define ASSERT(x)                                                                \
        ((void)((!(x)) ? (dr_fprintf(STDERR, "ASSERT FAILURE (client): %s:%d: %s\n", \
                                     __FILE__, __LINE__, #x),                        \
                          dr_abort(), 0)                                             \
                       : 0))
#endif

static void
test_fmov_general(void *dc)
{
    byte *pc;
    instr_t *instr;

    instr = INSTR_CREATE_fmov_general(dc, opnd_create_reg(DR_REG_D10),
                                      opnd_create_reg(DR_REG_W9));
    ASSERT(!instr_is_encoding_possible(instr));
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_fmov_general(dc, opnd_create_reg(DR_REG_S10),
                                      opnd_create_reg(DR_REG_X9));
    ASSERT(!instr_is_encoding_possible(instr));
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_fmov_general(dc, opnd_create_reg(DR_REG_W10),
                                      opnd_create_reg(DR_REG_X9));
    ASSERT(!instr_is_encoding_possible(instr));
    instr_destroy(dc, instr);
}

static void
test_sve_int_bin_pred_log(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* SVE bitwise logical operations (predicated)
     * Make sure we fail to encode if output and first input registers do not match.
     */

    instr = INSTR_CREATE_orr_sve_pred(
        dc, opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_P7),
        opnd_create_reg(DR_REG_Z5), opnd_create_reg(DR_REG_Z13), OPND_CREATE_BYTE());
    ASSERT(!instr_is_encoding_possible(instr));
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_eor_sve_pred(
        dc, opnd_create_reg(DR_REG_Z29), opnd_create_reg(DR_REG_P4),
        opnd_create_reg(DR_REG_Z9), opnd_create_reg(DR_REG_Z2), OPND_CREATE_DOUBLE());
    ASSERT(!instr_is_encoding_possible(instr));
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_and_sve_pred(
        dc, opnd_create_reg(DR_REG_Z31), opnd_create_reg(DR_REG_P1),
        opnd_create_reg(DR_REG_Z1), opnd_create_reg(DR_REG_Z23), OPND_CREATE_SINGLE());
    ASSERT(!instr_is_encoding_possible(instr));
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_bic_sve_pred(
        dc, opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_P2),
        opnd_create_reg(DR_REG_Z3), opnd_create_reg(DR_REG_Z24), OPND_CREATE_HALF());
    ASSERT(!instr_is_encoding_possible(instr));
    instr_destroy(dc, instr);

    /* Make sure predicate registers P8-P15 are not accepted. */
    instr = INSTR_CREATE_orr_sve_pred(
        dc, opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_P8),
        opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_Z13), OPND_CREATE_BYTE());
    ASSERT(!instr_is_encoding_possible(instr));
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_eor_sve_pred(
        dc, opnd_create_reg(DR_REG_Z29), opnd_create_reg(DR_REG_P9),
        opnd_create_reg(DR_REG_Z29), opnd_create_reg(DR_REG_Z2), OPND_CREATE_DOUBLE());
    ASSERT(!instr_is_encoding_possible(instr));
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_and_sve_pred(
        dc, opnd_create_reg(DR_REG_Z31), opnd_create_reg(DR_REG_P10),
        opnd_create_reg(DR_REG_Z31), opnd_create_reg(DR_REG_Z23), OPND_CREATE_SINGLE());
    ASSERT(!instr_is_encoding_possible(instr));
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_and_sve_pred(
        dc, opnd_create_reg(DR_REG_Z31), opnd_create_reg(DR_REG_P11),
        opnd_create_reg(DR_REG_Z31), opnd_create_reg(DR_REG_Z23), OPND_CREATE_SINGLE());
    ASSERT(!instr_is_encoding_possible(instr));
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_bic_sve_pred(
        dc, opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_P12),
        opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_Z24), OPND_CREATE_HALF());
    ASSERT(!instr_is_encoding_possible(instr));
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_and_sve_pred(
        dc, opnd_create_reg(DR_REG_Z31), opnd_create_reg(DR_REG_P13),
        opnd_create_reg(DR_REG_Z31), opnd_create_reg(DR_REG_Z23), OPND_CREATE_SINGLE());
    ASSERT(!instr_is_encoding_possible(instr));
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_bic_sve_pred(
        dc, opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_P14),
        opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_Z24), OPND_CREATE_HALF());
    ASSERT(!instr_is_encoding_possible(instr));
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_bic_sve_pred(
        dc, opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_P15),
        opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_Z24), OPND_CREATE_HALF());
    ASSERT(!instr_is_encoding_possible(instr));
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

    test_fmov_general(dcontext);
    print("test_fmov_general complete\n");

    test_sve_int_bin_pred_log(dcontext);
    print("test_sve_int_bin_pred_log complete\n");

    print("All tests complete\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    return 0;
}
