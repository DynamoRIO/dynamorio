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

/*
 * FCVTAS
 */

TEST_INSTR(fcvtas_vector)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* FCVTAS  <Vd>.8H, <Vn>.8H */
    opnd_t elsz;
    reg_id_t Rd_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_0[3] = {
        "fcvtas %q0 $0x01 -> %q0",
        "fcvtas %q10 $0x01 -> %q10",
        "fcvtas %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtas_vector(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]), elsz);
        if (!test_instr_encoding(dc, OP_fcvtas, instr, expected_0[i]))
            success = false;
    }

    /* FCVTAS  <Vd>.4H, <Vn>.4H */
    reg_id_t Rd_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_1[3] = {
        "fcvtas %d0 $0x01 -> %d0",
        "fcvtas %d10 $0x01 -> %d10",
        "fcvtas %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtas_vector(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_1[i]), elsz);
        if (!test_instr_encoding(dc, OP_fcvtas, instr, expected_1[i]))
            success = false;
    }

    return success;
}

TEST_INSTR(fcvtas_scalar)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* FCVTAS  <Wd>, <Hn> */
    reg_id_t Rd_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_0[3] = {
        "fcvtas %h0 -> %w0",
        "fcvtas %h10 -> %w10",
        "fcvtas %h31 -> %w30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtas_scalar(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtas, instr, expected_0[i]))
            success = false;
    }

    /* FCVTAS  <Xd>, <Hn> */
    reg_id_t Rd_1[3] = { DR_REG_X0, DR_REG_X10, DR_REG_X30 };
    const char *expected_1[3] = {
        "fcvtas %h0 -> %x0",
        "fcvtas %h10 -> %x10",
        "fcvtas %h31 -> %x30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtas_scalar(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtas, instr, expected_1[i]))
            success = false;
    }

    /* FCVTAS  <Hd>, <Hn> */
    reg_id_t Rd_2[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_2[3] = {
        "fcvtas %h0 -> %h0",
        "fcvtas %h10 -> %h10",
        "fcvtas %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtas_scalar(dc, opnd_create_reg(Rd_2[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtas, instr, expected_2[i]))
            success = false;
    }

    return success;
}

/*
 * FCVTAU
 */

TEST_INSTR(fcvtau_vector)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* FCVTAU  <Vd>.8H, <Vn>.8H */
    opnd_t elsz;
    reg_id_t Rd_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_0[3] = {
        "fcvtau %q0 $0x01 -> %q0",
        "fcvtau %q10 $0x01 -> %q10",
        "fcvtau %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtau_vector(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]), elsz);
        if (!test_instr_encoding(dc, OP_fcvtau, instr, expected_0[i]))
            success = false;
    }

    /* FCVTAU  <Vd>.4H, <Vn>.4H */
    reg_id_t Rd_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_1[3] = {
        "fcvtau %d0 $0x01 -> %d0",
        "fcvtau %d10 $0x01 -> %d10",
        "fcvtau %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtau_vector(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_1[i]), elsz);
        if (!test_instr_encoding(dc, OP_fcvtau, instr, expected_1[i]))
            success = false;
    }

    return success;
}

TEST_INSTR(fcvtau_scalar)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* FCVTAU  <Wd>, <Hn> */
    reg_id_t Rd_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_0[3] = {
        "fcvtau %h0 -> %w0",
        "fcvtau %h10 -> %w10",
        "fcvtau %h31 -> %w30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtau_scalar(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtau, instr, expected_0[i]))
            success = false;
    }

    /* FCVTAU  <Xd>, <Hn> */
    reg_id_t Rd_1[3] = { DR_REG_X0, DR_REG_X10, DR_REG_X30 };
    const char *expected_1[3] = {
        "fcvtau %h0 -> %x0",
        "fcvtau %h10 -> %x10",
        "fcvtau %h31 -> %x30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtau_scalar(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtau, instr, expected_1[i]))
            success = false;
    }

    /* FCVTAU  <Hd>, <Hn> */
    reg_id_t Rd_2[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_2[3] = {
        "fcvtau %h0 -> %h0",
        "fcvtau %h10 -> %h10",
        "fcvtau %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtau_scalar(dc, opnd_create_reg(Rd_2[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtau, instr, expected_2[i]))
            success = false;
    }

    return success;
}

/*
 * FCVTMS
 */

