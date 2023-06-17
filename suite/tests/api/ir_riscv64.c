/* **********************************************************
 * Copyright (c) 2023 Institue of Software Chinese Academy of Sciences (ISCAS).
 * All rights reserved.
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
 * * Neither the name of ISCAS nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ISCAS OR CONTRIBUTORS BE LIABLE
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

static byte buf[8192];

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
test_instr_encoding(void *dc, uint opcode, instr_t *instr)
{
    instr_t *decin;
    byte *pc;

    ASSERT(instr_get_opcode(instr) == opcode);
    instr_disassemble(dc, instr, STDERR);
    print("\n");
    ASSERT(instr_is_encoding_possible(instr));
    pc = instr_encode(dc, instr, buf);
    decin = instr_create(dc);
    decode(dc, buf, decin);
    if (!instr_same(instr, decin)) {
        print("Disassembled as:\n");
        instr_disassemble(dc, decin, STDERR);
        print("\n");
        ASSERT(instr_same(instr, decin));
    }

    instr_destroy(dc, instr);
    instr_destroy(dc, decin);
}

static void
test_integer_load_store(void *dc)
{
    instr_t *instr;

    /* Load */
    instr = INSTR_CREATE_lb(
        dc, opnd_create_reg(DR_REG_A0),
        opnd_create_base_disp_decimal(DR_REG_A1, DR_REG_NULL, 0, 0, OPSZ_1));
    test_instr_encoding(dc, OP_lb, instr);
    instr = INSTR_CREATE_lbu(
        dc, opnd_create_reg(DR_REG_X0),
        opnd_create_base_disp_decimal(DR_REG_A1, DR_REG_NULL, 0, -1, OPSZ_1));
    test_instr_encoding(dc, OP_lbu, instr);
    instr = INSTR_CREATE_lh(
        dc, opnd_create_reg(DR_REG_A0),
        opnd_create_base_disp_decimal(DR_REG_A1, DR_REG_NULL, 0, (1 << 11) - 1, OPSZ_2));
    test_instr_encoding(dc, OP_lh, instr);
    instr = INSTR_CREATE_lhu(
        dc, opnd_create_reg(DR_REG_A0),
        opnd_create_base_disp_decimal(DR_REG_X0, DR_REG_NULL, 0, 0, OPSZ_2));
    test_instr_encoding(dc, OP_lhu, instr);
    instr = INSTR_CREATE_lw(
        dc, opnd_create_reg(DR_REG_X31),
        opnd_create_base_disp_decimal(DR_REG_A1, DR_REG_NULL, 0, -1, OPSZ_4));
    test_instr_encoding(dc, OP_lw, instr);
    instr = INSTR_CREATE_lwu(
        dc, opnd_create_reg(DR_REG_A0),
        opnd_create_base_disp_decimal(DR_REG_X31, DR_REG_NULL, 0, 0, OPSZ_4));
    test_instr_encoding(dc, OP_lwu, instr);
    instr = INSTR_CREATE_ld(
        dc, opnd_create_reg(DR_REG_A0),
        opnd_create_base_disp_decimal(DR_REG_A1, DR_REG_NULL, 0, 42, OPSZ_8));
    test_instr_encoding(dc, OP_ld, instr);

    /* Store */
    instr = INSTR_CREATE_sb(
        dc, opnd_create_base_disp_decimal(DR_REG_A1, DR_REG_NULL, 0, 0, OPSZ_1),
        opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_sb, instr);
    instr = INSTR_CREATE_sh(
        dc, opnd_create_base_disp_decimal(DR_REG_X0, DR_REG_NULL, 0, -1, OPSZ_2),
        opnd_create_reg(DR_REG_X31));
    test_instr_encoding(dc, OP_sh, instr);
    instr = INSTR_CREATE_sw(
        dc,
        opnd_create_base_disp_decimal(DR_REG_X31, DR_REG_NULL, 0, (1 << 11) - 1, OPSZ_4),
        opnd_create_reg(DR_REG_X0));
    test_instr_encoding(dc, OP_sw, instr);
    instr = INSTR_CREATE_sd(
        dc, opnd_create_base_disp_decimal(DR_REG_A1, DR_REG_NULL, 0, 42, OPSZ_8),
        opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_sd, instr);

    /* Compressed Load */
    instr = INSTR_CREATE_c_ldsp(
        dc, opnd_create_reg(DR_REG_A0),
        opnd_create_base_disp_decimal(DR_REG_SP, DR_REG_NULL, 0, 0, OPSZ_8));
    test_instr_encoding(dc, OP_c_ldsp, instr);
    instr = INSTR_CREATE_c_ld(dc, opnd_create_reg(DR_REG_X8),
                              opnd_create_base_disp_decimal(DR_REG_X15, DR_REG_NULL, 0,
                                                            ((1 << 5) - 1) << 3, OPSZ_8));
    test_instr_encoding(dc, OP_c_ld, instr);
    instr =
        INSTR_CREATE_c_lwsp(dc, opnd_create_reg(DR_REG_X0),
                            opnd_create_base_disp_decimal(DR_REG_SP, DR_REG_NULL, 0,
                                                          ((1 << 5) - 1) << 2, OPSZ_4));
    test_instr_encoding(dc, OP_c_lwsp, instr);
    instr = INSTR_CREATE_c_lw(dc, opnd_create_reg(DR_REG_X8),
                              opnd_create_base_disp_decimal(DR_REG_X15, DR_REG_NULL, 0,
                                                            ((1 << 5) - 1) << 2, OPSZ_4));
    test_instr_encoding(dc, OP_c_lw, instr);

    /* Compressed Store */
    instr = INSTR_CREATE_c_sdsp(
        dc, opnd_create_base_disp_decimal(DR_REG_SP, DR_REG_NULL, 0, 0, OPSZ_8),
        opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_c_sdsp, instr);
    instr = INSTR_CREATE_c_sd(dc,
                              opnd_create_base_disp_decimal(DR_REG_X15, DR_REG_NULL, 0,
                                                            ((1 << 5) - 1) << 3, OPSZ_8),
                              opnd_create_reg(DR_REG_X8));
    test_instr_encoding(dc, OP_c_sd, instr);
    instr =
        INSTR_CREATE_c_swsp(dc,
                            opnd_create_base_disp_decimal(DR_REG_SP, DR_REG_NULL, 0,
                                                          ((1 << 5) - 1) << 2, OPSZ_4),
                            opnd_create_reg(DR_REG_X0));
    test_instr_encoding(dc, OP_c_swsp, instr);
    instr = INSTR_CREATE_c_sw(dc,
                              opnd_create_base_disp_decimal(DR_REG_X15, DR_REG_NULL, 0,
                                                            ((1 << 5) - 1) << 2, OPSZ_4),
                              opnd_create_reg(DR_REG_X8));
    test_instr_encoding(dc, OP_c_sw, instr);
}

