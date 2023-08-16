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
            *psuccess = false;
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
            *psuccess = false;
    }
}

TEST_INSTR(fcvtas_scalar)
{

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
            *psuccess = false;
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
            *psuccess = false;
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
            *psuccess = false;
    }
}

/*
 * FCVTAU
 */

TEST_INSTR(fcvtau_vector)
{

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
            *psuccess = false;
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
            *psuccess = false;
    }
}

TEST_INSTR(fcvtau_scalar)
{

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
            *psuccess = false;
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
            *psuccess = false;
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
            *psuccess = false;
    }
}

/*
 * FCVTMS
 */

TEST_INSTR(fcvtms_vector)
{

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
            *psuccess = false;
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
            *psuccess = false;
    }
}

TEST_INSTR(fcvtms_scalar)
{

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
            *psuccess = false;
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
            *psuccess = false;
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
            *psuccess = false;
    }
}

/*
 * FCVTMU
 */

TEST_INSTR(fcvtmu_vector)
{

    /* FCVTMU  <Vd>.8H, <Vn>.8H */
    opnd_t elsz;
    reg_id_t Rd_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_0[3] = {
        "fcvtmu %q0 $0x01 -> %q0",
        "fcvtmu %q11 $0x01 -> %q10",
        "fcvtmu %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtmu_vector(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]), elsz);
        if (!test_instr_encoding(dc, OP_fcvtmu, instr, expected_0[i]))
            *psuccess = false;
    }

    /* FCVTMU  <Vd>.4H, <Vn>.4H */
    reg_id_t Rd_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_1[3] = {
        "fcvtmu %d0 $0x01 -> %d0",
        "fcvtmu %d11 $0x01 -> %d10",
        "fcvtmu %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtmu_vector(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_1[i]), elsz);
        if (!test_instr_encoding(dc, OP_fcvtmu, instr, expected_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(fcvtmu_scalar)
{

    /* FCVTMU  <Wd>, <Hn> */
    reg_id_t Rd_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    const char *expected_0[3] = {
        "fcvtmu %h0 -> %w0",
        "fcvtmu %h11 -> %w10",
        "fcvtmu %h31 -> %w30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtmu_scalar(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtmu, instr, expected_0[i]))
            *psuccess = false;
    }

    /* FCVTMU  <Xd>, <Hn> */
    reg_id_t Rd_1[3] = { DR_REG_X0, DR_REG_X10, DR_REG_X30 };
    const char *expected_1[3] = {
        "fcvtmu %h0 -> %x0",
        "fcvtmu %h11 -> %x10",
        "fcvtmu %h31 -> %x30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtmu_scalar(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtmu, instr, expected_1[i]))
            *psuccess = false;
    }

    /* FCVTMU  <Hd>, <Hn> */
    reg_id_t Rd_2[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_2[3] = {
        "fcvtmu %h0 -> %h0",
        "fcvtmu %h11 -> %h10",
        "fcvtmu %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtmu_scalar(dc, opnd_create_reg(Rd_2[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtmu, instr, expected_2[i]))
            *psuccess = false;
    }
}

/*
 * FCVTNS
 */

TEST_INSTR(fcvtns_vector)
{

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
            *psuccess = false;
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
            *psuccess = false;
    }
}

TEST_INSTR(fcvtns_scalar)
{

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
            *psuccess = false;
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
            *psuccess = false;
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
            *psuccess = false;
    }
}

/*
 * FCVTNU
 */

TEST_INSTR(fcvtnu_vector)
{

    /* FCVTNU  <Vd>.8H, <Vn>.8H */
    opnd_t elsz;
    reg_id_t Rd_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_0[3] = {
        "fcvtnu %q0 $0x01 -> %q0",
        "fcvtnu %q11 $0x01 -> %q10",
        "fcvtnu %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtnu_vector(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]), elsz);
        if (!test_instr_encoding(dc, OP_fcvtnu, instr, expected_0[i]))
            *psuccess = false;
    }

    /* FCVTNU  <Vd>.4H, <Vn>.4H */
    reg_id_t Rd_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_1[3] = {
        "fcvtnu %d0 $0x01 -> %d0",
        "fcvtnu %d11 $0x01 -> %d10",
        "fcvtnu %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtnu_vector(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_1[i]), elsz);
        if (!test_instr_encoding(dc, OP_fcvtnu, instr, expected_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(fcvtnu_scalar)
{

    /* FCVTNU  <Wd>, <Hn> */
    reg_id_t Rd_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    const char *expected_0[3] = {
        "fcvtnu %h0 -> %w0",
        "fcvtnu %h11 -> %w10",
        "fcvtnu %h31 -> %w30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtnu_scalar(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtnu, instr, expected_0[i]))
            *psuccess = false;
    }

    /* FCVTNU  <Xd>, <Hn> */
    reg_id_t Rd_1[3] = { DR_REG_X0, DR_REG_X10, DR_REG_X30 };
    const char *expected_1[3] = {
        "fcvtnu %h0 -> %x0",
        "fcvtnu %h11 -> %x10",
        "fcvtnu %h31 -> %x30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtnu_scalar(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtnu, instr, expected_1[i]))
            *psuccess = false;
    }

    /* FCVTNU  <Hd>, <Hn> */
    reg_id_t Rd_2[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_2[3] = {
        "fcvtnu %h0 -> %h0",
        "fcvtnu %h11 -> %h10",
        "fcvtnu %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtnu_scalar(dc, opnd_create_reg(Rd_2[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtnu, instr, expected_2[i]))
            *psuccess = false;
    }
}

/*
 * FCVTPS
 */

TEST_INSTR(fcvtps_vector)
{

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
            *psuccess = false;
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
            *psuccess = false;
    }
}

TEST_INSTR(fcvtps_scalar)
{

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
            *psuccess = false;
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
            *psuccess = false;
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
            *psuccess = false;
    }
}

/*
 * FCVTPU
 */

TEST_INSTR(fcvtpu_vector)
{

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
            *psuccess = false;
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
            *psuccess = false;
    }
}

TEST_INSTR(fcvtpu_scalar)
{

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
            *psuccess = false;
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
            *psuccess = false;
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
            *psuccess = false;
    }
}

/*
 * FCVTZS
 */

TEST_INSTR(fcvtzs_vector)
{

    /* Testing FCVTZS  <Hd>.<Ts>, <Hn>.<Ts> */

    /* Testing FCVTZS  <Hd>.4H, <Hn>.4H */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    opnd_t Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[3] = {
        "fcvtzs %d0 $0x01 -> %d0",
        "fcvtzs %d11 $0x01 -> %d10",
        "fcvtzs %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtzs_vector(dc, opnd_create_reg(Rd_0_0[i]),
                                           opnd_create_reg(Rn_0_0[i]), Rn_elsz);
        if (!test_instr_encoding(dc, OP_fcvtzs, instr, expected_0_0[i]))
            *psuccess = false;
    }

    /* Testing FCVTZS  <Hd>.8H, <Hn>.8H */
    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[3] = {
        "fcvtzs %q0 $0x01 -> %q0",
        "fcvtzs %q11 $0x01 -> %q10",
        "fcvtzs %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtzs_vector(dc, opnd_create_reg(Rd_0_1[i]),
                                           opnd_create_reg(Rn_0_1[i]), Rn_elsz);
        if (!test_instr_encoding(dc, OP_fcvtzs, instr, expected_0_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(fcvtzs_scalar)
{

    /* Testing FCVTZS  <Wd>, <Hn> */
    reg_id_t Rd_1_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_1_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    const char *expected_1_0[3] = {
        "fcvtzs %h0 -> %w0",
        "fcvtzs %h11 -> %w10",
        "fcvtzs %h31 -> %w30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtzs_scalar(dc, opnd_create_reg(Rd_1_0[i]),
                                           opnd_create_reg(Rn_1_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtzs, instr, expected_1_0[i]))
            *psuccess = false;
    }

    /* Testing FCVTZS  <Xd>, <Hn> */
    reg_id_t Rd_4_0[3] = { DR_REG_X0, DR_REG_X10, DR_REG_X30 };
    reg_id_t Rn_4_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    const char *expected_4_0[3] = {
        "fcvtzs %h0 -> %x0",
        "fcvtzs %h11 -> %x10",
        "fcvtzs %h31 -> %x30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtzs_scalar(dc, opnd_create_reg(Rd_4_0[i]),
                                           opnd_create_reg(Rn_4_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtzs, instr, expected_4_0[i]))
            *psuccess = false;
    }

    /* Testing FCVTZS  <Wd>, <Hn>, #<fbits> */
    reg_id_t Rd_10_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_10_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    uint scale_10_0[3] = { 32, 22, 1 };
    const char *expected_10_0[3] = {
        "fcvtzs %h0 $0x20 -> %w0",
        "fcvtzs %h11 $0x16 -> %w10",
        "fcvtzs %h31 $0x01 -> %w30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtzs_scalar_fixed(
            dc, opnd_create_reg(Rd_10_0[i]), opnd_create_reg(Rn_10_0[i]),
            opnd_create_immed_uint(scale_10_0[i], OPSZ_0));
        if (!test_instr_encoding(dc, OP_fcvtzs, instr, expected_10_0[i]))
            *psuccess = false;
    }

    /* Testing FCVTZS  <Xd>, <Hn>, #<fbits> */
    reg_id_t Rd_11_0[3] = { DR_REG_X0, DR_REG_X10, DR_REG_X30 };
    reg_id_t Rn_11_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    uint scale_11_0[3] = { 64, 33, 1 };
    const char *expected_11_0[3] = {
        "fcvtzs %h0 $0x40 -> %x0",
        "fcvtzs %h11 $0x21 -> %x10",
        "fcvtzs %h31 $0x01 -> %x30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtzs_scalar_fixed(
            dc, opnd_create_reg(Rd_11_0[i]), opnd_create_reg(Rn_11_0[i]),
            opnd_create_immed_uint(scale_11_0[i], OPSZ_0));
        if (!test_instr_encoding(dc, OP_fcvtzs, instr, expected_11_0[i]))
            *psuccess = false;
    }

    /* Testing FCVTZS  <Hd>, <Hn> */
    reg_id_t Rd_12_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_12_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    const char *expected_12_0[3] = {
        "fcvtzs %h0 -> %h0",
        "fcvtzs %h11 -> %h10",
        "fcvtzs %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtzs_scalar(dc, opnd_create_reg(Rd_12_0[i]),
                                           opnd_create_reg(Rn_12_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtzs, instr, expected_12_0[i]))
            *psuccess = false;
    }
}

/*
 * FCVTZU
 */

TEST_INSTR(fcvtzu_vector)
{

    /* Testing FCVTZU  <Hd>.<Ts>, <Hn>.<Ts> */

    /* Testing FCVTZU  <Hd>.4H, <Hn>.4H */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    opnd_t Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[3] = {
        "fcvtzu %d0 $0x01 -> %d0",
        "fcvtzu %d11 $0x01 -> %d10",
        "fcvtzu %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtzu_vector(dc, opnd_create_reg(Rd_0_0[i]),
                                           opnd_create_reg(Rn_0_0[i]), Rn_elsz);
        if (!test_instr_encoding(dc, OP_fcvtzu, instr, expected_0_0[i]))
            *psuccess = false;
    }

    /* Testing FCVTZU  <Hd>.8H, <Hn>.8H */
    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[3] = {
        "fcvtzu %q0 $0x01 -> %q0",
        "fcvtzu %q11 $0x01 -> %q10",
        "fcvtzu %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtzu_vector(dc, opnd_create_reg(Rd_0_1[i]),
                                           opnd_create_reg(Rn_0_1[i]), Rn_elsz);
        if (!test_instr_encoding(dc, OP_fcvtzu, instr, expected_0_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(fcvtzu_scalar)
{

    /* Testing FCVTZU  <Wd>, <Hn> */
    reg_id_t Rd_1_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_1_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    const char *expected_1_0[3] = {
        "fcvtzu %h0 -> %w0",
        "fcvtzu %h11 -> %w10",
        "fcvtzu %h31 -> %w30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtzu_scalar(dc, opnd_create_reg(Rd_1_0[i]),
                                           opnd_create_reg(Rn_1_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtzu, instr, expected_1_0[i]))
            *psuccess = false;
    }

    /* Testing FCVTZU  <Xd>, <Hn> */
    reg_id_t Rd_4_0[3] = { DR_REG_X0, DR_REG_X10, DR_REG_X30 };
    reg_id_t Rn_4_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    const char *expected_4_0[3] = {
        "fcvtzu %h0 -> %x0",
        "fcvtzu %h11 -> %x10",
        "fcvtzu %h31 -> %x30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtzu_scalar(dc, opnd_create_reg(Rd_4_0[i]),
                                           opnd_create_reg(Rn_4_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtzu, instr, expected_4_0[i]))
            *psuccess = false;
    }

    /* Testing FCVTZU  <Wd>, <Hn>, #<fbits> */
    reg_id_t Rd_10_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_10_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    uint scale_10_0[3] = { 32, 22, 1 };
    const char *expected_10_0[3] = {
        "fcvtzu %h0 $0x20 -> %w0",
        "fcvtzu %h11 $0x16 -> %w10",
        "fcvtzu %h31 $0x01 -> %w30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtzu_scalar_fixed(
            dc, opnd_create_reg(Rd_10_0[i]), opnd_create_reg(Rn_10_0[i]),
            opnd_create_immed_uint(scale_10_0[i], OPSZ_0));
        if (!test_instr_encoding(dc, OP_fcvtzu, instr, expected_10_0[i]))
            *psuccess = false;
    }

    /* Testing FCVTZU  <Xd>, <Hn>, #<fbits> */
    reg_id_t Rd_11_0[3] = { DR_REG_X0, DR_REG_X10, DR_REG_X30 };
    reg_id_t Rn_11_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    uint scale_11_0[3] = { 64, 33, 1 };
    const char *expected_11_0[3] = {
        "fcvtzu %h0 $0x40 -> %x0",
        "fcvtzu %h11 $0x21 -> %x10",
        "fcvtzu %h31 $0x01 -> %x30",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtzu_scalar_fixed(
            dc, opnd_create_reg(Rd_11_0[i]), opnd_create_reg(Rn_11_0[i]),
            opnd_create_immed_uint(scale_11_0[i], OPSZ_0));
        if (!test_instr_encoding(dc, OP_fcvtzu, instr, expected_11_0[i]))
            *psuccess = false;
    }

    /* Testing FCVTZU  <Hd>, <Hn> */
    reg_id_t Rd_12_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_12_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    const char *expected_12_0[3] = {
        "fcvtzu %h0 -> %h0",
        "fcvtzu %h11 -> %h10",
        "fcvtzu %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcvtzu_scalar(dc, opnd_create_reg(Rd_12_0[i]),
                                           opnd_create_reg(Rn_12_0[i]));
        if (!test_instr_encoding(dc, OP_fcvtzu, instr, expected_12_0[i]))
            *psuccess = false;
    }
}

/*
 * FRINTA
 */