TEST_INSTR(fcvtms_vector)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* FCVTMS  <Vd>.8H, <Vn>.8H */
    opnd_t elsz;
    reg_id_t Rd_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_0[3] = {
        "fcvtms %q0 $0x01 -> %q0",
        "fcvtms %q10 $0x01 -> %q10",
        "fcvtms %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtms_vector(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]), elsz);
        if (!test_instr_encoding(dc, OP_fcvtms, instr, expected_0[i]))
            success = false;
    }

    /* FCVTMS  <Vd>.4H, <Vn>.4H */
    reg_id_t Rd_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_1[3] = {
        "fcvtms %d0 $0x01 -> %d0",
        "fcvtms %d10 $0x01 -> %d10",
        "fcvtms %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtms_vector(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_1[i]), elsz);
        if (!test_instr_encoding(dc, OP_fcvtms, instr, expected_1[i]))
            success = false;
    }

    return success;
}

TEST_INSTR(fcvtms_scalar)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* FCVTMS  <Wd>, <Hn> */
    reg_id_t Rd_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_0[3] = {
        "fcvtms %h0 -> %w0",
        "fcvtms %h10 -> %w10",
        "fcvtms %h31 -> %w30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtms_scalar(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtms, instr, expected_0[i]))
            success = false;
    }

    /* FCVTMS  <Xd>, <Hn> */
    reg_id_t Rd_1[3] = { DR_REG_X0, DR_REG_X10, DR_REG_X30 };
    const char *expected_1[3] = {
        "fcvtms %h0 -> %x0",
        "fcvtms %h10 -> %x10",
        "fcvtms %h31 -> %x30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtms_scalar(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtms, instr, expected_1[i]))
            success = false;
    }

    /* FCVTMS  <Hd>, <Hn> */
    reg_id_t Rd_2[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_2[3] = {
        "fcvtms %h0 -> %h0",
        "fcvtms %h10 -> %h10",
        "fcvtms %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtms_scalar(dc, opnd_create_reg(Rd_2[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtms, instr, expected_2[i]))
            success = false;
    }

    return success;
}

/*
 * FCVTNS
 */

TEST_INSTR(fcvtns_vector)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* FCVTNS  <Vd>.8H, <Vn>.8H */
    opnd_t elsz;
    reg_id_t Rd_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_0[3] = {
        "fcvtns %q0 $0x01 -> %q0",
        "fcvtns %q10 $0x01 -> %q10",
        "fcvtns %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtns_vector(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]), elsz);
        if (!test_instr_encoding(dc, OP_fcvtns, instr, expected_0[i]))
            success = false;
    }

    /* FCVTNS  <Vd>.4H, <Vn>.4H */
    reg_id_t Rd_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_1[3] = {
        "fcvtns %d0 $0x01 -> %d0",
        "fcvtns %d10 $0x01 -> %d10",
        "fcvtns %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtns_vector(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_1[i]), elsz);
        if (!test_instr_encoding(dc, OP_fcvtns, instr, expected_1[i]))
            success = false;
    }

    return success;
}

TEST_INSTR(fcvtns_scalar)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* FCVTNS  <Wd>, <Hn> */
    reg_id_t Rd_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_0[3] = {
        "fcvtns %h0 -> %w0",
        "fcvtns %h10 -> %w10",
        "fcvtns %h31 -> %w30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtns_scalar(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtns, instr, expected_0[i]))
            success = false;
    }

    /* FCVTNS  <Xd>, <Hn> */
    reg_id_t Rd_1[3] = { DR_REG_X0, DR_REG_X10, DR_REG_X30 };
    const char *expected_1[3] = {
        "fcvtns %h0 -> %x0",
        "fcvtns %h10 -> %x10",
        "fcvtns %h31 -> %x30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtns_scalar(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtns, instr, expected_1[i]))
            success = false;
    }

    /* FCVTNS  <Hd>, <Hn> */
    reg_id_t Rd_2[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_2[3] = {
        "fcvtns %h0 -> %h0",
        "fcvtns %h10 -> %h10",
        "fcvtns %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtns_scalar(dc, opnd_create_reg(Rd_2[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtns, instr, expected_2[i]))
            success = false;
    }

    return success;
}

/*
 * FCVTPS
 */

TEST_INSTR(fcvtps_vector)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* FCVTPS  <Vd>.8H, <Vn>.8H */
    opnd_t elsz;
    reg_id_t Rd_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_0[3] = {
        "fcvtps %q0 $0x01 -> %q0",
        "fcvtps %q10 $0x01 -> %q10",
        "fcvtps %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtps_vector(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]), elsz);
        if (!test_instr_encoding(dc, OP_fcvtps, instr, expected_0[i]))
            success = false;
    }

    /* FCVTPS  <Vd>.4H, <Vn>.4H */
    reg_id_t Rd_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_1[3] = {
        "fcvtps %d0 $0x01 -> %d0",
        "fcvtps %d10 $0x01 -> %d10",
        "fcvtps %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtps_vector(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_1[i]), elsz);
        if (!test_instr_encoding(dc, OP_fcvtps, instr, expected_1[i]))
            success = false;
    }

    return success;
}