static void
test_float_load_store(void *dc)
{
    instr_t *instr;

    /* Load */
    instr = INSTR_CREATE_flw(
        dc, opnd_create_reg(DR_REG_F0),
        opnd_create_base_disp_decimal(DR_REG_A1, DR_REG_NULL, 0, 0, OPSZ_4));
    test_instr_encoding(dc, OP_flw, instr);
    instr = INSTR_CREATE_fld(
        dc, opnd_create_reg(DR_REG_F31),
        opnd_create_base_disp_decimal(DR_REG_X0, DR_REG_NULL, 0, -1, OPSZ_8));
    test_instr_encoding(dc, OP_fld, instr);

    /* Store */
    instr = INSTR_CREATE_fsw(
        dc,
        opnd_create_base_disp_decimal(DR_REG_X31, DR_REG_NULL, 0, (1 << 11) - 1, OPSZ_4),
        opnd_create_reg(DR_REG_F1));
    test_instr_encoding(dc, OP_fsw, instr);
    instr = INSTR_CREATE_fsd(
        dc,
        opnd_create_base_disp_decimal(DR_REG_X31, DR_REG_NULL, 0, (1 << 11) - 1, OPSZ_8),
        opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_fsd, instr);

    /* Compressed Load */
    instr = INSTR_CREATE_c_fldsp(
        dc, opnd_create_reg(DR_REG_F0),
        opnd_create_base_disp_decimal(DR_REG_SP, DR_REG_NULL, 0, 0, OPSZ_8));
    test_instr_encoding(dc, OP_c_fldsp, instr);
    instr =
        INSTR_CREATE_c_fld(dc, opnd_create_reg(DR_REG_F8),
                           opnd_create_base_disp_decimal(DR_REG_X15, DR_REG_NULL, 0,
                                                         ((1 << 5) - 1) << 3, OPSZ_8));
    test_instr_encoding(dc, OP_c_fld, instr);
    /* There is no c.flw* instructions in RV64. */

    /* Compressed Store */
    instr = INSTR_CREATE_c_fsdsp(
        dc, opnd_create_base_disp_decimal(DR_REG_SP, DR_REG_NULL, 0, 0, OPSZ_8),
        opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_c_fsdsp, instr);
    instr = INSTR_CREATE_c_fsd(dc,
                               opnd_create_base_disp_decimal(DR_REG_X15, DR_REG_NULL, 0,
                                                             ((1 << 5) - 1) << 3, OPSZ_8),
                               opnd_create_reg(DR_REG_F8));
    test_instr_encoding(dc, OP_c_fsd, instr);
    /* There is no c.fsw* instructions in RV64. */
}