TEST_INSTR(frinta_vector)
{

    /* FRINTA  <Hd>.8H, <Hn>.8H */
    opnd_t elsz;
    reg_id_t Rd_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_0[3] = {
        "frinta %q0 $0x01 -> %q0",
        "frinta %q10 $0x01 -> %q10",
        "frinta %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frinta_vector(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]), elsz);
        if (!test_instr_encoding(dc, OP_frinta, instr, expected_0[i]))
            *psuccess = false;
    }

    /* FRINTA  <Hd>.4H, <Hn>.4H */
    reg_id_t Rd_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_1[3] = {
        "frinta %d0 $0x01 -> %d0",
        "frinta %d10 $0x01 -> %d10",
        "frinta %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frinta_vector(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_1[i]), elsz);
        if (!test_instr_encoding(dc, OP_frinta, instr, expected_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(frinta_scalar)
{

    /* FRINTA  <Hd>, <Hn> */
    reg_id_t Rd_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_0[3] = {
        "frinta %h0 -> %h0",
        "frinta %h10 -> %h10",
        "frinta %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frinta_scalar(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_frinta, instr, expected_0[i]))
            *psuccess = false;
    }
}

/*
 * FRINTI
 */

TEST_INSTR(frinti_vector)
{

    /* FRINTI  <Hd>.8H, <Hn>.8H */
    opnd_t elsz;
    reg_id_t Rd_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_0[3] = {
        "frinti %q0 $0x01 -> %q0",
        "frinti %q10 $0x01 -> %q10",
        "frinti %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frinti_vector(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]), elsz);
        if (!test_instr_encoding(dc, OP_frinti, instr, expected_0[i]))
            *psuccess = false;
    }

    /* FRINTI  <Hd>.4H, <Hn>.4H */
    reg_id_t Rd_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_1[3] = {
        "frinti %d0 $0x01 -> %d0",
        "frinti %d10 $0x01 -> %d10",
        "frinti %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frinti_vector(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_1[i]), elsz);
        if (!test_instr_encoding(dc, OP_frinti, instr, expected_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(frinti_scalar)
{

    /* FRINTI  <Hd>, <Hn> */
    reg_id_t Rd_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_0[3] = {
        "frinti %h0 -> %h0",
        "frinti %h10 -> %h10",
        "frinti %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frinti_scalar(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_frinti, instr, expected_0[i]))
            *psuccess = false;
    }
}

/*
 * FRINTM
 */

TEST_INSTR(frintm_vector)
{

    /* FRINTM  <Hd>.8H, <Hn>.8H */
    opnd_t elsz;
    reg_id_t Rd_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_0[3] = {
        "frintm %q0 $0x01 -> %q0",
        "frintm %q10 $0x01 -> %q10",
        "frintm %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frintm_vector(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]), elsz);
        if (!test_instr_encoding(dc, OP_frintm, instr, expected_0[i]))
            *psuccess = false;
    }

    /* FRINTM  <Hd>.4H, <Hn>.4H */
    reg_id_t Rd_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_1[3] = {
        "frintm %d0 $0x01 -> %d0",
        "frintm %d10 $0x01 -> %d10",
        "frintm %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frintm_vector(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_1[i]), elsz);
        if (!test_instr_encoding(dc, OP_frintm, instr, expected_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(frintm_scalar)
{

    /* FRINTM  <Hd>, <Hn> */
    reg_id_t Rd_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_0[3] = {
        "frintm %h0 -> %h0",
        "frintm %h10 -> %h10",
        "frintm %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frintm_scalar(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_frintm, instr, expected_0[i]))
            *psuccess = false;
    }
}

/*
 * FRINTN
 */

TEST_INSTR(frintn_vector)
{

    /* FRINTN  <Hd>.8H, <Hn>.8H */
    opnd_t elsz;
    reg_id_t Rd_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_0[3] = {
        "frintn %q0 $0x01 -> %q0",
        "frintn %q10 $0x01 -> %q10",
        "frintn %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frintn_vector(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]), elsz);
        if (!test_instr_encoding(dc, OP_frintn, instr, expected_0[i]))
            *psuccess = false;
    }

    /* FRINTN  <Hd>.4H, <Hn>.4H */
    reg_id_t Rd_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_1[3] = {
        "frintn %d0 $0x01 -> %d0",
        "frintn %d10 $0x01 -> %d10",
        "frintn %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frintn_vector(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_1[i]), elsz);
        if (!test_instr_encoding(dc, OP_frintn, instr, expected_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(frintn_scalar)
{

    /* FRINTN  <Hd>, <Hn> */
    reg_id_t Rd_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_0[3] = {
        "frintn %h0 -> %h0",
        "frintn %h10 -> %h10",
        "frintn %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frintn_scalar(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_frintn, instr, expected_0[i]))
            *psuccess = false;
    }
}

/*
 * FRINTP
 */

TEST_INSTR(frintp_vector)
{

    /* FRINTP  <Hd>.8H, <Hn>.8H */
    opnd_t elsz;
    reg_id_t Rd_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_0[3] = {
        "frintp %q0 $0x01 -> %q0",
        "frintp %q10 $0x01 -> %q10",
        "frintp %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frintp_vector(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]), elsz);
        if (!test_instr_encoding(dc, OP_frintp, instr, expected_0[i]))
            *psuccess = false;
    }

    /* FRINTP  <Hd>.4H, <Hn>.4H */
    reg_id_t Rd_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_1[3] = {
        "frintp %d0 $0x01 -> %d0",
        "frintp %d10 $0x01 -> %d10",
        "frintp %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frintp_vector(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_1[i]), elsz);
        if (!test_instr_encoding(dc, OP_frintp, instr, expected_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(frintp_scalar)
{

    /* FRINTP  <Hd>, <Hn> */
    reg_id_t Rd_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_0[3] = {
        "frintp %h0 -> %h0",
        "frintp %h10 -> %h10",
        "frintp %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frintp_scalar(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_frintp, instr, expected_0[i]))
            *psuccess = false;
    }
}

/*
 * FRINTX
 */

TEST_INSTR(frintx_vector)
{

    /* FRINTX  <Hd>.8H, <Hn>.8H */
    opnd_t elsz;
    reg_id_t Rd_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_0[3] = {
        "frintx %q0 $0x01 -> %q0",
        "frintx %q10 $0x01 -> %q10",
        "frintx %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frintx_vector(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]), elsz);
        if (!test_instr_encoding(dc, OP_frintx, instr, expected_0[i]))
            *psuccess = false;
    }

    /* FRINTX  <Hd>.4H, <Hn>.4H */
    reg_id_t Rd_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_1[3] = {
        "frintx %d0 $0x01 -> %d0",
        "frintx %d10 $0x01 -> %d10",
        "frintx %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frintx_vector(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_1[i]), elsz);
        if (!test_instr_encoding(dc, OP_frintx, instr, expected_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(frintx_scalar)
{

    /* FRINTX  <Hd>, <Hn> */
    reg_id_t Rd_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_0[3] = {
        "frintx %h0 -> %h0",
        "frintx %h10 -> %h10",
        "frintx %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frintx_scalar(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_frintx, instr, expected_0[i]))
            *psuccess = false;
    }
}

/*
 * FRINTZ
 */

TEST_INSTR(frintz_vector)
{

    /* FRINTZ  <Hd>.8H, <Hn>.8H */
    opnd_t elsz;
    reg_id_t Rd_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_0[3] = {
        "frintz %q0 $0x01 -> %q0",
        "frintz %q10 $0x01 -> %q10",
        "frintz %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frintz_vector(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]), elsz);
        if (!test_instr_encoding(dc, OP_frintz, instr, expected_0[i]))
            *psuccess = false;
    }

    /* FRINTZ  <Hd>.4H, <Hn>.4H */
    reg_id_t Rd_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    elsz = OPND_CREATE_HALF();
    const char *expected_1[3] = {
        "frintz %d0 $0x01 -> %d0",
        "frintz %d10 $0x01 -> %d10",
        "frintz %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frintz_vector(dc, opnd_create_reg(Rd_1[i]),
                                           opnd_create_reg(Rn_1[i]), elsz);
        if (!test_instr_encoding(dc, OP_frintz, instr, expected_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(frintz_scalar)
{

    /* FRINTZ  <Hd>, <Hn> */
    reg_id_t Rd_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_0[3] = {
        "frintz %h0 -> %h0",
        "frintz %h10 -> %h10",
        "frintz %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_frintz_scalar(dc, opnd_create_reg(Rd_0[i]),
                                           opnd_create_reg(Rn_0[i]));
        if (!test_instr_encoding(dc, OP_frintz, instr, expected_0[i]))
            *psuccess = false;
    }
}

/*
 * FMLAL
 */

TEST_INSTR(fmlal_vector)
{

    /* Testing FMLAL   <Sd>.<Ts>, <Hn>.<Tb>, <Hm>.<Tb> */
    reg_id_t Rd_0_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_0_0[6] = { DR_REG_S0,  DR_REG_S6,  DR_REG_S11,
                           DR_REG_S17, DR_REG_S22, DR_REG_S31 };
    reg_id_t Rm_0_0[6] = { DR_REG_S0,  DR_REG_S7,  DR_REG_S12,
                           DR_REG_S18, DR_REG_S23, DR_REG_S31 };
    const char *expected_0_0[6] = {
        "fmlal  %d0 %s0 %s0 $0x01 -> %d0",     "fmlal  %d5 %s6 %s7 $0x01 -> %d5",
        "fmlal  %d10 %s11 %s12 $0x01 -> %d10", "fmlal  %d16 %s17 %s18 $0x01 -> %d16",
        "fmlal  %d21 %s22 %s23 $0x01 -> %d21", "fmlal  %d31 %s31 %s31 $0x01 -> %d31",
    };
    TEST_LOOP(fmlal, fmlal_vector, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]));

    reg_id_t Rd_0_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_0_1[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_0_1[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    const char *expected_0_1[6] = {
        "fmlal  %q0 %d0 %d0 $0x01 -> %q0",     "fmlal  %q5 %d6 %d7 $0x01 -> %q5",
        "fmlal  %q10 %d11 %d12 $0x01 -> %q10", "fmlal  %q16 %d17 %d18 $0x01 -> %q16",
        "fmlal  %q21 %d22 %d23 $0x01 -> %q21", "fmlal  %q31 %d31 %d31 $0x01 -> %q31",
    };
    TEST_LOOP(fmlal, fmlal_vector, 6, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]));
}

TEST_INSTR(fmlal_vector_idx)
{

    /* Testing FMLAL   <Sd>.<Ts>, <Hn>.<Tb>, <Hm>.H[<index>] */
    reg_id_t Rd_0_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_0_0[6] = { DR_REG_S0,  DR_REG_S6,  DR_REG_S11,
                           DR_REG_S17, DR_REG_S22, DR_REG_S31 };
    reg_id_t Rm_0_0[6] = { DR_REG_D0,  DR_REG_D4,  DR_REG_D7,
                           DR_REG_D10, DR_REG_D12, DR_REG_D15 };
    uint index_0_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *expected_0_0[6] = {
        "fmlal  %d0 %s0 %d0 $0x00 $0x01 -> %d0",
        "fmlal  %d5 %s6 %d4 $0x04 $0x01 -> %d5",
        "fmlal  %d10 %s11 %d7 $0x05 $0x01 -> %d10",
        "fmlal  %d16 %s17 %d10 $0x07 $0x01 -> %d16",
        "fmlal  %d21 %s22 %d12 $0x00 $0x01 -> %d21",
        "fmlal  %d31 %s31 %d15 $0x07 $0x01 -> %d31",
    };
    TEST_LOOP(fmlal, fmlal_vector_idx, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]),
              opnd_create_immed_uint(index_0_0[i], OPSZ_0));

    reg_id_t Rd_0_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_0_1[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_0_1[6] = { DR_REG_Q0,  DR_REG_Q4,  DR_REG_Q7,
                           DR_REG_Q10, DR_REG_Q12, DR_REG_Q15 };
    uint index_0_1[6] = { 0, 4, 5, 7, 0, 7 };
    const char *expected_0_1[6] = {
        "fmlal  %q0 %d0 %q0 $0x00 $0x01 -> %q0",
        "fmlal  %q5 %d6 %q4 $0x04 $0x01 -> %q5",
        "fmlal  %q10 %d11 %q7 $0x05 $0x01 -> %q10",
        "fmlal  %q16 %d17 %q10 $0x07 $0x01 -> %q16",
        "fmlal  %q21 %d22 %q12 $0x00 $0x01 -> %q21",
        "fmlal  %q31 %d31 %q15 $0x07 $0x01 -> %q31",
    };
    TEST_LOOP(fmlal, fmlal_vector_idx, 6, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]),
              opnd_create_immed_uint(index_0_1[i], OPSZ_0));
}

TEST_INSTR(fmlal2_vector)
{

    /* Testing FMLAL2  <Sd>.<Ts>, <Hn>.<Tb>, <Hm>.<Tb> */
    reg_id_t Rd_0_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_0_0[6] = { DR_REG_S0,  DR_REG_S6,  DR_REG_S11,
                           DR_REG_S17, DR_REG_S22, DR_REG_S31 };
    reg_id_t Rm_0_0[6] = { DR_REG_S0,  DR_REG_S7,  DR_REG_S12,
                           DR_REG_S18, DR_REG_S23, DR_REG_S31 };
    const char *expected_0_0[6] = {
        "fmlal2 %d0 %s0 %s0 $0x01 -> %d0",     "fmlal2 %d5 %s6 %s7 $0x01 -> %d5",
        "fmlal2 %d10 %s11 %s12 $0x01 -> %d10", "fmlal2 %d16 %s17 %s18 $0x01 -> %d16",
        "fmlal2 %d21 %s22 %s23 $0x01 -> %d21", "fmlal2 %d31 %s31 %s31 $0x01 -> %d31",
    };
    TEST_LOOP(fmlal2, fmlal2_vector, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]));

    reg_id_t Rd_0_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_0_1[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_0_1[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    const char *expected_0_1[6] = {
        "fmlal2 %q0 %d0 %d0 $0x01 -> %q0",     "fmlal2 %q5 %d6 %d7 $0x01 -> %q5",
        "fmlal2 %q10 %d11 %d12 $0x01 -> %q10", "fmlal2 %q16 %d17 %d18 $0x01 -> %q16",
        "fmlal2 %q21 %d22 %d23 $0x01 -> %q21", "fmlal2 %q31 %d31 %d31 $0x01 -> %q31",
    };
    TEST_LOOP(fmlal2, fmlal2_vector, 6, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]));
}

TEST_INSTR(fmlal2_vector_idx)
{

    /* Testing FMLAL2  <Sd>.<Ts>, <Hn>.<Tb>, <Hm>.H[<index>] */
    reg_id_t Rd_0_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_0_0[6] = { DR_REG_S0,  DR_REG_S6,  DR_REG_S11,
                           DR_REG_S17, DR_REG_S22, DR_REG_S31 };
    reg_id_t Rm_0_0[6] = { DR_REG_D0,  DR_REG_D4,  DR_REG_D7,
                           DR_REG_D10, DR_REG_D12, DR_REG_D15 };
    uint index_0_0[6] = { 0, 4, 5, 7, 0, 7 };
    const char *expected_0_0[6] = {
        "fmlal2 %d0 %s0 %d0 $0x00 $0x01 -> %d0",
        "fmlal2 %d5 %s6 %d4 $0x04 $0x01 -> %d5",
        "fmlal2 %d10 %s11 %d7 $0x05 $0x01 -> %d10",
        "fmlal2 %d16 %s17 %d10 $0x07 $0x01 -> %d16",
        "fmlal2 %d21 %s22 %d12 $0x00 $0x01 -> %d21",
        "fmlal2 %d31 %s31 %d15 $0x07 $0x01 -> %d31",
    };
    TEST_LOOP(fmlal2, fmlal2_vector_idx, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]),
              opnd_create_immed_uint(index_0_0[i], OPSZ_0));

    reg_id_t Rd_0_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_0_1[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_0_1[6] = { DR_REG_Q0,  DR_REG_Q4,  DR_REG_Q7,
                           DR_REG_Q10, DR_REG_Q12, DR_REG_Q15 };
    uint index_0_1[6] = { 0, 4, 5, 7, 0, 7 };
    const char *expected_0_1[6] = {
        "fmlal2 %q0 %d0 %q0 $0x00 $0x01 -> %q0",
        "fmlal2 %q5 %d6 %q4 $0x04 $0x01 -> %q5",
        "fmlal2 %q10 %d11 %q7 $0x05 $0x01 -> %q10",
        "fmlal2 %q16 %d17 %q10 $0x07 $0x01 -> %q16",
        "fmlal2 %q21 %d22 %q12 $0x00 $0x01 -> %q21",
        "fmlal2 %q31 %d31 %q15 $0x07 $0x01 -> %q31",
    };
    TEST_LOOP(fmlal2, fmlal2_vector_idx, 6, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]),
              opnd_create_immed_uint(index_0_1[i], OPSZ_0));
}

/*
 * FMLSL
 */

TEST_INSTR(fmlsl_vector)
{

    /* FMLSL <Vd>.<Ta>, <Vn>.<Tb>, <Vm>.<Tb> */

    /* FMLSL <Vd>.2S, <Vn>.2H, <Vm>.2H */
    reg_id_t Rd_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0[3] = { DR_REG_S1, DR_REG_S11, DR_REG_S30 };
    reg_id_t Rm_0[3] = { DR_REG_S2, DR_REG_S12, DR_REG_S29 };
    const char *expected_0[3] = {
        "fmlsl  %d0 %s1 %s2 $0x01 -> %d0",
        "fmlsl  %d10 %s11 %s12 $0x01 -> %d10",
        "fmlsl  %d31 %s30 %s29 $0x01 -> %d31",
    };
    TEST_LOOP(fmlsl, fmlsl_vector, 3, expected_0[i], opnd_create_reg(Rd_0[i]),
              opnd_create_reg(Rn_0[i]), opnd_create_reg(Rm_0[i]));

    /* FMLSL <Vd>.4S, <Vn>.4H, <Vm>.4H */
    reg_id_t Rd_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1[3] = { DR_REG_D1, DR_REG_D11, DR_REG_D30 };
    reg_id_t Rm_1[3] = { DR_REG_D2, DR_REG_D12, DR_REG_D29 };
    const char *expected_1[3] = {
        "fmlsl  %q0 %d1 %d2 $0x01 -> %q0",
        "fmlsl  %q10 %d11 %d12 $0x01 -> %q10",
        "fmlsl  %q31 %d30 %d29 $0x01 -> %q31",
    };
    TEST_LOOP(fmlsl, fmlsl_vector, 3, expected_1[i], opnd_create_reg(Rd_1[i]),
              opnd_create_reg(Rn_1[i]), opnd_create_reg(Rm_1[i]));
}

TEST_INSTR(fmlsl_vector_idx)
{

    /* FMLSL <Vd>.<Ta>, <Vn>.<Tb>, <Vm>.H[<index>] */
    reg_id_t Rd_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0[3] = { DR_REG_S2, DR_REG_S20, DR_REG_S30 };
    reg_id_t Rm_0[3] = { DR_REG_D0, DR_REG_D7, DR_REG_D15 };
    short index[3] = { 0, 5, 7 };
    const char *expected_0[3] = {
        "fmlsl  %d0 %s2 %d0 $0x0000000000000000 $0x01 -> %d0",
        "fmlsl  %d10 %s20 %d7 $0x0000000000000005 $0x01 -> %d10",
        "fmlsl  %d31 %s30 %d15 $0x0000000000000007 $0x01 -> %d31",
    };
    TEST_LOOP(fmlsl, fmlsl_vector_idx, 3, expected_0[i], opnd_create_reg(Rd_0[i]),
              opnd_create_reg(Rn_0[i]), opnd_create_reg(Rm_0[i]),
              OPND_CREATE_INT(index[i]));
}

/*
 * FMLSL2
 */

TEST_INSTR(fmlsl2_vector)
{

    /* FMLSL2 <Vd>.<Ta>, <Vn>.<Tb>, <Vm>.<Tb> */

    /* FMLSL2 <Vd>.2S, <Vn>.2H, <Vm>.2H */
    reg_id_t Rd_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0[3] = { DR_REG_S1, DR_REG_S11, DR_REG_S30 };
    reg_id_t Rm_0[3] = { DR_REG_S2, DR_REG_S12, DR_REG_S29 };
    const char *expected_0[3] = {
        "fmlsl2 %d0 %s1 %s2 $0x01 -> %d0",
        "fmlsl2 %d10 %s11 %s12 $0x01 -> %d10",
        "fmlsl2 %d31 %s30 %s29 $0x01 -> %d31",
    };
    TEST_LOOP(fmlsl2, fmlsl2_vector, 3, expected_0[i], opnd_create_reg(Rd_0[i]),
              opnd_create_reg(Rn_0[i]), opnd_create_reg(Rm_0[i]));

    /* FMLSL2 <Vd>.4S, <Vn>.4H, <Vm>.4H */
    reg_id_t Rd_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1[3] = { DR_REG_D1, DR_REG_D11, DR_REG_D30 };
    reg_id_t Rm_1[3] = { DR_REG_D2, DR_REG_D12, DR_REG_D29 };
    const char *expected_1[3] = {
        "fmlsl2 %q0 %d1 %d2 $0x01 -> %q0",
        "fmlsl2 %q10 %d11 %d12 $0x01 -> %q10",
        "fmlsl2 %q31 %d30 %d29 $0x01 -> %q31",
    };
    TEST_LOOP(fmlsl2, fmlsl2_vector, 3, expected_1[i], opnd_create_reg(Rd_1[i]),
              opnd_create_reg(Rn_1[i]), opnd_create_reg(Rm_1[i]));
}

TEST_INSTR(fmlsl2_vector_idx)
{

    /* FMLSL2 <Vd>.<Ta>, <Vn>.<Tb>, <Vm>.H[<index>] */
    reg_id_t Rd_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0[3] = { DR_REG_S2, DR_REG_S20, DR_REG_S30 };
    reg_id_t Rm_0[3] = { DR_REG_D0, DR_REG_D7, DR_REG_D15 };
    short index[3] = { 0, 5, 7 };
    const char *expected_0[3] = {
        "fmlsl2 %d0 %s2 %d0 $0x0000000000000000 $0x01 -> %d0",
        "fmlsl2 %d10 %s20 %d7 $0x0000000000000005 $0x01 -> %d10",
        "fmlsl2 %d31 %s30 %d15 $0x0000000000000007 $0x01 -> %d31",
    };
    TEST_LOOP(fmlsl2, fmlsl2_vector_idx, 3, expected_0[i], opnd_create_reg(Rd_0[i]),
              opnd_create_reg(Rn_0[i]), opnd_create_reg(Rm_0[i]),
              OPND_CREATE_INT(index[i]));
}

TEST_INSTR(sm3partw1_vector)
{

    /* Testing SM3PARTW1 <Sd>.4S, <Sn>.4S, <Sm>.4S */
    reg_id_t Rd_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_0[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_0[3] = { DR_REG_Q0, DR_REG_Q1, DR_REG_Q31 };
    opnd_t Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_0_0[3] = {
        "sm3partw1 %q0 %q0 $0x02 -> %q0",
        "sm3partw1 %q11 %q1 $0x02 -> %q10",
        "sm3partw1 %q31 %q31 $0x02 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sm3partw1_vector(dc, opnd_create_reg(Rd_0_0[i]),
                                              opnd_create_reg(Rn_0_0[i]),
                                              opnd_create_reg(Rm_0_0[i]), Rm_elsz);
        if (!test_instr_encoding(dc, OP_sm3partw1, instr, expected_0_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(sm3partw2_vector)
{

    /* Testing SM3PARTW2 <Sd>.4S, <Sn>.4S, <Sm>.4S */
    reg_id_t Rd_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_0[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_0[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    opnd_t Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_0_0[3] = {
        "sm3partw2 %q0 %q0 $0x02 -> %q0",
        "sm3partw2 %q11 %q12 $0x02 -> %q10",
        "sm3partw2 %q31 %q31 $0x02 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sm3partw2_vector(dc, opnd_create_reg(Rd_0_0[i]),
                                              opnd_create_reg(Rn_0_0[i]),
                                              opnd_create_reg(Rm_0_0[i]), Rm_elsz);
        if (!test_instr_encoding(dc, OP_sm3partw2, instr, expected_0_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(sm3ss1_vector)
{

    /* Testing SM3SS1  <Sd>.4S, <Sn>.4S, <Sm>.4S, <Sa>.4S */
    reg_id_t Rd_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_0[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_0[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    reg_id_t Ra_0_0[3] = { DR_REG_Q0, DR_REG_Q13, DR_REG_Q31 };
    opnd_t Ra_elsz = OPND_CREATE_SINGLE();
    const char *expected_0_0[3] = {
        "sm3ss1 %q0 %q0 %q0 $0x02 -> %q0",
        "sm3ss1 %q11 %q12 %q13 $0x02 -> %q10",
        "sm3ss1 %q31 %q31 %q31 $0x02 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sm3ss1_vector(
            dc, opnd_create_reg(Rd_0_0[i]), opnd_create_reg(Rn_0_0[i]),
            opnd_create_reg(Rm_0_0[i]), opnd_create_reg(Ra_0_0[i]), Ra_elsz);
        if (!test_instr_encoding(dc, OP_sm3ss1, instr, expected_0_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(sm3tt1a_vector_indexed)
{

    /* Testing SM3TT1A <Sd>.4S, <Sn>.4S, <Sm>.S[<index>] */
    reg_id_t Rd_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_0[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_0[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    uint imm2_0_0[3] = { 0, 1, 3 };
    opnd_t Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_0_0[3] = {
        "sm3tt1a %q0 %q0 $0x00 $0x02 -> %q0",
        "sm3tt1a %q11 %q12 $0x01 $0x02 -> %q10",
        "sm3tt1a %q31 %q31 $0x03 $0x02 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sm3tt1a_vector_indexed(
            dc, opnd_create_reg(Rd_0_0[i]), opnd_create_reg(Rn_0_0[i]),
            opnd_create_reg(Rm_0_0[i]), opnd_create_immed_uint(imm2_0_0[i], OPSZ_0),
            Rm_elsz);
        if (!test_instr_encoding(dc, OP_sm3tt1a, instr, expected_0_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(sm3tt1b_vector_indexed)
{

    /* Testing SM3TT1B <Sd>.4S, <Sn>.4S, <Sm>.S[<index>] */
    reg_id_t Rd_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_0[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_0[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    uint imm2_0_0[3] = { 0, 1, 3 };
    opnd_t Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_0_0[3] = {
        "sm3tt1b %q0 %q0 $0x00 $0x02 -> %q0",
        "sm3tt1b %q11 %q12 $0x01 $0x02 -> %q10",
        "sm3tt1b %q31 %q31 $0x03 $0x02 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sm3tt1b_vector_indexed(
            dc, opnd_create_reg(Rd_0_0[i]), opnd_create_reg(Rn_0_0[i]),
            opnd_create_reg(Rm_0_0[i]), opnd_create_immed_uint(imm2_0_0[i], OPSZ_0),
            Rm_elsz);
        if (!test_instr_encoding(dc, OP_sm3tt1b, instr, expected_0_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(sm3tt2a_vector_indexed)
{

    /* Testing SM3TT2A <Sd>.4S, <Sn>.4S, <Sm>.S[<index>] */
    reg_id_t Rd_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_0[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_0[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    uint imm2_0_0[3] = { 0, 1, 3 };
    opnd_t Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_0_0[3] = {
        "sm3tt2a %q0 %q0 $0x00 $0x02 -> %q0",
        "sm3tt2a %q11 %q12 $0x01 $0x02 -> %q10",
        "sm3tt2a %q31 %q31 $0x03 $0x02 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sm3tt2a_vector_indexed(
            dc, opnd_create_reg(Rd_0_0[i]), opnd_create_reg(Rn_0_0[i]),
            opnd_create_reg(Rm_0_0[i]), opnd_create_immed_uint(imm2_0_0[i], OPSZ_0),
            Rm_elsz);
        if (!test_instr_encoding(dc, OP_sm3tt2a, instr, expected_0_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(sm3tt2b_vector_indexed)
{

    /* Testing SM3TT2B <Sd>.4S, <Sn>.4S, <Sm>.S[<index>] */
    reg_id_t Rd_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_0[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_0[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    uint imm2_0_0[3] = { 0, 1, 3 };
    opnd_t Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_0_0[3] = {
        "sm3tt2b %q0 %q0 $0x00 $0x02 -> %q0",
        "sm3tt2b %q11 %q12 $0x01 $0x02 -> %q10",
        "sm3tt2b %q31 %q31 $0x03 $0x02 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sm3tt2b_vector_indexed(
            dc, opnd_create_reg(Rd_0_0[i]), opnd_create_reg(Rn_0_0[i]),
            opnd_create_reg(Rm_0_0[i]), opnd_create_immed_uint(imm2_0_0[i], OPSZ_0),
            Rm_elsz);
        if (!test_instr_encoding(dc, OP_sm3tt2b, instr, expected_0_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(sm4e_vector)
{

    /* Testing SM4E    <Sd>.4S, <Sn>.4S */
    reg_id_t Rd_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_0[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    opnd_t Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_0_0[3] = {
        "sm4e   %q0 $0x02 -> %q0",
        "sm4e   %q11 $0x02 -> %q10",
        "sm4e   %q31 $0x02 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sm4e_vector(dc, opnd_create_reg(Rd_0_0[i]),
                                         opnd_create_reg(Rn_0_0[i]), Rn_elsz);
        if (!test_instr_encoding(dc, OP_sm4e, instr, expected_0_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(sm4ekey_vector)
{

    /* Testing SM4EKEY <Sd>.4S, <Sn>.4S, <Sm>.4S */
    reg_id_t Rd_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_0[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_0[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    opnd_t Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_0_0[3] = {
        "sm4ekey %q0 %q0 $0x02 -> %q0",
        "sm4ekey %q11 %q12 $0x02 -> %q10",
        "sm4ekey %q31 %q31 $0x02 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sm4ekey_vector(dc, opnd_create_reg(Rd_0_0[i]),
                                            opnd_create_reg(Rn_0_0[i]),
                                            opnd_create_reg(Rm_0_0[i]), Rm_elsz);
        if (!test_instr_encoding(dc, OP_sm4ekey, instr, expected_0_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(sha512h)
{

    /* Testing SHA512H <Qd>, <Qn>, <Dm>.2D */
    reg_id_t Rd_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rm_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    opnd_t Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_0_0[3] = {
        "sha512h %q0 %q0 %q0 $0x03 -> %q0",
        "sha512h %q10 %q10 %q10 $0x03 -> %q10",
        "sha512h %q31 %q31 %q31 $0x03 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sha512h(dc, opnd_create_reg(Rd_0_0[i]),
                                     opnd_create_reg(Rn_0_0[i]),
                                     opnd_create_reg(Rm_0_0[i]), Rm_elsz);
        if (!test_instr_encoding(dc, OP_sha512h, instr, expected_0_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(sha512h2)
{

    /* Testing SHA512H2 <Qd>, <Qn>, <Dm>.2D */
    reg_id_t Rd_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rm_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    opnd_t Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_0_0[3] = {
        "sha512h2 %q0 %q0 %q0 $0x03 -> %q0",
        "sha512h2 %q10 %q10 %q10 $0x03 -> %q10",
        "sha512h2 %q31 %q31 %q31 $0x03 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sha512h2(dc, opnd_create_reg(Rd_0_0[i]),
                                      opnd_create_reg(Rn_0_0[i]),
                                      opnd_create_reg(Rm_0_0[i]), Rm_elsz);
        if (!test_instr_encoding(dc, OP_sha512h2, instr, expected_0_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(sha512su0)
{

    /* Testing SHA512SU0 <Dd>.2D, <Dn>.2D */
    reg_id_t Rd_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    opnd_t Rn_elsz = OPND_CREATE_DOUBLE();
    const char *expected_0_0[3] = {
        "sha512su0 %q0 %q0 $0x03 -> %q0",
        "sha512su0 %q10 %q10 $0x03 -> %q10",
        "sha512su0 %q31 %q31 $0x03 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sha512su0(dc, opnd_create_reg(Rd_0_0[i]),
                                       opnd_create_reg(Rn_0_0[i]), Rn_elsz);
        if (!test_instr_encoding(dc, OP_sha512su0, instr, expected_0_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(sha512su1)
{

    /* Testing SHA512SU1 <Dd>.2D, <Dn>.2D, <Dm>.2D */
    reg_id_t Rd_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rm_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    opnd_t Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_0_0[3] = {
        "sha512su1 %q0 %q0 %q0 $0x03 -> %q0",
        "sha512su1 %q10 %q10 %q10 $0x03 -> %q10",
        "sha512su1 %q31 %q31 %q31 $0x03 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sha512su1(dc, opnd_create_reg(Rd_0_0[i]),
                                       opnd_create_reg(Rn_0_0[i]),
                                       opnd_create_reg(Rm_0_0[i]), Rm_elsz);
        if (!test_instr_encoding(dc, OP_sha512su1, instr, expected_0_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(bcax)
{

    /* Testing BCAX    <Bd>.16B, <Bn>.16B, <Bm>.16B, <Ba>.16B */
    reg_id_t Rd_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_0[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_0[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    reg_id_t Ra_0_0[3] = { DR_REG_Q0, DR_REG_Q13, DR_REG_Q31 };
    const char *expected_0_0[3] = {
        "bcax   %q0 %q0 %q0 $0x00 -> %q0",
        "bcax   %q11 %q12 %q13 $0x00 -> %q10",
        "bcax   %q31 %q31 %q31 $0x00 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr =
            INSTR_CREATE_bcax(dc, opnd_create_reg(Rd_0_0[i]), opnd_create_reg(Rn_0_0[i]),
                              opnd_create_reg(Rm_0_0[i]), opnd_create_reg(Ra_0_0[i]));
        if (!test_instr_encoding(dc, OP_bcax, instr, expected_0_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(eor3)
{

    /* Testing EOR3    <Bd>.16B, <Bn>.16B, <Bm>.16B, <Ba>.16B */
    reg_id_t Rd_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_0[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_0[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    reg_id_t Ra_0_0[3] = { DR_REG_Q0, DR_REG_Q13, DR_REG_Q31 };
    const char *expected_0_0[3] = {
        "eor3   %q0 %q0 %q0 $0x00 -> %q0",
        "eor3   %q11 %q12 %q13 $0x00 -> %q10",
        "eor3   %q31 %q31 %q31 $0x00 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr =
            INSTR_CREATE_eor3(dc, opnd_create_reg(Rd_0_0[i]), opnd_create_reg(Rn_0_0[i]),
                              opnd_create_reg(Rm_0_0[i]), opnd_create_reg(Ra_0_0[i]));
        if (!test_instr_encoding(dc, OP_eor3, instr, expected_0_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(esb)
{

    /* Testing ESB      */
    const char *expected_0_0[1] = { "esb" };
    instr = INSTR_CREATE_esb(dc);
    if (!test_instr_encoding(dc, OP_esb, instr, expected_0_0[0]))
        *psuccess = false;
}

TEST_INSTR(psb)
{

    /* Testing PSB      */
    const char *expected_0_0[1] = { "psb" };
    instr = INSTR_CREATE_psb_csync(dc);
    if (!test_instr_encoding(dc, OP_psb, instr, expected_0_0[0]))
        *psuccess = false;
}

TEST_INSTR(fsqrt_vector)
{

    /* Testing FSQRT   <Hd>.<Ts>, <Hn>.<Ts> */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    opnd_t Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[3] = {
        "fsqrt  %d0 $0x01 -> %d0",
        "fsqrt  %d11 $0x01 -> %d10",
        "fsqrt  %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fsqrt_vector(dc, opnd_create_reg(Rd_0_0[i]),
                                          opnd_create_reg(Rn_0_0[i]), Rn_elsz);
        if (!test_instr_encoding(dc, OP_fsqrt, instr, expected_0_0[i]))
            *psuccess = false;
    }
    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[3] = {
        "fsqrt  %q0 $0x01 -> %q0",
        "fsqrt  %q11 $0x01 -> %q10",
        "fsqrt  %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fsqrt_vector(dc, opnd_create_reg(Rd_0_1[i]),
                                          opnd_create_reg(Rn_0_1[i]), Rn_elsz);
        if (!test_instr_encoding(dc, OP_fsqrt, instr, expected_0_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(fsqrt_scalar)
{

    /* Testing FSQRT   <Hd>, <Hn> */
    reg_id_t Rd_1_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_1_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    const char *expected_1_0[3] = {
        "fsqrt  %h0 -> %h0",
        "fsqrt  %h11 -> %h10",
        "fsqrt  %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fsqrt_scalar(dc, opnd_create_reg(Rd_1_0[i]),
                                          opnd_create_reg(Rn_1_0[i]));
        if (!test_instr_encoding(dc, OP_fsqrt, instr, expected_1_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(scvtf_vector)
{

    /* Testing SCVTF   <Hd>.<Ts>, <Hn>.<Ts> */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    opnd_t Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[3] = {
        "scvtf  %d0 $0x01 -> %d0",
        "scvtf  %d11 $0x01 -> %d10",
        "scvtf  %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_scvtf_vector(dc, opnd_create_reg(Rd_0_0[i]),
                                          opnd_create_reg(Rn_0_0[i]), Rn_elsz);
        if (!test_instr_encoding(dc, OP_scvtf, instr, expected_0_0[i]))
            *psuccess = false;
    }
    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[3] = {
        "scvtf  %q0 $0x01 -> %q0",
        "scvtf  %q11 $0x01 -> %q10",
        "scvtf  %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_scvtf_vector(dc, opnd_create_reg(Rd_0_1[i]),
                                          opnd_create_reg(Rn_0_1[i]), Rn_elsz);
        if (!test_instr_encoding(dc, OP_scvtf, instr, expected_0_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(scvtf_scalar)
{

    /* Testing SCVTF   <Hd>, <Wn> */
    reg_id_t Rd_0_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0_0[3] = { DR_REG_W0, DR_REG_W11, DR_REG_W30 };
    const char *expected_0_0[3] = {
        "scvtf  %w0 -> %h0",
        "scvtf  %w11 -> %h10",
        "scvtf  %w30 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_scvtf_scalar(dc, opnd_create_reg(Rd_0_0[i]),
                                          opnd_create_reg(Rn_0_0[i]));
        if (!test_instr_encoding(dc, OP_scvtf, instr, expected_0_0[i]))
            *psuccess = false;
    }

    /* Testing SCVTF   <Hd>, <Xn> */
    reg_id_t Rd_1_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_1_0[3] = { DR_REG_X0, DR_REG_X11, DR_REG_X30 };
    const char *expected_1_0[3] = {
        "scvtf  %x0 -> %h0",
        "scvtf  %x11 -> %h10",
        "scvtf  %x30 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_scvtf_scalar(dc, opnd_create_reg(Rd_1_0[i]),
                                          opnd_create_reg(Rn_1_0[i]));
        if (!test_instr_encoding(dc, OP_scvtf, instr, expected_1_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(scvtf_scalar_fixed)
{

    /* Testing SCVTF   <Hd>, <Wn>, #<imm> */
    reg_id_t Rd_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0[3] = { DR_REG_W0, DR_REG_W11, DR_REG_W30 };
    uint scale_0[3] = { 32, 22, 1 };
    const char *expected_0[3] = {
        "scvtf  %w0 $0x0000000000000020 -> %h0",
        "scvtf  %w11 $0x0000000000000016 -> %h10",
        "scvtf  %w30 $0x0000000000000001 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(Rd_0[i]),
                                                opnd_create_reg(Rn_0[i]),
                                                OPND_CREATE_INT(scale_0[i]));
        if (!test_instr_encoding(dc, OP_scvtf, instr, expected_0[i]))
            *psuccess = false;
    }

    /* Testing SCVTF   <Hd>, <Xn>, #<imm> */
    reg_id_t Rd_1[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_1[3] = { DR_REG_X0, DR_REG_X11, DR_REG_X30 };
    uint scale_1[3] = { 64, 43, 1 };
    const char *expected_1[3] = {
        "scvtf  %x0 $0x0000000000000040 -> %h0",
        "scvtf  %x11 $0x000000000000002b -> %h10",
        "scvtf  %x30 $0x0000000000000001 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(Rd_1[i]),
                                                opnd_create_reg(Rn_1[i]),
                                                OPND_CREATE_INT(scale_1[i]));
        if (!test_instr_encoding(dc, OP_scvtf, instr, expected_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(ucvtf_vector)
{

    /* Testing UCVTF   <Hd>.<Ts>, <Hn>.<Ts> */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    opnd_t Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[3] = {
        "ucvtf  %d0 $0x01 -> %d0",
        "ucvtf  %d11 $0x01 -> %d10",
        "ucvtf  %d31 $0x01 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_ucvtf_vector(dc, opnd_create_reg(Rd_0_0[i]),
                                          opnd_create_reg(Rn_0_0[i]), Rn_elsz);
        if (!test_instr_encoding(dc, OP_ucvtf, instr, expected_0_0[i]))
            *psuccess = false;
    }
    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[3] = {
        "ucvtf  %q0 $0x01 -> %q0",
        "ucvtf  %q11 $0x01 -> %q10",
        "ucvtf  %q31 $0x01 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_ucvtf_vector(dc, opnd_create_reg(Rd_0_1[i]),
                                          opnd_create_reg(Rn_0_1[i]), Rn_elsz);
        if (!test_instr_encoding(dc, OP_ucvtf, instr, expected_0_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(ucvtf_scalar)
{

    /* Testing UCVTF   <Hd>, <Wn> */
    reg_id_t Rd_0_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0_0[3] = { DR_REG_W0, DR_REG_W11, DR_REG_W30 };
    const char *expected_0_0[3] = {
        "ucvtf  %w0 -> %h0",
        "ucvtf  %w11 -> %h10",
        "ucvtf  %w30 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_ucvtf_scalar(dc, opnd_create_reg(Rd_0_0[i]),
                                          opnd_create_reg(Rn_0_0[i]));
        if (!test_instr_encoding(dc, OP_ucvtf, instr, expected_0_0[i]))
            *psuccess = false;
    }

    /* Testing UCVTF   <Hd>, <Xn> */
    reg_id_t Rd_1_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_1_0[3] = { DR_REG_X0, DR_REG_X11, DR_REG_X30 };
    const char *expected_1_0[3] = {
        "ucvtf  %x0 -> %h0",
        "ucvtf  %x11 -> %h10",
        "ucvtf  %x30 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_ucvtf_scalar(dc, opnd_create_reg(Rd_1_0[i]),
                                          opnd_create_reg(Rn_1_0[i]));
        if (!test_instr_encoding(dc, OP_ucvtf, instr, expected_1_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(ucvtf_scalar_fixed)
{

    /* Testing UCVTF   <Hd>, <Wn>, #<imm> */
    reg_id_t Rd_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0[3] = { DR_REG_W0, DR_REG_W11, DR_REG_W30 };
    uint scale_0[3] = { 32, 22, 1 };
    const char *expected_0[3] = {
        "ucvtf  %w0 $0x0000000000000020 -> %h0",
        "ucvtf  %w11 $0x0000000000000016 -> %h10",
        "ucvtf  %w30 $0x0000000000000001 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(Rd_0[i]),
                                                opnd_create_reg(Rn_0[i]),
                                                OPND_CREATE_INT(scale_0[i]));
        if (!test_instr_encoding(dc, OP_ucvtf, instr, expected_0[i]))
            *psuccess = false;
    }

    /* Testing UCVTF   <Hd>, <Xn>, #<imm> */
    reg_id_t Rd_1[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_1[3] = { DR_REG_X0, DR_REG_X11, DR_REG_X30 };
    uint scale_1[3] = { 64, 43, 1 };
    const char *expected_1[3] = {
        "ucvtf  %x0 $0x0000000000000040 -> %h0",
        "ucvtf  %x11 $0x000000000000002b -> %h10",
        "ucvtf  %x30 $0x0000000000000001 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(Rd_1[i]),
                                                opnd_create_reg(Rn_1[i]),
                                                OPND_CREATE_INT(scale_1[i]));
        if (!test_instr_encoding(dc, OP_ucvtf, instr, expected_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(rax1)
{

    /* Testing RAX1    <Dd>.2D, <Dn>.2D, <Dm>.2D */
    reg_id_t Rd_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0[3] = { DR_REG_Q1, DR_REG_Q11, DR_REG_Q30 };
    reg_id_t Rm_0[3] = { DR_REG_Q2, DR_REG_Q12, DR_REG_Q29 };
    const char *expected_0[3] = {
        "rax1   %q1 %q2 $0x03 -> %q0",
        "rax1   %q11 %q12 $0x03 -> %q10",
        "rax1   %q30 %q29 $0x03 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_rax1(dc, opnd_create_reg(Rd_0[i]), opnd_create_reg(Rn_0[i]),
                                  opnd_create_reg(Rm_0[i]));
        if (!test_instr_encoding(dc, OP_rax1, instr, expected_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(xar)
{

    /* Testing XAR     <Dd>.2D, <Dn>.2D, <Dm>.2D, #<imm> */
    reg_id_t Rd_0_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_0[3] = { DR_REG_Q1, DR_REG_Q11, DR_REG_Q30 };
    reg_id_t Rm_0_0[3] = { DR_REG_Q2, DR_REG_Q12, DR_REG_Q29 };
    uint imm6_0_0[3] = { 0, 21, 63 };
    const char *expected_0_0[3] = {
        "xar    %q1 %q2 $0x00 $0x03 -> %q0",
        "xar    %q11 %q12 $0x15 $0x03 -> %q10",
        "xar    %q30 %q29 $0x3f $0x03 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_xar(dc, opnd_create_reg(Rd_0_0[i]),
                                 opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]),
                                 opnd_create_immed_uint(imm6_0_0[i], OPSZ_0));
        if (!test_instr_encoding(dc, OP_xar, instr, expected_0_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(fccmp)
{
    /* Testing FCCMP   <Dn>, <Dm>, #<imm>, <cond> */
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rm_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    uint nzcv_0_0[3] = { 0, 7, 15 };
    dr_pred_type_t condition_code_0_0[3] = { DR_PRED_EQ, DR_PRED_HI, DR_PRED_NV };
    const char *expected_0_0[3] = {
        "fccmp.eq %d0 %d0 $0x00",
        "fccmp.hi %d10 %d11 $0x07",
        "fccmp.nv %d31 %d31 $0x0f",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fccmp(
            dc, opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]),
            opnd_create_immed_uint(nzcv_0_0[i], OPSZ_0), condition_code_0_0[i]);
        if (!test_instr_encoding(dc, OP_fccmp, instr, expected_0_0[i]))
            *psuccess = false;
    }
    /* Testing FCCMP   <Hn>, <Hm>, #<imm>, <cond> */
    reg_id_t Rn_1_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rm_1_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    uint nzcv_1_0[3] = { 0, 7, 15 };
    dr_pred_type_t condition_code_1_0[3] = { DR_PRED_EQ, DR_PRED_HI, DR_PRED_NV };
    const char *expected_1_0[3] = {
        "fccmp.eq %h0 %h0 $0x00",
        "fccmp.hi %h10 %h11 $0x07",
        "fccmp.nv %h31 %h31 $0x0f",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fccmp(
            dc, opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]),
            opnd_create_immed_uint(nzcv_1_0[i], OPSZ_0), condition_code_1_0[i]);
        if (!test_instr_encoding(dc, OP_fccmp, instr, expected_1_0[i]))
            *psuccess = false;
    }
    /* Testing FCCMP   <Sn>, <Sm>, #<imm>, <cond> */
    reg_id_t Rn_2_0[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rm_2_0[3] = { DR_REG_S0, DR_REG_S11, DR_REG_S31 };
    uint nzcv_2_0[3] = { 0, 7, 15 };
    dr_pred_type_t condition_code_2_0[3] = { DR_PRED_EQ, DR_PRED_HI, DR_PRED_NV };
    const char *expected_2_0[3] = {
        "fccmp.eq %s0 %s0 $0x00",
        "fccmp.hi %s10 %s11 $0x07",
        "fccmp.nv %s31 %s31 $0x0f",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fccmp(
            dc, opnd_create_reg(Rn_2_0[i]), opnd_create_reg(Rm_2_0[i]),
            opnd_create_immed_uint(nzcv_2_0[i], OPSZ_0), condition_code_2_0[i]);
        if (!test_instr_encoding(dc, OP_fccmp, instr, expected_2_0[i]))
            *psuccess = false;
    }
}
TEST_INSTR(fccmpe)
{
    /* Testing FCCMPE  <Dn>, <Dm>, #<imm>, <cond> */
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rm_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    uint nzcv_0_0[3] = { 0, 7, 15 };
    dr_pred_type_t condition_code_0_0[3] = { DR_PRED_EQ, DR_PRED_HI, DR_PRED_NV };
    const char *expected_0_0[3] = {
        "fccmpe.eq %d0 %d0 $0x00",
        "fccmpe.hi %d10 %d11 $0x07",
        "fccmpe.nv %d31 %d31 $0x0f",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fccmpe(
            dc, opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]),
            opnd_create_immed_uint(nzcv_0_0[i], OPSZ_0), condition_code_0_0[i]);
        if (!test_instr_encoding(dc, OP_fccmpe, instr, expected_0_0[i]))
            *psuccess = false;
    }
    /* Testing FCCMPE  <Hn>, <Hm>, #<imm>, <cond> */
    reg_id_t Rn_1_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rm_1_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    uint nzcv_1_0[3] = { 0, 7, 15 };
    dr_pred_type_t condition_code_1_0[3] = { DR_PRED_EQ, DR_PRED_HI, DR_PRED_NV };
    const char *expected_1_0[3] = {
        "fccmpe.eq %h0 %h0 $0x00",
        "fccmpe.hi %h10 %h11 $0x07",
        "fccmpe.nv %h31 %h31 $0x0f",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fccmpe(
            dc, opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]),
            opnd_create_immed_uint(nzcv_1_0[i], OPSZ_0), condition_code_1_0[i]);
        if (!test_instr_encoding(dc, OP_fccmpe, instr, expected_1_0[i]))
            *psuccess = false;
    }
    /* Testing FCCMPE  <Sn>, <Sm>, #<imm>, <cond> */
    reg_id_t Rn_2_0[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rm_2_0[3] = { DR_REG_S0, DR_REG_S11, DR_REG_S31 };
    uint nzcv_2_0[3] = { 0, 7, 15 };
    dr_pred_type_t condition_code_2_0[3] = { DR_PRED_EQ, DR_PRED_HI, DR_PRED_NV };
    const char *expected_2_0[3] = {
        "fccmpe.eq %s0 %s0 $0x00",
        "fccmpe.hi %s10 %s11 $0x07",
        "fccmpe.nv %s31 %s31 $0x0f",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fccmpe(
            dc, opnd_create_reg(Rn_2_0[i]), opnd_create_reg(Rm_2_0[i]),
            opnd_create_immed_uint(nzcv_2_0[i], OPSZ_0), condition_code_2_0[i]);
        if (!test_instr_encoding(dc, OP_fccmpe, instr, expected_2_0[i]))
            *psuccess = false;
    }
}
TEST_INSTR(fcmp)
{
    /* Testing FCMP    <Dn>, #0.0 */
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    const char *expected_0_0[3] = {
        "fcmp   %d0 $0.000000",
        "fcmp   %d10 $0.000000",
        "fcmp   %d31 $0.000000",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcmp_zero(dc, opnd_create_reg(Rn_0_0[i]));
        if (!test_instr_encoding(dc, OP_fcmp, instr, expected_0_0[i]))
            *psuccess = false;
    }
    /* Testing FCMP    <Hn>, #0.0 */
    reg_id_t Rn_1_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_1_0[3] = {
        "fcmp   %h0 $0.000000",
        "fcmp   %h10 $0.000000",
        "fcmp   %h31 $0.000000",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcmp_zero(dc, opnd_create_reg(Rn_1_0[i]));
        if (!test_instr_encoding(dc, OP_fcmp, instr, expected_1_0[i]))
            *psuccess = false;
    }
    /* Testing FCMP    <Sn>, #0.0 */
    reg_id_t Rn_2_0[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    const char *expected_2_0[3] = {
        "fcmp   %s0 $0.000000",
        "fcmp   %s10 $0.000000",
        "fcmp   %s31 $0.000000",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcmp_zero(dc, opnd_create_reg(Rn_2_0[i]));
        if (!test_instr_encoding(dc, OP_fcmp, instr, expected_2_0[i]))
            *psuccess = false;
    }
    /* Testing FCMP    <Dn>, <Dm> */
    reg_id_t Rn_3_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rm_3_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    const char *expected_3_0[3] = {
        "fcmp   %d0 %d0",
        "fcmp   %d10 %d11",
        "fcmp   %d31 %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr =
            INSTR_CREATE_fcmp(dc, opnd_create_reg(Rn_3_0[i]), opnd_create_reg(Rm_3_0[i]));
        if (!test_instr_encoding(dc, OP_fcmp, instr, expected_3_0[i]))
            *psuccess = false;
    }
    /* Testing FCMP    <Hn>, <Hm> */
    reg_id_t Rn_4_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rm_4_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    const char *expected_4_0[3] = {
        "fcmp   %h0 %h0",
        "fcmp   %h10 %h11",
        "fcmp   %h31 %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr =
            INSTR_CREATE_fcmp(dc, opnd_create_reg(Rn_4_0[i]), opnd_create_reg(Rm_4_0[i]));
        if (!test_instr_encoding(dc, OP_fcmp, instr, expected_4_0[i]))
            *psuccess = false;
    }
    /* Testing FCMP    <Sn>, <Sm> */
    reg_id_t Rn_5_0[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rm_5_0[3] = { DR_REG_S0, DR_REG_S11, DR_REG_S31 };
    const char *expected_5_0[3] = {
        "fcmp   %s0 %s0",
        "fcmp   %s10 %s11",
        "fcmp   %s31 %s31",
    };
    for (int i = 0; i < 3; i++) {
        instr =
            INSTR_CREATE_fcmp(dc, opnd_create_reg(Rn_5_0[i]), opnd_create_reg(Rm_5_0[i]));
        if (!test_instr_encoding(dc, OP_fcmp, instr, expected_5_0[i]))
            *psuccess = false;
    }
}
TEST_INSTR(fcmpe)
{
    /* Testing FCMPE   <Dn>, #0.0 */
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    const char *expected_0_0[3] = {
        "fcmpe  %d0 $0.000000",
        "fcmpe  %d10 $0.000000",
        "fcmpe  %d31 $0.000000",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcmpe_zero(dc, opnd_create_reg(Rn_0_0[i]));
        if (!test_instr_encoding(dc, OP_fcmpe, instr, expected_0_0[i]))
            *psuccess = false;
    }
    /* Testing FCMPE   <Hn>, #0.0 */
    reg_id_t Rn_1_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    const char *expected_1_0[3] = {
        "fcmpe  %h0 $0.000000",
        "fcmpe  %h10 $0.000000",
        "fcmpe  %h31 $0.000000",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcmpe_zero(dc, opnd_create_reg(Rn_1_0[i]));
        if (!test_instr_encoding(dc, OP_fcmpe, instr, expected_1_0[i]))
            *psuccess = false;
    }
    /* Testing FCMPE   <Sn>, #0.0 */
    reg_id_t Rn_2_0[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    const char *expected_2_0[3] = {
        "fcmpe  %s0 $0.000000",
        "fcmpe  %s10 $0.000000",
        "fcmpe  %s31 $0.000000",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcmpe_zero(dc, opnd_create_reg(Rn_2_0[i]));
        if (!test_instr_encoding(dc, OP_fcmpe, instr, expected_2_0[i]))
            *psuccess = false;
    }
    /* Testing FCMPE   <Dn>, <Dm> */
    reg_id_t Rn_3_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rm_3_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    const char *expected_3_0[3] = {
        "fcmpe  %d0 %d0",
        "fcmpe  %d10 %d11",
        "fcmpe  %d31 %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcmpe(dc, opnd_create_reg(Rn_3_0[i]),
                                   opnd_create_reg(Rm_3_0[i]));
        if (!test_instr_encoding(dc, OP_fcmpe, instr, expected_3_0[i]))
            *psuccess = false;
    }
    /* Testing FCMPE   <Hn>, <Hm> */
    reg_id_t Rn_4_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rm_4_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    const char *expected_4_0[3] = {
        "fcmpe  %h0 %h0",
        "fcmpe  %h10 %h11",
        "fcmpe  %h31 %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcmpe(dc, opnd_create_reg(Rn_4_0[i]),
                                   opnd_create_reg(Rm_4_0[i]));
        if (!test_instr_encoding(dc, OP_fcmpe, instr, expected_4_0[i]))
            *psuccess = false;
    }
    /* Testing FCMPE   <Sn>, <Sm> */
    reg_id_t Rn_5_0[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rm_5_0[3] = { DR_REG_S0, DR_REG_S11, DR_REG_S31 };
    const char *expected_5_0[3] = {
        "fcmpe  %s0 %s0",
        "fcmpe  %s10 %s11",
        "fcmpe  %s31 %s31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_fcmpe(dc, opnd_create_reg(Rn_5_0[i]),
                                   opnd_create_reg(Rm_5_0[i]));
        if (!test_instr_encoding(dc, OP_fcmpe, instr, expected_5_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(fcsel)
{

    /* Testing FCSEL   <Dd>, <Dn>, <Dm>, <cond> */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_0_0[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    dr_pred_type_t condition_code_0_0[3] = { DR_PRED_EQ, DR_PRED_HI, DR_PRED_NV };
    const char *expected_0_0[3] = {
        "fcsel.eq %d0 %d0 -> %d0",
        "fcsel.hi %d11 %d12 -> %d10",
        "fcsel.nv %d31 %d31 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr =
            INSTR_CREATE_fcsel(dc, opnd_create_reg(Rd_0_0[i]), opnd_create_reg(Rn_0_0[i]),
                               opnd_create_reg(Rm_0_0[i]), condition_code_0_0[i]);
        if (!test_instr_encoding(dc, OP_fcsel, instr, expected_0_0[i]))
            *psuccess = false;
    }

    /* Testing FCSEL   <Hd>, <Hn>, <Hm>, <cond> */
    reg_id_t Rd_1_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_1_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    reg_id_t Rm_1_0[3] = { DR_REG_H0, DR_REG_H12, DR_REG_H31 };
    dr_pred_type_t condition_code_1_0[3] = { DR_PRED_EQ, DR_PRED_HI, DR_PRED_NV };
    const char *expected_1_0[3] = {
        "fcsel.eq %h0 %h0 -> %h0",
        "fcsel.hi %h11 %h12 -> %h10",
        "fcsel.nv %h31 %h31 -> %h31",
    };
    for (int i = 0; i < 3; i++) {
        instr =
            INSTR_CREATE_fcsel(dc, opnd_create_reg(Rd_1_0[i]), opnd_create_reg(Rn_1_0[i]),
                               opnd_create_reg(Rm_1_0[i]), condition_code_1_0[i]);
        if (!test_instr_encoding(dc, OP_fcsel, instr, expected_1_0[i]))
            *psuccess = false;
    }

    /* Testing FCSEL   <Sd>, <Sn>, <Sm>, <cond> */
    reg_id_t Rd_2_0[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rn_2_0[3] = { DR_REG_S0, DR_REG_S11, DR_REG_S31 };
    reg_id_t Rm_2_0[3] = { DR_REG_S0, DR_REG_S12, DR_REG_S31 };
    dr_pred_type_t condition_code_2_0[3] = { DR_PRED_EQ, DR_PRED_HI, DR_PRED_NV };
    const char *expected_2_0[3] = {
        "fcsel.eq %s0 %s0 -> %s0",
        "fcsel.hi %s11 %s12 -> %s10",
        "fcsel.nv %s31 %s31 -> %s31",
    };
    for (int i = 0; i < 3; i++) {
        instr =
            INSTR_CREATE_fcsel(dc, opnd_create_reg(Rd_2_0[i]), opnd_create_reg(Rn_2_0[i]),
                               opnd_create_reg(Rm_2_0[i]), condition_code_2_0[i]);
        if (!test_instr_encoding(dc, OP_fcsel, instr, expected_2_0[i]))
            *psuccess = false;
    }
}

TEST_INSTR(sdot_vector)
{

    /* Testing SDOT    <Sd>.<Ts>, <Bn>.<Tb>, <Bm>.<Tb> */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_0_0[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    const char *expected_0_0[3] = {
        "sdot   %d0 %d0 %d0 $0x00 -> %d0",
        "sdot   %d10 %d11 %d12 $0x00 -> %d10",
        "sdot   %d31 %d31 %d31 $0x00 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sdot_vector(dc, opnd_create_reg(Rd_0_0[i]),
                                         opnd_create_reg(Rn_0_0[i]),
                                         opnd_create_reg(Rm_0_0[i]));
        if (!test_instr_encoding(dc, OP_sdot, instr, expected_0_0[i]))
            *psuccess = false;
    }
    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_1[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    const char *expected_0_1[3] = {
        "sdot   %q0 %q0 %q0 $0x00 -> %q0",
        "sdot   %q10 %q11 %q12 $0x00 -> %q10",
        "sdot   %q31 %q31 %q31 $0x00 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sdot_vector(dc, opnd_create_reg(Rd_0_1[i]),
                                         opnd_create_reg(Rn_0_1[i]),
                                         opnd_create_reg(Rm_0_1[i]));
        if (!test_instr_encoding(dc, OP_sdot, instr, expected_0_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(sdot_vector_indexed)
{

    /* Testing SDOT    <Sd>.<Ts>, <Bn>.<Tb>, <Bm>.4B[<index>] */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_0_0[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    uint index_0_0[3] = { 0, 0, 3 };
    const char *expected_0_0[3] = {
        "sdot   %d0 %d0 %q0 $0x00 $0x00 -> %d0",
        "sdot   %d10 %d11 %q12 $0x00 $0x00 -> %d10",
        "sdot   %d31 %d31 %q31 $0x03 $0x00 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sdot_vector_indexed(
            dc, opnd_create_reg(Rd_0_0[i]), opnd_create_reg(Rn_0_0[i]),
            opnd_create_reg(Rm_0_0[i]), opnd_create_immed_uint(index_0_0[i], OPSZ_0));
        if (!test_instr_encoding(dc, OP_sdot, instr, expected_0_0[i]))
            *psuccess = false;
    }
    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_1[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    uint index_0_1[3] = { 0, 0, 3 };
    const char *expected_0_1[3] = {
        "sdot   %q0 %q0 %q0 $0x00 $0x00 -> %q0",
        "sdot   %q10 %q11 %q12 $0x00 $0x00 -> %q10",
        "sdot   %q31 %q31 %q31 $0x03 $0x00 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_sdot_vector_indexed(
            dc, opnd_create_reg(Rd_0_1[i]), opnd_create_reg(Rn_0_1[i]),
            opnd_create_reg(Rm_0_1[i]), opnd_create_immed_uint(index_0_1[i], OPSZ_0));
        if (!test_instr_encoding(dc, OP_sdot, instr, expected_0_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(udot_vector)
{

    /* Testing UDOT    <Sd>.<Ts>, <Bn>.<Tb>, <Bm>.<Tb> */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_0_0[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    const char *expected_0_0[3] = {
        "udot   %d0 %d0 %d0 $0x00 -> %d0",
        "udot   %d10 %d11 %d12 $0x00 -> %d10",
        "udot   %d31 %d31 %d31 $0x00 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_udot_vector(dc, opnd_create_reg(Rd_0_0[i]),
                                         opnd_create_reg(Rn_0_0[i]),
                                         opnd_create_reg(Rm_0_0[i]));
        if (!test_instr_encoding(dc, OP_udot, instr, expected_0_0[i]))
            *psuccess = false;
    }
    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_1[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    const char *expected_0_1[3] = {
        "udot   %q0 %q0 %q0 $0x00 -> %q0",
        "udot   %q10 %q11 %q12 $0x00 -> %q10",
        "udot   %q31 %q31 %q31 $0x00 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_udot_vector(dc, opnd_create_reg(Rd_0_1[i]),
                                         opnd_create_reg(Rn_0_1[i]),
                                         opnd_create_reg(Rm_0_1[i]));
        if (!test_instr_encoding(dc, OP_udot, instr, expected_0_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(udot_vector_indexed)
{

    /* Testing UDOT    <Sd>.<Ts>, <Bn>.<Tb>, <Bm>.4B[<index>] */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_0_0[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    uint index_0_0[3] = { 0, 0, 3 };
    const char *expected_0_0[3] = {
        "udot   %d0 %d0 %q0 $0x00 $0x00 -> %d0",
        "udot   %d10 %d11 %q12 $0x00 $0x00 -> %d10",
        "udot   %d31 %d31 %q31 $0x03 $0x00 -> %d31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_udot_vector_indexed(
            dc, opnd_create_reg(Rd_0_0[i]), opnd_create_reg(Rn_0_0[i]),
            opnd_create_reg(Rm_0_0[i]), opnd_create_immed_uint(index_0_0[i], OPSZ_0));
        if (!test_instr_encoding(dc, OP_udot, instr, expected_0_0[i]))
            *psuccess = false;
    }
    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_1[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    uint index_0_1[3] = { 0, 0, 3 };
    const char *expected_0_1[3] = {
        "udot   %q0 %q0 %q0 $0x00 $0x00 -> %q0",
        "udot   %q10 %q11 %q12 $0x00 $0x00 -> %q10",
        "udot   %q31 %q31 %q31 $0x03 $0x00 -> %q31",
    };
    for (int i = 0; i < 3; i++) {
        instr = INSTR_CREATE_udot_vector_indexed(
            dc, opnd_create_reg(Rd_0_1[i]), opnd_create_reg(Rn_0_1[i]),
            opnd_create_reg(Rm_0_1[i]), opnd_create_immed_uint(index_0_1[i], OPSZ_0));
        if (!test_instr_encoding(dc, OP_udot, instr, expected_0_1[i]))
            *psuccess = false;
    }
}

TEST_INSTR(fmov)
{

    /* Testing FMOV    <Wd>, <Hn> */
    reg_id_t Rd_0_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_0_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    const char *expected_0_0[3] = {
        "fmov   %h0 -> %w0",
        "fmov   %h11 -> %w10",
        "fmov   %h31 -> %w30",
    };
    TEST_LOOP(fmov, fmov_general, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]));

    /* Testing FMOV    <Wd>, <Sn> */
    reg_id_t Rd_1_0[3] = { DR_REG_W0, DR_REG_W10, DR_REG_W30 };
    reg_id_t Rn_1_0[3] = { DR_REG_S0, DR_REG_S11, DR_REG_S31 };
    const char *expected_1_0[3] = {
        "fmov   %s0 -> %w0",
        "fmov   %s11 -> %w10",
        "fmov   %s31 -> %w30",
    };
    TEST_LOOP(fmov, fmov_general, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]));

    /* Testing FMOV    <Xd>, <Dn> */
    reg_id_t Rd_2_0[3] = { DR_REG_X0, DR_REG_X10, DR_REG_X30 };
    reg_id_t Rn_2_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    const char *expected_2_0[3] = {
        "fmov   %d0 -> %x0",
        "fmov   %d11 -> %x10",
        "fmov   %d31 -> %x30",
    };
    TEST_LOOP(fmov, fmov_general, 3, expected_2_0[i], opnd_create_reg(Rd_2_0[i]),
              opnd_create_reg(Rn_2_0[i]));

    /* Testing FMOV    <Xd>, <Hn> */
    reg_id_t Rd_3_0[3] = { DR_REG_X0, DR_REG_X10, DR_REG_X30 };
    reg_id_t Rn_3_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    const char *expected_3_0[3] = {
        "fmov   %h0 -> %x0",
        "fmov   %h11 -> %x10",
        "fmov   %h31 -> %x30",
    };
    TEST_LOOP(fmov, fmov_general, 3, expected_3_0[i], opnd_create_reg(Rd_3_0[i]),
              opnd_create_reg(Rn_3_0[i]));
}

TEST_INSTR(fmulx_vector)
{

    /* Testing FMULX   <Hd>.<Ts>, <Hn>.<Ts>, <Hm>.<Ts> */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_0_0[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    opnd_t Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[3] = {
        "fmulx  %d0 %d0 $0x01 -> %d0",
        "fmulx  %d11 %d12 $0x01 -> %d10",
        "fmulx  %d31 %d31 $0x01 -> %d31",
    };
    TEST_LOOP(fmulx, fmulx_vector, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]), Rm_elsz);

    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_1[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[3] = {
        "fmulx  %q0 %q0 $0x01 -> %q0",
        "fmulx  %q11 %q12 $0x01 -> %q10",
        "fmulx  %q31 %q31 $0x01 -> %q31",
    };
    TEST_LOOP(fmulx, fmulx_vector, 3, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]), Rm_elsz);

    /* Testing FMULX   <Dd>.<Ts>, <Dn>.<Ts>, <Dm>.<Ts> */
    reg_id_t Rd_1_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_1_0[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[3] = {
        "fmulx  %d0 %d0 $0x02 -> %d0",
        "fmulx  %d11 %d12 $0x02 -> %d10",
        "fmulx  %d31 %d31 $0x02 -> %d31",
    };
    TEST_LOOP(fmulx, fmulx_vector, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]), Rm_elsz);

    reg_id_t Rd_1_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_1_1[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[3] = {
        "fmulx  %q0 %q0 $0x02 -> %q0",
        "fmulx  %q11 %q12 $0x02 -> %q10",
        "fmulx  %q31 %q31 $0x02 -> %q31",
    };
    TEST_LOOP(fmulx, fmulx_vector, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]), Rm_elsz);

    reg_id_t Rd_1_2[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_2[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_1_2[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[3] = {
        "fmulx  %q0 %q0 $0x03 -> %q0",
        "fmulx  %q11 %q12 $0x03 -> %q10",
        "fmulx  %q31 %q31 $0x03 -> %q31",
    };
    TEST_LOOP(fmulx, fmulx_vector, 3, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), opnd_create_reg(Rm_1_2[i]), Rm_elsz);
}

TEST_INSTR(fmulx_vector_idx)
{

    /* Testing FMULX   <Hd>.<Ts>, <Hn>.<Ts>, <Hm>.H[<index>] */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_0_0[3] = { DR_REG_D0, DR_REG_D7, DR_REG_D15 };
    uint index_0_0[3] = { 0, 5, 7 };
    opnd_t Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[3] = {
        "fmulx  %d0 %d0 $0x00 $0x01 -> %d0",
        "fmulx  %d11 %d7 $0x05 $0x01 -> %d10",
        "fmulx  %d31 %d15 $0x07 $0x01 -> %d31",
    };
    TEST_LOOP(fmulx, fmulx_vector_idx, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]),
              opnd_create_immed_uint(index_0_0[i], OPSZ_0), Rm_elsz);

    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_1[3] = { DR_REG_Q0, DR_REG_Q7, DR_REG_Q15 };
    uint index_0_1[3] = { 0, 5, 7 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[3] = {
        "fmulx  %q0 %q0 $0x00 $0x01 -> %q0",
        "fmulx  %q11 %q7 $0x05 $0x01 -> %q10",
        "fmulx  %q31 %q15 $0x07 $0x01 -> %q31",
    };
    TEST_LOOP(fmulx, fmulx_vector_idx, 3, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]),
              opnd_create_immed_uint(index_0_1[i], OPSZ_0), Rm_elsz);

    /* Testing FMULX   <Dd>.<Ts>, <Dn>.<Ts>, <Dm>.<Tb>[<index>] */
    reg_id_t Rd_1_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_1_0[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    uint index_1_0[3] = { 0, 0, 3 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[3] = {
        "fmulx  %d0 %d0 $0x00 $0x02 -> %d0",
        "fmulx  %d11 %d12 $0x00 $0x02 -> %d10",
        "fmulx  %d31 %d31 $0x03 $0x02 -> %d31",
    };
    TEST_LOOP(fmulx, fmulx_vector_idx, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]),
              opnd_create_immed_uint(index_1_0[i], OPSZ_0), Rm_elsz);

    reg_id_t Rd_1_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_1_1[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    uint index_1_1[3] = { 0, 0, 3 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[3] = {
        "fmulx  %q0 %q0 $0x00 $0x02 -> %q0",
        "fmulx  %q11 %q12 $0x00 $0x02 -> %q10",
        "fmulx  %q31 %q31 $0x03 $0x02 -> %q31",
    };
    TEST_LOOP(fmulx, fmulx_vector_idx, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]),
              opnd_create_immed_uint(index_1_1[i], OPSZ_0), Rm_elsz);

    reg_id_t Rd_1_2[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_2[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_1_2[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    uint index_1_2[3] = { 0, 1, 1 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[3] = {
        "fmulx  %q0 %q0 $0x00 $0x03 -> %q0",
        "fmulx  %q11 %q12 $0x01 $0x03 -> %q10",
        "fmulx  %q31 %q31 $0x01 $0x03 -> %q31",
    };
    TEST_LOOP(fmulx, fmulx_vector_idx, 3, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), opnd_create_reg(Rm_1_2[i]),
              opnd_create_immed_uint(index_1_2[i], OPSZ_0), Rm_elsz);

    /* Testing FMULX   <Hd>, <Hn>, <Hm>.H[<index>] */
    reg_id_t Rd_2_0[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_2_0[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_2_0[3] = { DR_REG_Q0, DR_REG_Q7, DR_REG_Q15 };
    uint index_2_0[3] = { 0, 5, 7 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_2_0[3] = {
        "fmulx  %q0 %q0 $0x00 $0x01 -> %q0",
        "fmulx  %q11 %q7 $0x05 $0x01 -> %q10",
        "fmulx  %q31 %q15 $0x07 $0x01 -> %q31",
    };
    TEST_LOOP(fmulx, fmulx_vector_idx, 3, expected_2_0[i], opnd_create_reg(Rd_2_0[i]),
              opnd_create_reg(Rn_2_0[i]), opnd_create_reg(Rm_2_0[i]),
              opnd_create_immed_uint(index_2_0[i], OPSZ_0), Rm_elsz);

    /* Testing FMULX   <V><d>, <V><n>, <Sm>.<Ts>[<index>] */
    reg_id_t Rd_3_0[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rn_3_0[3] = { DR_REG_S0, DR_REG_S11, DR_REG_S31 };
    reg_id_t Rm_3_0[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    uint index_3_0[3] = { 0, 0, 3 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_3_0[3] = {
        "fmulx  %s0 %q0 $0x00 $0x02 -> %s0",
        "fmulx  %s11 %q12 $0x00 $0x02 -> %s10",
        "fmulx  %s31 %q31 $0x03 $0x02 -> %s31",
    };
    TEST_LOOP(fmulx, fmulx_vector_idx, 3, expected_3_0[i], opnd_create_reg(Rd_3_0[i]),
              opnd_create_reg(Rn_3_0[i]), opnd_create_reg(Rm_3_0[i]),
              opnd_create_immed_uint(index_3_0[i], OPSZ_0), Rm_elsz);

    reg_id_t Rd_3_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_3_1[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_3_1[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    uint index_3_1[3] = { 0, 1, 1 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_3_1[3] = {
        "fmulx  %d0 %q0 $0x00 $0x03 -> %d0",
        "fmulx  %d11 %q12 $0x01 $0x03 -> %d10",
        "fmulx  %d31 %q31 $0x01 $0x03 -> %d31",
    };
    TEST_LOOP(fmulx, fmulx_vector_idx, 3, expected_3_1[i], opnd_create_reg(Rd_3_1[i]),
              opnd_create_reg(Rn_3_1[i]), opnd_create_reg(Rm_3_1[i]),
              opnd_create_immed_uint(index_3_1[i], OPSZ_0), Rm_elsz);
}

TEST_INSTR(fmulx)
{

    /* Testing FMULX   <Hd>, <Hn>, <Hm> */
    reg_id_t Rd_0_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    reg_id_t Rm_0_0[3] = { DR_REG_H0, DR_REG_H12, DR_REG_H31 };
    const char *expected_0_0[3] = {
        "fmulx  %h0 %h0 -> %h0",
        "fmulx  %h11 %h12 -> %h10",
        "fmulx  %h31 %h31 -> %h31",
    };
    TEST_LOOP(fmulx, fmulx, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]));

    /* Testing FMULX   <V><d>, <V><n>, <V><m> */
    reg_id_t Rd_1_0[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rn_1_0[3] = { DR_REG_S0, DR_REG_S11, DR_REG_S31 };
    reg_id_t Rm_1_0[3] = { DR_REG_S0, DR_REG_S12, DR_REG_S31 };
    const char *expected_1_0[3] = {
        "fmulx  %s0 %s0 -> %s0",
        "fmulx  %s11 %s12 -> %s10",
        "fmulx  %s31 %s31 -> %s31",
    };
    TEST_LOOP(fmulx, fmulx, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]));

    reg_id_t Rd_1_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_1[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_1_1[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    const char *expected_1_1[3] = {
        "fmulx  %d0 %d0 -> %d0",
        "fmulx  %d11 %d12 -> %d10",
        "fmulx  %d31 %d31 -> %d31",
    };
    TEST_LOOP(fmulx, fmulx, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]));
}

TEST_INSTR(facge_vector)
{

    /* Testing FACGE   <Hd>.<Ts>, <Hn>.<Ts>, <Hm>.<Ts> */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_0_0[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    opnd_t Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[3] = {
        "facge  %d0 %d0 $0x01 -> %d0",
        "facge  %d11 %d12 $0x01 -> %d10",
        "facge  %d31 %d31 $0x01 -> %d31",
    };
    TEST_LOOP(facge, facge_vector, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]), Rm_elsz);

    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_1[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[3] = {
        "facge  %q0 %q0 $0x01 -> %q0",
        "facge  %q11 %q12 $0x01 -> %q10",
        "facge  %q31 %q31 $0x01 -> %q31",
    };
    TEST_LOOP(facge, facge_vector, 3, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]), Rm_elsz);

    /* Testing FACGE   <Dd>.<Ts>, <Dn>.<Ts>, <Dm>.<Ts> */
    reg_id_t Rd_1_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_1_0[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[3] = {
        "facge  %d0 %d0 $0x02 -> %d0",
        "facge  %d11 %d12 $0x02 -> %d10",
        "facge  %d31 %d31 $0x02 -> %d31",
    };
    TEST_LOOP(facge, facge_vector, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]), Rm_elsz);

    reg_id_t Rd_1_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_1_1[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[3] = {
        "facge  %q0 %q0 $0x02 -> %q0",
        "facge  %q11 %q12 $0x02 -> %q10",
        "facge  %q31 %q31 $0x02 -> %q31",
    };
    TEST_LOOP(facge, facge_vector, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]), Rm_elsz);

    reg_id_t Rd_1_2[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_2[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_1_2[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[3] = {
        "facge  %q0 %q0 $0x03 -> %q0",
        "facge  %q11 %q12 $0x03 -> %q10",
        "facge  %q31 %q31 $0x03 -> %q31",
    };
    TEST_LOOP(facge, facge_vector, 3, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), opnd_create_reg(Rm_1_2[i]), Rm_elsz);
}

TEST_INSTR(facge)
{

    /* Testing FACGE   <Hd>, <Hn>, <Hm> */
    reg_id_t Rd_0_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    reg_id_t Rm_0_0[3] = { DR_REG_H0, DR_REG_H12, DR_REG_H31 };
    const char *expected_0_0[3] = {
        "facge  %h0 %h0 -> %h0",
        "facge  %h11 %h12 -> %h10",
        "facge  %h31 %h31 -> %h31",
    };
    TEST_LOOP(facge, facge, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]));

    /* Testing FACGE   <V><d>, <V><n>, <V><m> */
    reg_id_t Rd_1_0[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rn_1_0[3] = { DR_REG_S0, DR_REG_S11, DR_REG_S31 };
    reg_id_t Rm_1_0[3] = { DR_REG_S0, DR_REG_S12, DR_REG_S31 };
    const char *expected_1_0[3] = {
        "facge  %s0 %s0 -> %s0",
        "facge  %s11 %s12 -> %s10",
        "facge  %s31 %s31 -> %s31",
    };
    TEST_LOOP(facge, facge, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]));

    reg_id_t Rd_1_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_1[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_1_1[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    const char *expected_1_1[3] = {
        "facge  %d0 %d0 -> %d0",
        "facge  %d11 %d12 -> %d10",
        "facge  %d31 %d31 -> %d31",
    };
    TEST_LOOP(facge, facge, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]));
}

TEST_INSTR(facgt_vector)
{

    /* Testing FACGT   <Hd>.<Ts>, <Hn>.<Ts>, <Hm>.<Ts> */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_0_0[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    opnd_t Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[3] = {
        "facgt  %d0 %d0 $0x01 -> %d0",
        "facgt  %d11 %d12 $0x01 -> %d10",
        "facgt  %d31 %d31 $0x01 -> %d31",
    };
    TEST_LOOP(facgt, facgt_vector, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]), Rm_elsz);

    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_1[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[3] = {
        "facgt  %q0 %q0 $0x01 -> %q0",
        "facgt  %q11 %q12 $0x01 -> %q10",
        "facgt  %q31 %q31 $0x01 -> %q31",
    };
    TEST_LOOP(facgt, facgt_vector, 3, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]), Rm_elsz);

    /* Testing FACGT   <Dd>.<Ts>, <Dn>.<Ts>, <Dm>.<Ts> */
    reg_id_t Rd_1_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_1_0[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[3] = {
        "facgt  %d0 %d0 $0x02 -> %d0",
        "facgt  %d11 %d12 $0x02 -> %d10",
        "facgt  %d31 %d31 $0x02 -> %d31",
    };
    TEST_LOOP(facgt, facgt_vector, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]), Rm_elsz);

    reg_id_t Rd_1_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_1_1[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[3] = {
        "facgt  %q0 %q0 $0x02 -> %q0",
        "facgt  %q11 %q12 $0x02 -> %q10",
        "facgt  %q31 %q31 $0x02 -> %q31",
    };
    TEST_LOOP(facgt, facgt_vector, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]), Rm_elsz);

    reg_id_t Rd_1_2[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_2[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_1_2[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[3] = {
        "facgt  %q0 %q0 $0x03 -> %q0",
        "facgt  %q11 %q12 $0x03 -> %q10",
        "facgt  %q31 %q31 $0x03 -> %q31",
    };
    TEST_LOOP(facgt, facgt_vector, 3, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), opnd_create_reg(Rm_1_2[i]), Rm_elsz);
}

TEST_INSTR(facgt)
{

    /* Testing FACGT   <Hd>, <Hn>, <Hm> */
    reg_id_t Rd_0_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    reg_id_t Rm_0_0[3] = { DR_REG_H0, DR_REG_H12, DR_REG_H31 };
    const char *expected_0_0[3] = {
        "facgt  %h0 %h0 -> %h0",
        "facgt  %h11 %h12 -> %h10",
        "facgt  %h31 %h31 -> %h31",
    };
    TEST_LOOP(facgt, facgt, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]));

    /* Testing FACGT   <V><d>, <V><n>, <V><m> */
    reg_id_t Rd_1_0[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rn_1_0[3] = { DR_REG_S0, DR_REG_S11, DR_REG_S31 };
    reg_id_t Rm_1_0[3] = { DR_REG_S0, DR_REG_S12, DR_REG_S31 };
    const char *expected_1_0[3] = {
        "facgt  %s0 %s0 -> %s0",
        "facgt  %s11 %s12 -> %s10",
        "facgt  %s31 %s31 -> %s31",
    };
    TEST_LOOP(facgt, facgt, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]));

    reg_id_t Rd_1_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_1[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_1_1[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    const char *expected_1_1[3] = {
        "facgt  %d0 %d0 -> %d0",
        "facgt  %d11 %d12 -> %d10",
        "facgt  %d31 %d31 -> %d31",
    };
    TEST_LOOP(facgt, facgt, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]));
}

TEST_INSTR(faddp_vector)
{

    /* Testing FADDP   <Hd>.<Ts>, <Hn>.<Ts>, <Hm>.<Ts> */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_0_0[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    opnd_t Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[3] = {
        "faddp  %d0 %d0 $0x01 -> %d0",
        "faddp  %d11 %d12 $0x01 -> %d10",
        "faddp  %d31 %d31 $0x01 -> %d31",
    };
    TEST_LOOP(faddp, faddp_vector, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]), Rm_elsz);

    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_1[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[3] = {
        "faddp  %q0 %q0 $0x01 -> %q0",
        "faddp  %q11 %q12 $0x01 -> %q10",
        "faddp  %q31 %q31 $0x01 -> %q31",
    };
    TEST_LOOP(faddp, faddp_vector, 3, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]), Rm_elsz);

    /* Testing FADDP   <Dd>.<Ts>, <Dn>.<Ts>, <Dm>.<Ts> */
    reg_id_t Rd_1_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_1_0[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[3] = {
        "faddp  %d0 %d0 $0x02 -> %d0",
        "faddp  %d11 %d12 $0x02 -> %d10",
        "faddp  %d31 %d31 $0x02 -> %d31",
    };
    TEST_LOOP(faddp, faddp_vector, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]), Rm_elsz);

    reg_id_t Rd_1_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_1_1[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[3] = {
        "faddp  %q0 %q0 $0x02 -> %q0",
        "faddp  %q11 %q12 $0x02 -> %q10",
        "faddp  %q31 %q31 $0x02 -> %q31",
    };
    TEST_LOOP(faddp, faddp_vector, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]), Rm_elsz);

    reg_id_t Rd_1_2[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_2[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_1_2[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[3] = {
        "faddp  %q0 %q0 $0x03 -> %q0",
        "faddp  %q11 %q12 $0x03 -> %q10",
        "faddp  %q31 %q31 $0x03 -> %q31",
    };
    TEST_LOOP(faddp, faddp_vector, 3, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), opnd_create_reg(Rm_1_2[i]), Rm_elsz);
}

TEST_INSTR(faddp_scalar)
{

    /* Testing FADDP   <Hd>, <Hn>.2H */
    reg_id_t Rd_0_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0_0[3] = { DR_REG_S0, DR_REG_S11, DR_REG_S31 };
    opnd_t Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[3] = {
        "faddp  %s0 $0x01 -> %h0",
        "faddp  %s11 $0x01 -> %h10",
        "faddp  %s31 $0x01 -> %h31",
    };
    TEST_LOOP(faddp, faddp_scalar, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), Rn_elsz);

    /* Testing FADDP   <V><d>, <Sn>.<Ts> */
    reg_id_t Rd_1_0[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rn_1_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[3] = {
        "faddp  %d0 $0x02 -> %s0",
        "faddp  %d11 $0x02 -> %s10",
        "faddp  %d31 $0x02 -> %s31",
    };
    TEST_LOOP(faddp, faddp_scalar, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), Rn_elsz);

    reg_id_t Rd_1_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_1[3] = {
        "faddp  %q0 $0x03 -> %d0",
        "faddp  %q11 $0x03 -> %d10",
        "faddp  %q31 $0x03 -> %d31",
    };
    TEST_LOOP(faddp, faddp_scalar, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), Rn_elsz);
}

TEST_INSTR(fcmeq_vector_zero)
{

    /* Testing FCMEQ   <Hd>.<Ts>, <Hn>.<Ts>, #0 */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    opnd_t Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[3] = {
        "fcmeq  %d0 $0.000000 $0x01 -> %d0",
        "fcmeq  %d11 $0.000000 $0x01 -> %d10",
        "fcmeq  %d31 $0.000000 $0x01 -> %d31",
    };
    TEST_LOOP(fcmeq, fcmeq_vector_zero, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), Rn_elsz);

    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[3] = {
        "fcmeq  %q0 $0.000000 $0x01 -> %q0",
        "fcmeq  %q11 $0.000000 $0x01 -> %q10",
        "fcmeq  %q31 $0.000000 $0x01 -> %q31",
    };
    TEST_LOOP(fcmeq, fcmeq_vector_zero, 3, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), Rn_elsz);

    /* Testing FCMEQ   <Dd>.<Ts>, <Dn>.<Ts>, #0 */
    reg_id_t Rd_1_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[3] = {
        "fcmeq  %d0 $0.000000 $0x02 -> %d0",
        "fcmeq  %d11 $0.000000 $0x02 -> %d10",
        "fcmeq  %d31 $0.000000 $0x02 -> %d31",
    };
    TEST_LOOP(fcmeq, fcmeq_vector_zero, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), Rn_elsz);

    reg_id_t Rd_1_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[3] = {
        "fcmeq  %q0 $0.000000 $0x02 -> %q0",
        "fcmeq  %q11 $0.000000 $0x02 -> %q10",
        "fcmeq  %q31 $0.000000 $0x02 -> %q31",
    };
    TEST_LOOP(fcmeq, fcmeq_vector_zero, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), Rn_elsz);

    reg_id_t Rd_1_2[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_2[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[3] = {
        "fcmeq  %q0 $0.000000 $0x03 -> %q0",
        "fcmeq  %q11 $0.000000 $0x03 -> %q10",
        "fcmeq  %q31 $0.000000 $0x03 -> %q31",
    };
    TEST_LOOP(fcmeq, fcmeq_vector_zero, 3, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), Rn_elsz);
}

TEST_INSTR(fcmeq_vector)
{

    /* Testing FCMEQ   <Hd>.<Ts>, <Hn>.<Ts>, <Hm>.<Ts> */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_0_0[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    opnd_t Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[3] = {
        "fcmeq  %d0 %d0 $0x01 -> %d0",
        "fcmeq  %d11 %d12 $0x01 -> %d10",
        "fcmeq  %d31 %d31 $0x01 -> %d31",
    };
    TEST_LOOP(fcmeq, fcmeq_vector, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]), Rm_elsz);

    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_1[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[3] = {
        "fcmeq  %q0 %q0 $0x01 -> %q0",
        "fcmeq  %q11 %q12 $0x01 -> %q10",
        "fcmeq  %q31 %q31 $0x01 -> %q31",
    };
    TEST_LOOP(fcmeq, fcmeq_vector, 3, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]), Rm_elsz);

    /* Testing FCMEQ   <Dd>.<Ts>, <Dn>.<Ts>, <Dm>.<Ts> */
    reg_id_t Rd_1_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_1_0[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[3] = {
        "fcmeq  %d0 %d0 $0x02 -> %d0",
        "fcmeq  %d11 %d12 $0x02 -> %d10",
        "fcmeq  %d31 %d31 $0x02 -> %d31",
    };
    TEST_LOOP(fcmeq, fcmeq_vector, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]), Rm_elsz);

    reg_id_t Rd_1_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_1_1[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[3] = {
        "fcmeq  %q0 %q0 $0x02 -> %q0",
        "fcmeq  %q11 %q12 $0x02 -> %q10",
        "fcmeq  %q31 %q31 $0x02 -> %q31",
    };
    TEST_LOOP(fcmeq, fcmeq_vector, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]), Rm_elsz);

    reg_id_t Rd_1_2[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_2[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_1_2[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[3] = {
        "fcmeq  %q0 %q0 $0x03 -> %q0",
        "fcmeq  %q11 %q12 $0x03 -> %q10",
        "fcmeq  %q31 %q31 $0x03 -> %q31",
    };
    TEST_LOOP(fcmeq, fcmeq_vector, 3, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), opnd_create_reg(Rm_1_2[i]), Rm_elsz);
}

TEST_INSTR(fcmeq_zero)
{

    /* Testing FCMEQ   <Hd>, <Hn>, #0 */
    reg_id_t Rd_0_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    const char *expected_0_0[3] = {
        "fcmeq  %h0 $0.000000 -> %h0",
        "fcmeq  %h11 $0.000000 -> %h10",
        "fcmeq  %h31 $0.000000 -> %h31",
    };
    TEST_LOOP(fcmeq, fcmeq_zero, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]));

    /* Testing FCMEQ   <V><d>, <V><n>, #0 */
    reg_id_t Rd_1_0[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rn_1_0[3] = { DR_REG_S0, DR_REG_S11, DR_REG_S31 };
    const char *expected_1_0[3] = {
        "fcmeq  %s0 $0.000000 -> %s0",
        "fcmeq  %s11 $0.000000 -> %s10",
        "fcmeq  %s31 $0.000000 -> %s31",
    };
    TEST_LOOP(fcmeq, fcmeq_zero, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]));

    reg_id_t Rd_1_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_1[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    const char *expected_1_1[3] = {
        "fcmeq  %d0 $0.000000 -> %d0",
        "fcmeq  %d11 $0.000000 -> %d10",
        "fcmeq  %d31 $0.000000 -> %d31",
    };
    TEST_LOOP(fcmeq, fcmeq_zero, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]));
}

TEST_INSTR(fcmeq)
{

    /* Testing FCMEQ   <Hd>, <Hn>, <Hm> */
    reg_id_t Rd_0_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    reg_id_t Rm_0_0[3] = { DR_REG_H0, DR_REG_H12, DR_REG_H31 };
    const char *expected_0_0[3] = {
        "fcmeq  %h0 %h0 -> %h0",
        "fcmeq  %h11 %h12 -> %h10",
        "fcmeq  %h31 %h31 -> %h31",
    };
    TEST_LOOP(fcmeq, fcmeq, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]));

    /* Testing FCMEQ   <V><d>, <V><n>, <V><m> */
    reg_id_t Rd_1_0[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rn_1_0[3] = { DR_REG_S0, DR_REG_S11, DR_REG_S31 };
    reg_id_t Rm_1_0[3] = { DR_REG_S0, DR_REG_S12, DR_REG_S31 };
    const char *expected_1_0[3] = {
        "fcmeq  %s0 %s0 -> %s0",
        "fcmeq  %s11 %s12 -> %s10",
        "fcmeq  %s31 %s31 -> %s31",
    };
    TEST_LOOP(fcmeq, fcmeq, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]));

    reg_id_t Rd_1_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_1[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_1_1[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    const char *expected_1_1[3] = {
        "fcmeq  %d0 %d0 -> %d0",
        "fcmeq  %d11 %d12 -> %d10",
        "fcmeq  %d31 %d31 -> %d31",
    };
    TEST_LOOP(fcmeq, fcmeq, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]));
}

TEST_INSTR(fcmgt_vector_zero)
{
    opnd_t Rn_elsz;

    /* Testing FCMGT   <Hd>.<Ts>, <Hn>.<Ts>, #0 */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[3] = {
        "fcmgt  %d0 $0.000000 $0x01 -> %d0",
        "fcmgt  %d11 $0.000000 $0x01 -> %d10",
        "fcmgt  %d31 $0.000000 $0x01 -> %d31",
    };
    TEST_LOOP(fcmgt, fcmgt_vector_zero, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), Rn_elsz);

    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[3] = {
        "fcmgt  %q0 $0.000000 $0x01 -> %q0",
        "fcmgt  %q11 $0.000000 $0x01 -> %q10",
        "fcmgt  %q31 $0.000000 $0x01 -> %q31",
    };
    TEST_LOOP(fcmgt, fcmgt_vector_zero, 3, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), Rn_elsz);

    /* Testing FCMGT   <Dd>.<Ts>, <Dn>.<Ts>, #0 */
    reg_id_t Rd_1_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[3] = {
        "fcmgt  %d0 $0.000000 $0x02 -> %d0",
        "fcmgt  %d11 $0.000000 $0x02 -> %d10",
        "fcmgt  %d31 $0.000000 $0x02 -> %d31",
    };
    TEST_LOOP(fcmgt, fcmgt_vector_zero, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), Rn_elsz);

    reg_id_t Rd_1_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[3] = {
        "fcmgt  %q0 $0.000000 $0x02 -> %q0",
        "fcmgt  %q11 $0.000000 $0x02 -> %q10",
        "fcmgt  %q31 $0.000000 $0x02 -> %q31",
    };
    TEST_LOOP(fcmgt, fcmgt_vector_zero, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), Rn_elsz);

    reg_id_t Rd_1_2[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_2[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[3] = {
        "fcmgt  %q0 $0.000000 $0x03 -> %q0",
        "fcmgt  %q11 $0.000000 $0x03 -> %q10",
        "fcmgt  %q31 $0.000000 $0x03 -> %q31",
    };
    TEST_LOOP(fcmgt, fcmgt_vector_zero, 3, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), Rn_elsz);
}

TEST_INSTR(fcmgt_vector)
{
    opnd_t Rm_elsz;

    /* Testing FCMGT   <Hd>.<Ts>, <Hn>.<Ts>, <Hm>.<Ts> */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_0_0[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[3] = {
        "fcmgt  %d0 %d0 $0x01 -> %d0",
        "fcmgt  %d11 %d12 $0x01 -> %d10",
        "fcmgt  %d31 %d31 $0x01 -> %d31",
    };
    TEST_LOOP(fcmgt, fcmgt_vector, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]), Rm_elsz);

    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_0_1[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[3] = {
        "fcmgt  %q0 %q0 $0x01 -> %q0",
        "fcmgt  %q11 %q12 $0x01 -> %q10",
        "fcmgt  %q31 %q31 $0x01 -> %q31",
    };
    TEST_LOOP(fcmgt, fcmgt_vector, 3, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]), Rm_elsz);

    /* Testing FCMGT   <Dd>.<Ts>, <Dn>.<Ts>, <Dm>.<Ts> */
    reg_id_t Rd_1_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_1_0[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[3] = {
        "fcmgt  %d0 %d0 $0x02 -> %d0",
        "fcmgt  %d11 %d12 $0x02 -> %d10",
        "fcmgt  %d31 %d31 $0x02 -> %d31",
    };
    TEST_LOOP(fcmgt, fcmgt_vector, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]), Rm_elsz);

    reg_id_t Rd_1_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_1_1[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[3] = {
        "fcmgt  %q0 %q0 $0x02 -> %q0",
        "fcmgt  %q11 %q12 $0x02 -> %q10",
        "fcmgt  %q31 %q31 $0x02 -> %q31",
    };
    TEST_LOOP(fcmgt, fcmgt_vector, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]), Rm_elsz);

    reg_id_t Rd_1_2[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_2[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    reg_id_t Rm_1_2[3] = { DR_REG_Q0, DR_REG_Q12, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[3] = {
        "fcmgt  %q0 %q0 $0x03 -> %q0",
        "fcmgt  %q11 %q12 $0x03 -> %q10",
        "fcmgt  %q31 %q31 $0x03 -> %q31",
    };
    TEST_LOOP(fcmgt, fcmgt_vector, 3, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), opnd_create_reg(Rm_1_2[i]), Rm_elsz);
}

TEST_INSTR(fcmgt_zero)
{

    /* Testing FCMGT   <Hd>, <Hn>, #0 */
    reg_id_t Rd_0_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    const char *expected_0_0[3] = {
        "fcmgt  %h0 $0.000000 -> %h0",
        "fcmgt  %h11 $0.000000 -> %h10",
        "fcmgt  %h31 $0.000000 -> %h31",
    };
    TEST_LOOP(fcmgt, fcmgt_zero, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]));

    /* Testing FCMGT   <V><d>, <V><n>, #0 */
    reg_id_t Rd_1_0[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rn_1_0[3] = { DR_REG_S0, DR_REG_S11, DR_REG_S31 };
    const char *expected_1_0[3] = {
        "fcmgt  %s0 $0.000000 -> %s0",
        "fcmgt  %s11 $0.000000 -> %s10",
        "fcmgt  %s31 $0.000000 -> %s31",
    };
    TEST_LOOP(fcmgt, fcmgt_zero, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]));

    reg_id_t Rd_1_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_1[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    const char *expected_1_1[3] = {
        "fcmgt  %d0 $0.000000 -> %d0",
        "fcmgt  %d11 $0.000000 -> %d10",
        "fcmgt  %d31 $0.000000 -> %d31",
    };
    TEST_LOOP(fcmgt, fcmgt_zero, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]));
}

TEST_INSTR(fcmgt)
{

    /* Testing FCMGT   <Hd>, <Hn>, <Hm> */
    reg_id_t Rd_0_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    reg_id_t Rm_0_0[3] = { DR_REG_H0, DR_REG_H12, DR_REG_H31 };
    const char *expected_0_0[3] = {
        "fcmgt  %h0 %h0 -> %h0",
        "fcmgt  %h11 %h12 -> %h10",
        "fcmgt  %h31 %h31 -> %h31",
    };
    TEST_LOOP(fcmgt, fcmgt, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]));

    /* Testing FCMGT   <V><d>, <V><n>, <V><m> */
    reg_id_t Rd_1_0[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rn_1_0[3] = { DR_REG_S0, DR_REG_S11, DR_REG_S31 };
    reg_id_t Rm_1_0[3] = { DR_REG_S0, DR_REG_S12, DR_REG_S31 };
    const char *expected_1_0[3] = {
        "fcmgt  %s0 %s0 -> %s0",
        "fcmgt  %s11 %s12 -> %s10",
        "fcmgt  %s31 %s31 -> %s31",
    };
    TEST_LOOP(fcmgt, fcmgt, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]));

    reg_id_t Rd_1_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_1[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    reg_id_t Rm_1_1[3] = { DR_REG_D0, DR_REG_D12, DR_REG_D31 };
    const char *expected_1_1[3] = {
        "fcmgt  %d0 %d0 -> %d0",
        "fcmgt  %d11 %d12 -> %d10",
        "fcmgt  %d31 %d31 -> %d31",
    };
    TEST_LOOP(fcmgt, fcmgt, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]));
}

TEST_INSTR(fcmle_vector_zero)
{
    opnd_t Rn_elsz;

    /* Testing FCMLE   <Hd>.<Ts>, <Hn>.<Ts>, #0 */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[3] = {
        "fcmle  %d0 $0.000000 $0x01 -> %d0",
        "fcmle  %d11 $0.000000 $0x01 -> %d10",
        "fcmle  %d31 $0.000000 $0x01 -> %d31",
    };
    TEST_LOOP(fcmle, fcmle_vector_zero, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), Rn_elsz);

    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[3] = {
        "fcmle  %q0 $0.000000 $0x01 -> %q0",
        "fcmle  %q11 $0.000000 $0x01 -> %q10",
        "fcmle  %q31 $0.000000 $0x01 -> %q31",
    };
    TEST_LOOP(fcmle, fcmle_vector_zero, 3, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), Rn_elsz);

    /* Testing FCMLE   <Dd>.<Ts>, <Dn>.<Ts>, #0 */
    reg_id_t Rd_1_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[3] = {
        "fcmle  %d0 $0.000000 $0x02 -> %d0",
        "fcmle  %d11 $0.000000 $0x02 -> %d10",
        "fcmle  %d31 $0.000000 $0x02 -> %d31",
    };
    TEST_LOOP(fcmle, fcmle_vector_zero, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), Rn_elsz);

    reg_id_t Rd_1_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[3] = {
        "fcmle  %q0 $0.000000 $0x02 -> %q0",
        "fcmle  %q11 $0.000000 $0x02 -> %q10",
        "fcmle  %q31 $0.000000 $0x02 -> %q31",
    };
    TEST_LOOP(fcmle, fcmle_vector_zero, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), Rn_elsz);

    reg_id_t Rd_1_2[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_2[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[3] = {
        "fcmle  %q0 $0.000000 $0x03 -> %q0",
        "fcmle  %q11 $0.000000 $0x03 -> %q10",
        "fcmle  %q31 $0.000000 $0x03 -> %q31",
    };
    TEST_LOOP(fcmle, fcmle_vector_zero, 3, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), Rn_elsz);
}

TEST_INSTR(fcmle_zero)
{

    /* Testing FCMLE   <Hd>, <Hn>, #0 */
    reg_id_t Rd_0_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    const char *expected_0_0[3] = {
        "fcmle  %h0 $0.000000 -> %h0",
        "fcmle  %h11 $0.000000 -> %h10",
        "fcmle  %h31 $0.000000 -> %h31",
    };
    TEST_LOOP(fcmle, fcmle_zero, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]));

    /* Testing FCMLE   <V><d>, <V><n>, #0 */
    reg_id_t Rd_1_0[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rn_1_0[3] = { DR_REG_S0, DR_REG_S11, DR_REG_S31 };
    const char *expected_1_0[3] = {
        "fcmle  %s0 $0.000000 -> %s0",
        "fcmle  %s11 $0.000000 -> %s10",
        "fcmle  %s31 $0.000000 -> %s31",
    };
    TEST_LOOP(fcmle, fcmle_zero, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]));

    reg_id_t Rd_1_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_1[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    const char *expected_1_1[3] = {
        "fcmle  %d0 $0.000000 -> %d0",
        "fcmle  %d11 $0.000000 -> %d10",
        "fcmle  %d31 $0.000000 -> %d31",
    };
    TEST_LOOP(fcmle, fcmle_zero, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]));
}

TEST_INSTR(fcmlt_vector_zero)
{
    opnd_t Rn_elsz;

    /* Testing FCMLT   <Hd>.<Ts>, <Hn>.<Ts>, #0 */
    reg_id_t Rd_0_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_0_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[3] = {
        "fcmlt  %d0 $0.000000 $0x01 -> %d0",
        "fcmlt  %d11 $0.000000 $0x01 -> %d10",
        "fcmlt  %d31 $0.000000 $0x01 -> %d31",
    };
    TEST_LOOP(fcmlt, fcmlt_vector_zero, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), Rn_elsz);

    reg_id_t Rd_0_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_0_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[3] = {
        "fcmlt  %q0 $0.000000 $0x01 -> %q0",
        "fcmlt  %q11 $0.000000 $0x01 -> %q10",
        "fcmlt  %q31 $0.000000 $0x01 -> %q31",
    };
    TEST_LOOP(fcmlt, fcmlt_vector_zero, 3, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), Rn_elsz);

    /* Testing FCMLT   <Dd>.<Ts>, <Dn>.<Ts>, #0 */
    reg_id_t Rd_1_0[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_0[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[3] = {
        "fcmlt  %d0 $0.000000 $0x02 -> %d0",
        "fcmlt  %d11 $0.000000 $0x02 -> %d10",
        "fcmlt  %d31 $0.000000 $0x02 -> %d31",
    };
    TEST_LOOP(fcmlt, fcmlt_vector_zero, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), Rn_elsz);

    reg_id_t Rd_1_1[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_1[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[3] = {
        "fcmlt  %q0 $0.000000 $0x02 -> %q0",
        "fcmlt  %q11 $0.000000 $0x02 -> %q10",
        "fcmlt  %q31 $0.000000 $0x02 -> %q31",
    };
    TEST_LOOP(fcmlt, fcmlt_vector_zero, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), Rn_elsz);

    reg_id_t Rd_1_2[3] = { DR_REG_Q0, DR_REG_Q10, DR_REG_Q31 };
    reg_id_t Rn_1_2[3] = { DR_REG_Q0, DR_REG_Q11, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[3] = {
        "fcmlt  %q0 $0.000000 $0x03 -> %q0",
        "fcmlt  %q11 $0.000000 $0x03 -> %q10",
        "fcmlt  %q31 $0.000000 $0x03 -> %q31",
    };
    TEST_LOOP(fcmlt, fcmlt_vector_zero, 3, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), Rn_elsz);
}

TEST_INSTR(fcmlt_zero)
{

    /* Testing FCMLT   <Hd>, <Hn>, #0 */
    reg_id_t Rd_0_0[3] = { DR_REG_H0, DR_REG_H10, DR_REG_H31 };
    reg_id_t Rn_0_0[3] = { DR_REG_H0, DR_REG_H11, DR_REG_H31 };
    const char *expected_0_0[3] = {
        "fcmlt  %h0 $0.000000 -> %h0",
        "fcmlt  %h11 $0.000000 -> %h10",
        "fcmlt  %h31 $0.000000 -> %h31",
    };
    TEST_LOOP(fcmlt, fcmlt_zero, 3, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]));

    /* Testing FCMLT   <V><d>, <V><n>, #0 */
    reg_id_t Rd_1_0[3] = { DR_REG_S0, DR_REG_S10, DR_REG_S31 };
    reg_id_t Rn_1_0[3] = { DR_REG_S0, DR_REG_S11, DR_REG_S31 };
    const char *expected_1_0[3] = {
        "fcmlt  %s0 $0.000000 -> %s0",
        "fcmlt  %s11 $0.000000 -> %s10",
        "fcmlt  %s31 $0.000000 -> %s31",
    };
    TEST_LOOP(fcmlt, fcmlt_zero, 3, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]));

    reg_id_t Rd_1_1[3] = { DR_REG_D0, DR_REG_D10, DR_REG_D31 };
    reg_id_t Rn_1_1[3] = { DR_REG_D0, DR_REG_D11, DR_REG_D31 };
    const char *expected_1_1[3] = {
        "fcmlt  %d0 $0.000000 -> %d0",
        "fcmlt  %d11 $0.000000 -> %d10",
        "fcmlt  %d31 $0.000000 -> %d31",
    };
    TEST_LOOP(fcmlt, fcmlt_zero, 3, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]));
}

TEST_INSTR(fmaxnmp_vector)
{
    opnd_t Rm_elsz;

    /* Testing FMAXNMP <Hd>.<Ts>, <Hn>.<Ts>, <Hm>.<Ts> */
    reg_id_t Rd_0_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_0_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_0_0[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[6] = {
        "fmaxnmp %d0 %d0 $0x01 -> %d0",    "fmaxnmp %d6 %d7 $0x01 -> %d5",
        "fmaxnmp %d11 %d12 $0x01 -> %d10", "fmaxnmp %d17 %d18 $0x01 -> %d16",
        "fmaxnmp %d22 %d23 $0x01 -> %d21", "fmaxnmp %d31 %d31 $0x01 -> %d31",
    };
    TEST_LOOP(fmaxnmp, fmaxnmp_vector, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]), Rm_elsz);

    reg_id_t Rd_0_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_0_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_0_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[6] = {
        "fmaxnmp %q0 %q0 $0x01 -> %q0",    "fmaxnmp %q6 %q7 $0x01 -> %q5",
        "fmaxnmp %q11 %q12 $0x01 -> %q10", "fmaxnmp %q17 %q18 $0x01 -> %q16",
        "fmaxnmp %q22 %q23 $0x01 -> %q21", "fmaxnmp %q31 %q31 $0x01 -> %q31",
    };
    TEST_LOOP(fmaxnmp, fmaxnmp_vector, 6, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]), Rm_elsz);

    /* Testing FMAXNMP <Dd>.<Ts>, <Dn>.<Ts>, <Dm>.<Ts> */
    reg_id_t Rd_1_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_1_0[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[6] = {
        "fmaxnmp %d0 %d0 $0x02 -> %d0",    "fmaxnmp %d6 %d7 $0x02 -> %d5",
        "fmaxnmp %d11 %d12 $0x02 -> %d10", "fmaxnmp %d17 %d18 $0x02 -> %d16",
        "fmaxnmp %d22 %d23 $0x02 -> %d21", "fmaxnmp %d31 %d31 $0x02 -> %d31",
    };
    TEST_LOOP(fmaxnmp, fmaxnmp_vector, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]), Rm_elsz);

    reg_id_t Rd_1_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[6] = {
        "fmaxnmp %q0 %q0 $0x02 -> %q0",    "fmaxnmp %q6 %q7 $0x02 -> %q5",
        "fmaxnmp %q11 %q12 $0x02 -> %q10", "fmaxnmp %q17 %q18 $0x02 -> %q16",
        "fmaxnmp %q22 %q23 $0x02 -> %q21", "fmaxnmp %q31 %q31 $0x02 -> %q31",
    };
    TEST_LOOP(fmaxnmp, fmaxnmp_vector, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]), Rm_elsz);

    reg_id_t Rd_1_2[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_2[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_2[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[6] = {
        "fmaxnmp %q0 %q0 $0x03 -> %q0",    "fmaxnmp %q6 %q7 $0x03 -> %q5",
        "fmaxnmp %q11 %q12 $0x03 -> %q10", "fmaxnmp %q17 %q18 $0x03 -> %q16",
        "fmaxnmp %q22 %q23 $0x03 -> %q21", "fmaxnmp %q31 %q31 $0x03 -> %q31",
    };
    TEST_LOOP(fmaxnmp, fmaxnmp_vector, 6, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), opnd_create_reg(Rm_1_2[i]), Rm_elsz);
}

TEST_INSTR(fmaxnmp_scalar)
{
    opnd_t Rn_elsz;

    /* Testing FMAXNMP <Hd>, <Hn>.2H */
    reg_id_t Rd_0_0[6] = { DR_REG_H0,  DR_REG_H5,  DR_REG_H10,
                           DR_REG_H16, DR_REG_H21, DR_REG_H31 };
    reg_id_t Rn_0_0[6] = { DR_REG_S0,  DR_REG_S6,  DR_REG_S11,
                           DR_REG_S17, DR_REG_S22, DR_REG_S31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[6] = {
        "fmaxnmp %s0 $0x01 -> %h0",   "fmaxnmp %s6 $0x01 -> %h5",
        "fmaxnmp %s11 $0x01 -> %h10", "fmaxnmp %s17 $0x01 -> %h16",
        "fmaxnmp %s22 $0x01 -> %h21", "fmaxnmp %s31 $0x01 -> %h31",
    };
    TEST_LOOP(fmaxnmp, fmaxnmp_scalar, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), Rn_elsz);

    /* Testing FMAXNMP <V><d>, <Sn>.<Ts> */
    reg_id_t Rd_1_0[6] = { DR_REG_S0,  DR_REG_S5,  DR_REG_S10,
                           DR_REG_S16, DR_REG_S21, DR_REG_S31 };
    reg_id_t Rn_1_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[6] = {
        "fmaxnmp %d0 $0x02 -> %s0",   "fmaxnmp %d6 $0x02 -> %s5",
        "fmaxnmp %d11 $0x02 -> %s10", "fmaxnmp %d17 $0x02 -> %s16",
        "fmaxnmp %d22 $0x02 -> %s21", "fmaxnmp %d31 $0x02 -> %s31",
    };
    TEST_LOOP(fmaxnmp, fmaxnmp_scalar, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), Rn_elsz);

    reg_id_t Rd_1_1[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_1[6] = {
        "fmaxnmp %q0 $0x03 -> %d0",   "fmaxnmp %q6 $0x03 -> %d5",
        "fmaxnmp %q11 $0x03 -> %d10", "fmaxnmp %q17 $0x03 -> %d16",
        "fmaxnmp %q22 $0x03 -> %d21", "fmaxnmp %q31 $0x03 -> %d31",
    };
    TEST_LOOP(fmaxnmp, fmaxnmp_scalar, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), Rn_elsz);
}

TEST_INSTR(fmaxp_vector)
{
    opnd_t Rm_elsz;

    /* Testing FMAXP   <Hd>.<Ts>, <Hn>.<Ts>, <Hm>.<Ts> */
    reg_id_t Rd_0_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_0_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_0_0[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[6] = {
        "fmaxp  %d0 %d0 $0x01 -> %d0",    "fmaxp  %d6 %d7 $0x01 -> %d5",
        "fmaxp  %d11 %d12 $0x01 -> %d10", "fmaxp  %d17 %d18 $0x01 -> %d16",
        "fmaxp  %d22 %d23 $0x01 -> %d21", "fmaxp  %d31 %d31 $0x01 -> %d31",
    };
    TEST_LOOP(fmaxp, fmaxp_vector, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]), Rm_elsz);

    reg_id_t Rd_0_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_0_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_0_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[6] = {
        "fmaxp  %q0 %q0 $0x01 -> %q0",    "fmaxp  %q6 %q7 $0x01 -> %q5",
        "fmaxp  %q11 %q12 $0x01 -> %q10", "fmaxp  %q17 %q18 $0x01 -> %q16",
        "fmaxp  %q22 %q23 $0x01 -> %q21", "fmaxp  %q31 %q31 $0x01 -> %q31",
    };
    TEST_LOOP(fmaxp, fmaxp_vector, 6, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]), Rm_elsz);

    /* Testing FMAXP   <Dd>.<Ts>, <Dn>.<Ts>, <Dm>.<Ts> */
    reg_id_t Rd_1_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_1_0[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[6] = {
        "fmaxp  %d0 %d0 $0x02 -> %d0",    "fmaxp  %d6 %d7 $0x02 -> %d5",
        "fmaxp  %d11 %d12 $0x02 -> %d10", "fmaxp  %d17 %d18 $0x02 -> %d16",
        "fmaxp  %d22 %d23 $0x02 -> %d21", "fmaxp  %d31 %d31 $0x02 -> %d31",
    };
    TEST_LOOP(fmaxp, fmaxp_vector, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]), Rm_elsz);

    reg_id_t Rd_1_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[6] = {
        "fmaxp  %q0 %q0 $0x02 -> %q0",    "fmaxp  %q6 %q7 $0x02 -> %q5",
        "fmaxp  %q11 %q12 $0x02 -> %q10", "fmaxp  %q17 %q18 $0x02 -> %q16",
        "fmaxp  %q22 %q23 $0x02 -> %q21", "fmaxp  %q31 %q31 $0x02 -> %q31",
    };
    TEST_LOOP(fmaxp, fmaxp_vector, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]), Rm_elsz);

    reg_id_t Rd_1_2[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_2[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_2[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[6] = {
        "fmaxp  %q0 %q0 $0x03 -> %q0",    "fmaxp  %q6 %q7 $0x03 -> %q5",
        "fmaxp  %q11 %q12 $0x03 -> %q10", "fmaxp  %q17 %q18 $0x03 -> %q16",
        "fmaxp  %q22 %q23 $0x03 -> %q21", "fmaxp  %q31 %q31 $0x03 -> %q31",
    };
    TEST_LOOP(fmaxp, fmaxp_vector, 6, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), opnd_create_reg(Rm_1_2[i]), Rm_elsz);
}

TEST_INSTR(fmaxp_scalar)
{
    opnd_t Rn_elsz;

    /* Testing FMAXP   <Hd>, <Hn>.2H */
    reg_id_t Rd_0_0[6] = { DR_REG_H0,  DR_REG_H5,  DR_REG_H10,
                           DR_REG_H16, DR_REG_H21, DR_REG_H31 };
    reg_id_t Rn_0_0[6] = { DR_REG_S0,  DR_REG_S6,  DR_REG_S11,
                           DR_REG_S17, DR_REG_S22, DR_REG_S31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[6] = {
        "fmaxp  %s0 $0x01 -> %h0",   "fmaxp  %s6 $0x01 -> %h5",
        "fmaxp  %s11 $0x01 -> %h10", "fmaxp  %s17 $0x01 -> %h16",
        "fmaxp  %s22 $0x01 -> %h21", "fmaxp  %s31 $0x01 -> %h31",
    };
    TEST_LOOP(fmaxp, fmaxp_scalar, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), Rn_elsz);

    /* Testing FMAXP   <V><d>, <Sn>.<Ts> */
    reg_id_t Rd_1_0[6] = { DR_REG_S0,  DR_REG_S5,  DR_REG_S10,
                           DR_REG_S16, DR_REG_S21, DR_REG_S31 };
    reg_id_t Rn_1_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[6] = {
        "fmaxp  %d0 $0x02 -> %s0",   "fmaxp  %d6 $0x02 -> %s5",
        "fmaxp  %d11 $0x02 -> %s10", "fmaxp  %d17 $0x02 -> %s16",
        "fmaxp  %d22 $0x02 -> %s21", "fmaxp  %d31 $0x02 -> %s31",
    };
    TEST_LOOP(fmaxp, fmaxp_scalar, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), Rn_elsz);

    reg_id_t Rd_1_1[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_1[6] = {
        "fmaxp  %q0 $0x03 -> %d0",   "fmaxp  %q6 $0x03 -> %d5",
        "fmaxp  %q11 $0x03 -> %d10", "fmaxp  %q17 $0x03 -> %d16",
        "fmaxp  %q22 $0x03 -> %d21", "fmaxp  %q31 $0x03 -> %d31",
    };
    TEST_LOOP(fmaxp, fmaxp_scalar, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), Rn_elsz);
}

TEST_INSTR(fminnmp_vector)
{
    opnd_t Rm_elsz;

    /* Testing FMINNMP <Hd>.<Ts>, <Hn>.<Ts>, <Hm>.<Ts> */
    reg_id_t Rd_0_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_0_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_0_0[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[6] = {
        "fminnmp %d0 %d0 $0x01 -> %d0",    "fminnmp %d6 %d7 $0x01 -> %d5",
        "fminnmp %d11 %d12 $0x01 -> %d10", "fminnmp %d17 %d18 $0x01 -> %d16",
        "fminnmp %d22 %d23 $0x01 -> %d21", "fminnmp %d31 %d31 $0x01 -> %d31",
    };
    TEST_LOOP(fminnmp, fminnmp_vector, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]), Rm_elsz);

    reg_id_t Rd_0_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_0_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_0_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[6] = {
        "fminnmp %q0 %q0 $0x01 -> %q0",    "fminnmp %q6 %q7 $0x01 -> %q5",
        "fminnmp %q11 %q12 $0x01 -> %q10", "fminnmp %q17 %q18 $0x01 -> %q16",
        "fminnmp %q22 %q23 $0x01 -> %q21", "fminnmp %q31 %q31 $0x01 -> %q31",
    };
    TEST_LOOP(fminnmp, fminnmp_vector, 6, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]), Rm_elsz);

    /* Testing FMINNMP <Dd>.<Ts>, <Dn>.<Ts>, <Dm>.<Ts> */
    reg_id_t Rd_1_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_1_0[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[6] = {
        "fminnmp %d0 %d0 $0x02 -> %d0",    "fminnmp %d6 %d7 $0x02 -> %d5",
        "fminnmp %d11 %d12 $0x02 -> %d10", "fminnmp %d17 %d18 $0x02 -> %d16",
        "fminnmp %d22 %d23 $0x02 -> %d21", "fminnmp %d31 %d31 $0x02 -> %d31",
    };
    TEST_LOOP(fminnmp, fminnmp_vector, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]), Rm_elsz);

    reg_id_t Rd_1_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[6] = {
        "fminnmp %q0 %q0 $0x02 -> %q0",    "fminnmp %q6 %q7 $0x02 -> %q5",
        "fminnmp %q11 %q12 $0x02 -> %q10", "fminnmp %q17 %q18 $0x02 -> %q16",
        "fminnmp %q22 %q23 $0x02 -> %q21", "fminnmp %q31 %q31 $0x02 -> %q31",
    };
    TEST_LOOP(fminnmp, fminnmp_vector, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]), Rm_elsz);

    reg_id_t Rd_1_2[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_2[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_2[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[6] = {
        "fminnmp %q0 %q0 $0x03 -> %q0",    "fminnmp %q6 %q7 $0x03 -> %q5",
        "fminnmp %q11 %q12 $0x03 -> %q10", "fminnmp %q17 %q18 $0x03 -> %q16",
        "fminnmp %q22 %q23 $0x03 -> %q21", "fminnmp %q31 %q31 $0x03 -> %q31",
    };
    TEST_LOOP(fminnmp, fminnmp_vector, 6, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), opnd_create_reg(Rm_1_2[i]), Rm_elsz);
}

TEST_INSTR(fminnmp_scalar)
{
    opnd_t Rn_elsz;

    /* Testing FMINNMP <Hd>, <Hn>.2H */
    reg_id_t Rd_0_0[6] = { DR_REG_H0,  DR_REG_H5,  DR_REG_H10,
                           DR_REG_H16, DR_REG_H21, DR_REG_H31 };
    reg_id_t Rn_0_0[6] = { DR_REG_S0,  DR_REG_S6,  DR_REG_S11,
                           DR_REG_S17, DR_REG_S22, DR_REG_S31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[6] = {
        "fminnmp %s0 $0x01 -> %h0",   "fminnmp %s6 $0x01 -> %h5",
        "fminnmp %s11 $0x01 -> %h10", "fminnmp %s17 $0x01 -> %h16",
        "fminnmp %s22 $0x01 -> %h21", "fminnmp %s31 $0x01 -> %h31",
    };
    TEST_LOOP(fminnmp, fminnmp_scalar, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), Rn_elsz);

    /* Testing FMINNMP <V><d>, <Sn>.<Ts> */
    reg_id_t Rd_1_0[6] = { DR_REG_S0,  DR_REG_S5,  DR_REG_S10,
                           DR_REG_S16, DR_REG_S21, DR_REG_S31 };
    reg_id_t Rn_1_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[6] = {
        "fminnmp %d0 $0x02 -> %s0",   "fminnmp %d6 $0x02 -> %s5",
        "fminnmp %d11 $0x02 -> %s10", "fminnmp %d17 $0x02 -> %s16",
        "fminnmp %d22 $0x02 -> %s21", "fminnmp %d31 $0x02 -> %s31",
    };
    TEST_LOOP(fminnmp, fminnmp_scalar, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), Rn_elsz);

    reg_id_t Rd_1_1[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_1[6] = {
        "fminnmp %q0 $0x03 -> %d0",   "fminnmp %q6 $0x03 -> %d5",
        "fminnmp %q11 $0x03 -> %d10", "fminnmp %q17 $0x03 -> %d16",
        "fminnmp %q22 $0x03 -> %d21", "fminnmp %q31 $0x03 -> %d31",
    };
    TEST_LOOP(fminnmp, fminnmp_scalar, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), Rn_elsz);
}

TEST_INSTR(fminnmv_vector)
{
    opnd_t Rn_elsz;

    /* Testing FMINNMV <Hd>, <Hn>.<Ts> */
    reg_id_t Rd_0_0[6] = { DR_REG_H0,  DR_REG_H5,  DR_REG_H10,
                           DR_REG_H16, DR_REG_H21, DR_REG_H31 };
    reg_id_t Rn_0_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[6] = {
        "fminnmv %d0 $0x01 -> %h0",   "fminnmv %d6 $0x01 -> %h5",
        "fminnmv %d11 $0x01 -> %h10", "fminnmv %d17 $0x01 -> %h16",
        "fminnmv %d22 $0x01 -> %h21", "fminnmv %d31 $0x01 -> %h31",
    };
    TEST_LOOP(fminnmv, fminnmv_vector, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), Rn_elsz);

    reg_id_t Rd_0_1[6] = { DR_REG_H0,  DR_REG_H5,  DR_REG_H10,
                           DR_REG_H16, DR_REG_H21, DR_REG_H31 };
    reg_id_t Rn_0_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[6] = {
        "fminnmv %q0 $0x01 -> %h0",   "fminnmv %q6 $0x01 -> %h5",
        "fminnmv %q11 $0x01 -> %h10", "fminnmv %q17 $0x01 -> %h16",
        "fminnmv %q22 $0x01 -> %h21", "fminnmv %q31 $0x01 -> %h31",
    };
    TEST_LOOP(fminnmv, fminnmv_vector, 6, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), Rn_elsz);

    /* Testing FMINNMV <Sd>, <Sn>.4S */
    reg_id_t Rd_1_0[6] = { DR_REG_S0,  DR_REG_S5,  DR_REG_S10,
                           DR_REG_S16, DR_REG_S21, DR_REG_S31 };
    reg_id_t Rn_1_0[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[6] = {
        "fminnmv %q0 $0x02 -> %s0",   "fminnmv %q6 $0x02 -> %s5",
        "fminnmv %q11 $0x02 -> %s10", "fminnmv %q17 $0x02 -> %s16",
        "fminnmv %q22 $0x02 -> %s21", "fminnmv %q31 $0x02 -> %s31",
    };
    TEST_LOOP(fminnmv, fminnmv_vector, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), Rn_elsz);
}

TEST_INSTR(fminp_vector)
{
    opnd_t Rm_elsz;

    /* Testing FMINP   <Hd>.<Ts>, <Hn>.<Ts>, <Hm>.<Ts> */
    reg_id_t Rd_0_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_0_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_0_0[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[6] = {
        "fminp  %d0 %d0 $0x01 -> %d0",    "fminp  %d6 %d7 $0x01 -> %d5",
        "fminp  %d11 %d12 $0x01 -> %d10", "fminp  %d17 %d18 $0x01 -> %d16",
        "fminp  %d22 %d23 $0x01 -> %d21", "fminp  %d31 %d31 $0x01 -> %d31",
    };
    TEST_LOOP(fminp, fminp_vector, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]), Rm_elsz);

    reg_id_t Rd_0_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_0_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_0_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[6] = {
        "fminp  %q0 %q0 $0x01 -> %q0",    "fminp  %q6 %q7 $0x01 -> %q5",
        "fminp  %q11 %q12 $0x01 -> %q10", "fminp  %q17 %q18 $0x01 -> %q16",
        "fminp  %q22 %q23 $0x01 -> %q21", "fminp  %q31 %q31 $0x01 -> %q31",
    };
    TEST_LOOP(fminp, fminp_vector, 6, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]), Rm_elsz);

    /* Testing FMINP   <Dd>.<Ts>, <Dn>.<Ts>, <Dm>.<Ts> */
    reg_id_t Rd_1_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_1_0[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[6] = {
        "fminp  %d0 %d0 $0x02 -> %d0",    "fminp  %d6 %d7 $0x02 -> %d5",
        "fminp  %d11 %d12 $0x02 -> %d10", "fminp  %d17 %d18 $0x02 -> %d16",
        "fminp  %d22 %d23 $0x02 -> %d21", "fminp  %d31 %d31 $0x02 -> %d31",
    };
    TEST_LOOP(fminp, fminp_vector, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]), Rm_elsz);

    reg_id_t Rd_1_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[6] = {
        "fminp  %q0 %q0 $0x02 -> %q0",    "fminp  %q6 %q7 $0x02 -> %q5",
        "fminp  %q11 %q12 $0x02 -> %q10", "fminp  %q17 %q18 $0x02 -> %q16",
        "fminp  %q22 %q23 $0x02 -> %q21", "fminp  %q31 %q31 $0x02 -> %q31",
    };
    TEST_LOOP(fminp, fminp_vector, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]), Rm_elsz);

    reg_id_t Rd_1_2[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_2[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_2[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[6] = {
        "fminp  %q0 %q0 $0x03 -> %q0",    "fminp  %q6 %q7 $0x03 -> %q5",
        "fminp  %q11 %q12 $0x03 -> %q10", "fminp  %q17 %q18 $0x03 -> %q16",
        "fminp  %q22 %q23 $0x03 -> %q21", "fminp  %q31 %q31 $0x03 -> %q31",
    };
    TEST_LOOP(fminp, fminp_vector, 6, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), opnd_create_reg(Rm_1_2[i]), Rm_elsz);
}

TEST_INSTR(fminp_scalar)
{
    opnd_t Rn_elsz;

    /* Testing FMINP   <Hd>, <Hn>.2H */
    reg_id_t Rd_0_0[6] = { DR_REG_H0,  DR_REG_H5,  DR_REG_H10,
                           DR_REG_H16, DR_REG_H21, DR_REG_H31 };
    reg_id_t Rn_0_0[6] = { DR_REG_S0,  DR_REG_S6,  DR_REG_S11,
                           DR_REG_S17, DR_REG_S22, DR_REG_S31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[6] = {
        "fminp  %s0 $0x01 -> %h0",   "fminp  %s6 $0x01 -> %h5",
        "fminp  %s11 $0x01 -> %h10", "fminp  %s17 $0x01 -> %h16",
        "fminp  %s22 $0x01 -> %h21", "fminp  %s31 $0x01 -> %h31",
    };
    TEST_LOOP(fminp, fminp_scalar, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), Rn_elsz);

    /* Testing FMINP   <V><d>, <Sn>.<Ts> */
    reg_id_t Rd_1_0[6] = { DR_REG_S0,  DR_REG_S5,  DR_REG_S10,
                           DR_REG_S16, DR_REG_S21, DR_REG_S31 };
    reg_id_t Rn_1_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[6] = {
        "fminp  %d0 $0x02 -> %s0",   "fminp  %d6 $0x02 -> %s5",
        "fminp  %d11 $0x02 -> %s10", "fminp  %d17 $0x02 -> %s16",
        "fminp  %d22 $0x02 -> %s21", "fminp  %d31 $0x02 -> %s31",
    };
    TEST_LOOP(fminp, fminp_scalar, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), Rn_elsz);

    reg_id_t Rd_1_1[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_1[6] = {
        "fminp  %q0 $0x03 -> %d0",   "fminp  %q6 $0x03 -> %d5",
        "fminp  %q11 $0x03 -> %d10", "fminp  %q17 $0x03 -> %d16",
        "fminp  %q22 $0x03 -> %d21", "fminp  %q31 $0x03 -> %d31",
    };
    TEST_LOOP(fminp, fminp_scalar, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), Rn_elsz);
}

TEST_INSTR(fmla_vector)
{
    opnd_t Rm_elsz;

    /* Testing FMLA    <Hd>.<Ts>, <Hn>.<Ts>, <Hm>.<Ts> */
    reg_id_t Rd_0_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_0_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_0_0[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[6] = {
        "fmla   %d0 %d0 %d0 $0x01 -> %d0",     "fmla   %d5 %d6 %d7 $0x01 -> %d5",
        "fmla   %d10 %d11 %d12 $0x01 -> %d10", "fmla   %d16 %d17 %d18 $0x01 -> %d16",
        "fmla   %d21 %d22 %d23 $0x01 -> %d21", "fmla   %d31 %d31 %d31 $0x01 -> %d31",
    };
    TEST_LOOP(fmla, fmla_vector, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]), Rm_elsz);

    reg_id_t Rd_0_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_0_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_0_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[6] = {
        "fmla   %q0 %q0 %q0 $0x01 -> %q0",     "fmla   %q5 %q6 %q7 $0x01 -> %q5",
        "fmla   %q10 %q11 %q12 $0x01 -> %q10", "fmla   %q16 %q17 %q18 $0x01 -> %q16",
        "fmla   %q21 %q22 %q23 $0x01 -> %q21", "fmla   %q31 %q31 %q31 $0x01 -> %q31",
    };
    TEST_LOOP(fmla, fmla_vector, 6, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]), Rm_elsz);

    /* Testing FMLA    <Dd>.<Ts>, <Dn>.<Ts>, <Dm>.<Ts> */
    reg_id_t Rd_1_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_1_0[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[6] = {
        "fmla   %d0 %d0 %d0 $0x02 -> %d0",     "fmla   %d5 %d6 %d7 $0x02 -> %d5",
        "fmla   %d10 %d11 %d12 $0x02 -> %d10", "fmla   %d16 %d17 %d18 $0x02 -> %d16",
        "fmla   %d21 %d22 %d23 $0x02 -> %d21", "fmla   %d31 %d31 %d31 $0x02 -> %d31",
    };
    TEST_LOOP(fmla, fmla_vector, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]), Rm_elsz);

    reg_id_t Rd_1_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[6] = {
        "fmla   %q0 %q0 %q0 $0x02 -> %q0",     "fmla   %q5 %q6 %q7 $0x02 -> %q5",
        "fmla   %q10 %q11 %q12 $0x02 -> %q10", "fmla   %q16 %q17 %q18 $0x02 -> %q16",
        "fmla   %q21 %q22 %q23 $0x02 -> %q21", "fmla   %q31 %q31 %q31 $0x02 -> %q31",
    };
    TEST_LOOP(fmla, fmla_vector, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]), Rm_elsz);

    reg_id_t Rd_1_2[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_2[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_2[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[6] = {
        "fmla   %q0 %q0 %q0 $0x03 -> %q0",     "fmla   %q5 %q6 %q7 $0x03 -> %q5",
        "fmla   %q10 %q11 %q12 $0x03 -> %q10", "fmla   %q16 %q17 %q18 $0x03 -> %q16",
        "fmla   %q21 %q22 %q23 $0x03 -> %q21", "fmla   %q31 %q31 %q31 $0x03 -> %q31",
    };
    TEST_LOOP(fmla, fmla_vector, 6, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), opnd_create_reg(Rm_1_2[i]), Rm_elsz);
}

TEST_INSTR(fmla_vector_idx)
{
    opnd_t Rm_elsz;

    /* Testing FMLA    <Hd>.<Ts>, <Hn>.<Ts>, <Hm>.H[<index>] */
    reg_id_t Rd_0_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_0_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_0_0[6] = { DR_REG_D0,  DR_REG_D4,  DR_REG_D7,
                           DR_REG_D10, DR_REG_D12, DR_REG_D15 };
    uint index_0_0[6] = { 0, 4, 5, 7, 0, 7 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[6] = {
        "fmla   %d0 %d0 %d0 $0x00 $0x01 -> %d0",
        "fmla   %d5 %d6 %d4 $0x04 $0x01 -> %d5",
        "fmla   %d10 %d11 %d7 $0x05 $0x01 -> %d10",
        "fmla   %d16 %d17 %d10 $0x07 $0x01 -> %d16",
        "fmla   %d21 %d22 %d12 $0x00 $0x01 -> %d21",
        "fmla   %d31 %d31 %d15 $0x07 $0x01 -> %d31",
    };
    TEST_LOOP(fmla, fmla_vector_idx, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]),
              opnd_create_immed_uint(index_0_0[i], OPSZ_0), Rm_elsz);

    reg_id_t Rd_0_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_0_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_0_1[6] = { DR_REG_Q0,  DR_REG_Q4,  DR_REG_Q7,
                           DR_REG_Q10, DR_REG_Q12, DR_REG_Q15 };
    uint index_0_1[6] = { 0, 4, 5, 7, 0, 7 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[6] = {
        "fmla   %q0 %q0 %q0 $0x00 $0x01 -> %q0",
        "fmla   %q5 %q6 %q4 $0x04 $0x01 -> %q5",
        "fmla   %q10 %q11 %q7 $0x05 $0x01 -> %q10",
        "fmla   %q16 %q17 %q10 $0x07 $0x01 -> %q16",
        "fmla   %q21 %q22 %q12 $0x00 $0x01 -> %q21",
        "fmla   %q31 %q31 %q15 $0x07 $0x01 -> %q31",
    };
    TEST_LOOP(fmla, fmla_vector_idx, 6, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]),
              opnd_create_immed_uint(index_0_1[i], OPSZ_0), Rm_elsz);

    /* Testing FMLA    <Dd>.<Ts>, <Dn>.<Ts>, <Dm>.<Tb>[<index>] */
    reg_id_t Rd_1_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_1_0[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    uint index_1_0[6] = { 0, 3, 0, 1, 1, 3 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[6] = {
        "fmla   %d0 %d0 %d0 $0x00 $0x02 -> %d0",
        "fmla   %d5 %d6 %d7 $0x03 $0x02 -> %d5",
        "fmla   %d10 %d11 %d12 $0x00 $0x02 -> %d10",
        "fmla   %d16 %d17 %d18 $0x01 $0x02 -> %d16",
        "fmla   %d21 %d22 %d23 $0x01 $0x02 -> %d21",
        "fmla   %d31 %d31 %d31 $0x03 $0x02 -> %d31",
    };
    TEST_LOOP(fmla, fmla_vector_idx, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]),
              opnd_create_immed_uint(index_1_0[i], OPSZ_0), Rm_elsz);

    reg_id_t Rd_1_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    uint index_1_1[6] = { 0, 3, 0, 1, 1, 3 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[6] = {
        "fmla   %q0 %q0 %q0 $0x00 $0x02 -> %q0",
        "fmla   %q5 %q6 %q7 $0x03 $0x02 -> %q5",
        "fmla   %q10 %q11 %q12 $0x00 $0x02 -> %q10",
        "fmla   %q16 %q17 %q18 $0x01 $0x02 -> %q16",
        "fmla   %q21 %q22 %q23 $0x01 $0x02 -> %q21",
        "fmla   %q31 %q31 %q31 $0x03 $0x02 -> %q31",
    };
    TEST_LOOP(fmla, fmla_vector_idx, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]),
              opnd_create_immed_uint(index_1_1[i], OPSZ_0), Rm_elsz);

    reg_id_t Rd_1_2[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_2[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_2[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    uint index_1_2[6] = { 0, 1, 1, 1, 0, 1 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[6] = {
        "fmla   %q0 %q0 %q0 $0x00 $0x03 -> %q0",
        "fmla   %q5 %q6 %q7 $0x01 $0x03 -> %q5",
        "fmla   %q10 %q11 %q12 $0x01 $0x03 -> %q10",
        "fmla   %q16 %q17 %q18 $0x01 $0x03 -> %q16",
        "fmla   %q21 %q22 %q23 $0x00 $0x03 -> %q21",
        "fmla   %q31 %q31 %q31 $0x01 $0x03 -> %q31",
    };
    TEST_LOOP(fmla, fmla_vector_idx, 6, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), opnd_create_reg(Rm_1_2[i]),
              opnd_create_immed_uint(index_1_2[i], OPSZ_0), Rm_elsz);

    /* Testing FMLA    <Hd>, <Hn>, <Hm>.H[<index>] */
    reg_id_t Rd_2_0[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_2_0[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_2_0[6] = { DR_REG_Q0,  DR_REG_Q4,  DR_REG_Q7,
                           DR_REG_Q10, DR_REG_Q12, DR_REG_Q15 };
    uint index_2_0[6] = { 0, 4, 5, 7, 0, 7 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_2_0[6] = {
        "fmla   %q0 %q0 %q0 $0x00 $0x01 -> %q0",
        "fmla   %q5 %q6 %q4 $0x04 $0x01 -> %q5",
        "fmla   %q10 %q11 %q7 $0x05 $0x01 -> %q10",
        "fmla   %q16 %q17 %q10 $0x07 $0x01 -> %q16",
        "fmla   %q21 %q22 %q12 $0x00 $0x01 -> %q21",
        "fmla   %q31 %q31 %q15 $0x07 $0x01 -> %q31",
    };
    TEST_LOOP(fmla, fmla_vector_idx, 6, expected_2_0[i], opnd_create_reg(Rd_2_0[i]),
              opnd_create_reg(Rn_2_0[i]), opnd_create_reg(Rm_2_0[i]),
              opnd_create_immed_uint(index_2_0[i], OPSZ_0), Rm_elsz);

    /* Testing FMLA    <V><d>, <V><n>, <Sm>.<Ts>[<index>] */
    reg_id_t Rd_3_0[6] = { DR_REG_S0,  DR_REG_S5,  DR_REG_S10,
                           DR_REG_S16, DR_REG_S21, DR_REG_S31 };
    reg_id_t Rn_3_0[6] = { DR_REG_S0,  DR_REG_S6,  DR_REG_S11,
                           DR_REG_S17, DR_REG_S22, DR_REG_S31 };
    reg_id_t Rm_3_0[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    uint index_3_0[6] = { 0, 3, 0, 1, 1, 3 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_3_0[6] = {
        "fmla   %s0 %s0 %q0 $0x00 $0x02 -> %s0",
        "fmla   %s5 %s6 %q7 $0x03 $0x02 -> %s5",
        "fmla   %s10 %s11 %q12 $0x00 $0x02 -> %s10",
        "fmla   %s16 %s17 %q18 $0x01 $0x02 -> %s16",
        "fmla   %s21 %s22 %q23 $0x01 $0x02 -> %s21",
        "fmla   %s31 %s31 %q31 $0x03 $0x02 -> %s31",
    };
    TEST_LOOP(fmla, fmla_vector_idx, 6, expected_3_0[i], opnd_create_reg(Rd_3_0[i]),
              opnd_create_reg(Rn_3_0[i]), opnd_create_reg(Rm_3_0[i]),
              opnd_create_immed_uint(index_3_0[i], OPSZ_0), Rm_elsz);

    reg_id_t Rd_3_1[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_3_1[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_3_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    uint index_3_1[6] = { 0, 1, 1, 1, 0, 1 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_3_1[6] = {
        "fmla   %d0 %d0 %q0 $0x00 $0x03 -> %d0",
        "fmla   %d5 %d6 %q7 $0x01 $0x03 -> %d5",
        "fmla   %d10 %d11 %q12 $0x01 $0x03 -> %d10",
        "fmla   %d16 %d17 %q18 $0x01 $0x03 -> %d16",
        "fmla   %d21 %d22 %q23 $0x00 $0x03 -> %d21",
        "fmla   %d31 %d31 %q31 $0x01 $0x03 -> %d31",
    };
    TEST_LOOP(fmla, fmla_vector_idx, 6, expected_3_1[i], opnd_create_reg(Rd_3_1[i]),
              opnd_create_reg(Rn_3_1[i]), opnd_create_reg(Rm_3_1[i]),
              opnd_create_immed_uint(index_3_1[i], OPSZ_0), Rm_elsz);
}

TEST_INSTR(fmls_vector)
{
    opnd_t Rm_elsz;

    /* Testing FMLS    <Hd>.<Ts>, <Hn>.<Ts>, <Hm>.<Ts> */
    reg_id_t Rd_0_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_0_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_0_0[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[6] = {
        "fmls   %d0 %d0 %d0 $0x01 -> %d0",     "fmls   %d5 %d6 %d7 $0x01 -> %d5",
        "fmls   %d10 %d11 %d12 $0x01 -> %d10", "fmls   %d16 %d17 %d18 $0x01 -> %d16",
        "fmls   %d21 %d22 %d23 $0x01 -> %d21", "fmls   %d31 %d31 %d31 $0x01 -> %d31",
    };
    TEST_LOOP(fmls, fmls_vector, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]), Rm_elsz);

    reg_id_t Rd_0_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_0_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_0_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[6] = {
        "fmls   %q0 %q0 %q0 $0x01 -> %q0",     "fmls   %q5 %q6 %q7 $0x01 -> %q5",
        "fmls   %q10 %q11 %q12 $0x01 -> %q10", "fmls   %q16 %q17 %q18 $0x01 -> %q16",
        "fmls   %q21 %q22 %q23 $0x01 -> %q21", "fmls   %q31 %q31 %q31 $0x01 -> %q31",
    };
    TEST_LOOP(fmls, fmls_vector, 6, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]), Rm_elsz);

    /* Testing FMLS    <Dd>.<Ts>, <Dn>.<Ts>, <Dm>.<Ts> */
    reg_id_t Rd_1_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_1_0[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[6] = {
        "fmls   %d0 %d0 %d0 $0x02 -> %d0",     "fmls   %d5 %d6 %d7 $0x02 -> %d5",
        "fmls   %d10 %d11 %d12 $0x02 -> %d10", "fmls   %d16 %d17 %d18 $0x02 -> %d16",
        "fmls   %d21 %d22 %d23 $0x02 -> %d21", "fmls   %d31 %d31 %d31 $0x02 -> %d31",
    };
    TEST_LOOP(fmls, fmls_vector, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]), Rm_elsz);

    reg_id_t Rd_1_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[6] = {
        "fmls   %q0 %q0 %q0 $0x02 -> %q0",     "fmls   %q5 %q6 %q7 $0x02 -> %q5",
        "fmls   %q10 %q11 %q12 $0x02 -> %q10", "fmls   %q16 %q17 %q18 $0x02 -> %q16",
        "fmls   %q21 %q22 %q23 $0x02 -> %q21", "fmls   %q31 %q31 %q31 $0x02 -> %q31",
    };
    TEST_LOOP(fmls, fmls_vector, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]), Rm_elsz);

    reg_id_t Rd_1_2[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_2[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_2[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[6] = {
        "fmls   %q0 %q0 %q0 $0x03 -> %q0",     "fmls   %q5 %q6 %q7 $0x03 -> %q5",
        "fmls   %q10 %q11 %q12 $0x03 -> %q10", "fmls   %q16 %q17 %q18 $0x03 -> %q16",
        "fmls   %q21 %q22 %q23 $0x03 -> %q21", "fmls   %q31 %q31 %q31 $0x03 -> %q31",
    };
    TEST_LOOP(fmls, fmls_vector, 6, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), opnd_create_reg(Rm_1_2[i]), Rm_elsz);
}

TEST_INSTR(fmls_vector_idx)
{
    opnd_t Rm_elsz;

    /* Testing FMLS    <Hd>.<Ts>, <Hn>.<Ts>, <Hm>.H[<index>] */
    reg_id_t Rd_0_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_0_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_0_0[6] = { DR_REG_D0,  DR_REG_D4,  DR_REG_D7,
                           DR_REG_D10, DR_REG_D12, DR_REG_D15 };
    uint index_0_0[6] = { 0, 4, 5, 7, 0, 7 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[6] = {
        "fmls   %d0 %d0 %d0 $0x00 $0x01 -> %d0",
        "fmls   %d5 %d6 %d4 $0x04 $0x01 -> %d5",
        "fmls   %d10 %d11 %d7 $0x05 $0x01 -> %d10",
        "fmls   %d16 %d17 %d10 $0x07 $0x01 -> %d16",
        "fmls   %d21 %d22 %d12 $0x00 $0x01 -> %d21",
        "fmls   %d31 %d31 %d15 $0x07 $0x01 -> %d31",
    };
    TEST_LOOP(fmls, fmls_vector_idx, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]),
              opnd_create_immed_uint(index_0_0[i], OPSZ_0), Rm_elsz);

    reg_id_t Rd_0_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_0_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_0_1[6] = { DR_REG_Q0,  DR_REG_Q4,  DR_REG_Q7,
                           DR_REG_Q10, DR_REG_Q12, DR_REG_Q15 };
    uint index_0_1[6] = { 0, 4, 5, 7, 0, 7 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[6] = {
        "fmls   %q0 %q0 %q0 $0x00 $0x01 -> %q0",
        "fmls   %q5 %q6 %q4 $0x04 $0x01 -> %q5",
        "fmls   %q10 %q11 %q7 $0x05 $0x01 -> %q10",
        "fmls   %q16 %q17 %q10 $0x07 $0x01 -> %q16",
        "fmls   %q21 %q22 %q12 $0x00 $0x01 -> %q21",
        "fmls   %q31 %q31 %q15 $0x07 $0x01 -> %q31",
    };
    TEST_LOOP(fmls, fmls_vector_idx, 6, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]),
              opnd_create_immed_uint(index_0_1[i], OPSZ_0), Rm_elsz);

    /* Testing FMLS    <Dd>.<Ts>, <Dn>.<Ts>, <Dm>.<Tb>[<index>] */
    reg_id_t Rd_1_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_1_0[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    uint index_1_0[6] = { 0, 3, 0, 1, 1, 3 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[6] = {
        "fmls   %d0 %d0 %d0 $0x00 $0x02 -> %d0",
        "fmls   %d5 %d6 %d7 $0x03 $0x02 -> %d5",
        "fmls   %d10 %d11 %d12 $0x00 $0x02 -> %d10",
        "fmls   %d16 %d17 %d18 $0x01 $0x02 -> %d16",
        "fmls   %d21 %d22 %d23 $0x01 $0x02 -> %d21",
        "fmls   %d31 %d31 %d31 $0x03 $0x02 -> %d31",
    };
    TEST_LOOP(fmls, fmls_vector_idx, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]),
              opnd_create_immed_uint(index_1_0[i], OPSZ_0), Rm_elsz);

    reg_id_t Rd_1_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    uint index_1_1[6] = { 0, 3, 0, 1, 1, 3 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[6] = {
        "fmls   %q0 %q0 %q0 $0x00 $0x02 -> %q0",
        "fmls   %q5 %q6 %q7 $0x03 $0x02 -> %q5",
        "fmls   %q10 %q11 %q12 $0x00 $0x02 -> %q10",
        "fmls   %q16 %q17 %q18 $0x01 $0x02 -> %q16",
        "fmls   %q21 %q22 %q23 $0x01 $0x02 -> %q21",
        "fmls   %q31 %q31 %q31 $0x03 $0x02 -> %q31",
    };
    TEST_LOOP(fmls, fmls_vector_idx, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]),
              opnd_create_immed_uint(index_1_1[i], OPSZ_0), Rm_elsz);

    reg_id_t Rd_1_2[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_2[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_2[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    uint index_1_2[6] = { 0, 1, 1, 1, 0, 1 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[6] = {
        "fmls   %q0 %q0 %q0 $0x00 $0x03 -> %q0",
        "fmls   %q5 %q6 %q7 $0x01 $0x03 -> %q5",
        "fmls   %q10 %q11 %q12 $0x01 $0x03 -> %q10",
        "fmls   %q16 %q17 %q18 $0x01 $0x03 -> %q16",
        "fmls   %q21 %q22 %q23 $0x00 $0x03 -> %q21",
        "fmls   %q31 %q31 %q31 $0x01 $0x03 -> %q31",
    };
    TEST_LOOP(fmls, fmls_vector_idx, 6, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), opnd_create_reg(Rm_1_2[i]),
              opnd_create_immed_uint(index_1_2[i], OPSZ_0), Rm_elsz);

    /* Testing FMLS    <Hd>, <Hn>, <Hm>.H[<index>] */
    reg_id_t Rd_2_0[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_2_0[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_2_0[6] = { DR_REG_Q0,  DR_REG_Q4,  DR_REG_Q7,
                           DR_REG_Q10, DR_REG_Q12, DR_REG_Q15 };
    uint index_2_0[6] = { 0, 4, 5, 7, 0, 7 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_2_0[6] = {
        "fmls   %q0 %q0 %q0 $0x00 $0x01 -> %q0",
        "fmls   %q5 %q6 %q4 $0x04 $0x01 -> %q5",
        "fmls   %q10 %q11 %q7 $0x05 $0x01 -> %q10",
        "fmls   %q16 %q17 %q10 $0x07 $0x01 -> %q16",
        "fmls   %q21 %q22 %q12 $0x00 $0x01 -> %q21",
        "fmls   %q31 %q31 %q15 $0x07 $0x01 -> %q31",
    };
    TEST_LOOP(fmls, fmls_vector_idx, 6, expected_2_0[i], opnd_create_reg(Rd_2_0[i]),
              opnd_create_reg(Rn_2_0[i]), opnd_create_reg(Rm_2_0[i]),
              opnd_create_immed_uint(index_2_0[i], OPSZ_0), Rm_elsz);

    /* Testing FMLS    <V><d>, <V><n>, <Sm>.<Ts>[<index>] */
    reg_id_t Rd_3_0[6] = { DR_REG_S0,  DR_REG_S5,  DR_REG_S10,
                           DR_REG_S16, DR_REG_S21, DR_REG_S31 };
    reg_id_t Rn_3_0[6] = { DR_REG_S0,  DR_REG_S6,  DR_REG_S11,
                           DR_REG_S17, DR_REG_S22, DR_REG_S31 };
    reg_id_t Rm_3_0[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    uint index_3_0[6] = { 0, 3, 0, 1, 1, 3 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_3_0[6] = {
        "fmls   %s0 %s0 %q0 $0x00 $0x02 -> %s0",
        "fmls   %s5 %s6 %q7 $0x03 $0x02 -> %s5",
        "fmls   %s10 %s11 %q12 $0x00 $0x02 -> %s10",
        "fmls   %s16 %s17 %q18 $0x01 $0x02 -> %s16",
        "fmls   %s21 %s22 %q23 $0x01 $0x02 -> %s21",
        "fmls   %s31 %s31 %q31 $0x03 $0x02 -> %s31",
    };
    TEST_LOOP(fmls, fmls_vector_idx, 6, expected_3_0[i], opnd_create_reg(Rd_3_0[i]),
              opnd_create_reg(Rn_3_0[i]), opnd_create_reg(Rm_3_0[i]),
              opnd_create_immed_uint(index_3_0[i], OPSZ_0), Rm_elsz);

    reg_id_t Rd_3_1[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_3_1[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_3_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    uint index_3_1[6] = { 0, 1, 1, 1, 0, 1 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_3_1[6] = {
        "fmls   %d0 %d0 %q0 $0x00 $0x03 -> %d0",
        "fmls   %d5 %d6 %q7 $0x01 $0x03 -> %d5",
        "fmls   %d10 %d11 %q12 $0x01 $0x03 -> %d10",
        "fmls   %d16 %d17 %q18 $0x01 $0x03 -> %d16",
        "fmls   %d21 %d22 %q23 $0x00 $0x03 -> %d21",
        "fmls   %d31 %d31 %q31 $0x01 $0x03 -> %d31",
    };
    TEST_LOOP(fmls, fmls_vector_idx, 6, expected_3_1[i], opnd_create_reg(Rd_3_1[i]),
              opnd_create_reg(Rn_3_1[i]), opnd_create_reg(Rm_3_1[i]),
              opnd_create_immed_uint(index_3_1[i], OPSZ_0), Rm_elsz);
}

TEST_INSTR(frecpe_vector)
{
    opnd_t Rn_elsz;

    /* Testing FRECPE  <Hd>.<Ts>, <Hn>.<Ts> */
    reg_id_t Rd_0_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_0_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[6] = {
        "frecpe %d0 $0x01 -> %d0",   "frecpe %d6 $0x01 -> %d5",
        "frecpe %d11 $0x01 -> %d10", "frecpe %d17 $0x01 -> %d16",
        "frecpe %d22 $0x01 -> %d21", "frecpe %d31 $0x01 -> %d31",
    };
    TEST_LOOP(frecpe, frecpe_vector, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), Rn_elsz);

    reg_id_t Rd_0_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_0_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[6] = {
        "frecpe %q0 $0x01 -> %q0",   "frecpe %q6 $0x01 -> %q5",
        "frecpe %q11 $0x01 -> %q10", "frecpe %q17 $0x01 -> %q16",
        "frecpe %q22 $0x01 -> %q21", "frecpe %q31 $0x01 -> %q31",
    };
    TEST_LOOP(frecpe, frecpe_vector, 6, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), Rn_elsz);

    /* Testing FRECPE  <Dd>.<Ts>, <Dn>.<Ts> */
    reg_id_t Rd_1_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[6] = {
        "frecpe %d0 $0x02 -> %d0",   "frecpe %d6 $0x02 -> %d5",
        "frecpe %d11 $0x02 -> %d10", "frecpe %d17 $0x02 -> %d16",
        "frecpe %d22 $0x02 -> %d21", "frecpe %d31 $0x02 -> %d31",
    };
    TEST_LOOP(frecpe, frecpe_vector, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), Rn_elsz);

    reg_id_t Rd_1_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[6] = {
        "frecpe %q0 $0x02 -> %q0",   "frecpe %q6 $0x02 -> %q5",
        "frecpe %q11 $0x02 -> %q10", "frecpe %q17 $0x02 -> %q16",
        "frecpe %q22 $0x02 -> %q21", "frecpe %q31 $0x02 -> %q31",
    };
    TEST_LOOP(frecpe, frecpe_vector, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), Rn_elsz);

    reg_id_t Rd_1_2[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_2[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[6] = {
        "frecpe %q0 $0x03 -> %q0",   "frecpe %q6 $0x03 -> %q5",
        "frecpe %q11 $0x03 -> %q10", "frecpe %q17 $0x03 -> %q16",
        "frecpe %q22 $0x03 -> %q21", "frecpe %q31 $0x03 -> %q31",
    };
    TEST_LOOP(frecpe, frecpe_vector, 6, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), Rn_elsz);
}

TEST_INSTR(frecpe)
{

    /* Testing FRECPE  <Hd>, <Hn> */
    reg_id_t Rd_0_0[6] = { DR_REG_H0,  DR_REG_H5,  DR_REG_H10,
                           DR_REG_H16, DR_REG_H21, DR_REG_H31 };
    reg_id_t Rn_0_0[6] = { DR_REG_H0,  DR_REG_H6,  DR_REG_H11,
                           DR_REG_H17, DR_REG_H22, DR_REG_H31 };
    const char *expected_0_0[6] = {
        "frecpe %h0 -> %h0",   "frecpe %h6 -> %h5",   "frecpe %h11 -> %h10",
        "frecpe %h17 -> %h16", "frecpe %h22 -> %h21", "frecpe %h31 -> %h31",
    };
    TEST_LOOP(frecpe, frecpe, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]));

    /* Testing FRECPE  <V><d>, <V><n> */
    reg_id_t Rd_1_0[6] = { DR_REG_S0,  DR_REG_S5,  DR_REG_S10,
                           DR_REG_S16, DR_REG_S21, DR_REG_S31 };
    reg_id_t Rn_1_0[6] = { DR_REG_S0,  DR_REG_S6,  DR_REG_S11,
                           DR_REG_S17, DR_REG_S22, DR_REG_S31 };
    const char *expected_1_0[6] = {
        "frecpe %s0 -> %s0",   "frecpe %s6 -> %s5",   "frecpe %s11 -> %s10",
        "frecpe %s17 -> %s16", "frecpe %s22 -> %s21", "frecpe %s31 -> %s31",
    };
    TEST_LOOP(frecpe, frecpe, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]));

    reg_id_t Rd_1_1[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_1[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    const char *expected_1_1[6] = {
        "frecpe %d0 -> %d0",   "frecpe %d6 -> %d5",   "frecpe %d11 -> %d10",
        "frecpe %d17 -> %d16", "frecpe %d22 -> %d21", "frecpe %d31 -> %d31",
    };
    TEST_LOOP(frecpe, frecpe, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]));
}

TEST_INSTR(frecps_vector)
{
    opnd_t Rm_elsz;

    /* Testing FRECPS  <Hd>.<Ts>, <Hn>.<Ts>, <Hm>.<Ts> */
    reg_id_t Rd_0_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_0_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_0_0[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[6] = {
        "frecps %d0 %d0 $0x01 -> %d0",    "frecps %d6 %d7 $0x01 -> %d5",
        "frecps %d11 %d12 $0x01 -> %d10", "frecps %d17 %d18 $0x01 -> %d16",
        "frecps %d22 %d23 $0x01 -> %d21", "frecps %d31 %d31 $0x01 -> %d31",
    };
    TEST_LOOP(frecps, frecps_vector, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]), Rm_elsz);

    reg_id_t Rd_0_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_0_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_0_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[6] = {
        "frecps %q0 %q0 $0x01 -> %q0",    "frecps %q6 %q7 $0x01 -> %q5",
        "frecps %q11 %q12 $0x01 -> %q10", "frecps %q17 %q18 $0x01 -> %q16",
        "frecps %q22 %q23 $0x01 -> %q21", "frecps %q31 %q31 $0x01 -> %q31",
    };
    TEST_LOOP(frecps, frecps_vector, 6, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]), Rm_elsz);

    /* Testing FRECPS  <Dd>.<Ts>, <Dn>.<Ts>, <Dm>.<Ts> */
    reg_id_t Rd_1_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_1_0[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[6] = {
        "frecps %d0 %d0 $0x02 -> %d0",    "frecps %d6 %d7 $0x02 -> %d5",
        "frecps %d11 %d12 $0x02 -> %d10", "frecps %d17 %d18 $0x02 -> %d16",
        "frecps %d22 %d23 $0x02 -> %d21", "frecps %d31 %d31 $0x02 -> %d31",
    };
    TEST_LOOP(frecps, frecps_vector, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]), Rm_elsz);

    reg_id_t Rd_1_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[6] = {
        "frecps %q0 %q0 $0x02 -> %q0",    "frecps %q6 %q7 $0x02 -> %q5",
        "frecps %q11 %q12 $0x02 -> %q10", "frecps %q17 %q18 $0x02 -> %q16",
        "frecps %q22 %q23 $0x02 -> %q21", "frecps %q31 %q31 $0x02 -> %q31",
    };
    TEST_LOOP(frecps, frecps_vector, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]), Rm_elsz);

    reg_id_t Rd_1_2[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_2[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_2[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[6] = {
        "frecps %q0 %q0 $0x03 -> %q0",    "frecps %q6 %q7 $0x03 -> %q5",
        "frecps %q11 %q12 $0x03 -> %q10", "frecps %q17 %q18 $0x03 -> %q16",
        "frecps %q22 %q23 $0x03 -> %q21", "frecps %q31 %q31 $0x03 -> %q31",
    };
    TEST_LOOP(frecps, frecps_vector, 6, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), opnd_create_reg(Rm_1_2[i]), Rm_elsz);
}

TEST_INSTR(frecps)
{

    /* Testing FRECPS  <Hd>, <Hn>, <Hm> */
    reg_id_t Rd_0_0[6] = { DR_REG_H0,  DR_REG_H5,  DR_REG_H10,
                           DR_REG_H16, DR_REG_H21, DR_REG_H31 };
    reg_id_t Rn_0_0[6] = { DR_REG_H0,  DR_REG_H6,  DR_REG_H11,
                           DR_REG_H17, DR_REG_H22, DR_REG_H31 };
    reg_id_t Rm_0_0[6] = { DR_REG_H0,  DR_REG_H7,  DR_REG_H12,
                           DR_REG_H18, DR_REG_H23, DR_REG_H31 };
    const char *expected_0_0[6] = {
        "frecps %h0 %h0 -> %h0",    "frecps %h6 %h7 -> %h5",
        "frecps %h11 %h12 -> %h10", "frecps %h17 %h18 -> %h16",
        "frecps %h22 %h23 -> %h21", "frecps %h31 %h31 -> %h31",
    };
    TEST_LOOP(frecps, frecps, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]));

    /* Testing FRECPS  <V><d>, <V><n>, <V><m> */
    reg_id_t Rd_1_0[6] = { DR_REG_S0,  DR_REG_S5,  DR_REG_S10,
                           DR_REG_S16, DR_REG_S21, DR_REG_S31 };
    reg_id_t Rn_1_0[6] = { DR_REG_S0,  DR_REG_S6,  DR_REG_S11,
                           DR_REG_S17, DR_REG_S22, DR_REG_S31 };
    reg_id_t Rm_1_0[6] = { DR_REG_S0,  DR_REG_S7,  DR_REG_S12,
                           DR_REG_S18, DR_REG_S23, DR_REG_S31 };
    const char *expected_1_0[6] = {
        "frecps %s0 %s0 -> %s0",    "frecps %s6 %s7 -> %s5",
        "frecps %s11 %s12 -> %s10", "frecps %s17 %s18 -> %s16",
        "frecps %s22 %s23 -> %s21", "frecps %s31 %s31 -> %s31",
    };
    TEST_LOOP(frecps, frecps, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]));

    reg_id_t Rd_1_1[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_1[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_1_1[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    const char *expected_1_1[6] = {
        "frecps %d0 %d0 -> %d0",    "frecps %d6 %d7 -> %d5",
        "frecps %d11 %d12 -> %d10", "frecps %d17 %d18 -> %d16",
        "frecps %d22 %d23 -> %d21", "frecps %d31 %d31 -> %d31",
    };
    TEST_LOOP(frecps, frecps, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]));
}

TEST_INSTR(frsqrte_vector)
{
    opnd_t Rn_elsz;

    /* Testing FRSQRTE <Hd>.<Ts>, <Hn>.<Ts> */
    reg_id_t Rd_0_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_0_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[6] = {
        "frsqrte %d0 $0x01 -> %d0",   "frsqrte %d6 $0x01 -> %d5",
        "frsqrte %d11 $0x01 -> %d10", "frsqrte %d17 $0x01 -> %d16",
        "frsqrte %d22 $0x01 -> %d21", "frsqrte %d31 $0x01 -> %d31",
    };
    TEST_LOOP(frsqrte, frsqrte_vector, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), Rn_elsz);

    reg_id_t Rd_0_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_0_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[6] = {
        "frsqrte %q0 $0x01 -> %q0",   "frsqrte %q6 $0x01 -> %q5",
        "frsqrte %q11 $0x01 -> %q10", "frsqrte %q17 $0x01 -> %q16",
        "frsqrte %q22 $0x01 -> %q21", "frsqrte %q31 $0x01 -> %q31",
    };
    TEST_LOOP(frsqrte, frsqrte_vector, 6, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), Rn_elsz);

    /* Testing FRSQRTE <Dd>.<Ts>, <Dn>.<Ts> */
    reg_id_t Rd_1_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[6] = {
        "frsqrte %d0 $0x02 -> %d0",   "frsqrte %d6 $0x02 -> %d5",
        "frsqrte %d11 $0x02 -> %d10", "frsqrte %d17 $0x02 -> %d16",
        "frsqrte %d22 $0x02 -> %d21", "frsqrte %d31 $0x02 -> %d31",
    };
    TEST_LOOP(frsqrte, frsqrte_vector, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), Rn_elsz);

    reg_id_t Rd_1_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[6] = {
        "frsqrte %q0 $0x02 -> %q0",   "frsqrte %q6 $0x02 -> %q5",
        "frsqrte %q11 $0x02 -> %q10", "frsqrte %q17 $0x02 -> %q16",
        "frsqrte %q22 $0x02 -> %q21", "frsqrte %q31 $0x02 -> %q31",
    };
    TEST_LOOP(frsqrte, frsqrte_vector, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), Rn_elsz);

    reg_id_t Rd_1_2[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_2[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    Rn_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[6] = {
        "frsqrte %q0 $0x03 -> %q0",   "frsqrte %q6 $0x03 -> %q5",
        "frsqrte %q11 $0x03 -> %q10", "frsqrte %q17 $0x03 -> %q16",
        "frsqrte %q22 $0x03 -> %q21", "frsqrte %q31 $0x03 -> %q31",
    };
    TEST_LOOP(frsqrte, frsqrte_vector, 6, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), Rn_elsz);
}

TEST_INSTR(frsqrte)
{

    /* Testing FRSQRTE <Hd>, <Hn> */
    reg_id_t Rd_0_0[6] = { DR_REG_H0,  DR_REG_H5,  DR_REG_H10,
                           DR_REG_H16, DR_REG_H21, DR_REG_H31 };
    reg_id_t Rn_0_0[6] = { DR_REG_H0,  DR_REG_H6,  DR_REG_H11,
                           DR_REG_H17, DR_REG_H22, DR_REG_H31 };
    const char *expected_0_0[6] = {
        "frsqrte %h0 -> %h0",   "frsqrte %h6 -> %h5",   "frsqrte %h11 -> %h10",
        "frsqrte %h17 -> %h16", "frsqrte %h22 -> %h21", "frsqrte %h31 -> %h31",
    };
    TEST_LOOP(frsqrte, frsqrte, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]));

    /* Testing FRSQRTE <V><d>, <V><n> */
    reg_id_t Rd_1_0[6] = { DR_REG_S0,  DR_REG_S5,  DR_REG_S10,
                           DR_REG_S16, DR_REG_S21, DR_REG_S31 };
    reg_id_t Rn_1_0[6] = { DR_REG_S0,  DR_REG_S6,  DR_REG_S11,
                           DR_REG_S17, DR_REG_S22, DR_REG_S31 };
    const char *expected_1_0[6] = {
        "frsqrte %s0 -> %s0",   "frsqrte %s6 -> %s5",   "frsqrte %s11 -> %s10",
        "frsqrte %s17 -> %s16", "frsqrte %s22 -> %s21", "frsqrte %s31 -> %s31",
    };
    TEST_LOOP(frsqrte, frsqrte, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]));

    reg_id_t Rd_1_1[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_1[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    const char *expected_1_1[6] = {
        "frsqrte %d0 -> %d0",   "frsqrte %d6 -> %d5",   "frsqrte %d11 -> %d10",
        "frsqrte %d17 -> %d16", "frsqrte %d22 -> %d21", "frsqrte %d31 -> %d31",
    };
    TEST_LOOP(frsqrte, frsqrte, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]));
}

TEST_INSTR(frsqrts_vector)
{
    opnd_t Rm_elsz;

    /* Testing FRSQRTS <Hd>.<Ts>, <Hn>.<Ts>, <Hm>.<Ts> */
    reg_id_t Rd_0_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_0_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_0_0[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_0[6] = {
        "frsqrts %d0 %d0 $0x01 -> %d0",    "frsqrts %d6 %d7 $0x01 -> %d5",
        "frsqrts %d11 %d12 $0x01 -> %d10", "frsqrts %d17 %d18 $0x01 -> %d16",
        "frsqrts %d22 %d23 $0x01 -> %d21", "frsqrts %d31 %d31 $0x01 -> %d31",
    };
    TEST_LOOP(frsqrts, frsqrts_vector, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]), Rm_elsz);

    reg_id_t Rd_0_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_0_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_0_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_HALF();
    const char *expected_0_1[6] = {
        "frsqrts %q0 %q0 $0x01 -> %q0",    "frsqrts %q6 %q7 $0x01 -> %q5",
        "frsqrts %q11 %q12 $0x01 -> %q10", "frsqrts %q17 %q18 $0x01 -> %q16",
        "frsqrts %q22 %q23 $0x01 -> %q21", "frsqrts %q31 %q31 $0x01 -> %q31",
    };
    TEST_LOOP(frsqrts, frsqrts_vector, 6, expected_0_1[i], opnd_create_reg(Rd_0_1[i]),
              opnd_create_reg(Rn_0_1[i]), opnd_create_reg(Rm_0_1[i]), Rm_elsz);

    /* Testing FRSQRTS <Dd>.<Ts>, <Dn>.<Ts>, <Dm>.<Ts> */
    reg_id_t Rd_1_0[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_0[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_1_0[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_0[6] = {
        "frsqrts %d0 %d0 $0x02 -> %d0",    "frsqrts %d6 %d7 $0x02 -> %d5",
        "frsqrts %d11 %d12 $0x02 -> %d10", "frsqrts %d17 %d18 $0x02 -> %d16",
        "frsqrts %d22 %d23 $0x02 -> %d21", "frsqrts %d31 %d31 $0x02 -> %d31",
    };
    TEST_LOOP(frsqrts, frsqrts_vector, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]), Rm_elsz);

    reg_id_t Rd_1_1[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_1[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_1[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_SINGLE();
    const char *expected_1_1[6] = {
        "frsqrts %q0 %q0 $0x02 -> %q0",    "frsqrts %q6 %q7 $0x02 -> %q5",
        "frsqrts %q11 %q12 $0x02 -> %q10", "frsqrts %q17 %q18 $0x02 -> %q16",
        "frsqrts %q22 %q23 $0x02 -> %q21", "frsqrts %q31 %q31 $0x02 -> %q31",
    };
    TEST_LOOP(frsqrts, frsqrts_vector, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]), Rm_elsz);

    reg_id_t Rd_1_2[6] = { DR_REG_Q0,  DR_REG_Q5,  DR_REG_Q10,
                           DR_REG_Q16, DR_REG_Q21, DR_REG_Q31 };
    reg_id_t Rn_1_2[6] = { DR_REG_Q0,  DR_REG_Q6,  DR_REG_Q11,
                           DR_REG_Q17, DR_REG_Q22, DR_REG_Q31 };
    reg_id_t Rm_1_2[6] = { DR_REG_Q0,  DR_REG_Q7,  DR_REG_Q12,
                           DR_REG_Q18, DR_REG_Q23, DR_REG_Q31 };
    Rm_elsz = OPND_CREATE_DOUBLE();
    const char *expected_1_2[6] = {
        "frsqrts %q0 %q0 $0x03 -> %q0",    "frsqrts %q6 %q7 $0x03 -> %q5",
        "frsqrts %q11 %q12 $0x03 -> %q10", "frsqrts %q17 %q18 $0x03 -> %q16",
        "frsqrts %q22 %q23 $0x03 -> %q21", "frsqrts %q31 %q31 $0x03 -> %q31",
    };
    TEST_LOOP(frsqrts, frsqrts_vector, 6, expected_1_2[i], opnd_create_reg(Rd_1_2[i]),
              opnd_create_reg(Rn_1_2[i]), opnd_create_reg(Rm_1_2[i]), Rm_elsz);
}

TEST_INSTR(frsqrts)
{

    /* Testing FRSQRTS <Hd>, <Hn>, <Hm> */
    reg_id_t Rd_0_0[6] = { DR_REG_H0,  DR_REG_H5,  DR_REG_H10,
                           DR_REG_H16, DR_REG_H21, DR_REG_H31 };
    reg_id_t Rn_0_0[6] = { DR_REG_H0,  DR_REG_H6,  DR_REG_H11,
                           DR_REG_H17, DR_REG_H22, DR_REG_H31 };
    reg_id_t Rm_0_0[6] = { DR_REG_H0,  DR_REG_H7,  DR_REG_H12,
                           DR_REG_H18, DR_REG_H23, DR_REG_H31 };
    const char *expected_0_0[6] = {
        "frsqrts %h0 %h0 -> %h0",    "frsqrts %h6 %h7 -> %h5",
        "frsqrts %h11 %h12 -> %h10", "frsqrts %h17 %h18 -> %h16",
        "frsqrts %h22 %h23 -> %h21", "frsqrts %h31 %h31 -> %h31",
    };
    TEST_LOOP(frsqrts, frsqrts, 6, expected_0_0[i], opnd_create_reg(Rd_0_0[i]),
              opnd_create_reg(Rn_0_0[i]), opnd_create_reg(Rm_0_0[i]));

    /* Testing FRSQRTS <V><d>, <V><n>, <V><m> */
    reg_id_t Rd_1_0[6] = { DR_REG_S0,  DR_REG_S5,  DR_REG_S10,
                           DR_REG_S16, DR_REG_S21, DR_REG_S31 };
    reg_id_t Rn_1_0[6] = { DR_REG_S0,  DR_REG_S6,  DR_REG_S11,
                           DR_REG_S17, DR_REG_S22, DR_REG_S31 };
    reg_id_t Rm_1_0[6] = { DR_REG_S0,  DR_REG_S7,  DR_REG_S12,
                           DR_REG_S18, DR_REG_S23, DR_REG_S31 };
    const char *expected_1_0[6] = {
        "frsqrts %s0 %s0 -> %s0",    "frsqrts %s6 %s7 -> %s5",
        "frsqrts %s11 %s12 -> %s10", "frsqrts %s17 %s18 -> %s16",
        "frsqrts %s22 %s23 -> %s21", "frsqrts %s31 %s31 -> %s31",
    };
    TEST_LOOP(frsqrts, frsqrts, 6, expected_1_0[i], opnd_create_reg(Rd_1_0[i]),
              opnd_create_reg(Rn_1_0[i]), opnd_create_reg(Rm_1_0[i]));

    reg_id_t Rd_1_1[6] = { DR_REG_D0,  DR_REG_D5,  DR_REG_D10,
                           DR_REG_D16, DR_REG_D21, DR_REG_D31 };
    reg_id_t Rn_1_1[6] = { DR_REG_D0,  DR_REG_D6,  DR_REG_D11,
                           DR_REG_D17, DR_REG_D22, DR_REG_D31 };
    reg_id_t Rm_1_1[6] = { DR_REG_D0,  DR_REG_D7,  DR_REG_D12,
                           DR_REG_D18, DR_REG_D23, DR_REG_D31 };
    const char *expected_1_1[6] = {
        "frsqrts %d0 %d0 -> %d0",    "frsqrts %d6 %d7 -> %d5",
        "frsqrts %d11 %d12 -> %d10", "frsqrts %d17 %d18 -> %d16",
        "frsqrts %d22 %d23 -> %d21", "frsqrts %d31 %d31 -> %d31",
    };
    TEST_LOOP(frsqrts, frsqrts, 6, expected_1_1[i], opnd_create_reg(Rd_1_1[i]),
              opnd_create_reg(Rn_1_1[i]), opnd_create_reg(Rm_1_1[i]));
}

TEST_INSTR(dc_cvap)
{
    const char *expected[6] = {
        "dc_cvap (%x0)[1byte]",  "dc_cvap (%x5)[1byte]",  "dc_cvap (%x10)[1byte]",
        "dc_cvap (%x15)[1byte]", "dc_cvap (%x20)[1byte]", "dc_cvap (%x30)[1byte]",
    };
    TEST_LOOP(dc_cvap, dc_cvap, 6, expected[i], opnd_create_reg(Xn_six_offset_0[i]));
}

TEST_INSTR(dc_cvadp)
{
    const char *expected[6] = {
        "dc_cvadp (%x0)[1byte]",  "dc_cvadp (%x5)[1byte]",  "dc_cvadp (%x10)[1byte]",
        "dc_cvadp (%x15)[1byte]", "dc_cvadp (%x20)[1byte]", "dc_cvadp (%x30)[1byte]",
    };
    TEST_LOOP(dc_cvadp, dc_cvadp, 6, expected[i], opnd_create_reg(Xn_six_offset_0[i]));
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

    RUN_INSTR_TEST(fcvtas_vector);
    RUN_INSTR_TEST(fcvtas_scalar);
    RUN_INSTR_TEST(fcvtau_vector);
    RUN_INSTR_TEST(fcvtau_scalar);
    RUN_INSTR_TEST(fcvtms_vector);
    RUN_INSTR_TEST(fcvtms_scalar);
    RUN_INSTR_TEST(fcvtmu_vector);
    RUN_INSTR_TEST(fcvtmu_scalar);
    RUN_INSTR_TEST(fcvtns_vector);
    RUN_INSTR_TEST(fcvtns_scalar);
    RUN_INSTR_TEST(fcvtnu_vector);
    RUN_INSTR_TEST(fcvtnu_scalar);
    RUN_INSTR_TEST(fcvtps_vector);
    RUN_INSTR_TEST(fcvtps_scalar);
    RUN_INSTR_TEST(fcvtpu_vector);
    RUN_INSTR_TEST(fcvtpu_scalar);
    RUN_INSTR_TEST(fcvtzs_vector);
    RUN_INSTR_TEST(fcvtzs_scalar);
    RUN_INSTR_TEST(fcvtzu_vector);
    RUN_INSTR_TEST(fcvtzu_scalar);

    RUN_INSTR_TEST(frinta_vector);
    RUN_INSTR_TEST(frinta_scalar);
    RUN_INSTR_TEST(frinti_vector);
    RUN_INSTR_TEST(frinti_scalar);
    RUN_INSTR_TEST(frintm_vector);
    RUN_INSTR_TEST(frintm_scalar);
    RUN_INSTR_TEST(frintn_vector);
    RUN_INSTR_TEST(frintn_scalar);
    RUN_INSTR_TEST(frintp_vector);
    RUN_INSTR_TEST(frintp_scalar);
    RUN_INSTR_TEST(frintx_vector);
    RUN_INSTR_TEST(frintx_scalar);
    RUN_INSTR_TEST(frintz_vector);
    RUN_INSTR_TEST(frintz_scalar);

    RUN_INSTR_TEST(fmlal_vector);
    RUN_INSTR_TEST(fmlal_vector_idx);
    RUN_INSTR_TEST(fmlal2_vector);
    RUN_INSTR_TEST(fmlal2_vector_idx);
    RUN_INSTR_TEST(fmlsl_vector);
    RUN_INSTR_TEST(fmlsl_vector_idx);
    RUN_INSTR_TEST(fmlsl2_vector);
    RUN_INSTR_TEST(fmlsl2_vector_idx);

    RUN_INSTR_TEST(sm3partw1_vector);
    RUN_INSTR_TEST(sm3partw2_vector);
    RUN_INSTR_TEST(sm3ss1_vector);
    RUN_INSTR_TEST(sm3tt1a_vector_indexed);
    RUN_INSTR_TEST(sm3tt1b_vector_indexed);
    RUN_INSTR_TEST(sm3tt2a_vector_indexed);
    RUN_INSTR_TEST(sm3tt2b_vector_indexed);
    RUN_INSTR_TEST(sm4e_vector);
    RUN_INSTR_TEST(sm4ekey_vector);

    RUN_INSTR_TEST(sha512h);
    RUN_INSTR_TEST(sha512h2);
    RUN_INSTR_TEST(sha512su0);
    RUN_INSTR_TEST(sha512su1);

    RUN_INSTR_TEST(bcax);
    RUN_INSTR_TEST(eor3);
    RUN_INSTR_TEST(esb);
    RUN_INSTR_TEST(psb);

    RUN_INSTR_TEST(fsqrt_vector);
    RUN_INSTR_TEST(fsqrt_scalar);

    RUN_INSTR_TEST(scvtf_vector);
    RUN_INSTR_TEST(scvtf_scalar);
    RUN_INSTR_TEST(scvtf_scalar_fixed);
    RUN_INSTR_TEST(ucvtf_vector);
    RUN_INSTR_TEST(ucvtf_scalar);
    RUN_INSTR_TEST(ucvtf_scalar_fixed);

    RUN_INSTR_TEST(rax1);
    RUN_INSTR_TEST(xar);

    RUN_INSTR_TEST(fccmp);
    RUN_INSTR_TEST(fccmpe);
    RUN_INSTR_TEST(fcmp);
    RUN_INSTR_TEST(fcmpe);
    RUN_INSTR_TEST(fcsel);

    RUN_INSTR_TEST(sdot_vector);
    RUN_INSTR_TEST(sdot_vector_indexed);
    RUN_INSTR_TEST(udot_vector);
    RUN_INSTR_TEST(udot_vector_indexed);

    RUN_INSTR_TEST(fmov);
    RUN_INSTR_TEST(fmulx);
    RUN_INSTR_TEST(fmulx_vector);
    RUN_INSTR_TEST(fmulx_vector_idx);

    RUN_INSTR_TEST(facge_vector);
    RUN_INSTR_TEST(facge);
    RUN_INSTR_TEST(facgt_vector);
    RUN_INSTR_TEST(facgt);
    RUN_INSTR_TEST(faddp_vector);
    RUN_INSTR_TEST(faddp_scalar);
    RUN_INSTR_TEST(fcmeq_vector);
    RUN_INSTR_TEST(fcmeq_vector_zero);
    RUN_INSTR_TEST(fcmeq);
    RUN_INSTR_TEST(fcmeq_zero);
    RUN_INSTR_TEST(fcmgt_vector_zero);
    RUN_INSTR_TEST(fcmgt_vector);
    RUN_INSTR_TEST(fcmgt_zero);
    RUN_INSTR_TEST(fcmgt);
    RUN_INSTR_TEST(fcmle_vector_zero);
    RUN_INSTR_TEST(fcmle_zero);
    RUN_INSTR_TEST(fcmlt_vector_zero);
    RUN_INSTR_TEST(fcmlt_zero);

    RUN_INSTR_TEST(fmaxnmp_vector);
    RUN_INSTR_TEST(fmaxnmp_scalar);
    RUN_INSTR_TEST(fmaxp_vector);
    RUN_INSTR_TEST(fmaxp_scalar);
    RUN_INSTR_TEST(fminnmp_vector);
    RUN_INSTR_TEST(fminnmp_scalar);
    RUN_INSTR_TEST(fminnmv_vector);
    RUN_INSTR_TEST(fminp_vector);
    RUN_INSTR_TEST(fminp_scalar);

    RUN_INSTR_TEST(fmla_vector);
    RUN_INSTR_TEST(fmla_vector_idx);
    RUN_INSTR_TEST(fmls_vector);
    RUN_INSTR_TEST(fmls_vector_idx);

    RUN_INSTR_TEST(frecpe_vector);
    RUN_INSTR_TEST(frecpe);
    RUN_INSTR_TEST(frecps_vector);
    RUN_INSTR_TEST(frecps);
    RUN_INSTR_TEST(frsqrte_vector);
    RUN_INSTR_TEST(frsqrte);
    RUN_INSTR_TEST(frsqrts_vector);
    RUN_INSTR_TEST(frsqrts);

    RUN_INSTR_TEST(dc_cvap);
    RUN_INSTR_TEST(dc_cvadp);

    print("All v8.2 tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