TEST_INSTR(fcvtps_scalar)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* FCVTPS  <Wd>, <Hn> */
    reg_id_t Rd_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_0[3] = {
        "fcvtps %h0 -> %w0",
        "fcvtps %h10 -> %w10",
        "fcvtps %h31 -> %w30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtps_scalar(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtps, instr, expected_0[i]))
            success = false;
    }

    /* FCVTPS  <Xd>, <Hn> */
    reg_id_t Rd_1[3] = { DR_REG_X0, DR_REG_X10, DR_REG_X30 };
    const char *expected_1[3] = {
        "fcvtps %h0 -> %x0",
        "fcvtps %h10 -> %x10",
        "fcvtps %h31 -> %x30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtps_scalar(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtps, instr, expected_1[i]))
            success = false;
    }

    /* FCVTPS  <Hd>, <Hn> */
    reg_id_t Rd_2[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_2[3] = {
        "fcvtps %h0 -> %h0",
        "fcvtps %h10 -> %h10",
        "fcvtps %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtps_scalar(dc, opnd_create_reg(Rd_2[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtps, instr, expected_2[i]))
            success = false;
    }

    return success;
}

/*
 * FCVTPU
 */

TEST_INSTR(fcvtpu_vector)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* FCVTPU  <Vd>.8H, <Vn>.8H */
    opnd_t elsz;
    reg_id_t Rd_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_0[3] = {
        "fcvtpu %q0 $0x01 -> %q0",
        "fcvtpu %q10 $0x01 -> %q10",
        "fcvtpu %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtpu_vector(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]), elsz);
        if (!test_instr_encoding(dc, OP_fcvtpu, instr, expected_0[i]))
            success = false;
    }

    /* FCVTPU  <Vd>.4H, <Vn>.4H */
    reg_id_t Rd_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_1[3] = {
        "fcvtpu %d0 $0x01 -> %d0",
        "fcvtpu %d10 $0x01 -> %d10",
        "fcvtpu %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtpu_vector(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_1[i]), elsz);
        if (!test_instr_encoding(dc, OP_fcvtpu, instr, expected_1[i]))
            success = false;
    }

    return success;
}

TEST_INSTR(fcvtpu_scalar)
{
    bool success = true;
    instr_t *instr;
    byte *pc;

    /* FCVTPU  <Wd>, <Hn> */
    reg_id_t Rd_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_0[3] = {
        "fcvtpu %h0 -> %w0",
        "fcvtpu %h10 -> %w10",
        "fcvtpu %h31 -> %w30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtpu_scalar(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtpu, instr, expected_0[i]))
            success = false;
    }

    /* FCVTPU  <Xd>, <Hn> */
    reg_id_t Rd_1[3] = { DR_REG_X0, DR_REG_X10, DR_REG_X30 };
    const char *expected_1[3] = {
        "fcvtpu %h0 -> %x0",
        "fcvtpu %h10 -> %x10",
        "fcvtpu %h31 -> %x30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtpu_scalar(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtpu, instr, expected_1[i]))
            success = false;
    }

    /* FCVTPU  <Hd>, <Hn> */
    reg_id_t Rd_2[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_2[3] = {
        "fcvtpu %h0 -> %h0",
        "fcvtpu %h10 -> %h10",
        "fcvtpu %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtpu_scalar(dc, opnd_create_reg(Rd_2[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtpu, instr, expected_2[i]))
            success = false;
    }

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

    RUN_INSTR_TEST(fcvtas_vector);
    RUN_INSTR_TEST(fcvtas_scalar);
    RUN_INSTR_TEST(fcvtau_vector);
    RUN_INSTR_TEST(fcvtau_scalar);
    RUN_INSTR_TEST(fcvtms_vector);
    RUN_INSTR_TEST(fcvtms_scalar);
    RUN_INSTR_TEST(fcvtns_vector);
    RUN_INSTR_TEST(fcvtns_scalar);
    RUN_INSTR_TEST(fcvtps_vector);
    RUN_INSTR_TEST(fcvtps_scalar);
    RUN_INSTR_TEST(fcvtpu_vector);
    RUN_INSTR_TEST(fcvtpu_scalar);

    print("All v8.2 tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