static void
test_atomic(void *dc)
{
    instr_t *instr;

    /* FIXME i#3544: Use [aq][rl] instead of hex number when disassembling. */

    /* LR/SC */
    instr = INSTR_CREATE_lr_w(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_immed_int(0b00, OPSZ_2b));
    test_instr_encoding(dc, OP_lr_w, instr);
    instr = INSTR_CREATE_lr_d(dc, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_X31),
                              opnd_create_immed_int(0b10, OPSZ_2b));
    test_instr_encoding(dc, OP_lr_d, instr);
    instr = INSTR_CREATE_sc_w(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A2),
                              opnd_create_immed_int(0b01, OPSZ_2b));
    test_instr_encoding(dc, OP_sc_w, instr);
    instr = INSTR_CREATE_sc_d(dc, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_X31),
                              opnd_create_reg(DR_REG_A1),
                              opnd_create_immed_int(0b11, OPSZ_2b));
    test_instr_encoding(dc, OP_sc_d, instr);

    /* AMO */
    instr = INSTR_CREATE_amoswap_w(
        dc, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_X1),
        opnd_create_reg(DR_REG_X31), opnd_create_immed_int(0b00, OPSZ_2b));
    test_instr_encoding(dc, OP_amoswap_w, instr);
    instr = INSTR_CREATE_amoswap_d(dc, opnd_create_reg(DR_REG_X31),
                                   opnd_create_reg(DR_REG_X1), opnd_create_reg(DR_REG_X0),
                                   opnd_create_immed_int(0b01, OPSZ_2b));
    test_instr_encoding(dc, OP_amoswap_d, instr);
    instr = INSTR_CREATE_amoadd_w(dc, opnd_create_reg(DR_REG_X0),
                                  opnd_create_reg(DR_REG_X31), opnd_create_reg(DR_REG_A2),
                                  opnd_create_immed_int(0b10, OPSZ_2b));
    test_instr_encoding(dc, OP_amoadd_w, instr);
    instr = INSTR_CREATE_amoadd_d(dc, opnd_create_reg(DR_REG_A0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2),
                                  opnd_create_immed_int(0b11, OPSZ_2b));
    test_instr_encoding(dc, OP_amoadd_d, instr);
    instr = INSTR_CREATE_amoxor_w(dc, opnd_create_reg(DR_REG_A0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2),
                                  opnd_create_immed_int(0b01, OPSZ_2b));
    test_instr_encoding(dc, OP_amoxor_w, instr);
    instr = INSTR_CREATE_amoxor_d(dc, opnd_create_reg(DR_REG_A0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2),
                                  opnd_create_immed_int(0b01, OPSZ_2b));
    test_instr_encoding(dc, OP_amoxor_d, instr);
    instr = INSTR_CREATE_amoand_w(dc, opnd_create_reg(DR_REG_A0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2),
                                  opnd_create_immed_int(0b01, OPSZ_2b));
    test_instr_encoding(dc, OP_amoand_w, instr);
    instr = INSTR_CREATE_amoand_d(dc, opnd_create_reg(DR_REG_A0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2),
                                  opnd_create_immed_int(0b01, OPSZ_2b));
    test_instr_encoding(dc, OP_amoand_d, instr);
    instr = INSTR_CREATE_amoor_w(dc, opnd_create_reg(DR_REG_A0),
                                 opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2),
                                 opnd_create_immed_int(0b01, OPSZ_2b));
    test_instr_encoding(dc, OP_amoor_w, instr);
    instr = INSTR_CREATE_amoor_d(dc, opnd_create_reg(DR_REG_A0),
                                 opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2),
                                 opnd_create_immed_int(0b01, OPSZ_2b));
    test_instr_encoding(dc, OP_amoor_d, instr);
    instr = INSTR_CREATE_amomin_w(dc, opnd_create_reg(DR_REG_A0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2),
                                  opnd_create_immed_int(0b01, OPSZ_2b));
    test_instr_encoding(dc, OP_amomin_w, instr);
    instr = INSTR_CREATE_amomin_d(dc, opnd_create_reg(DR_REG_A0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2),
                                  opnd_create_immed_int(0b01, OPSZ_2b));
    test_instr_encoding(dc, OP_amomin_d, instr);
    instr = INSTR_CREATE_amomax_w(dc, opnd_create_reg(DR_REG_A0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2),
                                  opnd_create_immed_int(0b01, OPSZ_2b));
    test_instr_encoding(dc, OP_amomax_w, instr);
    instr = INSTR_CREATE_amomax_d(dc, opnd_create_reg(DR_REG_A0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2),
                                  opnd_create_immed_int(0b01, OPSZ_2b));
    test_instr_encoding(dc, OP_amomax_d, instr);
    instr = INSTR_CREATE_amominu_w(dc, opnd_create_reg(DR_REG_A0),
                                   opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2),
                                   opnd_create_immed_int(0b01, OPSZ_2b));
    test_instr_encoding(dc, OP_amominu_w, instr);
    instr = INSTR_CREATE_amominu_d(dc, opnd_create_reg(DR_REG_A0),
                                   opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2),
                                   opnd_create_immed_int(0b01, OPSZ_2b));
    test_instr_encoding(dc, OP_amominu_d, instr);
    instr = INSTR_CREATE_amomaxu_w(dc, opnd_create_reg(DR_REG_A0),
                                   opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2),
                                   opnd_create_immed_int(0b01, OPSZ_2b));
    test_instr_encoding(dc, OP_amomaxu_w, instr);
    instr = INSTR_CREATE_amomaxu_d(dc, opnd_create_reg(DR_REG_A0),
                                   opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2),
                                   opnd_create_immed_int(0b01, OPSZ_2b));
    test_instr_encoding(dc, OP_amomaxu_d, instr);
}

static void
test_fcvt(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_fcvt_l_s(dc, opnd_create_reg(DR_REG_A0),
                                  opnd_create_immed_int(0b000, OPSZ_3b),
                                  opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fcvt_l_s, instr);
    instr = INSTR_CREATE_fcvt_lu_s(dc, opnd_create_reg(DR_REG_A0),
                                   opnd_create_immed_int(0b001, OPSZ_3b),
                                   opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fcvt_lu_s, instr);

    instr = INSTR_CREATE_fcvt_s_l(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_immed_int(0b010, OPSZ_3b),
                                  opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_fcvt_s_l, instr);
    instr = INSTR_CREATE_fcvt_s_lu(dc, opnd_create_reg(DR_REG_F0),
                                   opnd_create_immed_int(0b011, OPSZ_3b),
                                   opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_fcvt_s_lu, instr);

    instr = INSTR_CREATE_fcvt_l_d(dc, opnd_create_reg(DR_REG_A0),
                                  opnd_create_immed_int(0b100, OPSZ_3b),
                                  opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fcvt_l_d, instr);
    instr = INSTR_CREATE_fcvt_lu_d(dc, opnd_create_reg(DR_REG_A0),
                                   opnd_create_immed_int(0b111, OPSZ_3b),
                                   opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fcvt_lu_d, instr);

    instr = INSTR_CREATE_fcvt_d_l(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_immed_int(0b000, OPSZ_3b),
                                  opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_fcvt_d_l, instr);
    instr = INSTR_CREATE_fcvt_d_lu(dc, opnd_create_reg(DR_REG_F0),
                                   opnd_create_immed_int(0b000, OPSZ_3b),
                                   opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_fcvt_d_lu, instr);

    instr = INSTR_CREATE_fcvt_d_s(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_immed_int(0b000, OPSZ_3b),
                                  opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fcvt_d_s, instr);
    instr = INSTR_CREATE_fcvt_s_d(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_immed_int(0b000, OPSZ_3b),
                                  opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fcvt_s_d, instr);

    instr = INSTR_CREATE_fcvt_w_d(dc, opnd_create_reg(DR_REG_A0),
                                  opnd_create_immed_int(0b000, OPSZ_3b),
                                  opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fcvt_w_d, instr);
    instr = INSTR_CREATE_fcvt_wu_d(dc, opnd_create_reg(DR_REG_A0),
                                   opnd_create_immed_int(0b000, OPSZ_3b),
                                   opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fcvt_wu_d, instr);

    instr = INSTR_CREATE_fcvt_d_w(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_immed_int(0b000, OPSZ_3b),
                                  opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_fcvt_d_w, instr);
    instr = INSTR_CREATE_fcvt_d_wu(dc, opnd_create_reg(DR_REG_F0),
                                   opnd_create_immed_int(0b000, OPSZ_3b),
                                   opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_fcvt_d_wu, instr);

    instr = INSTR_CREATE_fcvt_s_q(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_immed_int(0b000, OPSZ_3b),
                                  opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fcvt_s_q, instr);
    instr = INSTR_CREATE_fcvt_q_s(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_immed_int(0b000, OPSZ_3b),
                                  opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fcvt_q_s, instr);

    instr = INSTR_CREATE_fcvt_d_q(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_immed_int(0b000, OPSZ_3b),
                                  opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fcvt_d_q, instr);
    instr = INSTR_CREATE_fcvt_q_d(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_immed_int(0b000, OPSZ_3b),
                                  opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fcvt_q_d, instr);

    instr = INSTR_CREATE_fcvt_w_q(dc, opnd_create_reg(DR_REG_A0),
                                  opnd_create_immed_int(0b000, OPSZ_3b),
                                  opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fcvt_w_q, instr);
    instr = INSTR_CREATE_fcvt_q_w(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_immed_int(0b000, OPSZ_3b),
                                  opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_fcvt_q_w, instr);

    instr = INSTR_CREATE_fcvt_wu_q(dc, opnd_create_reg(DR_REG_A0),
                                   opnd_create_immed_int(0b000, OPSZ_3b),
                                   opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fcvt_wu_q, instr);
    instr = INSTR_CREATE_fcvt_q_wu(dc, opnd_create_reg(DR_REG_F0),
                                   opnd_create_immed_int(0b000, OPSZ_3b),
                                   opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_fcvt_q_wu, instr);

    instr = INSTR_CREATE_fcvt_w_s(dc, opnd_create_reg(DR_REG_A0),
                                  opnd_create_immed_int(0b000, OPSZ_3b),
                                  opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fcvt_w_s, instr);
    instr = INSTR_CREATE_fcvt_s_w(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_immed_int(0b000, OPSZ_3b),
                                  opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_fcvt_s_w, instr);

    instr = INSTR_CREATE_fcvt_wu_s(dc, opnd_create_reg(DR_REG_A0),
                                   opnd_create_immed_int(0b000, OPSZ_3b),
                                   opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fcvt_wu_s, instr);
    instr = INSTR_CREATE_fcvt_s_wu(dc, opnd_create_reg(DR_REG_F0),
                                   opnd_create_immed_int(0b000, OPSZ_3b),
                                   opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_fcvt_s_wu, instr);

    instr = INSTR_CREATE_fcvt_l_q(dc, opnd_create_reg(DR_REG_A0),
                                  opnd_create_immed_int(0b000, OPSZ_3b),
                                  opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fcvt_l_q, instr);
    instr = INSTR_CREATE_fcvt_lu_q(dc, opnd_create_reg(DR_REG_A0),
                                   opnd_create_immed_int(0b000, OPSZ_3b),
                                   opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fcvt_lu_q, instr);

    instr = INSTR_CREATE_fcvt_q_l(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_immed_int(0b000, OPSZ_3b),
                                  opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_fcvt_q_l, instr);
    instr = INSTR_CREATE_fcvt_q_lu(dc, opnd_create_reg(DR_REG_F0),
                                   opnd_create_immed_int(0b000, OPSZ_3b),
                                   opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_fcvt_q_lu, instr);
}

static void
test_fmv(void *dc)
{
    instr_t *instr;
    instr =
        INSTR_CREATE_fmv_x_d(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fmv_x_d, instr);
    instr =
        INSTR_CREATE_fmv_d_x(dc, opnd_create_reg(DR_REG_F31), opnd_create_reg(DR_REG_X0));
    test_instr_encoding(dc, OP_fmv_d_x, instr);
    instr =
        INSTR_CREATE_fmv_x_w(dc, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_fmv_x_w, instr);
    instr =
        INSTR_CREATE_fmv_w_x(dc, opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_fmv_w_x, instr);
}

int
main(int argc, char *argv[])
{
#ifdef STANDALONE_DECODER
    void *dcontext = GLOBAL_DCONTEXT;
#else
    void *dcontext = dr_standalone_init();
#endif

    test_integer_load_store(dcontext);
    print("test_integer_load_store complete\n");

    test_float_load_store(dcontext);
    print("test_float_load_store complete\n");

    test_atomic(dcontext);
    print("test_atomic complete\n");

    test_fcvt(dcontext);
    print("test_fcvt complete\n");

    test_fmv(dcontext);
    print("test_fmv complete\n");

    print("All tests complete\n");
    return 0;
}
